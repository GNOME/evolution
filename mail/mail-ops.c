/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Author : 
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <errno.h>
#include <gnome.h>
#include "mail.h"
#include "folder-browser.h"
#include "e-util/e-setup.h"

#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

static void
mail_exception_dialog (char *head, CamelException *ex, gpointer widget)
{
	char *msg;
	GtkWindow *window =
		GTK_WINDOW (gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW));

	msg = g_strdup_printf ("%s:\n%s", head,
			       camel_exception_get_description (ex));
	gnome_error_dialog_parented (msg, window);
	g_free (msg);
}

static gboolean
check_configured (void)
{
	char *path;
	gboolean configured;

	path = g_strdup_printf ("=%s/config=/mail/configured", evolution_dir);
	if (gnome_config_get_bool (path)) {
		g_free (path);
		return TRUE;
	}

	mail_config_druid ();

	configured = gnome_config_get_bool (path);
	g_free (path);
	return configured;
}

/* FIXME: This is BROKEN! It fetches mail into whatever folder you're
 * currently viewing.
 */
void
fetch_mail (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	CamelException *ex;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;
	int nmsgs, i;
	CamelMimeMessage *msg = NULL;
	char *path, *url = NULL;

	if (!check_configured ())
		return;

	path = g_strdup_printf ("=%s/config=/mail/source", evolution_dir);
	url = gnome_config_get_string (path);
	g_free (path);
	if (!url) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (fb),
							  GTK_TYPE_WINDOW);

		gnome_error_dialog_parented ("You have no remote mail source "
					     "configured", GTK_WINDOW (win));
		return;
	}

	path = CAMEL_SERVICE (fb->folder->parent_store)->url->path;
	ex = camel_exception_new ();

	/* If fetching mail from an mbox store, safely copy it to a
	 * temporary store first.
	 */
	if (!strncmp (url, "mbox:", 5)) {
		char *tmp_mbox, *source;
		int tmpfd;

		tmp_mbox = g_strdup_printf ("%s/movemail.XXXX", path);
#ifdef HAVE_MKSTEMP
		tmpfd = mkstemp (tmp_mbox);
#else
		if (mktemp (tmp_mbox)) {
			tmpfd = open (tmp_mbox, O_RDWR | O_CREAT | O_EXCL,
				      S_IRUSR | S_IWUSR);
		} else
			tmpfd = -1;
#endif
		if (tmpfd == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Couldn't create temporary "
					      "mbox: %s", g_strerror (errno));
			mail_exception_dialog ("Unable to move mail", ex, fb);
			goto cleanup;
		}
		close (tmpfd);

		/* Skip over "mbox:" plus host part (if any) of url. */
		source = url + 5;
		if (!strncmp (source, "//", 2))
			source = strchr (source + 2, '/');

		switch (camel_movemail (source, tmp_mbox, ex)) {
		case -1:
			mail_exception_dialog ("Unable to move mail", ex, fb);
			/* FALL THROUGH */

		case 0:
			unlink (tmp_mbox);
			g_free (tmp_mbox);
			goto cleanup;
		}

		folder = camel_store_get_folder (fb->folder->parent_store,
						 strrchr (tmp_mbox, '/') + 1,
						 ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to move mail", ex, fb);
			g_free (tmp_mbox);
			goto cleanup;
		}
	} else {
		store = camel_session_get_store (session, url, ex);
		if (!store) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, fb);
			goto cleanup;
		}
		camel_service_connect_with_url (CAMEL_SERVICE (store),
						url, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, fb);
			goto cleanup;
		}

		folder = camel_store_get_folder (store, "inbox", ex);
		if (!folder) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, fb);
			goto cleanup;
		}
	}

	camel_folder_open (folder, FOLDER_OPEN_READ, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	nmsgs = camel_folder_get_message_count (folder, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	if (nmsgs == 0)
		goto cleanup;

	for (i = 1; i <= nmsgs; i++) {
		msg = camel_folder_get_message_by_number (folder, i, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to read message",
					       ex, fb);
			goto cleanup;
		}

		camel_folder_append_message (fb->folder, msg, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to write message",
					       ex, fb);
			goto cleanup;
		}

		camel_folder_delete_message_by_number (folder, i, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to delete message",
					       ex, fb);
			goto cleanup;
		}
	}
	msg = NULL;

	/* Redisplay. Ick. FIXME */
	path = g_strdup_printf ("file://%s", path);
	folder_browser_set_uri (fb, path);
	g_free (path);

 cleanup:
	if (url)
		g_free (url);
	if (folder) {
		if (camel_folder_is_open (folder))
			camel_folder_close (folder, TRUE, ex);
		gtk_object_unref (GTK_OBJECT (folder));
	}
	if (store) {
		camel_service_disconnect (CAMEL_SERVICE (store), ex);
		gtk_object_unref (GTK_OBJECT (store));
	}
	camel_exception_free (ex);
	if (msg)
		gtk_object_unref (GTK_OBJECT (msg));
}


static void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	static CamelTransport *transport = NULL;
	static char *from = NULL;
	CamelException *ex;
	CamelMimeMessage *message;
	char *name, *addr, *path;

	ex = camel_exception_new ();

	if (!from) {
		CamelInternetAddress *ciaddr;

		path = g_strdup_printf ("=%s/config=/mail/id_name",
					evolution_dir);
		name = gnome_config_get_string (path);
		g_assert (name);
		g_free (path);
		path = g_strdup_printf ("=%s/config=/mail/id_addr",
					evolution_dir);
		addr = gnome_config_get_string (path);
		g_assert (addr);
		g_free (path);

		ciaddr = camel_internet_address_new ();
		camel_internet_address_add (ciaddr, name, addr);

		from = camel_address_encode (CAMEL_ADDRESS (ciaddr));
	}

	if (!transport) {
		char *url;

		path = g_strdup_printf ("=%s/config=/mail/transport",
					evolution_dir);
		url = gnome_config_get_string (path);
		g_assert (url);
		g_free (path);

		transport = camel_session_get_transport (session, url, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Could not load mail transport",
					       ex, composer);
			camel_exception_free (ex);
			return;
		}
	}

	message = e_msg_composer_get_message (composer);

	camel_mime_message_set_from (message, from);
	camel_medium_add_header (CAMEL_MEDIUM (message), "X-Mailer",
				 "Evolution (Developer Preview)");
	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	camel_transport_send (transport, CAMEL_MEDIUM (message), ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Could not send message", ex, composer);
		camel_exception_free (ex);
		gtk_object_unref (GTK_OBJECT (message));
		return;
	}

	gtk_object_unref (GTK_OBJECT (message));
	gtk_object_destroy (GTK_OBJECT (composer));
}


void
send_msg (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *composer;

	if (!check_configured ())
		return;

	composer = e_msg_composer_new ();

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}

/* Send according to a mailto (RFC 2368) URL. */
void
send_to_url (const char *url)
{
	GtkWidget *composer;

	if (!check_configured ())
		return;

	composer = e_msg_composer_new_from_url (url);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}	

static void
reply (FolderBrowser *fb, gboolean to_all)
{
	EMsgComposer *composer;

	if (!check_configured ())
		return;

	composer = mail_generate_reply (fb->mail_display->current_message,
					to_all);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	gtk_widget_show (GTK_WIDGET (composer));	
}

void
reply_to_sender (GtkWidget *button, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), FALSE);
}

void
reply_to_all (GtkWidget *button, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), TRUE);
}


void
forward_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb;
	EMsgComposer *composer;

	if (!check_configured ())
		return;

	fb = FOLDER_BROWSER (user_data);
	composer = mail_generate_forward (fb->mail_display->current_message,
					  TRUE, TRUE);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	gtk_widget_show (GTK_WIDGET (composer));	
}

void
delete_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;

	if (fb->mail_display->current_message) {
		guint32 flags;

		/* FIXME: table should watch the message with a signal and update display! */

		flags = camel_mime_message_get_flags(fb->mail_display->current_message);
		camel_mime_message_set_flags(fb->mail_display->current_message, CAMEL_MESSAGE_DELETED, ~flags);
		printf("Message %s set to %s\n", fb->mail_display->current_message->message_uid, flags&CAMEL_MESSAGE_DELETED?"UNDELETED":"DELETED");
	}
}

void
expunge_folder (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	CamelException ex;

	if (fb->message_list->folder) {
		camel_exception_init(&ex);

		camel_folder_expunge(fb->message_list->folder, &ex);

		/* FIXME: is there a better way to force an update? */
		/* FIXME: Folder should raise a signal to say its contents has changed ... */
		e_table_model_changed (fb->message_list->table_model);

/* this always throws an error, when it shouldn't? */
#if 0
		if (camel_exception_get_id (&ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to expunge deleted messages", &ex, fb);
		}
#endif
	}
}


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
#include "camel/camel.h"
#include "mail-ops.h"
#include "mail-format.h"
#include "folder-browser.h"
#include "session.h"
#include "e-util/e-setup.h"
#include "composer/e-msg-composer.h"

#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

static void
mail_exception_dialog (char *head, CamelException *ex, GtkWindow *window)
{
	char *msg;

	msg = g_strdup_printf ("%s:\n%s", head,
			       camel_exception_get_description (ex));
	gnome_error_dialog_parented (msg, window);
	g_free (msg);
}

void
fetch_mail (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GtkWindow *window;
	CamelException *ex;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL, *outfolder = NULL;
	int nmsgs, i;
	CamelMimeMessage *msg = NULL;
	char *path, *url = NULL;
	gboolean get_remote;

	window = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb),
						      GTK_TYPE_WINDOW));

	/* FIXME: We want a better config solution than this. */
	path = g_strdup_printf ("=%s/config=/mail/get_remote", evolution_dir);
	get_remote = gnome_config_get_bool_with_default (path, FALSE);
	g_free (path);
	path = g_strdup_printf ("=%s/config=/mail/remote_url", evolution_dir);
	url = gnome_config_get_string_with_default (path, NULL);
	g_free (path);
	if (!get_remote || !url) {
		if (url)
			g_free (url);
		gnome_error_dialog_parented ("You have no remote mail source "
					     "configured", window);
		return;
	}

	ex = camel_exception_new ();

	/* If fetching mail from an mbox store, safely copy it to a
	 * temporary store first.
	 */
	if (!strncmp (url, "mbox:", 5)) {
		char *tmp_mbox, *source;
		int tmpfd;

		tmp_mbox = g_strdup_printf ("%s/movemail.XXXX",
					    evolution_folders_dir);
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
			mail_exception_dialog ("Unable to move mail", ex,
					       window);
			goto cleanup;
		}
		close (tmpfd);

		/* Skip over "mbox://" plus host part (if any) or url. */
		source = strchr (url + 7, '/');

		switch (camel_movemail (source, tmp_mbox, ex)) {
		case -1:
			mail_exception_dialog ("Unable to move mail", ex,
					       window);
			/* FALL THROUGH */

		case 0:
			unlink (tmp_mbox);
			g_free (tmp_mbox);
			goto cleanup;
		}

		folder = camel_store_get_folder (default_session->store,
						 strrchr (tmp_mbox, '/') + 1,
						 ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to move mail", ex,
					       window);
			g_free (tmp_mbox);
			goto cleanup;
		}
	} else {
		store = camel_session_get_store (default_session->session,
						 url, ex);
		if (!store) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, window);
			goto cleanup;
		}
		camel_service_connect_with_url (CAMEL_SERVICE (store),
						url, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, window);
			goto cleanup;
		}

		folder = camel_store_get_folder (store, "inbox", ex);
		if (!folder) {
			mail_exception_dialog ("Unable to get new mail",
					       ex, window);
			goto cleanup;
		}
	}

	camel_folder_open (folder, FOLDER_OPEN_READ, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to get new mail", ex, window);
		goto cleanup;
	}

	nmsgs = camel_folder_get_message_count (folder, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to get new mail", ex, window);
		goto cleanup;
	}

	if (nmsgs == 0)
		goto cleanup;

	outfolder = camel_store_get_folder (default_session->store,
					    "inbox", ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to open inbox to store mail",
				       ex, window);
		goto cleanup;
	}
	camel_folder_open (outfolder, FOLDER_OPEN_WRITE, ex);
	if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
		mail_exception_dialog ("Unable to open inbox to store mail",
				       ex, window);
		goto cleanup;
	}

	for (i = 1; i <= nmsgs; i++) {
		msg = camel_folder_get_message_by_number (folder, i, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to read message",
					       ex, window);
			goto cleanup;
		}

		camel_folder_append_message (outfolder, msg, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to write message",
					       ex, window);
			goto cleanup;
		}

		camel_folder_delete_message_by_number (folder, i, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Unable to delete message",
					       ex, window);
			goto cleanup;
		}
	}
	msg = NULL;

	folder_browser_set_uri (fb, "inbox"); /* FIXME */

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
#if 0
	/* FIXME: we'll want to do this when the rest of the mail
	 * stuff is refcounting things properly.
	 */
	if (outfolder) {
		if (camel_folder_is_open (outfolder))
			camel_folder_close (outfolder, FALSE, ex);
		gtk_object_unref (GTK_OBJECT (outfolder));
	}
#endif
	camel_exception_free (ex);
	if (msg)
		gtk_object_unref (GTK_OBJECT (msg));
}


static void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	int stdout_dup;

	message = e_msg_composer_get_message (composer);

	stdout_dup = dup (1);
	stream = camel_stream_fs_new_with_fd (stdout_dup);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message),
					    stream);
	camel_stream_close (stream);

	gtk_object_unref (GTK_OBJECT (message));
	gtk_object_destroy (GTK_OBJECT (composer));
}


void
send_msg (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *composer;

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

	composer = e_msg_composer_new_from_url (url);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}	

static void
reply (FolderBrowser *fb, gboolean to_all)
{
	EMsgComposer *composer;

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

	fb = FOLDER_BROWSER (user_data);
	composer = mail_generate_forward (fb->mail_display->current_message,
					  TRUE, TRUE);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	gtk_widget_show (GTK_WIDGET (composer));	
}

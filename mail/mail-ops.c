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
#include <ctype.h>
#include <errno.h>
#include <gnome.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#include "mail.h"
#include "mail-threads.h"
#include "folder-browser.h"
#include "e-util/e-setup.h"
#include "filter/filter-editor.h"
#include "filter/filter-driver.h"
#include "widgets/e-table/e-table.h"
#include "mail-local.h"

/* FIXME: is there another way to do this? */
#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-client.h"

#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

struct post_send_data {
	CamelFolder *folder;
	const char *uid;
	guint32 flags;
};

typedef struct rfm_s { 
	FolderBrowser *fb; 
	MailConfigService *source;
} rfm_t;

typedef struct rsm_s {
	EMsgComposer *composer;
	CamelTransport *transport;
	CamelMimeMessage *message;
	const char *subject;
	char *from;
	struct post_send_data *psd;
	gboolean ok;
} rsm_t;

static void
real_fetch_mail( gpointer user_data );

static void
real_send_mail( gpointer user_data );

static void
cleanup_send_mail( gpointer userdata );

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

#ifdef USE_BROKEN_THREADS
static void
async_mail_exception_dialog (char *head, CamelException *ex, gpointer unused )
{
	mail_op_error( "%s: %s", head, camel_exception_get_description( ex ) );
}
#else
#define async_mail_exception_dialog mail_exception_dialog
#endif

static gboolean
check_configured (void)
{
	if (mail_config_is_configured ())
		return TRUE;
	
	mail_config_druid ();

	return mail_config_is_configured ();
}

static void
select_first_unread (CamelFolder *folder, int type, gpointer data)
{
	FolderBrowser *fb = data;
	ETable *table = E_TABLE_SCROLLED (fb->message_list->etable)->table;
	int mrow;

	mrow = e_table_view_to_model_row (table, 0);
	message_list_select (fb->message_list, mrow, MESSAGE_LIST_SELECT_NEXT,
			     0, CAMEL_MESSAGE_SEEN);
}

static CamelFolder *
filter_get_folder(FilterDriver *fd, const char *uri, void *data)
{
	return mail_uri_to_folder(uri);
}

static void
fetch_remote_mail (CamelFolder *source, CamelFolder *dest,
		   gboolean keep_on_server, FolderBrowser *fb,
		   CamelException *ex)
{
	CamelUIDCache *cache;
	GPtrArray *uids;
	int i;

	uids = camel_folder_get_uids (source);
	if (keep_on_server) {
		GPtrArray *new_uids;
		char *url, *p, *filename;

		url = camel_url_to_string (
			CAMEL_SERVICE (source->parent_store)->url, FALSE);
		for (p = url; *p; p++) {
			if (!isascii ((unsigned char)*p) ||
			    strchr (" /'\"`&();|<>${}!", *p))
				*p = '_';
		}
		filename = g_strdup_printf ("%s/config/cache-%s",
					    evolution_dir, url);
		g_free (url);

		cache = camel_uid_cache_new (filename);
		g_free (filename);
		if (cache) {
			new_uids = camel_uid_cache_get_new_uids (cache, uids);
			camel_folder_free_uids (source, uids);
			uids = new_uids;
		} else {
			async_mail_exception_dialog ("Could not read UID "
						     "cache file. You may "
						     "receive duplicate "
						     "messages.", NULL, fb);
		}
	} else
		cache = NULL;

	printf ("got %d new messages in source\n", uids->len);
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *msg;

		msg = camel_folder_get_message (source, uids->pdata[i], ex);
		if (camel_exception_is_set (ex)) {
			async_mail_exception_dialog ("Unable to get message",
						     ex, fb);
			goto done;
		}

		/* Append with flags = 0 since this is a new message */
		camel_folder_append_message (dest, msg, 0, ex);
		if (camel_exception_is_set (ex)) {
			async_mail_exception_dialog ("Unable to write message",
						     ex, fb);
			gtk_object_unref (GTK_OBJECT (msg));
			goto done;
		}

		if (!cache)
			camel_folder_delete_message (source, uids->pdata[i]);
		gtk_object_unref (GTK_OBJECT (msg));
	}

	camel_folder_sync (source, TRUE, ex);

 done:
	if (cache) {
		camel_uid_cache_free_uids (uids);
		if (!camel_exception_is_set (ex))
			camel_uid_cache_save (cache);
		camel_uid_cache_destroy (cache);
	} else
		camel_folder_free_uids (source, uids);
}

void
real_fetch_mail (gpointer user_data)
{
	rfm_t *info;
	FolderBrowser *fb = NULL;
	CamelException *ex;
	CamelStore *store = NULL, *dest_store = NULL;
	CamelFolder *folder = NULL, *dest_folder = NULL;
	char *url = NULL, *dest_url;
	FilterContext *fc = NULL;
	FilterDriver *driver = NULL;
	char *userrules, *systemrules;
	char *tmp_mbox = NULL, *source;
	guint handler_id = 0;
	struct stat st;
	gboolean keep;

	info = (rfm_t *) user_data;
	fb = info->fb;
	url = info->source->url;
	keep = info->source->keep_on_server;
	
	/* If using IMAP, don't do anything... */
	if (!strncmp (url, "imap:", 5))
		return;

	ex = camel_exception_new ();

	dest_url = g_strdup_printf ("mbox://%s/local/Inbox", evolution_dir);
	dest_store = camel_session_get_store (session, dest_url, ex);
	g_free (dest_url);
	if (!dest_store) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}
	
	dest_folder = camel_store_get_folder (dest_store, "mbox", FALSE, ex);
	if (!dest_folder) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	tmp_mbox = g_strdup_printf ("%s/local/Inbox/movemail", evolution_dir);

	/* If fetching mail from an mbox store, safely copy it to a
	 * temporary store first.
	 */
	if (!strncmp (url, "mbox:", 5)) {
		int tmpfd;

		tmpfd = open (tmp_mbox, O_RDWR | O_CREAT | O_APPEND, 0660);

		if (tmpfd == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Couldn't create temporary "
					      "mbox: %s", g_strerror (errno));
			async_mail_exception_dialog ("Unable to move mail", ex, fb );
			goto cleanup;
		}
		close (tmpfd);

		/* Skip over "mbox:" plus host part (if any) of url. */
		source = url + 5;
		if (!strncmp (source, "//", 2))
			source = strchr (source + 2, '/');

		camel_movemail (source, tmp_mbox, ex);
		if (camel_exception_is_set (ex)) {
			async_mail_exception_dialog ("Unable to move mail",
						     ex, fb);
			goto cleanup;
		}

		if (stat (tmp_mbox, &st) == -1 || st.st_size == 0) {
			gnome_ok_dialog ("No new messages.");
			goto cleanup;
		}

		folder = camel_store_get_folder (dest_store, "movemail",
						 FALSE, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			async_mail_exception_dialog ("Unable to move mail", ex, fb);
			goto cleanup;
		}
	} else {
		CamelFolder *sourcefolder;

		store = camel_session_get_store(session, url, ex);
 		if (!store) {
 			async_mail_exception_dialog("Unable to get new mail", ex, fb);
 			goto cleanup;
 		}

		camel_service_connect(CAMEL_SERVICE (store), ex);
		if (camel_exception_get_id(ex) != CAMEL_EXCEPTION_NONE) {
			if (camel_exception_get_id(ex) != CAMEL_EXCEPTION_USER_CANCEL)
				async_mail_exception_dialog("Unable to get new mail", ex, fb);
			goto cleanup;
		}

		sourcefolder = camel_store_get_folder(store, "inbox", FALSE, ex);
		if (camel_exception_get_id(ex) != CAMEL_EXCEPTION_NONE) {
			async_mail_exception_dialog("Unable to get new mail", ex, fb);
			goto cleanup;
		}

		/* can we perform filtering on this source? */
		if (!(sourcefolder->has_summary_capability
		      && sourcefolder->has_search_capability)) {
			folder = camel_store_get_folder (dest_store,
							 "movemail", TRUE, ex);
			if (camel_exception_is_set (ex)) {
				async_mail_exception_dialog ("Unable to move mail", ex, fb);
				goto cleanup;
			}

			fetch_remote_mail (sourcefolder, folder, keep, fb, ex);
			gtk_object_unref (GTK_OBJECT (sourcefolder));
			if (camel_exception_is_set (ex))
				goto cleanup;
		} else {
			folder = sourcefolder;
		}
	}

	if (camel_folder_get_message_count (folder) == 0) {
		gnome_ok_dialog ("No new messages.");
		goto cleanup;
	} else if (camel_exception_is_set (ex)) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	folder_browser_clear_search (fb);

	/* apply filtering rules to this inbox */
	fc = filter_context_new();
	userrules = g_strdup_printf("%s/filters.xml", evolution_dir);
	systemrules = g_strdup_printf("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	rule_context_load((RuleContext *)fc, systemrules, userrules);
	g_free (userrules);
	g_free (systemrules);

	driver = filter_driver_new(fc, filter_get_folder, 0);

	/* Attach a handler to the destination folder to select the first unread
	 * message iff it changes and iff it's the folder being viewed.
	 */
	if (dest_folder == fb->folder)
		handler_id = gtk_signal_connect (GTK_OBJECT (dest_folder), "folder_changed",
						 GTK_SIGNAL_FUNC (select_first_unread), fb);

	if (filter_driver_run(driver, folder, dest_folder) == -1) {
		async_mail_exception_dialog ("Unable to get new mail", ex, fb);
		goto cleanup;
	}

	if (dest_folder == fb->folder)
		gtk_signal_disconnect (GTK_OBJECT (dest_folder), handler_id);

 cleanup:
	if (stat (tmp_mbox, &st) == 0 && st.st_size == 0)
		unlink (tmp_mbox); /* FIXME: should use camel to do this */
	g_free (tmp_mbox);

	if (driver)
		gtk_object_unref((GtkObject *)driver);
	if (fc)
		gtk_object_unref((GtkObject *)fc);

	if (folder) {
		camel_folder_sync (folder, TRUE, ex);
		gtk_object_unref (GTK_OBJECT (folder));
	}

	if (dest_folder) {
		camel_folder_sync (dest_folder, TRUE, ex);
		gtk_object_unref (GTK_OBJECT (dest_folder));
	}

	if (store) {
		camel_service_disconnect (CAMEL_SERVICE (store), ex);
		gtk_object_unref (GTK_OBJECT (store));
	}

	if (dest_store && dest_store != fb->folder->parent_store) {
		camel_service_disconnect (CAMEL_SERVICE (dest_store), ex);
		gtk_object_unref (GTK_OBJECT (dest_store));
	}
	camel_exception_free (ex);
}

void
fetch_mail (GtkWidget *button, gpointer user_data)
{
	MailConfigService *source;
	rfm_t *info;

	if (!check_configured ())
		return;

	source = mail_config_get_default_source ();
	if (!source || !source->url) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);

		gnome_error_dialog_parented ("You have no remote mail source "
					     "configured", GTK_WINDOW (win));
		return;
	}

	/* This must be dynamically allocated so as not to be clobbered
	 * when we return. Actually, making it static in the whole file
	 * would probably work.
	 */

	info = g_new (rfm_t, 1);
	info->fb = FOLDER_BROWSER (user_data);
	info->source = source;

#ifdef USE_BROKEN_THREADS
	mail_operation_try (_("Fetching mail"), real_fetch_mail, NULL, info);
#else
	real_fetch_mail (info);
#endif
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	GtkWidget *message_box;
	int button;

	message_box = gnome_message_box_new (_("This message has no subject.\nReally send?"),
					     GNOME_MESSAGE_BOX_QUESTION,
					     GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO,
					     NULL);

	button = gnome_dialog_run_and_close (GNOME_DIALOG (message_box));

	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

static void
set_x_mailer_header (CamelMedium *medium)
{
	char *mailer_string;

	mailer_string = g_strdup_printf ("Evolution %s (Developer Preview)", VERSION);

	camel_medium_add_header (medium, "X-Mailer", mailer_string);

	g_free (mailer_string);
}

static void
real_send_mail (gpointer user_data)
{
	rsm_t *info = (rsm_t *) user_data;
	EMsgComposer *composer = NULL;
	CamelTransport *transport = NULL;
	CamelException *ex = NULL;
	CamelMimeMessage *message = NULL;
	const char *subject = NULL;
	char *from = NULL;
	struct post_send_data *psd = NULL;

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar ();
	mail_op_set_message ("Connecting to transport...");
#endif

	ex = camel_exception_new ();
	composer = info->composer;
	transport = info->transport;
	message = info->message;
	subject = info->subject;
	from = info->from;
	psd = info->psd;

	set_x_mailer_header (CAMEL_MEDIUM (message));

	camel_mime_message_set_from (message, from);
	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	camel_service_connect (CAMEL_SERVICE (transport), ex);

#ifdef USE_BROKEN_THREADS
	mail_op_set_message ("Connected. Sending...");
#endif

	if (!camel_exception_is_set (ex))
		camel_transport_send (transport, CAMEL_MEDIUM (message), ex);

	if (!camel_exception_is_set (ex)) {
#ifdef USE_BROKEN_THREADS
		mail_op_set_message ("Sent. Disconnecting...");
#endif 		
		camel_service_disconnect (CAMEL_SERVICE (transport), ex);
	}

	if (camel_exception_is_set (ex)) {
		async_mail_exception_dialog ("Could not send message", ex, composer);
		info->ok = FALSE;
	} else {
		if (psd) {
			camel_folder_set_message_flags (psd->folder, psd->uid,
							psd->flags, psd->flags);
		}
		info->ok = TRUE;

	}

	camel_exception_free (ex);
}

static void
cleanup_send_mail (gpointer userdata)
{
	rsm_t *info = (rsm_t *) userdata;
	
	if (info->ok) {
		gtk_object_destroy (GTK_OBJECT (info->composer));
	}

	gtk_object_unref (GTK_OBJECT (info->message));
	g_free (info);
}

static void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	const MailConfigIdentity *id = NULL; 
	static CamelTransport *transport = NULL;
	struct post_send_data *psd = data;
	rsm_t *info;
	static char *from = NULL;
	const char *subject;
	CamelException *ex;
	CamelMimeMessage *message;
	char *name, *addr;

	ex = camel_exception_new ();

	id = mail_config_get_default_identity ();
	
	if (!check_configured() || !id) {
		GtkWidget *message;

		message = gnome_warning_dialog_parented (_("You need to configure an identity\n"
							   "before you can send mail."),
							 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (composer),
											      GTK_TYPE_WINDOW)));
		gnome_dialog_run_and_close (GNOME_DIALOG (message));
		return;
	}

	if (!from) {
		CamelInternetAddress *ciaddr;

		g_assert (id);
		
		name = id->name;
		g_assert (name);

		addr = id->address;
		g_assert (addr);

		ciaddr = camel_internet_address_new ();
		camel_internet_address_add (ciaddr, name, addr);

		from = camel_address_encode (CAMEL_ADDRESS (ciaddr));
	}

	if (!transport) {
		MailConfigService *t;
		char *url;

		t = mail_config_get_transport ();
		url = t->url;
		g_assert (url);

		transport = camel_session_get_transport (session, url, ex);
		if (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE) {
			mail_exception_dialog ("Could not load mail transport",
					       ex, composer);
			camel_exception_free (ex);
			return;
		}
	}

	message = e_msg_composer_get_message (composer);

	subject = camel_mime_message_get_subject (message);
	if (!subject || !*subject) {
		if (!ask_confirm_for_empty_subject (composer)) {
			gtk_object_unref (GTK_OBJECT (message));
			return;
		}
	}

	info = g_new0 (rsm_t, 1);
	info->composer = composer;
	info->transport = transport;
	info->message = message;
	info->subject = subject;
	info->from = from;
	info->psd = psd;

#ifdef USE_BROKEN_THREADS
	mail_operation_try ("Send Message", real_send_mail, cleanup_send_mail, info);
#else
	real_send_mail (info);
	cleanup_send_mail (info);
#endif
}

static void
free_psd (GtkWidget *composer, gpointer user_data)
{
	struct post_send_data *psd = user_data;

	gtk_object_unref (GTK_OBJECT (psd->folder));
	g_free (psd);
}

static GtkWidget *
create_msg_composer (const char *url)
{
	MailConfigIdentity *id;
	gboolean send_html;
	gchar *sig_file = NULL;
	GtkWidget *composer_widget;

	id = mail_config_get_default_identity ();
	send_html = mail_config_send_html ();
	
	if (id) {
		sig_file = id->sig;
	}
	
	if (url != NULL)
		composer_widget = e_msg_composer_new_from_url (url);
	else
		composer_widget = e_msg_composer_new_with_sig_file (sig_file);

	e_msg_composer_set_send_html (E_MSG_COMPOSER (composer_widget), 
				      send_html);

	return composer_widget;
}

void
compose_msg (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *composer;
	
	if (!check_configured ())
		return;
	
	composer = create_msg_composer (NULL);
	
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

	composer = create_msg_composer (url);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}	

static void
reply (FolderBrowser *fb, gboolean to_all)
{
	EMsgComposer *composer;
	struct post_send_data *psd;

	if (!check_configured () || !fb->message_list->cursor_uid ||
	    !fb->mail_display->current_message)
		return;

	psd = g_new (struct post_send_data, 1);
	psd->folder = fb->folder;
	gtk_object_ref (GTK_OBJECT (psd->folder));
	psd->uid = fb->message_list->cursor_uid;
	psd->flags = CAMEL_MESSAGE_ANSWERED;

	composer = mail_generate_reply (fb->mail_display->current_message, to_all);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), psd); 
	gtk_signal_connect (GTK_OBJECT (composer), "destroy",
			    GTK_SIGNAL_FUNC (free_psd), psd); 

	gtk_widget_show (GTK_WIDGET (composer));	
}

void
reply_to_sender (GtkWidget *widget, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), FALSE);
}

void
reply_to_all (GtkWidget *widget, gpointer user_data)
{
	reply (FOLDER_BROWSER (user_data), TRUE);
}

static void
attach_msg (MessageList *ml, const char *uid, gpointer data)
{
	EMsgComposer *composer = data;
	CamelMimeMessage *message;
	CamelMimePart *part;
	const char *subject;
	char *desc;
	
	message = camel_folder_get_message (ml->folder, uid, NULL);
	if (!message)
		return;
	subject = camel_mime_message_get_subject (message);
	if (subject)
		desc = g_strdup_printf ("Forwarded message - %s", subject);
	else
		desc = g_strdup ("Forwarded message");
	
	part = camel_mime_part_new ();
	camel_mime_part_set_disposition (part, "inline");
	camel_mime_part_set_description (part, desc);
	camel_medium_set_content_object (CAMEL_MEDIUM (part),
					 CAMEL_DATA_WRAPPER (message));
	camel_mime_part_set_content_type (part, "message/rfc822");
	
	e_msg_composer_attach (composer, part);
	
	gtk_object_unref (GTK_OBJECT (part));
	gtk_object_unref (GTK_OBJECT (message));
	g_free (desc);
}

void
forward_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	EMsgComposer *composer;
	CamelMimeMessage *cursor_msg;
	const char *from, *subject;
	char *fwd_subj;
	
	cursor_msg = fb->mail_display->current_message;
	if (!check_configured () || !cursor_msg)
		return;

	composer = E_MSG_COMPOSER (create_msg_composer (NULL));
	message_list_foreach (fb->message_list, attach_msg, composer);

	from = camel_mime_message_get_from (cursor_msg);
	subject = camel_mime_message_get_subject (cursor_msg);
	if (from) {
		if (subject && *subject) {
			fwd_subj = g_strdup_printf ("[%s] %s", from, subject);
		} else {
			fwd_subj = g_strdup_printf ("[%s] (forwarded message)",
						    from);
		}
	} else {
		fwd_subj = NULL;
	}

	e_msg_composer_set_headers (composer, NULL, NULL, NULL, fwd_subj);
	g_free (fwd_subj);

	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	gtk_widget_show (GTK_WIDGET (composer));	
}

struct move_data {
	CamelFolder *source, *dest;
	CamelException *ex;
};

static void
real_move_msg (MessageList *ml, const char *uid, gpointer user_data)
{
	struct move_data *rfd = user_data;

	if (camel_exception_is_set (rfd->ex))
		return;

	camel_folder_move_message_to (rfd->source, uid, rfd->dest, rfd->ex);
}

void
move_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	char *uri, *physical, *path;
	struct move_data rfd;
	const char *allowed_types[] = { "mail", NULL };
	extern EvolutionShellClient *global_shell_client;
	static char *last = NULL;

	if (!last)
		last = g_strdup ("");

	evolution_shell_client_user_select_folder  (global_shell_client,
						    _("Move message(s) to"),
						    last, allowed_types, &uri, &physical);
	if (!uri)
		return;

	path = strchr (uri, '/');
	if (path && strcmp (last, path) != 0) {
		g_free (last);
		last = g_strdup (path);
	}
	g_free (uri);

	rfd.source = ml->folder;
	rfd.dest = mail_uri_to_folder (physical);
	g_free (physical);
	if (!rfd.dest)
		return;
	rfd.ex = camel_exception_new ();
	
	message_list_foreach (ml, real_move_msg, &rfd);
	gtk_object_unref (GTK_OBJECT (rfd.dest));
	
	if (camel_exception_is_set (rfd.ex))
		mail_exception_dialog ("Could not move message", rfd.ex, fb);
	camel_exception_free (rfd.ex);
}

void
mark_all_seen (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	MessageList *ml = fb->message_list;
	GPtrArray *uids;
	int i;

	uids = camel_folder_get_uids (ml->folder);
	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_flags (ml->folder, uids->pdata[i],
						CAMEL_MESSAGE_SEEN,
						CAMEL_MESSAGE_SEEN);
	}
}

static void
real_edit_msg (MessageList *ml, const char *uid, gpointer user_data)
{
	CamelException *ex = user_data;
	CamelMimeMessage *msg;
	GtkWidget *composer;
	
	if (camel_exception_is_set (ex))
		return;
	
	msg = camel_folder_get_message (ml->folder, uid, ex);
	
	composer = e_msg_composer_new_with_message (msg);
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_widget_show (composer);
}

void
edit_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	MessageList *ml = fb->message_list;
	CamelException ex;
	extern CamelFolder *drafts_folder;
	
	camel_exception_init (&ex);
	
	if (fb->folder != drafts_folder) {
		camel_exception_setv (&ex, CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				      "FIXME: some error message about not being in the Drafts folder...");
		mail_exception_dialog ("Could not open message for editing", &ex, fb);
		return;
	}
	
	message_list_foreach (ml, real_edit_msg, &ex);
	if (camel_exception_is_set (&ex)) {
		mail_exception_dialog ("Could not open message for editing", &ex, fb);
		camel_exception_clear (&ex);
		return;
	}
}

void
edit_message (BonoboUIHandler *uih, void *user_data, const char *path)
{
	edit_msg (NULL, user_data);
}

static void
real_delete_msg (MessageList *ml, const char *uid, gpointer user_data)
{
	CamelException *ex = user_data;
	guint32 flags;

	if (camel_exception_is_set (ex))
		return;

	/* Toggle the deleted flag without touching other flags. */
	flags = camel_folder_get_message_flags (ml->folder, uid);
	camel_folder_set_message_flags (ml->folder, uid,
					CAMEL_MESSAGE_DELETED, ~flags);
}

void
delete_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	CamelException ex;

	camel_exception_init (&ex);
	message_list_foreach (ml, real_delete_msg, &ex);
	if (camel_exception_is_set (&ex)) {
		mail_exception_dialog ("Could not toggle deleted flag",
				       &ex, fb);
		camel_exception_clear (&ex);
		return;
	}
}

static void real_expunge_folder (gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	CamelException ex;

	e_table_model_pre_change(fb->message_list->table_model);

#ifdef USE_BROKEN_THREADS
	mail_op_hide_progressbar ();
	mail_op_set_message ("Expunging %s...", fb->message_list->folder->full_name);
#endif

	camel_exception_init (&ex);

	camel_folder_expunge (fb->message_list->folder, &ex);

	/* FIXME: is there a better way to force an update? */
	/* FIXME: Folder should raise a signal to say its contents has changed ... */
	e_table_model_changed (fb->message_list->table_model);

	if (camel_exception_get_id (&ex) != CAMEL_EXCEPTION_NONE) {
		async_mail_exception_dialog ("Unable to expunge deleted messages", &ex, fb);
	}
}

void
expunge_folder (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);

	if (fb->message_list->folder) {
#ifdef USE_BROKEN_THREADS
		mail_operation_try ("Expunge Folder", real_expunge_folder, NULL, fb);
#else
		real_expunge_folder (fb);
#endif
	}
}

static void
filter_druid_clicked(GtkWidget *w, int button, FolderBrowser *fb)
{
	FilterContext *fc;

	if (button == 0) {
		char *user;

		fc = gtk_object_get_data((GtkObject *)w, "context");
		user = g_strdup_printf("%s/filters.xml", evolution_dir);
		rule_context_save((RuleContext *)fc, user);
		g_free(user);
	}
	
	if (button != -1) {
		gnome_dialog_close((GnomeDialog *)w);
	}
}

void
filter_edit (BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	FilterContext *fc;
	char *user, *system;
	GtkWidget *w;

	fc = filter_context_new();
	user = g_strdup_printf("%s/filters.xml", evolution_dir);
	system = g_strdup_printf("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	rule_context_load((RuleContext *)fc, system, user);
	g_free(user);
	g_free(system);
	w = filter_editor_construct(fc);
	gtk_object_set_data_full((GtkObject *)w, "context", fc, (GtkDestroyNotify)gtk_object_unref);
	gtk_signal_connect((GtkObject *)w, "clicked", filter_druid_clicked, fb);
	gtk_widget_show(w);
}

void
vfolder_edit_vfolders (BonoboUIHandler *uih, void *user_data, const char *path)
{
	void vfolder_edit(void);

	vfolder_edit();
}

void
providers_config (BonoboUIHandler *uih, void *user_data, const char *path)
{
	mail_config();
}

void
print_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	GnomePrintMaster *print_master;
	GnomePrintContext *print_context;
	GtkWidget *preview;

	print_master = gnome_print_master_new ();

	print_context = gnome_print_master_get_context (print_master);
	gtk_html_print (fb->mail_display->html, print_context);

	preview = GTK_WIDGET (gnome_print_master_preview_new (
		print_master, "Mail Print Preview"));
	gtk_widget_show (preview);

	gtk_object_unref (GTK_OBJECT (print_master));
}

void
configure_folder(BonoboUIHandler *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);

	local_reconfigure_folder(fb);
}


struct view_msg_data {
	FolderBrowser *fb;
	CamelException *ex;
};

static void
real_view_msg (MessageList *ml, const char *uid, gpointer user_data)
{
	struct view_msg_data *data = user_data;
	CamelMimeMessage *msg;
	GtkWidget *view;
	
	if (camel_exception_is_set (data->ex))
		return;
	
	msg = camel_folder_get_message (ml->folder, uid, data->ex);
	
	view = mail_view_create (msg, data->fb);
	
	gtk_widget_show (view);
}

void
view_msg (GtkWidget *widget, gpointer user_data)
{
	struct view_msg_data data;
	FolderBrowser *fb = user_data;
	FolderBrowser *folder_browser;
	CamelException ex;
	MessageList *ml;
	
	camel_exception_init (&ex);
	
	folder_browser = FOLDER_BROWSER (folder_browser_new ());
	folder_browser_set_uri (folder_browser, fb->uri);
	
	data.fb = folder_browser;
	data.ex = &ex;
	
	ml = fb->message_list;
	message_list_foreach (ml, real_view_msg, &data);
	if (camel_exception_is_set (&ex)) {
		mail_exception_dialog ("Could not open message for viewing", &ex, fb);
		camel_exception_clear (&ex);
		return;
	}
}

void
view_message (BonoboUIHandler *uih, void *user_data, const char *path)
{
	view_msg (NULL, user_data);
}

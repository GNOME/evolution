/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Authors: 
 *  Dan Winship <danw@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
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
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#include "mail.h"
#include "mail-callbacks.h"
#include "mail-config.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-local.h"
#include "mail-vfolder.h"
#include "folder-browser.h"
#include "subscribe-dialog.h"
#include "filter/filter-editor.h"
#include "filter/filter-driver.h"
#include <gal/e-table/e-table.h>
#include <gal/widgets/e-gui-utils.h>

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
	gchar *uid;
	guint32 flags;
};

static gboolean
check_configured (FolderBrowser *fb)
{
	if (mail_config_is_configured ())
		return TRUE;
	
	if (fb) {
		GtkWidget *dialog;
		
		dialog = gnome_message_box_new (_("You have not configured the mail client.\n"
						  "You need to do this before you can send,\n"
						  "receive or compose mail.\n"
						  "Would you like to configure it now?"),
						GNOME_MESSAGE_BOX_QUESTION,
						GNOME_STOCK_BUTTON_YES,
						GNOME_STOCK_BUTTON_NO, NULL);
		gnome_dialog_set_parent (GNOME_DIALOG (dialog),
					 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET(fb), GTK_TYPE_WINDOW)));
		
		switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
		case 0:
			mail_config_druid (fb->shell);
			break;
		case 1:
		default:
			break;
		}
		
		return mail_config_is_configured ();
	} else
		return FALSE;
}

static gboolean
check_send_configuration (FolderBrowser *fb)
{
	MailConfigService *xport = NULL;
	
	/* Check general */
	
	if (!check_configured (fb)) {
		return FALSE;
	}
	
	/* Check for an identity */
	
	if (!mail_config_get_default_identity ()) {
		GtkWidget *message;
		
		message = gnome_warning_dialog_parented (_("You need to configure an identity\n"
							   "before you can compose mail."),
							 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb),
											      GTK_TYPE_WINDOW)));
		gnome_dialog_run_and_close (GNOME_DIALOG (message));
		return FALSE;
	}
	
	/* Check for a transport */
	
	xport = mail_config_get_transport ();
	if (!xport || !xport->url) {
		GtkWidget *message;
		
		message = gnome_warning_dialog_parented (_("You need to configure a mail transport\n"
							   "before you can compose mail."),
							 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb),
											      GTK_TYPE_WINDOW)));
		gnome_dialog_run_and_close (GNOME_DIALOG (message));
		return FALSE;
	}
	
	return TRUE;
}

static void
main_select_first_unread (CamelObject *object, gpointer event_data, gpointer data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	/*ETable *table = E_TABLE_SCROLLED (fb->message_list->etable)->table;*/
	
	message_list_select (fb->message_list, 0, MESSAGE_LIST_SELECT_NEXT,
  			     0, CAMEL_MESSAGE_SEEN);
}

static void
select_first_unread (CamelObject *object, gpointer event_data, gpointer data)
{
	mail_op_forward_event (main_select_first_unread, object, event_data, data);
}

void
fetch_mail (GtkWidget *widget, gpointer user_data)
{
	GSList *sources;
	
	if (!check_configured (FOLDER_BROWSER (user_data))) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);
		
		gnome_error_dialog_parented (_("You have no mail sources "
					       "configured"),
					     GTK_WINDOW (win));
		return;
	}
	
	sources = mail_config_get_sources ();
	
	if (!sources || !sources->data) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);
		
		gnome_error_dialog_parented (_("You have no mail sources "
					       "configured"),
					     GTK_WINDOW (win));
		return;
	}
	
	while (sources) {
		MailConfigService *source;
		
		source = (MailConfigService *) sources->data;
		sources = sources->next;
		
		if (!source || !source->url) {
			g_warning ("Bad source in fetch_mail??");
			continue;
		}
		
		mail_do_fetch_mail (source->url, source->keep_on_server, 
				    NULL, select_first_unread, user_data);
	}
}

void
send_queued_mail (GtkWidget *widget, gpointer user_data)
{
	extern CamelFolder *outbox_folder;
	MailConfigService *transport;
	
	if (!mail_config_is_configured ()) {
		return;
	}
	
	transport = mail_config_get_transport ();
	if (!transport) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);
		
		gnome_error_dialog_parented (_("You have not set a mail transport method"),
					     GTK_WINDOW (win));
		return;
	}
	
	if (!outbox_folder) {
		GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (user_data),
							  GTK_TYPE_WINDOW);
		
		gnome_error_dialog_parented (_("You have no Outbox configured"),
					     GTK_WINDOW (win));
		return;
	}
	
	mail_do_send_queue (outbox_folder, transport->url);
	
	mail_do_expunge_folder (outbox_folder);
}

void
send_receieve_mail (GtkWidget *widget, gpointer user_data)
{
	/* receive first then send, this is a temp fix for POP-before-SMTP */
	fetch_mail (widget, user_data);
	send_queued_mail (widget, user_data);
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
free_psd (GtkWidget *composer, gpointer user_data)
{
	struct post_send_data *psd = user_data;
	
	if (psd->folder)
		camel_object_unref (CAMEL_OBJECT (psd->folder));
	if (psd->uid)
		g_free (psd->uid);
	g_free (psd);
}

void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	MailConfigService *xport = NULL;
	CamelMimeMessage *message;
	const CamelInternetAddress *iaddr;
	const char *subject;
	struct post_send_data *psd = data;
	
	/* Config info */
	xport = mail_config_get_transport ();
	
	/* Get the message */
	message = e_msg_composer_get_message (composer);
	
	/* Check for no recipients */
	iaddr = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	if (!iaddr || CAMEL_ADDRESS (iaddr)->addresses->len == 0) {
		GtkWidget *message_box;
		
		message_box = gnome_message_box_new (_("You must specify recipients in order to send this message."),
						     GNOME_MESSAGE_BOX_WARNING,
						     GNOME_STOCK_BUTTON_OK,
						     NULL);
		
		gnome_dialog_run_and_close (GNOME_DIALOG (message_box));
		
		camel_object_unref (CAMEL_OBJECT (message));
		return;
	}
	
	/* Check for no subject */
	subject = camel_mime_message_get_subject (message);
	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer)) {
			camel_object_unref (CAMEL_OBJECT (message));
			return;
		}
	}
	
	if (psd) {
		mail_do_send_mail (xport->url, message, 
				   psd->folder, psd->uid, psd->flags, 
				   GTK_WIDGET (composer));
	} else {
		mail_do_send_mail (xport->url, message, NULL, NULL, 0,
				   GTK_WIDGET (composer));
	}
}

void
composer_postpone_cb (EMsgComposer *composer, gpointer data)
{
	extern CamelFolder *outbox_folder;
	CamelMimeMessage *message;
	struct post_send_data *psd = data;
	const char *subject;
	
	/* Get the message */
	message = e_msg_composer_get_message (composer);
	
	/* Check for no subject */
	subject = camel_mime_message_get_subject (message);
	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer)) {
			camel_object_unref (CAMEL_OBJECT (message));
			return;
		}
	}
	
	/* Save the message in Outbox */
	mail_do_append_mail (outbox_folder, message, NULL);
	
	if (psd) {
		guint32 set;
		
		set = camel_folder_get_message_flags (psd->folder, psd->uid);
		camel_folder_set_message_flags (psd->folder, psd->uid,
						psd->flags, psd->flags);
	}
	
	gtk_widget_destroy (GTK_WIDGET (composer));
}

static GtkWidget *
create_msg_composer (const char *url)
{
       MailConfigIdentity *id;
       gboolean send_html;
       gchar *sig_file = NULL;
       EMsgComposer *composer;
       
       id = mail_config_get_default_identity ();
       send_html = mail_config_send_html ();
       
       if (id)
               sig_file = id->sig;
       
       if (url != NULL) {
               composer = e_msg_composer_new_from_url (url);
	       if (composer)
		       e_msg_composer_set_send_html (composer, send_html);
       } else
               composer = e_msg_composer_new_with_sig_file (sig_file, send_html);

       return (GtkWidget *)composer;
}

void
compose_msg (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *composer;
	
	if (!check_send_configuration (FOLDER_BROWSER (user_data)))
		return;
	
	composer = create_msg_composer (NULL);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
	
	gtk_widget_show (composer);
}

/* Send according to a mailto (RFC 2368) URL. */
void
send_to_url (const char *url)
{
	GtkWidget *composer;
	
	/* FIXME: no way to get folder browser? Not without
	 * big pain in the ass, as far as I can tell */
	if (!check_send_configuration (NULL))
		return;
	
	composer = create_msg_composer (url);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
	
	gtk_widget_show (composer);
}	

void
mail_reply (CamelFolder *folder, CamelMimeMessage *msg, const char *uid, gboolean to_all)
{
	EMsgComposer *composer;
	struct post_send_data *psd;
	
	g_return_if_fail (folder != NULL);
	g_return_if_fail (msg != NULL);
	g_return_if_fail (uid != NULL);

	psd = g_new (struct post_send_data, 1);
	psd->folder = folder;
	camel_object_ref (CAMEL_OBJECT (psd->folder));
	psd->uid = g_strdup (uid);
	psd->flags = CAMEL_MESSAGE_ANSWERED;
	
	composer = mail_generate_reply (msg, to_all);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), psd);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), psd);
	gtk_signal_connect (GTK_OBJECT (composer), "destroy",
			    GTK_SIGNAL_FUNC (free_psd), psd);
	
	gtk_widget_show (GTK_WIDGET (composer));	
}

void
reply_to_sender (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (!check_send_configuration (fb))
		return;

	mail_reply (fb->folder, fb->mail_display->current_message, 
	       fb->message_list->cursor_uid, FALSE);
}

void
reply_to_all (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);

	if (!check_send_configuration (fb))
		return;

	mail_reply (fb->folder, fb->mail_display->current_message, 
	       fb->message_list->cursor_uid, TRUE);
}

void
enumerate_msg (MessageList *ml, const char *uid, gpointer data)
{
	g_ptr_array_add ((GPtrArray *) data, g_strdup (uid));
}

static void
forward_message (FolderBrowser *fb, gboolean attach)
{
	EMsgComposer *composer;
	CamelMimeMessage *cursor_msg;
	GPtrArray *uids;
	
	cursor_msg = fb->mail_display->current_message;
	g_return_if_fail (cursor_msg != NULL);
	
	if (!check_send_configuration (fb))
		return;
	
	composer = e_msg_composer_new ();
	if (!composer)
		return;
	
	uids = g_ptr_array_new ();
	if (attach)
		message_list_foreach (fb->message_list, enumerate_msg, uids);
	else
		g_ptr_array_add (uids, g_strdup (fb->message_list->cursor_uid));
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
	
	mail_do_forward_message (cursor_msg,
				 fb->message_list->folder,
				 uids, composer, attach);
}

void
forward_inlined (GtkWidget *widget, gpointer user_data)
{
	forward_message (FOLDER_BROWSER (user_data), FALSE);
}

void
forward_attached (GtkWidget *widget, gpointer user_data)
{
	forward_message (FOLDER_BROWSER (user_data), TRUE);
}

static void
transfer_msg (GtkWidget *widget, gpointer user_data, gboolean delete_from_source)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	GPtrArray *uids;
	char *uri, *physical, *path;
	char *desc;
	const char *allowed_types[] = { "mail", NULL };
	extern EvolutionShellClient *global_shell_client;
	static char *last = NULL;
	
	if (last == NULL)
		last = g_strdup ("");
	
	if (delete_from_source)
		desc = _("Move message(s) to");
	else
		desc = _("Copy message(s) to");
	
	evolution_shell_client_user_select_folder  (global_shell_client,
						    desc,
						    last, allowed_types, &uri, &physical);
	if (!uri)
		return;
	
	path = strchr (uri, '/');
	if (path && strcmp (last, path) != 0) {
		g_free (last);
		last = g_strdup (path);
	}
	g_free (uri);
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	mail_do_transfer_messages (ml->folder, uids, delete_from_source, physical);
}

void
move_msg (GtkWidget *widget, gpointer user_data)
{
	transfer_msg (widget, user_data, TRUE);
}

void
copy_msg (GtkWidget *widget, gpointer user_data)
{
	transfer_msg (widget, user_data, FALSE);
}

void
apply_filters (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
        MessageList *ml = fb->message_list;
	GPtrArray *uids;
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	
	mail_do_filter_ondemand (fb->folder, uids);
}

void
select_all (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
        MessageList *ml = fb->message_list;
	ETableScrolled *scrolled;
	
	if (ml->folder == NULL)
		return;
	
	scrolled = E_TABLE_SCROLLED (ml->etable);
	e_table_select_all (scrolled->table);
}

void
invert_selection (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
        MessageList *ml = fb->message_list;
	ETableScrolled *scrolled;
	
	if (ml->folder == NULL)
		return;
	
	scrolled = E_TABLE_SCROLLED (ml->etable);
	e_table_invert_selection (scrolled->table);
}

void
mark_as_seen (BonoboUIComponent *uih, void *user_data, const char *path)
{
        FolderBrowser *fb = FOLDER_BROWSER(user_data);
        MessageList *ml = fb->message_list;
	GPtrArray *uids;
	
	if (ml->folder == NULL)
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	mail_do_flag_messages (ml->folder, uids, FALSE,
			       CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

void
mark_as_unseen (BonoboUIComponent *uih, void *user_data, const char *path)
{
        FolderBrowser *fb = FOLDER_BROWSER(user_data);
        MessageList *ml = fb->message_list;
	GPtrArray *uids;
	
	if (ml->folder == NULL)
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	mail_do_flag_messages (ml->folder, uids, FALSE,
			       CAMEL_MESSAGE_SEEN, 0);
}
				   
void
edit_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	extern CamelFolder *drafts_folder;
	
	if (fb->folder != drafts_folder) {
		GtkWidget *message;
		
		message = gnome_warning_dialog (_("You may only edit messages saved\n"
						  "in the Drafts folder."));
		gnome_dialog_run_and_close (GNOME_DIALOG (message));
		return;
	}
	
	if (!check_send_configuration (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	/* FIXME: do we need to pass the postpone callback too? */
	mail_do_edit_messages (fb->folder, uids, (GtkSignalFunc) composer_send_cb);
}

static void
save_msg_ok (GtkWidget *widget, gpointer user_data)
{
	CamelFolder *folder;
	GPtrArray *uids;
	char *path;
	int fd, ret = 0;
	
	/* FIXME: is path an allocated string? */
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (user_data));
	
        fd = open (path, O_RDONLY);
	if (fd != -1) {
		GtkWidget *dlg;
		GtkWidget *text;
		
		close (fd);
		
		dlg = gnome_dialog_new (_("Overwrite file?"),
					GNOME_STOCK_BUTTON_YES, 
					GNOME_STOCK_BUTTON_NO,
					NULL);
		text = gtk_label_new (_("A file by that name already exists.\nOverwrite it?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), text, TRUE, TRUE, 4);
		gtk_window_set_policy (GTK_WINDOW (dlg), FALSE, TRUE, FALSE);
		gtk_widget_show (text);
		
		ret = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
	}
	
	if (ret == 0) {
		folder = gtk_object_get_data (GTK_OBJECT (user_data), "folder");
		uids = gtk_object_get_data (GTK_OBJECT (user_data), "uids");
		gtk_object_remove_no_notify (GTK_OBJECT (user_data), "uids");
		mail_do_save_messages (folder, uids, path);
	}
	
	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static void
save_msg_destroy (gpointer user_data)
{
	GPtrArray *uids = user_data;
	
	if (uids) {
		int i;
		
		for (i = 0; i < uids->len; i++)
			g_free (uids->pdata[i]);
		
		g_ptr_array_free (uids, TRUE);
	}
}

void
save_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GtkFileSelection *filesel;
	GPtrArray *uids;
	char *title;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len == 1)
		title = _("Save Message As...");
	else
		title = _("Save Messages As...");
	
	filesel = GTK_FILE_SELECTION (gtk_file_selection_new (title));
	gtk_object_set_data_full (GTK_OBJECT (filesel), "uids", uids, save_msg_destroy);
	gtk_object_set_data (GTK_OBJECT (filesel), "folder", fb->folder);
	gtk_signal_connect (GTK_OBJECT (filesel->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (save_msg_ok), filesel);
	gtk_signal_connect_object (GTK_OBJECT (filesel->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (filesel));
	
	gtk_widget_show (GTK_WIDGET (filesel));
}

void
delete_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	GPtrArray *uids;
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	
	/*
	 * Toggling a flag is an "instantaneous" operation, so if
	 * we're only doing one, just do it and return, rather than
	 * queueing it for the other thread. This makes the "Delete"
	 * key work correctly (move to the next message) again.
	 * - Dan
	 */
	if (uids->len == 1) {
		char *uid = uids->pdata[0];
		
		mail_tool_camel_lock_up ();
		camel_folder_set_message_flags (ml->folder, uid,
						CAMEL_MESSAGE_DELETED,
						CAMEL_MESSAGE_DELETED);
		mail_tool_camel_lock_down ();
	} else {
		mail_do_flag_messages (ml->folder, uids, FALSE,
				       CAMEL_MESSAGE_DELETED,
				       CAMEL_MESSAGE_DELETED);
	}
}

void
undelete_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	MessageList *ml = fb->message_list;
	GPtrArray *uids;
	
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	
	/*
	 * Toggling a flag is an "instantaneous" operation, so if
	 * we're only doing one, just do it and return, rather than
	 * queueing it for the other thread. This makes the "Delete"
	 * key work correctly (move to the next message) again.
	 * - Dan
	 */
	if (uids->len == 1) {
		char *uid = uids->pdata[0];
		
		mail_tool_camel_lock_up ();
		camel_folder_set_message_flags (ml->folder, uid,
						CAMEL_MESSAGE_DELETED,
						0);
		mail_tool_camel_lock_down ();
	} else {
		mail_do_flag_messages (ml->folder, uids, FALSE,
				       CAMEL_MESSAGE_DELETED,
				       0);
	}
}

void
expunge_folder (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	
	e_table_model_pre_change (fb->message_list->table_model);
	
	if (fb->message_list->folder)
		mail_do_expunge_folder (fb->message_list->folder);
}

static void
filter_druid_clicked (GtkWidget *w, int button, FolderBrowser *fb)
{
	FilterContext *fc;
	
	if (button == 0) {
		char *user;
		
		fc = gtk_object_get_data (GTK_OBJECT (w), "context");
		user = g_strdup_printf ("%s/filters.xml", evolution_dir);
		rule_context_save ((RuleContext *)fc, user);
		g_free (user);
	}
	
	if (button != -1) {
		gnome_dialog_close (GNOME_DIALOG (w));
	}
}

void
filter_edit (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	FilterContext *fc;
	char *user, *system;
	GtkWidget *w;
	
	fc = filter_context_new ();
	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
	system = g_strdup_printf ("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	rule_context_load ((RuleContext *)fc, system, user);
	g_free (user);
	g_free (system);
	
	if (((RuleContext *)fc)->error) {
		GtkWidget *dialog;
		gchar *err;
		
		err = g_strdup_printf (_("Error loading filter information:\n%s"),
				       ((RuleContext *)fc)->error);
		dialog = gnome_warning_dialog (err);
		g_free (err);
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		return;
	}
	
	w = filter_editor_construct (fc);
	gtk_object_set_data_full (GTK_OBJECT (w), "context", fc, (GtkDestroyNotify)gtk_object_unref);
	gtk_signal_connect (GTK_OBJECT (w), "clicked", filter_druid_clicked, fb);
	gtk_widget_show (GTK_WIDGET (w));
}

void
vfolder_edit_vfolders (BonoboUIComponent *uih, void *user_data, const char *path)
{
	vfolder_edit ();
}

void
providers_config (BonoboUIComponent *uih, void *user_data, const char *path)
{
	mail_config ((FOLDER_BROWSER (user_data))->shell);
}

/*
 * FIXME: This routine could be made generic, by having a closure
 * function plus data, and having the whole process be taken care
 * of for you
 */
static void
do_mail_print (MailDisplay *md, gboolean preview)
{
	GnomePrintContext *print_context;
	GnomePrintMaster *print_master;
	GnomePrintDialog *gpd;
	GnomePrinter *printer = NULL;
	int copies = 1;
	int collate = FALSE;

	if (!preview){

		gpd = GNOME_PRINT_DIALOG (
			gnome_print_dialog_new (_("Print Message"), GNOME_PRINT_DIALOG_COPIES));
		gnome_dialog_set_default (GNOME_DIALOG (gpd), GNOME_PRINT_PRINT);

		switch (gnome_dialog_run (GNOME_DIALOG (gpd))){
		case GNOME_PRINT_PRINT:
			break;
			
		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;

		case -1:
			return;

		default:
			gnome_dialog_close (GNOME_DIALOG (gpd));
			return;
		}

		gnome_print_dialog_get_copies (gpd, &copies, &collate);
		printer = gnome_print_dialog_get_printer (gpd);
		gnome_dialog_close (GNOME_DIALOG (gpd));
	}

	print_master = gnome_print_master_new ();

/*	FIXME: set paper size gnome_print_master_set_paper (print_master,  */

	if (printer)
		gnome_print_master_set_printer (print_master, printer);
	gnome_print_master_set_copies (print_master, copies, collate);
	print_context = gnome_print_master_get_context (print_master);
	gtk_html_print (md->html, print_context);
	gnome_print_master_close (print_master);

	if (preview){
		gboolean landscape = FALSE;
		GnomePrintMasterPreview *preview;
		
		preview = gnome_print_master_preview_new_with_orientation (
			print_master, _("Print Preview"), landscape);
		gtk_widget_show (GTK_WIDGET (preview));
	} else {
		int result = gnome_print_master_print (print_master);

		if (result == -1){
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("Printing of message failed"));
		}
	}
	gtk_object_unref (GTK_OBJECT (print_master));
}

void
mail_print_preview_msg (MailDisplay *md)
{
	do_mail_print (md, TRUE);
}

void
mail_print_msg (MailDisplay *md)
{
	do_mail_print (md, FALSE);
}

void
print_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;

	mail_print_msg (fb->mail_display);
}

void
print_preview_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = user_data;

	mail_print_preview_msg (fb->mail_display);
}

void
manage_subscriptions (BonoboUIComponent *uih, void *user_data, const char *path)
{
	/* XXX pass in the selected storage */
	GtkWidget *subscribe = subscribe_dialog_new ((FOLDER_BROWSER (user_data))->shell);

	gtk_widget_show (subscribe);
}

void
configure_folder (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);
	
	local_reconfigure_folder(fb);
}

void
view_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	GPtrArray *uids;
	
	if (!fb->folder)
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	mail_do_view_messages (fb->folder, uids, fb);
}

void
view_message (BonoboUIComponent *uih, void *user_data, const char *path)
{
        view_msg (NULL, user_data);
}

void
edit_message (BonoboUIComponent *uih, void *user_data, const char *path)
{
        edit_msg (NULL, user_data);
}

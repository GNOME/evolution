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
#include "mail-accounts.h"
#include "mail-config-druid.h"
#include "mail-mt.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-local.h"
#include "mail-send-recv.h"
#include "mail-vfolder.h"
#include "folder-browser.h"
#include "subscribe-dialog.h"
#include "filter/filter-editor.h"
#include <gal/e-table/e-table.h>
#include <gal/widgets/e-gui-utils.h>
#include "e-messagebox.h"

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

static void
druid_destroyed (void)
{
	gtk_main_quit ();
}

static void
configure_mail (FolderBrowser *fb)
{
	MailConfigDruid *druid;
	
	if (fb) {
		GtkWidget *dialog;
		
		dialog = gnome_message_box_new (
			_("You have not configured the mail client.\n"
			  "You need to do this before you can send,\n"
			  "receive or compose mail.\n"
			  "Would you like to configure it now?"),
			GNOME_MESSAGE_BOX_QUESTION,
			GNOME_STOCK_BUTTON_YES,
			GNOME_STOCK_BUTTON_NO, NULL);

		/*
		 * Focus YES
		 */
		gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
		gtk_widget_grab_focus (GTK_WIDGET (GNOME_DIALOG (dialog)->buttons->data));
		
		gnome_dialog_set_parent (GNOME_DIALOG (dialog),
					 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb), GTK_TYPE_WINDOW)));
		
		switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog))) {
		case 0:
			druid = mail_config_druid_new (fb->shell);
			gtk_signal_connect (GTK_OBJECT (druid), "destroy",
					    GTK_SIGNAL_FUNC (druid_destroyed), NULL);
			gtk_widget_show (GTK_WIDGET (druid));
			gtk_grab_add (GTK_WIDGET (druid));
			gtk_main ();
			break;
		case 1:
		default:
			break;
		}
	}
}

static gboolean
check_send_configuration (FolderBrowser *fb)
{
	const MailConfigAccount *account;
	
	/* Check general */
	if (!mail_config_is_configured ()) {
		configure_mail (fb);
		return FALSE;
	}
	
	/* Get the default account */
	account = mail_config_get_default_account ();
	
	/* Check for an identity */
	if (!account) {
		GtkWidget *message;
		
		message = gnome_warning_dialog_parented (_("You need to configure an identity\n"
							   "before you can compose mail."),
							 GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb),
											      GTK_TYPE_WINDOW)));
		gnome_dialog_run_and_close (GNOME_DIALOG (message));
		return FALSE;
	}
	
	/* Check for a transport */
	if (!account->transport || !account->transport->url) {
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

#if 0
/* FIXME: is this still required when we send & receive email ?  I am not so sure ... */
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
#endif

void
send_receive_mail (GtkWidget *widget, gpointer user_data)
{
	const MailConfigAccount *account;

	/* receive first then send, this is a temp fix for POP-before-SMTP */
	if (!mail_config_is_configured ()) {
		configure_mail (FOLDER_BROWSER (user_data));
		return;
	}
	
	account = mail_config_get_default_account ();
	if (!account || !account->transport) {
		GtkWidget *win = gtk_widget_get_ancestor(GTK_WIDGET (user_data), GTK_TYPE_WINDOW);
		gnome_error_dialog_parented (_("You have not set a mail transport method"), GTK_WINDOW(win));
		return;
	}

	mail_send_receive();
}

static void
empty_subject_destroyed (GtkWidget *widget, gpointer data)
{
	gboolean *show_again = data;
	GtkWidget *checkbox;
	
	checkbox = e_message_box_get_checkbox (E_MESSAGE_BOX (widget));
	*show_again = !GTK_TOGGLE_BUTTON (checkbox)->active;
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	/* FIXME: EMessageBox should really handle this stuff
           automagically. What Miguel thinks would be nice is to pass
           in a message-id which could be used as a key in the config
           file and the value would be an int. -1 for always show or
           the button pressed otherwise. This probably means we'd have
           to write e_messagebox_run () */
	gboolean show_again = TRUE;
	GtkWidget *mbox;
	int button;
	
	if (!mail_config_get_prompt_empty_subject ())
		return TRUE;
	
	mbox = e_message_box_new (_("This message has no subject.\nReally send?"),
				  E_MESSAGE_BOX_QUESTION,
				  GNOME_STOCK_BUTTON_YES,
				  GNOME_STOCK_BUTTON_NO,
				  NULL);
	
	gtk_signal_connect (GTK_OBJECT (mbox), "destroy",
			    empty_subject_destroyed, &show_again);
	
	button = gnome_dialog_run_and_close (GNOME_DIALOG (mbox));
	
	mail_config_set_prompt_empty_subject (show_again);
	
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

struct _send_data {
	EMsgComposer *composer;
	struct post_send_data *psd;
};

static void
composer_sent_cb(char *uri, CamelMimeMessage *message, gboolean sent, void *data)
{
	struct _send_data *send = data;
	
	if (sent) {
		if (send->psd) {
			camel_folder_set_message_flags (send->psd->folder, send->psd->uid,
							send->psd->flags, send->psd->flags);
		}
		gtk_widget_destroy (GTK_WIDGET (send->composer));
	} else {
		gtk_widget_show (GTK_WIDGET (send->composer));
		gtk_object_unref (GTK_OBJECT (send->composer));
	}
	g_free (send);
}

void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	const MailConfigAccount *account = NULL;
	CamelMimeMessage *message;
	const CamelInternetAddress *iaddr;
	const char *subject;
	struct post_send_data *psd = data;
	struct _send_data *send;
	
	if (!mail_config_is_configured ()) {
		GtkWidget *dialog;
		
		dialog = gnome_ok_dialog_parented (_("You must configure an account before you "
						     "can send this email."),
						   GTK_WINDOW (composer));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		return;
	}
	
	/* Use the preferred account */
	account = e_msg_composer_get_preferred_account (composer);
	if (!account)
		account = mail_config_get_default_account ();
	
	/* Get the message */
	message = e_msg_composer_get_message (composer);
	
	/* Check for no recipients */
	iaddr = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	if (!iaddr || CAMEL_ADDRESS (iaddr)->addresses->len == 0) {
		GtkWidget *message_box;
		
		message_box = gnome_message_box_new (_("You must specify recipients in order to "
						       "send this message."),
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
	
	send = g_malloc (sizeof (*send));
	send->psd = psd;
	send->composer = composer;
	gtk_object_ref (GTK_OBJECT (composer));
	gtk_widget_hide (GTK_WIDGET (composer));
	mail_send_mail (account->transport->url, message, composer_sent_cb, send);
}

void
composer_postpone_cb (EMsgComposer *composer, gpointer data)
{
	const MailConfigAccount *account = NULL;
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
	
	/* Attach a X-Evolution-Transport header so we know which account
	   to use when it gets sent later. */
	account = e_msg_composer_get_preferred_account (composer);
	if (!account)
		account = mail_config_get_default_account ();
	camel_medium_add_header (CAMEL_MEDIUM (message), "X-Evolution-Transport", account->transport->url);
	
	/* Save the message in Outbox */
	mail_append_mail (outbox_folder, message, NULL, NULL, NULL);
	
	if (psd)
		camel_folder_set_message_flags (psd->folder, psd->uid, psd->flags, psd->flags);
	
	gtk_widget_destroy (GTK_WIDGET (composer));
}

static GtkWidget *
create_msg_composer (const char *url)
{
	const MailConfigAccount *account;
	gboolean send_html;
	gchar *sig_file = NULL;
	EMsgComposer *composer;
	
	account = mail_config_get_default_account ();
	send_html = mail_config_get_send_html ();
	
	if (account->id)
		sig_file = account->id->signature;
	
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

static GList *
list_add_addresses (GList *list, const CamelInternetAddress *cia, const GSList *accounts, const MailConfigAccount **me)
{
	const char *name, *addr;
	const GSList *l;
	gboolean notme;
	char *full;
	int i;
	
	for (i = 0; camel_internet_address_get (cia, i, &name, &addr); i++) {
		/* now, we format this, as if for display, but does the composer
		   then use it as a real address?  If so, very broken. */
		/* we should probably pass around CamelAddresse's if thats what
		   we mean */
		full = camel_internet_address_format_address (name, addr);
		
		/* Here I'll check to see if the cc:'d address is the address
		   of the sender, and if so, don't add it to the cc: list; this
		   is to fix Bugzilla bug #455. */
		notme = TRUE;
		l = accounts;
		while (l) {
			const MailConfigAccount *acnt = l->data;
			
			if (!strcmp (acnt->id->address, addr)) {
				notme = FALSE;
				if (me && !*me)
					*me = acnt;
				break;
			}
			
			l = l->next;
		}
		
		if (notme)
			list = g_list_append (list, full);
		else
			g_free (full);
	}
	
	return list;
}

static const MailConfigAccount *
guess_me (const CamelInternetAddress *to, const CamelInternetAddress *cc, const GSList *accounts)
{
	const char *name, *addr;
	const GSList *l;
	gboolean notme;
	char *full;
	int i;
	
	if (to) {
		for (i = 0; camel_internet_address_get (to, i, &name, &addr); i++) {
			full = camel_internet_address_format_address (name, addr);
			l = accounts;
			while (l) {
				const MailConfigAccount *acnt = l->data;
				
				if (!strcmp (acnt->id->address, addr)) {
					notme = FALSE;
					return acnt;
				}
				
				l = l->next;
			}
		}
	}
	
	if (cc) {
		for (i = 0; camel_internet_address_get (cc, i, &name, &addr); i++) {
			full = camel_internet_address_format_address (name, addr);
			l = accounts;
			while (l) {
				const MailConfigAccount *acnt = l->data;
				
				if (!strcmp (acnt->id->address, addr)) {
					notme = FALSE;
					return acnt;
				}
				
				l = l->next;
			}
		}
	}
	
	return NULL;
}

static void
free_recipients (GList *list)
{
	GList *l;
	
	for (l = list; l; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

static EMsgComposer *
mail_generate_reply (CamelMimeMessage *message, gboolean to_all)
{
	const CamelInternetAddress *reply_to, *sender, *to_addrs, *cc_addrs;
	const char *name = NULL, *address = NULL;
	const char *message_id, *references;
	char *text, *subject, *date_str;
	const MailConfigAccount *me = NULL;
	const MailConfigIdentity *id;
	const GSList *accounts = NULL;
	GList *to = NULL, *cc = NULL;
	EMsgComposer *composer;
	gchar *sig_file = NULL;
	time_t date;
	int offset;
	
	id = mail_config_get_default_identity ();
	if (id)
	      sig_file = id->signature;
	
	composer = e_msg_composer_new_with_sig_file (sig_file, mail_config_get_send_html ());
	if (!composer)
		return NULL;
	
	/* FIXME: should probably use a shorter date string */
	sender = camel_mime_message_get_from (message);
	camel_internet_address_get (sender, 0, &name, &address);
	date = camel_mime_message_get_date (message, &offset);
	date_str = header_format_date (date, offset);
	text = mail_tool_quote_message (message, _("On %s, %s wrote:\n"), date_str, name && *name ? name : address);
	g_free (date_str);
	
	if (text) {
		e_msg_composer_set_body_text (composer, text);
		g_free (text);
		e_msg_composer_mark_text_orig (composer);
	}
	
	/* Set the recipients */
	accounts = mail_config_get_accounts ();
	
	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);
	if (reply_to)
		to = g_list_append (to, camel_address_format (CAMEL_ADDRESS (reply_to)));
	
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	
	if (to_all) {
		cc = list_add_addresses (cc, to_addrs, accounts, &me);
		cc = list_add_addresses (cc, cc_addrs, accounts, me ? NULL : &me);
	} else {
		me = guess_me (to_addrs, cc_addrs, accounts);
	}
	
	/* Set the subject of the new message. */
	subject = (char *)camel_mime_message_get_subject (message);
	if (!subject)
		subject = g_strdup ("");
	else {
		if (!g_strncasecmp (subject, "Re: ", 4))
			subject = g_strdup (subject);
		else
			subject = g_strdup_printf ("Re: %s", subject);
	}
	
	e_msg_composer_set_headers (composer, me ? me->name : NULL, to, cc, NULL, subject);
	free_recipients (to);
	free_recipients (cc);
	g_free (subject);
	
	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message),
					      "Message-Id");
	references = camel_medium_get_header (CAMEL_MEDIUM (message),
					      "References");
	if (message_id) {
		e_msg_composer_add_header (composer, "In-Reply-To", message_id);
		if (references) {
			char *reply_refs;
			reply_refs = g_strdup_printf ("%s %s", references, message_id);
			e_msg_composer_add_header (composer, "References", reply_refs);
			g_free (reply_refs);
		}
	} else if (references) {
		e_msg_composer_add_header (composer, "References", references);
	}
	
	return composer;
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
	e_msg_composer_unset_changed (composer);
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


static EMsgComposer *
forward_get_composer (const char *subject)
{
	const MailConfigAccount *account;
	EMsgComposer *composer;
	
	account = mail_config_get_default_account ();
	composer = e_msg_composer_new_with_sig_file (account && account->id ? account->id->signature : NULL,
						     mail_config_get_send_html ());
	if (composer) {
		gtk_signal_connect (GTK_OBJECT (composer), "send",
				    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "postpone",
				    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
		e_msg_composer_set_headers (composer, account->name, NULL, NULL, NULL, subject);
	} else {
		g_warning("Could not create composer");
	}

	return composer;
}

static void
do_forward_inline(CamelFolder *folder, char *uid, CamelMimeMessage *message, void *data)
{
	char *subject;
	char *text;

	if (message) {
		subject = mail_tool_generate_forward_subject(message);
		text = mail_tool_quote_message (message, _("Forwarded message:\n"));

		if (text) {
			EMsgComposer *composer = forward_get_composer(subject);
			if (composer) {
				e_msg_composer_set_body_text(composer, text);
				gtk_widget_show(GTK_WIDGET(composer));
				e_msg_composer_unset_changed(composer);
			}
			g_free(text);
		}

		g_free(subject);
	}
}

static void
do_forward_attach(CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *data)
{
	if (part) {
		EMsgComposer *composer = forward_get_composer(subject);
		if (composer) {
			e_msg_composer_attach(composer, part);
			gtk_widget_show(GTK_WIDGET(composer));
			e_msg_composer_unset_changed(composer);
		}
	}
}

void
forward_messages (CamelFolder *folder, GPtrArray *uids, gboolean doinline)
{
	if (doinline && uids->len == 1) {
		mail_get_message(folder, uids->pdata[0], do_forward_inline, NULL, mail_thread_new);
	} else {
		mail_build_attachment(folder, uids, do_forward_attach, NULL);
	}
}

void
forward_inlined (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	
	if (!check_send_configuration (fb))
		return;
	
	uids = g_ptr_array_new ();
	g_ptr_array_add (uids, g_strdup (fb->message_list->cursor_uid));
	forward_messages (fb->message_list->folder, uids, TRUE);
	g_free (uids->pdata[0]);
	g_ptr_array_free (uids, TRUE);
}

void
forward_attached (GtkWidget *widget, gpointer user_data)
{
	GPtrArray *uids;
	FolderBrowser *fb = (FolderBrowser *)user_data;

	if (!check_send_configuration (fb))
		return;

	uids = g_ptr_array_new();
	message_list_foreach(fb->message_list, enumerate_msg, uids);
	forward_messages(fb->message_list->folder, uids, FALSE);
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

	mail_filter_on_demand(fb->folder, uids);
}

void
select_all (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
        MessageList *ml = fb->message_list;

	if (ml->folder == NULL)
		return;

	e_table_select_all (ml->table);
}

void
invert_selection (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
        MessageList *ml = fb->message_list;

	if (ml->folder == NULL)
		return;

	e_table_invert_selection (ml->table);
}

/* flag all selected messages */
static void
flag_messages(FolderBrowser *fb, guint32 mask, guint32 set)
{
        MessageList *ml = fb->message_list;
	GPtrArray *uids;
	int i;

	if (ml->folder == NULL)
		return;

	/* could just use specific callback but i'm lazy */
	uids = g_ptr_array_new ();
	message_list_foreach (ml, enumerate_msg, uids);
	camel_folder_freeze(ml->folder);
	for (i=0;i<uids->len;i++) {
		camel_folder_set_message_flags(ml->folder, uids->pdata[i], mask, set);
		g_free(uids->pdata[i]);
	}
	camel_folder_thaw(ml->folder);

	g_ptr_array_free(uids, TRUE);
}

void
mark_as_seen (BonoboUIComponent *uih, void *user_data, const char *path)
{
	flag_messages(FOLDER_BROWSER(user_data), CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

void
mark_as_unseen (BonoboUIComponent *uih, void *user_data, const char *path)
{
	flag_messages(FOLDER_BROWSER(user_data), CAMEL_MESSAGE_SEEN, 0);
}

static void
do_edit_messages(CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *data)
{
	/*FolderBrowser *fb = data;*/
	int i;

	for (i=0; i<messages->len; i++) {
		EMsgComposer *composer;

		composer = e_msg_composer_new_with_message(messages->pdata[i]);
		if (composer) {
			gtk_signal_connect (GTK_OBJECT (composer), "send",
					    composer_send_cb, NULL);
			gtk_signal_connect (GTK_OBJECT (composer), "postpone",
					    composer_postpone_cb, NULL);
			gtk_widget_show (GTK_WIDGET (composer));
		}
	}
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
	
	mail_get_messages(fb->folder, uids, do_edit_messages, fb);
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
		mail_save_messages (folder, uids, path, NULL, NULL);
		gtk_widget_destroy (GTK_WIDGET (user_data));
	}
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
	flag_messages(FOLDER_BROWSER(user_data), CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
}

void
undelete_msg (GtkWidget *button, gpointer user_data)
{
	flag_messages(FOLDER_BROWSER(user_data), CAMEL_MESSAGE_DELETED, 0);
}

void
next_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	int row;
	
	row = e_table_get_cursor_row (fb->message_list->table);
	message_list_select (fb->message_list, row,
			     MESSAGE_LIST_SELECT_NEXT,
			     0, CAMEL_MESSAGE_SEEN);
}

void
previous_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	int row;
	
	row = e_table_get_cursor_row (fb->message_list->table);
	message_list_select (fb->message_list, row,
			     MESSAGE_LIST_SELECT_PREVIOUS,
			     0, CAMEL_MESSAGE_SEEN);
}

static void expunged_folder(CamelFolder *f, void *data)
{
	FolderBrowser *fb = data;

	fb->expunging = NULL;
}

void
expunge_folder (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER(user_data);

	if (fb->folder
	    && (fb->expunging == NULL
		|| fb->folder != fb->expunging)) {
		fb->expunging = fb->folder;
		mail_expunge_folder(fb->folder, expunged_folder, fb);
	}
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
	/* FIXME: should we block until mail-config is done? */
	MailAccountsDialog *dialog;
	
	dialog = mail_accounts_dialog_new ((FOLDER_BROWSER (user_data))->shell);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
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
	
	mail_local_reconfigure_folder(fb);
}

static void
do_view_message(CamelFolder *folder, char *uid, CamelMimeMessage *message, void *data)
{
	/*FolderBrowser *fb = data;*/
	GtkWidget *view;

	if (message) {
		view = mail_view_create(folder, uid, message);
		gtk_widget_show(view);
	}
}

void
view_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = user_data;
	GPtrArray *uids;
	int i;

	if (!fb->folder)
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	for (i=0;i<uids->len;i++) {
		mail_get_message(fb->folder, uids->pdata[i], do_view_message, fb, mail_thread_queued);
		g_free(uids->pdata[i]);
	}
	g_ptr_array_free(uids, TRUE);
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


void
stop_threads(BonoboUIComponent *uih, void *user_data, const char *path)
{
	camel_operation_cancel(NULL);
}


static void
create_folders (EvolutionStorage *storage, const char *prefix, CamelFolderInfo *fi)
{
	char *name, *path;
	
	if (fi->unread_message_count > 0)
		name = g_strdup_printf ("%s (%d)", fi->name,
					fi->unread_message_count);
	else
		name = g_strdup (fi->name);
	
	path = g_strdup_printf ("%s/%s", prefix, fi->name);
	evolution_storage_new_folder (storage, path, name,
				      "mail", fi->url ? fi->url : "",
				      fi->name, /* description */
				      fi->unread_message_count > 0);
	g_free (name);
	
	if (fi->child)
		create_folders (storage, path, fi->child);
	g_free (path);
	
	if (fi->sibling)
		create_folders (storage, prefix, fi->sibling);
}

void
folder_created (CamelStore *store, CamelFolderInfo *fi)
{
	EvolutionStorage *storage;
	
	if ((storage = mail_lookup_storage (store))) {
		if (fi)
			/* FIXME: this won't work. (the "prefix" is wrong) */
			create_folders (storage, "", fi);
		
		gtk_object_unref (GTK_OBJECT (storage));
	}
}

void
mail_storage_create_folder (EvolutionStorage *storage, CamelStore *store, CamelFolderInfo *fi)
{
	gboolean unref = FALSE;
	
	if (!storage && store) {
		storage = mail_lookup_storage (store);
		unref = TRUE;
	}
	
	if (storage) {
		if (fi)
			create_folders (storage, "", fi);
		
		if (unref)
			gtk_object_unref (GTK_OBJECT (storage));
	}
}

static void
delete_folders (EvolutionStorage *storage, CamelFolderInfo *fi)
{
	char *path;
	
	if (fi->child)
		delete_folders (storage, fi->child);
	
	path = g_strdup_printf ("/%s", fi->full_name);
	evolution_storage_removed_folder (storage, path);
	g_free (path);
	
	if (fi->sibling)
		delete_folders (storage, fi->sibling);
}

void
folder_deleted (CamelStore *store, CamelFolderInfo *fi)
{
	EvolutionStorage *storage;
	
	if ((storage = mail_lookup_storage (store))) {
		if (fi)
			delete_folders (storage, fi);
		
		gtk_object_unref (GTK_OBJECT (storage));
	}
}

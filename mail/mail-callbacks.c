/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Authors: 
 *  Dan Winship <danw@ximian.com>
 *  Peter Williams <peterw@ximian.com>
 *  Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <time.h>
#include <libgnome/gnome-paper.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnome/gnome-paper.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-socket.h>
#include <gal/e-table/e-table.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-dialog-utils.h>
#include <filter/filter-editor.h>
#include "mail.h"
#include "message-browser.h"
#include "mail-callbacks.h"
#include "mail-config.h"
#include "mail-accounts.h"
#include "mail-config-druid.h"
#include "mail-mt.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-local.h"
#include "mail-search.h"
#include "mail-send-recv.h"
#include "mail-vfolder.h"
#include "mail-folder-cache.h"
#include "folder-browser.h"
#include "subscribe-dialog.h"
#include "message-tag-editor.h"
#include "message-tag-followup.h"
#include "e-messagebox.h"

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-client.h"

#ifndef HAVE_MKSTEMP
#include <fcntl.h>
#include <sys/stat.h>
#endif

#define FB_WINDOW(fb) GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb), GTK_TYPE_WINDOW))

struct post_send_data {
	CamelFolder *folder;
	gchar *uid;
	guint32 flags, set;
};

static void
druid_destroyed (void)
{
	gtk_main_quit ();
}

static gboolean
configure_mail (FolderBrowser *fb)
{
	MailConfigDruid *druid;
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
	
	e_gnome_dialog_set_parent (GNOME_DIALOG (dialog), FB_WINDOW (fb));
	
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
	
	return mail_config_is_configured ();
}

static gboolean
check_send_configuration (FolderBrowser *fb)
{
	const MailConfigAccount *account;
	
	/* Check general */
	if (!mail_config_is_configured () && !configure_mail (fb))
			return FALSE;
	
	/* Get the default account */
	account = mail_config_get_default_account ();
	
	/* Check for an identity */
	if (!account) {
		GtkWidget *message;
		
		message = e_gnome_warning_dialog_parented (_("You need to configure an identity\n"
							     "before you can compose mail."),
							   FB_WINDOW (fb));
		
		gnome_dialog_set_close (GNOME_DIALOG (message), TRUE);
		gtk_widget_show (message);
		
		return FALSE;
	}
	
	/* Check for a transport */
	if (!account->transport || !account->transport->url) {
		GtkWidget *message;
		
		message = e_gnome_warning_dialog_parented (_("You need to configure a mail transport\n"
							     "before you can compose mail."),
							   GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (fb),
												GTK_TYPE_WINDOW)));
		
		gnome_dialog_set_close (GNOME_DIALOG (message), TRUE);
		gtk_widget_show (message);
		
		return FALSE;
	}
	
	return TRUE;
}

void
send_receive_mail (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	const MailConfigAccount *account;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!mail_config_is_configured () && !configure_mail (fb))
			return;
	
	account = mail_config_get_default_account ();
	if (!account || !account->transport) {
		GtkWidget *dialog;
		
		dialog = gnome_error_dialog_parented (_("You have not set a mail transport method"),
						      FB_WINDOW (fb));
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		
		return;
	}
	
	mail_send_receive (fb->folder);
}

static void
msgbox_destroyed (GtkWidget *widget, gpointer data)
{
	gboolean *show_again = data;
	GtkWidget *checkbox;
	
	checkbox = e_message_box_get_checkbox (E_MESSAGE_BOX (widget));
	*show_again = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
}

static gboolean
ask_confirm_for_unwanted_html_mail (EMsgComposer *composer, EDestination **recipients)
{
	gboolean show_again = TRUE;
	GString *str;
	GtkWidget *mbox;
	gint i, button;
	
	if (!mail_config_get_confirm_unwanted_html ()) {
		g_message ("doesn't want to see confirm html messages!");
		return TRUE;
	}
	
	/* FIXME: this wording sucks */
	str = g_string_new (_("You are sending an HTML-formatted message, but the following recipients "
			      "do not want HTML-formatted mail:\n"));
	for (i = 0; recipients[i] != NULL; ++i) {
		if (!e_destination_get_html_mail_pref (recipients[i])) {
			const char *name;
			char *buf;
			
			name = e_destination_get_textrep (recipients[i]);
			buf = e_utf8_to_locale_string (name);
			
			g_string_sprintfa (str, "     %s\n", buf);
			g_free (buf);
		}
	}
	
	g_string_append (str, _("Send anyway?"));
	
	mbox = e_message_box_new (str->str,
				  E_MESSAGE_BOX_QUESTION,
				  GNOME_STOCK_BUTTON_YES,
				  GNOME_STOCK_BUTTON_NO,
				  NULL);
	
	g_string_free (str, TRUE);
	
	gtk_signal_connect (GTK_OBJECT (mbox), "destroy",
			    msgbox_destroyed, &show_again);
	
	button = gnome_dialog_run_and_close (GNOME_DIALOG (mbox));
	
	if (!show_again) {
		mail_config_set_confirm_unwanted_html (show_again);
		g_message ("don't show HTML warning again");
	}
	
	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	/* FIXME: EMessageBox should really handle this stuff
           automagically. What Miguel thinks would be nice is to pass
           in a unique id which could be used as a key in the config
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
			    msgbox_destroyed, &show_again);
	
	button = gnome_dialog_run_and_close (GNOME_DIALOG (mbox));
	
	mail_config_set_prompt_empty_subject (show_again);
	
	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

static gboolean
ask_confirm_for_only_bcc (EMsgComposer *composer, gboolean hidden_list_case)
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
	const gchar *first_text;
	gchar *message_text;
	
	if (!mail_config_get_prompt_only_bcc ())
		return TRUE;

	/* If the user is mailing a hidden contact list, it is possible for
	   them to create a message with only Bcc recipients without really
	   realizing it.  To try to avoid being totally confusing, I've changed
	   this dialog to provide slightly different text in that case, to
	   better explain what the hell is going on. */

	if (hidden_list_case) {
		first_text =  _("Since the contact list you are sending to "
				"is configured to hide the list's addresses, "
				"this message will contain only Bcc recipients.");
	} else {
		first_text = _("This message contains only Bcc recipients.");
	}
	
	message_text = g_strdup_printf ("%s\n%s", first_text,
					_("It is possible that the mail server may reveal the recipients "
					  "by adding an Apparently-To header.\nSend anyway?"));
	
	mbox = e_message_box_new (message_text, 
				  E_MESSAGE_BOX_QUESTION,
				  GNOME_STOCK_BUTTON_YES,
				  GNOME_STOCK_BUTTON_NO,
				  NULL);
	
	gtk_signal_connect (GTK_OBJECT (mbox), "destroy",
			    msgbox_destroyed, &show_again);
	
	button = gnome_dialog_run_and_close (GNOME_DIALOG (mbox));
	
	mail_config_set_prompt_only_bcc (show_again);

	g_free (message_text);
	
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
composer_sent_cb (char *uri, CamelMimeMessage *message, gboolean sent, void *data)
{
	struct _send_data *send = data;
	
	if (sent) {
		if (send->psd) {
			camel_folder_set_message_flags (send->psd->folder, send->psd->uid,
							send->psd->flags, send->psd->set);
		}
		gtk_widget_destroy (GTK_WIDGET (send->composer));
	} else {
		e_msg_composer_set_enable_autosave(send->composer, TRUE);
		gtk_widget_show (GTK_WIDGET (send->composer));
		gtk_object_unref (GTK_OBJECT (send->composer));
	}
	
	g_free (send);
	camel_object_unref (CAMEL_OBJECT (message));
}

static CamelMimeMessage *
composer_get_message (EMsgComposer *composer)
{
	static char *recipient_type[] = {
		CAMEL_RECIPIENT_TYPE_TO,
		CAMEL_RECIPIENT_TYPE_CC,
		CAMEL_RECIPIENT_TYPE_BCC
	};
	const CamelInternetAddress *iaddr;
	const MailConfigAccount *account;
	CamelMimeMessage *message;
	EDestination **recipients;
	const char *subject;
	int num_addrs, i;
	
	message = e_msg_composer_get_message (composer);
	if (message == NULL)
		return NULL;
	
	/* Add info about the sending account */
	account = e_msg_composer_get_preferred_account (composer);
	
	if (account) {
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Account", account->name);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Transport", account->transport->url);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc", account->sent_folder_uri);
		if (account->id->organization)
			camel_medium_set_header (CAMEL_MEDIUM (message), "Organization", account->id->organization);
	}
	
	/* get the message recipients */
	recipients = e_msg_composer_get_recipients (composer);
	
	/* Check for recipients */
	for (num_addrs = 0, i = 0; i < 3; i++) {
		iaddr = camel_mime_message_get_recipients (message, recipient_type[i]);
		num_addrs += iaddr ? camel_address_length (CAMEL_ADDRESS (iaddr)) : 0;
	}
	
	/* I'm sensing a lack of love, er, I mean recipients. */
	if (num_addrs == 0) {
		GtkWidget *message_box;
		
		message_box = gnome_message_box_new (_("You must specify recipients in order to "
						       "send this message."),
						     GNOME_MESSAGE_BOX_WARNING,
						     GNOME_STOCK_BUTTON_OK,
						     NULL);
		
		gnome_dialog_run_and_close (GNOME_DIALOG (message_box));
		
		camel_object_unref (CAMEL_OBJECT (message));
		message = NULL;
		goto finished;
	}
	
	if (iaddr && num_addrs == camel_address_length (CAMEL_ADDRESS (iaddr))) {
		/* this means that the only recipients are Bcc's */
		
		/* OK, this is an abusive hack.  If someone sends a mail with a
		   hidden contact list on to to: line and no other recipients,
		   they will unknowingly create a message with only bcc: recipients.
		   We try to detect this and pass a flag to ask_confirm_for_only_bcc,
		   so that it can present the user with a dialog whose text has been
		   modified to reflect this situation. */
		
		const char *to_header = camel_medium_get_header (CAMEL_MEDIUM (message), CAMEL_RECIPIENT_TYPE_TO);
		gboolean hidden_list_case = FALSE;
		
		if (to_header && !strcmp (to_header, "Undisclosed-Recipient:;"))
			hidden_list_case = TRUE;
		
		if (!ask_confirm_for_only_bcc (composer, hidden_list_case)) {
			camel_object_unref (CAMEL_OBJECT (message));
			message = NULL;
			goto finished;
		}
	}
	
	/* Only show this warning if our default is to send html.  If it isn't, we've
	   manually switched into html mode in the composer and (presumably) had a good
	   reason for doing this. */
	if (e_msg_composer_get_send_html (composer)
	    && mail_config_get_send_html ()
	    && mail_config_get_confirm_unwanted_html ()) {
		gboolean html_problem = FALSE;
		for (i = 0; recipients[i] != NULL && !html_problem; ++i) {
			if (!e_destination_get_html_mail_pref (recipients[i]))
				html_problem = TRUE;
		}
		
		if (html_problem) {
			html_problem = !ask_confirm_for_unwanted_html_mail (composer, recipients);
			if (html_problem) {
				camel_object_unref (CAMEL_OBJECT (message));
				message = NULL;
				goto finished;
			}
		}
	}
	
	/* Check for no subject */
	subject = camel_mime_message_get_subject (message);
	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer)) {
			camel_object_unref (CAMEL_OBJECT (message));
			message = NULL;
			goto finished;
		}
	}
	
	/* Get the message recipients and 'touch' them, boosting their use scores */
	e_destination_touchv (recipients);
	
 finished:
	
	e_destination_freev (recipients);
	
	return message;
}

void
composer_send_cb (EMsgComposer *composer, gpointer data)
{
	const MailConfigService *transport;
	CamelMimeMessage *message;
	struct post_send_data *psd = data;
	struct _send_data *send;
	
	if (!mail_config_is_configured ()) {
		GtkWidget *dialog;
		
		dialog = gnome_ok_dialog_parented (_("You must configure an account before you "
						     "can send this email."),
						   GTK_WINDOW (composer));
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		return;
	}
	
	message = composer_get_message (composer);
	if (!message)
		return;
	
	transport = mail_config_get_default_transport ();
	if (!transport)
		return;
	
	send = g_malloc (sizeof (*send));
	send->psd = psd;
	send->composer = composer;
	gtk_object_ref (GTK_OBJECT (composer));
	gtk_widget_hide (GTK_WIDGET (composer));
	e_msg_composer_set_enable_autosave (composer, FALSE);
	mail_send_mail (transport->url, message, composer_sent_cb, send);
}

static void
append_mail_cleanup (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok, void *data)
{
	camel_message_info_free (info);
}

void
composer_postpone_cb (EMsgComposer *composer, gpointer data)
{
	extern CamelFolder *outbox_folder;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	struct post_send_data *psd = data;
	
	message = composer_get_message (composer);
	if (message == NULL)
		return;
	
	info = camel_message_info_new ();
	info->flags = CAMEL_MESSAGE_SEEN;
	
	mail_append_mail (outbox_folder, message, info, append_mail_cleanup, NULL);
	camel_object_unref (CAMEL_OBJECT (message));
	
	if (psd)
		camel_folder_set_message_flags (psd->folder, psd->uid, psd->flags, psd->set);
	
	gtk_widget_destroy (GTK_WIDGET (composer));
}

struct _save_draft_info {
	EMsgComposer *composer;
	int quit;
};

static void
save_draft_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok, void *data)
{
	struct _save_draft_info *sdi = data;
	
	if (ok && sdi->quit)
		gtk_widget_destroy (GTK_WIDGET (sdi->composer));
	else
		gtk_object_unref (GTK_OBJECT (sdi->composer));
	
	g_free (info);
	g_free (sdi);
}

static void
use_default_drafts_cb (int reply, gpointer data)
{
	extern CamelFolder *drafts_folder;
	CamelFolder **folder = data;
	
	if (reply == 0) {
		*folder = drafts_folder;
		camel_object_ref (CAMEL_OBJECT (*folder));
	}
}

static void
save_draft_folder (char *uri, CamelFolder *folder, gpointer data)
{
	CamelFolder **save = data;
	
	if (folder) {
		*save = folder;
		camel_object_ref (CAMEL_OBJECT (folder));
	}
}

void
composer_save_draft_cb (EMsgComposer *composer, int quit, gpointer data)
{
	extern char *default_drafts_folder_uri;
	extern CamelFolder *drafts_folder;
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	const MailConfigAccount *account;
	struct _save_draft_info *sdi;
	CamelFolder *folder = NULL;
	
	account = e_msg_composer_get_preferred_account (composer);
	if (account && account->drafts_folder_uri &&
	    strcmp (account->drafts_folder_uri, default_drafts_folder_uri) != 0) {
		int id;
		
		id = mail_get_folder (account->drafts_folder_uri, 0, save_draft_folder, &folder, mail_thread_new);
		mail_msg_wait (id);
		
		if (!folder) {
			GtkWidget *dialog;
			
			dialog = gnome_ok_cancel_dialog_parented (_("Unable to open the drafts folder for this account.\n"
								    "Would you like to use the default drafts folder?"),
								  use_default_drafts_cb, &folder, GTK_WINDOW (composer));
			gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
			if (!folder)
				return;
		}
	} else {
		folder = drafts_folder;
		camel_object_ref (CAMEL_OBJECT (folder));
	}
	
	msg = e_msg_composer_get_message_draft (composer);
	
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN;
	
	sdi = g_malloc (sizeof (struct _save_draft_info));
	sdi->composer = composer;
	gtk_object_ref (GTK_OBJECT (composer));
	sdi->quit = quit;
	
	mail_append_mail (folder, msg, info, save_draft_done, sdi);
	camel_object_unref (CAMEL_OBJECT (folder));
	camel_object_unref (CAMEL_OBJECT (msg));
}

static GtkWidget *
create_msg_composer (const MailConfigAccount *account, const char *url)
{
	EMsgComposer *composer;
	gboolean send_html;
	
	/* Make sure that we've actually been passed in an account. If one has
	 * not been passed in, grab the default account.
	 */
	if (account == NULL) {
		account = mail_config_get_default_account ();
	}
	
	send_html = mail_config_get_send_html ();
	
	composer = url ? e_msg_composer_new_from_url (url) : e_msg_composer_new ();
	
	if (composer) {
		e_msg_composer_hdrs_set_from_account (E_MSG_COMPOSER_HDRS (composer->hdrs), account->name);
		e_msg_composer_set_send_html (composer, send_html);
		e_msg_composer_unset_changed (composer);
		e_msg_composer_drop_editor_undo (composer);
		return GTK_WIDGET (composer);
	} else
		return NULL;
}

void
compose_msg (GtkWidget *widget, gpointer user_data)
{
	const MailConfigAccount *account;
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GtkWidget *composer;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb) || !check_send_configuration (fb))
		return;
	
	/* Figure out which account we want to initially compose from */
	account = mail_config_get_account_by_source_url (fb->uri);
	
	composer = create_msg_composer (account, NULL);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
			    GTK_SIGNAL_FUNC (composer_save_draft_cb), NULL);
	
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
	
	/* Tell create_msg_composer to use the default email profile */
	composer = create_msg_composer (NULL, url);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
			    GTK_SIGNAL_FUNC (composer_save_draft_cb), NULL);
	
	gtk_widget_show (composer);
}	

static GList *
list_add_addresses (GList *list, const CamelInternetAddress *cia, const GSList *accounts,
		    GHashTable *rcpt_hash, const MailConfigAccount **me)
{
	const MailConfigAccount *account;
	GHashTable *account_hash;
	const char *name, *addr;
	const GSList *l;
	int i;
	
	account_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	l = accounts;
	while (l) {
		account = l->data;
		g_hash_table_insert (account_hash, (char *) account->id->address, (void *) account);
		l = l->next;
	}
	
	for (i = 0; camel_internet_address_get (cia, i, &name, &addr); i++) {
		/* Here I'll check to see if the cc:'d address is the address
		   of the sender, and if so, don't add it to the cc: list; this
		   is to fix Bugzilla bug #455. */
		account = g_hash_table_lookup (account_hash, addr);
		if (account && me && !*me)
			*me = account;
		
		if (!account && !g_hash_table_lookup (rcpt_hash, addr)) {
			EDestination *dest;
			
			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);
			
			list = g_list_append (list, dest);
			g_hash_table_insert (rcpt_hash, (char *) addr, GINT_TO_POINTER (1));
		} 
	}
	
	g_hash_table_destroy (account_hash);
	
	return list;
}

static const MailConfigAccount *
guess_me (const CamelInternetAddress *to, const CamelInternetAddress *cc, const GSList *accounts)
{
	const MailConfigAccount *account;
	GHashTable *account_hash;
	const char *addr;
	const GSList *l;
	int i;
	
	/* "optimization" */
	if (!to && !cc)
		return NULL;
	
	account_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	l = accounts;
	while (l) {
		account = l->data;
		g_hash_table_insert (account_hash, (char *) account->id->address, (void *) account);
		l = l->next;
	}
	
	if (to) {
		for (i = 0; camel_internet_address_get (to, i, NULL, &addr); i++) {
			account = g_hash_table_lookup (account_hash, addr);
			if (account)
				goto found;
		}
	}
	
	if (cc) {
		for (i = 0; camel_internet_address_get (cc, i, NULL, &addr); i++) {
			account = g_hash_table_lookup (account_hash, addr);
			if (account)
				goto found;
		}
	}
	
	account = NULL;
	
 found:
	
	g_hash_table_destroy (account_hash);
	
	return account;
}

inline static void
mail_ignore (EMsgComposer *composer, const gchar *name, const gchar *address)
{
	e_msg_composer_ignore (composer, name && *name ? name : address);
}

static void
mail_ignore_address (EMsgComposer *composer, const CamelInternetAddress *addr)
{
	const gchar *name, *address;
	gint i, max;

	max = camel_address_length (CAMEL_ADDRESS (addr));
	for (i = 0; i < max; i++) {
		camel_internet_address_get (addr, i, &name, &address);
		mail_ignore (composer, name, address);
	}
}

static EMsgComposer *
mail_generate_reply (CamelFolder *folder, CamelMimeMessage *message, const char *uid, int mode)
{
	const CamelInternetAddress *reply_to, *sender, *to_addrs, *cc_addrs;
	const char *name = NULL, *address = NULL, *source = NULL;
	const char *message_id, *references, *reply_addr = NULL;
	char *text = NULL, *subject, date_str[100], *format;
	const MailConfigAccount *me = NULL;
	const GSList *accounts = NULL;
	GList *to = NULL, *cc = NULL;
	EDestination **tov, **ccv;
	EMsgComposer *composer;
	time_t date;
	const int max_subject_length = 1024;
	
	composer = e_msg_composer_new ();	
	if (!composer)
		return NULL;
	
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	/* Set the recipients */
	accounts = mail_config_get_accounts ();
	
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	mail_ignore_address (composer, to_addrs);
	mail_ignore_address (composer, cc_addrs);
	
	/* default 'me' to the source account... */
	source = camel_mime_message_get_source (message);
	if (source)
		me = mail_config_get_account_by_source_url (source);
	
	if (mode == REPLY_LIST) {
		CamelMessageInfo *info;
		const char *mlist;
		int i, max, len;
		
		info = camel_folder_get_message_info (folder, uid);
		mlist = camel_message_info_mlist (info);
		
		if (mlist) {
			/* look through the recipients to find the *real* mailing list address */
			len = strlen (mlist);
			
			to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
			max = camel_address_length (CAMEL_ADDRESS (to_addrs));
			for (i = 0; i < max; i++) {
				camel_internet_address_get (to_addrs, i, &name, &address);
				if (!g_strncasecmp (address, mlist, len))
					break;
			}
			
			if (i == max) {
				cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
				max = camel_address_length (CAMEL_ADDRESS (cc_addrs));
				for (i = 0; i < max; i++) {
					camel_internet_address_get (cc_addrs, i, &name, &address);
					if (!g_strncasecmp (address, mlist, len))
						break;
				}
			}
			
			if (address && i != max) {
				EDestination *dest;
				
				dest = e_destination_new ();
				e_destination_set_name (dest, name);
				e_destination_set_email (dest, address);
				
				to = g_list_append (to, dest);
			}
		}
		
		if (!me)
			me = guess_me (to_addrs, cc_addrs, accounts);
	} else {
		GHashTable *rcpt_hash;
		
		rcpt_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
		
		reply_to = camel_mime_message_get_reply_to (message);
		if (!reply_to)
			reply_to = camel_mime_message_get_from (message);
		if (reply_to) {
			int i;
			
			for (i = 0; camel_internet_address_get (reply_to, i, &name, &reply_addr); i++) {
				/* Get the Reply-To address so we can ignore references to it in the Cc: list */
				EDestination *dest;
				
				dest = e_destination_new ();
				e_destination_set_name (dest, name);
				e_destination_set_email (dest, reply_addr);
				to = g_list_append (to, dest);
				g_hash_table_insert (rcpt_hash, (char *) reply_addr, GINT_TO_POINTER (1));
				mail_ignore (composer, name, reply_addr);
			}
		}
		
		if (mode == REPLY_ALL) {
			cc = list_add_addresses (cc, to_addrs, accounts, rcpt_hash, me ? NULL : &me);
			cc = list_add_addresses (cc, cc_addrs, accounts, rcpt_hash, me ? NULL : &me);
		} else {
			if (!me)
				me = guess_me (to_addrs, cc_addrs, accounts);
		}
		
		g_hash_table_destroy (rcpt_hash);
	}
	
	/* set body text here as we want all ignored words to take effect */
	if ((mode & REPLY_NO_QUOTE) == 0) {
		sender = camel_mime_message_get_from (message);
		if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
			camel_internet_address_get (sender, 0, &name, &address);
		} else {
			name = _("an unknown sender");
		}
		
		date = camel_mime_message_get_date (message, NULL);
		strftime (date_str, sizeof (date_str), _("On %a, %Y-%m-%d at %H:%M, %%s wrote:"),
			  localtime (&date));
		format = e_utf8_from_locale_string (date_str);
		text = mail_tool_quote_message (message, format, name && *name ? name : address);
		mail_ignore (composer, name, address);
		g_free (format);
		if (text) {
			e_msg_composer_set_body_text (composer, text);
			g_free (text);
		}
	}
	
	/* Set the subject of the new message. */
	subject = (char *)camel_mime_message_get_subject (message);
	if (!subject)
		subject = g_strdup ("");
	else {
		if (!g_strncasecmp (subject, "Re: ", 4))
			subject = g_strndup (subject, max_subject_length);
		else {
			if (strlen (subject) < max_subject_length) {
				subject = g_strdup_printf ("Re: %s", subject);
			} else {
				/* We can't use %.*s because it depends on the locale being C/POSIX
                                   or UTF-8 to work correctly in glibc */
				char *sub;
				
				/*subject = g_strdup_printf ("Re: %.*s...", max_subject_length, subject);*/
				sub = g_malloc (max_subject_length + 8);
				memcpy (sub, "Re: ", 4);
				memcpy (sub + 4, subject, max_subject_length);
				memcpy (sub + 4 + max_subject_length, "...", 4);
				subject = sub;
			}
		}
	}
	
	tov = e_destination_list_to_vector (to);
	ccv = e_destination_list_to_vector (cc);
	
	g_list_free (to);
	g_list_free (cc);
	
	e_msg_composer_set_headers (composer, me ? me->name : NULL, tov, ccv, NULL, subject);
	
	e_destination_freev (tov);
	e_destination_freev (ccv);
	
	g_free (subject);
	
	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message), "Message-Id");
	references = camel_medium_get_header (CAMEL_MEDIUM (message), "References");
	if (message_id) {
		char *reply_refs;
		
		e_msg_composer_add_header (composer, "In-Reply-To", message_id);
		
		if (references)
			reply_refs = g_strdup_printf ("%s %s", references, message_id);
		else
			reply_refs = g_strdup (message_id);
		
		e_msg_composer_add_header (composer, "References", reply_refs);
		g_free (reply_refs);
	} else if (references) {
		e_msg_composer_add_header (composer, "References", references);
	}
	
	e_msg_composer_drop_editor_undo (composer);
	
	return composer;
}

static void
requeue_mail_reply (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data)
{
	int mode = GPOINTER_TO_INT (data);
	
	mail_reply (folder, msg, uid, mode);
}

void
mail_reply (CamelFolder *folder, CamelMimeMessage *msg, const char *uid, int mode)
{
	EMsgComposer *composer;
	struct post_send_data *psd;
	
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);
	
	if (!msg) {
		mail_get_message (folder, uid, requeue_mail_reply,
				  GINT_TO_POINTER (mode), mail_thread_new);
		return;
	}
	
	psd = g_new (struct post_send_data, 1);
	psd->folder = folder;
	camel_object_ref (CAMEL_OBJECT (psd->folder));
	psd->uid = g_strdup (uid);
	psd->flags = CAMEL_MESSAGE_ANSWERED;
	psd->set = CAMEL_MESSAGE_ANSWERED;
	
	composer = mail_generate_reply (folder, msg, uid, mode);
	if (!composer)
		return;
	
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), psd);
	gtk_signal_connect (GTK_OBJECT (composer), "postpone",
			    GTK_SIGNAL_FUNC (composer_postpone_cb), psd);
	gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
			    GTK_SIGNAL_FUNC (composer_save_draft_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (composer), "destroy",
			    GTK_SIGNAL_FUNC (free_psd), psd);
	
	gtk_widget_show (GTK_WIDGET (composer));	
	e_msg_composer_unset_changed (composer);
}

void
reply_to_sender (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	/* FIXME: make this always load the message based on cursor */
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb) || !check_send_configuration (fb))
		return;
	
	mail_reply (fb->folder, fb->mail_display->current_message, 
		    fb->message_list->cursor_uid, REPLY_SENDER);
}

void
reply_to_list (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb) || !check_send_configuration (fb))
		return;
	
	/* FIXME: make this always load the message based on cursor */
	
	mail_reply (fb->folder, fb->mail_display->current_message, 
		    fb->message_list->cursor_uid, REPLY_LIST);
}

void
reply_to_all (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb) || !check_send_configuration (fb))
		return;
	
	/* FIXME: make this always load the message based on cursor */
	
	mail_reply (fb->folder, fb->mail_display->current_message, 
		    fb->message_list->cursor_uid, REPLY_ALL);
}

void
enumerate_msg (MessageList *ml, const char *uid, gpointer data)
{
	g_return_if_fail (ml != NULL);
	
	g_ptr_array_add ((GPtrArray *) data, g_strdup (uid));
}


static EMsgComposer *
forward_get_composer (CamelMimeMessage *message, const char *subject)
{
	const MailConfigAccount *account = NULL;
	EMsgComposer *composer;
	
	if (message) {
		const CamelInternetAddress *to_addrs, *cc_addrs;
		const GSList *accounts = NULL;
		
		accounts = mail_config_get_accounts ();
		to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
		
		account = guess_me (to_addrs, cc_addrs, accounts);
		
		if (!account) {
			const char *source;
			
			source = camel_mime_message_get_source (message);
			account = mail_config_get_account_by_source_url (source);
		}
	}
	
	if (!account)
		account = mail_config_get_default_account ();
	
	composer = e_msg_composer_new ();
	if (composer) {
		gtk_signal_connect (GTK_OBJECT (composer), "send",
				    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "postpone",
				    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
				    GTK_SIGNAL_FUNC (composer_save_draft_cb), NULL);
		e_msg_composer_set_headers (composer, account->name, NULL, NULL, NULL, subject);
	} else {
		g_warning ("Could not create composer");
	}
	
	return composer;
}

static void
do_forward_non_attached (CamelFolder *folder, char *uid, CamelMimeMessage *message, void *data)
{
	MailConfigForwardStyle style = GPOINTER_TO_INT (data);
	char *subject, *text;
	
	if (!message)
		return;
	
	subject = mail_tool_generate_forward_subject (message);
	text = mail_tool_forward_message (message, style == MAIL_CONFIG_FORWARD_QUOTED);
	
	if (text) {
		EMsgComposer *composer = forward_get_composer (message, subject);
		if (composer) {
			CamelDataWrapper *wrapper;
			
			e_msg_composer_set_body_text (composer, text);
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
			if (CAMEL_IS_MULTIPART (wrapper))
				e_msg_composer_add_message_attachments (composer, message, FALSE);
			
			gtk_widget_show (GTK_WIDGET (composer));
			e_msg_composer_unset_changed (composer);
			e_msg_composer_drop_editor_undo (composer);
		}
		g_free (text);
	}
	
	g_free (subject);
}

static void
forward_message (FolderBrowser *fb, MailConfigForwardStyle style)
{
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!check_send_configuration (fb))
		return;
	
	if (fb->mail_display && fb->mail_display->current_message) {
		do_forward_non_attached (fb->folder, NULL,
					 fb->mail_display->current_message,
					 GINT_TO_POINTER (style));
	} else {
		mail_get_message (fb->folder, fb->message_list->cursor_uid,
				  do_forward_non_attached, GINT_TO_POINTER (style),
				  mail_thread_new);
	}
}

void
forward_inline (GtkWidget *widget, gpointer user_data)
{
	forward_message (user_data, MAIL_CONFIG_FORWARD_INLINE);
}

void
forward_quoted (GtkWidget *widget, gpointer user_data)
{
	forward_message (user_data, MAIL_CONFIG_FORWARD_QUOTED);
}

static void
do_forward_attach (CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *data)
{
	CamelMimeMessage *message = data;
	
	if (part) {
		EMsgComposer *composer = forward_get_composer (message, subject);
		if (composer) {
			e_msg_composer_attach (composer, part);
			gtk_widget_show (GTK_WIDGET (composer));
			e_msg_composer_unset_changed (composer);
			e_msg_composer_drop_editor_undo (composer);
		}
	}
}

void
forward_attached (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = (FolderBrowser *) user_data;
	GPtrArray *uids;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb) || !check_send_configuration (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	mail_build_attachment (fb->folder, uids, do_forward_attach,
			       uids->len == 1 ? fb->mail_display->current_message : NULL);
}

void
forward (GtkWidget *widget, gpointer user_data)
{
	MailConfigForwardStyle style = mail_config_get_default_forward_style ();
	
	if (style == MAIL_CONFIG_FORWARD_ATTACHED)
		forward_attached (widget, user_data);
	else
		forward_message (user_data, style);
}

static EMsgComposer *
redirect_get_composer (CamelMimeMessage *message)
{
	const MailConfigAccount *account = NULL;
	const CamelInternetAddress *to_addrs, *cc_addrs;
	const GSList *accounts = NULL;
	EMsgComposer *composer;
	
	g_return_val_if_fail (message != NULL, NULL);
	
	accounts = mail_config_get_accounts ();
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	
	account = guess_me (to_addrs, cc_addrs, accounts);
	
	if (!account) {
		const char *source;
		
		source = camel_mime_message_get_source (message);
		account = mail_config_get_account_by_source_url (source);
	}
	
	if (!account)
		account = mail_config_get_default_account ();
	
	composer = e_msg_composer_new_redirect (message, account->name);
	if (composer) {
		gtk_signal_connect (GTK_OBJECT (composer), "send",
				    GTK_SIGNAL_FUNC (composer_send_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "postpone",
				    GTK_SIGNAL_FUNC (composer_postpone_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
				    GTK_SIGNAL_FUNC (composer_save_draft_cb), NULL);
	} else {
		g_warning ("Could not create composer");
	}
	
	return composer;
}

static void
do_redirect (CamelFolder *folder, char *uid, CamelMimeMessage *message, void *data)
{
	EMsgComposer *composer;
	
	if (!message)
		return;
	
	composer = redirect_get_composer (message);
	if (composer) {
		CamelDataWrapper *wrapper;
		
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
		if (CAMEL_IS_MULTIPART (wrapper))
			e_msg_composer_add_message_attachments (composer, message, FALSE);
		
		gtk_widget_show (GTK_WIDGET (composer));
		e_msg_composer_unset_changed (composer);
		e_msg_composer_drop_editor_undo (composer);
	}
}

void
redirect (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = (FolderBrowser *) user_data;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!check_send_configuration (fb))
		return;
	
	if (fb->mail_display && fb->mail_display->current_message) {
		do_redirect (fb->folder, NULL,
			     fb->mail_display->current_message,
			     NULL);
	} else {
		mail_get_message (fb->folder, fb->message_list->cursor_uid,
				  do_redirect, NULL, mail_thread_new);
	}
}

static void
transfer_msg_done (gboolean ok, void *data)
{
	FolderBrowser *fb = data;
	int row;
	
	if (ok && !FOLDER_BROWSER_IS_DESTROYED (fb)) {
		row = e_tree_row_of_node (fb->message_list->tree,
					  e_tree_get_cursor (fb->message_list->tree));
		
		/* If this is the last message and deleted messages
                   are hidden, select the previous */
		if ((row + 1 == e_tree_row_count (fb->message_list->tree))
		    && mail_config_get_hide_deleted ())
			message_list_select (fb->message_list, MESSAGE_LIST_SELECT_PREVIOUS,
					     0, CAMEL_MESSAGE_DELETED, FALSE);
		else
			message_list_select (fb->message_list, MESSAGE_LIST_SELECT_NEXT,
					     0, 0, FALSE);
	}
	
	gtk_object_unref (GTK_OBJECT (fb));
}

static void
transfer_msg (FolderBrowser *fb, gboolean delete_from_source)
{
	const char *allowed_types[] = { "mail", "vtrash", NULL };
	extern EvolutionShellClient *global_shell_client;
	char *uri, *physical, *path, *desc;
	static char *last = NULL;
	GPtrArray *uids;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (last == NULL)
		last = g_strdup ("");
	
	if (delete_from_source)
		desc = _("Move message(s) to");
	else
		desc = _("Copy message(s) to");
	
	uri = NULL;
	physical = NULL;
	evolution_shell_client_user_select_folder (global_shell_client,
						   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (fb))),
						   desc, last,
						   allowed_types, &uri, &physical);
	if (!uri)
		return;
	
	path = strchr (uri, '/');
	if (path && strcmp (last, path) != 0) {
		g_free (last);
		last = g_strdup_printf ("evolution:%s", path);
	}
	g_free (uri);
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (delete_from_source) {
		gtk_object_ref (GTK_OBJECT (fb));
		mail_transfer_messages (fb->folder, uids, delete_from_source,
					physical, 0, transfer_msg_done, fb);
	} else {
		mail_transfer_messages (fb->folder, uids, delete_from_source,
					physical, 0, NULL, NULL);
	}
	g_free(physical);
}

void
move_msg_cb (GtkWidget *widget, gpointer user_data)
{
	transfer_msg (user_data, TRUE);
}

void
move_msg (BonoboUIComponent *uih, void *user_data, const char *path)
{
	transfer_msg (user_data, TRUE);
}

void
copy_msg_cb (GtkWidget *widget, gpointer user_data)
{
	transfer_msg (user_data, FALSE);
}

void
copy_msg (BonoboUIComponent *uih, void *user_data, const char *path)
{
	transfer_msg (user_data, FALSE);
}

/* Copied from e-shell-view.c */
static GtkWidget *
find_socket (GtkContainer *container)
{
        GList *children, *tmp;
	
        children = gtk_container_children (container);
        while (children) {
                if (BONOBO_IS_SOCKET (children->data))
                        return children->data;
                else if (GTK_IS_CONTAINER (children->data)) {
                        GtkWidget *socket = find_socket (children->data);
                        if (socket)
                                return socket;
                }
                tmp = children->next;
                g_list_free_1 (children);
                children = tmp;
        }
        return NULL;
}

static void
popup_listener_cb (BonoboListener *listener,
		   char *event_name,
		   CORBA_any *any,
		   CORBA_Environment *ev,
		   gpointer user_data)
{
	char *type = bonobo_event_subtype (event_name);
	
	if (!strcmp (type, "Destroy")) {
		gtk_widget_destroy (GTK_WIDGET (user_data));
	}
	
	g_free (type);
}

void
addrbook_sender (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	CamelMimeMessage *msg = NULL;
	const CamelInternetAddress *addr;
	gchar *addr_str;
	GtkWidget *win;
	GtkWidget *control;
	GtkWidget *socket;
	
	/* FIXME: make this use the cursor message id */
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	msg = fb->mail_display->current_message;
	if (msg == NULL)
		return;
	
	addr = camel_mime_message_get_from (msg);
	if (addr == NULL)
		return;
	
	addr_str = camel_address_format (CAMEL_ADDRESS (addr));
	
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), _("Sender"));
	
	control = bonobo_widget_new_control ("OAFIID:GNOME_Evolution_Addressbook_AddressPopup",
					     CORBA_OBJECT_NIL);
	bonobo_widget_set_property (BONOBO_WIDGET (control),
				    "email", addr_str,
				    NULL);
	
	bonobo_event_source_client_add_listener (bonobo_widget_get_objref (BONOBO_WIDGET (control)),
						 popup_listener_cb, NULL, NULL, win);
	
	socket = find_socket (GTK_CONTAINER (control));
	gtk_signal_connect_object (GTK_OBJECT (socket),
				   "destroy",
				   GTK_SIGNAL_FUNC (gtk_widget_destroy),
				   GTK_OBJECT (win));
	
	gtk_container_add (GTK_CONTAINER (win), control);
	gtk_widget_show_all (win);
}

void
add_sender_to_addrbook (BonoboUIComponent *uih, void *user_data, const char *path)
{
	addrbook_sender (NULL, user_data);
}

void
apply_filters (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	mail_filter_on_demand (fb->folder, uids);
}

void
select_all (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	ESelectionModel *etsm;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (GTK_WIDGET_HAS_FOCUS (fb->mail_display->html)) {
		gtk_html_select_all (fb->mail_display->html);
	} else {
		etsm = e_tree_get_selection_model (fb->message_list->tree);
		
		e_selection_model_select_all (etsm);
	}
}

/* Thread selection */

typedef struct thread_select_info {
	MessageList *ml;
	GPtrArray *paths;
} thread_select_info_t;

static gboolean
select_node (ETreeModel *tm, ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	
	g_ptr_array_add (tsi->paths, path);
	return FALSE; /*not done yet*/
}

static void
thread_select_foreach (ETreePath path, gpointer user_data)
{
	thread_select_info_t *tsi = (thread_select_info_t *) user_data;
	ETreeModel *tm = tsi->ml->model;
	ETreePath node;
	
	/* @path part of the initial selection. If it has children,
	 * we select them as well. If it doesn't, we select its siblings and
	 * their children (ie, the current node must be inside the thread
	 * that the user wants to mark.
	 */
	
	if (e_tree_model_node_get_first_child (tm, path)) 
		node = path;
	else {
		node = e_tree_model_node_get_parent (tm, path);
		
		/* Let's make an exception: if no parent, then we're about
		 * to mark the whole tree. No. */
		if (e_tree_model_node_is_root (tm, node)) 
			node = path;
	}
	
	e_tree_model_node_traverse (tm, node, select_node, tsi);
}

void
select_thread (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	ETreeSelectionModel *selection_model;
	thread_select_info_t tsi;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	/* For every selected item, select the thread containing it.
	 * We can't alter the selection while iterating through it,
	 * so build up a list of paths.
	 */
	
	tsi.ml = fb->message_list;
	tsi.paths = g_ptr_array_new ();
	
	e_tree_selected_path_foreach (fb->message_list->tree, thread_select_foreach, &tsi);
	
	selection_model = E_TREE_SELECTION_MODEL (e_tree_get_selection_model (fb->message_list->tree));
	
	for (i = 0; i < tsi.paths->len; i++)
		e_tree_selection_model_add_to_selection (selection_model,
							 tsi.paths->pdata[i]);
	g_ptr_array_free (tsi.paths, TRUE);
}

void
invert_selection (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	ESelectionModel *etsm;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	etsm = e_tree_get_selection_model (fb->message_list->tree);
	
	e_selection_model_invert_selection (etsm);
}

/* flag all selected messages. Return number flagged */
static int
flag_messages (FolderBrowser *fb, guint32 mask, guint32 set)
{
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return 0;
	
	/* could just use specific callback but i'm lazy */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_flags (fb->folder, uids->pdata[i], mask, set);
		g_free (uids->pdata[i]);
	}
	camel_folder_thaw (fb->folder);
	
	g_ptr_array_free (uids, TRUE);
	
	return i;
}

static int
toggle_flags (FolderBrowser *fb, guint32 mask)
{
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return 0;
	
	/* could just use specific callback but i'm lazy */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++) {
		int flags;
		
		flags = camel_folder_get_message_flags (fb->folder, uids->pdata[i]);
		
		if (flags & mask)
			camel_folder_set_message_flags (fb->folder, uids->pdata[i], mask, 0);
		else {
			if ((mask & CAMEL_MESSAGE_FLAGGED) && (flags & CAMEL_MESSAGE_DELETED))
				camel_folder_set_message_flags (fb->folder, uids->pdata[i], CAMEL_MESSAGE_DELETED, 0);
			camel_folder_set_message_flags (fb->folder, uids->pdata[i], mask, mask);
		}
		
		g_free (uids->pdata[i]);
	}
	camel_folder_thaw (fb->folder);
	
	g_ptr_array_free (uids, TRUE);
	
	return i;
}

void
mark_as_seen (BonoboUIComponent *uih, void *user_data, const char *path)
{
	flag_messages (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

void
mark_as_unseen (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	/* Remove the automatic mark-as-read timer first */
	if (fb->seen_id) {
		gtk_timeout_remove (fb->seen_id);
		fb->seen_id = 0;
	}
	
	flag_messages (fb, CAMEL_MESSAGE_SEEN, 0);
}

void
mark_all_as_seen (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = camel_folder_get_uids (fb->folder);
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++)
		camel_folder_set_message_flags (fb->folder, uids->pdata[i], CAMEL_MESSAGE_SEEN, ~0);
	camel_folder_thaw (fb->folder);
	g_ptr_array_free (uids, TRUE);
}

void
mark_as_important (BonoboUIComponent *uih, void *user_data, const char *path)
{
	flag_messages (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_DELETED, 0);
	flag_messages (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED);
}

void
mark_as_unimportant (BonoboUIComponent *uih, void *user_data, const char *path)
{
	flag_messages (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_FLAGGED, 0);
}

void
toggle_as_important (BonoboUIComponent *uih, void *user_data, const char *path)
{
	toggle_flags (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_FLAGGED);
}


struct _tag_editor_data {
	MessageTagEditor *editor;
	FolderBrowser *fb;
	GPtrArray *uids;
};

static void
tag_editor_ok (GtkWidget *button, gpointer user_data)
{
	struct _tag_editor_data *data = user_data;
	const char *name, *value;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (data->fb))
		goto done;
	
	name = message_tag_editor_get_name (data->editor);
	if (!name)
		goto done;
	
	value = message_tag_editor_get_value (data->editor);
	
	camel_folder_freeze (data->fb->folder);
	for (i = 0; i < data->uids->len; i++) {
		camel_folder_set_message_user_tag (data->fb->folder, data->uids->pdata[i], name, value);
	}
	camel_folder_thaw (data->fb->folder);
	
 done:
	gtk_widget_destroy (GTK_WIDGET (data->editor));
}

static void
tag_editor_cancel (GtkWidget *button, gpointer user_data)
{
	struct _tag_editor_data *data = user_data;
	
	gtk_widget_destroy (GTK_WIDGET (data->editor));
}

static void
tag_editor_destroy (GnomeDialog *dialog, gpointer user_data)
{
	struct _tag_editor_data *data = user_data;
	
	gtk_object_unref (GTK_OBJECT (data->fb));
	g_ptr_array_free (data->uids, TRUE);
	g_free (data);
}

void
flag_for_followup (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	struct _tag_editor_data *data;
	GtkWidget *editor;
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	editor = (GtkWidget *) message_tag_followup_new ();
	
	data = g_new (struct _tag_editor_data, 1);
	data->editor = MESSAGE_TAG_EDITOR (editor);
	gtk_widget_ref (GTK_WIDGET (fb));
	data->fb = fb;
	data->uids = uids;
	
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;
		
		info = camel_folder_get_message_info (fb->folder, uids->pdata[i]);
		message_tag_followup_append_message (MESSAGE_TAG_FOLLOWUP (editor),
						     camel_message_info_from (info),
						     camel_message_info_subject (info));
	}
	
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 0, tag_editor_ok, data);
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 1, tag_editor_cancel, data);
	gnome_dialog_set_close (GNOME_DIALOG (editor), TRUE);
	
	/* special-case... */
	if (uids->len == 1) {
		const char *tag_value;
		
		tag_value = camel_folder_get_message_user_tag (fb->folder, uids->pdata[0], "follow-up");
		if (tag_value)
			message_tag_editor_set_value (MESSAGE_TAG_EDITOR (editor), tag_value);
	}
	
	gtk_signal_connect (GTK_OBJECT (editor), "destroy",
			    tag_editor_destroy, data);
	
	gtk_widget_show (editor);
}

void
flag_followup_completed (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	time_t now;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	now = time (NULL);
	
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++) {
		struct _FollowUpTag *tag;
		const char *tag_value;
		char *value;
		
		tag_value = camel_folder_get_message_user_tag (fb->folder, uids->pdata[i], "follow-up");
		if (!tag_value)
			continue;
		
		tag = message_tag_followup_decode (tag_value);
		tag->completed = now;
		
		value = message_tag_followup_encode (tag);
		g_free (tag);
		
		camel_folder_set_message_user_tag (fb->folder, uids->pdata[i], "follow-up", value);
		g_free (value);
	}
	camel_folder_thaw (fb->folder);
	
	g_ptr_array_free (uids, TRUE);
}

void
flag_followup_clear (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_user_tag (fb->folder, uids->pdata[i], "follow-up", NULL);
	}
	camel_folder_thaw (fb->folder);
	
	g_ptr_array_free (uids, TRUE);
}

void
zoom_in (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	gtk_html_zoom_in (fb->mail_display->html);
}

void
zoom_out (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	gtk_html_zoom_out (fb->mail_display->html);
}

void
zoom_reset (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	gtk_html_zoom_reset (fb->mail_display->html);
}

static void
do_edit_messages (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *data)
{
	/*FolderBrowser *fb = data;*/
	int i;
	
	if (messages == NULL)
		return;
	
	for (i = 0; i < messages->len; i++) {
		EMsgComposer *composer;
		
		camel_medium_remove_header (CAMEL_MEDIUM (messages->pdata[i]), "X-Mailer");
		
		composer = e_msg_composer_new_with_message (messages->pdata[i]);
		
		if (composer) {
			gtk_signal_connect (GTK_OBJECT (composer), "send",
					    composer_send_cb, NULL);
			gtk_signal_connect (GTK_OBJECT (composer), "postpone",
					    composer_postpone_cb, NULL);
			
			/* FIXME: we want to pass data to this callback so
                           we can remove the old draft when they save again */
			gtk_signal_connect (GTK_OBJECT (composer), "save-draft",
					    composer_save_draft_cb, NULL);
			
			gtk_widget_show (GTK_WIDGET (composer));
		}
	}
}

static gboolean
are_you_sure (const char *msg, GPtrArray *uids, FolderBrowser *fb)
{
	GtkWidget *dialog;
	char *buf;
	int button, i;
	
	buf = g_strdup_printf (msg, uids->len);
	dialog = e_gnome_ok_cancel_dialog_parented (buf, NULL, NULL, FB_WINDOW (fb));
	button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	if (button != 0) {
		for (i = 0; i < uids->len; i++)
			g_free (uids->pdata[i]);
		g_ptr_array_free (uids, TRUE);
	}
	
	return button == 0;
}

static void
edit_msg_internal (FolderBrowser *fb)
{
	GPtrArray *uids;
	
	if (!check_send_configuration (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len > 10 && !are_you_sure (_("Are you sure you want to edit all %d messages?"), uids, fb)) {
		int i;
		
		for (i = 0; i < uids->len; i++)
			g_free (uids->pdata[i]);
		
		g_ptr_array_free (uids, TRUE);
		
		return;
	}
	
	mail_get_messages (fb->folder, uids, do_edit_messages, fb);
}

void
edit_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!folder_browser_is_drafts (fb)) {
		GtkWidget *dialog;
		
		dialog = gnome_warning_dialog_parented (_("You may only edit messages saved\n"
							  "in the Drafts folder."),
							FB_WINDOW (fb));
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		return;
	}
	
	edit_msg_internal (fb);
}

static void
do_resend_messages (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *data)
{
	int i;
	
	for (i = 0; i < messages->len; i++) {
		/* generate a new Message-Id because they need to be unique */
		camel_mime_message_set_message_id (messages->pdata[i], NULL);
	}
	
	/* "Resend" should open up the composer to let the user edit the message */
	do_edit_messages (folder, uids, messages, data);
}



void
resend_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!folder_browser_is_sent (fb)) {
		GtkWidget *dialog;
		
		dialog = gnome_warning_dialog_parented (_("You may only resend messages\n"
							  "in the Sent folder."),
							FB_WINDOW (fb));
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		return;
	}
	
	if (!check_send_configuration (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len > 10 && !are_you_sure (_("Are you sure you want to resend all %d messages?"), uids, fb)) {
		int i;
		
		for (i = 0; i < uids->len; i++)
			g_free (uids->pdata[i]);
		
		g_ptr_array_free (uids, TRUE);
		
		return;
	}
	
	mail_get_messages (fb->folder, uids, do_resend_messages, fb);
}

void
search_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GtkWidget *w;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (fb->mail_display->current_message == NULL) {
		GtkWidget *dialog;
		
		dialog = gnome_warning_dialog_parented (_("No Message Selected"), FB_WINDOW (fb));
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		return;
	}
	
	w = mail_search_new (fb->mail_display);
	gtk_widget_show_all (w);
}

void
load_images (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	mail_display_load_images (fb->mail_display);
}

static void
save_msg_ok (GtkWidget *widget, gpointer user_data)
{
	CamelFolder *folder;
	GPtrArray *uids;
	const char *path;
	int fd, ret = 0;
	struct stat st;
	
	path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (user_data));
	if (path[0] == '\0')
		return;
	
	/* make sure we can actually save to it... */
	if (stat (path, &st) != -1 && !S_ISREG (st.st_mode))
		return;
	
	fd = open (path, O_RDONLY);
	if (fd != -1) {
		GtkWidget *dialog;
		GtkWidget *text;
		
		close (fd);
		
		dialog = gnome_dialog_new (_("Overwrite file?"),
					   GNOME_STOCK_BUTTON_YES, 
					   GNOME_STOCK_BUTTON_NO,
					   NULL);
		
		e_gnome_dialog_set_parent (GNOME_DIALOG (dialog), GTK_WINDOW (user_data));
		
		text = gtk_label_new (_("A file by that name already exists.\nOverwrite it?"));
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), text, TRUE, TRUE, 4);
		gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
		gtk_widget_show (text);
		
		ret = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
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
	char *title, *path;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len == 1)
		title = _("Save Message As...");
	else
		title = _("Save Messages As...");
	
	filesel = GTK_FILE_SELECTION (gtk_file_selection_new (title));
	path = g_strdup_printf ("%s/", g_get_home_dir ());
	gtk_file_selection_set_filename (filesel, path);
	g_free (path);
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
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	int deleted, row;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	deleted = flag_messages (fb, CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
				 CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
	
	/* Select the next message if we are only deleting one message */
	if (deleted == 1) {
		row = e_tree_row_of_node (fb->message_list->tree,
					  e_tree_get_cursor (fb->message_list->tree));
		
		/* If this is the last message and deleted messages
                   are hidden, select the previous */
		if ((row+1 == e_tree_row_count (fb->message_list->tree))
		    && mail_config_get_hide_deleted ())
			message_list_select (fb->message_list, MESSAGE_LIST_SELECT_PREVIOUS,
					     0, CAMEL_MESSAGE_DELETED, FALSE);
		else
			message_list_select (fb->message_list, MESSAGE_LIST_SELECT_NEXT,
					     0, 0, FALSE);
	}
}

void
undelete_msg (GtkWidget *button, gpointer user_data)
{
	flag_messages (FOLDER_BROWSER (user_data), CAMEL_MESSAGE_DELETED, 0);
}

void
next_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_NEXT, 0, 0, FALSE);
}

void
next_unread_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_NEXT,
			     0, CAMEL_MESSAGE_SEEN, TRUE);
}

void
next_flagged_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_NEXT,
			     CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED, FALSE);
}

void
previous_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_PREVIOUS,
			     0, 0, FALSE);
}

void
previous_unread_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_PREVIOUS,
			     0, CAMEL_MESSAGE_SEEN, TRUE);
}

void
previous_flagged_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	message_list_select (fb->message_list, MESSAGE_LIST_SELECT_PREVIOUS,
			     CAMEL_MESSAGE_FLAGGED, CAMEL_MESSAGE_FLAGGED, TRUE);
}

static void
expunged_folder (CamelFolder *f, void *data)
{
	FolderBrowser *fb = data;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	fb->expunging = NULL;
	gtk_widget_set_sensitive (GTK_WIDGET (fb->message_list), TRUE);
}

static gboolean
confirm_expunge (FolderBrowser *fb)
{
	GtkWidget *dialog, *label, *checkbox;
	int button;
	
	if (!mail_config_get_confirm_expunge ())
		return TRUE;
	
	dialog = gnome_dialog_new (_("Warning"),
				   GNOME_STOCK_BUTTON_YES,
				   GNOME_STOCK_BUTTON_NO,
				   NULL);
	
	e_gnome_dialog_set_parent (GNOME_DIALOG (dialog), FB_WINDOW (fb));
	
	label = gtk_label_new (_("This operation will permanently erase all messages marked as deleted. If you continue, you will not be able to recover these messages.\n\nReally erase these messages?"));
	
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 4);
	
	checkbox = gtk_check_button_new_with_label (_("Do not ask me again."));
	gtk_object_ref (GTK_OBJECT (checkbox));
	gtk_widget_show (checkbox);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), checkbox, TRUE, TRUE, 4);
	
	button = gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	
	if (button == 0 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		mail_config_set_confirm_expunge (FALSE);
	
	gtk_object_unref (GTK_OBJECT (checkbox));
	
	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

void
expunge_folder (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (fb->folder && (fb->expunging == NULL || fb->folder != fb->expunging) && confirm_expunge (fb)) {
		CamelMessageInfo *info;
		
		/* hide the deleted messages so user can't click on them while we expunge */
		gtk_widget_set_sensitive (GTK_WIDGET (fb->message_list), FALSE);
		
		/* Only blank the mail display if the message being
                   viewed is one of those to be expunged */
		if (fb->loaded_uid) {
			info = camel_folder_get_message_info (fb->folder, fb->loaded_uid);
			
			if (!info || info->flags & CAMEL_MESSAGE_DELETED)
				mail_display_set_message (fb->mail_display, NULL);
		}
		
		fb->expunging = fb->folder;
		mail_expunge_folder (fb->folder, expunged_folder, fb);
	}
}

/********************** Begin Filter Editor ********************/

static GtkWidget *filter_editor = NULL;

static void
filter_editor_destroy (GtkWidget *dialog, gpointer user_data)
{
	filter_editor = NULL;
}

static void
filter_editor_clicked (GtkWidget *dialog, int button, FolderBrowser *fb)
{
	FilterContext *fc;
	
	if (button == 0) {
		char *user;
		
		fc = gtk_object_get_data (GTK_OBJECT (dialog), "context");
		user = g_strdup_printf ("%s/filters.xml", evolution_dir);
		rule_context_save ((RuleContext *)fc, user);
		g_free (user);
	}
	
	if (button != -1) {
		gnome_dialog_close (GNOME_DIALOG (dialog));
	}
}

static const char *filter_source_names[] = {
	"incoming",
	"outgoing",
	NULL,
};

void
filter_edit (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	FilterContext *fc;
	char *user, *system;
	
	if (filter_editor) {
		gdk_window_raise (GTK_WIDGET (filter_editor)->window);
		return;
	}
	
	fc = filter_context_new ();
	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
	system = EVOLUTION_DATADIR "/evolution/filtertypes.xml";
	rule_context_load ((RuleContext *)fc, system, user);
	g_free (user);
	
	if (((RuleContext *)fc)->error) {
		GtkWidget *dialog;
		char *err;
		
		err = g_strdup_printf (_("Error loading filter information:\n%s"),
				       ((RuleContext *)fc)->error);
		dialog = gnome_warning_dialog_parented (err, FB_WINDOW (fb));
		g_free (err);
		
		gnome_dialog_set_close (GNOME_DIALOG (dialog), TRUE);
		gtk_widget_show (dialog);
		return;
	}
	
	filter_editor = (GtkWidget *)filter_editor_new (fc, filter_source_names);
	gnome_dialog_set_parent (GNOME_DIALOG (filter_editor), FB_WINDOW (fb));
	gtk_window_set_title (GTK_WINDOW (filter_editor), _("Filters"));
	
	gtk_object_set_data_full (GTK_OBJECT (filter_editor), "context", fc, (GtkDestroyNotify)gtk_object_unref);
	gtk_signal_connect (GTK_OBJECT (filter_editor), "clicked", filter_editor_clicked, fb);
	gtk_signal_connect (GTK_OBJECT (filter_editor), "destroy", filter_editor_destroy, NULL);
	gtk_widget_show (GTK_WIDGET (filter_editor));
}

/********************** End Filter Editor ********************/

void
vfolder_edit_vfolders (BonoboUIComponent *uih, void *user_data, const char *path)
{
	vfolder_edit ();
}


static MailAccountsDialog *accounts_dialog = NULL;

static void
accounts_dialog_close (GtkWidget *widget, gpointer user_data)
{
	accounts_dialog = NULL;
}

void
providers_config (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (!accounts_dialog) {
		accounts_dialog = mail_accounts_dialog_new (fb->shell);
		gtk_widget_set_parent_window (GTK_WIDGET (accounts_dialog),
					      GTK_WIDGET (FB_WINDOW (fb))->window);
		gtk_signal_connect (GTK_OBJECT (accounts_dialog), "destroy",
				    accounts_dialog_close, NULL);
		gnome_dialog_set_close (GNOME_DIALOG (accounts_dialog), TRUE);
		gtk_widget_show (GTK_WIDGET (accounts_dialog));
	} else {
		gdk_window_raise (GTK_WIDGET (accounts_dialog)->window);
		gtk_widget_grab_focus (GTK_WIDGET (accounts_dialog));
	}
}

/* static void
header_print_cb (GtkHTML *html, GnomePrintContext *print_context,
		 double x, double y, double width, double height, gpointer user_data)
{
	printf ("header_print_cb %f,%f x %f,%f\n", x, y, width, height);

	gnome_print_newpath (print_context);
	gnome_print_setlinewidth (print_context, 12.0);
	gnome_print_setrgbcolor (print_context, 1.0, 0.0, 0.0);
	gnome_print_moveto (print_context, x, y);
	gnome_print_lineto (print_context, x+width, y-height);
	gnome_print_strokepath (print_context);
} */

struct footer_info {
	GnomeFont *local_font;
	gint page_num, pages;
};

static void
footer_print_cb (GtkHTML *html, GnomePrintContext *print_context,
		 double x, double y, double width, double height, gpointer user_data)
{
	struct footer_info *info = (struct footer_info *) user_data;

	if (info->local_font) {
		gchar *text = g_strdup_printf (_("Page %d of %d"), info->page_num, info->pages);
		gdouble tw = gnome_font_get_width_string (info->local_font, text);

		gnome_print_newpath     (print_context);
		gnome_print_setrgbcolor (print_context, .0, .0, .0);
		gnome_print_moveto      (print_context, x + width - tw, y - gnome_font_get_ascender (info->local_font));
		gnome_print_setfont     (print_context, info->local_font);
		gnome_print_show        (print_context, text);

		g_free (text);
		info->page_num++;
	}
}

static void
footer_info_free (struct footer_info *info)
{
	if (info->local_font)
		gnome_font_unref (info->local_font);
	g_free (info);
}

static struct footer_info *
footer_info_new (GtkHTML *html, GnomePrintContext *pc, gdouble *line)
{
	struct footer_info *info;

	info = g_new (struct footer_info, 1);
	info->local_font = gnome_font_new_closest ("Helvetica", GNOME_FONT_BOOK, FALSE, 10);
	if (info->local_font) {
		*line = gnome_font_get_ascender (info->local_font) + gnome_font_get_descender (info->local_font);
	}
	info->page_num = 1;
	info->pages = gtk_html_print_get_pages_num (html, pc, 0.0, *line);

	return info;
}

static void
do_mail_print (FolderBrowser *fb, gboolean preview)
{
	GtkHTML *html;
	GnomePrintContext *print_context;
	GnomePrintMaster *print_master;
	GnomePrintDialog *dialog;
	GnomePrinter *printer = NULL;
	gdouble line = 0.0;
	int copies = 1;
	int collate = FALSE;
	struct footer_info *info;

	if (!preview) {
		dialog = GNOME_PRINT_DIALOG (gnome_print_dialog_new (_("Print Message"),
								     GNOME_PRINT_DIALOG_COPIES));
		gnome_dialog_set_default (GNOME_DIALOG (dialog), GNOME_PRINT_PRINT);
		e_gnome_dialog_set_parent (GNOME_DIALOG (dialog), FB_WINDOW (fb));
		
		switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
		case GNOME_PRINT_PRINT:
			break;	
		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;
		case -1:
			return;
		default:
			gnome_dialog_close (GNOME_DIALOG (dialog));
			return;
		}
		
		gnome_print_dialog_get_copies (dialog, &copies, &collate);
		printer = gnome_print_dialog_get_printer (dialog);
		gnome_dialog_close (GNOME_DIALOG (dialog));
	}
	
	print_master = gnome_print_master_new ();
	
	if (printer)
		gnome_print_master_set_printer (print_master, printer);
	gnome_print_master_set_paper (print_master, gnome_paper_with_name (_("US-Letter")));
	gnome_print_master_set_copies (print_master, copies, collate);
	print_context = gnome_print_master_get_context (print_master);

	html = GTK_HTML (gtk_html_new ());
	mail_display_initialize_gtkhtml (fb->mail_display, html);
	
	/* Set our 'printing' flag to true and render.  This causes us
	   to ignoring any adjustments we made to accomodate the
	   user's theme. */
	fb->mail_display->printing = TRUE;

	mail_display_render (fb->mail_display, html);
	gtk_html_print_set_master (html, print_master);

	info = footer_info_new (html, print_context, &line);
	gtk_html_print_with_header_footer (html, print_context, 0.0, line, NULL, footer_print_cb, info);
	footer_info_free (info);

	fb->mail_display->printing = FALSE;

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

	/* FIXME: We are leaking the GtkHTML object */
}

/* This is pretty evil.  FolderBrowser's API should be extended to allow these sorts of
   things to be done in a more natural way. */

/* <evil_code> */

struct blarg_this_sucks {
	FolderBrowser *fb;
	gboolean preview;
};

static void
done_message_selected (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data)
{
	struct blarg_this_sucks *blarg = data;
	FolderBrowser *fb = blarg->fb;
	gboolean preview = blarg->preview;
	
	g_free (blarg);

	mail_display_set_message (fb->mail_display, (CamelMedium *)msg);

	g_free (fb->loaded_uid);
	fb->loaded_uid = fb->loading_uid;
	fb->loading_uid = NULL;

	do_mail_print (fb, preview);
}

/* Ack!  Most of this is copied from folder-browser.c */
static void
do_mail_fetch_and_print (FolderBrowser *fb, gboolean preview)
{
	if (! fb->preview_shown) {
		/* If the preview pane is closed, we have to do some
		   extra magic to load the message. */
		struct blarg_this_sucks *blarg = g_new (struct blarg_this_sucks, 1);

		blarg->fb = fb;
		blarg->preview = preview;

		fb->loading_id = 0;
	
		/* if we are loading, then set a pending, but leave the loading, coudl cancel here (?) */
		if (fb->loading_uid) {
			g_free (fb->pending_uid);
			fb->pending_uid = g_strdup (fb->new_uid);
		} else {
			if (fb->new_uid) {
				fb->loading_uid = g_strdup (fb->new_uid);
				mail_get_message (fb->folder, fb->loading_uid, done_message_selected, blarg, mail_thread_new);
			} else {
				mail_display_set_message (fb->mail_display, NULL);
				g_free (blarg);
			}
		}

	} else {
		do_mail_print (fb, preview);
	}
}

/* </evil_code> */

void
print_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	do_mail_fetch_and_print (fb, FALSE);
}

void
print_preview_msg (GtkWidget *button, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	do_mail_fetch_and_print (fb, TRUE);
}

/******************** Begin Subscription Dialog ***************************/

static GtkObject *subscribe_dialog = NULL;

static void
subscribe_dialog_destroy (GtkWidget *widget, gpointer user_data)
{
	gtk_object_unref (subscribe_dialog);
	subscribe_dialog = NULL;
}

void
manage_subscriptions (BonoboUIComponent *uih, void *user_data, const char *path)
{
	if (!subscribe_dialog) {
		subscribe_dialog = subscribe_dialog_new ();
		gtk_signal_connect (GTK_OBJECT (SUBSCRIBE_DIALOG (subscribe_dialog)->app), "destroy",
				    subscribe_dialog_destroy, NULL);
		
		gnome_dialog_set_close (GNOME_DIALOG (SUBSCRIBE_DIALOG (subscribe_dialog)->app), TRUE);
		
		subscribe_dialog_show (subscribe_dialog);
	} else {
		gdk_window_raise (SUBSCRIBE_DIALOG (subscribe_dialog)->app->window);
	}
}

/******************** End Subscription Dialog ***************************/

void
configure_folder (BonoboUIComponent *uih, void *user_data, const char *path)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (fb->uri && strncmp (fb->uri, "vfolder:", 8) == 0) {
		vfolder_edit_rule (fb->uri);
	} else {
		mail_local_reconfigure_folder (fb);
	}
}

static void
do_view_message (CamelFolder *folder, char *uid, CamelMimeMessage *message, void *data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (message) {
		GtkWidget *mb;
		
		camel_folder_set_message_flags (folder, uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
		mb = message_browser_new (fb->shell, fb->uri, uid);
		gtk_widget_show (mb);
	}
}

void
view_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids;
	int i;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);

	if (uids->len > 10 && !are_you_sure (_("Are you sure you want to open all %d messages in separate windows?"), uids, fb))
		return;

	for (i = 0; i < uids->len; i++) {
		mail_get_message (fb->folder, uids->pdata [i], do_view_message, fb, mail_thread_queued);
		g_free (uids->pdata [i]);
	}
	g_ptr_array_free (uids, TRUE);
}

void
open_msg (GtkWidget *widget, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	extern CamelFolder *outbox_folder;
	
	if (FOLDER_BROWSER_IS_DESTROYED (fb))
		return;
	
	if (folder_browser_is_drafts (fb) || fb->folder == outbox_folder)
		edit_msg_internal (fb);
	else
		view_msg (NULL, user_data);
}

void
open_message (BonoboUIComponent *uih, void *user_data, const char *path)
{
	open_msg (NULL, user_data);
}

void
edit_message (BonoboUIComponent *uih, void *user_data, const char *path)
{
        edit_msg (NULL, user_data);
}

void
stop_threads (BonoboUIComponent *uih, void *user_data, const char *path)
{
	camel_operation_cancel (NULL);
}

static void
empty_trash_expunged_cb (CamelFolder *folder, void *data)
{
	camel_object_unref (CAMEL_OBJECT (folder));
}

void
empty_trash (BonoboUIComponent *uih, void *user_data, const char *path)
{
	MailConfigAccount *account;
	CamelProvider *provider;
	const GSList *accounts;
	CamelFolder *vtrash;
	FolderBrowser *fb;
	CamelException ex;
	
	fb = user_data ? FOLDER_BROWSER (user_data) : NULL;
	
	if (fb && !confirm_expunge (fb))
		return;
	
	camel_exception_init (&ex);
	
	/* expunge all remote stores */
	accounts = mail_config_get_accounts ();
	while (accounts) {
		account = accounts->data;
		
		/* make sure this is a valid source */
		if (account->source && account->source->enabled && account->source->url) {
			provider = camel_session_get_provider (session, account->source->url, &ex);
			if (provider) {
				/* make sure this store is a remote store */
				if (provider->flags & CAMEL_PROVIDER_IS_STORAGE &&
				    provider->flags & CAMEL_PROVIDER_IS_REMOTE) {
					vtrash = mail_tool_get_trash (account->source->url, FALSE, &ex);
					
					if (vtrash) {
						mail_expunge_folder (vtrash, empty_trash_expunged_cb, NULL);
					}
				}
			}
			
			/* clear the exception for the next round */
			camel_exception_clear (&ex);
		}
		accounts = accounts->next;
	}
	
	/* Now empty the local trash folder */
	vtrash = mail_tool_get_trash ("file:/", TRUE, &ex);
	if (vtrash) {
		mail_expunge_folder (vtrash, empty_trash_expunged_cb, NULL);
	}
	
	camel_exception_clear (&ex);
}

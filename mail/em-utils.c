/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <camel/camel-stream-fs.h>
#include <camel/camel-url-scanner.h>
#include <camel/camel-file-utils.h>

#include <filter/filter-editor.h>

#include "mail-component.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-config-druid.h"
#include "message-tag-followup.h"

#include <e-util/e-mktemp.h>
#include <e-util/e-dialog-utils.h>

#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format-quote.h"

static EAccount *guess_account (CamelMimeMessage *message, CamelFolder *folder);
static void emu_save_part_done (CamelMimePart *part, char *name, int done, void *data);

#define d(x)

/**
 * em_utils_prompt_user:
 * @parent: parent window
 * @def: default response
 * @promptkey: gconf key to check if we should prompt the user or not.
 * @fmt: prompt format
 * @Varargs: varargs
 *
 * Convenience function to query the user with a Yes/No dialog and a
 * "Don't show this dialog again" checkbox. If the user checks that
 * checkbox, then @promptkey is set to %FALSE, otherwise it is set to
 * %TRUE.
 *
 * Returns %TRUE if the user clicks Yes or %FALSE otherwise.
 **/
gboolean
em_utils_prompt_user(GtkWindow *parent, int def, const char *promptkey, const char *fmt, ...)
{
	GtkWidget *mbox, *check = NULL;
	va_list ap;
	int button;
	char *str;
	GConfClient *gconf = mail_config_get_gconf_client();

	if (promptkey
	    && !gconf_client_get_bool(gconf, promptkey, NULL))
		return TRUE;
	
	va_start (ap, fmt);
	str = g_strdup_vprintf (fmt, ap);
	va_end (ap);
	mbox = gtk_message_dialog_new (parent, GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
				       "%s", str);
	g_free (str);
	gtk_dialog_set_default_response ((GtkDialog *) mbox, def);
	if (promptkey) {
		check = gtk_check_button_new_with_label (_("Don't show this message again."));
		gtk_box_pack_start ((GtkBox *)((GtkDialog *) mbox)->vbox, check, TRUE, TRUE, 10);
		gtk_widget_show (check);
	}
	
	button = gtk_dialog_run ((GtkDialog *) mbox);
	if (promptkey)
		gconf_client_set_bool(gconf, promptkey, !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)), NULL);

	gtk_widget_destroy(mbox);
	
	return button == GTK_RESPONSE_YES;
}

/**
 * em_utils_uids_copy:
 * @uids: array of uids
 *
 * Duplicates the array of uids held by @uids into a new
 * GPtrArray. Use em_utils_uids_free() to free the resultant uid
 * array.
 *
 * Returns a duplicate copy of @uids.
 **/
GPtrArray *
em_utils_uids_copy (GPtrArray *uids)
{
	GPtrArray *copy;
	int i;
	
	copy = g_ptr_array_new ();
	g_ptr_array_set_size (copy, uids->len);
	
	for (i = 0; i < uids->len; i++)
		copy->pdata[i] = g_strdup (uids->pdata[i]);
	
	return copy;
}

/**
 * em_utils_uids_free:
 * @uids: array of uids
 *
 * Frees the array of uids pointed to by @uids back to the system.
 **/
void
em_utils_uids_free (GPtrArray *uids)
{
	int i;
	
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	
	g_ptr_array_free (uids, TRUE);
}

static void
druid_destroy_cb (gpointer user_data, GObject *deadbeef)
{
	gtk_main_quit ();
}

/**
 * em_utils_configure_account:
 * @parent: parent window for the druid to be a child of.
 *
 * Displays a druid allowing the user to configure an account. If
 * @parent is non-NULL, then the druid will be created as a child
 * window of @parent's toplevel window.
 *
 * Returns %TRUE if an account has been configured or %FALSE
 * otherwise.
 **/
gboolean
em_utils_configure_account (GtkWidget *parent)
{
	MailConfigDruid *druid;
	
	druid = mail_config_druid_new ();
	
	if (parent != NULL)
		e_dialog_set_transient_for ((GtkWindow *) druid, parent);
	
	g_object_weak_ref ((GObject *) druid, (GWeakNotify) druid_destroy_cb, NULL);
	gtk_widget_show ((GtkWidget *) druid);
	gtk_grab_add ((GtkWidget *) druid);
	gtk_main ();
	
	return mail_config_is_configured ();
}

/**
 * em_utils_check_user_can_send_mail:
 * @parent: parent window for the druid to be a child of.
 *
 * If no accounts have been configured, the user will be given a
 * chance to configure an account. In the case that no accounts are
 * configured, a druid will be created. If @parent is non-NULL, then
 * the druid will be created as a child window of @parent's toplevel
 * window.
 *
 * Returns %TRUE if the user has an account configured (to send mail)
 * or %FALSE otherwise.
 **/
gboolean
em_utils_check_user_can_send_mail (GtkWidget *parent)
{
	EAccount *account;
	
	if (!mail_config_is_configured ()) {
		if (!em_utils_configure_account (parent))
			return FALSE;
	}
	
	if (!(account = mail_config_get_default_account ()))
		return FALSE;
	
	/* Check for a transport */
	if (!account->transport->url)
		return FALSE;
	
	return TRUE;
}

/* Editing Filters/vFolders... */

static GtkWidget *filter_editor = NULL;

static void
filter_editor_response (GtkWidget *dialog, int button, gpointer user_data)
{
	FilterContext *fc;
	
	if (button == GTK_RESPONSE_OK) {
		char *user;
		
		fc = g_object_get_data ((GObject *) dialog, "context");
		user = g_strdup_printf ("%s/mail/filters.xml",
					mail_component_peek_base_directory (mail_component_peek ()));
		rule_context_save ((RuleContext *) fc, user);
		g_free (user);
	}
	
	gtk_widget_destroy (dialog);
	
	filter_editor = NULL;
}

static const char *filter_source_names[] = {
	"incoming",
	"outgoing",
	NULL,
};

/**
 * em_utils_edit_filters:
 * @parent: parent window
 *
 * Opens or raises the filters editor dialog so that the user may edit
 * his/her filters. If @parent is non-NULL, then the dialog will be
 * created as a child window of @parent's toplevel window.
 **/
void
em_utils_edit_filters (GtkWidget *parent)
{
	const char *base_directory = mail_component_peek_base_directory (mail_component_peek ());
	char *user, *system;
	FilterContext *fc;
	
	if (filter_editor) {
		gdk_window_raise (GTK_WIDGET (filter_editor)->window);
		return;
	}
	
	fc = filter_context_new ();
	user = g_strdup_printf ("%s/mail/filters.xml", base_directory);
	system = EVOLUTION_PRIVDATADIR "/filtertypes.xml";
	rule_context_load ((RuleContext *) fc, system, user);
	g_free (user);
	
	if (((RuleContext *) fc)->error) {
		e_notice (parent, GTK_MESSAGE_ERROR,
			  _("Error loading filter information:\n%s"),
			  ((RuleContext *) fc)->error);
		return;
	}
	
	filter_editor = (GtkWidget *) filter_editor_new (fc, filter_source_names);
	if (parent != NULL)
		e_dialog_set_transient_for ((GtkWindow *) filter_editor, parent);
	
	gtk_window_set_title (GTK_WINDOW (filter_editor), _("Filters"));
	g_object_set_data_full ((GObject *) filter_editor, "context", fc, (GtkDestroyNotify) g_object_unref);
	g_signal_connect (filter_editor, "response", G_CALLBACK (filter_editor_response), NULL);
	gtk_widget_show (GTK_WIDGET (filter_editor));
}

/* Composing messages... */

static EMsgComposer *
create_new_composer (void)
{
	EMsgComposer *composer;
	
	composer = e_msg_composer_new ();
	
	em_composer_utils_setup_default_callbacks (composer);
	
	return composer;
}

/**
 * em_utils_compose_new_message:
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window.
 **/
void
em_utils_compose_new_message (void)
{
	GtkWidget *composer;
	
	composer = (GtkWidget *) create_new_composer ();
	
	gtk_widget_show (composer);
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer fields will be filled in
 * according to the values in the mailto url.
 **/
void
em_utils_compose_new_message_with_mailto (const char *url)
{
	EMsgComposer *composer;
	
	if (url != NULL)
		composer = e_msg_composer_new_from_url (url);
	else
		composer = e_msg_composer_new ();
	
	em_composer_utils_setup_default_callbacks (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/**
 * em_utils_post_to_folder:
 * @folder: folder
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @folder is non-NULL, the composer will default to posting
 * mail to the folder specified by @folder.
 **/
void
em_utils_post_to_folder (CamelFolder *folder)
{
	EMsgComposer *composer;
	EAccount *account;
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);
	
	if (folder != NULL) {
		char *url = mail_tools_folder_to_url (folder);
		
		e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) ((EMsgComposer *) composer)->hdrs, url);
		g_free (url);
		
		url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
		account = mail_config_get_account_by_source_url (url);
		g_free (url);
		
		if (account)
			e_msg_composer_set_headers (composer, account->name, NULL, NULL, NULL, "");
	}
	
	em_composer_utils_setup_default_callbacks (composer);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/**
 * em_utils_post_to_url:
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer will default to posting
 * mail to the folder specified by @url.
 **/
void
em_utils_post_to_url (const char *url)
{
	EMsgComposer *composer;
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);
	
	if (url != NULL)
		e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) ((EMsgComposer *) composer)->hdrs, url);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/* Editing messages... */

static void
edit_message (CamelMimeMessage *message, CamelFolder *drafts, const char *uid)
{
	EMsgComposer *composer;
	
	composer = e_msg_composer_new_with_message (message);
	em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, drafts, uid);
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
}

/**
 * em_utils_edit_message:
 * @message: message to edit
 *
 * Opens a composer filled in with the headers/mime-parts/etc of
 * @message.
 **/
void
em_utils_edit_message (CamelMimeMessage *message)
{
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	
	edit_message (message, NULL, NULL);
}

static void
edit_messages (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *user_data)
{
	int i;
	
	if (msgs == NULL)
		return;
	
	for (i = 0; i < msgs->len; i++) {
		camel_medium_remove_header (CAMEL_MEDIUM (msgs->pdata[i]), "X-Mailer");
		
		edit_message (msgs->pdata[i], folder, uids->pdata[i]);
	}
}

/**
 * em_utils_edit_messages:
 * @folder: folder containing messages to edit
 * @uids: uids of messages to edit
 *
 * Opens a composer for each message to be edited.
 **/
void
em_utils_edit_messages (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, edit_messages, NULL);
}

/* Forwarding messages... */

static void
forward_attached (CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *user_data)
{
	EMsgComposer *composer;
	
	if (part == NULL)
		return;
	
	composer = create_new_composer ();
	e_msg_composer_set_headers (composer, NULL, NULL, NULL, NULL, subject);
	e_msg_composer_attach (composer, part);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
}

/**
 * em_utils_forward_attached:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * If there is more than a single message in @uids, a multipart/digest
 * will be constructed and attached to a new composer window preset
 * with the appropriate header defaults for forwarding the first
 * message in the list. If only one message is to be forwarded, it is
 * forwarded as a simple message/rfc822 attachment.
 **/
void
em_utils_forward_attached (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_build_attachment (folder, uids, forward_attached, NULL);
}

static void
forward_non_attached (GPtrArray *messages, int style)
{
	CamelMimeMessage *message;
	CamelDataWrapper *wrapper;
	EMsgComposer *composer;
	char *subject, *text;
	int i;
	guint32 flags;

	if (messages->len == 0)
		return;

	flags = EM_FORMAT_QUOTE_HEADERS;
	if (style == MAIL_CONFIG_FORWARD_QUOTED)
		flags |= EM_FORMAT_QUOTE_CITE;

	for (i = 0; i < messages->len; i++) {
		message = messages->pdata[i];
		subject = mail_tool_generate_forward_subject (message);
		
		text = em_utils_message_to_html (message, _("-------- Forwarded Message --------"), flags);
		
		if (text) {
			composer = create_new_composer ();
			e_msg_composer_set_headers (composer, NULL, NULL, NULL, NULL, subject);
			e_msg_composer_set_body_text (composer, text);
			
			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
			if (CAMEL_IS_MULTIPART (wrapper))
				e_msg_composer_add_message_attachments (composer, message, FALSE);
			
			e_msg_composer_unset_changed (composer);
			e_msg_composer_drop_editor_undo (composer);
			
			gtk_widget_show (GTK_WIDGET (composer));
			
			g_free (text);
		}
		
		g_free (subject);
	}
}

static void
forward_inline (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *user_data)
{
	forward_non_attached (messages, MAIL_CONFIG_FORWARD_INLINE);
}

/**
 * em_utils_forward_inline:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * Forwards each message in the 'inline' form, each in its own composer window.
 **/
void
em_utils_forward_inline (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, forward_inline, NULL);
}

static void
forward_quoted (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *user_data)
{
	forward_non_attached (messages, MAIL_CONFIG_FORWARD_QUOTED);
}

/**
 * em_utils_forward_quoted:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * Forwards each message in the 'quoted' form (each line starting with
 * a "> "), each in its own composer window.
 **/
void
em_utils_forward_quoted (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, forward_quoted, NULL);
}

/**
 * em_utils_forward_message:
 * @parent: parent window
 * @message: message to be forwarded
 *
 * Forwards a message in the user's configured default style.
 **/
void
em_utils_forward_message (CamelMimeMessage *message)
{
	GPtrArray *messages;
	CamelMimePart *part;
	GConfClient *gconf;
	char *subject;
	int mode;
	
	messages = g_ptr_array_new ();
	g_ptr_array_add (messages, message);
	
	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);
	
	switch (mode) {
	case MAIL_CONFIG_FORWARD_ATTACHED:
	default:
		part = mail_tool_make_message_attachment (message);
		
		subject = mail_tool_generate_forward_subject (message);
		
		forward_attached (NULL, messages, part, subject, NULL);
		camel_object_unref (part);
		g_free (subject);
		break;
	case MAIL_CONFIG_FORWARD_INLINE:
		forward_non_attached (messages, MAIL_CONFIG_FORWARD_INLINE);
		break;
	case MAIL_CONFIG_FORWARD_QUOTED:
		forward_non_attached (messages, MAIL_CONFIG_FORWARD_QUOTED);
		break;
	}
	
	g_ptr_array_free (messages, TRUE);
}

/**
 * em_utils_forward_messages:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * Forwards a group of messages in the user's configured default
 * style.
 **/
void
em_utils_forward_messages (CamelFolder *folder, GPtrArray *uids)
{
	GConfClient *gconf;
	int mode;
	
	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);
	
	switch (mode) {
	case MAIL_CONFIG_FORWARD_ATTACHED:
	default:
		em_utils_forward_attached (folder, uids);
		break;
	case MAIL_CONFIG_FORWARD_INLINE:
		em_utils_forward_inline (folder, uids);
		break;
	case MAIL_CONFIG_FORWARD_QUOTED:
		em_utils_forward_quoted (folder, uids);
		break;
	}
}

/* Redirecting messages... */

static EMsgComposer *
redirect_get_composer (CamelMimeMessage *message)
{
	EMsgComposer *composer;
	EAccount *account;
	
	/* QMail will refuse to send a message if it finds one of
	   it's Delivered-To headers in the message, so remove all
	   Delivered-To headers. Fixes bug #23635. */
	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Delivered-To"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Delivered-To");
	
	account = guess_account (message, NULL);
	
	composer = e_msg_composer_new_redirect (message, account ? account->name : NULL);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	return composer;
}

/**
 * em_utils_redirect_message:
 * @message: message to redirect
 *
 * Opens a composer to redirect @message (Note: only headers will be
 * editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message (CamelMimeMessage *message)
{
	EMsgComposer *composer;
	CamelDataWrapper *wrapper;
	
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	
	composer = redirect_get_composer (message);
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	if (CAMEL_IS_MULTIPART (wrapper))
		e_msg_composer_add_message_attachments (composer, message, FALSE);
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
}

static void
redirect_msg (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	if (message == NULL)
		return;
	
	em_utils_redirect_message (message);
}

/**
 * em_utils_redirect_message_by_uid:
 * @folder: folder containing message to be redirected
 * @uid: uid of message to be redirected
 *
 * Opens a composer to redirect the message (Note: only headers will
 * be editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message_by_uid (CamelFolder *folder, const char *uid)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, redirect_msg, NULL, mail_thread_new);
}

/* Replying to messages... */

static GHashTable *
generate_account_hash (void)
{
	GHashTable *account_hash;
	EAccount *account, *def;
	EAccountList *accounts;
	EIterator *iter;
	
	accounts = mail_config_get_accounts ();
	account_hash = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
	
	/* add the default account to the hash first */
	if ((def = mail_config_get_default_account ())) {
		if (def->id->address)
			g_hash_table_insert (account_hash, (char *) def->id->address, (void *) def);
	}
	
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->id->address) {
			EAccount *acnt;
			
			/* Accounts with identical email addresses that are enabled
			 * take precedence over the accounts that aren't. If all
			 * accounts with matching email addresses are disabled, then
			 * the first one in the list takes precedence. The default
			 * account always takes precedence no matter what.
			 */
			acnt = g_hash_table_lookup (account_hash, account->id->address);
			if (acnt && acnt != def && !acnt->enabled && account->enabled) {
				g_hash_table_remove (account_hash, acnt->id->address);
				acnt = NULL;
			}
			
			if (!acnt)
				g_hash_table_insert (account_hash, (char *) account->id->address, (void *) account);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return account_hash;
}

static EABDestination **
em_utils_camel_address_to_destination (CamelInternetAddress *iaddr)
{
	EABDestination *dest, **destv;
	int n, i, j;
	
	if (iaddr == NULL)
		return NULL;
	
	if ((n = camel_address_length ((CamelAddress *) iaddr)) == 0)
		return NULL;
	
	destv = g_malloc (sizeof (EABDestination *) * (n + 1));
	for (i = 0, j = 0; i < n; i++) {
		const char *name, *addr;
		
		if (camel_internet_address_get (iaddr, i, &name, &addr)) {
			dest = eab_destination_new ();
			eab_destination_set_name (dest, name);
			eab_destination_set_email (dest, addr);
			
			destv[j++] = dest;
		}
	}
	
	if (j == 0) {
		g_free (destv);
		return NULL;
	}
	
	destv[j] = NULL;
	
	return destv;
}

static EMsgComposer *
reply_get_composer (CamelMimeMessage *message, EAccount *account,
		    CamelInternetAddress *to, CamelInternetAddress *cc,
		    CamelFolder *folder, const char *postto)
{
	const char *message_id, *references;
	EABDestination **tov, **ccv;
	EMsgComposer *composer;
	char *subject;
	
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (to == NULL || CAMEL_IS_INTERNET_ADDRESS (to), NULL);
	g_return_val_if_fail (cc == NULL || CAMEL_IS_INTERNET_ADDRESS (cc), NULL);

	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	ccv = em_utils_camel_address_to_destination (cc);

	if (tov || ccv) {
		if (postto)
			composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL_POST);
		else
			composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL);
	} else
		composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);

	/* Set the subject of the new message. */
	if ((subject = (char *) camel_mime_message_get_subject (message))) {
		if (strncasecmp (subject, "Re: ", 4) != 0)
			subject = g_strdup_printf ("Re: %s", subject);
		else
			subject = g_strdup (subject);
	} else {
		subject = g_strdup ("");
	}

	e_msg_composer_set_headers (composer, account ? account->name : NULL, tov, ccv, NULL, subject);
	
	g_free (subject);
	
	/* add post-to, if nessecary */
	if (postto) {
		char *store_url = NULL;
		
		if (folder) {
			store_url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
			if (store_url[strlen (store_url) - 1] == '/')
				store_url[strlen (store_url)-1] = '\0';
		}
		
		e_msg_composer_hdrs_set_post_to_base (E_MSG_COMPOSER_HDRS (composer->hdrs), store_url ? store_url : "", postto);
		g_free (store_url);
	}
	
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

static EAccount *
guess_account (CamelMimeMessage *message, CamelFolder *folder)
{
	const CamelInternetAddress *to, *cc;
	GHashTable *account_hash = NULL;
	EAccount *account = NULL;
	const char *posthdr, *addr;
	char *tmp;
	int i;
	
	/* check for newsgroup header */
	posthdr = camel_medium_get_header (CAMEL_MEDIUM (message), "Newsgroups");
	
	if (posthdr && folder) {
		/* this was posted at a newsgroup! */
		tmp = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
		account = mail_config_get_account_by_source_url (tmp);
		g_free (tmp);
		if (account)
			goto found;
	}
	
	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	
	if (to == NULL && cc == NULL)
		return NULL;
	
	account_hash = generate_account_hash ();
	
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
	
 found:
	
	if (account_hash)
		g_hash_table_destroy (account_hash);
	
	return account;
}

static void
get_reply_sender (CamelMimeMessage *message, CamelInternetAddress **to, const char **postto)
{
	const CamelInternetAddress *reply_to;
	const char *name, *addr, *posthdr;
	int i;
	
	/* check whether there is a 'Newsgroups: ' header in there */
	posthdr = camel_medium_get_header (CAMEL_MEDIUM (message), "Newsgroups");
	if (posthdr && postto) {
		*postto = posthdr;
		while (**postto == ' ')
			(*postto)++;
		return;
	}
	
	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);
	
	if (reply_to) {
		*to = camel_internet_address_new ();
		
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++)
			camel_internet_address_add (*to, name, addr);
	}
}

static gboolean
get_reply_list (CamelMimeMessage *message, CamelInternetAddress **to)
{
	const char *header, *p;
	char *addr;
	
	/* Examples:
	 * 
	 * List-Post: <mailto:list@host.com>
	 * List-Post: <mailto:moderator@host.com?subject=list%20posting>
	 * List-Post: NO (posting not allowed on this list)
	 */
	if (!(header = camel_medium_get_header ((CamelMedium *) message, "List-Post")))
		return FALSE;
	
	while (*header == ' ' || *header == '\t')
		header++;
	
	/* check for NO */
	if (!strncasecmp (header, "NO", 2))
		return FALSE;
	
	/* Search for the first mailto angle-bracket enclosed URL.
	 * (See rfc2369, Section 2, paragraph 3 for details) */
	if (!(header = camel_strstrcase (header, "<mailto:")))
		return FALSE;
	
	header += 8;
	
	p = header;
	while (*p && !strchr ("?>", *p))
		p++;
	
	addr = g_strndup (header, p - header);
	
	*to = camel_internet_address_new ();
	camel_internet_address_add (*to, NULL, addr);
	
	g_free (addr);
	
	return TRUE;
}

static void
concat_unique_addrs (CamelInternetAddress *dest, const CamelInternetAddress *src, GHashTable *rcpt_hash)
{
	const char *name, *addr;
	int i;
	
	for (i = 0; camel_internet_address_get (src, i, &name, &addr); i++) {
		if (!g_hash_table_lookup (rcpt_hash, addr)) {
			camel_internet_address_add (dest, name, addr);
			g_hash_table_insert (rcpt_hash, (char *) addr, GINT_TO_POINTER (1));
		}
	}
}

static void
get_reply_all (CamelMimeMessage *message, CamelInternetAddress **to, CamelInternetAddress **cc, const char **postto)
{
	const CamelInternetAddress *reply_to, *to_addrs, *cc_addrs;
	const char *name, *addr, *posthdr;
	GHashTable *rcpt_hash;
	int i;
	
	/* check whether there is a 'Newsgroups: ' header in there */
	posthdr = camel_medium_get_header (CAMEL_MEDIUM(message), "Newsgroups");
	if (posthdr && postto) {
		*postto = posthdr;
		while (**postto == ' ')
			(*postto)++;
	}
	
	rcpt_hash = generate_account_hash ();
	
	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);
	
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	
	*to = camel_internet_address_new ();
	*cc = camel_internet_address_new ();
	
	if (reply_to) {
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++) {
			/* ignore references to the Reply-To address in the To and Cc lists */
			if (addr && !g_hash_table_lookup (rcpt_hash, addr)) {
				/* In the case that we are doing a Reply-To-All, we do not want
				   to include the user's email address because replying to oneself
				   is kinda silly. */
				
				camel_internet_address_add (*to, name, addr);
				g_hash_table_insert (rcpt_hash, (char *) addr, GINT_TO_POINTER (1));
			}
		}
	}
	
	concat_unique_addrs (*cc, to_addrs, rcpt_hash);
	concat_unique_addrs (*cc, cc_addrs, rcpt_hash);
	
	/* promote the first Cc: address to To: if To: is empty */
	if (camel_address_length ((CamelAddress *) *to) == 0 && camel_address_length ((CamelAddress *) *cc) > 0) {
		camel_internet_address_get (*cc, 0, &name, &addr);
		camel_internet_address_add (*to, name, addr);
		camel_address_remove ((CamelAddress *) *cc, 0);
	}
	
	g_hash_table_destroy (rcpt_hash);
}

static void
composer_set_body (EMsgComposer *composer, CamelMimeMessage *message)
{
	const CamelInternetAddress *sender;
	char *text, *credits, format[256];
	const char *name, *addr;
	CamelMimePart *part;
	GConfClient *gconf;
	time_t date;
	int date_offset;
	
	gconf = mail_config_get_gconf_client ();
	
	switch (gconf_client_get_int (gconf, "/apps/evolution/mail/format/reply_style", NULL)) {
	case MAIL_CONFIG_REPLY_DO_NOT_QUOTE:
		/* do nothing */
		break;
	case MAIL_CONFIG_REPLY_ATTACH:
		/* attach the original message as an attachment */
		part = mail_tool_make_message_attachment (message);
		e_msg_composer_attach (composer, part);
		camel_object_unref (part);
		break;
	case MAIL_CONFIG_REPLY_QUOTED:
	default:
		/* do what any sane user would want when replying... */
		sender = camel_mime_message_get_from (message);
		if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
			camel_internet_address_get (sender, 0, &name, &addr);
		} else {
			name = _("an unknown sender");
		}

		date = camel_mime_message_get_date(message, &date_offset);
		/* Convert to UTC */
		date += (date_offset / 100) * 60 * 60;
		date += (date_offset % 100) * 60;

		/* translators: attribution string used when quoting messages,
		   it must contain a single single %%+05d followed by a single '%%s' */
		e_utf8_strftime(format, sizeof(format), _("On %a, %Y-%m-%d at %H:%M %%+05d, %%s wrote:"), gmtime(&date));
		credits = g_strdup_printf(format, date_offset, name && *name ? name : addr);
		text = em_utils_message_to_html(message, credits, EM_FORMAT_QUOTE_CITE);
		g_free (credits);
		e_msg_composer_set_body_text(composer, text);
		g_free (text);
		break;
	}
	
	e_msg_composer_drop_editor_undo (composer);
}

/**
 * em_utils_reply_to_message:
 * @message: message to reply to
 * @mode: reply mode
 *
 * Creates a new composer ready to reply to @message.
 **/
void
em_utils_reply_to_message (CamelMimeMessage *message, int mode)
{
	CamelInternetAddress *to = NULL, *cc = NULL;
	EMsgComposer *composer;
	EAccount *account;
	
	account = guess_account (message, NULL);
	
	switch (mode) {
	case REPLY_MODE_SENDER:
		get_reply_sender (message, &to, NULL);
		break;
	case REPLY_MODE_LIST:
		if (get_reply_list (message, &to))
			break;
	case REPLY_MODE_ALL:
		get_reply_all (message, &to, &cc, NULL);
		break;
	}
	
	composer = reply_get_composer (message, account, to, cc, NULL, NULL);
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	if (cc != NULL)
		camel_object_unref (cc);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
}

static void
reply_to_message (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	CamelInternetAddress *to = NULL, *cc = NULL;
	const char *postto = NULL;
	EMsgComposer *composer;
	EAccount *account;
	guint32 flags;
	int mode;
	
	mode = GPOINTER_TO_INT (user_data);

	account = guess_account (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;
	
	switch (mode) {
	case REPLY_MODE_SENDER:
		get_reply_sender (message, &to, &postto);
		break;
	case REPLY_MODE_LIST:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (get_reply_list (message, &to))
			break;
	case REPLY_MODE_ALL:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		get_reply_all (message, &to, &cc, &postto);
		break;
	}
	
	composer = reply_get_composer (message, account, to, cc, folder, postto);
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	if (cc != NULL)
		camel_object_unref (cc);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_callbacks (composer, folder, uid, flags, flags, NULL, NULL);
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
}

/**
 * em_utils_reply_to_message_by_uid:
 * @folder: folder containing message to reply to
 * @uid: message uid
 * @mode: reply mode
 *
 * Creates a new composer ready to reply to the message referenced by
 * @folder and @uid.
 **/
void
em_utils_reply_to_message_by_uid (CamelFolder *folder, const char *uid, int mode)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, reply_to_message, GINT_TO_POINTER (mode), mail_thread_new);
}

/* Posting replies... */

static void
post_reply_to_message (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	/* FIXME: would be nice if this shared more code with reply_get_composer() */
	const char *message_id, *references;
	CamelInternetAddress *to = NULL;
	EABDestination **tov = NULL;
	EMsgComposer *composer;
	char *subject, *url;
	EAccount *account;
	guint32 flags;
	
	account = guess_account (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;
	
	get_reply_sender (message, &to, NULL);
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL_POST);
	
	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	
	/* Set the subject of the new message. */
	if ((subject = (char *) camel_mime_message_get_subject (message))) {
		if (strncasecmp (subject, "Re: ", 4) != 0)
			subject = g_strdup_printf ("Re: %s", subject);
		else
			subject = g_strdup (subject);
	} else {
		subject = g_strdup ("");
	}
	
	e_msg_composer_set_headers (composer, account ? account->name : NULL, tov, NULL, NULL, subject);
	
	g_free (subject);
	
	url = mail_tools_folder_to_url (folder);
	e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) composer->hdrs, url);
	g_free (url);
	
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
	
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_callbacks (composer, folder, uid, flags, flags, NULL, NULL);
	
	gtk_widget_show (GTK_WIDGET (composer));	
	e_msg_composer_unset_changed (composer);
}

/**
 * em_utils_post_reply_to_message_by_uid:
 * @folder: folder containing message to reply to
 * @uid: message uid
 * @mode: reply mode
 *
 * Creates a new composer (post mode) ready to reply to the message
 * referenced by @folder and @uid.
 **/
void
em_utils_post_reply_to_message_by_uid (CamelFolder *folder, const char *uid)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, post_reply_to_message, NULL, mail_thread_new);
}

/* Saving messages... */

static GtkFileSelection *
emu_get_save_filesel (GtkWidget *parent, const char *title, const char *name)
{
	GtkFileSelection *filesel;
	char *gdir, *mname = NULL, *filename;
	const char *realname, *dir;
	GConfClient *gconf;

	filesel = (GtkFileSelection *)gtk_file_selection_new(title);
	if (parent)
		e_dialog_set_transient_for((GtkWindow *)filesel, parent);

	gconf = gconf_client_get_default();
	dir = gdir = gconf_client_get_string(gconf, "/apps/evolution/mail/save_dir", NULL);
	g_object_unref(gconf);
	if (dir == NULL)
		dir = g_get_home_dir();

	if (name && name[0]) {
		realname = mname = g_strdup(name);
		e_filename_make_safe(mname);
	} else {
		realname = "/";
	}

	filename = g_build_filename(dir, realname, NULL);
	gtk_file_selection_set_filename(filesel, filename);
	g_free(filename);
	g_free(mname);
	g_free (gdir);

	return filesel;
}

static void
emu_update_save_path(const char *filename)
{
	char *dir = g_path_get_dirname(filename);
	GConfClient *gconf = gconf_client_get_default();

	gconf_client_set_string(gconf, "/apps/evolution/mail/save_dir", dir, NULL);
	g_object_unref(gconf);
	g_free(dir);
}

static gboolean
emu_can_save(GtkWindow *parent, const char *path)
{
	struct stat st;
	
	if (path[0] == 0)
		return FALSE;

	/* make sure we can actually save to it... */
	if (stat (path, &st) != -1 && !S_ISREG (st.st_mode))
		return FALSE;
	
	if (access (path, F_OK) == 0) {
		if (access (path, W_OK) != 0) {
			e_notice (parent, GTK_MESSAGE_ERROR,
				 _("Cannot save to `%s'\n %s"), path, g_strerror (errno));
			return FALSE;
		}
		
		return em_utils_prompt_user (parent, GTK_RESPONSE_NO, NULL,
					     _("`%s' already exists.\nOverwrite it?"), path);
	}
	
	return TRUE;
}

static void
emu_save_part_response(GtkFileSelection *filesel, int response, CamelMimePart *part)
{
	if (response == GTK_RESPONSE_OK) {
		const char *path = gtk_file_selection_get_filename(filesel);

		if (!emu_can_save((GtkWindow *)filesel, path))
			return;

		emu_update_save_path(path);
		/* FIXME: popup error if it fails? */
		mail_save_part(part, path, NULL, NULL);
	}

	gtk_widget_destroy((GtkWidget *)filesel);
	camel_object_unref(part);
}

/**
 * em_utils_save_part:
 * @parent: parent window
 * @prompt: prompt string
 * @part: part to save
 *
 * Saves a mime part to disk (prompting the user for filename).
 **/
void
em_utils_save_part(GtkWidget *parent, const char *prompt, CamelMimePart *part)
{
	const char *name;
	GtkFileSelection *filesel;

	name = camel_mime_part_get_filename(part);
	if (name == NULL) {
		if (CAMEL_IS_MIME_MESSAGE(part)) {
			name = camel_mime_message_get_subject((CamelMimeMessage *)part);
			if (name == NULL)
				name = _("message");
		} else {
			name = _("attachment");
		}
	}

	filesel = emu_get_save_filesel(parent, prompt, name);
	camel_object_ref(part);
	g_signal_connect(filesel, "response", G_CALLBACK(emu_save_part_response), part);
	gtk_widget_show((GtkWidget *)filesel);
}

/**
 * em_utils_save_part_to_file:
 * @parent: parent window
 * @filename: filename to save to
 * @part: part to save
 * 
 * Save a part's content to a specific file
 * Creates all needed directories and overwrites without prompting
 *
 * Returns %TRUE if saving succeeded, %FALSE otherwise
 **/
gboolean
em_utils_save_part_to_file(GtkWidget *parent, const char *filename, CamelMimePart *part) 
{
	int done;
	char *dirname;
	struct stat st;
	
	if (filename[0] == 0)
		return FALSE;
	
	dirname = g_path_get_dirname(filename);
	if (camel_mkdir(dirname, 0777) == -1) {
		e_notice(parent, GTK_MESSAGE_ERROR,
				 _("Cannot save to `%s'\n %s"), filename, g_strerror(errno));
		g_free(dirname);
		return FALSE;
	}
	g_free(dirname);

	if (access(filename, F_OK) == 0) {
		if (access(filename, W_OK) != 0) {
			e_notice(parent, GTK_MESSAGE_ERROR,
				 _("Cannot save to `%s'\n %s"), filename, g_strerror(errno));
			return FALSE;
		}
	}
	
	if (stat(filename, &st) != -1 && !S_ISREG(st.st_mode)) {
		e_notice(parent, GTK_MESSAGE_ERROR,
				 _("Error: '%s' exists and is not a regular file"), filename);
		return FALSE;
	}
	
	/* FIXME: This doesn't handle default charsets */
	mail_msg_wait(mail_save_part(part, filename, emu_save_part_done, &done));
	
	return done;
}

struct _save_messages_data {
	CamelFolder *folder;
	GPtrArray *uids;
};

static void
emu_save_messages_response(GtkFileSelection *filesel, int response, struct _save_messages_data *data)
{
	if (response == GTK_RESPONSE_OK) {
		const char *path = gtk_file_selection_get_filename(filesel);

		if (!emu_can_save((GtkWindow *)filesel, path))
			return;

		emu_update_save_path(path);
		mail_save_messages(data->folder, data->uids, path, NULL, NULL);
		data->uids = NULL;
	}

	camel_object_unref(data->folder);
	if (data->uids)
		em_utils_uids_free(data->uids);
	g_free(data);
	gtk_widget_destroy((GtkWidget *)filesel);
}

/**
 * em_utils_save_messages:
 * @parent: parent window
 * @folder: folder containing messages to save
 * @uids: uids of messages to save
 *
 * Saves a group of messages to disk in mbox format (prompting the
 * user for filename).
 **/
void
em_utils_save_messages (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids)
{
	struct _save_messages_data *data;
	GtkFileSelection *filesel;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	filesel = emu_get_save_filesel(parent, _("Save Message..."), NULL);
	camel_object_ref(folder);
	
	data = g_malloc(sizeof(struct _save_messages_data));
	data->folder = folder;
	data->uids = uids;

	g_signal_connect(filesel, "response", G_CALLBACK(emu_save_messages_response), data);
	gtk_widget_show((GtkWidget *)filesel);
}

/* ********************************************************************** */

static void
emu_add_address_cb(BonoboListener *listener, const char *name, const CORBA_any *any, CORBA_Environment *ev, void *data)
{
	char *type = bonobo_event_subtype(name);
	
	if (!strcmp(type, "Destroy"))
		gtk_widget_destroy((GtkWidget *)data);
	
	g_free(type);
}

/**
 * em_utils_add_address:
 * @parent: 
 * @email: 
 * 
 * Add address @email to the addressbook.
 **/
void em_utils_add_address(struct _GtkWidget *parent, const char *email)
{
	GtkWidget *win;
	GtkWidget *control;
	/*GtkWidget *socket;*/

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title((GtkWindow *)win, _("Add address"));
	
	control = bonobo_widget_new_control("OAFIID:GNOME_Evolution_Addressbook_AddressPopup:" BASE_VERSION, CORBA_OBJECT_NIL);
	bonobo_widget_set_property((BonoboWidget *)control, "email", TC_CORBA_string, email, NULL);
	
	bonobo_event_source_client_add_listener(bonobo_widget_get_objref((BonoboWidget *)control), emu_add_address_cb, NULL, NULL, win);
	
	/*socket = find_socket (GTK_CONTAINER (control));
	  g_object_weak_ref ((GObject *) socket, (GWeakNotify) gtk_widget_destroy, win);*/

	gtk_container_add((GtkContainer *)win, control);
	gtk_widget_show_all(win);
}

/* ********************************************************************** */
/* Flag-for-Followup... */

/* tag-editor callback data */
struct ted_t {
	MessageTagEditor *editor;
	CamelFolder *folder;
	GPtrArray *uids;
};

static void
ted_free (struct ted_t *ted)
{
	camel_object_unref (ted->folder);
	em_utils_uids_free (ted->uids);
	g_free (ted);
}

static void
tag_editor_response (GtkWidget *dialog, int button, struct ted_t *ted)
{
	CamelFolder *folder;
	CamelTag *tags, *t;
	GPtrArray *uids;
	int i;
	
	if (button == GTK_RESPONSE_OK && (tags = message_tag_editor_get_tag_list (ted->editor))) {
		folder = ted->folder;
		uids = ted->uids;
		
		camel_folder_freeze (folder);
		for (i = 0; i < uids->len; i++) {
			for (t = tags; t; t = t->next)
				camel_folder_set_message_user_tag (folder, uids->pdata[i], t->name, t->value);
		}
		
		camel_folder_thaw (folder);
		camel_tag_list_free (&tags);
	}
	
	gtk_widget_destroy (dialog);
}

/**
 * em_utils_flag_for_followup:
 * @parent: parent window
 * @folder: folder containing messages to flag
 * @uids: uids of messages to flag
 *
 * Open the Flag-for-Followup editor for the messages specified by
 * @folder and @uids.
 **/
void
em_utils_flag_for_followup (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids)
{
	GtkWidget *editor;
	struct ted_t *ted;
	int i;
	
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	editor = (GtkWidget *) message_tag_followup_new ();
	
	if (parent != NULL)
		e_dialog_set_transient_for ((GtkWindow *) editor, parent);
	
	camel_object_ref (folder);
	
	ted = g_new (struct ted_t, 1);
	ted->editor = MESSAGE_TAG_EDITOR (editor);
	ted->folder = folder;
	ted->uids = uids;
	
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;
		
		info = camel_folder_get_message_info (folder, uids->pdata[i]);
		message_tag_followup_append_message (MESSAGE_TAG_FOLLOWUP (editor),
						     camel_message_info_from (info),
						     camel_message_info_subject (info));
	}
	
	/* special-case... */
	if (uids->len == 1) {
		CamelMessageInfo *info;
		
		info = camel_folder_get_message_info (folder, uids->pdata[0]);
		if (info) {
			if (info->user_tags)
				message_tag_editor_set_tag_list (MESSAGE_TAG_EDITOR (editor), info->user_tags);
			camel_folder_free_message_info (folder, info);
		}
	}
	
	g_signal_connect (editor, "response", G_CALLBACK (tag_editor_response), ted);
	g_object_weak_ref ((GObject *) editor, (GWeakNotify) ted_free, ted);
	
	gtk_widget_show (editor);
}

/**
 * em_utils_flag_for_followup_clear:
 * @parent: parent window
 * @folder: folder containing messages to unflag
 * @uids: uids of messages to unflag
 *
 * Clears the Flag-for-Followup flag on the messages referenced by
 * @folder and @uids.
 **/
void
em_utils_flag_for_followup_clear (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids)
{
	int i;
	
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_user_tag (folder, uids->pdata[i], "follow-up", "");
		camel_folder_set_message_user_tag (folder, uids->pdata[i], "due-by", "");
		camel_folder_set_message_user_tag (folder, uids->pdata[i], "completed-on", "");
	}
	camel_folder_thaw (folder);
	
	em_utils_uids_free (uids);
}

/**
 * em_utils_flag_for_followup_completed:
 * @parent: parent window
 * @folder: folder containing messages to 'complete'
 * @uids: uids of messages to 'complete'
 *
 * Sets the completed state (and date/time) for each message
 * referenced by @folder and @uids that is marked for
 * Flag-for-Followup.
 **/
void
em_utils_flag_for_followup_completed (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids)
{
	char *now;
	int i;
	
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	now = camel_header_format_date (time (NULL), 0);
	
	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		const char *tag;
		
		tag = camel_folder_get_message_user_tag (folder, uids->pdata[i], "follow-up");
		if (tag == NULL || *tag == '\0')
			continue;
		
		camel_folder_set_message_user_tag (folder, uids->pdata[i], "completed-on", now);
	}
	camel_folder_thaw (folder);
	
	g_free (now);
	
	em_utils_uids_free (uids);
}

#include "camel/camel-stream-mem.h"
#include "camel/camel-stream-filter.h"
#include "camel/camel-mime-filter-from.h"

/* This kind of sucks, because for various reasons most callers need to run synchronously
   in the gui thread, however this could take a long, blocking time, to run */
static int
em_utils_write_messages_to_stream(CamelFolder *folder, GPtrArray *uids, CamelStream *stream)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterFrom *from_filter;
	int i, res = 0;

	from_filter = camel_mime_filter_from_new();
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)from_filter);
	camel_object_unref(from_filter);
	
	for (i=0; i<uids->len; i++) {
		CamelMimeMessage *message;
		char *from;

		message = camel_folder_get_message(folder, uids->pdata[i], NULL);
		if (message == NULL) {
			res = -1;
			break;
		}

		/* we need to flush after each stream write since we are writing to the same stream */
		from = camel_mime_message_build_mbox_from(message);

		if (camel_stream_write_string(stream, from) == -1
		    || camel_stream_flush(stream) == -1
		    || camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, (CamelStream *)filtered_stream) == -1
		    || camel_stream_flush((CamelStream *)filtered_stream) == -1)
			res = -1;
		
		g_free(from);
		camel_object_unref(message);

		if (res == -1)
			break;
	}

	camel_object_unref(filtered_stream);

	return res;
}

/* This kind of sucks, because for various reasons most callers need to run synchronously
   in the gui thread, however this could take a long, blocking time, to run */
static int
em_utils_read_messages_from_stream(CamelFolder *folder, CamelStream *stream)
{
	CamelException *ex = camel_exception_new();
	CamelMimeParser *mp = camel_mime_parser_new();
	int res = -1;

	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_init_with_stream(mp, stream);
	camel_object_unref(stream);

	while (camel_mime_parser_step(mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *msg;

		/* NB: de-from filter, once written */
		msg = camel_mime_message_new();
		if (camel_mime_part_construct_from_parser((CamelMimePart *)msg, mp) == -1) {
			camel_object_unref(msg);
			break;
		}
		
		camel_folder_append_message(folder, msg, NULL, NULL, ex);
		camel_object_unref(msg);
		
		if (camel_exception_is_set (ex))
			break;
		
		camel_mime_parser_step(mp, 0, 0);
	}
	
	camel_object_unref(mp);
	if (!camel_exception_is_set(ex))
		res = 0;
	camel_exception_free(ex);

	return res;
}

/**
 * em_utils_selection_set_mailbox:
 * @data: selection data
 * @folder: folder containign messages to copy into the selection
 * @uids: uids of the messages to copy into the selection
 * 
 * Creates a mailbox-format selection.
 * Warning: Could be BIG!
 * Warning: This could block the ui for an extended period.
 **/
void
em_utils_selection_set_mailbox(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids)
{
	CamelStream *stream;

	stream = camel_stream_mem_new();
	if (em_utils_write_messages_to_stream(folder, uids, stream) == 0)
		gtk_selection_data_set(data, data->target, 8,
				       ((CamelStreamMem *)stream)->buffer->data,
				       ((CamelStreamMem *)stream)->buffer->len);

	camel_object_unref(stream);
}

/**
 * em_utils_selection_get_mailbox:
 * @data: selection data
 * @folder: 
 * 
 * Receive a mailbox selection/dnd
 * Warning: Could be BIG!
 * Warning: This could block the ui for an extended period.
 * FIXME: Exceptions?
 **/
void
em_utils_selection_get_mailbox(GtkSelectionData *data, CamelFolder *folder)
{
	CamelStream *stream;

	if (data->data == NULL || data->length == -1)
		return;

	/* TODO: a stream mem with read-only access to existing data? */
	/* NB: Although copying would let us run this async ... which it should */
	stream = camel_stream_mem_new_with_buffer(data->data, data->length);
	em_utils_read_messages_from_stream(folder, stream);
	camel_object_unref(stream);
}

/**
 * em_utils_selection_set_uidlist:
 * @data: selection data
 * @uri:
 * @uids: 
 * 
 * Sets a "x-uid-list" format selection data.
 *
 * FIXME: be nice if this could take a folder argument rather than uri
 **/
void
em_utils_selection_set_uidlist(GtkSelectionData *data, const char *uri, GPtrArray *uids)
{
	GByteArray *array = g_byte_array_new();
	int i;

	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn\0" */
	
	g_byte_array_append(array, uri, strlen(uri)+1);

	for (i=0; i<uids->len; i++)
		g_byte_array_append(array, uids->pdata[i], strlen(uids->pdata[i])+1);
		
	gtk_selection_data_set(data, data->target, 8, array->data, array->len);
	g_byte_array_free(array, TRUE);
}

/**
 * em_utils_selection_get_uidlist:
 * @data: selection data
 * @urip: Pointer to uri string, to be free'd by caller
 * @uidsp: Pointer to an array of uid's.
 * 
 * Convert an x-uid-list type to a uri and a uid list.
 * 
 * Return value: The number of uid's found.  If 0, then @urip and
 * @uidsp will be empty.
 **/
int
em_utils_selection_get_uidlist(GtkSelectionData *data, char **urip, GPtrArray **uidsp)
{
	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
	char *inptr, *inend;
	GPtrArray *uids;
	int res;

	*urip = NULL;
	*uidsp = NULL;

	if (data == NULL || data->data == NULL || data->length == -1)
		return 0;
	
	uids = g_ptr_array_new();

	inptr = data->data;
	inend = data->data + data->length;
	while (inptr < inend) {
		char *start = inptr;

		while (inptr < inend && *inptr)
			inptr++;

		if (start > (char *)data->data)
			g_ptr_array_add(uids, g_strndup(start, inptr-start));

		inptr++;
	}

	if (uids->len == 0) {
		g_ptr_array_free(uids, TRUE);
		res = 0;
	} else {
		*urip = g_strdup(data->data);
		*uidsp = uids;
		res = uids->len;
	}

	return res;
}

/**
 * em_utils_selection_set_urilist:
 * @data: 
 * @folder: 
 * @uids: 
 * 
 * Set the selection data @data to a uri which points to a file, which is
 * a berkely mailbox format mailbox.  The file is automatically cleaned
 * up when the application quits.
 **/
void
em_utils_selection_set_urilist(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids)
{
	const char *tmpdir;
	CamelStream *fstream;
	char *uri, *p;
	int fd;

	tmpdir = e_mkdtemp("drag-n-drop-XXXXXX");
	if (tmpdir == NULL)
		return;

	/* FIXME: this used to save a single message with the subject
	   as the filename but it was unsafe, and makes this messier,
	   the pain */
	
	p = uri = g_alloca (strlen (tmpdir) + 16);
	p += sprintf (uri, "file:///%s/mbox", tmpdir);
	
	fd = open(uri + 7, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1)
		return;
	
	fstream = camel_stream_fs_new_with_fd(fd);
	if (fstream) {
		/* terminate with \r\n to be compliant with the spec */
		strcpy (p, "\r\n");
		
		if (em_utils_write_messages_to_stream(folder, uids, fstream) == 0)
			gtk_selection_data_set(data, data->target, 8, uri, strlen(uri));

		camel_object_unref(fstream);
	}
}

static void
emu_save_part_done(CamelMimePart *part, char *name, int done, void *data)
{
	((int *)data)[0] = done;
}

/**
 * em_utils_temp_save_part:
 * @parent: 
 * @part: 
 * 
 * Save a part's content to a temporary file, and return the
 * filename.
 * 
 * Return value: NULL if anything failed.
 **/
char *
em_utils_temp_save_part(GtkWidget *parent, CamelMimePart *part)
{
	const char *tmpdir, *filename;
	char *path, *mfilename = NULL;
	int done;

	tmpdir = e_mkdtemp("evolution-tmp-XXXXXX");
	if (tmpdir == NULL) {
		e_notice(parent, GTK_MESSAGE_ERROR,
			 _("Could not create temporary directory: %s"),
			 g_strerror (errno));

		return NULL;
	}

	filename = camel_mime_part_get_filename (part);
	if (filename == NULL) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		mfilename = g_strdup(filename);
		e_filename_make_safe(mfilename);
		filename = mfilename;
	}

	path = g_build_filename(tmpdir, filename, NULL);
	g_free(mfilename);

	/* FIXME: This doesn't handle default charsets */
	mail_msg_wait(mail_save_part(part, path, emu_save_part_done, &done));

	if (!done) {
		/* mail_save_part should popup an error box automagically */
		g_free(path);
		path = NULL;
	}

	return path;
}

/**
 * em_utils_folder_is_drafts:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is a Drafts folder.
 * 
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_drafts(CamelFolder *folder, const char *uri)
{
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	int is = FALSE;

	if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_DRAFTS))
		return TRUE;

	if (uri == NULL)
		return FALSE;

	accounts = mail_config_get_accounts();
	iter = e_list_get_iterator((EList *)accounts);
	while (e_iterator_is_valid(iter)) {
		account = (EAccount *)e_iterator_get(iter);
		if (account->drafts_folder_uri &&
		    camel_store_uri_cmp(folder->parent_store, account->drafts_folder_uri, uri)) {
			is = TRUE;
			break;
		}
		
		e_iterator_next(iter);
	}
	
	g_object_unref(iter);
	
	return is;
}

/**
 * em_utils_folder_is_sent:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is a Sent folder
 * 
 * Returns %TRUE if this is a Sent folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_sent(CamelFolder *folder, const char *uri)
{
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	int is = FALSE;

	if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_SENT))
		return TRUE;

	if (uri == NULL)
		return FALSE;

	accounts = mail_config_get_accounts();
	iter = e_list_get_iterator((EList *)accounts);
	while (e_iterator_is_valid(iter)) {
		account = (EAccount *)e_iterator_get(iter);
		if (account->sent_folder_uri &&
		    camel_store_uri_cmp(folder->parent_store, account->sent_folder_uri, uri)) {
			is = TRUE;
			break;
		}
		
		e_iterator_next(iter);
	}
	
	g_object_unref(iter);
	
	return is;
}

/**
 * em_utils_folder_is_outbox:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is an Outbox folder
 * 
 * Returns %TRUE if this is an Outbox folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_outbox(CamelFolder *folder, const char *uri)
{
	/* <Highlander>There can be only one.</Highlander> */
	return folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
}

/**
 * em_utils_adjustment_page:
 * @adj: 
 * @down: 
 * 
 * Move an adjustment up/down forward/back one page.
 **/
void
em_utils_adjustment_page(GtkAdjustment *adj, gboolean down)
{
	float page_size = adj->page_size - adj->step_increment;

	if (down) {
		if (adj->value < adj->upper - adj->page_size - page_size)
			adj->value += page_size;
		else if (adj->upper >= adj->page_size)
			adj->value = adj->upper - adj->page_size;
		else
			adj->value = adj->lower;
	} else {
		if (adj->value > adj->lower + page_size)
			adj->value -= page_size;
		else
			adj->value = adj->lower;
	}

	gtk_adjustment_value_changed(adj);
}

/* ********************************************************************** */
static char *emu_proxy_uri;

static void
emu_set_proxy(GConfClient *client)
{
	char *server;
	int port;

	if (!gconf_client_get_bool(client, "/system/http_proxy/use_http_proxy", NULL)) {
		g_free(emu_proxy_uri);
		emu_proxy_uri = NULL;

		return;
	}

	/* TODO: Should lock ... */

	server = gconf_client_get_string(client, "/system/http_proxy/host", NULL);
	port = gconf_client_get_int(client, "/system/http_proxy/port", NULL);

	if (server && server[0]) {
		g_free(emu_proxy_uri);

		if (gconf_client_get_bool(client, "/system/http_proxy/use_authentication", NULL)) {
			char *user = gconf_client_get_string(client, "/system/http_proxy/authentication_user", NULL);
			char *pass = gconf_client_get_string(client, "/system/http_proxy/authentication_password", NULL);

			emu_proxy_uri = g_strdup_printf("http://%s:%s@%s:%d", user, pass, server, port);
			g_free(user);
			g_free(pass);
		} else {
			emu_proxy_uri = g_strdup_printf("http://%s:%d", server, port);
		}
	}

	g_free(server);
}

static void
emu_proxy_changed(GConfClient *client, guint32 cnxn_id, GConfEntry *entry, gpointer user_data)
{
	emu_set_proxy(client);
}

/**
 * em_utils_get_proxy_uri:
 * 
 * Get the system proxy uri.
 * 
 * Return value: Must be freed when finished with.
 **/
char *
em_utils_get_proxy_uri(void)
{
	static int init;

	if (!init) {
		GConfClient *client = gconf_client_get_default();

		gconf_client_add_dir(client, "/system/http_proxy", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		gconf_client_notify_add(client, "/system/http_proxy", emu_proxy_changed, NULL, NULL, NULL);
		emu_set_proxy(client);
		g_object_unref(client);
		init = TRUE;
	}

	return g_strdup(emu_proxy_uri);
}

/**
 * em_utils_part_to_html:
 * @part:
 *
 * Converts a mime part's contents into html text.  If @credits is given,
 * then it will be used as an attribution string, and the
 * content will be cited.  Otherwise no citation or attribution
 * will be performed.
 * 
 * Return Value: The part in displayable html format.
 **/
char *
em_utils_part_to_html(CamelMimePart *part)
{
	EMFormatQuote *emfq;
	CamelStreamMem *mem;
	GByteArray *buf;
	char *text;
	
	buf = g_byte_array_new ();
	mem = (CamelStreamMem *) camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (mem, buf);
	
	emfq = em_format_quote_new(NULL, (CamelStream *)mem, 0);
	em_format_part((EMFormat *) emfq, (CamelStream *) mem, part);
	g_object_unref (emfq);
	
	camel_stream_write ((CamelStream *) mem, "", 1);
	camel_object_unref (mem);
	
	text = buf->data;
	g_byte_array_free (buf, FALSE);
	
	return text;
}

/**
 * em_utils_message_to_html:
 * @message: 
 * @credits: 
 * @flags: EMFormatQuote flags
 *
 * Convert a message to html, quoting if the @credits attribution
 * string is given.
 * 
 * Return value: The html version.
 **/
char *
em_utils_message_to_html(CamelMimeMessage *message, const char *credits, guint32 flags)
{
	EMFormatQuote *emfq;
	CamelStreamMem *mem;
	GByteArray *buf;
	char *text;
	
	buf = g_byte_array_new ();
	mem = (CamelStreamMem *) camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (mem, buf);
	
	emfq = em_format_quote_new(credits, (CamelStream *)mem, flags);
	em_format_format((EMFormat *)emfq, NULL, NULL, message);
	g_object_unref (emfq);
	
	camel_stream_write ((CamelStream *) mem, "", 1);
	camel_object_unref (mem);
	
	text = buf->data;
	g_byte_array_free (buf, FALSE);
	
	return text;
}

/* ********************************************************************** */

/**
 * em_utils_expunge_folder:
 * @parent: parent window
 * @folder: folder to expunge
 *
 * Expunges @folder.
 **/
void
em_utils_expunge_folder (GtkWidget *parent, CamelFolder *folder)
{
	char *name;

	camel_object_get(folder, NULL, CAMEL_OBJECT_DESCRIPTION, &name, 0);

	if (!em_utils_prompt_user ((GtkWindow *) parent, GTK_RESPONSE_NO, 
				   "/apps/evolution/mail/prompts/expunge",
				   _("This operation will permanently remove all deleted messages "
				     "in the folder `%s'. If you continue, you "
				     "will not be able to recover these messages.\n"
				     "\nReally erase these messages?"), name))
		return;
	
	mail_expunge_folder(folder, NULL, NULL);
}

/**
 * em_utils_empty_trash:
 * @parent: parent window
 *
 * Empties all Trash folders.
 **/
void
em_utils_empty_trash (GtkWidget *parent)
{
	extern CamelSession *session;
	CamelProvider *provider;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	CamelException ex;
	
	if (!em_utils_prompt_user ((GtkWindow *) parent, GTK_RESPONSE_NO, "/apps/evolution/mail/prompts/empty_trash",
				   _("This operation will permanently remove all deleted messages "
				     "in all folders. If you continue, you will not be able to "
				     "recover these messages.\n"
				     "\nReally erase these messages?")))
		return;
	
	camel_exception_init (&ex);
	
	/* expunge all remote stores */
	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		/* make sure this is a valid source */
		if (account->enabled && account->source->url) {
			provider = camel_session_get_provider (session, account->source->url, &ex);
			if (provider) {
				/* make sure this store is a remote store */
				if (provider->flags & CAMEL_PROVIDER_IS_STORAGE &&
				    provider->flags & CAMEL_PROVIDER_IS_REMOTE) {
					mail_empty_trash (account, NULL, NULL);
				}
			}
			
			/* clear the exception for the next round */
			camel_exception_clear (&ex);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	/* Now empty the local trash folder */
	mail_empty_trash (NULL, NULL, NULL);
}

char *
em_utils_folder_name_from_uri (const char *uri)
{
	CamelURL *url;
	char *folder_name;
	
	if (uri == NULL || (url = camel_url_new (uri, NULL)) == NULL)
	    return NULL;
	
	folder_name = url->fragment ? url->fragment : url->path + 1;
	
	if (folder_name == NULL) {
		camel_url_free (url);
		return NULL;
	}
	
	folder_name = g_strdup (folder_name);
	camel_url_free (url);
	
	return folder_name;
}

extern struct _CamelSession *session;

/* email: uri's are based on the account, with special cases for local
 * stores, vfolder and local mail.
 * e.g.
 *  imap account imap://user@host/ -> email://accountid@accountid.host/
 *  vfolder      vfolder:/storage/path#folder -> email://vfolder@local/folder
 *  local        local:/storage/path#folder   -> email://local@local/folder
 */

char *em_uri_from_camel(const char *curi)
{
	CamelURL *curl;
	EAccount *account;
	const char *uid, *path;
	char *euri, *tmp;
	CamelProvider *provider;

	provider = camel_session_get_provider(session, curi, NULL);
	if (provider == NULL) {
		d(printf("em uri from camel failed '%s'\n", curi));
		return g_strdup(curi);
	}

	curl = camel_url_new(curi, NULL);
	if (curl == NULL)
		return g_strdup(curi);

	if (strcmp(curl->protocol, "vfolder") == 0)
		uid = "vfolder@local";
	else if ((account = mail_config_get_account_by_source_url(curi)) == NULL)
		uid = "local@local";
	else
		uid = account->uid;
	path = (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)?curl->fragment:curl->path;
	if (path[0] == '/')
		path++;

	tmp = camel_url_encode(path, ";?");
	euri = g_strdup_printf("email://%s/%s", uid, tmp);
	g_free(tmp);
	
	d(printf("em uri from camel '%s' -> '%s'\n", curi, euri));

	camel_url_free(curl);

	return euri;
}

char *em_uri_to_camel(const char *euri)
{
	EAccountList *accounts;
	const EAccount *account;
	EAccountService *service;
	CamelProvider *provider;
	CamelURL *eurl, *curl;
	char *uid, *curi;

	if (strncmp(euri, "email:", 6) != 0) {
		d(printf("em uri to camel not euri '%s'\n", euri));
		return g_strdup(euri);
	}

	eurl = camel_url_new(euri, NULL);
	if (eurl == NULL)
		return g_strdup(euri);

	g_assert(eurl->host != NULL);

	if (eurl->user != NULL) {
		/* Sigh, shoul'dve used mbox@local for mailboxes, not local@local */
		if (strcmp(eurl->host, "local") == 0
		    && (strcmp(eurl->user, "local") == 0 || strcmp(eurl->user, "vfolder") == 0)) {
			char *base;

			if (strcmp(eurl->user, "vfolder") == 0)
				curl = camel_url_new("vfolder:", NULL);
			else
				curl = camel_url_new("mbox:", NULL);

			base = g_strdup_printf("%s/.evolution/mail/%s", g_get_home_dir(), eurl->user);
			camel_url_set_path(curl, base);
			g_free(base);
			camel_url_set_fragment(curl, eurl->path[0]=='/'?eurl->path+1:eurl->path);
			curi = camel_url_to_string(curl, 0);
			camel_url_free(curl);
			camel_url_free(eurl);

			d(printf("em uri to camel local '%s' -> '%s'\n", euri, curi));
			return curi;
		}

		uid = g_strdup_printf("%s@%s", eurl->user, eurl->host);
	} else {
		uid = g_strdup(eurl->host);
	}

	accounts = mail_config_get_accounts();
	account = e_account_list_find(accounts, E_ACCOUNT_FIND_UID, uid);
	g_free(uid);

	if (account == NULL) {
		camel_url_free(eurl);
		d(printf("em uri to camel no account '%s' -> '%s'\n", euri, euri));
		return g_strdup(euri);
	}

	service = account->source;
	provider = camel_session_get_provider(session, service->url, NULL);

	curl = camel_url_new(service->url, NULL);
	if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		camel_url_set_fragment(curl, eurl->path[0]=='/'?eurl->path+1:eurl->path);
	else
		camel_url_set_path(curl, eurl->path);

	curi = camel_url_to_string(curl, 0);

	camel_url_free(eurl);
	camel_url_free(curl);

	d(printf("em uri to camel '%s' -> '%s'\n", euri, curi));

	return curi;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-composer-actions.h"
#include "e-composer-private.h"

#include <errno.h>
#include <fcntl.h>
#include <e-util/e-error.h>
#include <mail/em-event.h>
#include <mail/em-format-html-print.h>

#include "misc/e-charset-picker.h"

static void
action_attach_cb (GtkAction *action,
                  EMsgComposer *composer)
{
	EAttachmentBar *bar;
	GtkWidget *dialog;
	GtkWidget *option;
	GSList *uris, *iter;
	gboolean active;
	gint response;

	bar = E_ATTACHMENT_BAR (composer->priv->attachment_bar);

	dialog = gtk_file_chooser_dialog_new (
		_("Insert Attachment"),
		GTK_WINDOW (composer),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("A_ttach"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (
		GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_file_chooser_set_select_multiple (
		GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_window_set_icon_name (
		GTK_WINDOW (dialog), "mail-message-new");

	option = gtk_check_button_new_with_mnemonic (
		_("_Suggest automatic display of attachment"));
	gtk_widget_show (option);
	gtk_file_chooser_set_extra_widget (
		GTK_FILE_CHOOSER (dialog), option);

	response = gtkhtml_editor_file_chooser_dialog_run (
		GTKHTML_EDITOR (composer), dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dialog));
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (option));

	for (iter = uris; iter != NULL; iter = iter->next) {
		const gchar *disposition;
		CamelURL *url;

		url = camel_url_new (iter->data, NULL);
		if (url == NULL)
			continue;

		disposition = active ? "inline" : "attachment";
		if (!g_ascii_strcasecmp (url->protocol, "file"))
			e_attachment_bar_attach (bar, url->path, disposition);
		else
			e_attachment_bar_attach (bar, iter->data, disposition);

		camel_url_free (url);
	}

	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);

exit:
	gtk_widget_destroy (dialog);
}

static void
action_charset_cb (GtkRadioAction *action,
                   GtkRadioAction *current,
                   EMsgComposer *composer)
{
	const gchar *charset;

	if (action != current)
		return;

	charset = gtk_action_get_name (GTK_ACTION (current));

	g_free (composer->priv->charset);
	composer->priv->charset = g_strdup (charset);
}

static void
action_close_cb (GtkAction *action,
                 EMsgComposer *composer)
{
	GtkhtmlEditor *editor;
	EComposerHeaderTable *table;
	const gchar *subject;
	gint response;

	editor = GTKHTML_EDITOR (composer);

	if (!gtkhtml_editor_get_changed (editor) &&
		!e_composer_autosave_get_saved (composer)) {

		gtk_widget_destroy (GTK_WIDGET (composer));
		return;
	}

	gdk_window_raise (GTK_WIDGET (composer)->window);

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || *subject == '\0')
		subject = _("Untitled Message");

	response = e_error_run (
		GTK_WINDOW (composer),
		"mail-composer:exit-unsaved",
		subject, NULL);

	switch (response) {
		case GTK_RESPONSE_YES:
			gtk_action_activate (ACTION (SAVE_DRAFT));
			break;

		case GTK_RESPONSE_NO:
			gtk_widget_destroy (GTK_WIDGET (composer));
			break;

		case GTK_RESPONSE_CANCEL:
			break;
	}
}

static void
action_pgp_encrypt_cb (GtkToggleAction *action,
                       EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static void
action_pgp_sign_cb (GtkToggleAction *action,
                    EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static void
action_print_cb (GtkAction *action,
                 EMsgComposer *composer)
{
	GtkPrintOperationAction print_action;
	CamelMimeMessage *message;
	EMFormatHTMLPrint *efhp;

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	message = e_msg_composer_get_message (composer, 1);

	efhp = em_format_html_print_new (NULL, print_action);
	em_format_html_print_raw_message (efhp, message);
	g_object_unref (efhp);
}

static void
action_print_preview_cb (GtkAction *action,
                         EMsgComposer *composer)
{
	GtkPrintOperationAction print_action;
	CamelMimeMessage *message;
	EMFormatHTMLPrint *efhp;

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	message = e_msg_composer_get_message_print (composer, 1);

	efhp = em_format_html_print_new (NULL, print_action);
	em_format_html_print_raw_message (efhp, message);
	g_object_unref (efhp);
}

static void
action_save_cb (GtkAction *action,
                EMsgComposer *composer)
{
	GtkhtmlEditor *editor = GTKHTML_EDITOR (composer);
	const gchar *filename;
	gint fd;
	GError *error = NULL;

	filename = gtkhtml_editor_get_filename (editor);
	if (filename == NULL) {
		gtk_action_activate (ACTION (SAVE_AS));
		return;
	}

	/* Check if the file already exists and we can create it. */
	fd = g_open (filename, O_RDONLY | O_CREAT | O_EXCL, 0777);
	if (fd < 0) {
		gint errno_saved = errno;

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			gint response;

			response = e_error_run (
				GTK_WINDOW (composer),
				E_ERROR_ASK_FILE_EXISTS_OVERWRITE,
				filename, NULL);
			if (response != GTK_RESPONSE_OK)
				return;
		} else {
			e_error_run (
				GTK_WINDOW (composer),
				E_ERROR_NO_SAVE_FILE, filename,
				g_strerror (errno_saved));
			return;
		}
	} else
		close (fd);

	if (!gtkhtml_editor_save (editor, filename, TRUE, &error)) {
		e_error_run (
			GTK_WINDOW (composer),
			E_ERROR_NO_SAVE_FILE,
			filename, error->message);
		g_error_free (error);
		return;
	}

	gtkhtml_editor_run_command (GTKHTML_EDITOR (composer), "saved");
	e_composer_autosave_set_saved (composer, FALSE);
}

static void
action_save_as_cb (GtkAction *action,
                   EMsgComposer *composer)
{
	GtkWidget *dialog;
	gchar *filename;
	gint response;

	dialog = gtk_file_chooser_dialog_new (
		_("Save as..."), GTK_WINDOW (composer),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (
		GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_window_set_icon_name (
		GTK_WINDOW (dialog), "mail-message-new");

	response = gtkhtml_editor_file_chooser_dialog_run (
		GTKHTML_EDITOR (composer), dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	gtkhtml_editor_set_filename (GTKHTML_EDITOR (composer), filename);
	g_free (filename);

	gtk_action_activate (ACTION (SAVE));

exit:
	gtk_widget_destroy (dialog);
}

static void
action_save_draft_cb (GtkAction *action,
                      EMsgComposer *composer)
{
	e_msg_composer_save_draft (composer);
}

static void
action_send_cb (GtkAction *action,
                EMsgComposer *composer)
{
	e_msg_composer_send (composer);
}

static void
action_send_options_cb (GtkAction *action,
                        EMsgComposer *composer)
{
	EMEvent *event = em_event_peek ();
	EMEventTargetComposer *target;

	target = em_event_target_new_composer (
		event, composer, EM_EVENT_COMPOSER_SEND_OPTION);
	e_msg_composer_set_send_options (composer, FALSE);

	e_event_emit (
		(EEvent *) event,
		"composer.selectsendoption",
		(EEventTarget *) target);

	if (!composer->priv->send_invoked)
		e_error_run (
			GTK_WINDOW (composer),
			"mail-component:send-options-support", NULL);
}

static void
action_smime_encrypt_cb (GtkToggleAction *action,
                         EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static void
action_smime_sign_cb (GtkToggleAction *action,
                      EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	editor = GTKHTML_EDITOR (composer);
	gtkhtml_editor_set_changed (editor, TRUE);
}

static void
action_view_bcc_cb (GtkToggleAction *action,
                    EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_BCC, active);
}

static void
action_view_cc_cb (GtkToggleAction *action,
                   EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_CC, active);
}

static void
action_view_from_cb (GtkToggleAction *action,
                     EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_FROM, active);
}

static void
action_view_post_to_cb (GtkToggleAction *action,
                        EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_POST_TO, active);
}

static void
action_view_reply_to_cb (GtkToggleAction *action,
                         EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_REPLY_TO, active);
}

static void
action_view_subject_cb (GtkToggleAction *action,
                        EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_SUBJECT, active);
}

static void
action_view_to_cb (GtkToggleAction *action,
                   EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	gboolean active;

	table = e_msg_composer_get_header_table (composer);
	active = gtk_toggle_action_get_active (action);

	e_composer_header_table_set_header_visible (
		table, E_COMPOSER_HEADER_TO, active);
}

static GtkActionEntry entries[] = {

	{ "attach",
          "mail-attachment",
	  N_("_Attachment..."),
	  "<Control>m",
	  N_("Attach a file"),
	  G_CALLBACK (action_attach_cb) },

	{ "close",
	  GTK_STOCK_CLOSE,
	  N_("_Close"),
	  "<Control>w",
	  N_("Close the current file"),
	  G_CALLBACK (action_close_cb) },

	{ "print",
	  GTK_STOCK_PRINT,
	  N_("_Print..."),
	  "<Control>p",
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  N_("Print Pre_view"),
	  "<Shift><Control>p",
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },

	{ "save",
	  GTK_STOCK_SAVE,
	  N_("_Save"),
	  "<Control>s",
	  N_("Save the current file"),
	  G_CALLBACK (action_save_cb) },

	{ "save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("Save _As..."),
	  NULL,
	  N_("Save the current file with a different name"),
	  G_CALLBACK (action_save_as_cb) },

	{ "save-draft",
	  GTK_STOCK_SAVE,
	  N_("Save _Draft"),
	  "<Shift><Control>s",
	  N_("Save as draft"),
	  G_CALLBACK (action_save_draft_cb) },

	{ "send",
	  "mail-send",
	  N_("S_end"),
	  "<Control>Return",
	  N_("Send this message"),
	  G_CALLBACK (action_send_cb) },

	{ "send-options",
	  NULL,
	  N_("_Send Options"),
	  NULL,
	  N_("Insert Send options"),
	  G_CALLBACK (action_send_options_cb) },

	/* Menus */

	{ "charset-menu",
	  NULL,
	  N_("Ch_aracter Encoding"),
	  NULL,
	  NULL,
	  NULL },

	{ "security-menu",
	  NULL,
	  N_("_Security"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkToggleActionEntry toggle_entries[] = {

	{ "pgp-encrypt",
	  NULL,
	  N_("PGP _Encrypt"),
	  NULL,
	  N_("Encrypt this message with PGP"),
	  G_CALLBACK (action_pgp_encrypt_cb),
	  FALSE },

	{ "pgp-sign",
	  NULL,
	  N_("PGP _Sign"),
	  NULL,
	  N_("Sign this message with your PGP key"),
	  G_CALLBACK (action_pgp_sign_cb),
	  FALSE },

	{ "prioritize-message",
	  NULL,
	  N_("_Prioritize Message"),
	  NULL,
	  N_("Set the message priority to high"),
	  NULL,  /* no callback */
	  FALSE },

	{ "request-read-receipt",
	  NULL,
	  N_("R_equest Read Receipt"),
	  NULL,
	  N_("Get delivery notification when your message is read"),
	  NULL,  /* no callback */
	  FALSE },

	{ "smime-encrypt",
	  NULL,
	  N_("S/MIME En_crypt"),
	  NULL,
	  N_("Encrypt this message with your S/MIME Encryption Certificate"),
	  G_CALLBACK (action_smime_encrypt_cb),
	  FALSE },

	{ "smime-sign",
	  NULL,
	  N_("S/MIME Sig_n"),
	  NULL,
	  N_("Sign this message with your S/MIME Signature Certificate"),
	  G_CALLBACK (action_smime_sign_cb),
	  FALSE },

	{ "view-bcc",
	  NULL,
	  N_("_Bcc Field"),
	  NULL,
	  N_("Toggles whether the BCC field is displayed"),
	  G_CALLBACK (action_view_bcc_cb),
	  FALSE },

	{ "view-cc",
	  NULL,
	  N_("_Cc Field"),
	  NULL,
	  N_("Toggles whether the CC field is displayed"),
	  G_CALLBACK (action_view_cc_cb),
	  FALSE },

	{ "view-from",
	  NULL,
	  N_("_From Field"),
	  NULL,
	  N_("Toggles whether the From chooser is displayed"),
	  G_CALLBACK (action_view_from_cb),
	  FALSE },

	{ "view-post-to",
	  NULL,
	  N_("_Post-To Field"),
	  NULL,
	  N_("Toggles whether the Post-To field is displayed"),
	  G_CALLBACK (action_view_post_to_cb),
	  FALSE },

	{ "view-reply-to",
	  NULL,
	  N_("_Reply-To Field"),
	  NULL,
	  N_("Toggles whether the Reply-To field is displayed"),
	  G_CALLBACK (action_view_reply_to_cb),
	  FALSE },

	{ "view-subject",
	  NULL,
	  N_("_Subject Field"),
	  NULL,
	  N_("Toggles whether the Subject field is displayed"),
	  G_CALLBACK (action_view_subject_cb),
	  FALSE },

	{ "view-to",
	  NULL,
	  N_("_To Field"),
	  NULL,
	  N_("Toggles whether the To field is displayed"),
	  G_CALLBACK (action_view_to_cb),
	  FALSE }
};

void
e_composer_actions_init (EMsgComposer *composer)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	gboolean visible;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));

	/* Composer Actions */
	action_group = composer->priv->composer_actions;
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, entries,
		G_N_ELEMENTS (entries), composer);
	gtk_action_group_add_toggle_actions (
		action_group, toggle_entries,
		G_N_ELEMENTS (toggle_entries), composer);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Character Set Actions */
	action_group = composer->priv->charset_actions;
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	e_charset_add_radio_actions (
		action_group, composer->priv->charset,
		G_CALLBACK (action_charset_cb), composer);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Fine Tuning */

	g_object_set (
		G_OBJECT (ACTION (ATTACH)),
		"short-label", _("Attach"), NULL);

#if defined (HAVE_NSS) && defined (SMIME_SUPPORTED)
	visible = TRUE;
#else
	visible = FALSE;
#endif

	gtk_action_set_visible (ACTION (SMIME_ENCRYPT), visible);
	gtk_action_set_visible (ACTION (SMIME_SIGN), visible);
}

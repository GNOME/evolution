/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "e-composer-actions.h"
#include "e-composer-private.h"

#include <errno.h>
#include <fcntl.h>
#include <e-util/e-error.h>
#include <mail/em-event.h>
#include <mail/em-format-html-print.h>
#include <mail/em-composer-utils.h>

#include "misc/e-charset-picker.h"

static void
action_attach_cb (GtkAction *action,
                  EMsgComposer *composer)
{
	EAttachmentView *view;
	EAttachmentStore *store;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	e_attachment_store_run_load_dialog (store, GTK_WINDOW (composer));
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
	GtkWidget *widget;
	const gchar *subject;
	gint response;

	editor = GTKHTML_EDITOR (composer);
	widget = GTK_WIDGET (composer);

	if (!gtkhtml_editor_get_changed (editor) ||
	    e_msg_composer_is_exiting (composer)) {
		gtk_widget_destroy (widget);
		return;
	}

	gdk_window_raise (widget->window);

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
			gtk_widget_hide (widget);
			e_msg_composer_request_close (composer);
			gtk_action_activate (ACTION (SAVE_DRAFT));
			break;

		case GTK_RESPONSE_NO:
			gtk_widget_destroy (widget);
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
	message = e_msg_composer_get_message_print (composer, 1);

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
				g_strerror (errno_saved), NULL);
			return;
		}
	} else
		close (fd);

	if (!gtkhtml_editor_save (editor, filename, TRUE, &error)) {
		e_error_run (
			GTK_WINDOW (composer),
			E_ERROR_NO_SAVE_FILE,
			filename, error->message, NULL);
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
			"mail-composer:send-options-support", NULL);
}

static void
action_new_message_cb (GtkAction *action,
                        EMsgComposer *composer)
{
	em_utils_compose_new_message (NULL);
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
	  "<Shift><Control>s",
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
	  N_("Save as _Draft"),
	  "<Control>s",
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

	{ "new-message",
	  "mail-message-new",
	  N_("New _Message"),
	  "<Control>n",
	  N_("Open New Message window"),
	  G_CALLBACK (action_new_message_cb) },

	/* Menus */

	{ "charset-menu",
	  NULL,
	  N_("Character _Encoding"),
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
	  N_("Re_quest Read Receipt"),
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
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "view-cc",
	  NULL,
	  N_("_Cc Field"),
	  NULL,
	  N_("Toggles whether the CC field is displayed"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "view-from",
	  NULL,
	  N_("_From Field"),
	  NULL,
	  N_("Toggles whether the From chooser is displayed"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "view-reply-to",
	  NULL,
	  N_("_Reply-To Field"),
	  NULL,
	  N_("Toggles whether the Reply-To field is displayed"),
	  NULL,  /* Handled by property bindings */
	  FALSE },
};

void
e_composer_actions_init (EMsgComposer *composer)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	gboolean visible;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	ui_manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (composer));

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
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* Character Set Actions */
	action_group = composer->priv->charset_actions;
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	e_charset_add_radio_actions (
		action_group, composer->priv->charset,
		G_CALLBACK (action_charset_cb), composer);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* Fine Tuning */

	g_object_set (
		ACTION (ATTACH), "short-label", _("Attach"), NULL);

	g_object_set (
		ACTION (SAVE_DRAFT), "short-label", _("Save Draft"), NULL);

#if defined (HAVE_NSS)
	visible = TRUE;
#else
	visible = FALSE;
#endif

	gtk_action_set_visible (ACTION (SMIME_ENCRYPT), visible);
	gtk_action_set_visible (ACTION (SMIME_SIGN), visible);
}

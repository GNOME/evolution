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
#include <e-util/e-alert-dialog.h>

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

	charset = g_object_get_data (G_OBJECT (action), "charset");

	g_free (composer->priv->charset);
	composer->priv->charset = g_strdup (charset);
}

static void
action_close_cb (GtkAction *action,
                 EMsgComposer *composer)
{
	if (e_msg_composer_can_close (composer, TRUE))
		gtk_widget_destroy (GTK_WIDGET (composer));
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

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_msg_composer_print (composer, print_action);
}

static void
action_print_preview_cb (GtkAction *action,
                         EMsgComposer *composer)
{
	GtkPrintOperationAction print_action;

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	e_msg_composer_print (composer, print_action);
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

			response = e_alert_run_dialog_for_args (
				GTK_WINDOW (composer),
				E_ALERT_ASK_FILE_EXISTS_OVERWRITE,
				filename, NULL);
			if (response != GTK_RESPONSE_OK)
				return;
		} else {
			e_alert_run_dialog_for_args (
				GTK_WINDOW (composer),
				E_ALERT_NO_SAVE_FILE, filename,
				g_strerror (errno_saved), NULL);
			return;
		}
	} else
		close (fd);

	if (!gtkhtml_editor_save (editor, filename, TRUE, &error)) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			E_ALERT_NO_SAVE_FILE,
			filename, error->message, NULL);
		g_error_free (error);
		return;
	}

	gtkhtml_editor_run_command (GTKHTML_EDITOR (composer), "saved");
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
action_new_message_cb (GtkAction *action,
                       EMsgComposer *composer)
{
	EMsgComposer *new_composer;
	EShell *shell;

	shell = e_msg_composer_get_shell (composer);

	new_composer = e_msg_composer_new (shell);
	gtk_widget_show (GTK_WIDGET (new_composer));
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

	{ "options-menu",
	  NULL,
	  N_("_Options"),
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
		action_group, "charset-", composer->priv->charset,
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

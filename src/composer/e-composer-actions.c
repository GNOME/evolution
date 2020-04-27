/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include "e-composer-actions.h"
#include "e-composer-private.h"

#include <e-util/e-util.h>

#include <errno.h>
#include <fcntl.h>

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
	if (e_msg_composer_can_close (composer, TRUE)) {
		e_composer_emit_before_destroy (composer);
		gtk_widget_destroy (GTK_WIDGET (composer));
	}
}

static void
action_new_message_composer_created_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	EMsgComposer *composer;
	GError *error = NULL;

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		gtk_widget_show (GTK_WIDGET (composer));
	}
}

static void
action_new_message_cb (GtkAction *action,
                       EMsgComposer *composer)
{
	EShell *shell;

	shell = e_msg_composer_get_shell (composer);

	e_msg_composer_new (shell, action_new_message_composer_created_cb, NULL);
}

static void
composer_set_content_editor_changed (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_changed (cnt_editor, TRUE);

}

static void
action_pgp_encrypt_cb (GtkToggleAction *action,
                       EMsgComposer *composer)
{
	composer_set_content_editor_changed (composer);
}

static void
action_pgp_sign_cb (GtkToggleAction *action,
                    EMsgComposer *composer)
{
	composer_set_content_editor_changed (composer);
}

static void
action_preferences_cb (GtkAction *action,
                       EMsgComposer *composer)
{
	EShell *shell;
	GtkWidget *preferences_window;
	const gchar *page_name = "composer";

	shell = e_msg_composer_get_shell (composer);
	preferences_window = e_shell_get_preferences_window (shell);
	e_preferences_window_setup (E_PREFERENCES_WINDOW (preferences_window));

	gtk_window_set_transient_for (
		GTK_WINDOW (preferences_window),
		GTK_WINDOW (composer));
	gtk_window_set_position (
		GTK_WINDOW (preferences_window),
		GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_present (GTK_WINDOW (preferences_window));

	e_preferences_window_show_page (
		E_PREFERENCES_WINDOW (preferences_window), page_name);
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
action_save_ready_cb (GObject *source_object,
		      GAsyncResult *result,
		      gpointer user_data)
{
	EMsgComposer *composer = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (E_IS_HTML_EDITOR (source_object));

	if (!e_html_editor_save_finish (E_HTML_EDITOR (source_object), result, &error)) {
		e_alert_submit (
			E_ALERT_SINK (composer),
			E_ALERT_NO_SAVE_FILE,
			e_html_editor_get_filename (E_HTML_EDITOR (source_object)), error ? error->message : _("Unknown error"), NULL);
	} else {
		composer_set_content_editor_changed (composer);
	}

	g_object_unref (composer);
	g_clear_error (&error);
}

static void
action_save_cb (GtkAction *action,
                EMsgComposer *composer)
{
	EHTMLEditor *editor;
	const gchar *filename;
	gint fd;

	editor = e_msg_composer_get_editor (composer);
	filename = e_html_editor_get_filename (editor);
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
			e_alert_submit (
				E_ALERT_SINK (composer),
				E_ALERT_NO_SAVE_FILE, filename,
				g_strerror (errno_saved), NULL);
			return;
		}
	} else
		close (fd);

	e_html_editor_save (editor, filename, TRUE, NULL, action_save_ready_cb, g_object_ref (composer));
}

static void
action_save_as_cb (GtkAction *action,
                   EMsgComposer *composer)
{
	EHTMLEditor *editor;
	GtkFileChooserNative *native;
	gchar *filename;

	native = gtk_file_chooser_native_new (
		_("Save as…"), GTK_WINDOW (composer),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (native), FALSE);
	if (GTK_IS_WINDOW (native))
		gtk_window_set_icon_name (GTK_WINDOW (native), "mail-message-new");

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (native));
	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (native));

		editor = e_msg_composer_get_editor (composer);
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (native));
		e_html_editor_set_filename (editor, filename);
		g_free (filename);

		gtk_action_activate (ACTION (SAVE));
	}

	g_object_unref (native);
}

static void
action_save_draft_cb (GtkAction *action,
                      EMsgComposer *composer)
{
	e_msg_composer_save_to_drafts (composer);
}

static void
action_send_cb (GtkAction *action,
                EMsgComposer *composer)
{
	e_msg_composer_send (composer);
}

static void
action_smime_encrypt_cb (GtkToggleAction *action,
                         EMsgComposer *composer)
{
	composer_set_content_editor_changed (composer);
}

static void
action_smime_sign_cb (GtkToggleAction *action,
                      EMsgComposer *composer)
{
	composer_set_content_editor_changed (composer);
}

static void
composer_actions_toolbar_option_toggled_cb (GtkToggleAction *toggle_action,
					    EMsgComposer *composer)
{
	GtkAction *action;

	action = GTK_ACTION (toggle_action);

	/* Show the action only after the first time the option is used */
	if (!gtk_action_get_visible (action) &&
	    gtk_toggle_action_get_active (toggle_action))
		gtk_action_set_visible (action, TRUE);
}

static gboolean
composer_actions_accel_activate_cb (GtkAccelGroup *accel_group,
				    GObject *acceleratable,
				    guint keyval,
				    GdkModifierType modifier,
				    gpointer user_data)
{
	EMsgComposer *composer = user_data;

	if (keyval == GDK_KEY_Return && (modifier & GDK_MODIFIER_MASK) == GDK_CONTROL_MASK &&
	    !e_util_prompt_user (GTK_WINDOW (composer), "org.gnome.evolution.mail",
		"prompt-on-accel-send", "mail-composer:prompt-accel-send", NULL)) {
		return TRUE;
	}
	return FALSE;
}

static GtkActionEntry entries[] = {

	{ "attach",
          "mail-attachment",
	  N_("_Attachment…"),
	  "<Control>m",
	  N_("Attach a file"),
	  G_CALLBACK (action_attach_cb) },

	{ "close",
	  "window-close",
	  N_("_Close"),
	  "<Control>w",
	  N_("Close the current file"),
	  G_CALLBACK (action_close_cb) },

	{ "new-message",
	  "mail-message-new",
	  N_("New _Message"),
	  "<Control>n",
	  N_("Open New Message window"),
	  G_CALLBACK (action_new_message_cb) },

	{ "preferences",
	  "preferences-system",
	  N_("_Preferences"),
	  NULL,
	  N_("Configure Evolution"),
	  G_CALLBACK (action_preferences_cb) },

	{ "save",
	  "document-save",
	  N_("_Save"),
	  "<Shift><Control>s",
	  N_("Save the current file"),
	  G_CALLBACK (action_save_cb) },

	{ "save-as",
	  "document-save-as",
	  N_("Save _As…"),
	  NULL,
	  N_("Save the current file with a different name"),
	  G_CALLBACK (action_save_as_cb) },

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

static GtkActionEntry async_entries[] = {

	{ "print",
	  "document-print",
	  N_("_Print…"),
	  "<Control>p",
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  "document-print-preview",
	  N_("Print Pre_view"),
	  "<Shift><Control>p",
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },

	{ "save-draft",
	  "document-save",
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

	{ "picture-gallery",
	  "emblem-photos",
	  N_("_Picture Gallery"),
	  NULL,
	  N_("Show a collection of pictures that you can drag to your message"),
	  NULL,  /* no callback */
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

	{ "toolbar-pgp-encrypt",
	  "security-high",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  FALSE },

	{ "toolbar-pgp-sign",
	  "stock_signature",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  FALSE },

	{ "toolbar-prioritize-message",
	  "emblem-important",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  FALSE },

	{ "toolbar-request-read-receipt",
	  "mail-forward",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  FALSE },

	{ "toolbar-smime-encrypt",
	  "security-high",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  FALSE },

	{ "toolbar-smime-sign",
	  "stock_signature",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
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

	{ "view-from-override",
	  NULL,
	  N_("_From Override Field"),
	  NULL,
	  N_("Toggles whether the From override field to change name or email address is displayed"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "view-reply-to",
	  NULL,
	  N_("_Reply-To Field"),
	  NULL,
	  N_("Toggles whether the Reply-To field is displayed"),
	  NULL,  /* Handled by property bindings */
	  FALSE },

	{ "visually-wrap-long-lines",
	  NULL,
	  N_("Visually _Wrap Long Lines"),
	  NULL,
	  N_("Whether to visually wrap long lines of text to avoid horizontal scrolling"),
	  NULL,
	  FALSE }
};

void
e_composer_actions_init (EMsgComposer *composer)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean visible;
	GIcon *gcr_gnupg_icon;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

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

	/* Asynchronous Actions */
	action_group = composer->priv->async_actions;
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, async_entries,
		G_N_ELEMENTS (async_entries), composer);
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
		ACTION (PICTURE_GALLERY), "is-important", TRUE, NULL);

	g_object_set (
		ACTION (SAVE_DRAFT), "short-label", _("Save Draft"), NULL);

	#define init_toolbar_option(x, always_visible)	\
		gtk_action_set_visible (ACTION (TOOLBAR_ ## x), always_visible); \
		e_binding_bind_property ( \
			ACTION (x), "active", \
			ACTION (TOOLBAR_ ## x), "active", \
			G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL); \
		e_binding_bind_property ( \
			ACTION (x), "label", \
			ACTION (TOOLBAR_ ## x), "label", \
			G_BINDING_SYNC_CREATE); \
		e_binding_bind_property ( \
			ACTION (x), "tooltip", \
			ACTION (TOOLBAR_ ## x), "tooltip", \
			G_BINDING_SYNC_CREATE); \
		e_binding_bind_property ( \
			ACTION (x), "sensitive", \
			ACTION (TOOLBAR_ ## x), "sensitive", \
			G_BINDING_SYNC_CREATE); \
		g_signal_connect (ACTION (TOOLBAR_ ## x), "toggled", \
			G_CALLBACK (composer_actions_toolbar_option_toggled_cb), composer);

	init_toolbar_option (PGP_SIGN, FALSE);
	init_toolbar_option (PGP_ENCRYPT, FALSE);
	init_toolbar_option (PRIORITIZE_MESSAGE, TRUE);
	init_toolbar_option (REQUEST_READ_RECEIPT, TRUE);
	init_toolbar_option (SMIME_SIGN, FALSE);
	init_toolbar_option (SMIME_ENCRYPT, FALSE);

	#undef init_toolbar_option

	/* Borrow a GnuPG icon from gcr to distinguish between GPG and S/MIME Sign/Encrypt actions */
	gcr_gnupg_icon = g_themed_icon_new ("gcr-gnupg");
	if (gcr_gnupg_icon) {
		GIcon *temp_icon;
		GIcon *action_icon;
		GEmblem *emblem;
		GtkAction *action;

		emblem = g_emblem_new (gcr_gnupg_icon);

		action = ACTION (TOOLBAR_PGP_SIGN);
		action_icon = g_themed_icon_new (gtk_action_get_icon_name (action));
		temp_icon = g_emblemed_icon_new (action_icon, emblem);
		g_object_unref (action_icon);

		gtk_action_set_gicon (action, temp_icon);
		g_object_unref (temp_icon);

		action = ACTION (TOOLBAR_PGP_ENCRYPT);
		action_icon = g_themed_icon_new (gtk_action_get_icon_name (action));
		temp_icon = g_emblemed_icon_new (action_icon, emblem);
		g_object_unref (action_icon);

		gtk_action_set_gicon (action, temp_icon);
		g_object_unref (temp_icon);

		g_object_unref (emblem);
		g_object_unref (gcr_gnupg_icon);
	}

	e_binding_bind_property (
		cnt_editor, "html-mode",
		ACTION (PICTURE_GALLERY), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "editable",
		e_html_editor_get_action (editor, "edit-menu"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "editable",
		e_html_editor_get_action (editor, "format-menu"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "editable",
		e_html_editor_get_action (editor, "insert-menu"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "editable",
		e_html_editor_get_action (editor, "options-menu"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "editable",
		e_html_editor_get_action (editor, "picture-gallery"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cnt_editor, "visually-wrap-long-lines",
		e_html_editor_get_action (editor, "visually-wrap-long-lines"), "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

#if defined (ENABLE_SMIME)
	visible = TRUE;
#else
	visible = FALSE;
#endif

	gtk_action_set_visible (ACTION (SMIME_ENCRYPT), visible);
	gtk_action_set_visible (ACTION (SMIME_SIGN), visible);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	g_signal_connect (accel_group, "accel-activate",
		G_CALLBACK (composer_actions_accel_activate_cb), composer);
}

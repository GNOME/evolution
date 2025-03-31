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
action_attach_cb (EUIAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EAttachmentView *view;
	EAttachmentStore *store;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	e_attachment_store_run_load_dialog (store, GTK_WINDOW (composer));
}

static void
action_charset_change_set_state_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	e_ui_action_set_state (action, parameter);

	g_free (composer->priv->charset);
	composer->priv->charset = g_strdup (g_variant_get_string (parameter, NULL));
}

static void
action_close_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

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
action_new_message_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EShell *shell;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

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
action_preferences_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EShell *shell;
	GtkWidget *preferences_window;
	const gchar *page_name = "composer";

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

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
action_print_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EMsgComposer *composer = user_data;
	GtkPrintOperationAction print_action;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_msg_composer_print (composer, print_action);
}

static void
action_print_preview_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMsgComposer *composer = user_data;
	GtkPrintOperationAction print_action;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

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
action_save_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EHTMLEditor *editor;
	const gchar *filename;
	gint fd;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	filename = e_html_editor_get_filename (editor);
	if (filename == NULL) {
		g_action_activate (G_ACTION (ACTION (SAVE_AS)), NULL);
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
action_save_as_cb (EUIAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EHTMLEditor *editor;
	GtkFileChooserNative *native;
	gchar *filename;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

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

		g_action_activate (G_ACTION (ACTION (SAVE)), NULL);
	}

	g_object_unref (native);
}

static void
action_save_draft_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	e_msg_composer_save_to_drafts (composer);
}

static void
action_send_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EMsgComposer *composer = user_data;
	EUIManager *ui_manager;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	ui_manager = e_html_editor_get_ui_manager (e_msg_composer_get_editor (composer));

	if (e_ui_manager_get_in_accel_activation (ui_manager) &&
	    !e_util_prompt_user (GTK_WINDOW (composer), "org.gnome.evolution.mail", "prompt-on-accel-send", "mail-composer:prompt-accel-send", NULL))
		return;

	e_msg_composer_send (composer);
}

static void
e_composer_set_action_state_with_changed_cb (EUIAction *action,
					     GVariant *parameter,
					     gpointer user_data)
{
	EMsgComposer *composer = user_data;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	e_ui_action_set_state (action, parameter);
	composer_set_content_editor_changed (composer);
}

static void
composer_actions_toolbar_option_notify_active_cb (EUIAction *action,
						  GParamSpec *param,
						  gpointer user_data)
{
	/* Show the action only after the first time the option is used */
	if (!e_ui_action_get_visible (action) &&
	    e_ui_action_get_active (action))
		e_ui_action_set_visible (action, TRUE);
}

static gboolean
eca_transform_mode_html_to_boolean_cb (GBinding *binding,
				       const GValue *source_value,
				       GValue *target_value,
				       gpointer not_used)
{
	g_value_set_boolean (target_value, g_value_get_enum (source_value) == E_CONTENT_EDITOR_MODE_HTML);

	return TRUE;
}

static gboolean
eca_mode_to_bool_hide_in_markdown_cb (GBinding *binding,
				      const GValue *from_value,
				      GValue *to_value,
				      gpointer user_data)
{
	EContentEditorMode mode;

	mode = g_value_get_enum (from_value);

	g_value_set_boolean (to_value,
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN &&
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT &&
		mode != E_CONTENT_EDITOR_MODE_MARKDOWN_HTML);

	return TRUE;
}

void
e_composer_actions_init (EMsgComposer *composer)
{
	static const EUIActionEntry entries[] = {

		{ "attach",
		  "mail-attachment",
		  N_("_Attachment…"),
		  "<Control>m",
		  N_("Attach a file"),
		  action_attach_cb, NULL, NULL, NULL },

		{ "close",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close the current file"),
		  action_close_cb, NULL, NULL, NULL },

		{ "new-message",
		  "mail-message-new",
		  N_("New _Message"),
		  "<Control>n",
		  N_("Open New Message window"),
		  action_new_message_cb, NULL, NULL, NULL },

		{ "preferences",
		  "preferences-system",
		  N_("_Preferences"),
		  NULL,
		  N_("Configure Evolution"),
		  action_preferences_cb, NULL, NULL, NULL },

		{ "save",
		  "document-save",
		  N_("_Save"),
		  "<Shift><Control>s",
		  N_("Save the current file"),
		  action_save_cb, NULL, NULL, NULL },

		{ "save-as",
		  "document-save-as",
		  N_("Save _As…"),
		  NULL,
		  N_("Save the current file with a different name"),
		  action_save_as_cb, NULL, NULL, NULL },

		/* Menus */

		{ "EMsgComposer::charset-menu",
		  NULL,
		  N_("Character _Encoding"),
		  NULL,
		  NULL,
		  NULL, "s", "''", action_charset_change_set_state_cb },

		{ "EMsgComposer::menu-button",
		  NULL,
		  N_("Menu"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL },

		{ "options-menu",
		  NULL,
		  N_("Option_s"),
		  NULL,
		  NULL,
		  NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry async_entries[] = {

		{ "print",
		  "document-print",
		  N_("_Print…"),
		  "<Control>p",
		  NULL,
		  action_print_cb, NULL, NULL, NULL },

		{ "print-preview",
		  "document-print-preview",
		  N_("Print Pre_view"),
		  "<Shift><Control>p",
		  NULL,
		  action_print_preview_cb, NULL, NULL, NULL },

		{ "save-draft",
		  "document-save",
		  N_("Save as _Draft"),
		  "<Control>s",
		  N_("Save as draft"),
		  action_save_draft_cb, NULL, NULL, NULL },

		{ "send",
		  "mail-send",
		  N_("S_end"),
		  "<Control>Return",
		  N_("Send this message"),
		  action_send_cb, NULL, NULL, NULL },
	};

	static const EUIActionEntry toggle_entries[] = {

		{ "toolbar-show-main",
		  NULL,
		  N_("_Main toolbar"),
		  NULL,
		  N_("Main toolbar"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-show-edit",
		  NULL,
		  N_("Edit _toolbar"),
		  NULL,
		  N_("Edit toolbar"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "pgp-encrypt",
		  NULL,
		  N_("PGP _Encrypt"),
		  NULL,
		  N_("Encrypt this message with PGP"),
		  NULL, NULL, "false", e_composer_set_action_state_with_changed_cb },

		{ "pgp-sign",
		  NULL,
		  N_("PGP _Sign"),
		  NULL,
		  N_("Sign this message with your PGP key"),
		  NULL, NULL, "false", e_composer_set_action_state_with_changed_cb },

		{ "picture-gallery",
		  "emblem-photos",
		  N_("_Picture Gallery"),
		  NULL,
		  N_("Show a collection of pictures that you can drag to your message"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "prioritize-message",
		  "mail-mark-important",
		  N_("_Prioritize Message"),
		  NULL,
		  N_("Set the message priority to high"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "request-read-receipt",
		  "preferences-system-notifications",
		  N_("Re_quest Read Receipt"),
		  NULL,
		  N_("Get delivery notification when your message is read"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "delivery-status-notification",
		  NULL,
		  N_("Request _Delivery Status Notification"),
		  NULL,
		  N_("Get delivery status notification for the message"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "smime-encrypt",
		  NULL,
		  N_("S/MIME En_crypt"),
		  NULL,
		  N_("Encrypt this message with S/MIME"),
		  NULL, NULL, "false", e_composer_set_action_state_with_changed_cb },

		{ "smime-sign",
		  NULL,
		  N_("S/MIME Sig_n"),
		  NULL,
		  N_("Sign this message with your S/MIME Signature Certificate"),
		  NULL, NULL, "false", e_composer_set_action_state_with_changed_cb },

		{ "toolbar-pgp-encrypt",
		  "gicon::EMsgComposer::pgp-encrypt",
		  N_("PGP _Encrypt"),
		  NULL,
		  N_("Encrypt this message with PGP"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-pgp-sign",
		  "gicon::EMsgComposer::pgp-sign",
		  N_("PGP _Sign"),
		  NULL,
		  N_("Sign this message with your PGP key"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-prioritize-message",
		  "emblem-important",
		  N_("_Prioritize Message"),
		  NULL,
		  N_("Set the message priority to high"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-request-read-receipt",
		  "mail-forward",
		  N_("Re_quest Read Receipt"),
		  NULL,
		  N_("Get delivery notification when your message is read"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-smime-encrypt",
		  "security-high",
		  N_("S/MIME En_crypt"),
		  NULL,
		  N_("Encrypt this message with S/MIME"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "toolbar-smime-sign",
		  "stock_signature",
		  N_("S/MIME Sig_n"),
		  NULL,
		  N_("Sign this message with your S/MIME Signature Certificate"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-bcc",
		  NULL,
		  N_("_Bcc Field"),
		  NULL,
		  N_("Toggles whether the BCC field is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-cc",
		  NULL,
		  N_("_Cc Field"),
		  NULL,
		  N_("Toggles whether the CC field is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-from-override",
		  NULL,
		  N_("_From Override Field"),
		  NULL,
		  N_("Toggles whether the From override field to change name or email address is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-mail-followup-to",
		  NULL,
		  N_("Mail-Follow_up-To Field"),
		  NULL,
		  N_("Toggles whether the Mail-Followup-To field is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-mail-reply-to",
		  NULL,
		  N_("Mail-R_eply-To Field"),
		  NULL,
		  N_("Toggles whether the Mail-Reply-To field is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-reply-to",
		  NULL,
		  N_("_Reply-To Field"),
		  NULL,
		  N_("Toggles whether the Reply-To field is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "visually-wrap-long-lines",
		  NULL,
		  N_("Visually _Wrap Long Lines"),
		  NULL,
		  N_("Whether to visually wrap long lines of text to avoid horizontal scrolling"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state }
	};

	EUIManager *ui_manager;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean visible;
	GSettings *settings;
	EUIAction *action;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);
	ui_manager = e_html_editor_get_ui_manager (editor);

	/* Composer Actions */
	e_ui_manager_add_actions (ui_manager, "composer", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), composer);
	e_ui_manager_add_actions (ui_manager, "composer", GETTEXT_PACKAGE,
		toggle_entries, G_N_ELEMENTS (toggle_entries), composer);

	/* Asynchronous Actions */
	e_ui_manager_add_actions (ui_manager, "async", GETTEXT_PACKAGE,
		async_entries, G_N_ELEMENTS (async_entries), composer);

	action = e_ui_manager_get_action (ui_manager, "close");
	e_ui_action_add_secondary_accel (action, "Escape");

	action = e_ui_manager_get_action (ui_manager, "send");
	e_ui_action_add_secondary_accel (action, "Send");

	#define init_toolbar_option(x, always_visible)	\
		e_ui_action_set_visible (ACTION (TOOLBAR_ ## x), always_visible); \
		e_binding_bind_property ( \
			ACTION (x), "active", \
			ACTION (TOOLBAR_ ## x), "active", \
			G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL); \
		e_binding_bind_property ( \
			ACTION (x), "tooltip", \
			ACTION (TOOLBAR_ ## x), "tooltip", \
			G_BINDING_SYNC_CREATE); \
		e_binding_bind_property ( \
			ACTION (x), "sensitive", \
			ACTION (TOOLBAR_ ## x), "sensitive", \
			G_BINDING_SYNC_CREATE); \
		g_signal_connect (ACTION (TOOLBAR_ ## x), "notify::active", \
			G_CALLBACK (composer_actions_toolbar_option_notify_active_cb), composer);

	init_toolbar_option (PGP_SIGN, FALSE);
	init_toolbar_option (PGP_ENCRYPT, FALSE);
	init_toolbar_option (PRIORITIZE_MESSAGE, TRUE);
	init_toolbar_option (REQUEST_READ_RECEIPT, TRUE);
	init_toolbar_option (SMIME_SIGN, FALSE);
	init_toolbar_option (SMIME_ENCRYPT, FALSE);

	#undef init_toolbar_option

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	action = ACTION (TOOLBAR_SHOW_MAIN);
	g_settings_bind (
		settings, "composer-show-main-toolbar",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = ACTION (TOOLBAR_SHOW_EDIT);
	g_settings_bind (
		settings, "composer-show-edit-toolbar",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	e_binding_bind_property_full (
		editor, "mode",
		ACTION (PICTURE_GALLERY), "sensitive",
		G_BINDING_SYNC_CREATE,
		eca_transform_mode_html_to_boolean_cb,
		NULL, NULL, NULL);

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

	e_binding_bind_property_full (
		editor, "mode",
		e_html_editor_get_action (editor, "visually-wrap-long-lines"), "visible",
		G_BINDING_SYNC_CREATE,
		eca_mode_to_bool_hide_in_markdown_cb,
		NULL, NULL, NULL);

#if defined (ENABLE_SMIME)
	visible = TRUE;
#else
	visible = FALSE;
#endif

	e_ui_action_set_visible (ACTION (SMIME_ENCRYPT), visible);
	e_ui_action_set_visible (ACTION (SMIME_SIGN), visible);
}

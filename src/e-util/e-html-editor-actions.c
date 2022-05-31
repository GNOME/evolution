/* e-html-editor-actions.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <enchant.h>

#include "e-html-editor.h"
#include "e-html-editor-private.h"
#include "e-html-editor-actions.h"
#include "e-emoticon-action.h"
#include "e-emoticon-chooser.h"
#include "e-gtkemojichooser.h"
#include "e-image-chooser-dialog.h"
#include "e-spell-checker.h"
#include "e-misc-utils.h"
#include "e-selection.h"
#include "e-content-editor.h"

static gboolean
e_html_editor_action_can_run (GtkWidget *widget)
{
	GtkWidget *toplevel, *focused;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	/* The action can be run if the widget is focused... */
	if (gtk_widget_has_focus (widget))
		return TRUE;

	toplevel = gtk_widget_get_toplevel (widget);
	if (!toplevel || !gtk_widget_is_toplevel (toplevel) || !GTK_IS_WINDOW (toplevel))
		return TRUE;

	focused = gtk_window_get_focus (GTK_WINDOW (toplevel));

	/* ..., or if there is no other widget focused. Eventually the window
	   can have set the widget as focused, but the widget doesn't have
	   the flag saet, because the window itself is in the background,
	   like during the unit tests of the HTML editor.*/
	return !focused || focused == widget;
}

static void
insert_html_file_ready_cb (GFile *file,
                           GAsyncResult *result,
                           EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	gchar *contents = NULL;
	gsize length;
	GError *error = NULL;

	g_file_load_contents_finish (
		file, result, &contents, &length, NULL, &error);
	if (error != NULL) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
			0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, _("Failed to insert HTML file."));
		gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_clear_error (&error);
		g_object_unref (editor);
		return;
	}

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_content (
		cnt_editor, contents, E_CONTENT_EDITOR_INSERT_TEXT_HTML);

	g_free (contents);

	g_object_unref (editor);
}

static void
insert_text_file_ready_cb (GFile *file,
                           GAsyncResult *result,
                           EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	gchar *contents;
	gsize length;
	GError *error = NULL;

	g_file_load_contents_finish (
		file, result, &contents, &length, NULL, &error);
	if (error != NULL) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))),
			0, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, _("Failed to insert text file."));
		gtk_message_dialog_format_secondary_text (
			GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_clear_error (&error);
		g_object_unref (editor);
		return;
	}

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_content (
		cnt_editor, contents, E_CONTENT_EDITOR_INSERT_CONVERT | E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);

	g_free (contents);

	g_object_unref (editor);
}

/*****************************************************************************
 * Action Callbacks
 *****************************************************************************/

static void
action_copy_link_cb (GtkAction *action,
		     EHTMLEditor *editor)
{
	GtkClipboard *clipboard;

	if (!editor->priv->context_hover_uri)
		return;

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, editor->priv->context_hover_uri, -1);
	gtk_clipboard_store (clipboard);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, editor->priv->context_hover_uri, -1);
	gtk_clipboard_store (clipboard);
}

static void
action_open_link_cb (GtkAction *action,
		     EHTMLEditor *editor)
{
	gpointer parent;

	if (!editor->priv->context_hover_uri)
		return;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_show_uri (parent, editor->priv->context_hover_uri);
}

static void
action_context_delete_cell_contents_cb (GtkAction *action,
					EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_cell_contents (cnt_editor);
}

static void
action_context_delete_column_cb (GtkAction *action,
                                 EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_column (cnt_editor);
}

static void
action_context_delete_row_cb (GtkAction *action,
                              EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_row (cnt_editor);
}

static void
action_context_delete_table_cb (GtkAction *action,
                                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_table (cnt_editor);
}

static void
action_context_delete_hrule_cb (GtkAction *action,
				EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_h_rule (cnt_editor);
}

static void
action_context_delete_image_cb (GtkAction *action,
				EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_image (cnt_editor);
}

static void
action_context_insert_column_after_cb (GtkAction *action,
                                       EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_column_after (cnt_editor);
}

static void
action_context_insert_column_before_cb (GtkAction *action,
                                        EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_column_before (cnt_editor);
}

static void
action_context_insert_row_above_cb (GtkAction *action,
                                    EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_row_above (cnt_editor);
}

static void
action_context_insert_row_below_cb (GtkAction *action,
                                    EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_row_below (cnt_editor);
}

static void
action_context_remove_link_cb (GtkAction *action,
                               EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_selection_unlink (cnt_editor);
}

static void
action_context_spell_add_cb (GtkAction *action,
                             EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	gchar *word;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	word = e_content_editor_get_caret_word (cnt_editor);
	if (word && *word)
		e_spell_checker_learn_word (spell_checker, word);
	g_free (word);
	g_clear_object (&spell_checker);
}

static void
action_context_spell_ignore_cb (GtkAction *action,
                                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	gchar *word;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	word = e_content_editor_get_caret_word (cnt_editor);
	if (word && *word)
		e_spell_checker_ignore_word (spell_checker, word);
	g_free (word);
	g_clear_object (&spell_checker);
}

static void
action_indent_cb (GtkAction *action,
                  EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_selection_indent (cnt_editor);
}

static void
emoji_chooser_emoji_picked_cb (EHTMLEditor *editor,
			       const gchar *emoji_text)
{
	if (emoji_text) {
		EContentEditor *cnt_editor;

		cnt_editor = e_html_editor_get_content_editor (editor);

		e_content_editor_insert_content (cnt_editor, emoji_text,
			E_CONTENT_EDITOR_INSERT_CONVERT |
			E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);
	}
}

static void
action_insert_emoji_cb (GtkAction *action,
			EHTMLEditor *editor)
{
	if (!editor->priv->emoji_chooser) {
		GtkWidget *popover;

		popover = e_gtk_emoji_chooser_new ();

		gtk_popover_set_relative_to (GTK_POPOVER (popover), GTK_WIDGET (editor));
		gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
		gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);

		g_signal_connect_object (popover, "emoji-picked",
			G_CALLBACK (emoji_chooser_emoji_picked_cb), editor, G_CONNECT_SWAPPED);

		editor->priv->emoji_chooser = popover;
	}

	gtk_popover_popup (GTK_POPOVER (editor->priv->emoji_chooser));
}

static void
action_insert_emoticon_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	EEmoticon *emoticon;

	emoticon = e_emoticon_chooser_get_current_emoticon (E_EMOTICON_CHOOSER (action));
	g_return_if_fail (emoticon != NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_emoticon (cnt_editor, emoticon);
}

static void
action_insert_html_file_cb (GtkToggleAction *action,
                            EHTMLEditor *editor)
{
	GtkFileChooserNative *native;
	GtkFileFilter *filter;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));

	native = gtk_file_chooser_native_new (
		_("Insert HTML File"), GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("HTML file"));
	gtk_file_filter_add_mime_type (filter, "text/html");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (native));

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		GFile *file;

		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (native));
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

		/* XXX Need a way to cancel this. */
		g_file_load_contents_async (
			file, NULL, (GAsyncReadyCallback)
			insert_html_file_ready_cb,
			g_object_ref (editor));

		g_object_unref (file);
	}

	g_object_unref (native);
}

static void
action_insert_image_cb (GtkAction *action,
                        EHTMLEditor *editor)
{
	GtkWidget *dialog = NULL;
	GtkFileChooserNative *native = NULL;
	GtkWidget *toplevel;
	gint response;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	if (e_util_is_running_flatpak ()) {
		GtkFileFilter *file_filter;

		native = gtk_file_chooser_native_new (
			C_("dialog-title", "Insert Image"),
			GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("_Open"), _("_Cancel"));

		file_filter = gtk_file_filter_new ();
		gtk_file_filter_add_pixbuf_formats (file_filter);
		gtk_file_filter_set_name (file_filter, _("Image files"));
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), file_filter);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (native), file_filter);

		file_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (file_filter, _("All files"));
		gtk_file_filter_add_pattern (file_filter, "*");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), file_filter);
	} else {
		GSList *filters, *link;

		dialog = e_image_chooser_dialog_new (C_("dialog-title", "Insert Image"), GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL);

		filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (dialog));

		for (link = filters; link; link = g_slist_next (link)) {
			GtkFileFilter *file_filter = link->data;

			if (g_strcmp0 (gtk_file_filter_get_name (file_filter), _("Image files")) == 0) {
				gtk_file_filter_add_mime_type (file_filter, "image/*");
				break;
			}
		}

		g_slist_free (filters);
	}

	if (dialog)
		response = gtk_dialog_run (GTK_DIALOG (dialog));
	else
		response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

	if (response == GTK_RESPONSE_ACCEPT) {
		EContentEditor *cnt_editor;
		gchar *uri;

		if (dialog)
			uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		else
			uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (native));

		cnt_editor = e_html_editor_get_content_editor (editor);
		e_content_editor_insert_image (cnt_editor, uri);

		g_free (uri);
	}

	if (dialog)
		gtk_widget_destroy (dialog);
	else
		g_object_unref (native);
}

static void
action_insert_link_cb (GtkAction *action,
                       EHTMLEditor *editor)
{
	if (editor->priv->link_dialog == NULL)
		editor->priv->link_dialog =
			e_html_editor_link_dialog_new (editor);

	gtk_window_present (GTK_WINDOW (editor->priv->link_dialog));
}

static void
action_insert_rule_cb (GtkAction *action,
                       EHTMLEditor *editor)
{
	if (editor->priv->hrule_dialog == NULL)
		editor->priv->hrule_dialog =
			e_html_editor_hrule_dialog_new (editor);

	gtk_window_present (GTK_WINDOW (editor->priv->hrule_dialog));
}

static void
action_insert_table_cb (GtkAction *action,
                        EHTMLEditor *editor)
{
	if (editor->priv->table_dialog == NULL)
		editor->priv->table_dialog =
			e_html_editor_table_dialog_new (editor);

	gtk_window_present (GTK_WINDOW (editor->priv->table_dialog));
}

static void
action_insert_text_file_cb (GtkAction *action,
                            EHTMLEditor *editor)
{
	GtkFileChooserNative *native;
	GtkWidget *toplevel;
	GtkFileFilter *filter;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));

	native = gtk_file_chooser_native_new (
		_("Insert text file"), GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Text file"));
	gtk_file_filter_add_mime_type (filter, "text/plain");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (native));

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		GFile *file;

		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (native));
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

		/* XXX Need a way to cancel this. */
		g_file_load_contents_async (
			file, NULL, (GAsyncReadyCallback)
			insert_text_file_ready_cb,
			g_object_ref (editor));

		g_object_unref (file);
	}

	g_object_unref (native);
}

static gboolean
editor_actions_add_to_recent_languages (EHTMLEditor *editor,
					const gchar *language_code);

static void
action_language_cb (GtkToggleAction *toggle_action,
                    EHTMLEditor *editor)
{
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	const gchar *language_code;
	GtkAction *add_action;
	gchar *action_name;
	gboolean active;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	language_code = gtk_action_get_name (GTK_ACTION (toggle_action));

	active = gtk_toggle_action_get_active (toggle_action);
	e_spell_checker_set_language_active (spell_checker, language_code, active);
	g_clear_object (&spell_checker);

	/* Update "Add Word To" context menu item visibility. */
	action_name = g_strdup_printf ("context-spell-add-%s", language_code);
	add_action = e_html_editor_get_action (editor, action_name);
	gtk_action_set_visible (add_action, active);
	g_free (action_name);

	e_html_editor_update_spell_actions (editor);

	g_signal_emit_by_name (editor, "spell-languages-changed");

	if (active) {
		GSettings *settings;
		GPtrArray *array;
		gchar **strv;
		gint ii, max_items;

		gtk_ui_manager_remove_ui (editor->priv->manager, editor->priv->recent_spell_languages_merge_id);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		strv = g_settings_get_strv (settings, "composer-spell-languages-recently-used");
		max_items = g_settings_get_int (settings, "composer-spell-languages-max-recently-used");
		if (max_items < 5)
			max_items = 5;

		array = g_ptr_array_sized_new (max_items + 1);
		g_ptr_array_add (array, (gpointer) language_code);

		editor_actions_add_to_recent_languages (editor, language_code);

		for (ii = 0; strv && strv[ii] && array->len < max_items; ii++) {
			if (g_strcmp0 (language_code, strv[ii]) != 0) {
				g_ptr_array_add (array, strv[ii]);
				editor_actions_add_to_recent_languages (editor, strv[ii]);
			}
		}

		g_ptr_array_add (array, NULL);

		g_settings_set_strv (settings, "composer-spell-languages-recently-used", (const gchar * const *) array->pdata);

		g_object_unref (settings);
		g_ptr_array_free (array, TRUE);
		g_strfreev (strv);
	}
}

static gboolean
update_mode_combobox (gpointer data)
{
	GWeakRef *weak_ref = data;
	EHTMLEditor *editor;
	EContentEditorMode mode;
	GtkAction *action;

	editor = g_weak_ref_get (weak_ref);
	if (!editor)
		return FALSE;

	mode = e_html_editor_get_mode (editor);

	action = gtk_action_group_get_action (editor->priv->core_editor_actions, "mode-html");
	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), mode);

	g_object_unref (editor);

	return FALSE;
}

static void
html_editor_actions_notify_mode_cb (EHTMLEditor *editor,
				    GParamSpec *param,
				    gpointer user_data)
{
	GtkActionGroup *action_group;
	GtkWidget *style_combo_box;
	gboolean is_html;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	is_html = e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML;

	g_object_set (G_OBJECT (editor->priv->html_actions), "sensitive", is_html, NULL);

	/* This must be done from idle callback, because apparently we can change
	 * current value in callback of current value change */
	g_idle_add_full (G_PRIORITY_HIGH_IDLE, update_mode_combobox, e_weak_ref_new (editor), (GDestroyNotify) e_weak_ref_free);

	action_group = editor->priv->html_actions;
	gtk_action_group_set_visible (action_group, is_html);

	action_group = editor->priv->html_context_actions;
	gtk_action_group_set_visible (action_group, is_html);

	gtk_widget_set_sensitive (editor->priv->fg_color_combo_box, is_html);
	gtk_widget_set_sensitive (editor->priv->bg_color_combo_box, is_html);

	if (is_html && gtk_widget_get_visible (editor->priv->edit_toolbar)) {
		gtk_widget_show (editor->priv->html_toolbar);
	} else {
		gtk_widget_hide (editor->priv->html_toolbar);
	}

	/* Certain paragraph styles are HTML-only. */
	gtk_action_set_sensitive (ACTION (STYLE_H1), is_html);
	gtk_action_set_visible (ACTION (STYLE_H1), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_H2), is_html);
	gtk_action_set_visible (ACTION (STYLE_H2), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_H3), is_html);
	gtk_action_set_visible (ACTION (STYLE_H3), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_H4), is_html);
	gtk_action_set_visible (ACTION (STYLE_H4), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_H5), is_html);
	gtk_action_set_visible (ACTION (STYLE_H5), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_H6), is_html);
	gtk_action_set_visible (ACTION (STYLE_H6), is_html);
	gtk_action_set_sensitive (ACTION (STYLE_ADDRESS), is_html);
	gtk_action_set_visible (ACTION (STYLE_ADDRESS), is_html);

	/* Hide them from the action combo box as well */
	style_combo_box = e_html_editor_get_style_combo_box (editor);
	e_action_combo_box_update_model (E_ACTION_COMBO_BOX (style_combo_box));
}

static void
action_mode_cb (GtkRadioAction *action,
		GtkRadioAction *current,
		EHTMLEditor *editor)
{
	/* Nothing to do here, wait for notification of
	   a property change from the EContentEditor */
}

static void
clipboard_text_received_for_paste_as_text (GtkClipboard *clipboard,
                                           const gchar *text,
                                           EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	if (!text || !*text)
		return;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_content (
		cnt_editor,
		text,
		E_CONTENT_EDITOR_INSERT_CONVERT |
		E_CONTENT_EDITOR_INSERT_TEXT_PLAIN |
		(editor->priv->paste_plain_prefer_pre ? E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE : 0));
}

static void
action_paste_as_text_cb (GtkAction *action,
                         EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	GtkClipboard *clipboard;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (!gtk_widget_has_focus (GTK_WIDGET (cnt_editor)))
		gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received_for_paste_as_text,
		editor);
}

static void
paste_quote_text (EHTMLEditor *editor,
		  const gchar *text,
		  gboolean is_html)
{
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (text != NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_content (
		cnt_editor,
		text,
		E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT |
		(is_html ? E_CONTENT_EDITOR_INSERT_TEXT_HTML : E_CONTENT_EDITOR_INSERT_TEXT_PLAIN) |
		((!is_html && editor->priv->paste_plain_prefer_pre) ? E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE : 0));
}

static void
clipboard_html_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, TRUE);
}

static void
clipboard_text_received_for_paste_quote (GtkClipboard *clipboard,
                                         const gchar *text,
                                         gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (text != NULL);

	paste_quote_text (editor, text, FALSE);
}

static void
action_paste_quote_cb (GtkAction *action,
                       EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	GtkClipboard *clipboard;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (!gtk_widget_has_focus (GTK_WIDGET (cnt_editor)))
		gtk_widget_grab_focus (GTK_WIDGET (cnt_editor));

	clipboard = gtk_clipboard_get_for_display (
		gdk_display_get_default (),
		GDK_SELECTION_CLIPBOARD);

	if (e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML) {
		if (e_clipboard_wait_is_html_available (clipboard))
			e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
		else if (gtk_clipboard_wait_is_text_available (clipboard))
			gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
	} else {
		if (gtk_clipboard_wait_is_text_available (clipboard))
			gtk_clipboard_request_text (clipboard, clipboard_text_received_for_paste_quote, editor);
		else if (e_clipboard_wait_is_html_available (clipboard))
			e_clipboard_request_html (clipboard, clipboard_html_received_for_paste_quote, editor);
	}
}

static void
action_properties_cell_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	if (editor->priv->cell_dialog == NULL) {
		editor->priv->cell_dialog =
			e_html_editor_cell_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->cell_dialog));
}

static void
action_properties_image_cb (GtkAction *action,
                            EHTMLEditor *editor)
{
	if (editor->priv->image_dialog == NULL) {
		editor->priv->image_dialog =
			e_html_editor_image_dialog_new (editor);
	}

	e_html_editor_image_dialog_show (
		E_HTML_EDITOR_IMAGE_DIALOG (editor->priv->image_dialog));
}

static void
action_properties_link_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	if (editor->priv->link_dialog == NULL) {
		editor->priv->link_dialog =
			e_html_editor_link_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->link_dialog));
}

static void
action_properties_page_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	if (editor->priv->page_dialog == NULL) {
		editor->priv->page_dialog =
			e_html_editor_page_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->page_dialog));
}

static void
action_properties_paragraph_cb (GtkAction *action,
                                EHTMLEditor *editor)
{
	if (editor->priv->paragraph_dialog == NULL) {
		editor->priv->paragraph_dialog =
			e_html_editor_paragraph_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->paragraph_dialog));
}

static void
action_properties_rule_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	if (editor->priv->hrule_dialog == NULL) {
		editor->priv->hrule_dialog =
			e_html_editor_hrule_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->hrule_dialog));
}

static void
action_properties_table_cb (GtkAction *action,
                            EHTMLEditor *editor)
{
	if (editor->priv->table_dialog == NULL) {
		editor->priv->table_dialog =
			e_html_editor_table_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->table_dialog));
}

static void
action_properties_text_cb (GtkAction *action,
                           EHTMLEditor *editor)
{
	if (editor->priv->text_dialog == NULL) {
		editor->priv->text_dialog =
			e_html_editor_text_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->text_dialog));
}

static void
action_redo_cb (GtkAction *action,
                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_redo (cnt_editor);
}

static void
action_show_find_cb (GtkAction *action,
                     EHTMLEditor *editor)
{
	if (editor->priv->find_dialog == NULL) {
		editor->priv->find_dialog = e_html_editor_find_dialog_new (editor);
		gtk_action_set_sensitive (ACTION (FIND_AGAIN), TRUE);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->find_dialog));
}

static void
action_find_again_cb (GtkAction *action,
                      EHTMLEditor *editor)
{
	if (editor->priv->find_dialog == NULL)
		return;

	e_html_editor_find_dialog_find_next (
		E_HTML_EDITOR_FIND_DIALOG (editor->priv->find_dialog));
}

static void
action_show_replace_cb (GtkAction *action,
                        EHTMLEditor *editor)
{
	if (editor->priv->replace_dialog == NULL) {
		editor->priv->replace_dialog =
			e_html_editor_replace_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->replace_dialog));
}

static void
action_spell_check_cb (GtkAction *action,
                       EHTMLEditor *editor)
{
	if (editor->priv->spell_check_dialog == NULL) {
		editor->priv->spell_check_dialog =
			e_html_editor_spell_check_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->spell_check_dialog));
}

static void
action_undo_cb (GtkAction *action,
                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor))) {
		e_content_editor_undo (cnt_editor);
	}
}

static void
action_unindent_cb (GtkAction *action,
                    EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_selection_unindent (cnt_editor);
}

static void
action_wrap_lines_cb (GtkAction *action,
                      EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_selection_wrap (cnt_editor);
}

/* This is when the user toggled the action */
static void
manage_format_subsuperscript_toggled (EHTMLEditor *editor,
				      GtkToggleAction *changed_action,
				      const gchar *prop_name,
				      GtkToggleAction *second_action)
{
	EContentEditor *cnt_editor = e_html_editor_get_content_editor (editor);

	g_signal_handlers_block_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	if (gtk_toggle_action_get_active (changed_action) &&
	    gtk_toggle_action_get_active (second_action))
		gtk_toggle_action_set_active (second_action, FALSE);

	g_object_set (G_OBJECT (cnt_editor), prop_name, gtk_toggle_action_get_active (changed_action), NULL);

	g_signal_handlers_unblock_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

/* This is when the content editor claimed change on the property */
static void
manage_format_subsuperscript_notify (EHTMLEditor *editor,
				     GtkToggleAction *changed_action,
				     const gchar *prop_name,
				     GtkToggleAction *second_action)
{
	EContentEditor *cnt_editor = e_html_editor_get_content_editor (editor);
	gboolean value = FALSE;

	g_signal_handlers_block_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	g_object_get (G_OBJECT (cnt_editor), prop_name, &value, NULL);

	gtk_toggle_action_set_active (changed_action, value);

	if (gtk_toggle_action_get_active (changed_action) &&
	    gtk_toggle_action_get_active (second_action))
		gtk_toggle_action_set_active (second_action, FALSE);

	g_signal_handlers_unblock_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

static void
html_editor_actions_subscript_toggled_cb (GtkToggleAction *action,
					  EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_toggled (editor, GTK_TOGGLE_ACTION (ACTION (SUBSCRIPT)), "subscript", GTK_TOGGLE_ACTION (ACTION (SUPERSCRIPT)));
}

static void
html_editor_actions_notify_subscript_cb (EContentEditor *cnt_editor,
					 GParamSpec *param,
					 EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_notify (editor, GTK_TOGGLE_ACTION (ACTION (SUBSCRIPT)), "subscript", GTK_TOGGLE_ACTION (ACTION (SUPERSCRIPT)));
}

static void
html_editor_actions_superscript_toggled_cb (GtkToggleAction *action,
					    EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_toggled (editor, GTK_TOGGLE_ACTION (ACTION (SUPERSCRIPT)), "superscript", GTK_TOGGLE_ACTION (ACTION (SUBSCRIPT)));
}

static void
html_editor_actions_notify_superscript_cb (EContentEditor *cnt_editor,
					   GParamSpec *param,
					   EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_notify (editor, GTK_TOGGLE_ACTION (ACTION (SUPERSCRIPT)), "superscript", GTK_TOGGLE_ACTION (ACTION (SUBSCRIPT)));
}


/*****************************************************************************
 * Core Actions
 *
 * These actions are always enabled.
 *****************************************************************************/

static GtkActionEntry core_entries[] = {

	{ "copy",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy selected text to the clipboard"),
	  NULL }, /* Handled by focus tracker */

	{ "cut",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut selected text to the clipboard"),
	  NULL }, /* Handled by focus tracker */

	{ "paste",
	  "edit-paste",
	  N_("_Paste"),
	  "<Control>v",
	  N_("Paste text from the clipboard"),
	  NULL }, /* Handled by focus tracker */

	{ "redo",
	  "edit-redo",
	  N_("_Redo"),
	  "<Shift><Control>z",
	  N_("Redo the last undone action"),
	  G_CALLBACK (action_redo_cb) },

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  "<Control>a",
	  NULL,
	  NULL }, /* Handled by focus tracker */

	{ "undo",
	  "edit-undo",
	  N_("_Undo"),
	  "<Control>z",
	  N_("Undo the last action"),
	  G_CALLBACK (action_undo_cb) },

	/* Menus */

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "format-menu",
	  NULL,
	  N_("For_mat"),
	  NULL,
	  NULL,
	  NULL },

	{ "paragraph-style-menu",
	  NULL,
	  N_("_Paragraph Style"),
	  NULL,
	  NULL,
	  NULL },

	{ "insert-menu",
	  NULL,
	  N_("_Insert"),
	  NULL,
	  NULL,
	  NULL },

	{ "justify-menu",
	  NULL,
	  N_("_Alignment"),
	  NULL,
	  NULL,
	  NULL },

	{ "language-menu",
	  NULL,
	  N_("Current _Languages"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry core_editor_entries[] = {

	{ "indent",
	  "format-indent-more",
	  N_("_Increase Indent"),
	  "<Control>bracketright",
	  N_("Increase Indent"),
	  G_CALLBACK (action_indent_cb) },

	{ "insert-emoji",
	  NULL,
	  N_("E_moji"),
	  NULL,
	  N_("Insert Emoji"),
	  G_CALLBACK (action_insert_emoji_cb) },

	{ "insert-emoji-toolbar",
	  "face-smile",
	  N_("Insert E_moji"),
	  NULL,
	  N_("Insert Emoji"),
	  G_CALLBACK (action_insert_emoji_cb) },

	{ "insert-html-file",
	  NULL,
	  N_("_HTML File…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_html_file_cb) },

	{ "insert-text-file",
	  NULL,
	  N_("Te_xt File…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_text_file_cb) },

	{ "paste-quote",
	  NULL,
	  N_("Paste _Quotation"),
	  "<Control><Alt>v",
	  NULL,
	  G_CALLBACK (action_paste_quote_cb) },

	{ "show-find",
	  "edit-find",
	  N_("_Find…"),
	  "<Control>f",
	  N_("Search for text"),
	  G_CALLBACK (action_show_find_cb) },

	{ "find-again",
	  NULL,
	  N_("Find A_gain"),
	  "<Control>g",
	  NULL,
	  G_CALLBACK (action_find_again_cb) },

	{ "show-replace",
	  "edit-find-replace",
	  N_("Re_place…"),
	  "<Control>h",
	  N_("Search for and replace text"),
	  G_CALLBACK (action_show_replace_cb) },

	{ "spell-check",
	  "tools-check-spelling",
	  N_("Check _Spelling…"),
	  "F7",
	  NULL,
	  G_CALLBACK (action_spell_check_cb) },

	{ "unindent",
	  "format-indent-less",
	  N_("_Decrease Indent"),
	  "<Control>bracketleft",
	  N_("Decrease Indent"),
	  G_CALLBACK (action_unindent_cb) },

	{ "wrap-lines",
	  NULL,
	  N_("_Wrap Lines"),
	  "<Control><Shift>k",
	  NULL,
	  G_CALLBACK (action_wrap_lines_cb) }
};

static GtkRadioActionEntry core_justify_entries[] = {

	{ "justify-center",
	  "format-justify-center",
	  N_("_Center"),
	  "<Control>e",
	  N_("Center Alignment"),
	  E_CONTENT_EDITOR_ALIGNMENT_CENTER },

	{ "justify-fill",
	  "format-justify-fill",
	  N_("_Justified"),
	  "<Control>j",
	  N_("Align Justified"),
	  E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY },

	{ "justify-left",
	  "format-justify-left",
	  N_("_Left"),
	  "<Control>l",
	  N_("Left Alignment"),
	  E_CONTENT_EDITOR_ALIGNMENT_LEFT },

	{ "justify-right",
	  "format-justify-right",
	  N_("_Right"),
	  "<Control>r",
	  N_("Right Alignment"),
	  E_CONTENT_EDITOR_ALIGNMENT_RIGHT }
};

static GtkRadioActionEntry core_mode_entries[] = {

	{ "mode-html",
	  NULL,
	  N_("_HTML"),
	  NULL,
	  N_("HTML editing mode"),
	  E_CONTENT_EDITOR_MODE_HTML },

	{ "mode-plain",
	  NULL,
	  N_("Plain _Text"),
	  NULL,
	  N_("Plain text editing mode"),
	  E_CONTENT_EDITOR_MODE_PLAIN_TEXT },

	{ "mode-markdown",
	  NULL,
	  N_("_Markdown"),
	  NULL,
	  N_("Markdown editing mode"),
	  E_CONTENT_EDITOR_MODE_MARKDOWN },

	{ "mode-markdown-plain",
	  NULL,
	  N_("Ma_rkdown as Plain Text"),
	  NULL,
	  N_("Markdown editing mode, exported as Plain Text"),
	  E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT },

	{ "mode-markdown-html",
	  NULL,
	  N_("Mar_kdown as HTML"),
	  NULL,
	  N_("Markdown editing mode, exported as HTML"),
	  E_CONTENT_EDITOR_MODE_MARKDOWN_HTML }
};

static GtkRadioActionEntry core_style_entries[] = {

	{ "style-normal",
	  NULL,
	  N_("_Normal"),
	  "<Control>0",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH },

	{ "style-h1",
	  NULL,
	  N_("Heading _1"),
	  "<Control>1",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H1 },

	{ "style-h2",
	  NULL,
	  N_("Heading _2"),
	  "<Control>2",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H2 },

	{ "style-h3",
	  NULL,
	  N_("Heading _3"),
	  "<Control>3",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H3 },

	{ "style-h4",
	  NULL,
	  N_("Heading _4"),
	  "<Control>4",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H4 },

	{ "style-h5",
	  NULL,
	  N_("Heading _5"),
	  "<Control>5",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H5 },

	{ "style-h6",
	  NULL,
	  N_("Heading _6"),
	  "<Control>6",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H6 },

        { "style-preformat",
          NULL,
          N_("_Preformatted"),
          "<Control>7",
          NULL,
          E_CONTENT_EDITOR_BLOCK_FORMAT_PRE },

	{ "style-address",
	  NULL,
	  N_("A_ddress"),
	  "<Control>8",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS },

	{ "style-list-bullet",
	  NULL,
	  N_("_Bulleted List"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST },

	{ "style-list-roman",
	  NULL,
	  N_("_Roman Numeral List"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN },

	{ "style-list-number",
	  NULL,
	  N_("Numbered _List"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST },

	{ "style-list-alpha",
	  NULL,
	  N_("_Alphabetical List"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA }
};

/*****************************************************************************
 * Core Actions (HTML only)
 *
 * These actions are only enabled in HTML mode.
 *****************************************************************************/

static GtkActionEntry html_entries[] = {

	{ "insert-image",
	  "insert-image",
	  N_("_Image…"),
	  NULL,
	  /* Translators: This is an action tooltip */
	  N_("Insert Image"),
	  G_CALLBACK (action_insert_image_cb) },

	{ "insert-link",
	  "insert-link",
	  N_("_Link…"),
	  "<Control>k",
	  N_("Insert Link"),
	  G_CALLBACK (action_insert_link_cb) },

	{ "insert-rule",
	  "stock_insert-rule",
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule…"),
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Insert Rule"),
	  G_CALLBACK (action_insert_rule_cb) },

	{ "insert-table",
	  "stock_insert-table",
	  N_("_Table…"),
	  NULL,
	  N_("Insert Table"),
	  G_CALLBACK (action_insert_table_cb) },

	{ "properties-cell",
	  NULL,
	  N_("_Cell…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_cell_cb) },

	{ "properties-image",
	  NULL,
	  N_("_Image…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_image_cb) },

	{ "properties-link",
	  NULL,
	  N_("_Link…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_link_cb) },

	{ "properties-page",
	  NULL,
	  N_("Pa_ge…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_page_cb) },

	{ "properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_rule_cb) },

	{ "properties-table",
	  NULL,
	  N_("_Table…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_table_cb) },

	/* Menus */

	{ "font-size-menu",
	  NULL,
	  N_("Font _Size"),
	  NULL,
	  NULL,
	  NULL },

	{ "font-style-menu",
	  NULL,
	  N_("_Font Style"),
	  NULL,
	  NULL,
	  NULL },

	{ "paste-as-text",
	  NULL,
	  N_("Paste As _Text"),
	  "<Shift><Control>v",
	  NULL,
	  G_CALLBACK (action_paste_as_text_cb) },

};

static GtkToggleActionEntry html_toggle_entries[] = {

	{ "bold",
	  "format-text-bold",
	  N_("_Bold"),
	  "<Control>b",
	  N_("Bold"),
	  NULL,
	  FALSE },

	{ "italic",
	  "format-text-italic",
	  N_("_Italic"),
	  "<Control>i",
	  N_("Italic"),
	  NULL,
	  FALSE },

	{ "strikethrough",
	  "format-text-strikethrough",
	  N_("_Strikethrough"),
	  NULL,
	  N_("Strikethrough"),
	  NULL,
	  FALSE },

	{ "subscript",
	  NULL,
	  N_("Subs_cript"),
	  "<Control><Shift>b",
	  N_("Subscript"),
	  NULL,
	  FALSE },

	{ "superscript",
	  NULL,
	  N_("Su_perscript"),
	  "<Control><Shift>p",
	  N_("Superscript"),
	  NULL,
	  FALSE },

	{ "underline",
	  "format-text-underline",
	  N_("_Underline"),
	  "<Control>u",
	  N_("Underline"),
	  NULL,
	  FALSE }
};

static GtkRadioActionEntry html_size_entries[] = {

	{ "size-minus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-2"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_TINY },

	{ "size-minus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-1"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_SMALL },

	{ "size-plus-zero",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+0"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_NORMAL },

	{ "size-plus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+1"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_BIG },

	{ "size-plus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+2"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_BIGGER },

	{ "size-plus-three",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+3"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_LARGE },

	{ "size-plus-four",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+4"),
	  NULL,
	  NULL,
	  E_CONTENT_EDITOR_FONT_SIZE_VERY_LARGE }
};

/*****************************************************************************
 * Context Menu Actions
 *
 * These require separate action entries so we can toggle their visiblity
 * rather than their sensitivity as we do with main menu / toolbar actions.
 * Note that some of these actions use the same callback function as their
 * non-context sensitive counterparts.
 *****************************************************************************/

static GtkActionEntry context_entries[] = {

	{ "context-copy-link",
	  "edit-copy",
	  N_("Copy _Link Location"),
	  NULL,
	  N_("Copy the link to the clipboard"),
	  G_CALLBACK (action_copy_link_cb) },

	{ "context-open-link",
	  "emblem-web",
	  N_("_Open Link in Browser"),
	  NULL,
	  N_("Open the link in a web browser"),
	  G_CALLBACK (action_open_link_cb) },

	{ "context-delete-cell",
	  NULL,
	  N_("Cell Contents"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_cell_contents_cb) },

	{ "context-delete-column",
	  NULL,
	  N_("Column"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_column_cb) },

	{ "context-delete-row",
	  NULL,
	  N_("Row"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_row_cb) },

	{ "context-delete-table",
	  NULL,
	  N_("Table"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_table_cb) },

	/* Menus */

	{ "context-delete-table-menu",
	  NULL,
	  /* Translators: Popup menu item caption, containing all the Delete options for a table */
	  N_("Table Delete"),
	  NULL,
	  NULL,
	  NULL },

	{ "context-insert-table-menu",
	  NULL,
	  /* Translators: Popup menu item caption, containing all the Insert options for a table */
	  N_("Table Insert"),
	  NULL,
	  NULL,
	  NULL },

	{ "context-properties-menu",
	  NULL,
	  N_("Properties"),
	  NULL,
	  NULL,
	  NULL },
};

/*****************************************************************************
 * Context Menu Actions (HTML only)
 *
 * These actions are never visible in plain-text mode.  Note that some
 * of them use the same callback function as their non-context sensitive
 * counterparts.
 *****************************************************************************/

static GtkActionEntry html_context_entries[] = {

	{ "context-delete-hrule",
	  NULL,
	  N_("Delete Rule"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_hrule_cb) },

	{ "context-delete-image",
	  NULL,
	  N_("Delete Image"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_delete_image_cb) },

	{ "context-insert-column-after",
	  NULL,
	  N_("Column After"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_insert_column_after_cb) },

	{ "context-insert-column-before",
	  NULL,
	  N_("Column Before"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_insert_column_before_cb) },

	{ "context-insert-link",
	  NULL,
	  N_("Insert _Link"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_link_cb) },

	{ "context-insert-row-above",
	  NULL,
	  N_("Row Above"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_insert_row_above_cb) },

	{ "context-insert-row-below",
	  NULL,
	  N_("Row Below"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_insert_row_below_cb) },

	{ "context-properties-cell",
	  NULL,
	  N_("Cell…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_cell_cb) },

	{ "context-properties-image",
	  NULL,
	  N_("Image…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_image_cb) },

	{ "context-properties-link",
	  NULL,
	  N_("Link…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_link_cb) },

	{ "context-properties-page",
	  NULL,
	  N_("Page…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_page_cb) },

	{ "context-properties-paragraph",
	  NULL,
	  N_("Paragraph…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_paragraph_cb) },

	{ "context-properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Rule…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_rule_cb) },

	{ "context-properties-table",
	  NULL,
	  N_("Table…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_table_cb) },

	{ "context-properties-text",
	  NULL,
	  N_("Text…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_text_cb) },

	{ "context-remove-link",
	  NULL,
	  N_("Remove Link"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_remove_link_cb) }
};

/*****************************************************************************
 * Context Menu Actions (spell checking only)
 *
 * These actions are only visible when the word underneath the cursor is
 * misspelled.
 *****************************************************************************/

static GtkActionEntry spell_context_entries[] = {

	{ "context-spell-add",
	  NULL,
	  N_("Add Word to Dictionary"),
	  NULL,
	  NULL,
          G_CALLBACK (action_context_spell_add_cb) },

	{ "context-spell-ignore",
	  NULL,
	  N_("Ignore Misspelled Word"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_context_spell_ignore_cb) },

	{ "context-spell-add-menu",
	  NULL,
	  N_("Add Word To"),
	  NULL,
	  NULL,
	  NULL },

	/* Menus */

	{ "context-more-suggestions-menu",
	  NULL,
	  N_("More Suggestions"),
	  NULL,
	  NULL,
	  NULL }
};

static gboolean
editor_actions_add_to_recent_languages (EHTMLEditor *editor,
					const gchar *language_code)
{
	GtkAction *language_action;
	gchar *name;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), FALSE);
	g_return_val_if_fail (language_code != NULL, FALSE);

	language_action = gtk_action_group_get_action (editor->priv->language_actions, language_code);
	if (!language_action)
		return FALSE;

	name = g_strconcat ("recent-spell-language-", language_code, NULL);

	if (!gtk_action_group_get_action (editor->priv->language_actions, name)) {
		GtkToggleAction *toggle_action;

		toggle_action = gtk_toggle_action_new (name,
			gtk_action_get_label (language_action),
			gtk_action_get_tooltip (language_action),
			NULL);

		e_binding_bind_property (language_action, "active",
			toggle_action, "active",
			G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

		gtk_action_group_add_action (editor->priv->language_actions, GTK_ACTION (toggle_action));

		g_object_unref (toggle_action);
	}

	gtk_ui_manager_add_ui (
		editor->priv->manager, editor->priv->recent_spell_languages_merge_id,
		"/main-menu/edit-menu/language-menu/recent-languages",
		name, name, GTK_UI_MANAGER_AUTO, FALSE);

	g_free (name);

	return TRUE;
}

static void
editor_actions_setup_languages_menu (EHTMLEditor *editor)
{
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GHashTable *lang_parents; /* gchar *name ~> GtkAction * */
	GList *list = NULL, *link;
	GSettings *settings;
	gchar **strv;
	gint ii, added = 0, max_items;
	guint merge_id;

	lang_parents = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	manager = editor->priv->manager;
	action_group = editor->priv->language_actions;
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	merge_id = gtk_ui_manager_new_merge_id (manager);
	editor->priv->recent_spell_languages_merge_id = gtk_ui_manager_new_merge_id (manager);

	list = e_spell_checker_list_available_dicts (spell_checker);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary = link->data;
		GtkAction *parent_action;
		GtkToggleAction *action;
		const gchar *dictionay_name;
		gchar *language_name, *path;
		GString *escaped_name = NULL;
		gboolean active = FALSE;

		if (!e_util_get_language_info (e_spell_dictionary_get_code (dictionary), &language_name, NULL)) {
			language_name = g_strdup (e_spell_dictionary_get_code (dictionary));
			if (language_name) {
				gchar *ptr;

				ptr = strchr (language_name, '_');
				if (ptr)
					*ptr = '\0';
			} else {
				language_name = g_strdup ("");
			}
		}

		dictionay_name = e_spell_dictionary_get_name (dictionary);
		if (dictionay_name && strchr (dictionay_name, '_') != NULL)
			escaped_name = e_str_replace_string (dictionay_name, "_", "__");

		action = gtk_toggle_action_new (
			e_spell_dictionary_get_code (dictionary),
			escaped_name ? escaped_name->str : dictionay_name,
			NULL, NULL);

		if (escaped_name)
			g_string_free (escaped_name, TRUE);

		/* Do this BEFORE connecting to the "toggled" signal.
		 * We're not prepared to invoke the signal handler yet.
		 * The "Add Word To" actions have not yet been added. */
		active = e_spell_checker_get_language_active (
			spell_checker, e_spell_dictionary_get_code (dictionary));
		gtk_toggle_action_set_active (action, active);

		g_signal_connect (
			action, "toggled",
			G_CALLBACK (action_language_cb), editor);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (action));

		g_object_unref (action);

		parent_action = g_hash_table_lookup (lang_parents, language_name);
		if (!parent_action) {
			gchar *name, *tmp;

			name = g_strdup (e_spell_dictionary_get_code (dictionary));
			tmp = strchr (name, '_');
			if (tmp)
				*tmp = '\0';

			tmp = g_strconcat ("language-parent-", name, NULL);
			g_free (name);
			name = tmp;

			parent_action = gtk_action_new (name, language_name, NULL, NULL);

			gtk_action_group_add_action (action_group, parent_action);

			g_hash_table_insert (lang_parents, g_strdup (language_name), parent_action);

			gtk_ui_manager_add_ui (
				manager, merge_id,
				"/main-menu/edit-menu/language-menu/all-languages",
				name, name, GTK_UI_MANAGER_MENU, FALSE);

			g_free (name);
		}

		path = g_strconcat ("/main-menu/edit-menu/language-menu/all-languages/", gtk_action_get_name (parent_action), NULL);

		gtk_ui_manager_add_ui (
			manager, merge_id,
			path,
			e_spell_dictionary_get_code (dictionary),
			e_spell_dictionary_get_code (dictionary),
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (language_name);
		g_free (path);
	}

	g_list_free (list);
	g_clear_object (&spell_checker);
	g_hash_table_destroy (lang_parents);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	strv = g_settings_get_strv (settings, "composer-spell-languages-recently-used");
	max_items = g_settings_get_int (settings, "composer-spell-languages-max-recently-used");
	if (max_items < 5)
		max_items = 5;
	g_object_unref (settings);

	for (ii = 0; strv && strv[ii] && added < max_items; ii++) {
		if (editor_actions_add_to_recent_languages (editor, strv[ii]))
			added++;
	}

	g_strfreev (strv);
}

static void
editor_actions_setup_spell_check_menu (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *available_dicts = NULL, *iter;
	guint merge_id;

	manager = editor->priv->manager;
	action_group = editor->priv->spell_check_actions;
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	available_dicts = e_spell_checker_list_available_dicts (spell_checker);
	merge_id = gtk_ui_manager_new_merge_id (manager);

	for (iter = available_dicts; iter; iter = iter->next) {
		ESpellDictionary *dictionary = iter->data;
		GtkAction *action;
		const gchar *code;
		const gchar *name;
		GString *escaped_name = NULL;
		gchar *action_label;
		gchar *action_name;

		code = e_spell_dictionary_get_code (dictionary);
		name = e_spell_dictionary_get_name (dictionary);

		/* Add a suggestion menu. */
		action_name = g_strdup_printf (
			"context-spell-suggest-%s-menu", code);

		if (name && strchr (name, '_') != NULL)
			escaped_name = e_str_replace_string (name, "_", "__");

		action = gtk_action_new (action_name, escaped_name ? escaped_name->str : name, NULL, NULL);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/context-menu/context-spell-suggest",
			action_name, action_name,
			GTK_UI_MANAGER_MENU, FALSE);

		g_free (action_name);

		/* Add an item to the "Add Word To" menu. */
		action_name = g_strdup_printf ("context-spell-add-%s", code);
		/* Translators: %s will be replaced with the actual dictionary
		 * name, where a user can add a word to. This is part of an
		 * "Add Word To" submenu. */
		action_label = g_strdup_printf (_("%s Dictionary"), escaped_name ? escaped_name->str : name);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_add_cb), editor);

		gtk_action_set_visible (action, e_spell_checker_get_language_active (spell_checker, code));

		gtk_action_group_add_action (action_group, action);

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/context-menu/context-spell-add-menu",
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_label);
		g_free (action_name);

		if (escaped_name)
			g_string_free (escaped_name, TRUE);
	}

	g_list_free (available_dicts);
	g_clear_object (&spell_checker);
}

void
e_html_editor_actions_init (EHTMLEditor *editor)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;
	guint ii;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manager = e_html_editor_get_ui_manager (editor);
	domain = GETTEXT_PACKAGE;

	/* Core Actions */
	action_group = editor->priv->core_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, core_entries,
		G_N_ELEMENTS (core_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	action_group = editor->priv->core_editor_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, core_editor_entries,
		G_N_ELEMENTS (core_editor_entries), editor);
	gtk_action_group_add_radio_actions (
		action_group, core_justify_entries,
		G_N_ELEMENTS (core_justify_entries),
		E_CONTENT_EDITOR_ALIGNMENT_LEFT,
		NULL, NULL);
	gtk_action_group_add_radio_actions (
		action_group, core_mode_entries,
		G_N_ELEMENTS (core_mode_entries),
		E_CONTENT_EDITOR_MODE_HTML,
		G_CALLBACK (action_mode_cb), editor);
	gtk_action_group_add_radio_actions (
		action_group, core_style_entries,
		G_N_ELEMENTS (core_style_entries),
		E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH,
		NULL, NULL);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Face Action */
	action = e_emoticon_action_new (
		"insert-emoticon", _("_Emoticon"),
		_("Insert Emoticon"), NULL);
	g_object_set (action, "icon-name", "face-smile", NULL);
	g_signal_connect (
		action, "item-activated",
		G_CALLBACK (action_insert_emoticon_cb), editor);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);

	/* Core Actions (HTML only) */
	action_group = editor->priv->html_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, html_entries,
		G_N_ELEMENTS (html_entries), editor);
	gtk_action_group_add_toggle_actions (
		action_group, html_toggle_entries,
		G_N_ELEMENTS (html_toggle_entries), editor);
	gtk_action_group_add_radio_actions (
		action_group, html_size_entries,
		G_N_ELEMENTS (html_size_entries),
		E_CONTENT_EDITOR_FONT_SIZE_NORMAL,
		NULL, NULL);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Context Menu Actions */
	action_group = editor->priv->context_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, context_entries,
		G_N_ELEMENTS (context_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Context Menu Actions (HTML only) */
	action_group = editor->priv->html_context_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, html_context_entries,
		G_N_ELEMENTS (html_context_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Context Menu Actions (spell check only) */
	action_group = editor->priv->spell_check_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, spell_context_entries,
		G_N_ELEMENTS (spell_context_entries), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Language actions are generated dynamically. */
	editor_actions_setup_languages_menu (editor);
	action_group = editor->priv->language_actions;
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Some spell check actions are generated dynamically. */
	action_group = editor->priv->suggestion_actions;
	editor_actions_setup_spell_check_menu (editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Do this after all language actions are initialized. */
	e_html_editor_update_spell_actions (editor);

	/* Fine Tuning */

	g_object_set (
		G_OBJECT (ACTION (SHOW_FIND)),
		"short-label", _("_Find"), NULL);
	g_object_set (
		G_OBJECT (ACTION (SHOW_REPLACE)),
		"short-label", _("Re_place"), NULL);
	g_object_set (
		G_OBJECT (ACTION (INSERT_EMOJI)),
		"short-label", _("E_moji"), NULL);
	g_object_set (
		G_OBJECT (ACTION (INSERT_IMAGE)),
		"short-label", _("_Image"), NULL);
	g_object_set (
		G_OBJECT (ACTION (INSERT_LINK)),
		"short-label", _("_Link"), NULL);
	g_object_set (
		G_OBJECT (ACTION (INSERT_RULE)),
		/* Translators: 'Rule' here means a horizontal line in an HTML text */
		"short-label", _("_Rule"), NULL);
	g_object_set (
		G_OBJECT (ACTION (INSERT_TABLE)),
		"short-label", _("_Table"), NULL);

	gtk_action_set_sensitive (ACTION (UNINDENT), FALSE);
	gtk_action_set_sensitive (ACTION (FIND_AGAIN), FALSE);

	g_signal_connect_object (ACTION (SUBSCRIPT), "toggled",
		G_CALLBACK (html_editor_actions_subscript_toggled_cb), editor, 0);
	g_signal_connect_object (ACTION (SUPERSCRIPT), "toggled",
		G_CALLBACK (html_editor_actions_superscript_toggled_cb), editor, 0);
	g_signal_connect (editor, "notify::mode",
		G_CALLBACK (html_editor_actions_notify_mode_cb), NULL);

	action_group = editor->priv->core_editor_actions;
	action = gtk_action_group_get_action (action_group, "mode-html");
	e_binding_bind_property (
		editor, "mode",
		action, "current-value",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	for (ii = 0; ii < G_N_ELEMENTS (core_mode_entries); ii++) {
		action = gtk_action_group_get_action (action_group, core_mode_entries[ii].name);

		gtk_action_set_visible (action, e_html_editor_has_editor_for_mode (editor, core_mode_entries[ii].value));
	}
}

static gboolean
e_html_editor_content_editor_font_name_to_combo_box (GBinding *binding,
						     const GValue *from_value,
						     GValue *to_value,
						     gpointer user_data)
{
	gchar *id = NULL;

	id = e_html_editor_util_dup_font_id (GTK_COMBO_BOX (g_binding_get_target (binding)), g_value_get_string (from_value));
	g_value_take_string (to_value, id ? id : g_strdup (""));

	return TRUE;
}

static gboolean
e_html_editor_indent_level_to_bool_indent_cb (GBinding *binding,
					      const GValue *from_value,
					      GValue *to_value,
					      gpointer user_data)
{
	g_value_set_boolean (to_value, g_value_get_int (from_value) < E_HTML_EDITOR_MAX_INDENT_LEVEL);

	return TRUE;
}

static gboolean
e_html_editor_indent_level_to_bool_unindent_cb (GBinding *binding,
						const GValue *from_value,
						GValue *to_value,
						gpointer user_data)
{
	g_value_set_boolean (to_value, g_value_get_int (from_value) > 0);

	return TRUE;
}

static gboolean
e_html_editor_sensitize_html_actions_cb (GBinding *binding,
					 const GValue *from_value,
					 GValue *to_value,
					 gpointer user_data)
{
	/* It should be editable... */
	if (g_value_get_boolean (from_value)) {
		EHTMLEditor *editor = user_data;

		/* ... and in the HTML mode */
		g_value_set_boolean (to_value, e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML);
	} else {
		g_value_set_boolean (to_value, FALSE);
	}

	return TRUE;
}

/**
 * e_html_editor_util_new_mode_combobox:
 *
 * Creates a new combo box containing all composer modes.
 *
 * It's a descendant of #EActionComboBox, thus use e_action_combo_box_get_current_value()
 * and e_action_combo_box_set_current_value() to get the currently selected mode.
 *
 * Returns: (transfer full): a new #EActionComboBox with composer modes
 *
 * Since: 3.44
 **/
EActionComboBox *
e_html_editor_util_new_mode_combobox (void)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkWidget *widget;

	action_group = gtk_action_group_new ("core-mode-entries");

	gtk_action_group_add_radio_actions (
		action_group, core_mode_entries,
		G_N_ELEMENTS (core_mode_entries),
		E_CONTENT_EDITOR_MODE_HTML,
		NULL, NULL);

	action = gtk_action_group_get_action (action_group, "mode-html");

	widget = e_action_combo_box_new_with_action (GTK_RADIO_ACTION (action));
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (widget), FALSE);
	gtk_widget_set_tooltip_text (widget, _("Editing Mode"));

	g_object_set_data_full (G_OBJECT (widget), "core-mode-entries-action-group", action_group, g_object_unref);

	return E_ACTION_COMBO_BOX (widget);
}

void
e_html_editor_actions_bind (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	cnt_editor = e_html_editor_get_content_editor (editor);

	/* 'rb' for 'remember binding' */
	#define rb(x) editor->priv->content_editor_bindings = g_slist_prepend (editor->priv->content_editor_bindings, g_object_ref (x))

	rb (e_binding_bind_property (
		editor->priv->fg_color_combo_box, "current-color",
		cnt_editor, "font-color",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->fg_color_combo_box, "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		editor->priv->bg_color_combo_box, "current-color",
		cnt_editor, "background-color",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->bg_color_combo_box, "sensitive",
		G_BINDING_SYNC_CREATE));

	rb (e_binding_bind_property (
		cnt_editor, "can-redo",
		ACTION (REDO), "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "can-undo",
		ACTION (UNDO), "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "can-copy",
		ACTION (COPY), "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "can-cut",
		ACTION (CUT), "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "can-paste",
		ACTION (PASTE), "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "can-paste",
		ACTION (PASTE_QUOTE), "sensitive",
		G_BINDING_SYNC_CREATE));

	/* This is connected to JUSTIFY_LEFT action only, but
	 * it automatically applies on all actions in the group. */
	rb (e_binding_bind_property (
		cnt_editor, "alignment",
		ACTION (JUSTIFY_LEFT), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "bold",
		ACTION (BOLD), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "font-size",
		ACTION (FONT_SIZE_GROUP), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "block-format",
		ACTION (STYLE_NORMAL), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property_full (
		cnt_editor, "indent-level",
		ACTION (INDENT), "sensitive",
		G_BINDING_SYNC_CREATE,
		e_html_editor_indent_level_to_bool_indent_cb,
		NULL, NULL, NULL));
	rb (e_binding_bind_property_full (
		cnt_editor, "indent-level",
		ACTION (UNINDENT), "sensitive",
		G_BINDING_SYNC_CREATE,
		e_html_editor_indent_level_to_bool_unindent_cb,
		NULL, NULL, NULL));
	rb (e_binding_bind_property (
		cnt_editor, "italic",
		ACTION (ITALIC), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "strikethrough",
		ACTION (STRIKETHROUGH), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property (
		cnt_editor, "underline",
		ACTION (UNDERLINE), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property_full (
		cnt_editor, "font-name",
		editor->priv->font_name_combo_box, "active-id",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
		e_html_editor_content_editor_font_name_to_combo_box,
		NULL,
		NULL, NULL));

	/* Cannot use binding, due to subscript and superscript being mutually exclusive */
	editor->priv->subscript_notify_id = g_signal_connect_object (cnt_editor, "notify::subscript",
		G_CALLBACK (html_editor_actions_notify_subscript_cb), editor, 0);
	editor->priv->superscript_notify_id = g_signal_connect_object (cnt_editor, "notify::superscript",
		G_CALLBACK (html_editor_actions_notify_superscript_cb), editor, 0);

	/* Disable all actions and toolbars when editor is not editable */
	rb (e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->core_editor_actions, "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property_full (
		cnt_editor, "editable",
		editor->priv->html_actions, "sensitive",
		G_BINDING_SYNC_CREATE,
		e_html_editor_sensitize_html_actions_cb,
		NULL, editor, NULL));
	rb (e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->spell_check_actions, "sensitive",
		G_BINDING_SYNC_CREATE));
	rb (e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->suggestion_actions, "sensitive",
		G_BINDING_SYNC_CREATE));

	#undef rb
}

void
e_html_editor_actions_unbind (EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	g_slist_foreach (editor->priv->content_editor_bindings, (GFunc) g_binding_unbind, NULL);
	g_slist_free_full (editor->priv->content_editor_bindings, g_object_unref);
	editor->priv->content_editor_bindings = NULL;

	cnt_editor = e_html_editor_get_content_editor (editor);

	if (cnt_editor) {
		e_signal_disconnect_notify_handler (cnt_editor, &editor->priv->subscript_notify_id);
		e_signal_disconnect_notify_handler (cnt_editor, &editor->priv->superscript_notify_id);
	}
}

void
e_html_editor_actions_update_spellcheck_languages_menu (EHTMLEditor *editor,
							const gchar * const *languages)
{
	GHashTable *active;
	GList *actions, *link;
	gint ii;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	active = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (ii = 0; languages && languages[ii]; ii++) {
		g_hash_table_insert (active, g_strdup (languages[ii]), NULL);
	}

	actions = gtk_action_group_list_actions (editor->priv->language_actions);
	for (link = actions; link; link = g_list_next (link)) {
		GtkToggleAction *toggle_action;
		gboolean is_active;

		if (!GTK_IS_TOGGLE_ACTION (link->data))
			continue;

		if (gtk_action_get_name (link->data) &&
		    g_str_has_prefix (gtk_action_get_name (link->data), "recent-spell-language-"))
			continue;

		is_active = g_hash_table_contains (active, gtk_action_get_name (link->data));
		toggle_action = GTK_TOGGLE_ACTION (link->data);

		if ((gtk_toggle_action_get_active (toggle_action) ? 1 : 0) != (is_active ? 1 : 0)) {
			g_signal_handlers_block_by_func (toggle_action, action_language_cb, editor);
			gtk_toggle_action_set_active (toggle_action, is_active);
			g_signal_handlers_unblock_by_func (toggle_action, action_language_cb, editor);
		}
	}

	g_hash_table_destroy (active);
	g_list_free (actions);
}

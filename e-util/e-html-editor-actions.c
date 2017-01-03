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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <enchant/enchant.h>

#include "e-html-editor.h"
#include "e-html-editor-private.h"
#include "e-html-editor-actions.h"
#include "e-emoticon-action.h"
#include "e-emoticon-chooser.h"
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
action_copy_cb (GtkAction *action,
                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_copy (cnt_editor);
}

static void
action_cut_cb (GtkAction *action,
               EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_cut (cnt_editor);
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
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new (
		_("Insert HTML File"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_ACCEPT, NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("HTML file"));
	gtk_file_filter_add_mime_type (filter, "text/html");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (dialog));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GFile *file;

		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (dialog));
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

		/* XXX Need a way to cancel this. */
		g_file_load_contents_async (
			file, NULL, (GAsyncReadyCallback)
			insert_html_file_ready_cb,
			g_object_ref (editor));

		g_object_unref (file);
	}

	gtk_widget_destroy (dialog);
}

static void
action_insert_image_cb (GtkAction *action,
                        EHTMLEditor *editor)
{
	GtkWidget *dialog;

	dialog = e_image_chooser_dialog_new (C_("dialog-title", "Insert Image"), NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		EContentEditor *cnt_editor;
		gchar *uri;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

		cnt_editor = e_html_editor_get_content_editor (editor);
		e_content_editor_insert_image (cnt_editor, uri);

		g_free (uri);
	}

	gtk_widget_destroy (dialog);
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
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new (
		_("Insert text file"), NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_ACCEPT, NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Text file"));
	gtk_file_filter_add_mime_type (filter, "text/plain");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (dialog));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GFile *file;

		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (dialog));
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

		/* XXX Need a way to cancel this. */
		g_file_load_contents_async (
			file, NULL, (GAsyncReadyCallback)
			insert_text_file_ready_cb,
			g_object_ref (editor));

		g_object_unref (file);
	}

	gtk_widget_destroy (dialog);
}

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
}

static gboolean
update_mode_combobox (gpointer data)
{
	EHTMLEditor *editor = data;
	EContentEditor *cnt_editor;
	GtkAction *action;
	gboolean is_html;

	if (!E_IS_HTML_EDITOR (editor))
		return FALSE;

	cnt_editor = e_html_editor_get_content_editor (editor);
	is_html = e_content_editor_get_html_mode (cnt_editor);

	action = gtk_action_group_get_action (
		editor->priv->core_editor_actions, "mode-html");
	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (action), (is_html ? 1 : 0));

	return FALSE;
}

static void
action_mode_cb (GtkRadioAction *action,
                GtkRadioAction *current,
                EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;
	GtkActionGroup *action_group;
	GtkWidget *style_combo_box;
	gboolean is_html;

	cnt_editor = e_html_editor_get_content_editor (editor);
	is_html = e_content_editor_get_html_mode (cnt_editor);

	/* This must be done from idle callback, because apparently we can change
	 * current value in callback of current value change */
	g_idle_add (update_mode_combobox, editor);

	action_group = editor->priv->html_actions;
	gtk_action_group_set_visible (action_group, is_html);

	action_group = editor->priv->html_context_actions;
	gtk_action_group_set_visible (action_group, is_html);

	gtk_widget_set_sensitive (editor->priv->color_combo_box, is_html);

	if (is_html) {
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
action_paste_cb (GtkAction *action,
                 EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_paste (cnt_editor);
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
		E_CONTENT_EDITOR_INSERT_TEXT_PLAIN);
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
		(is_html ? E_CONTENT_EDITOR_INSERT_TEXT_HTML : E_CONTENT_EDITOR_INSERT_TEXT_PLAIN));
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

	if (e_content_editor_get_html_mode (cnt_editor)) {
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
action_select_all_cb (GtkAction *action,
                      EHTMLEditor *editor)
{
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_select_all (cnt_editor);
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
	  G_CALLBACK (action_copy_cb) },

	{ "cut",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut selected text to the clipboard"),
	  G_CALLBACK (action_cut_cb) },

	{ "paste",
	  "edit-paste",
	  N_("_Paste"),
	  NULL, /* Widgets are treating Ctrl + v shortcut themselves */
	  N_("Paste text from the clipboard"),
	  G_CALLBACK (action_paste_cb) },

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
	  G_CALLBACK (action_select_all_cb) },

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

	{ "insert-html-file",
	  NULL,
	  N_("_HTML File..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_html_file_cb) },

	{ "insert-text-file",
	  NULL,
	  N_("Te_xt File..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_text_file_cb) },

	{ "paste-quote",
	  NULL,
	  N_("Paste _Quotation"),
	  "<Shift><Control>v",
	  NULL,
	  G_CALLBACK (action_paste_quote_cb) },

	{ "show-find",
	  "edit-find",
	  N_("_Find..."),
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
	  N_("Re_place..."),
	  "<Control>h",
	  N_("Search for and replace text"),
	  G_CALLBACK (action_show_replace_cb) },

	{ "spell-check",
	  "tools-check-spelling",
	  N_("Check _Spelling..."),
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
	  "<Control>k",
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
	  TRUE },	/* e_content_editor_set_html_mode */

	{ "mode-plain",
	  NULL,
	  N_("Plain _Text"),
	  NULL,
	  N_("Plain text editing mode"),
	  FALSE }	/* e_content_editor_set_html_mode */
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
	  N_("Header _1"),
	  "<Control>1",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H1 },

	{ "style-h2",
	  NULL,
	  N_("Header _2"),
	  "<Control>2",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H2 },

	{ "style-h3",
	  NULL,
	  N_("Header _3"),
	  "<Control>3",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H3 },

	{ "style-h4",
	  NULL,
	  N_("Header _4"),
	  "<Control>4",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H4 },

	{ "style-h5",
	  NULL,
	  N_("Header _5"),
	  "<Control>5",
	  NULL,
	  E_CONTENT_EDITOR_BLOCK_FORMAT_H5 },

	{ "style-h6",
	  NULL,
	  N_("Header _6"),
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
	  N_("_Image..."),
	  NULL,
	  /* Translators: This is an action tooltip */
	  N_("Insert Image"),
	  G_CALLBACK (action_insert_image_cb) },

	{ "insert-link",
	  "insert-link",
	  N_("_Link..."),
	  NULL,
	  N_("Insert Link"),
	  G_CALLBACK (action_insert_link_cb) },

	{ "insert-rule",
	  "stock_insert-rule",
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule..."),
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Insert Rule"),
	  G_CALLBACK (action_insert_rule_cb) },

	{ "insert-table",
	  "stock_insert-table",
	  N_("_Table..."),
	  NULL,
	  N_("Insert Table"),
	  G_CALLBACK (action_insert_table_cb) },

	{ "properties-cell",
	  NULL,
	  N_("_Cell..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_cell_cb) },

	{ "properties-image",
	  NULL,
	  N_("_Image..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_image_cb) },

	{ "properties-link",
	  NULL,
	  N_("_Link..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_link_cb) },

	{ "properties-page",
	  NULL,
	  N_("Pa_ge..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_page_cb) },

	{ "properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_rule_cb) },

	{ "properties-table",
	  NULL,
	  N_("_Table..."),
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
	  NULL,
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

	{ "monospaced",
	  "stock_text-monospaced",
	  N_("_Plain Text"),
	  "<Control>t",
	  N_("Plain Text"),
	  NULL,
	  FALSE },

	{ "strikethrough",
	  "format-text-strikethrough",
	  N_("_Strikethrough"),
	  NULL,
	  N_("Strikethrough"),
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
	  N_("Cell..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_cell_cb) },

	{ "context-properties-image",
	  NULL,
	  N_("Image..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_image_cb) },

	{ "context-properties-link",
	  NULL,
	  N_("Link..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_link_cb) },

	{ "context-properties-page",
	  NULL,
	  N_("Page..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_page_cb) },

	{ "context-properties-paragraph",
	  NULL,
	  N_("Paragraph..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_paragraph_cb) },

	{ "context-properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Rule..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_rule_cb) },

	{ "context-properties-table",
	  NULL,
	  N_("Table..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_properties_table_cb) },

	{ "context-properties-text",
	  NULL,
	  N_("Text..."),
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

static void
editor_actions_setup_languages_menu (EHTMLEditor *editor)
{
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	GList *list = NULL, *link;
	guint merge_id;

	manager = editor->priv->manager;
	action_group = editor->priv->language_actions;
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	merge_id = gtk_ui_manager_new_merge_id (manager);

	list = e_spell_checker_list_available_dicts (spell_checker);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary = link->data;
		GtkToggleAction *action;
		const gchar *language_name;
		GString *escaped_name = NULL;
		gboolean active = FALSE;

		language_name = e_spell_dictionary_get_name (dictionary);
		if (language_name && strchr (language_name, '_') != NULL)
			escaped_name = e_str_replace_string (language_name, "_", "__");

		action = gtk_toggle_action_new (
			e_spell_dictionary_get_code (dictionary),
			escaped_name ? escaped_name->str : language_name,
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

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/main-menu/edit-menu/language-menu",
			e_spell_dictionary_get_code (dictionary),
			e_spell_dictionary_get_code (dictionary),
			GTK_UI_MANAGER_AUTO, FALSE);
	}

	g_list_free (list);
	g_clear_object (&spell_checker);
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
	action_group = editor->priv->spell_check_actions;;
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

		/* Visibility is dependent on whether the
		 * corresponding language action is active. */
		gtk_action_set_visible (action, FALSE);

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
editor_actions_init (EHTMLEditor *editor)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;

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
		TRUE,
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
}

void
editor_actions_bind (EHTMLEditor *editor)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	EContentEditor *cnt_editor;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	cnt_editor = e_html_editor_get_content_editor (editor);

	action_group = editor->priv->core_editor_actions;
	action = gtk_action_group_get_action (action_group, "mode-html");
	e_binding_bind_property (
		cnt_editor, "html-mode",
		action, "current-value",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	/* Synchronize widget mode with the buttons */
	e_content_editor_set_html_mode (cnt_editor, TRUE);

	e_binding_bind_property (
		cnt_editor, "can-redo",
		ACTION (REDO), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "can-undo",
		ACTION (UNDO), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "can-copy",
		ACTION (COPY), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "can-cut",
		ACTION (CUT), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "can-paste",
		ACTION (PASTE), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "can-paste",
		ACTION (PASTE_QUOTE), "sensitive",
		G_BINDING_SYNC_CREATE);

	/* This is connected to JUSTIFY_LEFT action only, but
	 * it automatically applies on all actions in the group. */
	e_binding_bind_property (
		cnt_editor, "alignment",
		ACTION (JUSTIFY_LEFT), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "bold",
		ACTION (BOLD), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "font-size",
		ACTION (FONT_SIZE_GROUP), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "block-format",
		ACTION (STYLE_NORMAL), "current-value",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "indented",
		ACTION (UNINDENT), "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "italic",
		ACTION (ITALIC), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "monospaced",
		ACTION (MONOSPACED), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "strikethrough",
		ACTION (STRIKETHROUGH), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	e_binding_bind_property (
		cnt_editor, "underline",
		ACTION (UNDERLINE), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	e_binding_bind_property (
		cnt_editor, "html-mode",
		editor->priv->html_actions, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Disable all actions and toolbars when editor is not editable */
	e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->core_editor_actions, "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->html_actions, "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->spell_check_actions, "sensitive",
		G_BINDING_SYNC_CREATE);
	e_binding_bind_property (
		cnt_editor, "editable",
		editor->priv->suggestion_actions, "sensitive",
		G_BINDING_SYNC_CREATE);
}

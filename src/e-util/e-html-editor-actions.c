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
action_copy_link_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_open_link_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	gpointer parent;

	if (!editor->priv->context_hover_uri)
		return;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	e_show_uri (parent, editor->priv->context_hover_uri);
}

static void
action_context_delete_cell_contents_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_cell_contents (cnt_editor);
}

static void
action_context_delete_column_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_column (cnt_editor);
}

static void
action_context_delete_row_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_row (cnt_editor);
}

static void
action_context_delete_table_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_table (cnt_editor);
}

static void
action_context_delete_hrule_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_h_rule (cnt_editor);
}

static void
action_context_delete_image_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_delete_image (cnt_editor);
}

static void
action_context_insert_column_after_cb (EUIAction *action,
				       GVariant *parameter,
				       gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_column_after (cnt_editor);
}

static void
action_context_insert_column_before_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_column_before (cnt_editor);
}

static void
action_context_insert_row_above_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_row_above (cnt_editor);
}

static void
action_context_insert_row_below_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_row_below (cnt_editor);
}

static void
action_context_remove_link_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_selection_unlink (cnt_editor);
}

static void
action_context_spell_add_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_context_spell_add_to_dict_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;
	ESpellChecker *spell_checker;
	ESpellDictionary *dictionary;
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_warn_if_fail (state != NULL);

	if (!state)
		return;

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	dictionary = e_spell_checker_ref_dictionary (spell_checker, g_variant_get_string (state, NULL));

	if (dictionary) {
		gchar *word;

		word = e_content_editor_get_caret_word (cnt_editor);
		if (word && *word)
			e_spell_dictionary_learn_word (dictionary, word, -1);
		g_free (word);
	}

	g_clear_object (&dictionary);
	g_clear_object (&spell_checker);
	g_variant_unref (state);
}

static void
action_context_spell_ignore_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_indent_cb (EUIAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_insert_emoji_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;
	GtkPopover *popover;
	GtkWidget *relative_to;
	GdkRectangle rect = { 0, 0, -1, -1 };

	if (editor->priv->emoji_chooser) {
		popover = GTK_POPOVER (editor->priv->emoji_chooser);
	} else {
		popover = GTK_POPOVER (e_gtk_emoji_chooser_new ());

		gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
		gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);

		g_signal_connect_object (popover, "emoji-picked",
			G_CALLBACK (emoji_chooser_emoji_picked_cb), editor, G_CONNECT_SWAPPED);

		editor->priv->emoji_chooser = GTK_WIDGET (popover);
	}

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_get_caret_client_rect (cnt_editor, &rect);

	if (rect.width >= 0 && rect.height >= 0 && rect.x + rect.width >= 0 && rect.y + rect.height >= 0) {
		relative_to = GTK_WIDGET (cnt_editor);
	} else {
		relative_to = GTK_WIDGET (editor);

		rect.x = 0;
		rect.y = 0;
		rect.width = gtk_widget_get_allocated_width (relative_to);
		rect.height = 0;
	}

	gtk_popover_set_relative_to (popover, relative_to);
	gtk_popover_set_pointing_to (popover, &rect);

	gtk_popover_popup (popover);
}

static void
action_insert_emoticon_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;
	const EEmoticon *emoticon;
	const gchar *icon_name;

	g_return_if_fail (parameter != NULL);

	icon_name = g_variant_get_string (parameter, NULL);
	emoticon = e_emoticon_chooser_lookup_emoticon (icon_name);

	g_return_if_fail (emoticon != NULL);

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_insert_emoticon (cnt_editor, emoticon);
}

static void
action_insert_html_file_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_insert_image_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_insert_link_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (!editor->priv->link_popover)
		editor->priv->link_popover = e_html_editor_link_popover_new (editor);

	e_html_editor_link_popover_popup (E_HTML_EDITOR_LINK_POPOVER (editor->priv->link_popover));
}

static void
action_insert_rule_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->hrule_dialog == NULL)
		editor->priv->hrule_dialog =
			e_html_editor_hrule_dialog_new (editor);

	gtk_window_present (GTK_WINDOW (editor->priv->hrule_dialog));
}

static void
action_insert_table_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->table_dialog == NULL)
		editor->priv->table_dialog =
			e_html_editor_table_dialog_new (editor);

	gtk_window_present (GTK_WINDOW (editor->priv->table_dialog));
}

static void
action_insert_text_file_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_language_notify_active_cb (EUIAction *action,
				  GParamSpec *param,
				  gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	const gchar *language_code;
	EUIAction *add_action;
	gchar action_name[128];
	gboolean active;

	e_ui_menu_freeze (editor->priv->main_menu);

	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	language_code = g_action_get_name (G_ACTION (action));

	active = e_ui_action_get_active (action);
	e_spell_checker_set_language_active (spell_checker, language_code, active);
	g_clear_object (&spell_checker);

	/* Update "Add Word To" context menu item visibility. */
	g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "context-spell-add-%s", language_code) < sizeof (action_name));
	add_action = e_html_editor_get_action (editor, action_name);
	e_ui_action_set_visible (add_action, active);

	e_html_editor_update_spell_actions (editor);

	g_signal_emit_by_name (editor, "spell-languages-changed");

	if (active) {
		GSettings *settings;
		GPtrArray *array;
		gchar **strv;
		gint ii, max_items;

		g_menu_remove_all (editor->priv->recent_languages_menu);

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

	e_ui_menu_thaw (editor->priv->main_menu);
}

static gboolean
update_mode_combobox (gpointer data)
{
	GWeakRef *weak_ref = data;
	EHTMLEditor *editor;
	EContentEditorMode mode;
	EUIAction *action;

	editor = g_weak_ref_get (weak_ref);
	if (!editor)
		return FALSE;

	mode = e_html_editor_get_mode (editor);

	action = e_ui_action_group_get_action (editor->priv->core_editor_actions, "mode-html");
	e_ui_action_set_state (action, g_variant_new_int32 (mode));

	g_object_unref (editor);

	return FALSE;
}

static void
html_editor_actions_notify_mode_cb (EHTMLEditor *editor,
				    GParamSpec *param,
				    gpointer user_data)
{
	gboolean is_html;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	is_html = e_html_editor_get_mode (editor) == E_CONTENT_EDITOR_MODE_HTML;

	e_ui_manager_freeze (editor->priv->ui_manager);

	g_object_set (G_OBJECT (editor->priv->html_actions), "sensitive", is_html, NULL);

	/* This must be done from idle callback, because apparently we can change
	 * current value in callback of current value change */
	g_idle_add_full (G_PRIORITY_HIGH_IDLE, update_mode_combobox, e_weak_ref_new (editor), (GDestroyNotify) e_weak_ref_free);

	e_ui_action_group_set_visible (editor->priv->html_actions, is_html);
	e_ui_action_group_set_visible (editor->priv->html_context_actions, is_html);

	if (is_html && gtk_widget_get_visible (editor->priv->edit_toolbar)) {
		gtk_widget_show (editor->priv->html_toolbar);
	} else {
		gtk_widget_hide (editor->priv->html_toolbar);
	}

	/* Certain paragraph styles are HTML-only. */
	e_ui_action_set_sensitive (ACTION (STYLE_H1), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H1), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_H2), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H2), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_H3), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H3), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_H4), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H4), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_H5), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H5), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_H6), is_html);
	e_ui_action_set_visible (ACTION (STYLE_H6), is_html);
	e_ui_action_set_sensitive (ACTION (STYLE_ADDRESS), is_html);
	e_ui_action_set_visible (ACTION (STYLE_ADDRESS), is_html);

	e_html_editor_emit_after_mode_changed (editor);

	e_ui_manager_thaw (editor->priv->ui_manager);
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
action_paste_as_text_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_paste_quote_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EHTMLEditor *editor = user_data;
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
action_properties_cell_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->cell_dialog == NULL) {
		editor->priv->cell_dialog =
			e_html_editor_cell_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->cell_dialog));
}

static void
action_properties_image_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->image_dialog == NULL) {
		editor->priv->image_dialog =
			e_html_editor_image_dialog_new (editor);
	}

	e_html_editor_image_dialog_show (
		E_HTML_EDITOR_IMAGE_DIALOG (editor->priv->image_dialog));
}

static void
action_properties_link_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (!editor->priv->link_popover)
		editor->priv->link_popover = e_html_editor_link_popover_new (editor);

	e_html_editor_link_popover_popup (E_HTML_EDITOR_LINK_POPOVER (editor->priv->link_popover));
}

static void
action_properties_page_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->page_dialog == NULL) {
		editor->priv->page_dialog =
			e_html_editor_page_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->page_dialog));
}

static void
action_properties_paragraph_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->paragraph_dialog == NULL) {
		editor->priv->paragraph_dialog =
			e_html_editor_paragraph_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->paragraph_dialog));
}

static void
action_properties_rule_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->hrule_dialog == NULL) {
		editor->priv->hrule_dialog =
			e_html_editor_hrule_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->hrule_dialog));
}

static void
action_properties_table_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->table_dialog == NULL) {
		editor->priv->table_dialog =
			e_html_editor_table_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->table_dialog));
}

static void
action_properties_text_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->text_dialog == NULL) {
		editor->priv->text_dialog =
			e_html_editor_text_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->text_dialog));
}

static void
action_redo_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_redo (cnt_editor);
}

static void
action_show_find_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->find_dialog == NULL) {
		editor->priv->find_dialog = e_html_editor_find_dialog_new (editor);
		e_ui_action_set_sensitive (ACTION (FIND_AGAIN), TRUE);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->find_dialog));
}

static void
action_find_again_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->find_dialog == NULL)
		return;

	e_html_editor_find_dialog_find_next (
		E_HTML_EDITOR_FIND_DIALOG (editor->priv->find_dialog));
}

static void
action_show_replace_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->replace_dialog == NULL) {
		editor->priv->replace_dialog =
			e_html_editor_replace_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->replace_dialog));
}

static void
action_spell_check_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	if (editor->priv->spell_check_dialog == NULL) {
		editor->priv->spell_check_dialog =
			e_html_editor_spell_check_dialog_new (editor);
	}

	gtk_window_present (GTK_WINDOW (editor->priv->spell_check_dialog));
}

static void
action_undo_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor))) {
		e_content_editor_undo (cnt_editor);
	}
}


static void
action_zoom_100_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	e_content_editor_set_zoom_level (cnt_editor, 1.0);
}

void
e_html_editor_zoom_in (EHTMLEditor *editor,
		       EContentEditor *cnt_editor)
{
	gdouble level;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (E_IS_CONTENT_EDITOR (cnt_editor));

	level = e_content_editor_get_zoom_level (cnt_editor);

	if (level <= 0.0)
		return;

	level += 0.1;
	if (level < 4.9999)
		e_content_editor_set_zoom_level (cnt_editor, level);
}

void
e_html_editor_zoom_out (EHTMLEditor *editor,
			EContentEditor *cnt_editor)
{
	gdouble level;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));
	g_return_if_fail (E_IS_CONTENT_EDITOR (cnt_editor));

	level = e_content_editor_get_zoom_level (cnt_editor);

	if (level <= 0.0)
		return;

	level -= 0.1;
	if (level > 0.7999)
		e_content_editor_set_zoom_level (cnt_editor, level);
}

static void
action_zoom_in_cb (EUIAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);

	e_html_editor_zoom_in (editor, cnt_editor);
}

static void
action_zoom_out_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);

	e_html_editor_zoom_out (editor, cnt_editor);
}

static void
action_unindent_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_selection_unindent (cnt_editor);
}

static void
action_wrap_lines_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EHTMLEditor *editor = user_data;
	EContentEditor *cnt_editor;

	cnt_editor = e_html_editor_get_content_editor (editor);
	if (e_html_editor_action_can_run (GTK_WIDGET (cnt_editor)))
		e_content_editor_selection_wrap (cnt_editor);
}

/* This is when the user toggled the action */
static void
manage_format_subsuperscript_toggled (EHTMLEditor *editor,
				      EUIAction *changed_action,
				      const gchar *prop_name,
				      EUIAction *second_action)
{
	EContentEditor *cnt_editor = e_html_editor_get_content_editor (editor);

	g_signal_handlers_block_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	if (e_ui_action_get_active (changed_action) &&
	    e_ui_action_get_active (second_action))
		e_ui_action_set_active (second_action, FALSE);

	g_object_set (G_OBJECT (cnt_editor), prop_name, e_ui_action_get_active (changed_action), NULL);

	g_signal_handlers_unblock_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

/* This is when the content editor claimed change on the property */
static void
manage_format_subsuperscript_notify (EHTMLEditor *editor,
				     EUIAction *changed_action,
				     const gchar *prop_name,
				     EUIAction *second_action)
{
	EContentEditor *cnt_editor = e_html_editor_get_content_editor (editor);
	gboolean value = FALSE;

	g_signal_handlers_block_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_block_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	g_object_get (G_OBJECT (cnt_editor), prop_name, &value, NULL);

	e_ui_action_set_active (changed_action, value);

	if (e_ui_action_get_active (changed_action) &&
	    e_ui_action_get_active (second_action))
		e_ui_action_set_active (second_action, FALSE);

	g_signal_handlers_unblock_matched (second_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (changed_action, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_unblock_matched (cnt_editor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

static void
html_editor_actions_subscript_toggled_cb (EUIAction *action,
					  GParamSpec *param,
					  gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_toggled (editor, ACTION (SUBSCRIPT), "subscript", ACTION (SUPERSCRIPT));
}

static void
html_editor_actions_notify_subscript_cb (EContentEditor *cnt_editor,
					 GParamSpec *param,
					 EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_notify (editor, ACTION (SUBSCRIPT), "subscript", ACTION (SUPERSCRIPT));
}

static void
html_editor_actions_superscript_toggled_cb (EUIAction *action,
					    GParamSpec *param,
					    gpointer user_data)
{
	EHTMLEditor *editor = user_data;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_toggled (editor, ACTION (SUPERSCRIPT), "superscript", ACTION (SUBSCRIPT));
}

static void
html_editor_actions_notify_superscript_cb (EContentEditor *cnt_editor,
					   GParamSpec *param,
					   EHTMLEditor *editor)
{
	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	manage_format_subsuperscript_notify (editor, ACTION (SUPERSCRIPT), "superscript", ACTION (SUBSCRIPT));
}


/*****************************************************************************
 * Core Actions
 *
 * These actions are always enabled.
 *****************************************************************************/

static const EUIActionEntry core_entries[] = {

	{ "copy",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy selected text to the clipboard"),
	  NULL, NULL, NULL, NULL }, /* Handled by focus tracker */

	{ "cut",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut selected text to the clipboard"),
	  NULL, NULL, NULL, NULL }, /* Handled by focus tracker */

	{ "paste",
	  "edit-paste",
	  N_("_Paste"),
	  "<Control>v",
	  N_("Paste text from the clipboard"),
	  NULL, NULL, NULL, NULL }, /* Handled by focus tracker */

	{ "redo",
	  "edit-redo",
	  N_("_Redo"),
	  "<Shift><Control>z",
	  N_("Redo the last undone action"),
	  action_redo_cb, NULL, NULL, NULL },

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  "<Control>a",
	  NULL,
	  NULL, NULL, NULL, NULL }, /* Handled by focus tracker */

	{ "undo",
	  "edit-undo",
	  N_("_Undo"),
	  "<Control>z",
	  N_("Undo the last action"),
	  action_undo_cb, NULL, NULL, NULL },

	{ "zoom-100",
	  "zoom-original",
	  N_("_Normal Size"),
	  "<Alt><Control>0",
	  N_("Reset the text to its original size"),
	  action_zoom_100_cb, NULL, NULL, NULL },

	{ "zoom-in",
	  "zoom-in",
	  N_("_Zoom In"),
	  "<Control>plus",
	  N_("Increase the text size"),
	  action_zoom_in_cb, NULL, NULL, NULL },

	{ "zoom-out",
	  "zoom-out",
	  N_("Zoom _Out"),
	  "<Control>minus",
	  N_("Decrease the text size"),
	  action_zoom_out_cb, NULL, NULL, NULL },

	/* Menus */

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "format-menu",
	  NULL,
	  N_("For_mat"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "paragraph-style-menu",
	  NULL,
	  N_("_Paragraph Style"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "insert-menu",
	  NULL,
	  N_("_Insert"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "justify-menu",
	  NULL,
	  N_("_Alignment"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "language-menu",
	  NULL,
	  N_("Current _Languages"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "zoom-menu", NULL, N_("_Zoom"), NULL, NULL, NULL, NULL, NULL, NULL },

	/* fake actions, related to dynamic items */

	{ "EHTMLEditor::recent-languages",
	  NULL,
	  N_("Recent spell check languages"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::all-languages",
	  NULL,
	  N_("All spell check languages"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::editing-mode",
	  NULL,
	  N_("Editing Mode"),
	  NULL,
	  N_("Editing Mode"),
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::paragraph-style",
	  NULL,
	  N_("Paragraph Style"),
	  NULL,
	  N_("Paragraph Style"),
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::font-name",
	  NULL,
	  N_("Font Name"),
	  NULL,
	  N_("Font Name"),
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::font-size",
	  NULL,
	  N_("Font Size"),
	  NULL,
	  N_("Font Size"),
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::font-color",
	  NULL,
	  N_("Font Color"),
	  NULL,
	  N_("Font Color"),
	  NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::background-color",
	  NULL,
	  N_("Background Color"),
	  NULL,
	  N_("Background Color"),
	  NULL, NULL, NULL, NULL }
};

static const EUIActionEntry core_editor_entries[] = {

	{ "indent",
	  "format-indent-more",
	  N_("_Increase Indent"),
	  "<Control>bracketright",
	  N_("Increase Indent"),
	  action_indent_cb, NULL, NULL, NULL },

	{ "insert-emoji",
	  NULL,
	  N_("E_moji"),
	  NULL,
	  N_("Insert Emoji"),
	  action_insert_emoji_cb, NULL, NULL, NULL },

	{ "EHTMLEditor::insert-emoticon",
	  "face-smile",
	  N_("_Emoticon"),
	  NULL,
	  N_("Insert Emoticon"),
	  action_insert_emoticon_cb, "s", NULL, NULL },

	{ "insert-emoji-toolbar",
	  "face-smile",
	  N_("Insert E_moji"),
	  NULL,
	  N_("Insert Emoji"),
	  action_insert_emoji_cb, NULL, NULL, NULL },

	{ "insert-html-file",
	  NULL,
	  N_("_HTML File…"),
	  NULL,
	  NULL,
	  action_insert_html_file_cb, NULL, NULL, NULL },

	{ "insert-text-file",
	  NULL,
	  N_("Te_xt File…"),
	  NULL,
	  NULL,
	  action_insert_text_file_cb, NULL, NULL, NULL },

	{ "paste-quote",
	  NULL,
	  N_("Paste _Quotation"),
	  "<Control><Alt>v",
	  NULL,
	  action_paste_quote_cb, NULL, NULL, NULL },

	{ "show-find",
	  "edit-find",
	  N_("_Find…"),
	  "<Control>f",
	  N_("Search for text"),
	  action_show_find_cb, NULL, NULL, NULL },

	{ "find-again",
	  NULL,
	  N_("Find A_gain"),
	  "<Control>g",
	  NULL,
	  action_find_again_cb, NULL, NULL, NULL },

	{ "show-replace",
	  "edit-find-replace",
	  N_("Re_place…"),
	  "<Control>h",
	  N_("Search for and replace text"),
	  action_show_replace_cb, NULL, NULL, NULL },

	{ "spell-check",
	  "tools-check-spelling",
	  N_("Check _Spelling…"),
	  "F7",
	  NULL,
	  action_spell_check_cb, NULL, NULL, NULL },

	{ "unindent",
	  "format-indent-less",
	  N_("_Decrease Indent"),
	  "<Control>bracketleft",
	  N_("Decrease Indent"),
	  action_unindent_cb, NULL, NULL, NULL },

	{ "wrap-lines",
	  NULL,
	  N_("_Wrap Lines"),
	  "<Control><Shift>k",
	  NULL,
	  action_wrap_lines_cb, NULL, NULL, NULL }
};

static const EUIActionEnumEntry core_justify_entries[] = {

	{ "justify-center",
	  "format-justify-center",
	  N_("_Center"),
	  "<Control>e",
	  N_("Center Alignment"),
	  NULL, E_CONTENT_EDITOR_ALIGNMENT_CENTER },

	{ "justify-fill",
	  "format-justify-fill",
	  N_("_Justified"),
	  "<Control>j",
	  N_("Align Justified"),
	  NULL, E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY },

	{ "justify-left",
	  "format-justify-left",
	  N_("_Left"),
	  "<Control>l",
	  N_("Left Alignment"),
	  NULL, E_CONTENT_EDITOR_ALIGNMENT_LEFT },

	{ "justify-right",
	  "format-justify-right",
	  N_("_Right"),
	  "<Control>r",
	  N_("Right Alignment"),
	  NULL, E_CONTENT_EDITOR_ALIGNMENT_RIGHT }
};

static const EUIActionEnumEntry core_mode_entries[] = {

	{ "mode-html",
	  NULL,
	  N_("_HTML"),
	  NULL,
	  N_("HTML editing mode"),
	  NULL, E_CONTENT_EDITOR_MODE_HTML },

	{ "mode-plain",
	  NULL,
	  N_("Plain _Text"),
	  NULL,
	  N_("Plain text editing mode"),
	  NULL, E_CONTENT_EDITOR_MODE_PLAIN_TEXT },

	{ "mode-markdown",
	  NULL,
	  N_("_Markdown"),
	  NULL,
	  N_("Markdown editing mode"),
	  NULL, E_CONTENT_EDITOR_MODE_MARKDOWN },

	{ "mode-markdown-plain",
	  NULL,
	  N_("Ma_rkdown as Plain Text"),
	  NULL,
	  N_("Markdown editing mode, exported as Plain Text"),
	  NULL, E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT },

	{ "mode-markdown-html",
	  NULL,
	  N_("Mar_kdown as HTML"),
	  NULL,
	  N_("Markdown editing mode, exported as HTML"),
	  NULL, E_CONTENT_EDITOR_MODE_MARKDOWN_HTML }
};

static const EUIActionEnumEntry core_style_entries[] = {

	{ "style-normal",
	  NULL,
	  N_("_Normal"),
	  "<Control>0",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH },

	{ "style-h1",
	  NULL,
	  N_("Heading _1"),
	  "<Control>1",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H1 },

	{ "style-h2",
	  NULL,
	  N_("Heading _2"),
	  "<Control>2",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H2 },

	{ "style-h3",
	  NULL,
	  N_("Heading _3"),
	  "<Control>3",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H3 },

	{ "style-h4",
	  NULL,
	  N_("Heading _4"),
	  "<Control>4",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H4 },

	{ "style-h5",
	  NULL,
	  N_("Heading _5"),
	  "<Control>5",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H5 },

	{ "style-h6",
	  NULL,
	  N_("Heading _6"),
	  "<Control>6",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_H6 },

        { "style-preformat",
          NULL,
          N_("_Preformatted"),
          "<Control>7",
          NULL,
          NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_PRE },

	{ "style-address",
	  NULL,
	  N_("A_ddress"),
	  "<Control>8",
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS },

	{ "style-list-bullet",
	  NULL,
	  N_("_Bulleted List"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST },

	{ "style-list-roman",
	  NULL,
	  N_("_Roman Numeral List"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN },

	{ "style-list-number",
	  NULL,
	  N_("Numbered _List"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST },

	{ "style-list-alpha",
	  NULL,
	  N_("_Alphabetical List"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA }
};

/*****************************************************************************
 * Core Actions (HTML only)
 *
 * These actions are only enabled in HTML mode.
 *****************************************************************************/

static const EUIActionEntry html_entries[] = {

	{ "insert-image",
	  "insert-image",
	  N_("_Image…"),
	  NULL,
	  /* Translators: This is an action tooltip */
	  N_("Insert Image"),
	  action_insert_image_cb, NULL, NULL, NULL },

	{ "insert-link",
	  "insert-link",
	  N_("_Link…"),
	  "<Control>k",
	  N_("Insert Link"),
	  action_insert_link_cb, NULL, NULL, NULL },

	{ "insert-rule",
	  "stock_insert-rule",
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule…"),
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Insert Rule"),
	  action_insert_rule_cb, NULL, NULL, NULL },

	{ "insert-table",
	  "stock_insert-table",
	  N_("_Table…"),
	  NULL,
	  N_("Insert Table"),
	  action_insert_table_cb, NULL, NULL, NULL },

	{ "properties-cell",
	  NULL,
	  N_("_Cell…"),
	  NULL,
	  NULL,
	  action_properties_cell_cb, NULL, NULL, NULL },

	{ "properties-image",
	  NULL,
	  N_("_Image…"),
	  NULL,
	  NULL,
	  action_properties_image_cb, NULL, NULL, NULL },

	{ "properties-link",
	  NULL,
	  N_("_Link…"),
	  NULL,
	  NULL,
	  action_properties_link_cb, NULL, NULL, NULL },

	{ "properties-page",
	  NULL,
	  N_("Pa_ge…"),
	  NULL,
	  NULL,
	  action_properties_page_cb, NULL, NULL, NULL },

	{ "properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("_Rule…"),
	  NULL,
	  NULL,
	  action_properties_rule_cb, NULL, NULL, NULL },

	{ "properties-table",
	  NULL,
	  N_("_Table…"),
	  NULL,
	  NULL,
	  action_properties_table_cb, NULL, NULL, NULL },

	/* Menus */

	{ "font-size-menu",
	  NULL,
	  N_("Font _Size"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "font-style-menu",
	  NULL,
	  N_("_Font Style"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "paste-as-text",
	  NULL,
	  N_("Paste As _Text"),
	  "<Shift><Control>v",
	  NULL,
	  action_paste_as_text_cb, NULL, NULL, NULL },

};

static const EUIActionEntry html_toggle_entries[] = {

	{ "bold",
	  "format-text-bold",
	  N_("_Bold"),
	  "<Control>b",
	  N_("Bold"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

	{ "italic",
	  "format-text-italic",
	  N_("_Italic"),
	  "<Control>i",
	  N_("Italic"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

	{ "strikethrough",
	  "format-text-strikethrough",
	  N_("_Strikethrough"),
	  NULL,
	  N_("Strikethrough"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

	{ "subscript",
	  NULL,
	  N_("Subs_cript"),
	  "<Control><Shift>b",
	  N_("Subscript"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

	{ "superscript",
	  NULL,
	  N_("Su_perscript"),
	  "<Control><Shift>p",
	  N_("Superscript"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

	{ "underline",
	  "format-text-underline",
	  N_("_Underline"),
	  "<Control>u",
	  N_("Underline"),
	  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state }
};

static const EUIActionEnumEntry html_size_entries[] = {

	{ "size-minus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-2"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_TINY },

	{ "size-minus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-1"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_SMALL },

	{ "size-plus-zero",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+0"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_NORMAL },

	{ "size-plus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+1"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_BIG },

	{ "size-plus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+2"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_BIGGER },

	{ "size-plus-three",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+3"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_LARGE },

	{ "size-plus-four",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+4"),
	  NULL,
	  NULL,
	  NULL, E_CONTENT_EDITOR_FONT_SIZE_VERY_LARGE }
};

/*****************************************************************************
 * Context Menu Actions
 *
 * These require separate action entries so we can toggle their visiblity
 * rather than their sensitivity as we do with main menu / toolbar actions.
 * Note that some of these actions use the same callback function as their
 * non-context sensitive counterparts.
 *****************************************************************************/

static const EUIActionEntry context_entries[] = {

	{ "context-copy-link",
	  "edit-copy",
	  N_("Copy _Link Location"),
	  NULL,
	  N_("Copy the link to the clipboard"),
	  action_copy_link_cb, NULL, NULL, NULL },

	{ "context-open-link",
	  "emblem-web",
	  N_("_Open Link in Browser"),
	  NULL,
	  N_("Open the link in a web browser"),
	  action_open_link_cb, NULL, NULL, NULL },

	{ "context-delete-cell",
	  NULL,
	  N_("Cell Contents"),
	  NULL,
	  NULL,
	  action_context_delete_cell_contents_cb, NULL, NULL, NULL },

	{ "context-delete-column",
	  NULL,
	  N_("Column"),
	  NULL,
	  NULL,
	  action_context_delete_column_cb, NULL, NULL, NULL },

	{ "context-delete-row",
	  NULL,
	  N_("Row"),
	  NULL,
	  NULL,
	  action_context_delete_row_cb, NULL, NULL, NULL },

	{ "context-delete-table",
	  NULL,
	  N_("Table"),
	  NULL,
	  NULL,
	  action_context_delete_table_cb, NULL, NULL, NULL },

	/* Menus */

	{ "context-delete-table-menu",
	  NULL,
	  /* Translators: Popup menu item caption, containing all the Delete options for a table */
	  N_("Table Delete"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "context-insert-table-menu",
	  NULL,
	  /* Translators: Popup menu item caption, containing all the Insert options for a table */
	  N_("Table Insert"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	{ "context-properties-menu",
	  NULL,
	  N_("Properties"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL }
};

/*****************************************************************************
 * Context Menu Actions (HTML only)
 *
 * These actions are never visible in plain-text mode.  Note that some
 * of them use the same callback function as their non-context sensitive
 * counterparts.
 *****************************************************************************/

static const EUIActionEntry html_context_entries[] = {

	{ "context-delete-hrule",
	  NULL,
	  N_("Delete Rule"),
	  NULL,
	  NULL,
	  action_context_delete_hrule_cb, NULL, NULL, NULL },

	{ "context-delete-image",
	  NULL,
	  N_("Delete Image"),
	  NULL,
	  NULL,
	  action_context_delete_image_cb, NULL, NULL, NULL },

	{ "context-insert-column-after",
	  NULL,
	  N_("Column After"),
	  NULL,
	  NULL,
	  action_context_insert_column_after_cb, NULL, NULL, NULL },

	{ "context-insert-column-before",
	  NULL,
	  N_("Column Before"),
	  NULL,
	  NULL,
	  action_context_insert_column_before_cb, NULL, NULL, NULL },

	{ "context-insert-link",
	  NULL,
	  N_("Insert _Link"),
	  NULL,
	  NULL,
	  action_insert_link_cb, NULL, NULL, NULL },

	{ "context-insert-row-above",
	  NULL,
	  N_("Row Above"),
	  NULL,
	  NULL,
	  action_context_insert_row_above_cb, NULL, NULL, NULL },

	{ "context-insert-row-below",
	  NULL,
	  N_("Row Below"),
	  NULL,
	  NULL,
	  action_context_insert_row_below_cb, NULL, NULL, NULL },

	{ "context-properties-cell",
	  NULL,
	  N_("Cell…"),
	  NULL,
	  NULL,
	  action_properties_cell_cb, NULL, NULL, NULL },

	{ "context-properties-image",
	  NULL,
	  N_("Image…"),
	  NULL,
	  NULL,
	  action_properties_image_cb, NULL, NULL, NULL },

	{ "context-properties-link",
	  NULL,
	  N_("Link…"),
	  NULL,
	  NULL,
	  action_properties_link_cb, NULL, NULL, NULL },

	{ "context-properties-page",
	  NULL,
	  N_("Page…"),
	  NULL,
	  NULL,
	  action_properties_page_cb, NULL, NULL, NULL },

	{ "context-properties-paragraph",
	  NULL,
	  N_("Paragraph…"),
	  NULL,
	  NULL,
	  action_properties_paragraph_cb, NULL, NULL, NULL },

	{ "context-properties-rule",
	  NULL,
	  /* Translators: 'Rule' here means a horizontal line in an HTML text */
	  N_("Rule…"),
	  NULL,
	  NULL,
	  action_properties_rule_cb, NULL, NULL, NULL },

	{ "context-properties-table",
	  NULL,
	  N_("Table…"),
	  NULL,
	  NULL,
	  action_properties_table_cb, NULL, NULL, NULL },

	{ "context-properties-text",
	  NULL,
	  N_("Text…"),
	  NULL,
	  NULL,
	  action_properties_text_cb, NULL, NULL, NULL },

	{ "context-remove-link",
	  NULL,
	  N_("Remove Link"),
	  NULL,
	  NULL,
	  action_context_remove_link_cb, NULL, NULL, NULL }
};

/*****************************************************************************
 * Context Menu Actions (spell checking only)
 *
 * These actions are only visible when the word underneath the cursor is
 * misspelled.
 *****************************************************************************/

static const EUIActionEntry spell_context_entries[] = {

	{ "context-spell-add",
	  NULL,
	  N_("Add Word to Dictionary"),
	  NULL,
	  NULL,
          action_context_spell_add_cb, NULL, NULL, NULL },

	{ "context-spell-ignore",
	  NULL,
	  N_("Ignore Misspelled Word"),
	  NULL,
	  NULL,
	  action_context_spell_ignore_cb, NULL, NULL, NULL },

	{ "EHTMLEditor::context-spell-add-menu",
	  NULL,
	  N_("Add Word To"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL },

	/* Menus */

	{ "EHTMLEditor::context-spell-suggest",
	  NULL,
	  N_("Spell check suggestions"),
	  NULL,
	  NULL,
          NULL, NULL, NULL, NULL },

	{ "EHTMLEditor::context-spell-suggest-more-menu",
	  NULL,
	  N_("More Suggestions"),
	  NULL,
	  NULL,
	  NULL, NULL, NULL, NULL }
};

static void
editor_actions_setup_emoticon_menu (EHTMLEditor *editor)
{
	GList *list, *link;

	list = e_emoticon_chooser_get_items ();

	for (link = list; link; link = g_list_next (link)) {
		EEmoticon *emoticon = link->data;
		GMenuItem *item;

		item = g_menu_item_new (_(emoticon->label), NULL);
		g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_ACTION, "s", "core-editor.EHTMLEditor::insert-emoticon");
		g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_TARGET, "s", emoticon->icon_name);
		g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_ICON, "s", emoticon->icon_name);

		g_menu_append_item (editor->priv->emoticon_menu, item);

		g_clear_object (&item);
	}

	g_list_free (list);
}

static gboolean
editor_actions_add_to_recent_languages (EHTMLEditor *editor,
					const gchar *language_code)
{
	EUIAction *language_action;
	GMenuModel *menu_model;
	GMenuItem *menu_item;
	const gchar *language_action_name;
	guint ii, n_items;
	guint group_name_len_plus_one, action_name_len;

	g_return_val_if_fail (E_IS_HTML_EDITOR (editor), FALSE);
	g_return_val_if_fail (language_code != NULL, FALSE);

	language_action = e_ui_action_group_get_action (editor->priv->language_actions, language_code);
	if (!language_action)
		return FALSE;

	group_name_len_plus_one = strlen (e_ui_action_group_get_name (editor->priv->language_actions)) + 1;
	action_name_len = strlen (g_action_get_name (G_ACTION (language_action)));
	language_action_name = g_action_get_name (G_ACTION (language_action));

	menu_model = G_MENU_MODEL (editor->priv->recent_languages_menu);
	n_items = g_menu_model_get_n_items (menu_model);
	for (ii = 0; ii < n_items; ii++) {
		GVariant *value;
		const gchar *action_full_name;

		value = g_menu_model_get_item_attribute_value (menu_model, ii, G_MENU_ATTRIBUTE_ACTION, G_VARIANT_TYPE_STRING);
		if (!value || !g_variant_get_string (value, NULL)) {
			g_clear_pointer (&value, g_variant_unref);
			continue;
		}

		action_full_name = g_variant_get_string (value, NULL);

		if (strlen (action_full_name) == group_name_len_plus_one + action_name_len &&
		    g_str_has_suffix (action_full_name, language_action_name) &&
		    g_str_has_prefix (action_full_name, e_ui_action_group_get_name (editor->priv->language_actions))) {
			g_clear_pointer (&value, g_variant_unref);
			break;
		}

		g_clear_pointer (&value, g_variant_unref);
	}

	/* it's already in the list */
	if (ii < n_items)
		return TRUE;

	menu_item = g_menu_item_new (NULL, NULL);
	e_ui_manager_update_item_from_action (editor->priv->ui_manager, menu_item, language_action);

	g_menu_append_item (editor->priv->recent_languages_menu, menu_item);

	g_clear_object (&menu_item);

	return TRUE;
}

static void
editor_actions_setup_languages_menu (EHTMLEditor *editor)
{
	ESpellChecker *spell_checker;
	EContentEditor *cnt_editor;
	EUIActionGroup *action_group;
	GHashTable *lang_parents; /* gchar *name ~> GMenu * (to add variants into a submenu) */
	GList *list = NULL, *link;
	GSettings *settings;
	gchar **strv;
	const gchar *map_name;
	gint ii, added = 0, max_items;

	lang_parents = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	action_group = editor->priv->language_actions;
	map_name = e_ui_action_group_get_name (action_group);
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);

	g_menu_remove_all (editor->priv->all_languages_menu);

	list = e_spell_checker_list_available_dicts (spell_checker);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESpellDictionary *dictionary = link->data;
		GMenu *parent_lang_menu;
		GMenuItem *menu_item;
		EUIAction *action;
		gchar *language_name;
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

		action = e_ui_action_group_get_action (action_group, e_spell_dictionary_get_code (dictionary));
		if (action) {
			g_object_ref (action);
		} else {
			const gchar *dictionay_name;
			GString *escaped_name = NULL;

			dictionay_name = e_spell_dictionary_get_name (dictionary);
			if (dictionay_name && strchr (dictionay_name, '_') != NULL)
				escaped_name = e_str_replace_string (dictionay_name, "_", "__");

			action = e_ui_action_new_stateful (map_name,
				e_spell_dictionary_get_code (dictionary), NULL,
				g_variant_new_boolean (FALSE));
			e_ui_action_set_label (action, escaped_name ? escaped_name->str : dictionay_name);

			if (escaped_name)
				g_string_free (escaped_name, TRUE);

			g_signal_connect_object (
				action, "change-state",
				G_CALLBACK (e_ui_action_set_state), editor, 0);

			g_signal_connect_object (
				action, "notify::active",
				G_CALLBACK (action_language_notify_active_cb), editor, 0);

			e_ui_action_group_add (action_group, action);
			e_ui_menu_track_action (editor->priv->main_menu, action);
		}

		active = e_spell_checker_get_language_active (spell_checker, e_spell_dictionary_get_code (dictionary));

		if ((!e_ui_action_get_active (action)) != (!active)) {
			g_signal_handlers_block_by_func (action, action_language_notify_active_cb, editor);
			e_ui_action_set_active (action, active);
			g_signal_handlers_unblock_by_func (action, action_language_notify_active_cb, editor);
		}

		parent_lang_menu = g_hash_table_lookup (lang_parents, language_name);
		if (!parent_lang_menu) {
			parent_lang_menu = g_menu_new ();

			menu_item = g_menu_item_new_submenu (language_name, G_MENU_MODEL (parent_lang_menu));

			g_hash_table_insert (lang_parents, g_strdup (language_name), parent_lang_menu);

			g_menu_append_item (editor->priv->all_languages_menu, menu_item);

			g_clear_object (&menu_item);
		}

		menu_item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (editor->priv->ui_manager, menu_item, action);

		g_menu_append_item (parent_lang_menu, menu_item);

		g_clear_object (&menu_item);
		g_clear_object (&action);
		g_free (language_name);
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
	EUIActionGroup *action_group;
	GList *available_dicts = NULL, *iter;
	const gchar *map_name;

	action_group = editor->priv->spell_check_actions;
	map_name = e_ui_action_group_get_name (action_group);
	cnt_editor = e_html_editor_get_content_editor (editor);
	spell_checker = e_content_editor_ref_spell_checker (cnt_editor);
	available_dicts = e_spell_checker_list_available_dicts (spell_checker);

	for (iter = available_dicts; iter; iter = iter->next) {
		ESpellDictionary *dictionary = iter->data;
		EUIAction *action;
		GMenu *menu;
		const gchar *code;
		const gchar *name;
		GString *escaped_name = NULL;
		gchar *action_label;
		gchar action_name[128];

		code = e_spell_dictionary_get_code (dictionary);
		name = e_spell_dictionary_get_name (dictionary);

		/* Add a suggestion menu. */
		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "context-spell-suggest-%s-menu", code) < sizeof (action_name));

		if (name && strchr (name, '_') != NULL)
			escaped_name = e_str_replace_string (name, "_", "__");

		action = e_ui_action_new (map_name, action_name, NULL);
		e_ui_action_set_label (action, escaped_name ? escaped_name->str : name);
		e_ui_action_set_visible (action, FALSE);
		e_ui_action_group_add (action_group, action);

		g_ptr_array_add (editor->priv->spell_suggest_actions, g_object_ref (action));

		menu = g_menu_new ();

		g_hash_table_insert (editor->priv->spell_suggest_menus_by_code, g_strdup (code),
			/* assumes ownership of both */
			e_html_editor_action_menu_pair_new (action, menu));

		/* Add an item to the "Add Word To" menu. */
		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "context-spell-add-%s", code) < sizeof (action_name));
		/* Translators: %s will be replaced with the actual dictionary
		 * name, where a user can add a word to. This is part of an
		 * "Add Word To" submenu. */
		action_label = g_strdup_printf (_("%s Dictionary"), escaped_name ? escaped_name->str : name);

		action = e_ui_action_new_stateful (map_name, action_name, NULL, g_variant_new_string (code));
		e_ui_action_set_label (action, action_label);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_add_to_dict_cb), editor);

		e_ui_action_set_visible (action, e_spell_checker_get_language_active (spell_checker, code));

		e_ui_action_group_add (action_group, action);
		/* the array assumes ownership of the action */
		g_ptr_array_add (editor->priv->spell_add_actions, action);

		g_free (action_label);

		if (escaped_name)
			g_string_free (escaped_name, TRUE);
	}

	g_list_free (available_dicts);
	g_clear_object (&spell_checker);
}

void
e_html_editor_actions_add_actions (EHTMLEditor *editor)
{
	EUIManager *ui_manager;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	ui_manager = e_html_editor_get_ui_manager (editor);

	/* Core Actions */
	e_ui_manager_add_actions (ui_manager, "core", NULL,
		core_entries, G_N_ELEMENTS (core_entries), editor);
	e_ui_manager_add_actions (ui_manager, "core-editor", NULL,
		core_editor_entries, G_N_ELEMENTS (core_editor_entries), editor);
	e_ui_manager_add_actions_enum (ui_manager, "core-editor", NULL,
		core_justify_entries, G_N_ELEMENTS (core_justify_entries), editor);
	e_ui_manager_add_actions_enum (ui_manager, "core-editor", NULL,
		core_mode_entries, G_N_ELEMENTS (core_mode_entries), editor);
	e_ui_manager_add_actions_enum (ui_manager, "core-editor", NULL,
		core_style_entries, G_N_ELEMENTS (core_style_entries), editor);

	/* Core Actions (HTML only) */
	e_ui_manager_add_actions (ui_manager, "html", NULL,
		html_entries, G_N_ELEMENTS (html_entries), editor);
	e_ui_manager_add_actions (ui_manager, "html", NULL,
		html_toggle_entries, G_N_ELEMENTS (html_toggle_entries), editor);
	e_ui_manager_add_actions_enum (ui_manager, "html", NULL,
		html_size_entries, G_N_ELEMENTS (html_size_entries), editor);

	/* Context Menu Actions */
	e_ui_manager_add_actions (ui_manager, "core-context", NULL,
		context_entries, G_N_ELEMENTS (context_entries), editor);

	/* Context Menu Actions (HTML only) */
	e_ui_manager_add_actions (ui_manager, "html-context", NULL,
		html_context_entries, G_N_ELEMENTS (html_context_entries), editor);

	/* Context Menu Actions (spell check only) */
	e_ui_manager_add_actions (ui_manager, "spell-check", NULL,
		spell_context_entries, G_N_ELEMENTS (spell_context_entries), editor);

	e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_MENU,
		"EHTMLEditor::recent-languages",
		"EHTMLEditor::all-languages",
		"EHTMLEditor::context-spell-suggest",
		"EHTMLEditor::context-spell-suggest-more-menu",
		"EHTMLEditor::context-spell-add-menu",
		"EHTMLEditor::insert-emoticon",
		"edit-menu",
		"file-menu",
		"format-menu",
		"paragraph-style-menu",
		"insert-menu",
		"justify-menu",
		"language-menu",
		"view-menu",
		"font-size-menu",
		"font-style-menu",
		"context-delete-table-menu",
		"context-insert-table-menu",
		"context-properties-menu",
		NULL);

	e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_TOOLBAR,
		"EHTMLEditor::editing-mode",
		"EHTMLEditor::paragraph-style",
		"EHTMLEditor::font-name",
		"EHTMLEditor::font-size",
		"EHTMLEditor::font-color",
		"EHTMLEditor::background-color",
		NULL);
}

void
e_html_editor_actions_setup_actions (EHTMLEditor *editor)
{
	EUIAction *action;
	EUIManager *ui_manager;
	guint ii;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	ui_manager = e_html_editor_get_ui_manager (editor);

	editor_actions_setup_emoticon_menu (editor);

	/* Language actions are generated dynamically. */
	editor_actions_setup_languages_menu (editor);

	/* Some spell check actions are generated dynamically. */
	editor_actions_setup_spell_check_menu (editor);

	/* Do this after all language actions are initialized. */
	e_html_editor_update_spell_actions (editor);

	e_ui_action_set_sensitive (ACTION (UNINDENT), FALSE);
	e_ui_action_set_sensitive (ACTION (FIND_AGAIN), FALSE);

	g_signal_connect_object (ACTION (SUBSCRIPT), "notify::active",
		G_CALLBACK (html_editor_actions_subscript_toggled_cb), editor, 0);
	g_signal_connect_object (ACTION (SUPERSCRIPT), "notify::active",
		G_CALLBACK (html_editor_actions_superscript_toggled_cb), editor, 0);
	g_signal_connect (editor, "notify::mode",
		G_CALLBACK (html_editor_actions_notify_mode_cb), NULL);

	action = e_ui_manager_get_action (ui_manager, "mode-html");
	e_binding_bind_property_full (
		editor, "mode",
		action, "state",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
		e_ui_action_util_gvalue_to_enum_state,
		e_ui_action_util_enum_state_to_gvalue, NULL, NULL);

	for (ii = 0; ii < G_N_ELEMENTS (core_mode_entries); ii++) {
		action = e_ui_manager_get_action (ui_manager, core_mode_entries[ii].name);

		e_ui_action_set_visible (action, e_html_editor_has_editor_for_mode (editor, core_mode_entries[ii].state));
	}
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
	EUIActionGroup *action_group;
	EUIAction *action;
	EUIManager *ui_manager;
	GPtrArray *actions;
	GtkWidget *widget;

	ui_manager = e_ui_manager_new (NULL);

	e_ui_manager_add_actions_enum (ui_manager, "core-mode-entries", NULL,
		core_mode_entries, G_N_ELEMENTS (core_mode_entries), NULL);

	action_group = e_ui_manager_get_action_group (ui_manager, "core-mode-entries");

	g_object_ref (action_group);
	g_clear_object (&ui_manager);

	actions = e_ui_action_group_list_actions (action_group);
	if (actions) {
		GPtrArray *group;
		guint ii;

		group = g_ptr_array_new ();

		for (ii = 0; ii < actions->len; ii++) {
			EUIAction *radio_action = g_ptr_array_index (actions, ii);
			e_ui_action_set_radio_group (radio_action, group);
		}

		g_ptr_array_unref (group);
		g_ptr_array_unref (actions);
	}

	action = e_ui_action_group_get_action (action_group, "mode-html");

	widget = e_action_combo_box_new_with_action (action);
	gtk_widget_set_focus_on_click (widget, FALSE);
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
	rb (e_binding_bind_property_full (
		cnt_editor, "alignment",
		ACTION (JUSTIFY_LEFT), "state",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
		e_ui_action_util_gvalue_to_enum_state,
		e_ui_action_util_enum_state_to_gvalue, NULL, NULL));
	rb (e_binding_bind_property (
		cnt_editor, "bold",
		ACTION (BOLD), "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
	rb (e_binding_bind_property_full (
		cnt_editor, "font-size",
		ACTION (FONT_SIZE_GROUP), "state",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
		e_ui_action_util_gvalue_to_enum_state,
		e_ui_action_util_enum_state_to_gvalue, NULL, NULL));
	rb (e_binding_bind_property_full (
		cnt_editor, "block-format",
		ACTION (STYLE_NORMAL), "state",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
		e_ui_action_util_gvalue_to_enum_state,
		e_ui_action_util_enum_state_to_gvalue, NULL, NULL));
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
	GPtrArray *actions;
	guint ii;

	g_return_if_fail (E_IS_HTML_EDITOR (editor));

	active = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (ii = 0; languages && languages[ii]; ii++) {
		g_hash_table_insert (active, g_strdup (languages[ii]), NULL);
	}

	actions = e_ui_action_group_list_actions (editor->priv->language_actions);
	for (ii = 0; actions && ii < actions->len; ii++) {
		EUIAction *action = g_ptr_array_index (actions, ii);
		gboolean is_active;

		is_active = g_hash_table_contains (active, g_action_get_name (G_ACTION (action)));

		if ((e_ui_action_get_active (action) ? 1 : 0) != (is_active ? 1 : 0)) {
			g_signal_handlers_block_by_func (action, action_language_notify_active_cb, editor);
			e_ui_action_set_active (action, is_active);
			g_signal_handlers_unblock_by_func (action, action_language_notify_active_cb, editor);
		}
	}

	g_clear_pointer (&actions, g_ptr_array_unref);
	g_hash_table_destroy (active);
}

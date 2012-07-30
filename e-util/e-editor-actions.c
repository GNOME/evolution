/* e-editor-actions.c
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

//#include "e-editor-private.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "e-editor.h"
#include "e-editor-actions.h"
#include "e-editor-private.h"
#include "e-editor-widgets.h"
#include "e-emoticon-action.h"
#include "e-image-chooser-dialog.h"

static void
insert_html_file_ready_cb (GFile *file,
                           GAsyncResult *result,
                           EEditor *editor)
{
	EEditorSelection *selection;
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

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_insert_html (selection, contents);
	g_free (contents);

	g_object_unref (editor);
}

static void
insert_text_file_ready_cb (GFile *file,
                           GAsyncResult *result,
                           EEditor *editor)
{
	EEditorSelection *selection;
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

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_insert_text (selection, contents);
	g_free (contents);

	g_object_unref (editor);
}

static void
replace_answer (EEditor *editor,
                EEditorWidgetReplaceAnswer answer)
{
	/* FIXME WEBKIT
	if (e_editor_widget_replace (e_editor_get_editor_widget (editor), answer))
		gtk_widget_hide (WIDGET (REPLACE_CONFIRMATION_WINDOW));
	*/
}

/*****************************************************************************
 * Action Callbacks
 *****************************************************************************/

static void
action_bold_cb (GtkToggleAction *action,
                EEditor *editor)
{
	EEditorSelection *selection;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_bold (
		selection, gtk_toggle_action_get_active (action));
}


static void
action_confirm_replace_cb (GtkAction *action,
                           EEditor *editor)
{
	replace_answer (editor, E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE);
}

static void
action_confirm_replace_all_cb (GtkAction *action,
                               EEditor *editor)
{
	replace_answer (editor, E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE_ALL);
}

static void
action_confirm_replace_cancel_cb (GtkAction *action,
                                  EEditor *editor)
{
	replace_answer (editor, E_EDITOR_WIDGET_REPLACE_ANSWER_CANCEL);
}

static void
action_confirm_replace_next_cb (GtkAction *action,
                                EEditor *editor)
{
	replace_answer (editor, E_EDITOR_WIDGET_REPLACE_ANSWER_NEXT);
}

static WebKitDOMNode *
find_parent_element_by_type (WebKitDOMNode *node, GType type)
{
	while (node) {

		if (g_type_is_a (type, webkit_dom_node_get_type ()))
			return node;

		node = webkit_dom_node_get_parent_node (node);
	}

	return NULL;
}

static void
action_context_delete_cell_cb (GtkAction *action,
                               EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *start, *end, *cell;
	gboolean single_row;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	/* Find TD in which the selection starts */
	start = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (start)) {
		start = find_parent_element_by_type (
			start, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	/* Find TD in which the selection ends */
	end = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (end)) {
		end = find_parent_element_by_type (
			end, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	single_row = (webkit_dom_node_get_parent_node (start) ==
			webkit_dom_node_get_parent_node (end));

	cell = start;
	while (cell) {
		WebKitDOMNodeList *nodes;
		gulong length, i;

		/* Remove all child nodes in the cell */
		nodes = webkit_dom_node_get_child_nodes (cell);
		length = webkit_dom_node_list_get_length (nodes);
		for (i = 0; i < length; i++) {
			webkit_dom_node_remove_child (
				cell,
				webkit_dom_node_list_item (nodes, i),
				NULL);
		}

		if (cell == end)
			break;

		cell = webkit_dom_node_get_next_sibling (cell);

		if (!cell && !single_row) {
			cell = webkit_dom_node_get_first_child (
				webkit_dom_node_get_parent_node (cell));
		}
	}
}

static void
action_context_delete_column_cb (GtkAction *action,
                                 EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *start, *end, *first_row;
	gulong index, count, i;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	/* Find TD in which the selection starts */
	start = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (start)) {
		start = find_parent_element_by_type (
			start, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	/* Find TD in which the selection ends */
	end = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (end)) {
		end = find_parent_element_by_type (
			end, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	first_row = find_parent_element_by_type (
		start, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	first_row = webkit_dom_node_get_first_child (
		webkit_dom_node_get_parent_node (first_row));

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (start));
	count = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (end)) - index;

	for (i = 0; i < count; i++) {
		WebKitDOMNode *row = first_row;

		while (row) {
			webkit_dom_html_table_row_element_delete_cell (
				WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row),
				index, NULL);

			row = webkit_dom_node_get_next_sibling (row);
		}
	}
}

static void
action_context_delete_row_cb (GtkAction *action,
                              EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *start, *end, *table;
	gulong index, row_count, i;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	start = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (start)) {
		start = find_parent_element_by_type (
			start, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	}

	end = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (end)) {
		end = find_parent_element_by_type (
			end, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	}

	table = find_parent_element_by_type (
			start, WEBKIT_TYPE_DOM_HTML_TABLE_ELEMENT);

	index = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (start));
	row_count = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (end)) - index;

	for (i = 0; i < row_count; i++) {
		webkit_dom_html_table_element_delete_row (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table), index, NULL);
	}
}

static void
action_context_delete_table_cb (GtkAction *action,
    EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *table;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	table = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (table)) {
		table = find_parent_element_by_type (
			table, WEBKIT_TYPE_DOM_HTML_TABLE_ELEMENT);
	}

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (table), table, NULL);
}

static void
action_context_insert_column_after_cb (GtkAction *action,
                                       EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *cell, *row;
	gulong index;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	cell = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		cell = find_parent_element_by_type (
			cell, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	row = find_parent_element_by_type (
		cell, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	/* Get the first row in the table */
	row = webkit_dom_node_get_first_child (
		webkit_dom_node_get_parent_node (row));

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));

	while (row) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), index + 1, NULL);

		row = webkit_dom_node_get_next_sibling (row);
	}
}

static void
action_context_insert_column_before_cb (GtkAction *action,
                                        EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *cell, *row;
	gulong index;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	cell = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		cell = find_parent_element_by_type (
			cell, WEBKIT_TYPE_DOM_HTML_TABLE_CELL_ELEMENT);
	}

	row = find_parent_element_by_type (
		cell, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	/* Get the first row in the table */
	row = webkit_dom_node_get_first_child (
		webkit_dom_node_get_parent_node (row));

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));

	while (row) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), index - 1, NULL);

		row = webkit_dom_node_get_next_sibling (row);
	}
}

static void
action_context_insert_row_above_cb (GtkAction *action,
                                    EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *row, *table;
	gulong index;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	row = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (row)) {
		row = find_parent_element_by_type (
			row, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	}

	table = find_parent_element_by_type (
		row, WEBKIT_TYPE_DOM_HTML_TABLE_ELEMENT);

	index = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	webkit_dom_html_table_element_insert_row (
		WEBKIT_DOM_HTML_TABLE_ELEMENT (table), index - 1, NULL);
}

static void
action_context_insert_row_below_cb (GtkAction *action,
                                    EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *row, *table;
	gulong index;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	row = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (row)) {
		row = find_parent_element_by_type (
			row, WEBKIT_TYPE_DOM_HTML_TABLE_ROW_ELEMENT);
	}

	table = find_parent_element_by_type (
		row, WEBKIT_TYPE_DOM_HTML_TABLE_ELEMENT);

	index = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	webkit_dom_html_table_element_insert_row (
		WEBKIT_DOM_HTML_TABLE_ELEMENT (table), index + 1, NULL);
}

static void
action_context_remove_link_cb (GtkAction *action,
                               EEditor *editor)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	webkit_dom_document_exec_command (document, "unlink", FALSE, "");
}

static void
action_context_spell_add_cb (GtkAction *action,
                             EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitSpellChecker *spell_checker;
	gchar *word;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
	word = webkit_dom_range_get_text (range);

	spell_checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	webkit_spell_checker_learn_word (spell_checker, word);

	g_free (word);
}

static void
action_context_spell_ignore_cb (GtkAction *action,
                                EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitSpellChecker *spell_checker;
	gchar *word;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
	word = webkit_dom_range_get_text (range);

	spell_checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
	webkit_spell_checker_ignore_word (spell_checker, word);

	g_free (word);
}

static void
action_copy_cb (GtkAction *action,
                EEditor *editor)
{
	webkit_web_view_copy_clipboard (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_cut_cb (GtkAction *action,
               EEditor *editor)
{
	webkit_web_view_cut_clipboard (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_find_cb (GtkAction *action,
                EEditor *editor)
{
	gboolean found;


	found = webkit_web_view_search_text (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)),
		gtk_entry_get_text (GTK_ENTRY (WIDGET (FIND_ENTRY))),
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (WIDGET (FIND_CASE_SENSITIVE))),
		!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (WIDGET (FIND_BACKWARDS))),
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (WIDGET (FIND_WRAP))));

	gtk_action_set_sensitive (ACTION (FIND), found);

	if (!found)
		gtk_label_set_label (
			GTK_LABEL (WIDGET (FIND_RESULT_LABEL)),
			N_("No match found"));
}

static void
action_find_again_cb (GtkAction *action,
                      EEditor *editor)
{
	/* FIXME WEBKIT
	 * Verify that this actually works and if so, then just change the
	 * callback and remove this function. If not, then we are in trouble...
	 */

	action_find_cb (action, editor);
}


static void
action_find_and_replace_cb (GtkAction *action,
                            EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *start, *end;
	WebKitDOMNodeList *children;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMNode *node;

	gulong length, i;
	const gchar *needle, *replacement;
	gboolean case_sensitive, backwards, wrap, only_selection;

	needle = gtk_entry_get_text (GTK_ENTRY (WIDGET (REPLACE_ENTRY)));
	replacement = gtk_entry_get_text (
				GTK_ENTRY (WIDGET (REPLACE_WITH_ENTRY)));
	case_sensitive = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (WIDGET (REPLACE_CASE_SENSITIVE)));
	wrap = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (WIDGET (REPLACE_WRAP)));
	backwards = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (WIDGET (REPLACE_BACKWARDS)));
	only_selection = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (WIDGET (REPLACE_ONLY_SELECTION)));

	/* XXX Port this if to native WebKit API if they ever implement
	 * support for find&replace...unless this implementation
	 * is better ;) */

	/* FIXME WEBKIT This is not working at all */
	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
	start = webkit_dom_range_get_start_container (range, NULL);

	if (only_selection) {
		end = webkit_dom_range_get_end_container (range, NULL);
	} else {
		end = NULL;
	}

	/* WEBKIT FIXME Implement me! */
	/*
	node = start;
	while (node && (node != end)) {



	}
	*/

	/*
	html = gtkhtml_editor_get_html (editor);

	gtk_widget_hide (WIDGET (REPLACE_WINDOW));

	html_engine_replace (
		html->engine,
		gtk_entry_get_text (GTK_ENTRY (WIDGET (REPLACE_ENTRY))),
		gtk_entry_get_text (GTK_ENTRY (WIDGET (REPLACE_WITH_ENTRY))),
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (WIDGET (REPLACE_CASE_SENSITIVE))),
		!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (WIDGET (REPLACE_BACKWARDS))),
		FALSE, replace_ask_cb, editor);
	*/
}

/* FIXME WEBKIT
static void
replace_ask_cb (HTMLEngine *engine,
                gpointer data)
{
	GtkhtmlEditor *editor = data;

	gtk_window_present (GTK_WINDOW (WIDGET (REPLACE_CONFIRMATION_WINDOW)));
}

static void
replace_answer (GtkhtmlEditor *editor,
                HTMLReplaceQueryAnswer answer)
{
	GtkHTML *html;

	html = gtkhtml_editor_get_html (editor);

	if (html_engine_replace_do (html->engine, answer))
		gtk_widget_hide (WIDGET (REPLACE_CONFIRMATION_WINDOW));
}
*/

static void
action_indent_cb (GtkAction *action,
                  EEditor *editor)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	webkit_dom_document_exec_command (document, "indent", FALSE, "");
}

static void
action_insert_emoticon_cb (GtkAction *action,
      EEditor *editor)
{
	/* FIXME WEBKIT
	GtkHTML *html;
	HTMLObject *image;
	GtkIconInfo *icon_info;
	GtkIconTheme *icon_theme;
	GtkhtmlFaceChooser *chooser;
	GtkhtmlFace *face;
	const gchar *filename;
	gchar *uri = NULL;

	html = gtkhtml_editor_get_html (editor);

	chooser = GTKHTML_FACE_CHOOSER (action);
	face = gtkhtml_face_chooser_get_current_face (chooser);
	g_return_if_fail (face != NULL);

	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (
		icon_theme, face->icon_name, 16, 0);
	g_return_if_fail (icon_info != NULL);

	filename = gtk_icon_info_get_filename (icon_info);
	if (filename != NULL)
		uri = g_filename_to_uri (filename, NULL, NULL);
	gtk_icon_info_free (icon_info);
	g_return_if_fail (uri != NULL);

	image = html_image_new (
		html_engine_get_image_factory (html->engine),
		uri, NULL, NULL, -1, -1, FALSE, FALSE, 0, NULL,
		HTML_VALIGN_MIDDLE, FALSE);
	html_image_set_alt (HTML_IMAGE (image), face->text_face);
	html_engine_paste_object (
		html->engine, image, html_object_get_length (image));

	g_free (uri);
	*/
}

static void
action_insert_html_file_cb (GtkToggleAction *action,
                            EEditor *editor)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new (
			_("Insert HTML File"), NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("HTML file"));
	gtk_file_filter_add_mime_type (filter, "text/html");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file (
					GTK_FILE_CHOOSER (dialog));

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
                        EEditor *editor)
{
	GtkWidget *dialog;

	dialog = e_image_chooser_dialog_new (
			_("Insert Image"), NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *uri;
		EEditorSelection *selection;

		 uri = gtk_file_chooser_get_uri (
				GTK_FILE_CHOOSER (dialog));

		 selection = e_editor_widget_get_selection (
				e_editor_get_editor_widget (editor));
		 e_editor_selection_insert_image (selection, uri);
		 g_free (uri);
	}

	gtk_widget_destroy (dialog);
}

static void
action_insert_link_cb (GtkAction *action,
                       EEditor *editor)
{
	/* FIXME WEBKIT */
	gtk_window_present (GTK_WINDOW (WIDGET (LINK_PROPERTIES_WINDOW)));
}

static void
action_insert_rule_cb (GtkAction *action,
                       EEditor *editor)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	webkit_dom_document_exec_command (
		document, "insertHorizontalRule", FALSE, "");

	/* FIXME WEBKIT - does the action work? */
	gtk_action_activate (ACTION (PROPERTIES_RULE));
}

static void
action_insert_table_cb (GtkAction *action,
                        EEditor *editor)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *table;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	gint i;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return;

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

	/* Default 3x3 table */
	table = webkit_dom_document_create_element (document, "TABLE", NULL);
	for (i = 0; i < 3; i++) {
		WebKitDOMHTMLElement *row;
		gint j;

		row = webkit_dom_html_table_element_insert_row (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table), -1, NULL);

		for (j = 0; j < 3; j++) {
			webkit_dom_html_table_row_element_insert_cell (
				WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row),
				-1, NULL);
		}
	}

	webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (table), NULL);

	/* FIXME WEBKIT - does the action work? */
	gtk_action_activate (ACTION (PROPERTIES_TABLE));
}

static void
action_insert_text_file_cb (GtkAction *action,
                            EEditor *editor)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new (
			_("Insert text file"), NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Text file"));
	gtk_file_filter_add_mime_type (filter, "text/plain");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GFile *file = gtk_file_chooser_get_file (
					GTK_FILE_CHOOSER (dialog));

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
action_italic_cb (GtkToggleAction *action,
                  EEditor *editor)
{
	EEditorSelection *selection;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_italic (
		selection, gtk_toggle_action_get_active (action));
}

static void
action_justify_cb (GtkRadioAction *action,
                   GtkRadioAction *current,
                   EEditor *editor)
{
	WebKitDOMDocument *document;
	const gchar *command;

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));

	switch (gtk_radio_action_get_current_value (current))
	{
		case E_EDITOR_SELECTION_ALIGNMENT_CENTER:
			command = "justifyCenter";
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_LEFT:
			command = "justifyLeft";
			break;

		case E_EDITOR_SELECTION_ALIGNMENT_RIGHT:
			command = "justifyRight";
			break;
	}

	webkit_dom_document_exec_command (document, command, FALSE, "");
}

static void
action_language_cb (GtkToggleAction *action,
                    EEditor *editor)
{
	/* FIXME WEBKIT */
	/*
	const GtkhtmlSpellLanguage *language;
	GtkhtmlSpellChecker *checker;
	const gchar *language_code;
	GtkAction *add_action;
	GtkHTML *html;
	GList *list;
	guint length;
	gchar *action_name;
	gboolean active;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	language_code = gtk_action_get_name (GTK_ACTION (action));
	language = gtkhtml_spell_language_lookup (language_code);

	checker = g_hash_table_lookup (
		editor->priv->available_spell_checkers, language);
	g_return_if_fail (checker != NULL);

	//Update the list of active spell checkers.
	list = editor->priv->active_spell_checkers;
	if (active)
		list = g_list_insert_sorted (
			list, g_object_ref (checker),
			(GCompareFunc) gtkhtml_spell_checker_compare);
	else {
		GList *link;

		link = g_list_find (list, checker);
		g_return_if_fail (link != NULL);
		list = g_list_delete_link (list, link);
		g_object_unref (checker);
	}
	editor->priv->active_spell_checkers = list;
	length = g_list_length (list);

	// Update "Add Word To" context menu item visibility.
	action_name = g_strdup_printf ("context-spell-add-%s", language_code);
	add_action = gtkhtml_editor_get_action (editor, action_name);
	gtk_action_set_visible (add_action, active);
	g_free (action_name);

	gtk_action_set_visible (ACTION (CONTEXT_SPELL_ADD), length == 1);
	gtk_action_set_visible (ACTION (CONTEXT_SPELL_ADD_MENU), length > 1);
	gtk_action_set_visible (ACTION (CONTEXT_SPELL_IGNORE), length > 0);

	gtk_action_set_sensitive (ACTION (SPELL_CHECK), length > 0);

	html = gtkhtml_editor_get_html (editor);
	html_engine_spell_check (html->engine);

	gtkthtml_editor_emit_spell_languages_changed (editor);
	*/
}

static void
action_mode_cb (GtkRadioAction *action,
                GtkRadioAction *current,
                EEditor *editor)
{
	/* FIXME WEBKIT */
	/*
	GtkActionGroup *action_group;
	HTMLPainter *new_painter;
	HTMLPainter *old_painter;
	GtkHTML *html;
	EditorMode mode;
	gboolean html_mode;

	html = gtkhtml_editor_get_html (editor);
	mode = gtk_radio_action_get_current_value (current);
	html_mode = (mode == EDITOR_MODE_HTML);

	action_group = editor->priv->html_actions;
	gtk_action_group_set_sensitive (action_group, html_mode);

	action_group = editor->priv->html_context_actions;
	gtk_action_group_set_visible (action_group, html_mode);

	gtk_widget_set_sensitive (editor->priv->color_combo_box, html_mode);

	if (html_mode)
		gtk_widget_show (editor->priv->html_toolbar);
	else
		gtk_widget_hide (editor->priv->html_toolbar);

	// Certain paragraph styles are HTML-only.
	gtk_action_set_sensitive (ACTION (STYLE_H1), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_H2), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_H3), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_H4), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_H5), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_H6), html_mode);
	gtk_action_set_sensitive (ACTION (STYLE_ADDRESS), html_mode);

	// Swap painters.

	if (html_mode) {
		new_painter = editor->priv->html_painter;
		old_painter = editor->priv->plain_painter;
	} else {
		new_painter = editor->priv->plain_painter;
		old_painter = editor->priv->html_painter;
	}

	// Might be true during initialization.
	if (html->engine->painter == new_painter)
		return;

	html_gdk_painter_unrealize (HTML_GDK_PAINTER (old_painter));
	if (html->engine->window != NULL)
		html_gdk_painter_realize (
			HTML_GDK_PAINTER (new_painter),
			html->engine->window);

	html_font_manager_set_default (
		&new_painter->font_manager,
		old_painter->font_manager.variable.face,
		old_painter->font_manager.fixed.face,
		old_painter->font_manager.var_size,
		old_painter->font_manager.var_points,
		old_painter->font_manager.fix_size,
		old_painter->font_manager.fix_points);

	html_engine_set_painter (html->engine, new_painter);
	html_engine_schedule_redraw (html->engine);

	g_object_notify (G_OBJECT (editor), "html-mode");
	*/
}

static void
action_monospaced_cb (GtkToggleAction *action,
                      EEditor *editor)
{
	/* FIXME WEBKIT */
	/*
	GtkHTML *html;
	GtkHTMLFontStyle and_mask;
	GtkHTMLFontStyle or_mask;

	if (editor->priv->ignore_style_change)
		return;

	if (gtk_toggle_action_get_active (action)) {
		and_mask = GTK_HTML_FONT_STYLE_MAX;
		or_mask = GTK_HTML_FONT_STYLE_FIXED;
	} else {
		and_mask = ~GTK_HTML_FONT_STYLE_FIXED;
		or_mask = 0;
	}

	html = gtkhtml_editor_get_html (editor);
	gtk_html_set_font_style (html, and_mask, or_mask);
	*/
}

static void
action_paste_cb (GtkAction *action,
                 EEditor *editor)
{
	webkit_web_view_paste_clipboard (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_paste_quote_cb (GtkAction *action,
                       EEditor *editor)
{
	/* FIXME WEBKIT */
	/*
	GtkHTML *html;

	html = gtkhtml_editor_get_html (editor);
	gtk_html_paste (html, TRUE);
	*/
}

static void
action_properties_cell_cb (GtkAction *action,
                           EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (CELL_PROPERTIES_WINDOW)));
}

static void
action_properties_image_cb (GtkAction *action,
                            EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (IMAGE_PROPERTIES_WINDOW)));
}

static void
action_properties_link_cb (GtkAction *action,
                           EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (LINK_PROPERTIES_WINDOW)));
}

static void
action_properties_page_cb (GtkAction *action,
                           EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (PAGE_PROPERTIES_WINDOW)));
}

static void
action_properties_paragraph_cb (GtkAction *action,
                                EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (PARAGRAPH_PROPERTIES_WINDOW)));
}

static void
action_properties_rule_cb (GtkAction *action,
                           EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (RULE_PROPERTIES_WINDOW)));
}

static void
action_properties_table_cb (GtkAction *action,
                            EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (TABLE_PROPERTIES_WINDOW)));
}

static void
action_properties_text_cb (GtkAction *action,
                           EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (TEXT_PROPERTIES_WINDOW)));
}

static void
action_style_cb (GtkRadioAction *action,
                 GtkRadioAction *current,
                 EEditor *editor)
{
	EEditorSelection *selection;

	/* FIXME WEBKIT What's this good for? */
	if (editor->priv->ignore_style_change)
		return;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_block_format (
		selection, gtk_radio_action_get_current_value (current));
}

static void
action_redo_cb (GtkAction *action,
                EEditor *editor)
{
	webkit_web_view_redo (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_select_all_cb (GtkAction *action,
                      EEditor *editor)
{
	webkit_web_view_select_all (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_show_find_cb (GtkAction *action,
                     EEditor *editor)
{
	gtk_widget_set_sensitive (WIDGET (FIND_BUTTON), TRUE);

	gtk_window_present (GTK_WINDOW (WIDGET (FIND_WINDOW)));
}

static void
action_show_replace_cb (GtkAction *action,
                        EEditor *editor)
{
	gtk_window_present (GTK_WINDOW (WIDGET (REPLACE_WINDOW)));
}

static void
action_size_cb (GtkRadioAction *action,
                GtkRadioAction *current,
                EEditor *editor)
{
	EEditorSelection *selection;

	if (editor->priv->ignore_style_change)
		return;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_font_size (
		selection, gtk_radio_action_get_current_value (current));
}

static void
action_spell_check_cb (GtkAction *action,
                       EEditor *editor)
{
	/* FIXME WEBKIT
	e_editor_widget_spell_check (editor);
	*/
}

static void
action_strikethrough_cb (GtkToggleAction *action,
                         EEditor *editor)
{
	EEditorSelection *selection;

	if (editor->priv->ignore_style_change)
		return;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_italic (
		selection, gtk_toggle_action_get_active (action));
}

static void
action_test_url_cb (GtkAction *action,
                    EEditor *editor)
{
	/* FIXME WEBKIT What does this do? */
	/*
	GtkEntry *entry;
	GtkWindow *window;
	const gchar *text;

	entry = GTK_ENTRY (WIDGET (LINK_PROPERTIES_URL_ENTRY));
	window = GTK_WINDOW (WIDGET (LINK_PROPERTIES_WINDOW));

	text = gtk_entry_get_text (entry);

	if (text != NULL && *text != '\0')
		gtkhtml_editor_show_uri (window, text);
	*/
}

static void
action_underline_cb (GtkToggleAction *action,
                     EEditor *editor)
{
	EEditorSelection *selection;

	if (editor->priv->ignore_style_change)
		return;

	selection = e_editor_widget_get_selection (
			e_editor_get_editor_widget (editor));
	e_editor_selection_set_underline (
		selection, gtk_toggle_action_get_active (action));
}

static void
action_undo_cb (GtkAction *action,
                EEditor *editor)
{
	webkit_web_view_undo (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
}

static void
action_unindent_cb (GtkAction *action,
                    EEditor *editor)
{
	WebKitDOMDocument *document;

	document = webkit_web_view_get_dom_document (
		WEBKIT_WEB_VIEW (e_editor_get_editor_widget (editor)));
	webkit_dom_document_exec_command (
		document, "outdent", FALSE, "");
}

static void
action_wrap_lines_cb (GtkAction *action,
                      EEditor *editor)
{
	/* FIXME WEBKIT */
	/*
	e_editor_widget_wrap_lines (editor);
	*/
}

/*****************************************************************************
 * Core Actions
 *
 * These actions are always enabled.
 *****************************************************************************/

static GtkActionEntry core_entries[] = {

	{ "confirm-replace",
	  NULL,
	  N_("_Replace"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_confirm_replace_cb) },

	{ "confirm-replace-all",
	  NULL,
	  N_("Replace _All"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_confirm_replace_all_cb) },

	{ "confirm-replace-cancel",
	  GTK_STOCK_CLOSE,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (action_confirm_replace_cancel_cb) },

	{ "confirm-replace-next",
	  NULL,
	  N_("_Next"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_confirm_replace_next_cb) },

	{ "copy",
	  GTK_STOCK_COPY,
	  N_("_Copy"),
	  "<Control>c",
	  NULL,
	  G_CALLBACK (action_copy_cb) },

	{ "cut",
	  GTK_STOCK_CUT,
	  N_("Cu_t"),
	  "<Control>x",
	  NULL,
	  G_CALLBACK (action_cut_cb) },

	{ "find",
	  GTK_STOCK_FIND,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (action_find_cb) },

	{ "find-again",
	  NULL,
	  N_("Find A_gain"),
	  "<Control>g",
	  NULL,
	  G_CALLBACK (action_find_again_cb) },

	{ "find-and-replace",
	  GTK_STOCK_FIND_AND_REPLACE,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (action_find_and_replace_cb) },

	{ "indent",
	  GTK_STOCK_INDENT,
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

	{ "paste",
	  GTK_STOCK_PASTE,
	  N_("_Paste"),
	  "<Control>v",
	  NULL,
	  G_CALLBACK (action_paste_cb) },

	{ "paste-quote",
	  NULL,
	  N_("Paste _Quotation"),
	  "<Shift><Control>v",
	  NULL,
	  G_CALLBACK (action_paste_quote_cb) },

	{ "redo",
	  GTK_STOCK_REDO,
	  N_("_Redo"),
	  "<Shift><Control>z",
	  NULL,
	  G_CALLBACK (action_redo_cb) },

	{ "select-all",
	  GTK_STOCK_SELECT_ALL,
	  N_("Select _All"),
	  "<Control>a",
	  NULL,
	  G_CALLBACK (action_select_all_cb) },

	{ "show-find",
	  GTK_STOCK_FIND,
	  N_("_Find..."),
	  "<Control>f",
	  NULL,
	  G_CALLBACK (action_show_find_cb) },

	{ "show-replace",
	  GTK_STOCK_FIND_AND_REPLACE,
	  N_("Re_place..."),
	  "<Control>h",
	  NULL,
	  G_CALLBACK (action_show_replace_cb) },

	{ "spell-check",
	  GTK_STOCK_SPELL_CHECK,
	  N_("Check _Spelling..."),
	  "F7",
	  NULL,
	  G_CALLBACK (action_spell_check_cb) },

	{ "test-url",
	  NULL,
	  N_("_Test URL..."),
	  NULL,
	  NULL,
	  G_CALLBACK (action_test_url_cb) },

	{ "undo",
	  GTK_STOCK_UNDO,
	  N_("_Undo"),
	  "<Control>z",
	  NULL,
	  G_CALLBACK (action_undo_cb) },

	{ "unindent",
	  GTK_STOCK_UNINDENT,
	  N_("_Decrease Indent"),
	  "<Control>bracketleft",
	  N_("Decrease Indent"),
	  G_CALLBACK (action_unindent_cb) },

	{ "wrap-lines",
	  NULL,
	  N_("_Wrap Lines"),
	  "<Control>k",
	  NULL,
	  G_CALLBACK (action_wrap_lines_cb) },

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

static GtkRadioActionEntry core_justify_entries[] = {

	{ "justify-center",
	  GTK_STOCK_JUSTIFY_CENTER,
	  N_("_Center"),
	  "<Control>e",
	  N_("Center Alignment"),
	  E_EDITOR_SELECTION_ALIGNMENT_CENTER },

	{ "justify-left",
	  GTK_STOCK_JUSTIFY_LEFT,
	  N_("_Left"),
	  "<Control>l",
	  N_("Left Alignment"),
	  E_EDITOR_SELECTION_ALIGNMENT_LEFT },

	{ "justify-right",
	  GTK_STOCK_JUSTIFY_RIGHT,
	  N_("_Right"),
	  "<Control>r",
	  N_("Right Alignment"),
	  E_EDITOR_SELECTION_ALIGNMENT_RIGHT }
};

static GtkRadioActionEntry core_mode_entries[] = {

	{ "mode-html",
	  NULL,
	  N_("_HTML"),
	  NULL,
	  N_("HTML editing mode"),
	  E_EDITOR_WIDGET_MODE_HTML },

	{ "mode-plain",
	  NULL,
	  N_("Plain _Text"),
	  NULL,
	  N_("Plain text editing mode"),
	  E_EDITOR_WIDGET_MODE_PLAIN_TEXT }
};

static GtkRadioActionEntry core_style_entries[] = {

	{ "style-normal",
	  NULL,
	  N_("_Normal"),
	  "<Control>0",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH },

	{ "style-h1",
	  NULL,
	  N_("Header _1"),
	  "<Control>1",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H1 },

	{ "style-h2",
	  NULL,
	  N_("Header _2"),
	  "<Control>2",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H2 },

	{ "style-h3",
	  NULL,
	  N_("Header _3"),
	  "<Control>3",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H3 },

	{ "style-h4",
	  NULL,
	  N_("Header _4"),
	  "<Control>4",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H4 },

	{ "style-h5",
	  NULL,
	  N_("Header _5"),
	  "<Control>5",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H5 },

	{ "style-h6",
	  NULL,
	  N_("Header _6"),
	  "<Control>6",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_H6 },

	{ "style-address",
	  NULL,
	  N_("A_ddress"),
	  "<Control>8",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS },

	{ "style-preformat",
	  NULL,
	  N_("_Preformatted"),
	  "<Control>7",
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_PRE },

	{ "style-list-bullet",
	  NULL,
	  N_("_Bulleted List"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST },

	{ "style-list-roman",
	  NULL,
	  N_("_Roman Numeral List"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN },

	{ "style-list-number",
	  NULL,
	  N_("Numbered _List"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST },

	{ "style-list-alpha",
	  NULL,
	  N_("_Alphabetical List"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA }
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
};

static GtkToggleActionEntry html_toggle_entries[] = {

	{ "bold",
	  GTK_STOCK_BOLD,
	  N_("_Bold"),
	  "<Control>b",
	  N_("Bold"),
	  G_CALLBACK (action_bold_cb),
	  FALSE },

	{ "italic",
	  GTK_STOCK_ITALIC,
	  N_("_Italic"),
	  "<Control>i",
	  N_("Italic"),
	  G_CALLBACK (action_italic_cb),
	  FALSE },

	{ "monospaced",
	  "stock_text-monospaced",
	  N_("_Plain Text"),
	  "<Control>t",
	  N_("Plain Text"),
	  G_CALLBACK (action_monospaced_cb),
	  FALSE },

	{ "strikethrough",
	  GTK_STOCK_STRIKETHROUGH,
	  N_("_Strikethrough"),
	  NULL,
	  N_("Strikethrough"),
	  G_CALLBACK (action_strikethrough_cb),
	  FALSE },

	{ "underline",
	  GTK_STOCK_UNDERLINE,
	  N_("_Underline"),
	  "<Control>u",
	  N_("Underline"),
	  G_CALLBACK (action_underline_cb),
	  FALSE }
};

static GtkRadioActionEntry html_size_entries[] = {

	{ "size-minus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-2"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_TINY },

	{ "size-minus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("-1"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_SMALL },

	{ "size-plus-zero",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+0"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_NORMAL },

	{ "size-plus-one",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+1"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_BIG },

	{ "size-plus-two",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+2"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_BIGGER },

	{ "size-plus-three",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+3"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_LARGE },

	{ "size-plus-four",
	  NULL,
	  /* Translators: This is a font size level. It is shown on a tool bar. Please keep it as short as possible. */
	  N_("+4"),
	  NULL,
	  NULL,
	  E_EDITOR_SELECTION_FONT_SIZE_VERY_LARGE }
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
	  G_CALLBACK (action_context_delete_cell_cb) },

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

	{ "context-input-methods-menu",
	  NULL,
	  N_("Input Methods"),
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

	{ "context-insert-table",
	  NULL,
	  N_("Table"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_insert_table_cb) },

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
editor_actions_setup_languages_menu (EEditor *editor)
{
	/* FIXME WEBKIT
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	const GList *available_languages;
	guint merge_id;

	manager = editor->priv->manager;
	action_group = editor->priv->language_actions;
	available_languages = gtkhtml_spell_language_get_available ();
	merge_id = gtk_ui_manager_new_merge_id (manager);

	while (available_languages != NULL) {
		GtkhtmlSpellLanguage *language = available_languages->data;
		GtkhtmlSpellChecker *checker;
		GtkToggleAction *action;

		checker = gtkhtml_spell_checker_new (language);

		g_hash_table_insert (
			editor->priv->available_spell_checkers,
			language, checker);

		action = gtk_toggle_action_new (
			gtkhtml_spell_language_get_code (language),
			gtkhtml_spell_language_get_name (language),
			NULL, NULL);

		g_signal_connect (
			action, "toggled",
			G_CALLBACK (action_language_cb), editor);

		gtk_action_group_add_action (
			action_group, GTK_ACTION (action));

		g_object_unref (action);

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/main-menu/edit-menu/language-menu",
			gtkhtml_spell_language_get_code (language),
			gtkhtml_spell_language_get_code (language),
			GTK_UI_MANAGER_AUTO, FALSE);

		available_languages = g_list_next (available_languages);
	}
	*/
}

static void
editor_actions_setup_spell_check_menu (EEditor *editor)
{
	/*
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	const GList *available_languages;
	guint merge_id;

	manager = editor->priv->manager;
	action_group = editor->priv->spell_check_actions;;
	available_languages = gtkhtml_spell_language_get_available ();
	merge_id = gtk_ui_manager_new_merge_id (manager);

	while (available_languages != NULL) {
		GtkhtmlSpellLanguage *language = available_languages->data;
		GtkAction *action;
		const gchar *code;
		const gchar *name;
		gchar *action_label;
		gchar *action_name;

		code = gtkhtml_spell_language_get_code (language);
		name = gtkhtml_spell_language_get_name (language);

		// Add a suggestion menu. 

		action_name = g_strdup_printf (
			"context-spell-suggest-%s-menu", code);

		action = gtk_action_new (action_name, name, NULL, NULL);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (
			manager, merge_id,
			"/context-menu/context-spell-suggest",
			action_name, action_name,
			GTK_UI_MANAGER_MENU, FALSE);

		g_free (action_name);

		// Add an item to the "Add Word To" menu.

		action_name = g_strdup_printf ("context-spell-add-%s", code);
		// Translators: %s will be replaced with the actual dictionary name,
		//where a user can add a word to. This is part of an "Add Word To" submenu.
		action_label = g_strdup_printf (_("%s Dictionary"), name);

		action = gtk_action_new (
			action_name, action_label, NULL, NULL);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_context_spell_add_cb), editor);

		// Visibility is dependent on whether the
		//corresponding language action is active.
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

		available_languages = g_list_next (available_languages);
	}
	*/
}

void
editor_actions_init (EEditor *editor)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;
	EEditorWidget *editor_widget;

	g_return_if_fail (E_IS_EDITOR (editor));

	manager = e_editor_get_ui_manager (editor);
	domain = GETTEXT_PACKAGE;

	/* Core Actions */
	action_group = editor->priv->core_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, core_entries,
		G_N_ELEMENTS (core_entries), editor);
	gtk_action_group_add_radio_actions (
		action_group, core_justify_entries,
		G_N_ELEMENTS (core_justify_entries),
		E_EDITOR_SELECTION_ALIGNMENT_LEFT,
		G_CALLBACK (action_justify_cb), editor);
	gtk_action_group_add_radio_actions (
		action_group, core_mode_entries,
		G_N_ELEMENTS (core_mode_entries),
		E_EDITOR_WIDGET_MODE_HTML,
		G_CALLBACK (action_mode_cb), editor);
	gtk_action_group_add_radio_actions (
		action_group, core_style_entries,
		G_N_ELEMENTS (core_style_entries),
		E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH,
		G_CALLBACK (action_style_cb), editor);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Face Action */
	action = e_emoticon_action_new (
		"insert-face", _("_Emoticon"),
		_("Insert Emoticon"), NULL);
	g_object_set (action, "icon-name", "face-smile", NULL);
	g_signal_connect (
		action, "item-activated",
		G_CALLBACK (action_insert_emoticon_cb), editor);
	gtk_action_group_add_action (action_group, action);

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
		E_EDITOR_SELECTION_FONT_SIZE_NORMAL,
		G_CALLBACK (action_size_cb), editor);
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

	/* Fine Tuning */

	g_object_set (
		G_OBJECT (ACTION (FIND)),
		"short-label", _("_Find"), NULL);
	g_object_set (
		G_OBJECT (ACTION (FIND_AND_REPLACE)),
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

	editor_widget = e_editor_get_editor_widget (editor);
	g_object_bind_property (
		editor_widget, "can-redo",
		ACTION (REDO), "sensitive",
		G_BINDING_SYNC_CREATE);
	g_object_bind_property (
		editor_widget, "can-undo",
		ACTION (UNDO), "sensitive",
		G_BINDING_SYNC_CREATE);
}
 

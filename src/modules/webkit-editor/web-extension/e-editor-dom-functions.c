/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <string.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentUnstable.h>
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#include <webkitdom/WebKitDOMHTMLElementUnstable.h>
#include <webkitdom/WebKitDOMElementUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#undef WEBKIT_DOM_USE_UNSTABLE_API

#include "web-extensions/e-dom-utils.h"

#include "e-editor-page.h"
#include "e-editor-undo-redo-manager.h"

#include "e-editor-dom-functions.h"

#define HTML_KEY_CODE_BACKSPACE 8
#define HTML_KEY_CODE_RETURN 13
#define HTML_KEY_CODE_CONTROL 17
#define HTML_KEY_CODE_SPACE 32
#define HTML_KEY_CODE_DELETE 46
#define HTML_KEY_CODE_TABULATOR 9

/* ******************** Tests ******************** */

static gchar *
workaround_spaces (const gchar *text)
{
	GString *tmp;
	gchar *str = NULL;

	tmp = e_str_replace_string (text, "&nbsp;", " ");
	if (tmp) {
		str = g_string_free (tmp, FALSE);
		text = str;
	}

	tmp = e_str_replace_string (text, " ", " ");
	if (tmp) {
		g_free (str);
		str = g_string_free (tmp, FALSE);
	} else if (!str) {
		str = g_strdup (text);
	}

	return str;
}

gboolean
e_editor_dom_test_html_equal (WebKitDOMDocument *document,
			      const gchar *html1,
			      const gchar *html2)
{
	WebKitDOMElement *elem1, *elem2;
	gchar *str1, *str2;
	gboolean res = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (document), FALSE);
	g_return_val_if_fail (html1 != NULL, FALSE);
	g_return_val_if_fail (html2 != NULL, FALSE);

	elem1 = webkit_dom_document_create_element (document, "TestHtmlEqual", &error);
	if (error || !elem1) {
		g_warning ("%s: Failed to create elem1: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	elem2 = webkit_dom_document_create_element (document, "TestHtmlEqual", &error);
	if (error || !elem2) {
		g_warning ("%s: Failed to create elem2: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	/* FIXME WK2: Workaround when &nbsp; is used instead of regular spaces. (Placed by WebKit?) */
	str1 = workaround_spaces (html1);
	str2 = workaround_spaces (html2);

	webkit_dom_element_set_inner_html (elem1, str1, &error);
	if (!error) {
		webkit_dom_element_set_inner_html (elem2, str2, &error);

		if (!error) {
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (elem1));
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (elem2));

			res = webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (elem1), WEBKIT_DOM_NODE (elem2));
		} else {
			g_warning ("%s: Failed to set inner html2: %s", G_STRFUNC, error->message);
		}
	} else {
		g_warning ("%s: Failed to set inner html1: %s", G_STRFUNC, error->message);
	}

	if (res && (g_strcmp0 (html1, str1) != 0 || g_strcmp0 (html2, str2) != 0))
		g_warning ("%s: Applied the '&nbsp;' workaround", G_STRFUNC);

	g_clear_error (&error);
	g_free (str1);
	g_free (str2);

	return res;
}

/* ******************** Actions ******************** */

static WebKitDOMElement *
get_table_cell_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-cell");
}

static void
prepare_history_for_table (EEditorPage *editor_page,
                           WebKitDOMElement *table,
                           EEditorHistoryEvent *ev)
{
	ev->type = HISTORY_TABLE_DIALOG;

	e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

	ev->data.dom.from = g_object_ref (webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (table), TRUE, NULL));
}


static void
save_history_for_table (EEditorPage *editor_page,
                        WebKitDOMElement *table,
                        EEditorHistoryEvent *ev)
{
	EEditorUndoRedoManager *manager;

	if (table)
		ev->data.dom.to = g_object_ref (webkit_dom_node_clone_node_with_error (
			WEBKIT_DOM_NODE (table), TRUE, NULL));
	else
		ev->data.dom.to = NULL;

	e_editor_dom_selection_get_coordinates (editor_page,
		&ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	e_editor_undo_redo_manager_insert_history_event (manager, ev);
}

void
e_editor_dom_delete_cell_contents (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *node;
	WebKitDOMElement *cell, *table_cell, *table;
	EEditorHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	while ((node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (cell))))
		remove_node (node);

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_delete_column (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *cell, *table, *table_cell;
	WebKitDOMHTMLCollection *rows = NULL;
	EEditorHistoryEvent *ev = NULL;
	gulong index, length, ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	/* Find TD in which the selection starts */
	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	rows = webkit_dom_html_table_element_get_rows (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table));
	length = webkit_dom_html_collection_get_length (rows);

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *row;

		row = webkit_dom_html_collection_item (rows, ii);

		webkit_dom_html_table_row_element_delete_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), index, NULL);
	}

	g_clear_object (&rows);

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_delete_row (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *row, *table, *table_cell;
	EEditorHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	remove_node (WEBKIT_DOM_NODE (row));

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_delete_table (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *table, *table_cell;
	EEditorHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	remove_node (WEBKIT_DOM_NODE (table));

	save_history_for_table (editor_page, NULL, ev);
}

void
e_editor_dom_insert_column_after (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *cell, *row, *table_cell, *table;
	EEditorHistoryEvent *ev = NULL;
	gulong index;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	/* Get the first row in the table */
	row = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_get_first_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (row))));

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));

	while (row) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), index + 1, NULL);

		row = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (row)));
	}

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_insert_column_before (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *cell, *row, *table_cell, *table;
	EEditorHistoryEvent *ev = NULL;
	gulong index;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell) {
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	}
	g_return_if_fail (cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	/* Get the first row in the table */
	row = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_get_first_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (row))));

	index = webkit_dom_html_table_cell_element_get_cell_index (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));

	while (row) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), index - 1, NULL);

		row = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (row)));
	}

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_insert_row_above (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells = NULL;
	WebKitDOMHTMLElement *new_row;
	EEditorHistoryEvent *ev = NULL;
	gulong index, cell_count, ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	index = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	new_row = webkit_dom_html_table_element_insert_row (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table), index, NULL);

	cells = webkit_dom_html_table_row_element_get_cells (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));
	cell_count = webkit_dom_html_collection_get_length (cells);
	for (ii = 0; ii < cell_count; ii++) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (new_row), -1, NULL);
	}

	g_clear_object (&cells);

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_insert_row_below (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells = NULL;
	WebKitDOMHTMLElement *new_row;
	EEditorHistoryEvent *ev = NULL;
	gulong index, cell_count, ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	prepare_history_for_table (editor_page, table, ev);

	index = webkit_dom_html_table_row_element_get_row_index (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	new_row = webkit_dom_html_table_element_insert_row (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table), index + 1, NULL);

	cells = webkit_dom_html_table_row_element_get_cells (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));
	cell_count = webkit_dom_html_collection_get_length (cells);
	for (ii = 0; ii < cell_count; ii++) {
		webkit_dom_html_table_row_element_insert_cell (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (new_row), -1, NULL);
	}

	g_clear_object (&cells);

	save_history_for_table (editor_page, table, ev);
}

void
e_editor_dom_save_history_for_cut (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;
	EEditorHistoryEvent *ev;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection) ||
	    webkit_dom_dom_selection_get_is_collapsed (dom_selection)) {
		g_clear_object (&dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	ev = g_new0 (EEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	e_editor_dom_selection_get_coordinates (editor_page,
		&ev->before.start.x,
		&ev->before.start.y,
		&ev->before.end.x,
		&ev->before.end.y);

	ev->after.start.x = ev->before.start.x;
	ev->after.start.y = ev->before.start.y;
	ev->after.end.x = ev->before.start.x;
	ev->after.end.y = ev->before.start.y;

	/* Save the fragment. */
	fragment = webkit_dom_range_clone_contents (range, NULL);
	g_clear_object (&dom_selection);
	g_clear_object (&range);
	ev->data.fragment = g_object_ref (fragment);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	e_editor_undo_redo_manager_insert_history_event (manager, ev);
	e_editor_page_set_dont_save_history_in_body_input (editor_page, TRUE);
}

/* ******************** View ******************** */

/*
 * e_editor_dom_exec_command:
 * @document: a #WebKitDOMDocument
 * @command: an #EContentEditorCommand to execute
 * @value: value of the command (or @NULL if the command does not require value)
 *
 * The function will fail when @value is @NULL or empty but the current @command
 * requires a value to be passed. The @value is ignored when the @command does
 * not expect any value.
 *
 * Returns: @TRUE when the command was succesfully executed, @FALSE otherwise.
 */
gboolean
e_editor_dom_exec_command (EEditorPage *editor_page,
			   EContentEditorCommand command,
			   const gchar *value)
{
	const gchar *cmd_str = 0;
	gboolean has_value = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

#define CHECK_COMMAND(cmd,str,val) case cmd:\
	if (val) {\
		g_return_val_if_fail (value && *value, FALSE);\
	}\
	has_value = val; \
	cmd_str = str;\
	break;

	switch (command) {
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR, "BackColor", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_BOLD, "Bold", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_COPY, "Copy", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_CREATE_LINK, "CreateLink", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_CUT, "Cut", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR, "DefaultParagraphSeparator", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_DELETE, "Delete", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FIND_STRING, "FindString", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FONT_NAME, "FontName", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FONT_SIZE, "FontSize", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FONT_SIZE_DELTA, "FontSizeDelta", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FORE_COLOR, "ForeColor", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FORMAT_BLOCK, "FormatBlock", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE, "ForwardDelete", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_HILITE_COLOR, "HiliteColor", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INDENT, "Indent", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_HORIZONTAL_RULE, "InsertHorizontalRule", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_HTML, "InsertHTML", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_IMAGE, "InsertImage", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_LINE_BREAK, "InsertLineBreak", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, "InsertNewlineInQuotedContent", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_ORDERED_LIST, "InsertOrderedList", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_PARAGRAPH, "InsertParagraph", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_TEXT, "InsertText", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_INSERT_UNORDERED_LIST, "InsertUnorderedList", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_ITALIC, "Italic", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_JUSTIFY_CENTER, "JustifyCenter", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_JUSTIFY_FULL, "JustifyFull", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_JUSTIFY_LEFT, "JustifyLeft", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_JUSTIFY_NONE, "JustifyNone", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_JUSTIFY_RIGHT, "JustifyRight", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_OUTDENT, "Outdent", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_PASTE, "Paste", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_PASTE_AND_MATCH_STYLE, "PasteAndMatchStyle", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_PASTE_AS_PLAIN_TEXT, "PasteAsPlainText", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_PRINT, "Print", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_REDO, "Redo", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_REMOVE_FORMAT, "RemoveFormat", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_SELECT_ALL, "SelectAll", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH, "Strikethrough", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS, "StyleWithCSS", TRUE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_SUBSCRIPT, "Subscript", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT, "Superscript", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_TRANSPOSE, "Transpose", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_UNDERLINE, "Underline", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_UNDO, "Undo", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_UNLINK, "Unlink", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_UNSELECT, "Unselect", FALSE)
		CHECK_COMMAND (E_CONTENT_EDITOR_COMMAND_USE_CSS, "UseCSS", TRUE)
	}

	e_editor_page_set_dont_save_history_in_body_input (editor_page, TRUE);

	return webkit_dom_document_exec_command (
		e_editor_page_get_document (editor_page), cmd_str, FALSE, has_value ? value : "" );
}

static void
perform_spell_check (WebKitDOMDOMSelection *dom_selection,
                     WebKitDOMRange *start_range,
                     WebKitDOMRange *end_range)
{
	WebKitDOMRange *actual = start_range;

	/* FIXME WK2: this doesn't work, the cursor is moved, but the spellcheck is not updated */
	/* Go through all words to spellcheck them. To avoid this we have to wait for
	 * http://www.w3.org/html/wg/drafts/html/master/editing.html#dom-forcespellcheck */
	/* We are moving forward word by word until we hit the text on the end. */
	while (actual && webkit_dom_range_compare_boundary_points (actual, WEBKIT_DOM_RANGE_START_TO_START, end_range, NULL) < 0) {
		if (actual != start_range)
			g_object_unref (actual);
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "forward", "word");
		actual = webkit_dom_dom_selection_get_range_at (
			dom_selection, 0, NULL);
	}
	g_clear_object (&actual);
}

void
e_editor_dom_force_spell_check_for_current_paragraph (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMElement *parent;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range = NULL, *actual = NULL;
	WebKitDOMText *text;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	if (!e_editor_page_get_inline_spelling_enabled (editor_page))
		return;

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	if (!body)
		return;

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EEditorSelection and here as well
	 * when we are moving with caret */
	e_editor_page_block_selection_changed (editor_page);

	parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_end_marker));
	if (!parent)
		parent = WEBKIT_DOM_ELEMENT (body);

	/* Append some text on the end of the element */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (text),
		NULL);

	parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker));
	if (!parent)
		parent = WEBKIT_DOM_ELEMENT (body);

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	/* Move on the beginning of the paragraph */
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	actual = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		actual, WEBKIT_DOM_NODE (parent), NULL);
	webkit_dom_range_collapse (actual, TRUE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual);
	g_clear_object (&actual);

	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	perform_spell_check (dom_selection, actual, end_range);

	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
	g_clear_object (&end_range);
	g_clear_object (&actual);

	/* Remove the text that we inserted on the end of the paragraph */
	remove_node (WEBKIT_DOM_NODE (text));

	e_editor_dom_selection_restore (editor_page);
	/* Unblock the callbacks */
	e_editor_page_unblock_selection_changed (editor_page);
}

static void
refresh_spell_check (EEditorPage *editor_page,
                     gboolean enable_spell_check)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range = NULL, *actual = NULL;
	WebKitDOMText *text;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	/* Enable/Disable spellcheck in composer */
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body),
		"spellcheck",
		enable_spell_check ? "true" : "false",
		NULL);

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* Sometimes the web view is not focused, so we have to save the selection
	 * manually into the body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMNode *child;

		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (!WEBKIT_DOM_IS_ELEMENT (child))
			return;

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EEditorSelection and here as well
	 * when we are moving with caret */
	e_editor_page_block_selection_changed (editor_page);

	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of this text */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* Move on the beginning of the document */
	webkit_dom_dom_selection_modify (
		dom_selection, "move", "backward", "documentboundary");

	actual = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	perform_spell_check (dom_selection, actual, end_range);

	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
	g_clear_object (&end_range);
	g_clear_object (&actual);

	/* Remove the text that we inserted on the end of the body */
	remove_node (WEBKIT_DOM_NODE (text));

	e_editor_dom_selection_restore (editor_page);
	/* Unblock the callbacks */
	e_editor_page_unblock_selection_changed (editor_page);
}

void
e_editor_dom_turn_spell_check_off (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	refresh_spell_check (editor_page, FALSE);
}

void
e_editor_dom_force_spell_check_in_viewport (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMElement *last_element;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *end_range = NULL, *actual = NULL;
	WebKitDOMText *text;
	glong viewport_height;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_inline_spelling_enabled (editor_page))
		return;

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)))
		return;

	e_editor_dom_selection_save (editor_page);

	/* Block callbacks of selection-changed signal as we don't want to
	 * recount all the block format things in EEditorSelection and here as well
	 * when we are moving with caret */
	e_editor_page_block_selection_changed (editor_page);

	/* We have to add 10 px offset as otherwise just the HTML element will be returned */
	actual = webkit_dom_document_caret_range_from_point (document, 10, 10);
	if (!actual)
		goto out;

	/* Append some text on the end of the body */
	text = webkit_dom_document_create_text_node (document, "-x-evo-end");

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* We have to add 10 px offset as otherwise just the HTML element will be returned */
	viewport_height = webkit_dom_dom_window_get_inner_height (dom_window);
	last_element = webkit_dom_document_element_from_point (document, 10, viewport_height - 10);
	if (last_element && !WEBKIT_DOM_IS_HTML_HTML_ELEMENT (last_element) &&
	    !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (last_element)) {
		WebKitDOMElement *parent;

		parent = get_parent_block_element (WEBKIT_DOM_NODE (last_element));
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (parent ? parent : last_element), WEBKIT_DOM_NODE (text), NULL);
	} else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (text), NULL);

	/* Create range that's pointing on the end of viewport */
	end_range = webkit_dom_document_create_range (document);
	webkit_dom_range_select_node_contents (
		end_range, WEBKIT_DOM_NODE (text), NULL);
	webkit_dom_range_collapse (end_range, FALSE, NULL);

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual);
	perform_spell_check (dom_selection, actual, end_range);

	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
	g_clear_object (&end_range);
	g_clear_object (&actual);

	/* Remove the text that we inserted on the end of the body */
	remove_node (WEBKIT_DOM_NODE (text));

 out:
	e_editor_dom_selection_restore (editor_page);
	/* Unblock the callbacks */
	e_editor_page_unblock_selection_changed (editor_page);
}

void
e_editor_dom_force_spell_check (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_inline_spelling_enabled (editor_page))
		refresh_spell_check (editor_page, TRUE);
}

gboolean
e_editor_dom_node_is_citation_node (WebKitDOMNode *node)
{
	gboolean ret_val = FALSE;
	gchar *value;

	if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node))
		return FALSE;

	/* citation == <blockquote type='cite'> */
	if ((value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type")))
		ret_val = g_strcmp0 (value, "cite") == 0;

	g_free (value);

	return ret_val;
}

gint
e_editor_dom_get_citation_level (WebKitDOMNode *node,
                                 gboolean set_plaintext_quoted)
{
	WebKitDOMNode *parent = node;
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "type"))
			level++;

		parent = webkit_dom_node_get_parent_node (parent);
	}

	return level;
}

static gchar *
get_quotation_for_level (gint quote_level)
{
	const gchar *quote_element = "<span class=\"-x-evo-quote-character\">" QUOTE_SYMBOL " </span>";
	gint ii;
	GString *output = g_string_new ("");

	for (ii = 0; ii < quote_level; ii++)
		g_string_append (output, quote_element);

	return g_string_free (output, FALSE);
}

void
e_editor_dom_quote_plain_text_element_after_wrapping (EEditorPage *editor_page,
                                                      WebKitDOMElement *element,
                                                      gint quote_level)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMNode *quoted_node;
	gint ii;
	gchar *quotation;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (element != NULL);

	document = e_editor_page_get_document (editor_page);

	quoted_node = WEBKIT_DOM_NODE (
		webkit_dom_document_create_element (document, "SPAN", NULL));
	webkit_dom_element_set_class_name (
		WEBKIT_DOM_ELEMENT (quoted_node), "-x-evo-quoted");
	quotation = get_quotation_for_level (quote_level);
	webkit_dom_element_set_inner_html (
		WEBKIT_DOM_ELEMENT (quoted_node), quotation, NULL);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br, pre > br", NULL);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		quoted_node,
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);

	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *br = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (br);

		if ((!WEBKIT_DOM_IS_ELEMENT (prev_sibling) ||
		     !element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted")) &&
		     webkit_dom_node_get_next_sibling (br)) {

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (br),
				webkit_dom_node_clone_node_with_error (quoted_node, TRUE, NULL),
				webkit_dom_node_get_next_sibling (br),
				NULL);
		}
	}

	g_clear_object (&list);
	g_free (quotation);
}

static gboolean
return_pressed_in_empty_line (EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (!WEBKIT_DOM_IS_TEXT (node)) {
		WebKitDOMNode *first_child;

		first_child = webkit_dom_node_get_first_child (node);
		if (first_child && WEBKIT_DOM_IS_ELEMENT (first_child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (first_child), "-x-evo-quoted")) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (node);
			if (!prev_sibling) {
				gboolean collapsed;

				collapsed = webkit_dom_range_get_collapsed (range, NULL);
				g_clear_object (&range);
				return collapsed;
			}
		}
	}

	g_clear_object (&range);

	return FALSE;
}

WebKitDOMNode *
e_editor_dom_get_parent_block_node_from_child (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = node;

	if (!WEBKIT_DOM_IS_ELEMENT (parent) ||
	    e_editor_dom_is_selection_position_node (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quote-character") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-signature") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-resizable-wrapper") ||
	    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "Apple-tab-span") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-resizable-wrapper"))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

gboolean
e_editor_dom_node_is_paragraph (WebKitDOMNode *node)
{
	if (!WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node))
		return FALSE;

	return webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-evo-paragraph");
}

WebKitDOMElement *
e_editor_dom_wrap_and_quote_element (EEditorPage *editor_page,
                                     WebKitDOMElement *element)
{
	gint citation_level;
	WebKitDOMElement *tmp_element = element;

	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (element), element);

	if (e_editor_page_get_html_mode (editor_page))
		return element;

	citation_level = e_editor_dom_get_citation_level (WEBKIT_DOM_NODE (element), FALSE);

	e_editor_dom_remove_quoting_from_element (element);
	e_editor_dom_remove_wrapping_from_element (element);

	if (e_editor_dom_node_is_paragraph (WEBKIT_DOM_NODE (element))) {
		gint word_wrap_length, length;

		word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
		length = word_wrap_length - 2 * citation_level;
		tmp_element = e_editor_dom_wrap_paragraph_length (
			editor_page, element, length);
	}

	if (citation_level > 0) {
		webkit_dom_node_normalize (WEBKIT_DOM_NODE (tmp_element));
		e_editor_dom_quote_plain_text_element_after_wrapping (
			editor_page, tmp_element, citation_level);
	}

	return tmp_element;
}

WebKitDOMElement *
e_editor_dom_insert_new_line_into_citation (EEditorPage *editor_page,
                                            const gchar *html_to_insert)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *paragraph = NULL;
	WebKitDOMNode *last_block;
	gboolean html_mode = FALSE, ret_val, avoid_editor_call;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);

	avoid_editor_call = return_pressed_in_empty_line (editor_page);

	if (avoid_editor_call) {
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *current_block, *parent, *parent_block, *block_clone;

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		current_block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		block_clone = webkit_dom_node_clone_node_with_error (current_block, TRUE, NULL);
		/* Find selection start marker and restore it after the new line
		 * is inserted */
		selection_start_marker = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block_clone), "#-x-evo-selection-start-marker", NULL);

		/* Find parent node that is immediate child of the BODY */
		/* Build the same structure of parent nodes of the current block */
		parent_block = current_block;
		parent = webkit_dom_node_get_parent_node (parent_block);
		while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			WebKitDOMNode *node;

			parent_block = parent;
			node = webkit_dom_node_clone_node_with_error (parent_block, FALSE, NULL);
			webkit_dom_node_append_child (node, block_clone, NULL);
			block_clone = node;
			parent = webkit_dom_node_get_parent_node (parent_block);
		}

		paragraph = e_editor_dom_get_paragraph_element (editor_page, -1, 0);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (paragraph),
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_element (document, "BR", NULL)),
			NULL);

		/* Insert the selection markers to right place */
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (paragraph),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_start_marker)),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (paragraph)),
			NULL);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (paragraph),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (paragraph)),
			NULL);

		/* Insert the cloned nodes before the BODY parent node */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent_block),
			block_clone,
			parent_block,
			NULL);

		/* Insert the new empty paragraph before the BODY parent node */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent_block),
			WEBKIT_DOM_NODE (paragraph),
			parent_block,
			NULL);

		/* Remove the old block (its copy was moved to the right place) */
		remove_node (current_block);

		e_editor_dom_selection_restore (editor_page);

		return NULL;
	} else {
		e_editor_dom_remove_input_event_listener_from_body (editor_page);
		e_editor_page_block_selection_changed (editor_page);

		ret_val = e_editor_dom_exec_command (
			editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		e_editor_page_unblock_selection_changed (editor_page);
		e_editor_dom_register_input_event_listener_on_body (editor_page);

		if (!ret_val)
			return NULL;

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		if (!element)
			return NULL;
	}

	last_block = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	while (last_block && e_editor_dom_node_is_citation_node (last_block))
		last_block = webkit_dom_node_get_last_child (last_block);

	if (last_block) {
		WebKitDOMNode *last_child;

		if ((last_child = webkit_dom_node_get_last_child (last_block))) {
			if (WEBKIT_DOM_IS_ELEMENT (last_child) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (last_child), "-x-evo-quoted"))
				webkit_dom_node_append_child (
					last_block,
					WEBKIT_DOM_NODE (
						webkit_dom_document_create_element (
							document, "br", NULL)),
					NULL);
		}
	}

	if (!html_mode) {
		WebKitDOMNode *sibling;

		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (sibling)) {
			WebKitDOMNode *node;

			node = webkit_dom_node_get_first_child (sibling);
			while (node && e_editor_dom_node_is_citation_node (node))
				node = webkit_dom_node_get_first_child (node);

			/* Rewrap and requote nodes that were created by split. */
			if (WEBKIT_DOM_IS_ELEMENT (node))
				e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (node));

			if (WEBKIT_DOM_IS_ELEMENT (last_block))
				e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (last_block));

			e_editor_dom_force_spell_check_in_viewport (editor_page);
		}
	}

	if (html_to_insert && *html_to_insert) {
		paragraph = e_editor_dom_prepare_paragraph (editor_page, FALSE);
		webkit_dom_element_set_inner_html (
			paragraph, html_to_insert, NULL);
		if (!webkit_dom_element_query_selector (paragraph, "#-x-evo-selection-start-marker", NULL))
			dom_add_selection_markers_into_element_end (
				document, paragraph, NULL, NULL);
	} else
		paragraph = e_editor_dom_prepare_paragraph (editor_page, TRUE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		WEBKIT_DOM_NODE (paragraph),
		WEBKIT_DOM_NODE (element),
		NULL);

	remove_node (WEBKIT_DOM_NODE (element));

	e_editor_dom_selection_restore (editor_page);

	return paragraph;
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
put_body_in_citation (WebKitDOMDocument *document)
{
	WebKitDOMElement *cite_body = webkit_dom_document_query_selector (
		document, "span.-x-evo-cite-body", NULL);

	if (cite_body) {
		WebKitDOMHTMLElement *body = webkit_dom_document_get_body (document);
		WebKitDOMNode *citation;
		WebKitDOMNode *sibling;

		citation = WEBKIT_DOM_NODE (
			webkit_dom_document_create_element (document, "blockquote", NULL));
		webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (citation), "-x-evo-main-cite");
		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (citation), "type", "cite", NULL);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			citation,
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			NULL);

		while ((sibling = webkit_dom_node_get_next_sibling (citation)))
			webkit_dom_node_append_child (citation, sibling, NULL);

		remove_node (WEBKIT_DOM_NODE (cite_body));
	}
}

/* For purpose of this function see e-mail-formatter-quote.c */
static void
move_elements_to_body (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list = NULL;
	gint ii, jj;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);
	list = webkit_dom_document_query_selector_all (
		document, "div[data-headers]", NULL);
	for (jj = 0, ii = webkit_dom_node_list_get_length (list); ii--; jj++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-headers");
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			node,
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (body)),
			NULL);

	}
	g_clear_object (&list);

	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-credits]", NULL);
	e_editor_page_set_allow_top_signature (editor_page, webkit_dom_node_list_get_length (list) > 0);
	for (jj = 0, ii = webkit_dom_node_list_get_length (list); ii--; jj++) {
		char *credits;
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

		element = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
		credits = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "data-credits");
		if (credits)
			webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (element), credits, NULL);
		g_free (credits);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_first_child (
				WEBKIT_DOM_NODE (body)),
			NULL);

		remove_node (node);
	}
	g_clear_object (&list);
}

static void
repair_gmail_blockquotes (WebKitDOMDocument *document)
{
	WebKitDOMHTMLCollection *collection = NULL;
	gint ii;

	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "gmail_quote");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node = webkit_dom_html_collection_item (collection, ii);

		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node))
			continue;

		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "class");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "style");
		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "type", "cite", NULL);

		if (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_last_child (node)))
			webkit_dom_node_append_child (
				node,
				WEBKIT_DOM_NODE (
					webkit_dom_document_create_element (
						document, "br", NULL)),
				NULL);
	}
	g_clear_object (&collection);
}

static void
remove_thunderbird_signature (WebKitDOMDocument *document)
{
	WebKitDOMElement *signature;

	signature = webkit_dom_document_query_selector (
		document, "pre.moz-signature", NULL);
	if (signature)
		remove_node (WEBKIT_DOM_NODE (signature));
}

void
e_editor_dom_check_magic_links (EEditorPage *editor_page,
                                gboolean include_space_by_user)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	gchar *node_text;
	gchar **urls;
	gboolean include_space = FALSE;
	gboolean is_email_address = FALSE;
	gboolean return_key_pressed;
	GRegex *regex = NULL;
	GMatchInfo *match_info;
	gint start_pos_url, end_pos_url;
	gboolean has_selection;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_magic_links_enabled (editor_page))
		return;

	return_key_pressed = e_editor_page_get_return_key_pressed (editor_page);
	document = e_editor_page_get_document (editor_page);

	if (include_space_by_user)
		include_space = TRUE;
	else
		include_space = e_editor_page_get_space_key_pressed (editor_page);

	range = e_editor_dom_get_current_range (editor_page);
	node = webkit_dom_range_get_end_container (range, NULL);
	has_selection = !webkit_dom_range_get_collapsed (range, NULL);
	g_clear_object (&range);

	if (return_key_pressed) {
		WebKitDOMNode* block;

		block = e_editor_dom_get_parent_block_node_from_child (node);
		/* Get previous block */
		if (!(block = webkit_dom_node_get_previous_sibling (block)))
			return;

		/* If block is quoted content, get the last block there */
		while (block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (block))
			block = webkit_dom_node_get_last_child (block);

		/* Get the last non-empty node */
		node = webkit_dom_node_get_last_child (block);
		if (WEBKIT_DOM_IS_CHARACTER_DATA (node) &&
		    webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) == 0)
			node = webkit_dom_node_get_previous_sibling (node);
	} else {
		e_editor_dom_selection_save (editor_page);
		if (has_selection) {
			WebKitDOMElement *selection_end_marker;

			selection_end_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			node = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_end_marker));
		}
	}

	if (!node || WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node))
		goto out;

	if (!WEBKIT_DOM_IS_TEXT (node)) {
		if (webkit_dom_node_has_child_nodes (node))
			node = webkit_dom_node_get_first_child (node);
		if (!WEBKIT_DOM_IS_TEXT (node))
			goto out;
	}

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (!(node_text && *node_text) || !g_utf8_validate (node_text, -1, NULL)) {
		g_free (node_text);
		goto out;
	}

	if (strstr (node_text, "@") && !strstr (node_text, "://")) {
		is_email_address = TRUE;
		regex = g_regex_new (include_space ? E_MAIL_PATTERN_SPACE : E_MAIL_PATTERN, 0, 0, NULL);
	} else
		regex = g_regex_new (include_space ? URL_PATTERN_SPACE : URL_PATTERN, 0, 0, NULL);

	if (!regex) {
		g_free (node_text);
		goto out;
	}

	g_regex_match_all (regex, node_text, G_REGEX_MATCH_NOTEMPTY, &match_info);
	urls = g_match_info_fetch_all (match_info);

	if (urls) {
		const gchar *end_of_match = NULL;
		gchar *final_url, *url_end_raw, *url_text;
		glong url_start, url_end, url_length;
		WebKitDOMNode *url_text_node;
		WebKitDOMElement *anchor;

		g_match_info_fetch_pos (match_info, 0, &start_pos_url, &end_pos_url);

		/* Get start and end position of url in node's text because positions
		 * that we get from g_match_info_fetch_pos are not UTF-8 aware */
		url_end_raw = g_strndup(node_text, end_pos_url);
		url_end = g_utf8_strlen (url_end_raw, -1);
		url_length = g_utf8_strlen (urls[0], -1);

		end_of_match = url_end_raw + end_pos_url - (include_space ? 3 : 2);
		/* URLs are extremely unlikely to end with any punctuation, so
		 * strip any trailing punctuation off from link and put it after
		 * the link. Do the same for any closing double-quotes as well. */
		while (end_of_match && end_of_match != url_end_raw && strchr (URL_INVALID_TRAILING_CHARS, *end_of_match)) {
			url_length--;
			url_end--;
			end_of_match--;
		}

		url_start = url_end - url_length;

		webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node),
			include_space ? url_end - 1 : url_end,
			NULL);

		webkit_dom_text_split_text (
			WEBKIT_DOM_TEXT (node), url_start, NULL);
		url_text_node = webkit_dom_node_get_next_sibling (node);
		url_text = webkit_dom_character_data_get_data (
			WEBKIT_DOM_CHARACTER_DATA (url_text_node));

		if (g_str_has_prefix (url_text, "www."))
			final_url = g_strconcat ("http://" , url_text, NULL);
		else if (is_email_address)
			final_url = g_strconcat ("mailto:" , url_text, NULL);
		else
			final_url = g_strdup (url_text);

		/* Create and prepare new anchor element */
		anchor = webkit_dom_document_create_element (document, "A", NULL);

		webkit_dom_element_set_inner_html (anchor, url_text, NULL);

		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (anchor),
			final_url);

		/* Insert new anchor element into document */
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (anchor),
			WEBKIT_DOM_NODE (url_text_node),
			NULL);

		g_free (url_end_raw);
		g_free (final_url);
		g_free (url_text);
	} else {
		gboolean appending_to_link = FALSE;
		gchar *href, *text, *url, *text_to_append = NULL;
		gint diff;
		WebKitDOMElement *parent;
		WebKitDOMNode *prev_sibling;

		parent = webkit_dom_node_get_parent_element (node);
		prev_sibling = webkit_dom_node_get_previous_sibling (node);

		/* If previous sibling is ANCHOR and actual text node is not beginning with
		 * space => we're appending to link */
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
			text_to_append = webkit_dom_node_get_text_content (node);
			if (text_to_append && *text_to_append &&
			    !strstr (text_to_append, " ") &&
			    !(strchr (URL_INVALID_TRAILING_CHARS, *text_to_append) &&
			      !(*text_to_append == '?' && strlen(text_to_append) > 1)) &&
			    !g_str_has_prefix (text_to_append, UNICODE_NBSP)) {

				appending_to_link = TRUE;
				parent = WEBKIT_DOM_ELEMENT (prev_sibling);
				/* If the node(text) contains the some of unwanted characters
				 * split it into two nodes and select the right one. */
				if (g_str_has_suffix (text_to_append, UNICODE_NBSP) ||
				    g_str_has_suffix (text_to_append, UNICODE_ZERO_WIDTH_SPACE)) {
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node),
						g_utf8_strlen (text_to_append, -1) - 1,
						NULL);
					g_free (text_to_append);
					text_to_append = webkit_dom_node_get_text_content (node);
				}
			}
		}

		/* If parent is ANCHOR => we're editing the link */
		if ((!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) && !appending_to_link) || !text_to_append) {
			g_match_info_free (match_info);
			g_regex_unref (regex);
			g_free (node_text);
			g_free (text_to_append);
			goto out;
		}

		/* edit only if href and description are the same */
		href = webkit_dom_html_anchor_element_get_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent));

		if (appending_to_link) {
			gchar *inner_text;

			inner_text =
				webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (parent)),

			text = g_strconcat (inner_text, text_to_append, NULL);
			g_free (inner_text);
		} else
			text = webkit_dom_html_element_get_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (parent));

		element_remove_class (parent, "-x-evo-visited-link");

		if (strstr (href, "://") && !strstr (text, "://")) {
			url = strstr (href, "://") + 3;
			diff = strlen (text) - strlen (url);

			if (text [strlen (text) - 1] != '/')
				diff++;

			if ((g_strcmp0 (url, text) != 0 && ABS (diff) == 1) || appending_to_link) {
				gchar *inner_html, *protocol, *new_href;

				protocol = g_strndup (href, strstr (href, "://") - href + 3);
				inner_html = webkit_dom_element_get_inner_html (parent);
				new_href = g_strconcat (
					protocol, inner_html, appending_to_link ? text_to_append : "", NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					webkit_dom_element_insert_adjacent_html (
						WEBKIT_DOM_ELEMENT (parent),
						"beforeend",
						text_to_append,
						NULL);

					remove_node (node);
				}

				g_free (new_href);
				g_free (protocol);
				g_free (inner_html);
			}
		} else {
			diff = strlen (text) - strlen (href);
			if (text [strlen (text) - 1] != '/')
				diff++;

			if ((g_strcmp0 (href, text) != 0 && ABS (diff) == 1) || appending_to_link) {
				gchar *inner_html;
				gchar *new_href;

				inner_html = webkit_dom_element_get_inner_html (parent);
				new_href = g_strconcat (
						inner_html,
						appending_to_link ? text_to_append : "",
						NULL);

				webkit_dom_html_anchor_element_set_href (
					WEBKIT_DOM_HTML_ANCHOR_ELEMENT (parent),
					new_href);

				if (appending_to_link) {
					webkit_dom_element_insert_adjacent_html (
						WEBKIT_DOM_ELEMENT (parent),
						"beforeend",
						text_to_append,
						NULL);

					remove_node (node);
				}

				g_free (new_href);
				g_free (inner_html);
			}

		}
		g_free (text_to_append);
		g_free (text);
		g_free (href);
	}

	g_match_info_free (match_info);
	g_regex_unref (regex);
	g_free (node_text);

 out:
	if (!return_key_pressed)
		e_editor_dom_selection_restore (editor_page);
}

void
e_editor_dom_embed_style_sheet (EEditorPage *editor_page,
                                const gchar *style_sheet_content)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	e_dom_utils_create_and_add_css_style_sheet (document, "-x-evo-composer-sheet");

	sheet = webkit_dom_document_get_element_by_id (document, "-x-evo-composer-sheet");
	webkit_dom_element_set_attribute (
		sheet,
		"type",
		"text/css",
		NULL);

	webkit_dom_element_set_inner_html (sheet, style_sheet_content, NULL);
}

void
e_editor_dom_remove_embedded_style_sheet (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *sheet;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	sheet = webkit_dom_document_get_element_by_id (
		document, "-x-evo-composer-sheet");

	if (sheet)
		remove_node (WEBKIT_DOM_NODE (sheet));
}

static void
insert_delete_event (EEditorPage *editor_page,
                     WebKitDOMRange *range)
{
	EEditorHistoryEvent *ev;
	WebKitDOMDocumentFragment *fragment;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (e_editor_undo_redo_manager_is_operation_in_progress (manager))
		return;

	ev = g_new0 (EEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	fragment = webkit_dom_range_clone_contents (range, NULL);
	ev->data.fragment = g_object_ref (fragment);

	e_editor_dom_selection_get_coordinates (editor_page,
		&ev->before.start.x,
		&ev->before.start.y,
		&ev->before.end.x,
		&ev->before.end.y);

	ev->after.start.x = ev->before.start.x;
	ev->after.start.y = ev->before.start.y;
	ev->after.end.x = ev->before.start.x;
	ev->after.end.y = ev->before.start.y;

	e_editor_undo_redo_manager_insert_history_event (manager, ev);

	ev = g_new0 (EEditorHistoryEvent, 1);
	ev->type = HISTORY_AND;

	e_editor_undo_redo_manager_insert_history_event (manager, ev);
}

/* Based on original use_pictograms() from GtkHTML */
static const gchar *emoticons_chars =
	/*  0 */ "DO)(|/PQ*!"
	/* 10 */ "S\0:-\0:\0:-\0"
	/* 20 */ ":\0:;=-\"\0:;"
	/* 30 */ "B\"|\0:-'\0:X"
	/* 40 */ "\0:\0:-\0:\0:-"
	/* 50 */ "\0:\0:-\0:\0:-"
	/* 60 */ "\0:\0:\0:-\0:\0"
	/* 70 */ ":-\0:\0:-\0:\0";
static gint emoticons_states[] = {
	/*  0 */  12,  17,  22,  34,  43,  48,  53,  58,  65,  70,
	/* 10 */  75,   0, -15,  15,   0, -15,   0, -17,  20,   0,
	/* 20 */ -17,   0, -14, -20, -14,  28,  63,   0, -14, -20,
	/* 30 */  -3,  63, -18,   0, -12,  38,  41,   0, -12,  -2,
	/* 40 */   0,  -4,   0, -10,  46,   0, -10,   0, -19,  51,
	/* 50 */   0, -19,   0, -11,  56,   0, -11,   0, -13,  61,
	/* 60 */   0, -13,   0,  -6,   0,  68,  -7,   0,  -7,   0,
	/* 70 */ -16,  73,   0, -16,   0, -21,  78,   0, -21,   0 };
static const gchar *emoticons_icon_names[] = {
	"face-angel",
	"face-angry",
	"face-cool",
	"face-crying",
	"face-devilish",
	"face-embarrassed",
	"face-kiss",
	"face-laugh",		/* not used */
	"face-monkey",		/* not used */
	"face-plain",
	"face-raspberry",
	"face-sad",
	"face-sick",
	"face-smile",
	"face-smile-big",
	"face-smirk",
	"face-surprise",
	"face-tired",
	"face-uncertain",
	"face-wink",
	"face-worried"
};

typedef struct _EmoticonLoadContext {
	EEmoticon *emoticon;
	EEditorPage *editor_page;
	gchar *content_type;
	gchar *name;
} EmoticonLoadContext;

static EmoticonLoadContext *
emoticon_load_context_new (EEditorPage *editor_page,
                           EEmoticon *emoticon)
{
	EmoticonLoadContext *load_context;

	load_context = g_slice_new0 (EmoticonLoadContext);
	load_context->emoticon = emoticon;
	load_context->editor_page = editor_page;

	return load_context;
}

static void
emoticon_load_context_free (EmoticonLoadContext *load_context)
{
	g_free (load_context->content_type);
	g_free (load_context->name);
	g_slice_free (EmoticonLoadContext, load_context);
}

static void
emoticon_insert_span (EEmoticon *emoticon,
                      EmoticonLoadContext *load_context,
                      WebKitDOMElement *span)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	EEditorPage *editor_page = load_context->editor_page;
	gboolean misplaced_selection = FALSE, smiley_written;
	gchar *node_text = NULL;
	const gchar *emoticon_start;
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *node, *insert_before, *prev_sibling, *next_sibling;
	WebKitDOMNode *selection_end_marker_parent, *inserted_node;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	smiley_written = e_editor_page_get_is_smiley_written (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (e_editor_dom_selection_is_collapsed (editor_page)) {
		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (!smiley_written) {
			if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
				ev = g_new0 (EEditorHistoryEvent, 1);
				if (e_editor_page_get_unicode_smileys_enabled (editor_page))
					ev->type = HISTORY_INPUT;
				else {
					ev->type = HISTORY_SMILEY;

					e_editor_dom_selection_get_coordinates (editor_page,
						&ev->before.start.x,
						&ev->before.start.y,
						&ev->before.end.x,
						&ev->before.end.y);
				}
			}
		}
	} else {
		WebKitDOMRange *tmp_range = NULL;

		tmp_range = e_editor_dom_get_current_range (editor_page);
		insert_delete_event (editor_page, tmp_range);
		g_clear_object (&tmp_range);

		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DELETE, NULL);

		if (!smiley_written) {
			if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
				ev = g_new0 (EEditorHistoryEvent, 1);

				if (e_editor_page_get_unicode_smileys_enabled (editor_page))
					ev->type = HISTORY_INPUT;
				else {
					ev->type = HISTORY_SMILEY;

					e_editor_dom_selection_get_coordinates (editor_page,
						&ev->before.start.x,
						&ev->before.start.y,
						&ev->before.end.x,
						&ev->before.end.y);
				}
			}
		}

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
	}

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);

		if (ev && !e_editor_page_get_unicode_smileys_enabled (editor_page))
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);
	}

	/* Sometimes selection end marker is in body. Move it into next sibling */
	selection_end_marker_parent = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (selection_end_marker_parent)) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			WEBKIT_DOM_NODE (selection_start_marker),
			NULL);
		if (ev && !e_editor_page_get_unicode_smileys_enabled (editor_page))
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);
	}
	selection_end_marker_parent = webkit_dom_node_get_parent_node (
		WEBKIT_DOM_NODE (selection_end_marker));

	/* Determine before what node we have to insert the smiley */
	insert_before = WEBKIT_DOM_NODE (selection_start_marker);
	prev_sibling = webkit_dom_node_get_previous_sibling (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (prev_sibling) {
		if (webkit_dom_node_is_same_node (
			prev_sibling, WEBKIT_DOM_NODE (selection_end_marker))) {
			insert_before = WEBKIT_DOM_NODE (selection_end_marker);
		} else {
			prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
			if (prev_sibling &&
			    webkit_dom_node_is_same_node (
				prev_sibling, WEBKIT_DOM_NODE (selection_end_marker))) {
				insert_before = WEBKIT_DOM_NODE (selection_end_marker);
			}
		}
	} else
		insert_before = WEBKIT_DOM_NODE (selection_start_marker);

	/* Look if selection is misplaced - that means that the selection was
	 * restored before the previously inserted smiley in situations when we
	 * are writing more smileys in a row */
	next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
	if (next_sibling && WEBKIT_DOM_IS_ELEMENT (next_sibling))
		if (element_has_class (WEBKIT_DOM_ELEMENT (next_sibling), "-x-evo-smiley-wrapper"))
			misplaced_selection = TRUE;

	range = e_editor_dom_get_current_range (editor_page);
	node = webkit_dom_range_get_end_container (range, NULL);
	g_clear_object (&range);
	if (WEBKIT_DOM_IS_TEXT (node))
		node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));

	if (misplaced_selection) {
		/* Insert smiley and selection markers after it */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (insert_before),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_next_sibling (next_sibling),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (insert_before),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (next_sibling),
			NULL);
		if (e_editor_page_get_unicode_smileys_enabled (editor_page))
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (span)),
				webkit_dom_node_get_next_sibling (next_sibling),
				NULL);
		else
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				webkit_dom_node_get_next_sibling (next_sibling),
				NULL);
	} else {
		if (e_editor_page_get_unicode_smileys_enabled (editor_page))
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (span)),
				insert_before,
				NULL);
		else
			inserted_node = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (insert_before),
				WEBKIT_DOM_NODE (span),
				insert_before,
				NULL);
	}

	if (!e_editor_page_get_unicode_smileys_enabled (editor_page)) {
		/* &#8203 == UNICODE_ZERO_WIDTH_SPACE */
		webkit_dom_element_insert_adjacent_html (
			WEBKIT_DOM_ELEMENT (span), "afterend", "&#8203;", NULL);
	}

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (inserted_node), TRUE, NULL),
			NULL);
		if (e_editor_page_get_unicode_smileys_enabled (editor_page)) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, TRUE)),
				NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, FALSE)),
				NULL);
		} else
			webkit_dom_element_insert_adjacent_html (
				WEBKIT_DOM_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = g_object_ref (fragment);
	}

	/* Remove the text that represents the text version of smiley that was
	 * written into the composer. */
	if (node_text && smiley_written) {
		emoticon_start = g_utf8_strrchr (
			node_text, -1, g_utf8_get_char (emoticon->text_face));
		/* Check if the written smiley is really the one that we inserted. */
		if (emoticon_start) {
			/* The written smiley is the same as text version. */
			if (g_str_has_prefix (emoticon_start, emoticon->text_face)) {
				webkit_dom_character_data_delete_data (
					WEBKIT_DOM_CHARACTER_DATA (node),
					g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
					strlen (emoticon->text_face),
					NULL);
			} else if (strstr (emoticon->text_face, "-")) {
				gboolean same = TRUE, compensate = FALSE;
				gint ii = 0, jj = 0;

				/* Try to recognize smileys without the dash e.g. :). */
				while (emoticon_start[ii] && emoticon->text_face[jj]) {
					if (emoticon_start[ii] == emoticon->text_face[jj]) {
						if (emoticon->text_face[jj+1] && emoticon->text_face[jj+1] == '-') {
							ii++;
							jj+=2;
							compensate = TRUE;
						} else {
							ii++;
							jj++;
						}
					} else {
						same = FALSE;
						break;
					}
				}

				if (same) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						g_utf8_strlen (node_text, -1) - strlen (emoticon_start),
						ii,
						NULL);
				}
				/* If we recognize smiley without dash, but we inserted
				 * the text version with dash we need it insert new
				 * history input event with that dash. */
				if (compensate)
					e_editor_undo_redo_manager_insert_dash_history_event (manager);
			}
		}

		e_editor_page_set_is_smiley_written (editor_page, FALSE);
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);

	e_editor_page_emit_content_changed (editor_page);

	g_free (node_text);
}

static void
emoticon_read_async_cb (GFile *file,
                        GAsyncResult *result,
                        EmoticonLoadContext *load_context)
{
	EEmoticon *emoticon = load_context->emoticon;
	EEditorPage *editor_page = load_context->editor_page;
	GError *error = NULL;
	gboolean html_mode;
	gchar *mime_type;
	gchar *base64_encoded, *output, *data;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	gssize size;
	WebKitDOMElement *wrapper, *image, *smiley_text;
	WebKitDOMDocument *document;

	input_stream = g_file_read_finish (file, result, &error);
	g_return_if_fail (!error && input_stream);

	output_stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

	size = g_output_stream_splice (
		output_stream, G_INPUT_STREAM (input_stream),
		G_OUTPUT_STREAM_SPLICE_NONE, NULL, &error);

	if (error || (size == -1))
		goto out;

	mime_type = g_content_type_get_mime_type (load_context->content_type);

	data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream));
	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);

	html_mode = e_editor_page_get_html_mode (editor_page);
	document = e_editor_page_get_document (editor_page);

	/* Insert span with image representation and another one with text
	 * representation and hide/show them dependant on active composer mode */
	wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
	if (html_mode)
		webkit_dom_element_set_attribute (
			wrapper, "class", "-x-evo-smiley-wrapper -x-evo-resizable-wrapper", NULL);
	else
		webkit_dom_element_set_attribute (
			wrapper, "class", "-x-evo-smiley-wrapper", NULL);

	image = webkit_dom_document_create_element (document, "IMG", NULL);
	webkit_dom_element_set_attribute (image, "src", output, NULL);
	webkit_dom_element_set_attribute (image, "data-inline", "", NULL);
	webkit_dom_element_set_attribute (image, "data-name", load_context->name, NULL);
	webkit_dom_element_set_attribute (image, "alt", emoticon->text_face, NULL);
	webkit_dom_element_set_attribute (image, "class", "-x-evo-smiley-img", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (image), NULL);

	smiley_text = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (smiley_text, "class", "-x-evo-smiley-text", NULL);
	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (smiley_text), emoticon->text_face, NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (smiley_text), NULL);

	emoticon_insert_span (emoticon, load_context, wrapper);

	g_free (base64_encoded);
	g_free (output);
	g_free (mime_type);
	g_object_unref (output_stream);
 out:
	emoticon_load_context_free (load_context);
}

static void
emoticon_query_info_async_cb (GFile *file,
                              GAsyncResult *result,
                              EmoticonLoadContext *load_context)
{
	GError *error = NULL;
	GFileInfo *info;

	info = g_file_query_info_finish (file, result, &error);
	g_return_if_fail (!error && info);

	load_context->content_type = g_strdup (g_file_info_get_content_type (info));
	load_context->name = g_strdup (g_file_info_get_name (info));

	g_file_read_async (
		file, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) emoticon_read_async_cb, load_context);

	g_object_unref (info);
}

void
e_editor_dom_insert_smiley (EEditorPage *editor_page,
                            EEmoticon *emoticon)
{
	WebKitDOMDocument *document;
	GFile *file;
	gchar *filename_uri;
	EmoticonLoadContext *load_context;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	if (e_editor_page_get_unicode_smileys_enabled (editor_page)) {
		WebKitDOMElement *wrapper;

		wrapper = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (wrapper), emoticon->unicode_character, NULL);

		load_context = emoticon_load_context_new (editor_page, emoticon);
		emoticon_insert_span (emoticon, load_context, wrapper);
		emoticon_load_context_free (load_context);
	} else {
		filename_uri = e_emoticon_get_uri (emoticon);
		g_return_if_fail (filename_uri != NULL);

		load_context = emoticon_load_context_new (editor_page, emoticon);

		file = g_file_new_for_uri (filename_uri);
		g_file_query_info_async (
			file,  "standard::*", G_FILE_QUERY_INFO_NONE,
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) emoticon_query_info_async_cb, load_context);

		g_free (filename_uri);
		g_object_unref (file);
	}
}

void
e_editor_dom_insert_smiley_by_name (EEditorPage *editor_page,
                                    const gchar *name)
{
	const EEmoticon *emoticon;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	emoticon = e_emoticon_chooser_lookup_emoticon (name);
	e_editor_page_set_is_smiley_written (editor_page, FALSE);
	e_editor_dom_insert_smiley (editor_page, (EEmoticon *) emoticon);
}

void
e_editor_dom_check_magic_smileys (EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	gint pos, state, relative, start;
	gchar *node_text;
	gunichar uc;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_magic_smileys_enabled (editor_page))
		return;

	range = e_editor_dom_get_current_range (editor_page);
	node = webkit_dom_range_get_end_container (range, NULL);
	if (!WEBKIT_DOM_IS_TEXT (node)) {
		g_clear_object (&range);
		return;
	}

	node_text = webkit_dom_text_get_whole_text (WEBKIT_DOM_TEXT (node));
	if (node_text == NULL) {
		g_clear_object (&range);
		return;
	}

	start = webkit_dom_range_get_end_offset (range, NULL) - 1;
	pos = start;
	state = 0;
	while (pos >= 0) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos));
		relative = 0;
		while (emoticons_chars[state + relative]) {
			if (emoticons_chars[state + relative] == uc)
				break;
			relative++;
		}
		state = emoticons_states[state + relative];
		/* 0 .. not found, -n .. found n-th */
		if (state <= 0)
			break;
		pos--;
	}

	/* Special case needed to recognize angel and devilish. */
	if (pos > 0 && state == -14) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
		if (uc == 'O') {
			state = -1;
			pos--;
		} else if (uc == '>') {
			state = -5;
			pos--;
		}
	}

	if (state < 0) {
		const EEmoticon *emoticon;

		if (pos > 0) {
			uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
			if (!g_unichar_isspace (uc)) {
				g_free (node_text);
				g_clear_object (&range);
				return;
			}
		}

		emoticon = e_emoticon_chooser_lookup_emoticon (
			emoticons_icon_names[-state - 1]);
		e_editor_page_set_is_smiley_written (editor_page, TRUE);
		e_editor_dom_insert_smiley (editor_page, (EEmoticon *) emoticon);
	}

	g_clear_object (&range);
	g_free (node_text);
}

static void
dom_set_links_active (WebKitDOMDocument *document,
                      gboolean active)
{
	WebKitDOMElement *style;

	style = webkit_dom_document_get_element_by_id (document, "-x-evo-style-a");
	if (style)
		remove_node (WEBKIT_DOM_NODE (style));

	if (!active) {
		WebKitDOMHTMLHeadElement *head;
		head = webkit_dom_document_get_head (document);

		style = webkit_dom_document_create_element (document, "STYLE", NULL);
		webkit_dom_element_set_id (style, "-x-evo-style-a");
		webkit_dom_element_set_attribute (style, "type", "text/css", NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (style), "a { cursor: text; }", NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style), NULL);
	}
}

static void
fix_paragraph_structure_after_pressing_enter_after_smiley (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_query_selector (
		document, "span.-x-evo-smiley-wrapper > br", NULL);

	if (element) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		webkit_dom_element_set_inner_html (
			webkit_dom_node_get_parent_element (parent),
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);
	}
}

static gboolean
fix_paragraph_structure_after_pressing_enter (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *body, *prev_sibling, *node;
	WebKitDOMElement *br;
	gboolean prev_is_heading = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	e_editor_dom_selection_save (editor_page);

	/* When pressing Enter on empty line in the list (or after heading elements)
	 * WebKit will end that list and inserts <div><br></div> so replace it
	 * with the right paragraph element. */
	br = webkit_dom_document_query_selector (
		document, "body > div:not([data-evo-paragraph]) > #-x-evo-selection-end-marker + br", NULL);

	if (!br || webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (br)) ||
	     webkit_dom_node_get_previous_sibling (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (br))))
		goto out;

	node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (br));

	prev_sibling = webkit_dom_node_get_previous_sibling (node);
	if (prev_sibling && WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (prev_sibling))
		prev_is_heading = TRUE;

	webkit_dom_node_replace_child (
		body,
		WEBKIT_DOM_NODE (e_editor_dom_prepare_paragraph (editor_page, FALSE)),
		node,
		NULL);

 out:
	e_editor_dom_selection_restore (editor_page);

	return prev_is_heading;
}

static gboolean
surround_text_with_paragraph_if_needed (EEditorPage *editor_page,
                                        WebKitDOMNode *node)
{
	WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (node);
	WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (node);
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	WebKitDOMElement *element;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	/* All text in composer has to be written in div elements, so if
	 * we are writing something straight to the body, surround it with
	 * paragraph */
	if (WEBKIT_DOM_IS_TEXT (node) &&
	    (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent) ||
	     WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent))) {
		element = e_editor_dom_put_node_into_paragraph (editor_page, node, TRUE);
		if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent))
			webkit_dom_element_remove_attribute (element, "style");

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
			remove_node (next_sibling);

		/* Tab character */
		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "Apple-tab-span")) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (element),
				prev_sibling,
				webkit_dom_node_get_first_child (
					WEBKIT_DOM_NODE (element)),
				NULL);
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
selection_is_in_table (WebKitDOMDocument *document,
                       gboolean *first_cell,
                       WebKitDOMNode **table_node)
{
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *node, *parent;
	WebKitDOMRange *range = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (first_cell != NULL)
		*first_cell = FALSE;

	if (table_node != NULL)
		*table_node = NULL;

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_clear_object (&dom_selection);
		return FALSE;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	node = webkit_dom_range_get_start_container (range, NULL);
	g_clear_object (&dom_selection);

	parent = node;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (parent)) {
			if (first_cell != NULL) {
				if (!webkit_dom_node_get_previous_sibling (parent)) {
					gboolean on_start = TRUE;
					WebKitDOMNode *tmp;

					tmp = webkit_dom_node_get_previous_sibling (node);
					if (!tmp && WEBKIT_DOM_IS_TEXT (node))
						on_start = webkit_dom_range_get_start_offset (range, NULL) == 0;
					else if (tmp)
						on_start = FALSE;

					if (on_start) {
						node = webkit_dom_node_get_parent_node (parent);
						if (node && WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (node))
							if (!webkit_dom_node_get_previous_sibling (node))
								*first_cell = TRUE;
					}
				}
			} else {
				g_clear_object (&range);
				return TRUE;
			}
		}
		if (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (parent)) {
			if (table_node != NULL)
				*table_node = parent;
			else {
				g_clear_object (&range);
				return TRUE;
			}
		}
		parent = webkit_dom_node_get_parent_node (parent);
	}

	g_clear_object (&range);

	if (table_node == NULL)
		return FALSE;

	return *table_node != NULL;
}

static gboolean
jump_to_next_table_cell (WebKitDOMDocument *document,
                         gboolean jump_back)
{
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *node, *cell;
	WebKitDOMRange *range = NULL;

	if (!selection_is_in_table (document, NULL, NULL))
		return FALSE;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	node = webkit_dom_range_get_start_container (range, NULL);

	cell = node;
	while (cell && !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		cell = webkit_dom_node_get_parent_node (cell);
	}

	if (!WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (cell)) {
		g_clear_object (&range);
		g_clear_object (&dom_selection);
		return FALSE;
	}

	if (jump_back) {
		/* Get previous cell */
		node = webkit_dom_node_get_previous_sibling (cell);
		if (!node || !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node)) {
			/* No cell, go one row up. */
			node = webkit_dom_node_get_parent_node (cell);
			node = webkit_dom_node_get_previous_sibling (node);
			if (node && WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (node)) {
				node = webkit_dom_node_get_last_child (node);
			} else {
				/* No row above, move to the block before table. */
				node = webkit_dom_node_get_parent_node (cell);
				while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node)))
					node = webkit_dom_node_get_parent_node (node);

				node = webkit_dom_node_get_previous_sibling (node);
			}
		}
	} else {
		/* Get next cell */
		node = webkit_dom_node_get_next_sibling (cell);
		if (!node || !WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node)) {
			/* No cell, go one row below. */
			node = webkit_dom_node_get_parent_node (cell);
			node = webkit_dom_node_get_next_sibling (node);
			if (node && WEBKIT_DOM_IS_HTML_TABLE_ROW_ELEMENT (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else {
				/* No row below, move to the block after table. */
				node = webkit_dom_node_get_parent_node (cell);
				while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node)))
					node = webkit_dom_node_get_parent_node (node);

				node = webkit_dom_node_get_next_sibling (node);
			}
		}
	}

	if (!node) {
		g_clear_object (&range);
		g_clear_object (&dom_selection);
		return FALSE;
	}

	webkit_dom_range_select_node_contents (range, node, NULL);
	webkit_dom_range_collapse (range, TRUE, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&range);
	g_clear_object (&dom_selection);

	return TRUE;
}

static gboolean
save_history_before_event_in_table (EEditorPage *editor_page,
                                    WebKitDOMRange *range)
{
	WebKitDOMNode *node;
	WebKitDOMElement *block;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		block = WEBKIT_DOM_ELEMENT (node);
	else
		block = get_parent_block_element (node);

	if (block && WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (block)) {
		EEditorUndoRedoManager *manager;
		EEditorHistoryEvent *ev;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_INPUT;

		e_editor_dom_selection_save (editor_page);
		ev->data.dom.from = g_object_ref (webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (block), TRUE, NULL));
		e_editor_dom_selection_restore (editor_page);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		manager = e_editor_page_get_undo_redo_manager (editor_page);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);

		return TRUE;
	}

	return FALSE;
}

static gboolean
insert_tabulator (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	gboolean success;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		if (!e_editor_dom_selection_is_collapsed (editor_page)) {
			WebKitDOMRange *tmp_range = NULL;

			tmp_range = e_editor_dom_get_current_range (editor_page);
			insert_delete_event (editor_page, tmp_range);
			g_clear_object (&tmp_range);
		}

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->before.end.x = ev->before.start.x;
		ev->before.end.y = ev->before.start.y;
	}

	success = e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_TEXT, "\t");

	if (ev) {
		if (success) {
			WebKitDOMDocument *document;
			WebKitDOMElement *element;
			WebKitDOMDocumentFragment *fragment;

			document = e_editor_page_get_document (editor_page);

			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
			element = webkit_dom_document_create_element (document, "span", NULL);
			webkit_dom_html_element_set_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (element), "\t", NULL);
			webkit_dom_element_set_attribute (
				element, "class", "Apple-tab-span", NULL);
			webkit_dom_element_set_attribute (
				element, "style", "white-space:pre", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment), WEBKIT_DOM_NODE (element), NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (dom_create_selection_marker (document, TRUE)),
				NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (dom_create_selection_marker (document, FALSE)),
				NULL);
			ev->data.fragment = g_object_ref (fragment);

			e_editor_undo_redo_manager_insert_history_event (manager, ev);
			e_editor_page_emit_content_changed (editor_page);
		} else {
			e_editor_undo_redo_manager_remove_current_history_event (manager);
			e_editor_undo_redo_manager_remove_current_history_event (manager);
			g_free (ev);
		}
	}

	return success;
}

static void
body_keypress_event_cb (WebKitDOMElement *element,
                        WebKitDOMUIEvent *event,
                        EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_page_set_is_processing_keypress_event (editor_page, TRUE);

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (!webkit_dom_range_get_collapsed (range, NULL))
		insert_delete_event (editor_page, range);

	g_clear_object (&dom_selection);
	g_clear_object (&range);
}

static void
body_keydown_event_cb (WebKitDOMElement *element,
                       WebKitDOMUIEvent *event,
                       EEditorPage *editor_page)
{
	gboolean backspace_key, delete_key, space_key, return_key;
	gboolean shift_key, control_key, tabulator_key;
	glong key_code;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));

	key_code = webkit_dom_ui_event_get_key_code (event);
	delete_key = key_code == HTML_KEY_CODE_DELETE;
	return_key = key_code == HTML_KEY_CODE_RETURN;
	backspace_key = key_code == HTML_KEY_CODE_BACKSPACE;
	space_key = key_code == HTML_KEY_CODE_SPACE;
	tabulator_key = key_code == HTML_KEY_CODE_TABULATOR;

	if (key_code == HTML_KEY_CODE_CONTROL) {
		dom_set_links_active (document, TRUE);
		return;
	}

	e_editor_page_set_dont_save_history_in_body_input (editor_page, delete_key || backspace_key);

	e_editor_page_set_return_key_pressed (editor_page, return_key);
	e_editor_page_set_space_key_pressed (editor_page, space_key);

	if (!(delete_key || return_key || backspace_key || space_key || tabulator_key))
		return;

	shift_key = webkit_dom_keyboard_event_get_shift_key (WEBKIT_DOM_KEYBOARD_EVENT (event));
	control_key = webkit_dom_keyboard_event_get_ctrl_key (WEBKIT_DOM_KEYBOARD_EVENT (event));

	if (tabulator_key) {
		if (jump_to_next_table_cell (document, shift_key)) {
			webkit_dom_event_prevent_default (WEBKIT_DOM_EVENT (event));
			goto out;
		}

		if (!shift_key && insert_tabulator (editor_page))
			webkit_dom_event_prevent_default (WEBKIT_DOM_EVENT (event));

		goto out;
	}

	if (return_key && e_editor_dom_key_press_event_process_return_key (editor_page)) {
		webkit_dom_event_prevent_default (WEBKIT_DOM_EVENT (event));
		goto out;
	}

	if (backspace_key && e_editor_dom_key_press_event_process_backspace_key (editor_page)) {
		webkit_dom_event_prevent_default (WEBKIT_DOM_EVENT (event));
		goto out;
	}

	if (delete_key || backspace_key) {
		if (e_editor_dom_key_press_event_process_delete_or_backspace_key (editor_page, key_code, control_key, delete_key))
			webkit_dom_event_prevent_default (WEBKIT_DOM_EVENT (event));
		goto out;
	}

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (save_history_before_event_in_table (editor_page, range))
		goto out;

	if (return_key) {
		EEditorHistoryEvent *ev;
		EEditorUndoRedoManager *manager;

		/* Insert new history event for Return to have the right coordinates.
		 * The fragment will be added later. */
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		manager = e_editor_page_get_undo_redo_manager (editor_page);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
 out:
	g_clear_object (&range);
	g_clear_object (&dom_selection);
}

static gboolean
save_history_after_event_in_table (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	EEditorHistoryEvent *ev;
	EEditorUndoRedoManager *manager;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_clear_object (&dom_selection);
		return FALSE;
	}
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	/* Find if writing into table. */
	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = get_parent_block_element (node);

	g_clear_object (&dom_selection);
	g_clear_object (&range);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	/* If writing to table we have to create different history event. */
	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element)) {
		ev = e_editor_undo_redo_manager_get_current_history_event (manager);
		if (ev->type != HISTORY_TABLE_INPUT)
			return FALSE;
	} else
		return FALSE;

	e_editor_dom_selection_save (editor_page);

	e_editor_dom_selection_get_coordinates (editor_page,
		&ev->after.start.x,
		&ev->after.start.y,
		&ev->after.end.x,
		&ev->after.end.y);

	ev->data.dom.to = g_object_ref (webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (element), TRUE, NULL));

	e_editor_dom_selection_restore (editor_page);

	return TRUE;
}

static void
save_history_for_input (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL, *range_clone = NULL;
	WebKitDOMNode *start_container;
	EEditorHistoryEvent *ev;
	EEditorUndoRedoManager *manager;
	glong offset;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_clear_object (&dom_selection);
		return;
	}

	if (e_editor_page_get_return_key_pressed (editor_page)) {
		ev = e_editor_undo_redo_manager_get_current_history_event (manager);
		if (ev->type != HISTORY_INPUT) {
			g_clear_object (&dom_selection);
			return;
		}
	} else {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;
	}

	e_editor_page_block_selection_changed (editor_page);

	e_editor_dom_selection_get_coordinates (editor_page,
		&ev->after.start.x,
		&ev->after.start.y,
		&ev->after.end.x,
		&ev->after.end.y);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);
	offset = webkit_dom_range_get_start_offset (range_clone, NULL);
	start_container = webkit_dom_range_get_start_container (range_clone, NULL);
	if (offset > 0)
		webkit_dom_range_set_start (
			range_clone,
			start_container,
			offset - 1,
			NULL);
	fragment = webkit_dom_range_clone_contents (range_clone, NULL);
	/* We have to specially handle Return key press */
	if (e_editor_page_get_return_key_pressed (editor_page)) {
		WebKitDOMElement *element_start, *element_end;
		WebKitDOMNode *parent_start, *parent_end, *node;

		element_start = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element_start), NULL);
		webkit_dom_dom_selection_modify (dom_selection, "move", "left", "character");
		g_clear_object (&range);
		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		element_end = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (element_end), NULL);

		parent_start = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element_start));
		parent_end = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element_end));

		while (parent_start && parent_end && !webkit_dom_node_is_same_node (parent_start, parent_end) &&
		       !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_start) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent_end)) {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node_with_error (parent_start, FALSE, NULL),
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
				NULL);
			parent_start = webkit_dom_node_get_parent_node (parent_start);
			parent_end = webkit_dom_node_get_parent_node (parent_end);
		}

		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		while (webkit_dom_node_get_next_sibling (node)) {
			WebKitDOMNode *last_child;

			last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
			webkit_dom_node_append_child (
				webkit_dom_node_get_previous_sibling (last_child),
				last_child,
				NULL);
		}

		node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));
		while (webkit_dom_node_get_last_child (node)) {
			node = webkit_dom_node_get_last_child (node);
		}

		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_element (document, "br", NULL)),
			NULL);
		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			node,
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, FALSE)),
			NULL);

		remove_node (WEBKIT_DOM_NODE (element_start));
		remove_node (WEBKIT_DOM_NODE (element_end));

		g_object_set_data (
			G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));

		webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");
	} else {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, TRUE)),
			NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				dom_create_selection_marker (document, FALSE)),
			NULL);
	}

	g_clear_object (&dom_selection);
	g_clear_object (&range);
	g_clear_object (&range_clone);

	e_editor_page_unblock_selection_changed (editor_page);

	ev->data.fragment = g_object_ref (fragment);

	if (!e_editor_page_get_return_key_pressed (editor_page))
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
}

typedef struct _TimeoutContext TimeoutContext;

struct _TimeoutContext {
	EEditorPage *editor_page;
};

static void
timeout_context_free (TimeoutContext *context)
{
	g_slice_free (TimeoutContext, context);
}

static gboolean
force_spell_check_on_timeout (TimeoutContext *context)
{
	e_editor_dom_force_spell_check_in_viewport (context->editor_page);
	e_editor_page_set_spell_check_on_scroll_event_source_id (context->editor_page, 0);
	return FALSE;
}

static void
body_scroll_event_cb (WebKitDOMElement *element,
                      WebKitDOMEvent *event,
                      EEditorPage *editor_page)
{
	TimeoutContext *context;
	guint id;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_inline_spelling_enabled (editor_page))
		return;

	context = g_slice_new0 (TimeoutContext);
	context->editor_page = editor_page;

	id = e_editor_page_get_spell_check_on_scroll_event_source_id (editor_page);
	if (id > 0)
		g_source_remove (id);

	id = g_timeout_add_seconds_full (
		1,
		G_PRIORITY_DEFAULT,
		(GSourceFunc)force_spell_check_on_timeout,
		context,
		(GDestroyNotify)timeout_context_free);

	e_editor_page_set_spell_check_on_scroll_event_source_id (editor_page, id);
}

static void
remove_zero_width_spaces_on_body_input (EEditorPage *editor_page,
                                        WebKitDOMNode *node)
{
	gboolean html_mode;

	html_mode = e_editor_page_get_html_mode (editor_page);
	/* After toggling monospaced format, we are using UNICODE_ZERO_WIDTH_SPACE
	 * to move caret into right space. When this callback is called it is not
	 * necessary anymore so remove it */
	if (html_mode) {
		WebKitDOMElement *parent = webkit_dom_node_get_parent_element (node);

		if (parent) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (parent));

			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *text = webkit_dom_node_get_text_content (
					prev_sibling);

				if (g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0)
					remove_node (prev_sibling);

				g_free (text);
			}

		}
	}

	/* If text before caret includes UNICODE_ZERO_WIDTH_SPACE character, remove it */
	if (WEBKIT_DOM_IS_TEXT (node)) {
		gchar *text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (node));
		glong length = webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node));
		WebKitDOMNode *parent;

		/* We have to preserve empty paragraphs with just UNICODE_ZERO_WIDTH_SPACE
		 * character as when we will remove it it will collapse */
		if (length > 1) {
			if (g_str_has_prefix (text, UNICODE_ZERO_WIDTH_SPACE))
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
			else if (g_str_has_suffix (text, UNICODE_ZERO_WIDTH_SPACE))
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (node), length - 1, 1, "", NULL);
		}
		g_free (text);

		parent = webkit_dom_node_get_parent_node (node);
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) &&
		    !webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-evo-paragraph")) {
			if (html_mode)
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (parent),
					"data-evo-paragraph",
					"",
					NULL);
			else
				e_editor_dom_set_paragraph_style (
					editor_page, WEBKIT_DOM_ELEMENT (parent), -1, 0, NULL);
		}

		/* When new smiley is added we have to use UNICODE_HIDDEN_SPACE to set the
		 * caret position to right place. It is removed when user starts typing. But
		 * when the user will press left arrow he will move the caret into
		 * smiley wrapper. If he will start to write there we have to move the written
		 * text out of the wrapper and move caret to right place */
		if (WEBKIT_DOM_IS_ELEMENT (parent) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text")) {
			gchar *text;
			WebKitDOMCharacterData *data;
			WebKitDOMText *text_node;
			WebKitDOMDocument *document;

			document = e_editor_page_get_document (editor_page);

			/* Split out the newly written character to its own text node, */
			data = WEBKIT_DOM_CHARACTER_DATA (node);
			parent = webkit_dom_node_get_parent_node (parent);
			text = webkit_dom_character_data_substring_data (
				data,
				webkit_dom_character_data_get_length (data) - 1,
				1,
				NULL);
			webkit_dom_character_data_delete_data (
				data,
				webkit_dom_character_data_get_length (data) - 1,
				1,
				NULL);
			text_node = webkit_dom_document_create_text_node (document, text);
			g_free (text);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, FALSE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (
					dom_create_selection_marker (document, TRUE)),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			/* Move the text node outside of smiley. */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (text_node),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
			e_editor_dom_selection_restore (editor_page);
		}
	}
}

void
e_editor_dom_body_input_event_process (EEditorPage *editor_page,
                                       WebKitDOMEvent *event)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	EEditorUndoRedoManager *manager;
	gboolean do_spell_check = FALSE;
	gboolean html_mode;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	range = e_editor_dom_get_current_range (editor_page);
	node = webkit_dom_range_get_end_container (range, NULL);

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	html_mode = e_editor_page_get_html_mode (editor_page);
	e_editor_page_emit_content_changed (editor_page);

	if (e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		e_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
		e_editor_page_set_dont_save_history_in_body_input (editor_page, FALSE);
		remove_zero_width_spaces_on_body_input (editor_page, node);
		do_spell_check = TRUE;
		goto out;
	}

	/* When the Backspace is pressed in a bulleted list item with just one
	 * character left in it, WebKit will create another BR element in the
	 * item. */
	if (!html_mode) {
		WebKitDOMElement *element;

		element = webkit_dom_document_query_selector (
			document, "ul > li > br + br", NULL);

		if (element)
			remove_node (WEBKIT_DOM_NODE (element));
	}

	if (!save_history_after_event_in_table (editor_page)) {
		if (!e_editor_page_get_dont_save_history_in_body_input (editor_page))
			save_history_for_input (editor_page);
		else
			do_spell_check = TRUE;
	}

	/* Don't try to look for smileys if we are deleting text. */
	if (!e_editor_page_get_dont_save_history_in_body_input (editor_page))
		e_editor_dom_check_magic_smileys (editor_page);

	e_editor_page_set_dont_save_history_in_body_input (editor_page, FALSE);

	if (e_editor_page_get_return_key_pressed (editor_page) ||
	    e_editor_page_get_space_key_pressed (editor_page)) {
		e_editor_dom_check_magic_links (editor_page, FALSE);
		if (e_editor_page_get_return_key_pressed (editor_page)) {
			if (fix_paragraph_structure_after_pressing_enter (editor_page) &&
			    html_mode) {
				/* When the return is pressed in a H1-6 element, WebKit doesn't
				 * continue with the same element, but creates normal paragraph,
				 * so we have to unset the bold font. */
				e_editor_undo_redo_manager_set_operation_in_progress (manager, TRUE);
				e_editor_dom_selection_set_bold (editor_page, FALSE);
				e_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
			}

			fix_paragraph_structure_after_pressing_enter_after_smiley (document);

			do_spell_check = TRUE;
		}
	} else {
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);

		if (surround_text_with_paragraph_if_needed (editor_page, node)) {
			WebKitDOMElement *element;

			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			e_editor_dom_selection_restore (editor_page);
		}

		if (WEBKIT_DOM_IS_TEXT (node)) {
			WebKitDOMElement *parent;
			gchar *text;

			text = webkit_dom_node_get_text_content (node);

			if (text && *text && *text != ' ' && !g_str_has_prefix (text, UNICODE_NBSP)) {
				gboolean valid = FALSE;

				if (*text == '?' && strlen (text) > 1)
					valid = TRUE;
				else if (!strchr (URL_INVALID_TRAILING_CHARS, *text))
					valid = TRUE;

				if (valid) {
					WebKitDOMNode *prev_sibling;

					prev_sibling = webkit_dom_node_get_previous_sibling (node);

					if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling))
						e_editor_dom_check_magic_links (editor_page, FALSE);
				}
			}

			parent = webkit_dom_node_get_parent_element (node);
			if (element_has_class (parent, "-x-evo-resizable-wrapper") ||
			    element_has_class (parent, "-x-evo-smiley-wrapper")) {
				WebKitDOMDOMWindow *dom_window = NULL;
				WebKitDOMDOMSelection *dom_selection = NULL;
				WebKitDOMNode *prev_sibling;
				gboolean writing_before = TRUE;

				dom_window = webkit_dom_document_get_default_view (document);
				dom_selection = webkit_dom_dom_window_get_selection (dom_window);

				prev_sibling = webkit_dom_node_get_previous_sibling (node);
				if (prev_sibling && WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (prev_sibling))
					writing_before = FALSE;

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
					node,
					writing_before ?
						WEBKIT_DOM_NODE (parent) :
						webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
					NULL);

				g_clear_object (&range);

				range = webkit_dom_document_create_range (document);
				webkit_dom_range_select_node_contents (range, node, NULL);
				webkit_dom_range_collapse (range, FALSE, NULL);

				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range);

				g_clear_object (&dom_window);
				g_clear_object (&dom_selection);
			}

			g_free (text);
		}
	}

	remove_zero_width_spaces_on_body_input (editor_page, node);

	/* Writing into quoted content */
	if (!html_mode) {
		gint citation_level;
		WebKitDOMElement *selection_start_marker, *selection_end_marker;
		WebKitDOMNode *node, *parent;

		node = webkit_dom_range_get_end_container (range, NULL);

		citation_level = e_editor_dom_get_citation_level (node, FALSE);
		if (citation_level == 0)
			goto out;

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (selection_start_marker)
			goto out;

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		/* If the selection was not saved, move it into the first child of body */
		if (!selection_start_marker || !selection_end_marker) {
			WebKitDOMHTMLElement *body;
			WebKitDOMNode *child;

			body = webkit_dom_document_get_body (document);
			child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

			dom_add_selection_markers_into_element_start (
				document,
				WEBKIT_DOM_ELEMENT (child),
				&selection_start_marker,
				&selection_end_marker);
		}

		/* We have to process elements only inside normal block */
		parent = WEBKIT_DOM_NODE (get_parent_block_element (
			WEBKIT_DOM_NODE (selection_start_marker)));
		if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent)) {
			e_editor_dom_selection_restore (editor_page);
			goto out;
		}

		if (selection_start_marker) {
			gchar *content;
			gint text_length, word_wrap_length, length;
			WebKitDOMElement *block;
			gboolean remove_quoting = FALSE;

			word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
			length = word_wrap_length - 2 * citation_level;

			block = WEBKIT_DOM_ELEMENT (parent);
			if (webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (block), ".-x-evo-quoted", NULL)) {
				WebKitDOMNode *prev_sibling;

				prev_sibling = webkit_dom_node_get_previous_sibling (
					WEBKIT_DOM_NODE (selection_end_marker));

				if (WEBKIT_DOM_IS_ELEMENT (prev_sibling))
					remove_quoting = element_has_class (
						WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted");
			}

			content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (block));
			text_length = g_utf8_strlen (content, -1);
			g_free (content);

			/* Wrap and quote the line */
			if (!remove_quoting && text_length >= word_wrap_length) {
				e_editor_dom_remove_quoting_from_element (block);

				block = e_editor_dom_wrap_paragraph_length (editor_page, block, length);
				webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
				e_editor_dom_quote_plain_text_element_after_wrapping (
					editor_page, WEBKIT_DOM_ELEMENT (block), citation_level);
				selection_start_marker = webkit_dom_document_get_element_by_id (
					document, "-x-evo-selection-start-marker");
				if (!selection_start_marker)
					dom_add_selection_markers_into_element_end (
						document,
						WEBKIT_DOM_ELEMENT (block),
						NULL,
						NULL);

				e_editor_dom_selection_restore (editor_page);
				do_spell_check = TRUE;
				goto out;
			}
		}
		e_editor_dom_selection_restore (editor_page);
	}
 out:
	if (do_spell_check)
		e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	g_clear_object (&range);
}

static void
body_input_event_cb (WebKitDOMElement *element,
                     WebKitDOMEvent *event,
                     EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	/* Only process the input event if it was triggered by the key press
	 * and not i.e. by exexCommand. This behavior changed when the support
	 * for beforeinput event was introduced in WebKit. */
	if (e_editor_page_is_processing_keypress_event (editor_page))
		e_editor_dom_body_input_event_process (editor_page, event);

	e_editor_page_set_is_processing_keypress_event (editor_page, FALSE);
}

void
e_editor_dom_remove_input_event_listener_from_body (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_body_input_event_removed (editor_page)) {
		WebKitDOMDocument *document;

		document = e_editor_page_get_document (editor_page);

		webkit_dom_event_target_remove_event_listener (
			WEBKIT_DOM_EVENT_TARGET (webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE);

		e_editor_page_set_body_input_event_removed (editor_page, TRUE);
	}
}

void
e_editor_dom_register_input_event_listener_on_body (EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_body_input_event_removed (editor_page)) {
		WebKitDOMDocument *document;

		document = e_editor_page_get_document (editor_page);

		webkit_dom_event_target_add_event_listener (
			WEBKIT_DOM_EVENT_TARGET (webkit_dom_document_get_body (document)),
			"input",
			G_CALLBACK (body_input_event_cb),
			FALSE,
			editor_page);

		e_editor_page_set_body_input_event_removed (editor_page, FALSE);
	}
}

static void
remove_empty_blocks (WebKitDOMDocument *document)
{
	gint ii;
	WebKitDOMNodeList *list = NULL;

	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > :empty:not(br)", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_clear_object (&list);

	list = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite]:empty", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_clear_object (&list);
}

/* Following two functions are used when deleting the selection inside
 * the quoted content. The thing is that normally the quote marks are not
 * selectable by user. But this caused a lof of problems for WebKit when removing
 * the selection. This will avoid it as when the delete or backspace key is pressed
 * we will make the quote marks user selectable so they will act as any other text.
 * On HTML keyup event callback we will make them again non-selectable. */
void
e_editor_dom_disable_quote_marks_select (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *style_element;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	head = webkit_dom_document_get_head (document);

	if (!webkit_dom_document_get_element_by_id (document, "-x-evo-quote-style")) {
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (style_element, "-x-evo-quote-style");
		webkit_dom_element_set_attribute (style_element, "type", "text/css", NULL);
		webkit_dom_element_set_inner_html (
			style_element,
			".-x-evo-quoted { -webkit-user-select: none; }",
			NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style_element), NULL);
	}
}

static void
enable_quote_marks_select (WebKitDOMDocument *document)
{
	WebKitDOMElement *style_element;

	if ((style_element = webkit_dom_document_get_element_by_id (document, "-x-evo-quote-style")))
		remove_node (WEBKIT_DOM_NODE (style_element));
}

void
e_editor_dom_remove_node_and_parents_if_empty (WebKitDOMNode *node)
{
	WebKitDOMNode *parent;

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (node));

	remove_node (WEBKIT_DOM_NODE (node));

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		WebKitDOMNode *tmp;

		tmp = webkit_dom_node_get_parent_node (parent);
		remove_node_if_empty (parent);
		parent = tmp;
	}
}

void
e_editor_dom_merge_siblings_if_necessary (EEditorPage *editor_page,
                                          WebKitDOMDocumentFragment *deleted_content)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *prev_element;
	WebKitDOMNode *child;
	WebKitDOMNodeList *list = NULL;
	gboolean equal_nodes;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-main-cite")))
		webkit_dom_element_remove_attribute (element, "id");

	element = webkit_dom_document_query_selector (document, "blockquote:not([data-evo-query-skip]) + blockquote", NULL);
	if (!element)
		goto signature;
 repeat:
	child = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (WEBKIT_DOM_IS_ELEMENT (child))
		prev_element = WEBKIT_DOM_ELEMENT (child);
	else
		goto signature;

	equal_nodes = webkit_dom_node_is_equal_node (
		webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (element), FALSE, NULL),
		webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (prev_element), FALSE, NULL));

	if (equal_nodes) {
		if (webkit_dom_element_get_child_element_count (element) >
		    webkit_dom_element_get_child_element_count (prev_element)) {
			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (prev_element), child, NULL);
			remove_node (WEBKIT_DOM_NODE (element));
		} else {
			while ((child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (prev_element))))
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (element),
					child,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (element)),
					NULL);
			remove_node (WEBKIT_DOM_NODE (prev_element));
		}
	} else
		webkit_dom_element_set_attribute (element, "data-evo-query-skip", "", NULL);

	element = webkit_dom_document_query_selector (document, "blockquote:not([data-evo-query-skip]) + blockquote", NULL);
	if (element)
		goto repeat;

 signature:
	list = webkit_dom_document_query_selector_all (
		document, "blockquote[data-evo-query-skip]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-evo-query-skip");
	}
	g_clear_object (&list);

	if (!deleted_content)
		return;

	/* Replace the corrupted signatures with the right one. */
	element = webkit_dom_document_query_selector (
		document, ".-x-evo-signature-wrapper + .-x-evo-signature-wrapper", NULL);
	if (element) {
		WebKitDOMElement *right_signature;

		right_signature = webkit_dom_document_fragment_query_selector (
			deleted_content, ".-x-evo-signature-wrapper", NULL);
		remove_node (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element)));
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (right_signature), TRUE, NULL),
			WEBKIT_DOM_NODE (element),
			NULL);
	}
}

/* This will fix the structure after the situations where some text
 * inside the quoted content is selected and afterwards deleted with
 * BackSpace or Delete. */
void
e_editor_dom_body_key_up_event_process_backspace_or_delete (EEditorPage *editor_page,
                                                            gboolean delete)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent, *node;
	gint level;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_get_html_mode (editor_page)) {
		if (!delete) {
			e_editor_dom_selection_save (editor_page);
			e_editor_dom_merge_siblings_if_necessary (editor_page, NULL);
			e_editor_dom_selection_restore (editor_page);
		}
		return;
	}

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_disable_quote_marks_select (editor_page);
	/* Remove empty blocks if presented. */
	remove_empty_blocks (document);

	e_editor_dom_selection_save (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If we deleted a selection the caret will be inside the quote marks, fix it. */
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quote-character")) {
		parent = webkit_dom_node_get_parent_node (parent);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (selection_start_marker),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
	}

	/* Under some circumstances we will end with block inside the citation
	 * that has the quote marks removed and we have to reinsert them back. */
	level = e_editor_dom_get_citation_level (WEBKIT_DOM_NODE (selection_start_marker), FALSE);
	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
	if (level > 0 && node && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
		WebKitDOMElement *block;

		block = WEBKIT_DOM_ELEMENT (e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker)));

		e_editor_dom_remove_quoting_from_element (block);
		if (webkit_dom_element_has_attribute (block, "data-evo-paragraph")) {
			gint length, word_wrap_length;

			word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
			length =  word_wrap_length - 2 * level;
			block = e_editor_dom_wrap_paragraph_length (editor_page, block, length);
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (block));
		}
		e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, block, level);
	} else if (level > 0 && !node) {
		WebKitDOMNode *prev_sibling;

		prev_sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted") &&
		    !webkit_dom_node_get_previous_sibling (prev_sibling)) {
			webkit_dom_node_append_child (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)),
				NULL);
		}
	}

	e_editor_dom_merge_siblings_if_necessary (editor_page, NULL);

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
}

void
e_editor_dom_body_key_up_event_process_return_key (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	/* If the return is pressed in an unordered list in plain text mode
	 * the caret is moved to the "*" character before the newly inserted
	 * item. It looks like it is not enough that the item has BR element
	 * inside, but we have to again use the zero width space character
	 * to fix the situation. */
	if (e_editor_page_get_html_mode (editor_page))
		return;

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent) ||
	    !WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (webkit_dom_node_get_parent_node (parent))) {
		e_editor_dom_selection_restore (editor_page);
		return;
	}

	if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)) &&
	    (!webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)) ||
	     WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker)))))
		webkit_dom_element_insert_adjacent_text (
			WEBKIT_DOM_ELEMENT (parent),
			"afterbegin",
			UNICODE_ZERO_WIDTH_SPACE,
			NULL);

	e_editor_dom_selection_restore (editor_page);
}

static void
body_keyup_event_cb (WebKitDOMElement *element,
                     WebKitDOMUIEvent *event,
                     EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	glong key_code;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	if (!e_editor_page_is_composition_in_progress (editor_page))
		e_editor_dom_register_input_event_listener_on_body (editor_page);

	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return;

	key_code = webkit_dom_ui_event_get_key_code (event);
	if (key_code == HTML_KEY_CODE_BACKSPACE || key_code == HTML_KEY_CODE_DELETE) {
		e_editor_dom_body_key_up_event_process_backspace_or_delete (editor_page, key_code == HTML_KEY_CODE_DELETE);

		/* The content was wrapped and the coordinates
		 * of caret could be changed, so renew them. But
		 * only do that when we are not redoing a history
		 * event, otherwise it would modify the history. */
		if (e_editor_page_get_renew_history_after_coordinates (editor_page)) {
			EEditorHistoryEvent *ev = NULL;
			EEditorUndoRedoManager *manager;

			manager = e_editor_page_get_undo_redo_manager (editor_page);
			ev = e_editor_undo_redo_manager_get_current_history_event (manager);
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);
		}
	} else if (key_code == HTML_KEY_CODE_CONTROL)
		dom_set_links_active (document, FALSE);
	else if (key_code == HTML_KEY_CODE_RETURN)
		e_editor_dom_body_key_up_event_process_return_key (editor_page);
}

static void
fix_structure_after_pasting_multiline_content (WebKitDOMNode *node)
{
	WebKitDOMNode *first_child, *parent;

	/* When pasting content that does not contain just the
	 * one line text WebKit inserts all the content after the
	 * first line into one element. So we have to take it out
	 * of this element and insert it after that element. */
	parent = webkit_dom_node_get_parent_node (node);
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))
		return;
	first_child = webkit_dom_node_get_first_child (parent);
	while (first_child) {
		WebKitDOMNode *next_child =
			webkit_dom_node_get_next_sibling  (first_child);
		if (webkit_dom_node_has_child_nodes (first_child))
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				first_child,
				parent,
				NULL);
		first_child = next_child;
	}
}

static gboolean
delete_hidden_space (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *block;
	gint citation_level;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = WEBKIT_DOM_ELEMENT (e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker)));

	citation_level = e_editor_dom_get_citation_level (
		WEBKIT_DOM_NODE (selection_start_marker), FALSE);

	if (selection_start_marker && citation_level > 0) {
		EEditorUndoRedoManager *manager;
		EEditorHistoryEvent *ev = NULL;
		WebKitDOMNode *node;
		WebKitDOMDocumentFragment *fragment;

		manager = e_editor_page_get_undo_redo_manager (editor_page);

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-quoted")))
			return FALSE;

		node = webkit_dom_node_get_previous_sibling (node);
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")))
			return FALSE;

		node = webkit_dom_node_get_previous_sibling (node);
		if (!(WEBKIT_DOM_IS_ELEMENT (node) &&
		      webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-hidden-space")))
			return FALSE;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		remove_node (node);

		e_editor_dom_wrap_and_quote_element (editor_page, block);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			WEBKIT_DOM_NODE (
				webkit_dom_document_create_text_node (document, " ")),
			NULL);
		ev->data.fragment = g_object_ref (fragment);

		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);

		return TRUE;
	}

	return FALSE;
}

static gboolean
caret_is_on_the_line_beginning_html (WebKitDOMDocument *document)
{
	gboolean ret_val = FALSE;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *tmp_range = NULL, *actual_range = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	actual_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	webkit_dom_dom_selection_modify (dom_selection, "move", "left", "lineBoundary");

	tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (webkit_dom_range_compare_boundary_points (tmp_range, WEBKIT_DOM_RANGE_START_TO_START, actual_range, NULL) == 0)
		ret_val = TRUE;

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, actual_range);

	g_clear_object (&tmp_range);
	g_clear_object (&actual_range);

	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);

	return ret_val;
}

static gboolean
is_empty_quoted_element (WebKitDOMElement *element)
{
	WebKitDOMNode *node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));

	if (!WEBKIT_DOM_IS_ELEMENT (node) || !element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-quoted"))
		return FALSE;

	if (!(node = webkit_dom_node_get_next_sibling (node)))
		return TRUE;

	if (WEBKIT_DOM_IS_TEXT (node)) {
		gchar *content;

		content = webkit_dom_node_get_text_content (node);
		if (content && *content) {
			g_free (content);
			return FALSE;
		}

		g_free (content);
		return webkit_dom_node_get_next_sibling (node) ? FALSE : TRUE;
	}

	if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node))
		return webkit_dom_node_get_next_sibling (node) ? FALSE : TRUE;

	if (!WEBKIT_DOM_IS_ELEMENT (node) || !element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-selection-start-marker"))
		return FALSE;

	if (!(node = webkit_dom_node_get_next_sibling (node)))
		return FALSE;

	if (!WEBKIT_DOM_IS_ELEMENT (node) || !element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-selection-end-marker"))
		return FALSE;

	if (!(node = webkit_dom_node_get_next_sibling (node)))
		return TRUE;

	if (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
		if (WEBKIT_DOM_IS_TEXT (node)) {
			gchar *content;

			content = webkit_dom_node_get_text_content (node);
			if (content && *content) {
				g_free (content);
				return FALSE;
			}

			g_free (content);
			return webkit_dom_node_get_next_sibling (node) ? FALSE : TRUE;
		}
		return FALSE;
	}

	if (!(node = webkit_dom_node_get_next_sibling (node)))
		return TRUE;

	return TRUE;
}

gboolean
e_editor_dom_move_quoted_block_level_up (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean html_mode;
	gint citation_level, success = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	block = e_editor_dom_get_parent_block_node_from_child (WEBKIT_DOM_NODE (selection_start_marker));

	citation_level = e_editor_dom_get_citation_level (
		WEBKIT_DOM_NODE (selection_start_marker), FALSE);

	if (selection_start_marker && citation_level > 0) {
		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), ".-x-evo-quoted", NULL)) {

			WebKitDOMNode *prev_sibling;

			webkit_dom_node_normalize (block);

			prev_sibling = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));

			if (!prev_sibling) {
				WebKitDOMNode *parent;

				parent = webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start_marker));
				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
					prev_sibling = webkit_dom_node_get_previous_sibling (parent);
			}

			if (WEBKIT_DOM_IS_ELEMENT (prev_sibling))
				success = element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-quoted");

			/* We really have to be in the beginning of paragraph and
			 * not on the beginning of some line in the paragraph */
			if (success && webkit_dom_node_get_previous_sibling (prev_sibling))
				success = FALSE;
		}

		if (html_mode) {
			webkit_dom_node_normalize (block);

			success = caret_is_on_the_line_beginning_html (document);
			if (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)))
				block = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		}
	}

	if (!success)
		return FALSE;

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_UNQUOTE;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		ev->data.dom.from = g_object_ref (webkit_dom_node_clone_node_with_error (block, TRUE, NULL));
	}

	if (citation_level == 1) {
		gboolean is_empty_quoted_block = FALSE;
		gchar *inner_html = NULL;
		WebKitDOMElement *paragraph, *element;

		if (WEBKIT_DOM_IS_ELEMENT (block)) {
			is_empty_quoted_block = is_empty_quoted_element (WEBKIT_DOM_ELEMENT (block));
			inner_html = webkit_dom_element_get_inner_html (WEBKIT_DOM_ELEMENT (block));
			webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (block), "-x-evo-to-remove");
		}

		paragraph = e_editor_dom_insert_new_line_into_citation (editor_page, inner_html);
		g_free (inner_html);

		if (paragraph) {
			if (!(webkit_dom_element_query_selector (paragraph, "#-x-evo-selection-start-marker", NULL)))
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (paragraph),
					WEBKIT_DOM_NODE (selection_start_marker),
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (paragraph)),
					NULL);

			if (!(webkit_dom_element_query_selector (paragraph, "#-x-evo-selection-end-marker", NULL)))
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (paragraph),
					WEBKIT_DOM_NODE (selection_end_marker),
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (paragraph)),
					NULL);

			e_editor_dom_remove_quoting_from_element (paragraph);
			e_editor_dom_remove_wrapping_from_element (paragraph);

			/* Moving PRE block from citation to body */
			if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (block) && !is_empty_quoted_block) {
				WebKitDOMElement *pre;
				WebKitDOMNode *child;

				pre = webkit_dom_document_create_element (document, "pre", NULL);
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (paragraph)),
					WEBKIT_DOM_NODE (pre),
					WEBKIT_DOM_NODE (paragraph),
					NULL);

				while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (paragraph))))
					webkit_dom_node_append_child (WEBKIT_DOM_NODE (pre), child, NULL);

				remove_node (WEBKIT_DOM_NODE (paragraph));
				paragraph = pre;
			}
		}

		if (block)
			remove_node (block);

		while ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-to-remove")))
			remove_node (WEBKIT_DOM_NODE (element));

		if (paragraph)
			remove_node_if_empty (
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (paragraph)));
	}

	if (citation_level > 1) {
		WebKitDOMNode *parent;

		if (html_mode) {
			webkit_dom_node_insert_before (
				block,
				WEBKIT_DOM_NODE (selection_start_marker),
				webkit_dom_node_get_first_child (block),
				NULL);
			webkit_dom_node_insert_before (
				block,
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_first_child (block),
				NULL);

		}

		e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		parent = webkit_dom_node_get_parent_node (block);

		if (!webkit_dom_node_get_previous_sibling (block)) {
			/* Currect block is in the beginning of citation, just move it
			 * before the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				parent,
				NULL);
		} else if (!webkit_dom_node_get_next_sibling (block)) {
			/* Currect block is at the end of the citation, just move it
			 * after the citation where already is */
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		} else {
			/* Current block is somewhere in the middle of the citation
			 * so we need to split the citation and insert the block into
			 * the citation that is one level lower */
			WebKitDOMNode *clone, *child;

			clone = webkit_dom_node_clone_node_with_error (parent, FALSE, NULL);

			/* Move nodes that are after the currect block into the
			 * new blockquote */
			child = webkit_dom_node_get_next_sibling (block);
			while (child) {
				WebKitDOMNode *next = webkit_dom_node_get_next_sibling (child);
				webkit_dom_node_append_child (clone, child, NULL);
				child = next;
			}

			clone = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				clone,
				webkit_dom_node_get_next_sibling (parent),
				NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				block,
				clone,
				NULL);
		}

		e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (block));
	}

	remove_empty_blocks (document);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	return success;
}

static gboolean
prevent_from_deleting_last_element_in_body (WebKitDOMDocument *document)
{
	gboolean ret_val = FALSE;
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *node;

	body = webkit_dom_document_get_body (document);

	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
	if (!node || !webkit_dom_node_get_next_sibling (node)) {
		gchar *content;

		content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (body));

		if (!content || !*content)
			ret_val = TRUE;

		g_free (content);

		if (webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (body), "img", NULL))
			ret_val = FALSE;
	}

	return ret_val;
}

static void
insert_quote_symbols (WebKitDOMDocument *document,
                      WebKitDOMHTMLElement *element,
                      gint quote_level)
{
	gchar *quotation;
	WebKitDOMElement *quote_element;

	if (!WEBKIT_DOM_IS_ELEMENT (element))
		return;

	quotation = get_quotation_for_level (quote_level);

	quote_element = webkit_dom_document_create_element (document, "span", NULL);
	element_add_class (quote_element, "-x-evo-quoted");

	webkit_dom_element_set_inner_html (quote_element, quotation, NULL);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (element),
		WEBKIT_DOM_NODE (quote_element),
		webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)),
		NULL);

	g_free (quotation);
}

static void
quote_node (WebKitDOMDocument *document,
            WebKitDOMNode *node,
            gint quote_level)
{
	WebKitDOMNode *parent, *next_sibling;

	/* Don't quote when we are not in citation */
	if (quote_level == 0)
		return;

	if (WEBKIT_DOM_IS_COMMENT (node))
		return;

	if (WEBKIT_DOM_IS_ELEMENT (node)) {
		insert_quote_symbols (document, WEBKIT_DOM_HTML_ELEMENT (node), quote_level);
		return;
	}

	next_sibling = webkit_dom_node_get_next_sibling (node);

	/* Skip the BR between first blockquote and pre */
	if (quote_level == 1 && next_sibling && WEBKIT_DOM_IS_HTML_PRE_ELEMENT (next_sibling))
		return;

	parent = webkit_dom_node_get_parent_node (node);

	insert_quote_symbols (
		document, WEBKIT_DOM_HTML_ELEMENT (parent), quote_level);
}

static void
insert_quote_symbols_before_node (WebKitDOMDocument *document,
                                  WebKitDOMNode *node,
                                  gint quote_level,
                                  gboolean is_html_node)
{
	gboolean skip, wrap_br;
	gchar *quotation;
	WebKitDOMElement *element;

	quotation = get_quotation_for_level (quote_level);
	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	element_add_class (element, "-x-evo-quoted");
	webkit_dom_element_set_inner_html (element, quotation, NULL);

	/* Don't insert temporary BR before BR that is used for wrapping */
	skip = WEBKIT_DOM_IS_HTML_BR_ELEMENT (node);
	wrap_br = element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br");
	skip = skip && wrap_br;

	if (is_html_node && !skip) {
		WebKitDOMElement *new_br;

		new_br = webkit_dom_document_create_element (document, "br", NULL);
		element_add_class (new_br, "-x-evo-temp-br");

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (new_br),
			node,
			NULL);
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		node,
		NULL);

	if (is_html_node && !wrap_br)
		remove_node (node);

	g_free (quotation);
}

static gboolean
check_if_suppress_next_node (WebKitDOMNode *node)
{
	if (!node)
		return FALSE;

	if (node && WEBKIT_DOM_IS_ELEMENT (node))
		if (e_editor_dom_is_selection_position_node (node))
			if (!webkit_dom_node_get_previous_sibling (node))
				return FALSE;

	return TRUE;
}

static void
quote_br_node (WebKitDOMNode *node,
               gint quote_level)
{
	gchar *quotation, *content;

	quotation = get_quotation_for_level (quote_level);

	content = g_strconcat (
		"<span class=\"-x-evo-quoted\">",
		quotation,
		"</span><br class=\"-x-evo-temp-br\">",
		NULL);

	webkit_dom_element_set_outer_html (
		WEBKIT_DOM_ELEMENT (node), content, NULL);

	g_free (content);
	g_free (quotation);
}

static void
quote_plain_text_recursive (WebKitDOMDocument *document,
                            WebKitDOMNode *block,
                            WebKitDOMNode *start_node,
                            gint quote_level)
{
	gboolean skip_node = FALSE;
	gboolean move_next = FALSE;
	gboolean suppress_next = FALSE;
	gboolean is_html_node = FALSE;
	gboolean next = FALSE;
	WebKitDOMNode *node, *next_sibling, *prev_sibling;

	node = webkit_dom_node_get_first_child (block);

	while (node) {
		skip_node = FALSE;
		move_next = FALSE;
		is_html_node = FALSE;

		if (WEBKIT_DOM_IS_COMMENT (node) ||
		    WEBKIT_DOM_IS_HTML_META_ELEMENT (node) ||
		    WEBKIT_DOM_IS_HTML_STYLE_ELEMENT (node) ||
		    WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node)) {

			move_next = TRUE;
			goto next_node;
		}

		prev_sibling = webkit_dom_node_get_previous_sibling (node);
		next_sibling = webkit_dom_node_get_next_sibling (node);

		if (WEBKIT_DOM_IS_TEXT (node)) {
			/* Start quoting after we are in blockquote */
			if (quote_level > 0 && !suppress_next) {
				/* When quoting text node, we are wrappering it and
				 * afterwards replacing it with that wrapper, thus asking
				 * for next_sibling after quoting will return NULL bacause
				 * that node don't exist anymore */
				quote_node (document, node, quote_level);
				node = next_sibling;
				skip_node = TRUE;
			}

			goto next_node;
		}

		if (!(WEBKIT_DOM_IS_ELEMENT (node) || WEBKIT_DOM_IS_HTML_ELEMENT (node)))
			goto next_node;

		if (e_editor_dom_is_selection_position_node (node)) {
			/* If there is collapsed selection in the beginning of line
			 * we cannot suppress first text that is after the end of
			 * selection */
			suppress_next = check_if_suppress_next_node (prev_sibling);
			if (suppress_next)
				next = FALSE;
			move_next = TRUE;
			goto next_node;
		}

		if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) &&
		    webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (node)) != 0)
			goto with_children;

		/* Even in plain text mode we can have some basic html element
		 * like anchor and others. When Forwaring e-mail as Quoted EMFormat
		 * generates header that contains <b> tags (bold font).
		 * We have to treat these elements separately to avoid
		 * modifications of theirs inner texts */
		is_html_node =
			WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "b") ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "i") ||
			element_has_tag (WEBKIT_DOM_ELEMENT (node), "u") ||
			element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span");

		if (is_html_node) {
			gboolean wrap_br;

			wrap_br =
				prev_sibling &&
				WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
				element_has_class (
					WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br");

			if (!prev_sibling || wrap_br) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
				if (!prev_sibling && next_sibling && WEBKIT_DOM_IS_TEXT (next_sibling))
					suppress_next = TRUE;
			}

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) && !wrap_br)
				insert_quote_symbols_before_node (
					document, prev_sibling, quote_level, TRUE);

			move_next = TRUE;
			goto next_node;
		}

		/* If element doesn't have children, we can quote it */
		if (e_editor_dom_node_is_citation_node (node)) {
			/* Citation with just text inside */
			quote_node (document, node, quote_level + 1);

			move_next = TRUE;
			goto next_node;
		}

		if (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (prev_sibling)) {
				move_next = TRUE;
				goto next_node;
			}
			goto not_br;
		} else if (element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-first-br") ||
		           element_has_id (WEBKIT_DOM_ELEMENT (node), "-x-evo-last-br")) {
			quote_br_node (node, quote_level);
			node = next_sibling;
			skip_node = TRUE;
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling)) {
			quote_br_node (prev_sibling, quote_level);
			node = next_sibling;
			skip_node = TRUE;
			goto next_node;
		}

		if (!prev_sibling && !next_sibling) {
			WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (parent) ||
			    WEBKIT_DOM_IS_HTML_PRE_ELEMENT (parent) ||
			    (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
			     !e_editor_dom_node_is_citation_node (parent))) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);

				goto next_node;
			}
		}

		if (e_editor_dom_node_is_citation_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
		    !next_sibling && WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
		    e_editor_dom_is_selection_position_node (prev_sibling)) {
			insert_quote_symbols_before_node (
				document, node, quote_level, FALSE);
			goto next_node;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			if (!prev_sibling && !next_sibling) {
				insert_quote_symbols_before_node (
					document, node, quote_level, FALSE);
			} else
				move_next = TRUE;
			goto next_node;
		}

 not_br:
		quote_node (document, node, quote_level);

		move_next = TRUE;
		goto next_node;

 with_children:
		if (e_editor_dom_node_is_citation_node (node)) {
			/* Go deeper and increase level */
			quote_plain_text_recursive (
				document, node, start_node, quote_level + 1);
			move_next = TRUE;
		} else {
			quote_plain_text_recursive (
				document, node, start_node, quote_level);
			move_next = TRUE;
		}
 next_node:
		if (next) {
			suppress_next = FALSE;
			next = FALSE;
		}

		if (suppress_next)
			next = TRUE;

		if (!skip_node) {
			/* Move to next node */
			if (!move_next && webkit_dom_node_has_child_nodes (node)) {
				node = webkit_dom_node_get_first_child (node);
			} else if (webkit_dom_node_get_next_sibling (node)) {
				node = webkit_dom_node_get_next_sibling (node);
			} else {
				return;
			}
		}
	}
}

WebKitDOMElement *
e_editor_dom_quote_plain_text_element (EEditorPage *editor_page,
                                       WebKitDOMElement *element)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *element_clone;
	WebKitDOMHTMLCollection *collection = NULL;
	gint ii, level;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	element_clone = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (element), TRUE, NULL);
	level = e_editor_dom_get_citation_level (WEBKIT_DOM_NODE (element), TRUE);

	/* Remove old quote characters if the exists */
	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (element_clone), "-x-evo-quoted");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;)
		remove_node (webkit_dom_html_collection_item (collection, ii));
	g_clear_object (&collection);

	webkit_dom_node_normalize (element_clone);
	quote_plain_text_recursive (
		document, element_clone, element_clone, level);

	/* Replace old element with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		element_clone,
		WEBKIT_DOM_NODE (element),
		NULL);

	return WEBKIT_DOM_ELEMENT (element_clone);
}

/*
 * dom_quote_plain_text:
 *
 * Quote text inside citation blockquotes in plain text mode.
 *
 * As this function is cloning and replacing all citation blockquotes keep on
 * mind that any pointers to nodes inside these blockquotes will be invalidated.
 */
static WebKitDOMElement *
dom_quote_plain_text (WebKitDOMDocument *document)
{
	WebKitDOMHTMLElement *body;
	WebKitDOMNode *body_clone;
	WebKitDOMNamedNodeMap *attributes = NULL;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMElement *element;
	gint ii;
	gulong attributes_length;

	/* Check if the document is already quoted */
	element = webkit_dom_document_query_selector (
		document, ".-x-evo-quoted", NULL);
	if (element)
		return NULL;

	body = webkit_dom_document_get_body (document);
	body_clone = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (body), TRUE, NULL);

	/* Clean unwanted spaces before and after blockquotes */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (body_clone), "blockquote[type|=cite]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *blockquote = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *prev_sibling = webkit_dom_node_get_previous_sibling (blockquote);
		WebKitDOMNode *next_sibling = webkit_dom_node_get_next_sibling (blockquote);

		if (prev_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling))
			remove_node (prev_sibling);

		if (next_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling))
			remove_node (next_sibling);

		if (webkit_dom_node_has_child_nodes (blockquote)) {
			WebKitDOMNode *child = webkit_dom_node_get_first_child (blockquote);
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				remove_node (child);
		}
	}
	g_clear_object (&list);

	webkit_dom_node_normalize (body_clone);
	quote_plain_text_recursive (document, body_clone, body_clone, 0);

	/* Copy attributes */
	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
	attributes_length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = 0; ii < attributes_length; ii++) {
		gchar *name, *value;
		WebKitDOMNode *node = webkit_dom_named_node_map_item (attributes, ii);

		name = webkit_dom_attr_get_name (WEBKIT_DOM_ATTR (node));
		value = webkit_dom_node_get_node_value (node);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body_clone), name, value, NULL);

		g_free (name);
		g_free (value);
	}
	g_clear_object (&attributes);

	/* Replace old BODY with one, that is quoted */
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (body)),
		body_clone,
		WEBKIT_DOM_NODE (body),
		NULL);

	return WEBKIT_DOM_ELEMENT (body_clone);
}

/*
 * dom_dequote_plain_text:
 *
 * Dequote already quoted plain text in editor.
 * Editor have to be quoted with e_html_editor_view_quote_plain_text otherwise
 * it's not working.
 */
static void
dom_dequote_plain_text (WebKitDOMDocument *document)
{
	WebKitDOMNodeList *paragraphs = NULL;
	gint ii;

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite]", NULL);
	for (ii = webkit_dom_node_list_get_length (paragraphs); ii--;) {
		WebKitDOMElement *element;

		element = WEBKIT_DOM_ELEMENT (webkit_dom_node_list_item (paragraphs, ii));

		if (e_editor_dom_node_is_citation_node (WEBKIT_DOM_NODE (element)))
			e_editor_dom_remove_quoting_from_element (element);
	}
	g_clear_object (&paragraphs);
}

static gboolean
create_anchor_for_link (const GMatchInfo *info,
                        GString *res,
                        gpointer data)
{
	gboolean link_surrounded, with_nbsp = FALSE;
	gint offset = 0, truncate_from_end = 0;
	gint match_start, match_end;
	gchar *match_with_nbsp, *match_without_nbsp;
	const gchar *end_of_match = NULL;
	const gchar *match, *match_extra_characters;

	match_with_nbsp = g_match_info_fetch (info, 1);
	/* E-mail addresses will be here. */
	match_without_nbsp = g_match_info_fetch (info, 0);

	if (!match_with_nbsp || (strstr (match_with_nbsp, "&nbsp;") && !g_str_has_prefix (match_with_nbsp, "&nbsp;"))) {
		match = match_without_nbsp;
		match_extra_characters = match_with_nbsp;
		g_match_info_fetch_pos (info, 0, &match_start, &match_end);
		with_nbsp = TRUE;
	} else {
		match = match_with_nbsp;
		match_extra_characters = match_without_nbsp;
		g_match_info_fetch_pos (info, 1, &match_start, &match_end);
	}

	if (g_str_has_prefix (match, "&nbsp;"))
		offset += 6;

	end_of_match = match + match_end - match_start - 1;
	/* Taken from camel-url-scanner.c */
	/* URLs are extremely unlikely to end with any punctuation, so
	 * strip any trailing punctuation off from link and put it after
	 * the link. Do the same for any closing double-quotes as well. */
	while (end_of_match && end_of_match != match && strchr (URL_INVALID_TRAILING_CHARS, *end_of_match)) {
		truncate_from_end++;
		end_of_match--;
	}
	end_of_match++;

	link_surrounded =
		g_str_has_suffix (res->str, "&lt;");

	if (link_surrounded) {
		if (end_of_match && *end_of_match && strlen (match) > strlen (end_of_match) + 3)
			link_surrounded = link_surrounded && g_str_has_prefix (end_of_match - 3, "&gt;");
		else
			link_surrounded = link_surrounded && g_str_has_suffix (match, "&gt;");

		if (link_surrounded) {
			/* ";" is already counted by code above */
			truncate_from_end += 3;
			end_of_match -= 3;
		}
	}

	g_string_append (res, "<a href=\"");
	if (strstr (match, "@") && !strstr (match, "://"))
		g_string_append (res, "mailto:");
	g_string_append (res, match + offset);
	if (truncate_from_end > 0)
		g_string_truncate (res, res->len - truncate_from_end);

	g_string_append (res, "\">");
	g_string_append (res, match + offset);
	if (truncate_from_end > 0)
		g_string_truncate (res, res->len - truncate_from_end);

	g_string_append (res, "</a>");

	if (truncate_from_end > 0)
		g_string_append (res, end_of_match);

	if (!with_nbsp && match_extra_characters)
		g_string_append (res, match_extra_characters + (match_end - match_start));

	g_free (match_with_nbsp);
	g_free (match_without_nbsp);

	return FALSE;
}

static gboolean
replace_to_nbsp (const GMatchInfo *info,
                 GString *res)
{
	gchar *match;
	gint ii = 0;

	match = g_match_info_fetch (info, 0);

	while (match[ii] != '\0') {
		if (match[ii] == ' ') {
			/* Alone spaces or spaces before/after tabulator. */
			g_string_append (res, "&nbsp;");
		} else if (match[ii] == '\t') {
			/* Replace tabs with their WebKit HTML representation. */
			g_string_append (res, "<span class=\"Apple-tab-span\" style=\"white-space:pre\">\t</span>");
		}

		ii++;
	}

	g_free (match);

	return FALSE;
}

static gboolean
surround_links_with_anchor (const gchar *text)
{
	return (strstr (text, "http") || strstr (text, "ftp") ||
		strstr (text, "www") || strstr (text, "@"));
}

static void
append_new_block (WebKitDOMElement *parent,
                  WebKitDOMElement **block)
{
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (*block),
		NULL);

	*block = NULL;
}

static WebKitDOMElement *
create_and_append_new_block (EEditorPage *editor_page,
                             WebKitDOMElement *parent,
                             WebKitDOMElement *block_template,
                             const gchar *content)
{
	WebKitDOMElement *block;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	block = WEBKIT_DOM_ELEMENT (webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (block_template), FALSE, NULL));

	webkit_dom_element_set_inner_html (block, content, NULL);

	append_new_block (parent, &block);

	return block;
}

static void
append_citation_mark (WebKitDOMDocument *document,
                      WebKitDOMElement *parent,
                      const gchar *citation_mark_text)
{
	WebKitDOMText *text;

	text = webkit_dom_document_create_text_node (document, citation_mark_text);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (text),
		NULL);
}

static void
replace_selection_markers (gchar **text)
{
	if (!text)
		return;

	if (strstr (*text, "##SELECTION_START##")) {
		GString *tmp;

		tmp = e_str_replace_string (
			*text,
			"##SELECTION_START##",
			"<span id=\"-x-evo-selection-start-marker\"></span>");

		g_free (*text);
		*text = g_string_free (tmp, FALSE);
	}

	if (strstr (*text, "##SELECTION_END##")) {
		GString *tmp;

		tmp = e_str_replace_string (
			*text,
			"##SELECTION_END##",
			"<span id=\"-x-evo-selection-end-marker\"></span>");

		g_free (*text);
		*text = g_string_free (tmp, FALSE);
	}
}

static GString *
remove_new_lines_around_citations (const gchar *input)
{
	GString *str = NULL;
	const gchar *p, *next;

	str = g_string_new ("");

	/* Remove the new lines around citations:
	 * Replace <br><br>##CITATION_START## with <br>##CITATION_START##
	 * Replace ##CITATION_START##<br><br> with ##CITATION_START##<br>
	 * Replace ##CITATION_END##<br><br> with ##CITATION_END##<br>
	 * Replace <br>##CITATION_END## with ##CITATION_END##
	 * Replace <br>##CITATION_START## with ##CITATION_START## */
	p = input;
	while (next = strstr (p, "##CITATION_"), next) {
		gchar citation_type = 0;

		if (p < next)
			g_string_append_len (str, p, next - p);

		if (next + 11)
			citation_type = next[11];
		/* ##CITATION_START## */
		if (citation_type == 'S') {
			if (g_str_has_suffix (str->str, "<br><br>") ||
			    g_str_has_suffix (str->str, "<br>"))
				g_string_truncate (str, str->len - 4);

			if (g_str_has_prefix (next + 11, "START##<br><br>")) {
				g_string_append (str, "##CITATION_START##<br>");
				p = next + 26;
				continue;
			}
		} else if (citation_type == 'E') {
			if (g_str_has_suffix (str->str, "<br>"))
				g_string_truncate (str, str->len - 4);

			if (g_str_has_prefix (next + 11, "END##<br><br>")) {
				g_string_append (str, "##CITATION_END##<br>");
				p = next + 24;
				continue;
			}
		}

		g_string_append (str, "##CITATION_");

		p = next + 11;
	}

	g_string_append (str, p);

	if (camel_debug ("webkit:editor")) {
		printf ("EWebKitContentEditor - %s\n", G_STRFUNC);
		printf ("\toutput: '%s'\n", str->str);
	}

	return str;
}

static GString *
replace_citation_marks_to_citations (const gchar *input)
{
	GString *str = NULL;
	const gchar *p, *next;

	str = g_string_new ("");

	/* Replaces text markers with actual HTML blockquotes */
	p = input;
	while (next = strstr (p, "##CITATION_"), next) {
		gchar citation_type = 0;

		if (p < next)
			g_string_append_len (str, p, next - p);

		if (next + 11)
			citation_type = next[11];
		/* ##CITATION_START## */
		if (citation_type == 'S') {
			g_string_append (str, "<blockquote type=\"cite\">");
			p = next + 18;
		} else if (citation_type == 'E') {
			g_string_append (str, "</blockquote>");
			p = next + 16;
		} else
			p = next + 11;
	}

	g_string_append (str, p);

	return str;
}

/* This parses the HTML code (that contains just text, &nbsp; and BR elements)
 * into blocks.
 * HTML code in that format we can get by taking innerText from some element,
 * setting it to another one and finally getting innerHTML from it */
static void
parse_html_into_blocks (EEditorPage *editor_page,
                        WebKitDOMElement *parent,
                        WebKitDOMElement *passed_block_template,
                        const gchar *input)
{
	gboolean has_citation = FALSE, processing_last = FALSE;
	const gchar *prev_token, *next_token;
	const gchar *next_br_token = NULL, *next_citation_token = NULL;
	GString *html = NULL;
	GRegex *regex_nbsp = NULL, *regex_link = NULL, *regex_email = NULL;
	WebKitDOMDocument *document;
	WebKitDOMElement *block_template = passed_block_template;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!(input && *input))
		return;

	document = e_editor_page_get_document (editor_page);
	webkit_dom_element_set_inner_html (parent, "", NULL);

	if (!block_template) {
		gboolean use_paragraphs;
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		use_paragraphs = g_settings_get_boolean (
			settings, "composer-wrap-quoted-text-in-replies");

		if (use_paragraphs)
			block_template = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
		else
			block_template = webkit_dom_document_create_element (document, "pre", NULL);

		g_object_unref (settings);
	}

	/* Replace the tabulators with SPAN elements that corresponds to them.
	 * If not inserting the content into the PRE element also replace single
	 * spaces on the beginning of line, 2+ spaces and with non breaking
	 * spaces. */
	if (WEBKIT_DOM_IS_HTML_PRE_ELEMENT (block_template))
		regex_nbsp = g_regex_new ("\x9", 0, 0, NULL);
	else
		regex_nbsp = g_regex_new ("^\\s{1}|\\s{2,}|\x9|\\s$", 0, 0, NULL);

	if (camel_debug ("webkit:editor")) {
		printf ("EWebKitContentEditor - %s\n", G_STRFUNC);
		printf ("\tinput: '%s'\n", input);
	}
	html = remove_new_lines_around_citations (input);

	prev_token = html->str;
	next_br_token = (prev_token && *prev_token) ? strstr (prev_token + 1, "<br>") : NULL;
	next_citation_token = (prev_token && *prev_token) ? strstr (prev_token + 1, "##CITATION_") : NULL;
	if (next_br_token) {
		if (next_citation_token)
			next_token = next_br_token < next_citation_token ? next_br_token : next_citation_token;
		else
			next_token = next_br_token;
	} else {
		next_token = next_citation_token;
	}
	processing_last = !next_token;

	while (next_token || processing_last) {
		const gchar *citation_start = NULL, *citation_end = NULL;
		const gchar *rest = NULL, *with_br = NULL;
		gchar *to_process = NULL, *to_insert = NULL;
		guint to_insert_start = 0, to_insert_end = 0;

		if (!next_token) {
			to_process = g_strdup (prev_token);
			processing_last = TRUE;
		} else if ((to_process = g_utf8_substring (prev_token, 0, g_utf8_pointer_to_offset (prev_token, next_token))) &&
		           !*to_process && !processing_last) {
			g_free (to_process);
			to_process = g_strdup (next_token);
			next_token = NULL;
			processing_last = TRUE;
		}

		if (camel_debug ("webkit:editor"))
			printf ("\tto_process: '%s'\n", to_process);

		if (to_process && !*to_process && processing_last) {
			g_free (to_process);
			to_process = g_strdup (next_token);
			next_token = NULL;
		}

		to_insert_end = g_utf8_strlen (to_process, -1);

		if ((with_br = strstr (to_process, "<br>"))) {
			if (with_br == to_process)
				to_insert_start += 4;
		}

		if ((citation_start = strstr (to_process, "##CITATION_START"))) {
			if (with_br && citation_start == with_br + 4)
				to_insert_start += 18; /* + ## */
			else if (!with_br && citation_start == to_process)
				to_insert_start += 18; /* + ## */
			else
				to_insert_end -= 18; /* + ## */
			has_citation = TRUE;
		}

		if ((citation_end = strstr (to_process, "##CITATION_END"))) {
			if (citation_end == to_process)
				to_insert_start += 16;
			else
				to_insert_end -= 16; /* + ## */
		}

		/* First BR */
		if (with_br && prev_token == html->str)
			create_and_append_new_block (
				editor_page, parent, block_template, "<br id=\"-x-evo-first-br\">");

		if (with_br && citation_start && citation_start == with_br + 4) {
			create_and_append_new_block (
				editor_page, parent, block_template, "<br>");

			append_citation_mark (document, parent, "##CITATION_START##");
		} else if (!with_br && citation_start == to_process) {
			append_citation_mark (document, parent, "##CITATION_START##");
		}

		if (citation_end && citation_end == to_process) {
			append_citation_mark (document, parent, "##CITATION_END##");
		}

		if ((to_insert = g_utf8_substring (to_process, to_insert_start, to_insert_end)) && *to_insert) {
			gboolean empty = FALSE;
			gchar *truncated = g_strdup (to_insert);
			gchar *rest_to_insert;

			if (camel_debug ("webkit:editor"))
				printf ("\tto_insert: '%s'\n", to_insert);

			empty = !*truncated && strlen (to_insert) > 0;

			rest_to_insert = g_regex_replace_eval (
				regex_nbsp,
				empty ? rest : truncated,
				-1,
				0,
				0,
				(GRegexEvalCallback) replace_to_nbsp,
				NULL,
				NULL);
			g_free (truncated);

			replace_selection_markers (&rest_to_insert);

			if (surround_links_with_anchor (rest_to_insert)) {
				gboolean is_email_address =
					strstr (rest_to_insert, "@") &&
					!strstr (rest_to_insert, "://");

				if (is_email_address && !regex_email)
					regex_email = g_regex_new (E_MAIL_PATTERN, 0, 0, NULL);
				if (!is_email_address && !regex_link)
					regex_link = g_regex_new (URL_PATTERN, 0, 0, NULL);

				truncated = g_regex_replace_eval (
					is_email_address ? regex_email : regex_link,
					rest_to_insert,
					-1,
					0,
					G_REGEX_MATCH_NOTEMPTY,
					create_anchor_for_link,
					NULL,
					NULL);

				g_free (rest_to_insert);
				rest_to_insert = truncated;
			}

			create_and_append_new_block (
				editor_page, parent, block_template, rest_to_insert);

			g_free (rest_to_insert);
		} else if (to_insert) {
			if (!citation_start && (with_br || !citation_end))
				create_and_append_new_block (
					editor_page, parent, block_template, "<br>");
			else if (citation_end && citation_end == to_process &&
			         next_token && g_str_has_prefix (next_token, "<br>")) {
				create_and_append_new_block (
					editor_page, parent, block_template, "<br>");
			}
		}

		g_free (to_insert);

		if (with_br && citation_start && citation_start != with_br + 4)
			append_citation_mark (document, parent, "##CITATION_START##");

		if (!with_br && citation_start && citation_start != to_process)
			append_citation_mark (document, parent, "##CITATION_START##");

		if (citation_end && citation_end != to_process)
			append_citation_mark (document, parent, "##CITATION_END##");

		g_free (to_process);

		prev_token = next_token;
		next_br_token = (prev_token && *prev_token) ? strstr (prev_token + 1, "<br>") : NULL;
		next_citation_token = (prev_token && *prev_token) ? strstr (prev_token + 1, "##CITATION_") : NULL;
		if (next_br_token) {
			if (next_citation_token)
				next_token = next_br_token < next_citation_token ? next_br_token : next_citation_token;
			else
				next_token = next_br_token;
		} else {
			next_token = next_citation_token;
		}

		if (!next_token && !processing_last) {
			if (!prev_token)
				break;

			if (g_utf8_strlen (prev_token, -1) > 4) {
				next_token = prev_token;
			} else {
				WebKitDOMNode *child;

				if (g_strcmp0 (prev_token, "<br>") == 0)
					create_and_append_new_block (
						editor_page, parent, block_template, "<br>");

				child = webkit_dom_node_get_last_child (
					WEBKIT_DOM_NODE (parent));
				if (child) {
					child = webkit_dom_node_get_first_child (child);
					if (child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)) {
						/* If the processed HTML contained just
						 * the BR don't overwrite its id. */
						if (!element_has_id (WEBKIT_DOM_ELEMENT (child), "-x-evo-first-br"))
							webkit_dom_element_set_id (
								WEBKIT_DOM_ELEMENT (child),
								"-x-evo-last-br");
					}
				} else {
					create_and_append_new_block (
						editor_page, parent, block_template, "<br>");
				}
				break;
			}
			processing_last = TRUE;
		} else if (processing_last && !prev_token && !next_token) {
			break;
		}
	}

	if (has_citation) {
		gchar *inner_html;
		GString *parsed;

		/* Replace text markers with actual HTML blockquotes */
		inner_html = webkit_dom_element_get_inner_html (parent);
		parsed = replace_citation_marks_to_citations (inner_html);
		webkit_dom_element_set_inner_html (parent, parsed->str, NULL);

		if (camel_debug ("webkit:editor"))
			printf ("\tparsed content: '%s'\n", inner_html);

		g_free (inner_html);
		g_string_free (parsed, TRUE);
	} else if (camel_debug ("webkit:editor")) {
		gchar *inner_html;

		inner_html = webkit_dom_element_get_inner_html (parent);
		printf ("\tparsed content: '%s'\n", inner_html);
		g_free (inner_html);
	}

	g_string_free (html, TRUE);

	if (regex_email != NULL)
		g_regex_unref (regex_email);
	if (regex_link != NULL)
		g_regex_unref (regex_link);
	g_regex_unref (regex_nbsp);
}

void
e_editor_dom_quote_and_insert_text_into_selection (EEditorPage *editor_page,
                                                   const gchar *text,
                                                   gboolean is_html)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *blockquote, *element, *selection_start;
	WebKitDOMNode *node;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gchar *inner_html;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!text || !*text)
		return;

	document = e_editor_page_get_document (editor_page);

	if (is_html) {
		element = webkit_dom_document_create_element (document, "div", NULL);
		webkit_dom_element_set_inner_html (element, text, NULL);
	} else {
		/* This is a trick to escape any HTML characters (like <, > or &).
		 * <textarea> automatically replaces all these unsafe characters
		 * by &lt;, &gt; etc. */
		element = webkit_dom_document_create_element (document, "textarea", NULL);
		webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);
	}

	inner_html = webkit_dom_element_get_inner_html (element);

	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_PASTE_QUOTED;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (text);
	}

	blockquote = webkit_dom_document_create_element (document, "blockquote", NULL);
	webkit_dom_element_set_attribute (blockquote, "type", "cite", NULL);

	selection_start = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start));
	/* Check if block is empty. If so, replace it otherwise insert the quoted
	 * content after current block. */
	if (!node || WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
		node = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start));
		node = webkit_dom_node_get_next_sibling (node);
		if (!node || WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_start))),
				WEBKIT_DOM_NODE (blockquote),
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start)),
				NULL);
		}
	} else {
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
			WEBKIT_DOM_NODE (blockquote),
			webkit_dom_node_get_next_sibling (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start))),
			NULL);
	}

	parse_html_into_blocks (editor_page, blockquote, NULL, inner_html);

	if (e_editor_page_get_html_mode (editor_page)) {
		node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (blockquote));
	} else {
		gint word_wrap_length;

		word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (blockquote));
		while (node) {
			WebKitDOMNode *next_sibling;

			if (!WEBKIT_DOM_IS_HTML_PRE_ELEMENT (node))
				node = WEBKIT_DOM_NODE (e_editor_dom_wrap_paragraph_length (editor_page, WEBKIT_DOM_ELEMENT (node), word_wrap_length - 2));

			webkit_dom_node_normalize (node);
			e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (node), 1);

			next_sibling = webkit_dom_node_get_next_sibling (node);
			if (!next_sibling)
				break;

			node = next_sibling;
		}
	}

	dom_add_selection_markers_into_element_end (
		document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);

	e_editor_dom_selection_restore (editor_page);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
		webkit_dom_element_remove_attribute (element, "id");
	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
		webkit_dom_element_remove_attribute (element, "id");

	e_editor_dom_force_spell_check_in_viewport (editor_page);
	e_editor_page_emit_content_changed (editor_page);

	g_free (inner_html);
}

static void
mark_citation (WebKitDOMElement *citation)
{
	webkit_dom_element_insert_adjacent_text (
		citation,
		"beforebegin",
		"##CITATION_START##",
		NULL);

	webkit_dom_element_insert_adjacent_text (
		citation,
		"afterend",
		"##CITATION_END##",
		NULL);

	element_add_class (citation, "marked");
}

static gint
create_text_markers_for_citations_in_element (WebKitDOMElement *element)
{
	gint count = 0;
	WebKitDOMElement *citation;

	citation = webkit_dom_element_query_selector (
		element, "blockquote[type=cite]:not(.marked)", NULL);

	while (citation) {
		mark_citation (citation);
		count ++;

		citation = webkit_dom_element_query_selector (
			element, "blockquote[type=cite]:not(.marked)", NULL);
	}

	return count;
}

static void
create_text_markers_for_selection_in_element (WebKitDOMElement *element)
{
	WebKitDOMElement *selection_marker;

	selection_marker = webkit_dom_element_query_selector (
		element, "#-x-evo-selection-start-marker", NULL);
	if (selection_marker)
		webkit_dom_element_insert_adjacent_text (
			selection_marker,
			"afterend",
			"##SELECTION_START##",
			NULL);

	selection_marker = webkit_dom_element_query_selector (
		element, "#-x-evo-selection-end-marker", NULL);
	if (selection_marker)
		webkit_dom_element_insert_adjacent_text (
			selection_marker,
			"afterend",
			"##SELECTION_END##",
			NULL);
}

static void
quote_plain_text_elements_after_wrapping_in_element (EEditorPage *editor_page,
                                                     WebKitDOMElement *element)
{
	WebKitDOMNodeList *list = NULL;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	/* Also quote the PRE elements as well. */
	list = webkit_dom_element_query_selector_all (
		element, "blockquote[type=cite] > [data-evo-paragraph], blockquote[type=cite] > pre", NULL);

	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		gint citation_level;
		WebKitDOMNode *child;

		child = webkit_dom_node_list_item (list, ii);
		citation_level = e_editor_dom_get_citation_level (child, TRUE);
		e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (child), citation_level);
	}
	g_clear_object (&list);
}

static void
quote_plain_text_elements_after_wrapping_in_document (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	quote_plain_text_elements_after_wrapping_in_element (editor_page, WEBKIT_DOM_ELEMENT (body));
}

static void
clear_attributes (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNamedNodeMap *attributes = NULL;
	WebKitDOMHTMLElement *body;
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *document_element;
	gint length, ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);
	head = webkit_dom_document_get_head (document);
	document_element = webkit_dom_document_get_document_element (document);

	/* Remove all attributes from HTML element */
	attributes = webkit_dom_element_get_attributes (document_element);
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = length - 1; ii >= 0; ii--)
		webkit_dom_element_remove_attribute_node (
			document_element,
			WEBKIT_DOM_ATTR (webkit_dom_named_node_map_item (attributes, ii)),
			NULL);
	g_clear_object (&attributes);

	/* Remove everything from HEAD element */
	while (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)))
		remove_node (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (head)));

	/* Make the quote marks non-selectable. */
	e_editor_dom_disable_quote_marks_select (editor_page);

	/* Remove non Evolution attributes from BODY element */
	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (body));
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = length - 1; ii >= 0; ii--) {
		gchar *name;
		WebKitDOMAttr *attribute = WEBKIT_DOM_ATTR (webkit_dom_named_node_map_item (attributes, ii));

		name = webkit_dom_attr_get_name (attribute);

		if (!g_str_has_prefix (name, "data-") && (g_strcmp0 (name, "spellcheck") != 0))
			webkit_dom_element_remove_attribute_node (
				WEBKIT_DOM_ELEMENT (body), attribute, NULL);

		g_free (name);
	}
	g_clear_object (&attributes);
}

static void
body_compositionstart_event_cb (WebKitDOMElement *element,
                                WebKitDOMUIEvent *event,
                                EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_page_set_composition_in_progress (editor_page, TRUE);
	e_editor_dom_remove_input_event_listener_from_body (editor_page);
}

static void
body_compositionend_event_cb (WebKitDOMElement *element,
                              WebKitDOMUIEvent *event,
                              EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_page_set_composition_in_progress (editor_page, FALSE);
	e_editor_dom_remove_input_event_listener_from_body (editor_page);
}

static void
body_drop_event_cb (WebKitDOMElement *element,
                    WebKitDOMUIEvent *event,
                    EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_page_is_pasting_content_from_itself (editor_page)) {
		EEditorUndoRedoManager *manager;
		EEditorHistoryEvent *and_event, *event = NULL;

		/* There is a weird thing going on and I still don't know if it's
		 * caused by WebKit or Evolution. If dragging content around the
		 * editor sometimes the current selection is changed. The problem
		 * is that if moving the content, then WebKit is removing the
		 * currently selected content and at that point it could be a
		 * different one from the dragged one. So before the drop is
		 * performed we restore the selection to the state when the
		 * drag was initiated. */
		manager = e_editor_page_get_undo_redo_manager (editor_page);
		and_event = e_editor_undo_redo_manager_get_current_history_event (manager);
		while (and_event && and_event->type == HISTORY_AND) {
			event = e_editor_undo_redo_manager_get_next_history_event_for (manager, and_event);
			and_event = e_editor_undo_redo_manager_get_next_history_event_for (manager, event);
		}

		if (event)
			e_editor_dom_selection_restore_to_history_event_state (editor_page, event->before);

		e_editor_dom_save_history_for_drop (editor_page);
	}
}

static void
body_dragstart_event_cb (WebKitDOMElement *element,
                         WebKitDOMUIEvent *event,
                         EEditorPage *editor_page)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_dom_remove_input_event_listener_from_body (editor_page);
	e_editor_page_set_pasting_content_from_itself (editor_page, TRUE);
	e_editor_dom_save_history_for_drag (editor_page);
}

static void
body_dragend_event_cb (WebKitDOMElement *element,
                       WebKitDOMUIEvent *event,
                       EEditorPage *editor_page)
{
	EEditorHistoryEvent *ev;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (e_editor_page_is_pasting_content_from_itself (editor_page) &&
	   (ev = e_editor_undo_redo_manager_get_current_history_event (manager))) {
		if (ev->type == HISTORY_INSERT_HTML &&
		    ev->after.start.x == 0 && ev->after.start.y == 0 &&
		    ev->after.end.x == 0 && ev->after.end.y == 0) {
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);
			ev->before.start.x = ev->after.start.x;
			ev->before.start.y = ev->after.start.y;
			ev->before.end.x = ev->after.start.x;
			ev->before.end.y = ev->after.start.y;
			e_editor_dom_force_spell_check_in_viewport (editor_page);
		} else {
			/* Drag and Drop was cancelled */
			while (ev && ev->type == HISTORY_AND) {
				e_editor_undo_redo_manager_remove_current_history_event (manager);
				ev = e_editor_undo_redo_manager_get_current_history_event (manager);
				/* Basically the same as in body_drop_event_cb().  See the comment there. */
				e_editor_dom_selection_restore_to_history_event_state (editor_page, ev->before);
				e_editor_undo_redo_manager_remove_current_history_event (manager);
				ev = e_editor_undo_redo_manager_get_current_history_event (manager);
			}
		}
	}

	e_editor_page_set_pasting_content_from_itself (editor_page, FALSE);
	e_editor_dom_register_input_event_listener_on_body (editor_page);
}

static void
register_html_events_handlers (EEditorPage *editor_page,
                               WebKitDOMHTMLElement *body)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keydown",
		G_CALLBACK (body_keydown_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keypress",
		G_CALLBACK (body_keypress_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"keyup",
		G_CALLBACK (body_keyup_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionstart",
		G_CALLBACK (body_compositionstart_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"compositionend",
		G_CALLBACK (body_compositionend_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"drop",
		G_CALLBACK (body_drop_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"dragstart",
		G_CALLBACK (body_dragstart_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"dragend",
		G_CALLBACK (body_dragend_event_cb),
		FALSE,
		editor_page);
}

void
e_editor_dom_convert_content (EEditorPage *editor_page,
                              const gchar *preferred_text)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *paragraph, *content_wrapper, *top_signature;
	WebKitDOMElement *cite_body_element, *signature, *wrapper;
	WebKitDOMHTMLElement *body;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMNode *node;
	WebKitDOMDOMWindow *dom_window = NULL;
	gboolean start_bottom, empty = FALSE, cite_body = FALSE;
	gchar *inner_html;
	gint ii, jj, length;
	GSettings *settings;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	start_bottom = g_settings_get_boolean (settings, "composer-reply-start-bottom");
	g_object_unref (settings);

	dom_window = webkit_dom_document_get_default_view (document);
	body = webkit_dom_document_get_body (document);
	/* Wrapper that will represent the new body. */
	wrapper = webkit_dom_document_create_element (document, "div", NULL);

	cite_body_element = webkit_dom_document_query_selector (
		document, "span.-x-evo-cite-body", NULL);

	/* content_wrapper when the processed text will be placed. */
	content_wrapper = webkit_dom_document_create_element (
		document, cite_body_element ? "blockquote" : "div", NULL);
	if (cite_body_element) {
		cite_body = TRUE;
		webkit_dom_element_set_attribute (content_wrapper, "type", "cite", NULL);
		webkit_dom_element_set_attribute (content_wrapper, "id", "-x-evo-main-cite", NULL);
		remove_node (WEBKIT_DOM_NODE (cite_body_element));
	}

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (wrapper), WEBKIT_DOM_NODE (content_wrapper), NULL);

	/* Remove all previously inserted paragraphs. */
	list = webkit_dom_document_query_selector_all (
		document, "[data-evo-paragraph]:not([data-headers])", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_clear_object (&list);

	/* Insert the paragraph where the caret will be. */
	paragraph = e_editor_dom_prepare_paragraph (editor_page, TRUE);
	webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (wrapper),
		WEBKIT_DOM_NODE (paragraph),
		start_bottom ?
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (content_wrapper)) :
			WEBKIT_DOM_NODE (content_wrapper),
		NULL);

	/* Insert signature (if presented) to the right position. */
	top_signature = webkit_dom_document_query_selector (
		document, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_document_query_selector (
		document, ".-x-evo-signature-wrapper", NULL);
	if (signature) {
		if (top_signature) {
			WebKitDOMElement *spacer;

			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (signature),
				start_bottom ?
					WEBKIT_DOM_NODE (content_wrapper) :
					webkit_dom_node_get_next_sibling (
						WEBKIT_DOM_NODE (paragraph)),
				NULL);
			/* Insert NL after the signature */
			spacer = e_editor_dom_prepare_paragraph (editor_page, FALSE);
			element_add_class (spacer, "-x-evo-top-signature-spacer");
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (spacer),
				webkit_dom_node_get_next_sibling (
					WEBKIT_DOM_NODE (signature)),
				NULL);
		} else {
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (wrapper),
				WEBKIT_DOM_NODE (signature),
				webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (
					start_bottom ? paragraph : content_wrapper)),
				NULL);
		}
	}

	/* Move credits to the body */
	list = webkit_dom_document_query_selector_all (
		document, "span.-x-evo-to-body[data-credits]", NULL);
	e_editor_page_set_allow_top_signature (editor_page, webkit_dom_node_list_get_length (list) > 0);
	for (jj = 0, ii = webkit_dom_node_list_get_length (list); ii--; jj++) {
		char *credits;
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

		element = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
		credits = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "data-credits");
		if (credits)
			webkit_dom_html_element_set_inner_text (WEBKIT_DOM_HTML_ELEMENT (element), credits, NULL);
		g_free (credits);

		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (wrapper),
			WEBKIT_DOM_NODE (element),
			WEBKIT_DOM_NODE (content_wrapper),
			NULL);

		remove_node (node);
	}
	g_clear_object (&list);

	/* Move headers to body */
	list = webkit_dom_document_query_selector_all (
		document, "div[data-headers]", NULL);
	for (jj = 0, ii = webkit_dom_node_list_get_length (list); ii--; jj++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, jj);
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (node), "data-headers");
		e_editor_dom_set_paragraph_style (editor_page, WEBKIT_DOM_ELEMENT (node), -1, 0, NULL);
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (wrapper),
			node,
			WEBKIT_DOM_NODE (content_wrapper),
			NULL);
	}
	g_clear_object (&list);

	repair_gmail_blockquotes (document);
	remove_thunderbird_signature (document);
	create_text_markers_for_citations_in_element (WEBKIT_DOM_ELEMENT (body));

	if (preferred_text && *preferred_text)
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (content_wrapper), preferred_text, NULL);
	else {
		gchar *inner_text;
		WebKitDOMNode *last_child;

		inner_text = webkit_dom_html_element_get_inner_text (body);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (content_wrapper), inner_text, NULL);

		last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (content_wrapper));
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (last_child))
			remove_node (last_child);

		g_free (inner_text);
	}

	inner_html = webkit_dom_element_get_inner_html (content_wrapper);

	/* Replace the old body with the new one. */
	node = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (body), FALSE, NULL);
	webkit_dom_node_replace_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (body)),
		node,
		WEBKIT_DOM_NODE (body),
		NULL);
	body = WEBKIT_DOM_HTML_ELEMENT (node);

	/* Copy all to nodes to the new body. */
	while ((node = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (wrapper)))) {
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (body),
			WEBKIT_DOM_NODE (node),
			webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)),
			NULL);
	}

	if (inner_html && !*inner_html)
		empty = TRUE;

	remove_node (WEBKIT_DOM_NODE (wrapper));

	length = webkit_dom_element_get_child_element_count (WEBKIT_DOM_ELEMENT (body));
	if (length <= 1) {
		empty = TRUE;
		if (length == 1) {
			WebKitDOMNode *child;

			child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
			empty = child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child);
		}
	}

	if (preferred_text && *preferred_text)
		empty = FALSE;

	if (!empty)
		parse_html_into_blocks (editor_page, content_wrapper, NULL, inner_html);
	else
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (content_wrapper),
			WEBKIT_DOM_NODE (e_editor_dom_prepare_paragraph (editor_page, FALSE)),
			NULL);

	if (!cite_body) {
		if (!empty) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (content_wrapper)))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (content_wrapper)),
					child,
					WEBKIT_DOM_NODE (content_wrapper),
					NULL);
			}
		}

		remove_node (WEBKIT_DOM_NODE (content_wrapper));
	}

	/* If not editing a message, don't add any new block and just place
	 * the caret in the beginning of content. We want to have the same
	 * behaviour when editing message as new or we start replying on top. */
	if (!signature && !start_bottom) {
		WebKitDOMNode *child;

		remove_node (WEBKIT_DOM_NODE (paragraph));
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));
		if (child)
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (child), NULL, NULL);
	}

	if ((paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
		webkit_dom_element_remove_attribute (paragraph, "id");
	if ((paragraph = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
		webkit_dom_element_remove_attribute (paragraph, "id");

	e_editor_dom_merge_siblings_if_necessary (editor_page, NULL);

	if (!e_editor_page_get_html_mode (editor_page)) {
		e_editor_dom_wrap_paragraphs_in_document (editor_page);

		quote_plain_text_elements_after_wrapping_in_document (editor_page);
	}

	clear_attributes (editor_page);

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_in_viewport (editor_page);

	/* Register on input event that is called when the content (body) is modified */
	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (body),
		"input",
		G_CALLBACK (body_input_event_cb),
		FALSE,
		editor_page);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		editor_page);

	/* Intentionally leak the WebKitDOMDOMWindow object here as otherwise the
	 * callback won't be set up. */

	register_html_events_handlers (editor_page, body);

	g_free (inner_html);
}

static void
preserve_line_breaks_in_element (WebKitDOMDocument *document,
                                 WebKitDOMElement *element,
                                 const gchar *selector)
{
	WebKitDOMNodeList *list = NULL;
	gint ii;

	if (!(list = webkit_dom_element_query_selector_all (element, selector, NULL)))
		return;

	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		gboolean insert = TRUE;
		WebKitDOMNode *node, *next_sibling;

		node = webkit_dom_node_list_item (list, ii);
		next_sibling = webkit_dom_node_get_next_sibling (node);

		if (!next_sibling)
			insert = FALSE;

		while (insert && next_sibling) {
			if (!webkit_dom_node_has_child_nodes (next_sibling) &&
			    !webkit_dom_node_get_next_sibling (next_sibling))
				insert = FALSE;
			next_sibling = webkit_dom_node_get_next_sibling (next_sibling);
		}

		if (insert && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (webkit_dom_node_get_last_child (node)))
			webkit_dom_node_append_child (
				node,
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)),
				NULL);
	}
	g_clear_object (&list);
}

static void
preserve_pre_line_breaks_in_element (WebKitDOMDocument *document,
                                     WebKitDOMElement *element)
{
	WebKitDOMHTMLCollection *collection = NULL;
	gint ii;

	if (!(collection = webkit_dom_element_get_elements_by_tag_name_as_html_collection (element, "pre")))
		return;

	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;
		gchar *inner_html;
		GString *string;

		node = webkit_dom_html_collection_item (collection, ii);
		inner_html = webkit_dom_element_get_inner_html (WEBKIT_DOM_ELEMENT (node));
		string = e_str_replace_string (inner_html, "\n", "<br>");
		webkit_dom_element_set_inner_html (WEBKIT_DOM_ELEMENT (node), string->str, NULL);
		g_string_free (string, TRUE);
		g_free (inner_html);
	}
	g_clear_object (&collection);
}

void
e_editor_dom_convert_and_insert_html_into_selection (EEditorPage *editor_page,
                                                     const gchar *html,
                                                     gboolean is_html)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *element;
	WebKitDOMNode *node, *current_block;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean has_selection;
	gchar *inner_html;
	gint citation_level;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	e_editor_dom_remove_input_event_listener_from_body (editor_page);

	e_editor_dom_selection_save (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	current_block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (current_block))
		current_block = NULL;

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		gboolean collapsed;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_PASTE;
/* FIXME WK2
		ev->type = HISTORY_PASTE_AS_TEXT;*/

		collapsed = e_editor_dom_selection_is_collapsed (editor_page);
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (!collapsed) {
			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (html);
	}

	element = webkit_dom_document_create_element (document, "div", NULL);
	if (is_html) {
		gchar *inner_text;

		webkit_dom_element_set_inner_html (element, html, NULL);

		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (element),
			"data-evo-html-to-plain-text-wrapper",
			"",
			NULL);

		/* Add the missing BR elements on the end of DIV and P elements to
		 * preserve the line breaks. But we need to do that just in case that
		 * there is another element that contains text. */
		preserve_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (element), "p, div, address");
		preserve_line_breaks_in_element (
			document,
			WEBKIT_DOM_ELEMENT (element),
			"[data-evo-html-to-plain-text-wrapper] > :matches(h1, h2, h3, h4, h5, h6)");
		preserve_pre_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (element));

		inner_text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element));
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), inner_text, NULL);

		g_free (inner_text);
	} else
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (element), html, NULL);

	inner_html = webkit_dom_element_get_inner_html (element);
	parse_html_into_blocks (editor_page, element, WEBKIT_DOM_ELEMENT (current_block), inner_html);

	g_free (inner_html);

	has_selection = !e_editor_dom_selection_is_collapsed (editor_page);
	if (has_selection && !e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		WebKitDOMRange *range = NULL;

		range = e_editor_dom_get_current_range (editor_page);
		insert_delete_event (editor_page, range);
		g_clear_object (&range);

		/* Remove the text that was meant to be replaced by the pasted text */
		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DELETE, NULL);

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		current_block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (current_block))
			current_block = NULL;
	}

	citation_level = e_editor_dom_get_citation_level (WEBKIT_DOM_NODE (selection_end_marker), FALSE);
	/* Pasting into the citation */
	if (citation_level > 0) {
		gint length;
		gint word_wrap_length;
		WebKitDOMElement *br;
		WebKitDOMNode *first_paragraph, *last_paragraph;
		WebKitDOMNode *child, *parent, *current_block;

		first_paragraph = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		last_paragraph = webkit_dom_node_get_last_child (
			WEBKIT_DOM_NODE (element));

		word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
		length = word_wrap_length - 2 * citation_level;

		/* Pasting text that was parsed just into one paragraph */
		if (webkit_dom_node_is_same_node (first_paragraph, last_paragraph)) {
			WebKitDOMNode *child, *parent, *parent_block;

			parent_block = e_editor_dom_get_parent_block_node_from_child (
				WEBKIT_DOM_NODE (selection_start_marker));

			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent_block));
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent_block));

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
			while ((child = webkit_dom_node_get_first_child (first_paragraph))) {
				if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) &&
				    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (child)) {
					WebKitDOMNode *anchor_child;

					while ((anchor_child = webkit_dom_node_get_first_child (child)))
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (
								WEBKIT_DOM_NODE (selection_start_marker)),
							anchor_child,
							WEBKIT_DOM_NODE (selection_start_marker),
							NULL);
					remove_node (child);
				} else
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (
							WEBKIT_DOM_NODE (selection_start_marker)),
						child,
						WEBKIT_DOM_NODE (selection_start_marker),
						NULL);
			}

			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent)) {
				gchar *text_content;

				text_content = webkit_dom_node_get_text_content (parent);

				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (parent),
					"href",
					text_content,
					NULL);
				g_free (text_content);
			}

			parent_block = WEBKIT_DOM_NODE (
				e_editor_dom_wrap_paragraph_length (editor_page, WEBKIT_DOM_ELEMENT (parent_block), length));
			webkit_dom_node_normalize (parent_block);
			e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (parent_block), citation_level);

			e_editor_dom_selection_restore (editor_page);

			goto out;
		}

		/* Pasting content parsed into the multiple paragraphs */
		parent = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		/* Move the elements from the first paragraph before the selection start element */
		while ((child = webkit_dom_node_get_first_child (first_paragraph)))
			webkit_dom_node_insert_before (
				parent,
				child,
				WEBKIT_DOM_NODE (selection_start_marker),
				NULL);

		remove_node (first_paragraph);

		/* If the BR element is on the last position, remove it as we don't need it */
		child = webkit_dom_node_get_last_child (parent);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
			remove_node (child);

		parent = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		child = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_end_marker));
		/* Move the elements that are in the same paragraph as the selection end
		 * on the end of pasted text, but avoid BR on the end of paragraph */
		while (child) {
			WebKitDOMNode *next_child =
				webkit_dom_node_get_next_sibling  (child);
			if (!(!next_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)))
				webkit_dom_node_append_child (last_paragraph, child, NULL);
			child = next_child;
		}

		current_block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		dom_remove_selection_markers (document);

		/* Caret will be restored on the end of pasted text */
		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (dom_create_selection_marker (document, TRUE)),
			NULL);

		webkit_dom_node_append_child (
			last_paragraph,
			WEBKIT_DOM_NODE (dom_create_selection_marker (document, FALSE)),
			NULL);

		/* Insert the paragraph with the end of the pasted text after
		 * the paragraph that contains the selection end */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			last_paragraph,
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		/* Wrap, quote and move all paragraphs from pasted text into the body */
		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))) {
			child = WEBKIT_DOM_NODE (e_editor_dom_wrap_paragraph_length (
				editor_page, WEBKIT_DOM_ELEMENT (child), length));
			e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (child), citation_level);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (last_paragraph),
				child,
				last_paragraph,
				NULL);
		}

		webkit_dom_node_normalize (last_paragraph);

		last_paragraph = WEBKIT_DOM_NODE (
			e_editor_dom_wrap_paragraph_length (
				editor_page, WEBKIT_DOM_ELEMENT (last_paragraph), length));
		e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (last_paragraph), citation_level);

		e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (parent));
		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (parent));

		current_block = WEBKIT_DOM_NODE (e_editor_dom_wrap_paragraph_length (
			editor_page, WEBKIT_DOM_ELEMENT (current_block), length));
		e_editor_dom_quote_plain_text_element_after_wrapping (editor_page, WEBKIT_DOM_ELEMENT (current_block), citation_level);

		if ((br = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
			webkit_dom_element_remove_attribute (br, "class");

		if ((br = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
			webkit_dom_element_remove_attribute (br, "class");

		if (ev) {
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);
			e_editor_undo_redo_manager_insert_history_event (manager, ev);
		}

		e_editor_dom_selection_restore (editor_page);

		goto out;
	}

	remove_node (WEBKIT_DOM_NODE (selection_start_marker));
	remove_node (WEBKIT_DOM_NODE (selection_end_marker));

	/* If the text to insert was converted just to one block, pass just its
	 * text to WebKit otherwise WebKit will insert unwanted block with
	 * extra new line. */
	if (!webkit_dom_node_get_next_sibling (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
		inner_html = webkit_dom_element_get_inner_html (
			WEBKIT_DOM_ELEMENT (webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))));
	else
		inner_html = webkit_dom_element_get_inner_html (WEBKIT_DOM_ELEMENT (element));

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_HTML, inner_html);

	if (g_str_has_suffix (inner_html, " "))
		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_TEXT, " ");

	g_free (inner_html);

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_query_selector (
		document, "* > br#-x-evo-first-br", NULL);
	if (element) {
		WebKitDOMNode *sibling;
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (element));

		sibling = webkit_dom_node_get_previous_sibling (parent);
		if (sibling)
			remove_node (WEBKIT_DOM_NODE (parent));
		else
			webkit_dom_element_remove_attribute (element, "id");
	}

	element = webkit_dom_document_query_selector (
		document, "* > br#-x-evo-last-br", NULL);
	if (element) {
		WebKitDOMNode *parent;
		WebKitDOMNode *child;

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (element));

		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent));
		if (node) {
			node = webkit_dom_node_get_first_child (node);
			if (node) {
				inner_html = webkit_dom_node_get_text_content (node);
				if (g_str_has_prefix (inner_html, UNICODE_NBSP))
					webkit_dom_character_data_replace_data (
						WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
				g_free (inner_html);
			}
		}

		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		if (has_selection) {
			/* Everything after the selection end marker have to be in separate
			 * paragraph */
			child = webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_end_marker));
			/* Move the elements that are in the same paragraph as the selection end
			 * on the end of pasted text, but avoid BR on the end of paragraph */
			while (child) {
				WebKitDOMNode *next_child =
					webkit_dom_node_get_next_sibling  (child);
				if (!(!next_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)))
					webkit_dom_node_append_child (parent, child, NULL);
				child = next_child;
			}

			remove_node (WEBKIT_DOM_NODE (element));

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_end_marker))),
				parent,
				webkit_dom_node_get_next_sibling (
					webkit_dom_node_get_parent_node (
						WEBKIT_DOM_NODE (selection_end_marker))),
				NULL);
			node = parent;
		} else {
			node = webkit_dom_node_get_next_sibling (parent);
			if (!node) {
				fix_structure_after_pasting_multiline_content (parent);
				if (!webkit_dom_node_get_first_child (parent))
					remove_node (parent);
			}
		}

		if (node) {
			/* Restore caret on the end of pasted text */
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_end_marker),
				webkit_dom_node_get_first_child (node),
				NULL);

			selection_start_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			webkit_dom_node_insert_before (
				node,
				WEBKIT_DOM_NODE (selection_start_marker),
				webkit_dom_node_get_first_child (node),
				NULL);
		}

		if (element)
			webkit_dom_element_remove_attribute (element, "id");

		if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)) && !has_selection)
			remove_node (parent);
	} else {
		/* When pasting the content that was copied from the composer, WebKit
		 * restores the selection wrongly, thus is saved wrongly and we have
		 * to fix it */
		WebKitDOMNode *block, *parent, *clone1, *clone2;

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		selection_end_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		parent = webkit_dom_node_get_parent_node (block);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (parent), "id");

		/* Check if WebKit created wrong structure */
		clone1 = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (block), FALSE, NULL);
		clone2 = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (parent), FALSE, NULL);
		if (webkit_dom_node_is_equal_node (clone1, clone2) ||
		    (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (clone1) && WEBKIT_DOM_IS_HTML_DIV_ELEMENT (clone2) &&
		     !element_has_class (WEBKIT_DOM_ELEMENT (clone2), "-x-evo-indented"))) {
			fix_structure_after_pasting_multiline_content (block);
			if (g_strcmp0 (html, "\n") == 0) {
				WebKitDOMElement *br;

				br = webkit_dom_document_create_element (document, "br", NULL);
				webkit_dom_node_append_child (
					parent, WEBKIT_DOM_NODE (br), NULL);

				webkit_dom_node_insert_before (
					parent,
					WEBKIT_DOM_NODE (selection_start_marker),
					webkit_dom_node_get_last_child (parent),
					NULL);
			} else if (!webkit_dom_node_get_first_child (parent))
				remove_node (parent);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (selection_start_marker)),
			WEBKIT_DOM_NODE (selection_end_marker),
			webkit_dom_node_get_next_sibling (
				WEBKIT_DOM_NODE (selection_start_marker)),
			NULL);
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);
 out:
	e_editor_dom_force_spell_check_in_viewport (editor_page);
	e_editor_dom_scroll_to_caret (editor_page);

	e_editor_dom_register_input_event_listener_on_body (editor_page);

	e_editor_page_emit_content_changed (editor_page);
}

static gint
get_indentation_level (WebKitDOMElement *element)
{
	WebKitDOMElement *parent;
	gint level = 0;

	if (!element)
		return 0;

	if (element_has_class (element, "-x-evo-indented"))
		level++;

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	/* Count level of indentation */
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (parent, "-x-evo-indented"))
			level++;

		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (parent));
	}

	return level;
}

static void
process_indented_element (WebKitDOMElement *element)
{
	gchar *spaces;
	WebKitDOMNode *child;

	if (!element)
		return;

	spaces = g_strnfill (4 * get_indentation_level (element), ' ');

	child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
	while (child) {
		/* If next sibling is indented blockqoute skip it,
		 * it will be processed afterwards */
		if (WEBKIT_DOM_IS_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented"))
			child = webkit_dom_node_get_next_sibling (child);

		if (WEBKIT_DOM_IS_TEXT (child)) {
			gchar *text_content;
			gchar *indented_text;

			text_content = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (child));
			indented_text = g_strconcat (spaces, text_content, NULL);

			webkit_dom_character_data_set_data (
				WEBKIT_DOM_CHARACTER_DATA (child),
				indented_text,
				NULL);

			g_free (text_content);
			g_free (indented_text);
		}

		if (!child)
			break;

		/* Move to next node */
		if (webkit_dom_node_has_child_nodes (child))
			child = webkit_dom_node_get_first_child (child);
		else if (webkit_dom_node_get_next_sibling (child))
			child = webkit_dom_node_get_next_sibling (child);
		else {
			if (webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (element), child))
				break;

			child = webkit_dom_node_get_parent_node (child);
			if (child)
				child = webkit_dom_node_get_next_sibling (child);
		}
	}
	g_free (spaces);

	webkit_dom_element_remove_attribute (element, "style");
}

static void
process_quote_nodes (WebKitDOMElement *blockquote)
{
	WebKitDOMHTMLCollection *collection = NULL;
	int ii;

	/* Replace quote nodes with symbols */
	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		blockquote, "-x-evo-quoted");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *quoted_node;
		gchar *text_content;

		quoted_node = webkit_dom_html_collection_item (collection, ii);
		text_content = webkit_dom_node_get_text_content (quoted_node);
		webkit_dom_element_set_outer_html (
			WEBKIT_DOM_ELEMENT (quoted_node), text_content, NULL);
		g_free (text_content);
	}
	g_clear_object (&collection);
}

/* Taken from GtkHTML */
static gchar *
get_alpha_value (gint value,
                 gboolean lower)
{
	GString *str;
	gchar *rv;
	gint add = lower ? 'a' : 'A';

	str = g_string_new (". ");

	do {
		g_string_prepend_c (str, ((value - 1) % 26) + add);
		value = (value - 1) / 26;
	} while (value);

	rv = str->str;
	g_string_free (str, FALSE);

	return rv;
}

/* Taken from GtkHTML */
static gchar *
get_roman_value (gint value,
                 gboolean lower)
{
	GString *str;
	const gchar *base = "IVXLCDM";
	gchar *rv;
	gint b, r, add = lower ? 'a' - 'A' : 0;

	if (value > 3999)
		return g_strdup ("?. ");

	str = g_string_new (". ");

	for (b = 0; value > 0 && b < 7 - 1; b += 2, value /= 10) {
		r = value % 10;
		if (r != 0) {
			if (r < 4) {
				for (; r; r--)
					g_string_prepend_c (str, base[b] + add);
			} else if (r == 4) {
				g_string_prepend_c (str, base[b + 1] + add);
				g_string_prepend_c (str, base[b] + add);
			} else if (r == 5) {
				g_string_prepend_c (str, base[b + 1] + add);
			} else if (r < 9) {
				for (; r > 5; r--)
					g_string_prepend_c (str, base[b] + add);
				g_string_prepend_c (str, base[b + 1] + add);
			} else if (r == 9) {
				g_string_prepend_c (str, base[b + 2] + add);
				g_string_prepend_c (str, base[b] + add);
			}
		}
	}

	rv = str->str;
	g_string_free (str, FALSE);

	return rv;
}

static void
process_list_to_plain_text (EEditorPage *editor_page,
                            WebKitDOMElement *element,
                            gint level,
                            GString *output)
{
	EContentEditorBlockFormat format;
	EContentEditorAlignment alignment;
	gint counter = 1;
	gboolean empty = TRUE;
	gchar *indent_per_level;
	WebKitDOMNode *item;
	gint word_wrap_length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	indent_per_level = g_strnfill (SPACES_PER_LIST_LEVEL, ' ');
	word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
	format = dom_get_list_format_from_node (
		WEBKIT_DOM_NODE (element));

	/* Process list items to plain text */
	item = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
	while (item) {
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (item))
			g_string_append (output, "\n");

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			gchar *space = NULL, *item_str = NULL;
			gint ii = 0;
			WebKitDOMElement *wrapped;
			GString *item_value = g_string_new ("");

			empty = FALSE;

			alignment = e_editor_dom_get_list_alignment_from_node (
				WEBKIT_DOM_NODE (item));

			wrapped = webkit_dom_element_query_selector (
				WEBKIT_DOM_ELEMENT (item), ".-x-evo-wrap-br", NULL);
			/* Wrapped text */
			if (wrapped) {
				WebKitDOMNode *node = webkit_dom_node_get_first_child (item);
				GString *line = g_string_new ("");

				while (node) {
					if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
						g_string_append (line, "\n");
						/* put spaces before line characters -> wordwraplength - indentation */
						for (ii = 0; ii < level; ii++)
							g_string_append (line, indent_per_level);
						if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
							g_string_append (line, indent_per_level);
						g_string_append (item_value, line->str);
						g_string_erase (line, 0, -1);
					} else {
						/* append text from node to line */
						gchar *text_content;
						text_content = webkit_dom_node_get_text_content (node);
						g_string_append (line, text_content);
						g_free (text_content);
					}
					node = webkit_dom_node_get_next_sibling (node);
				}

				if (alignment == E_CONTENT_EDITOR_ALIGNMENT_LEFT)
					g_string_append (item_value, line->str);

				if (alignment == E_CONTENT_EDITOR_ALIGNMENT_CENTER) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (line->str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;
					if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
						fill_length += SPACES_PER_LIST_LEVEL;
					fill_length /= 2;

					if (fill_length < 0)
						fill_length = 0;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (item_value, fill);
					g_string_append (item_value, line->str);
					g_free (fill);
				}

				if (alignment == E_CONTENT_EDITOR_ALIGNMENT_RIGHT) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (line->str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;

					if (fill_length < 0)
						fill_length = 0;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (item_value, fill);
					g_string_append (item_value, line->str);
					g_free (fill);
				}
				g_string_free (line, TRUE);
				/* that same here */
			} else {
				gchar *text_content =
					webkit_dom_node_get_text_content (item);

				g_string_append (item_value, text_content);
				g_free (text_content);
			}

			if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST) {
				space = g_strnfill (SPACES_PER_LIST_LEVEL - 2, ' ');
				item_str = g_strdup_printf (
					"%s* %s", space, item_value->str);
				g_free (space);
			}

			if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST) {
				gint length = 1, tmp = counter, spaces_count;

				while ((tmp = tmp / 10) > 1)
					length++;

				if (tmp == 1)
					length++;

				spaces_count = SPACES_ORDERED_LIST_FIRST_LEVEL - 2 - length;
				if (spaces_count > 0)
					space = g_strnfill (spaces_count, ' ');

				item_str = g_strdup_printf (
					"%s%d. %s", space && *space ? space : "", counter, item_value->str);
				g_free (space);
			}

			if (format > E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST) {
				gchar *value, spaces_count;

				if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA)
					value = get_alpha_value (counter, FALSE);
				else
					value = get_roman_value (counter, FALSE);

				spaces_count = SPACES_ORDERED_LIST_FIRST_LEVEL - strlen (value);
				if (spaces_count > 0)
					space = g_strnfill (spaces_count, ' ');
				item_str = g_strdup_printf (
					"%s%s%s", space && *space ? space : "" , value, item_value->str);
				g_free (space);
				g_free (value);
			}

			if (alignment == E_CONTENT_EDITOR_ALIGNMENT_LEFT) {
				for (ii = 0; ii < level - 1; ii++) {
					g_string_append (output, indent_per_level);
				}
				if (WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (element))
					if (dom_node_find_parent_element (item, "OL"))
						g_string_append (output, indent_per_level);
				g_string_append (output, item_str);
			}

			if (alignment == E_CONTENT_EDITOR_ALIGNMENT_RIGHT) {
				if (!wrapped) {
					gchar *fill = NULL;
					gint fill_length;

					fill_length = word_wrap_length - g_utf8_strlen (item_str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;

					if (fill_length < 0)
						fill_length = 0;

					if (g_str_has_suffix (item_str, " "))
						fill_length++;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (output, fill);
					g_free (fill);
				}
				if (g_str_has_suffix (item_str, " "))
					g_string_append_len (output, item_str, g_utf8_strlen (item_str, -1) - 1);
				else
					g_string_append (output, item_str);
			}

			if (alignment == E_CONTENT_EDITOR_ALIGNMENT_CENTER) {
				if (!wrapped) {
					gchar *fill = NULL;
					gint fill_length = 0;

					for (ii = 0; ii < level - 1; ii++)
						g_string_append (output, indent_per_level);

					fill_length = word_wrap_length - g_utf8_strlen (item_str, -1);
				        fill_length -= ii * SPACES_PER_LIST_LEVEL;
					if (WEBKIT_DOM_IS_HTML_O_LIST_ELEMENT (element))
						fill_length += SPACES_PER_LIST_LEVEL;
					fill_length /= 2;

					if (fill_length < 0)
						fill_length = 0;

					if (g_str_has_suffix (item_str, " "))
						fill_length++;

					fill = g_strnfill (fill_length, ' ');

					g_string_append (output, fill);
					g_free (fill);
				}
				if (g_str_has_suffix (item_str, " "))
					g_string_append_len (output, item_str, g_utf8_strlen (item_str, -1) - 1);
				else
					g_string_append (output, item_str);
			}

			counter++;
			item = webkit_dom_node_get_next_sibling (item);
			if (item)
				g_string_append (output, "\n");

			g_free (item_str);
			g_string_free (item_value, TRUE);
		} else if (node_is_list (item)) {
			process_list_to_plain_text (
				editor_page, WEBKIT_DOM_ELEMENT (item), level + 1, output);
			item = webkit_dom_node_get_next_sibling (item);
		} else {
			item = webkit_dom_node_get_next_sibling (item);
		}
	}

	if (webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)) && !empty)
		g_string_append (output, "\n");

	g_free (indent_per_level);
}

static void
remove_base_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "class");
	webkit_dom_element_remove_attribute (element, "id");
	webkit_dom_element_remove_attribute (element, "name");
}

static void
remove_evolution_attributes (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "data-evo-paragraph");
	webkit_dom_element_remove_attribute (element, "data-converted");
	webkit_dom_element_remove_attribute (element, "data-edit-as-new");
	webkit_dom_element_remove_attribute (element, "data-evo-draft");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "data-uri");
	webkit_dom_element_remove_attribute (element, "data-message");
	webkit_dom_element_remove_attribute (element, "data-name");
	webkit_dom_element_remove_attribute (element, "data-new-message");
	webkit_dom_element_remove_attribute (element, "data-user-wrapped");
	webkit_dom_element_remove_attribute (element, "data-evo-plain-text");
	webkit_dom_element_remove_attribute (element, "data-plain-text-style");
	webkit_dom_element_remove_attribute (element, "data-style");
	webkit_dom_element_remove_attribute (element, "spellcheck");
}

static void
convert_element_from_html_to_plain_text (EEditorPage *editor_page,
                                         WebKitDOMElement *element,
                                         gboolean *wrap,
                                         gboolean *quote)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *top_signature, *signature, *blockquote, *main_blockquote, *br_element;
	WebKitDOMNode *signature_clone, *from;
	gint blockquotes_count;
	gchar *inner_text, *inner_html;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	top_signature = webkit_dom_element_query_selector (
		element, ".-x-evo-top-signature", NULL);
	signature = webkit_dom_element_query_selector (
		element, "span.-x-evo-signature", NULL);
	main_blockquote = webkit_dom_element_query_selector (
		element, "#-x-evo-main-cite", NULL);

	blockquote = webkit_dom_document_create_element (
		document, "blockquote", NULL);

	if (main_blockquote) {
		webkit_dom_element_set_attribute (
			blockquote, "type", "cite", NULL);
		from = WEBKIT_DOM_NODE (main_blockquote);
	} else {
		if (signature) {
			WebKitDOMNode *parent = webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (signature));
			signature_clone = webkit_dom_node_clone_node_with_error (parent, TRUE, NULL);
			remove_node (parent);
		}
		from = WEBKIT_DOM_NODE (element);
	}

	blockquotes_count = create_text_markers_for_citations_in_element (WEBKIT_DOM_ELEMENT (from));
	create_text_markers_for_selection_in_element (WEBKIT_DOM_ELEMENT (from));
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (from),
		"data-evo-html-to-plain-text-wrapper",
		"",
		NULL);

	/* Add the missing BR elements on the end of DIV and P elements to
	 * preserve the line breaks. But we need to do that just in case that
	 * there is another element that contains text. */
	preserve_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (from), "p, div, address");
	preserve_line_breaks_in_element (
		document,
		WEBKIT_DOM_ELEMENT (from),
		"[data-evo-html-to-plain-text-wrapper] > :matches(h1, h2, h3, h4, h5, h6)");
	preserve_pre_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (element));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (from), "data-evo-html-to-plain-text-wrapper");

	inner_text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (from));

	webkit_dom_html_element_set_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (blockquote), inner_text, NULL);

	inner_html = webkit_dom_element_get_inner_html (blockquote);

	parse_html_into_blocks (editor_page,
		main_blockquote ? blockquote : WEBKIT_DOM_ELEMENT (element),
		NULL,
		inner_html);

	if (main_blockquote) {
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (main_blockquote)),
			WEBKIT_DOM_NODE (blockquote),
			WEBKIT_DOM_NODE (main_blockquote),
			NULL);

		remove_evolution_attributes (WEBKIT_DOM_ELEMENT (element));
	} else {
		WebKitDOMNode *first_child;

		if (signature) {
			if (!top_signature) {
				signature_clone = webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element),
					signature_clone,
					NULL);
			} else {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (element),
					signature_clone,
					webkit_dom_node_get_first_child (
						WEBKIT_DOM_NODE (element)),
					NULL);
			}
		}

		first_child = webkit_dom_node_get_first_child (
			WEBKIT_DOM_NODE (element));
		if (first_child) {
			if (!webkit_dom_node_has_child_nodes (first_child)) {
				webkit_dom_element_set_inner_html (
					WEBKIT_DOM_ELEMENT (first_child),
					"<br>",
					NULL);
			}
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);
		}
	}

	if (wrap)
		*wrap = TRUE;
	if (quote)
		*quote = main_blockquote || blockquotes_count > 0;

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-converted", "", NULL);

	if ((br_element = webkit_dom_document_get_element_by_id (document, "-x-evo-first-br")))
		webkit_dom_element_remove_attribute (br_element, "id");

	if ((br_element = webkit_dom_document_get_element_by_id (document, "-x-evo-last-br")))
		webkit_dom_element_remove_attribute (br_element, "id");

	g_free (inner_text);
	g_free (inner_html);
}

void
e_editor_dom_convert_element_from_html_to_plain_text (EEditorPage *editor_page,
                                                      WebKitDOMElement *element)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	convert_element_from_html_to_plain_text (editor_page, element, NULL, NULL);
}

static void
process_node_to_plain_text_changing_composer_mode (EEditorPage *editor_page,
                                                   WebKitDOMNode *source)
{
	WebKitDOMElement *element;
	WebKitDOMNamedNodeMap *attributes = NULL;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	attributes = webkit_dom_element_get_attributes (WEBKIT_DOM_ELEMENT (source));
	for (ii = webkit_dom_named_node_map_get_length (attributes); ii--;) {
		gchar *name = NULL;
		WebKitDOMAttr *attribute;

		attribute = WEBKIT_DOM_ATTR (webkit_dom_named_node_map_item (attributes, ii));

		name = webkit_dom_attr_get_name (attribute);

		if (g_strcmp0 (name, "bgcolor") == 0 ||
		    g_strcmp0 (name, "text") == 0 ||
		    g_strcmp0 (name, "vlink") == 0 ||
		    g_strcmp0 (name, "link") == 0) {

			webkit_dom_element_remove_attribute_node (
				WEBKIT_DOM_ELEMENT (source), attribute, NULL);
		}
		g_free (name);
	}
	g_clear_object (&attributes);

	/* Signature */
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (source), "div.-x-evo-signature-wrapper", NULL);
	if (element) {
		WebKitDOMNode *first_child;
		gchar *id = NULL;

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
		id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (first_child));

		if (g_strcmp0 (id, "none") != 0)
			convert_element_from_html_to_plain_text (
				editor_page, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);
		g_free (id);
	}
}

/* This function is different than the others there as this needs to go through
 * the DOM node by node and generate the plain text of their content. For some
 * it will just take the text content, but for example the lists are not that
 * easy. */
static void
process_node_to_plain_text_for_exporting (EEditorPage *editor_page,
                                          WebKitDOMNode *source,
                                          GString *buffer)
{
	WebKitDOMNodeList *nodes = NULL;
	gboolean html_mode;
	gchar *content = NULL;
	gint ii, length;

	html_mode = e_editor_page_get_html_mode (editor_page);

	nodes = webkit_dom_node_get_child_nodes (source);
	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *child;
		gboolean skip_node = FALSE;

		child = webkit_dom_node_list_item (nodes, ii);

		if (WEBKIT_DOM_IS_TEXT (child)) {
			gchar *class;
			const gchar *css_align = NULL;
			GRegex *regex;

			content = webkit_dom_node_get_text_content (child);
			if (!content)
				goto next;

			/* The text nodes with only '\n' are reflected only in
			 * PRE elements, otherwise skip them. */
			/* FIXME wrong for "white-space: pre", but we don't use
			 * that in editor in our expected DOM structure */
			if (content[0] == '\n' && content[1] == '\0' &&
			    !WEBKIT_DOM_IS_HTML_PRE_ELEMENT (source)) {
				g_free (content);
				skip_node = TRUE;
				goto next;
			}

			if (strstr (content, UNICODE_ZERO_WIDTH_SPACE)) {
				gchar *tmp;

				regex = g_regex_new (UNICODE_ZERO_WIDTH_SPACE, 0, 0, NULL);
				tmp = g_regex_replace (
					regex, content, -1, 0, "", 0, NULL);
				g_free (content);
				content = tmp;
				g_regex_unref (regex);
			}

			class = webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (source));
			if (class && (css_align = strstr (class, "-x-evo-align-"))) {
				gchar *content_with_align;
				gint word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);

				if (!g_str_has_prefix (css_align + 13, "left")) {
					gchar *align;
					gint len;

					if (g_str_has_prefix (css_align + 13, "center"))
						len = (word_wrap_length - g_utf8_strlen (content, -1)) / 2;
					else
						len = word_wrap_length - g_utf8_strlen (content, -1);

					if (len < 0)
						len = 0;

					if (g_str_has_suffix (content, " ")) {
						gchar *tmp;

						len++;
						align = g_strnfill (len, ' ');

						tmp = g_strndup (content, g_utf8_strlen (content, -1) -1);

						content_with_align = g_strconcat (
							align, tmp, NULL);
						g_free (tmp);
					} else {
						align = g_strnfill (len, ' ');

						content_with_align = g_strconcat (
							align, content, NULL);
					}

					g_free (content);
					g_free (align);
					content = content_with_align;
				}
			}

			g_free (class);

			g_string_append (buffer, content);

			g_free (content);
			content = NULL;

			goto next;
		}

		if (!WEBKIT_DOM_IS_ELEMENT (child))
			goto next;

		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "Apple-tab-span")) {
			content = webkit_dom_node_get_text_content (child);
			g_string_append (buffer, content);
			g_free (content);
			skip_node = TRUE;
			goto next;
		}

		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (child))
			process_quote_nodes (WEBKIT_DOM_ELEMENT (child));

		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-indented"))
			process_indented_element (WEBKIT_DOM_ELEMENT (child));

		if (node_is_list (child)) {
			process_list_to_plain_text (editor_page, WEBKIT_DOM_ELEMENT (child), 1, buffer);
			skip_node = TRUE;
			goto next;
		}

		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-resizable-wrapper") &&
		    !element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-smiley-wrapper")) {
			skip_node = TRUE;
			goto next;
		}

		/* Signature */
		if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (child) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-signature-wrapper")) {
			WebKitDOMNode *first_child;
			gchar *id;

			first_child = webkit_dom_node_get_first_child (child);

			/* Don't generate any text if the signature is set to None. */
			id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (first_child));
			if (g_strcmp0 (id, "none") == 0) {
				g_free (id);

				remove_node (child);
				skip_node = TRUE;
				length--;
				ii--;
				goto next;
			}
			g_free (id);

			if (html_mode)
				convert_element_from_html_to_plain_text (
					editor_page, WEBKIT_DOM_ELEMENT (first_child), NULL, NULL);

			goto next;
		}

		/* Replace smileys with their text representation */
		if (element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-smiley-wrapper")) {
			WebKitDOMNode *text_version;

			text_version = webkit_dom_node_get_last_child (child);
			content = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (text_version));
			g_string_append (buffer, content);
			g_free (content);
			skip_node = TRUE;
			goto next;
		}

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child)) {
			g_string_append (buffer, "\n");
			goto next;
		}

		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (child)) {
			content = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (child));
			g_string_append (buffer, content);
			g_free (content);
			skip_node = TRUE;
		}
 next:
		if (!skip_node && webkit_dom_node_has_child_nodes (child))
			process_node_to_plain_text_for_exporting (editor_page, child, buffer);
	}
	g_clear_object (&nodes);

	if (!g_str_has_suffix (buffer->str, "\n") &&
	     (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (source) ||
	      WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT (source) ||
	      WEBKIT_DOM_IS_HTML_PRE_ELEMENT (source) ||
	      WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (source)))
		g_string_append (buffer, "\n");

	if (g_str_has_suffix (buffer->str, "\n") && buffer->len > 1 &&
	    WEBKIT_DOM_IS_HTML_BODY_ELEMENT (source))
		g_string_truncate (buffer, buffer->len - 1);
}

static void
process_node_to_html_changing_composer_mode (EEditorPage *editor_page,
                                             WebKitDOMNode *source)
{
}

static void
process_node_to_html_for_exporting (EEditorPage *editor_page,
                                    WebKitDOMNode *source)
{
	WebKitDOMNodeList *list = NULL;
	WebKitDOMHTMLCollection *collection = NULL;
	WebKitDOMElement *element;
	WebKitDOMDocument *document;
	gint ii;

	document = webkit_dom_node_get_owner_document (source);

	remove_evolution_attributes (WEBKIT_DOM_ELEMENT (source));

	/* Aligned elements */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), "[class*=\"-x-evo-align\"]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		gchar *class = NULL;
		WebKitDOMNode *node;
		gboolean center = FALSE;

		node = webkit_dom_node_list_item (list, ii);
		class = webkit_dom_element_get_class_name (WEBKIT_DOM_ELEMENT (node));
		center = g_strrstr (class, "center") != NULL;
		if (center || g_strrstr (class, "right")) {
			if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node))
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node),
					"style",
					center ?
						"list-style-position: inside; text-align: center" :
						"list-style-position: inside; text-align: right",
					NULL);
			else
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node),
					"style",
					center ?
						"text-align: center" :
						"text-align: right",
					NULL);
		}
		element_remove_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-left");
		element_remove_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-center");
		element_remove_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-right");
		g_free (class);
	}
	g_clear_object (&list);

	/* Indented elements */
	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "-x-evo-indented");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (collection, ii);
		element_remove_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-indented");
		remove_evolution_attributes (WEBKIT_DOM_ELEMENT (node));
	}
	g_clear_object (&collection);

	/* Tab characters */
	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "Apple-tab-span");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		gchar *text_content;
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (collection, ii);
		text_content = webkit_dom_node_get_text_content (node);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (webkit_dom_document_create_text_node (document, text_content)),
			node,
			NULL);

		remove_node (node);
		g_free (text_content);
	}
	g_clear_object (&collection);

	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "-x-evo-quoted");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *quoted_node;
		gchar *text_content;

		quoted_node = webkit_dom_html_collection_item (collection, ii);
		text_content = webkit_dom_node_get_text_content (quoted_node);
		webkit_dom_element_set_outer_html (
			WEBKIT_DOM_ELEMENT (quoted_node), text_content, NULL);

		g_free (text_content);
	}
	g_clear_object (&collection);

	/* Images */
	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), ".-x-evo-resizable-wrapper:not(.-x-evo-smiley-wrapper)", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node, *image;

		node = webkit_dom_node_list_item (list, ii);
		image = webkit_dom_node_get_first_child (node);

		if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (image)) {
			remove_evolution_attributes (
				WEBKIT_DOM_ELEMENT (image));

			webkit_dom_node_replace_child (
				webkit_dom_node_get_parent_node (node), image, node, NULL);
		}
	}
	g_clear_object (&list);

	/* Signature */
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (source), "div.-x-evo-signature-wrapper", NULL);
	if (element) {
		WebKitDOMNode *first_child;
		gchar *id;

		/* Don't generate any text if the signature is set to None. */
		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element));
		id = webkit_dom_element_get_id (WEBKIT_DOM_ELEMENT (first_child));
		if (g_strcmp0 (id, "none") == 0) {
			remove_node (WEBKIT_DOM_NODE (element));
		} else {
			remove_base_attributes (element);
			remove_base_attributes (WEBKIT_DOM_ELEMENT (first_child));
			remove_evolution_attributes (WEBKIT_DOM_ELEMENT (first_child));
		}
		g_free (id);
	}

	/* Smileys */
	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "-x-evo-smiley-wrapper");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;
		WebKitDOMElement *img;

		node = webkit_dom_html_collection_item (collection, ii);
		img = WEBKIT_DOM_ELEMENT (webkit_dom_node_get_first_child (node));

		remove_evolution_attributes (img);
		remove_base_attributes (img);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (img),
			node,
			NULL);
	}
	g_clear_object (&collection);

	collection = webkit_dom_element_get_elements_by_tag_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "pre");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (collection, ii);
		remove_evolution_attributes (WEBKIT_DOM_ELEMENT (node));
	}
	g_clear_object (&collection);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), "[data-evo-paragraph]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		remove_evolution_attributes (WEBKIT_DOM_ELEMENT (node));
		remove_base_attributes (WEBKIT_DOM_ELEMENT (node));
	}
	g_clear_object (&list);

	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		WEBKIT_DOM_ELEMENT (source), "-x-evo-wrap-br");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (collection, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "class");
	}
	g_clear_object (&collection);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), "#-x-evo-main-cite", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "id");
	}
	g_clear_object (&list);
}

static void
remove_image_attributes_from_element (WebKitDOMElement *element)
{
	webkit_dom_element_remove_attribute (element, "background");
	webkit_dom_element_remove_attribute (element, "data-uri");
	webkit_dom_element_remove_attribute (element, "data-inline");
	webkit_dom_element_remove_attribute (element, "data-name");
}

static void
remove_background_images_in_element (WebKitDOMElement *element)
{
	gint ii;
	WebKitDOMNodeList *images = NULL;

	images = webkit_dom_element_query_selector_all (
		element, "[background][data-inline]", NULL);
	for (ii = webkit_dom_node_list_get_length (images); ii--;) {
		WebKitDOMElement *image = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (images, ii));

		remove_image_attributes_from_element (image);
	}
	g_clear_object (&images);

	remove_image_attributes_from_element (element);
}

static void
remove_images_in_element (WebKitDOMElement *element)
{
	gint ii;
	WebKitDOMNodeList *images = NULL;

	images = webkit_dom_element_query_selector_all (
		element, "img:not(.-x-evo-smiley-img)", NULL);
	for (ii = webkit_dom_node_list_get_length (images); ii--;)
		remove_node (webkit_dom_node_list_item (images, ii));
	g_clear_object (&images);
}

static void
remove_images (WebKitDOMDocument *document)
{
	remove_images_in_element (
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)));
}

static void
toggle_smileys (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLCollection *collection = NULL;
	gboolean html_mode;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);

	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "-x-evo-smiley-img");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *img = webkit_dom_html_collection_item (collection, ii);
		WebKitDOMElement *parent = webkit_dom_node_get_parent_element (img);

		if (html_mode)
			element_add_class (parent, "-x-evo-resizable-wrapper");
		else
			element_remove_class (parent, "-x-evo-resizable-wrapper");
	}
	g_clear_object (&collection);
}

static void
toggle_paragraphs_style_in_element (EEditorPage *editor_page,
                                    WebKitDOMElement *element,
                                    gboolean html_mode)
{
	gint ii;
	WebKitDOMNodeList *paragraphs = NULL;

	paragraphs = webkit_dom_element_query_selector_all (
		element, ":not(td) > [data-evo-paragraph]", NULL);

	for (ii = webkit_dom_node_list_get_length (paragraphs); ii--;) {
		gchar *style;
		const gchar *css_align;
		WebKitDOMNode *node = webkit_dom_node_list_item (paragraphs, ii);

		if (html_mode) {
			style = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "style");

			if (style && (css_align = strstr (style, "text-align: "))) {
				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node),
					"style",
					g_str_has_prefix (css_align + 12, "center") ?
						"text-align: center" :
						"text-align: right",
					NULL);
			} else {
				/* In HTML mode the paragraphs don't have width limit */
				webkit_dom_element_remove_attribute (
					WEBKIT_DOM_ELEMENT (node), "style");
			}
			g_free (style);
		} else {
			WebKitDOMNode *parent;

			parent = webkit_dom_node_get_parent_node (node);
			/* If the paragraph is inside indented paragraph don't set
			 * the style as it will be inherited */
			if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent) && node_is_list (node)) {
				gint offset;

				offset = WEBKIT_DOM_IS_HTML_U_LIST_ELEMENT (node) ?
					SPACES_PER_LIST_LEVEL : SPACES_ORDERED_LIST_FIRST_LEVEL;
				/* In plain text mode the paragraphs have width limit */
				e_editor_dom_set_paragraph_style (
					editor_page, WEBKIT_DOM_ELEMENT (node), -1, -offset, NULL);
			} else if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented")) {
				const gchar *style_to_add = "";
				style = webkit_dom_element_get_attribute (
					WEBKIT_DOM_ELEMENT (node), "style");

				if (style && (css_align = strstr (style, "text-align: "))) {
					style_to_add = g_str_has_prefix (
						css_align + 12, "center") ?
							"text-align: center;" :
							"text-align: right;";
				}

				/* In plain text mode the paragraphs have width limit */
				e_editor_dom_set_paragraph_style (
					editor_page, WEBKIT_DOM_ELEMENT (node), -1, 0, style_to_add);

				g_free (style);
			}
		}
	}
	g_clear_object (&paragraphs);
}

static void
toggle_paragraphs_style (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	toggle_paragraphs_style_in_element (
		editor_page,
		WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document)),
		e_editor_page_get_html_mode (editor_page));
}

gchar *
e_editor_dom_process_content_for_draft (EEditorPage *editor_page,
					gboolean only_inner_body)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMElement *document_element;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMNode *document_element_clone;
	gboolean selection_saved = FALSE;
	gchar *content;
	gint ii;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft", "", NULL);

	if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
		selection_saved = TRUE;

	if (!selection_saved)
		e_editor_dom_selection_save (editor_page);

	document_element = webkit_dom_document_get_document_element (document);

	document_element_clone = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (document_element), TRUE, NULL);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (document_element_clone), "a.-x-evo-visited-link", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *anchor;

		anchor = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (anchor), "class");
	}
	g_clear_object (&list);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (document_element_clone), "#-x-evo-input-start", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (list, ii);
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "id");
	}
	g_clear_object (&list);

	if (only_inner_body) {
		WebKitDOMElement *body;
		WebKitDOMNode *first_child;

		body = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (document_element_clone), "body", NULL);

		first_child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		if (!e_editor_page_get_html_mode (editor_page))
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (first_child),
				"data-evo-signature-plain-text-mode",
				"",
				NULL);

		content = webkit_dom_element_get_inner_html (body);

		if (!e_editor_page_get_html_mode (editor_page))
			webkit_dom_element_remove_attribute (
				WEBKIT_DOM_ELEMENT (first_child),
				"data-evo-signature-plain-text-mode");
	} else
		content = webkit_dom_element_get_outer_html (
			WEBKIT_DOM_ELEMENT (document_element_clone));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-draft");

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_in_viewport (editor_page);

	if (selection_saved)
		e_editor_dom_selection_save (editor_page);

	return content;
}

static void
toggle_indented_elements (EEditorPage *editor_page)
{
	gboolean html_mode;
	gint ii;
	WebKitDOMDocument *document;
	WebKitDOMHTMLCollection *collection = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);
	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "-x-evo-indented");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node = webkit_dom_html_collection_item (collection, ii);

		if (html_mode)
			dom_element_swap_attributes (WEBKIT_DOM_ELEMENT (node), "style", "data-plain-text-style");
		else
			dom_element_swap_attributes (WEBKIT_DOM_ELEMENT (node), "data-plain-text-style", "style");
	}
	g_clear_object (&collection);
}

static void
process_content_to_html_changing_composer_mode (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *body;
	WebKitDOMElement *blockquote;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-plain-text");
	blockquote = webkit_dom_document_query_selector (
		document, "blockquote[type|=cite]", NULL);

	if (blockquote)
		dom_dequote_plain_text (document);

	toggle_paragraphs_style (editor_page);
	toggle_smileys (editor_page);
	remove_images (document);
	e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (body));

	process_node_to_html_changing_composer_mode (editor_page, body);
}

static void
wrap_paragraphs_in_quoted_content (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *paragraphs = NULL;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	paragraphs = webkit_dom_document_query_selector_all (
		document, "blockquote[type=cite] > [data-evo-paragraph]", NULL);
	for (ii = webkit_dom_node_list_get_length (paragraphs); ii--;) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (paragraphs, ii);

		e_editor_dom_wrap_paragraph (editor_page, WEBKIT_DOM_ELEMENT (paragraph));
	}
	g_clear_object (&paragraphs);
}

static void
process_content_to_plain_text_changing_composer_mode (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *body, *head, *node;
	WebKitDOMElement *blockquote;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	head = WEBKIT_DOM_NODE (webkit_dom_document_get_head (document));

	while ((node = webkit_dom_node_get_last_child (head)))
		remove_node (node);

	e_editor_dom_selection_save (editor_page);

	webkit_dom_element_remove_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-user-colors");

	e_editor_page_emit_user_changed_default_colors (editor_page, FALSE);

	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-evo-plain-text", "", NULL);

	blockquote = webkit_dom_document_query_selector (
		document, "blockquote[type|=cite]", NULL);

	if (blockquote) {
		wrap_paragraphs_in_quoted_content (editor_page);
		preserve_pre_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (body));
		quote_plain_text_elements_after_wrapping_in_document (editor_page);
	}

	toggle_paragraphs_style (editor_page);
	toggle_smileys (editor_page);
	toggle_indented_elements (editor_page);
	remove_images (document);
	remove_background_images_in_element (WEBKIT_DOM_ELEMENT (body));

	process_node_to_plain_text_changing_composer_mode (editor_page, body);

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_in_viewport (editor_page);
}

gchar *
e_editor_dom_process_content_to_plain_text_for_exporting (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *body, *source;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	gboolean wrap = TRUE, quote = FALSE, remove_last_new_line = FALSE;
	gint ii;
	GString *plain_text;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	plain_text = g_string_sized_new (1024);

	body = WEBKIT_DOM_NODE (webkit_dom_document_get_body (document));
	source = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (body), TRUE, NULL);

	e_editor_dom_selection_save (editor_page);

	/* If composer is in HTML mode we have to move the content to plain version */
	if (e_editor_page_get_html_mode (editor_page)) {
		if (e_editor_dom_check_if_conversion_needed (editor_page)) {
			WebKitDOMElement *wrapper;
			WebKitDOMNode *child, *last_child;

			wrapper = webkit_dom_document_create_element (document, "div", NULL);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (wrapper),
				"data-evo-html-to-plain-text-wrapper",
				"",
				NULL);
			while ((child = webkit_dom_node_get_first_child (source))) {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (wrapper),
					child,
					NULL);
			}

			list = webkit_dom_element_query_selector_all (
				wrapper, "#-x-evo-input-start", NULL);
			for (ii = webkit_dom_node_list_get_length (list); ii--;) {
				WebKitDOMNode *paragraph;

				paragraph = webkit_dom_node_list_item (list, ii);

				webkit_dom_element_remove_attribute (
					WEBKIT_DOM_ELEMENT (paragraph), "id");
			}
			g_clear_object (&list);

			remove_images_in_element (wrapper);

			list = webkit_dom_element_query_selector_all (
				wrapper, "[data-evo-html-to-plain-text-wrapper] > :matches(ul, ol)", NULL);
			for (ii = webkit_dom_node_list_get_length (list); ii--;) {
				WebKitDOMElement *list_pre;
				WebKitDOMNode *item;
				GString *list_plain_text;

				item = webkit_dom_node_list_item (list, ii);

				list_plain_text = g_string_new ("");

				process_list_to_plain_text (
					editor_page, WEBKIT_DOM_ELEMENT (item), 1, list_plain_text);

				list_pre = webkit_dom_document_create_element (document, "pre", NULL);
				webkit_dom_html_element_set_inner_text (
					WEBKIT_DOM_HTML_ELEMENT (list_pre),
					g_string_free (list_plain_text, FALSE),
					NULL);
				webkit_dom_node_replace_child (
					WEBKIT_DOM_NODE (wrapper),
					WEBKIT_DOM_NODE (list_pre),
					item,
					NULL);
			}
			g_clear_object (&list);

			/* BR on the end of the last element would cause an extra
			 * new line, remove it if there are some nodes before it. */
			last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (wrapper));
			while (webkit_dom_node_get_last_child (last_child))
				last_child = webkit_dom_node_get_last_child (last_child);

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (last_child) &&
			    webkit_dom_node_get_previous_sibling (last_child))
				remove_node (last_child);

			convert_element_from_html_to_plain_text (
				editor_page, wrapper, &wrap, &quote);

			source = WEBKIT_DOM_NODE (wrapper);

			remove_last_new_line = TRUE;
		} else {
			toggle_paragraphs_style_in_element (
				editor_page, WEBKIT_DOM_ELEMENT (source), FALSE);
			remove_images_in_element (
				WEBKIT_DOM_ELEMENT (source));
			remove_background_images_in_element (
				WEBKIT_DOM_ELEMENT (source));
		}
	}

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (source), "[data-evo-paragraph]", NULL);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
	g_clear_object (&dom_window);
	g_clear_object (&dom_selection);

	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *paragraph;

		paragraph = webkit_dom_node_list_item (list, ii);

		if (node_is_list (paragraph)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (paragraph);

			while (item) {
				WebKitDOMNode *next_item =
					webkit_dom_node_get_next_sibling (item);

				if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item))
					e_editor_dom_wrap_paragraph (editor_page, WEBKIT_DOM_ELEMENT (item));

				item = next_item;
			}
		} else if (!webkit_dom_element_query_selector (WEBKIT_DOM_ELEMENT (paragraph), ".-x-evo-wrap-br,.-x-evo-quoted", NULL)) {
			/* Don't try to wrap the already wrapped content. */
			e_editor_dom_wrap_paragraph (editor_page, WEBKIT_DOM_ELEMENT (paragraph));
		}
	}
	g_clear_object (&list);

	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker")))
		remove_node (WEBKIT_DOM_NODE (element));
	if ((element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker")))
		remove_node (WEBKIT_DOM_NODE (element));

	webkit_dom_node_normalize (source);

	if (quote) {
		quote_plain_text_elements_after_wrapping_in_element (editor_page, WEBKIT_DOM_ELEMENT (source));
	} else if (e_editor_page_get_html_mode (editor_page)) {
		WebKitDOMElement *citation;

		citation = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (source), "blockquote[type=cite]", NULL);
		if (citation) {
			preserve_pre_line_breaks_in_element (document, WEBKIT_DOM_ELEMENT (source));
			quote_plain_text_elements_after_wrapping_in_element (editor_page, WEBKIT_DOM_ELEMENT (source));
		}
	}

	process_node_to_plain_text_for_exporting (editor_page, source, plain_text);
	/* Truncate the extra new line on the end of generated text as the
	 * check inside the previous function is based on whether the processed
	 * node is BODY or not, but in this case the content is wrapped in DIV. */
	if (remove_last_new_line)
		g_string_truncate (plain_text, plain_text->len - 1);

	e_editor_dom_selection_restore (editor_page);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

static void
restore_image (WebKitDOMDocument *document,
               const gchar *id,
               const gchar *element_src)
{
	gchar *selector;
	gint ii;
	WebKitDOMNodeList *list = NULL;

	selector = g_strconcat ("[data-inline][background=\"cid:", id, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "background", element_src, NULL);
	}
	g_free (selector);
	g_clear_object (&list);

	selector = g_strconcat ("[data-inline][src=\"cid:", id, "\"]", NULL);
	list = webkit_dom_document_query_selector_all (document, selector, NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMElement *element = WEBKIT_DOM_ELEMENT (
			webkit_dom_node_list_item (list, ii));

		webkit_dom_element_set_attribute (element, "src", element_src, NULL);
	}
	g_free (selector);
	g_clear_object (&list);
}

void
e_editor_dom_restore_images (EEditorPage *editor_page,
                             GVariant *inline_images_to_restore)
{
	WebKitDOMDocument *document;
	const gchar *element_src, *name, *id;
	GVariantIter *iter;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	g_variant_get (inline_images_to_restore, "a(sss)", &iter);
	while (g_variant_iter_loop (iter, "(&s&s&s)", &element_src, &name, &id))
		restore_image (document, id, element_src);

	g_variant_iter_free (iter);
}

gchar *
e_editor_dom_process_content_to_html_for_exporting (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *document_clone;
	WebKitDOMNodeList *list = NULL;
	GSettings *settings;
	gint ii;
	gchar *html_content;
	gboolean send_editor_colors = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	document_clone = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (webkit_dom_document_get_document_element (document)), TRUE, NULL);
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-quote-style", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-a-color-style-visited", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	/* When the Ctrl + Enter is pressed for sending, the links are activated. */
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "style#-x-evo-style-a", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	node = WEBKIT_DOM_NODE (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (document_clone), "body", NULL));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-start-marker", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));
	element = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (node), "#-x-evo-selection-end-marker", NULL);
	if (element)
		remove_node (WEBKIT_DOM_NODE (element));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	send_editor_colors = g_settings_get_boolean (settings, "composer-inherit-theme-colors");
	g_object_unref (settings);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-user-colors")) {
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "data-user-colors");
	} else if (!send_editor_colors) {
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "bgcolor");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "text");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "link");
		webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (node), "vlink");
	}

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (node), "span[data-hidden-space]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;)
		remove_node (webkit_dom_node_list_item (list, ii));
	g_clear_object (&list);

	list = webkit_dom_element_query_selector_all (
		WEBKIT_DOM_ELEMENT (node), "[data-style]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *data_style_node;

		data_style_node = webkit_dom_node_list_item (list, ii);

		element_rename_attribute (WEBKIT_DOM_ELEMENT (data_style_node), "data-style", "style");
	}
	g_clear_object (&list);

	process_node_to_html_for_exporting (editor_page, node);

	html_content = webkit_dom_element_get_outer_html (
		WEBKIT_DOM_ELEMENT (document_clone));

	if (strstr (html_content, UNICODE_ZERO_WIDTH_SPACE)) {
		GString *processed;

		processed = e_str_replace_string (html_content, UNICODE_ZERO_WIDTH_SPACE, "");
		g_free (html_content);
		html_content = g_string_free (processed, FALSE);
	}

	return html_content;
}

void
e_editor_dom_convert_when_changing_composer_mode (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	gboolean quote = FALSE, wrap = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	body = webkit_dom_document_get_body (document);

	convert_element_from_html_to_plain_text (
		editor_page, WEBKIT_DOM_ELEMENT (body), &wrap, &quote);

	if (wrap)
		e_editor_dom_wrap_paragraphs_in_document (editor_page);

	if (quote) {
		e_editor_dom_selection_save (editor_page);
		if (wrap)
			quote_plain_text_elements_after_wrapping_in_document (editor_page);
		else
			body = WEBKIT_DOM_HTML_ELEMENT (dom_quote_plain_text (document));
		e_editor_dom_selection_restore (editor_page);
	}

	toggle_paragraphs_style (editor_page);
	toggle_smileys (editor_page);
	remove_images (document);
	remove_background_images_in_element (WEBKIT_DOM_ELEMENT (body));

	clear_attributes (editor_page);

	if (!e_editor_page_get_html_mode (editor_page))
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-evo-plain-text", "", NULL);
	else
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-evo-plain-text");

	e_editor_dom_force_spell_check_in_viewport (editor_page);
	e_editor_dom_scroll_to_caret (editor_page);
}

static void
set_base64_to_element_attribute (GHashTable *inline_images,
                                 WebKitDOMElement *element,
                                 const gchar *attribute)
{
	gchar *attribute_value;
	const gchar *base64_src;

	attribute_value = webkit_dom_element_get_attribute (element, attribute);

	if (attribute_value && (base64_src = g_hash_table_lookup (inline_images, attribute_value)) != NULL) {
		const gchar *base64_data = strstr (base64_src, ";") + 1;
		gchar *name;
		glong name_length;

		name_length =
			g_utf8_strlen (base64_src, -1) -
			g_utf8_strlen (base64_data, -1) - 1;
		name = g_strndup (base64_src, name_length);

		webkit_dom_element_set_attribute (element, "data-inline", "", NULL);
		webkit_dom_element_set_attribute (element, "data-name", name, NULL);
		webkit_dom_element_set_attribute (element, attribute, base64_data, NULL);

		g_free (name);
	}
	g_free (attribute_value);
}

static void
change_cid_images_src_to_base64 (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *document_element;
	WebKitDOMNamedNodeMap *attributes = NULL;
	WebKitDOMNodeList *list = NULL;
	GHashTable *inline_images;
	gint ii, length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	inline_images = e_editor_page_get_inline_images (editor_page);

	document_element = webkit_dom_document_get_document_element (document);

	list = webkit_dom_document_query_selector_all (document, "img[src^=\"cid:\"]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (inline_images, WEBKIT_DOM_ELEMENT (node), "src");
	}
	g_clear_object (&list);

	/* Namespaces */
	attributes = webkit_dom_element_get_attributes (document_element);
	length = webkit_dom_named_node_map_get_length (attributes);
	for (ii = 0; ii < length; ii++) {
		gchar *name;
		WebKitDOMAttr *attribute = WEBKIT_DOM_ATTR( webkit_dom_named_node_map_item (attributes, ii));

		name = webkit_dom_attr_get_name (attribute);

		if (g_str_has_prefix (name, "xmlns:")) {
			const gchar *ns = name + 6;
			gchar *attribute_ns = g_strconcat (ns, ":src", NULL);
			gchar *selector = g_strconcat ("img[", ns, "\\:src^=\"cid:\"]", NULL);
			gint jj;

			list = webkit_dom_document_query_selector_all (
				document, selector, NULL);
			for (jj = webkit_dom_node_list_get_length (list); jj--;) {
				WebKitDOMNode *node = webkit_dom_node_list_item (list, jj);

				set_base64_to_element_attribute (
					inline_images, WEBKIT_DOM_ELEMENT (node), attribute_ns);
			}

			g_clear_object (&list);
			g_free (attribute_ns);
			g_free (selector);
		}
		g_free (name);
	}
	g_clear_object (&attributes);

	list = webkit_dom_document_query_selector_all (
		document, "[background^=\"cid:\"]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		set_base64_to_element_attribute (
			inline_images, WEBKIT_DOM_ELEMENT (node), "background");
	}
	g_clear_object (&list);
}

static void
adapt_to_editor_dom_changes (WebKitDOMDocument *document)
{
	WebKitDOMHTMLCollection *collection = NULL;
	gint ii;

	/* Normal block code div.-x-evo-paragraph replaced by div[data-evo-paragraph] */
	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (document, "-x-evo-paragraph");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node;

		node = webkit_dom_html_collection_item (collection, ii);
		element_remove_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-paragraph");
		webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "data-evo-paragraph", "", NULL);
	}
	g_clear_object (&collection);
}

void
e_editor_dom_process_content_after_load (EEditorPage *editor_page)
{
	gboolean html_mode;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMDOMWindow *dom_window = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS, "false");
	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR, "div");

	body = webkit_dom_document_get_body (document);

	webkit_dom_element_remove_attribute (WEBKIT_DOM_ELEMENT (body), "style");
	html_mode = e_editor_page_get_html_mode (editor_page);
	if (!html_mode)
		webkit_dom_element_set_attribute (
			WEBKIT_DOM_ELEMENT (body), "data-evo-plain-text", "", NULL);

	if (e_editor_page_get_convert_in_situ (editor_page)) {
		e_editor_dom_convert_content (editor_page, NULL);
		/* The BODY could be replaced during the conversion */
		body = webkit_dom_document_get_body (document);
		/* Make the quote marks non-selectable. */
		e_editor_dom_disable_quote_marks_select (editor_page);
		dom_set_links_active (document, FALSE);
		e_editor_page_set_convert_in_situ (editor_page, FALSE);

		/* The composer body could be empty in some case (loading an empty string
		 * or empty HTML). In that case create the initial paragraph. */
		if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body))) {
			WebKitDOMElement *paragraph;

			paragraph = e_editor_dom_prepare_paragraph (editor_page, TRUE);
			webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (paragraph), NULL);
			e_editor_dom_selection_restore (editor_page);
		}

		goto out;
	}

	adapt_to_editor_dom_changes (document);

	/* Make the quote marks non-selectable. */
	e_editor_dom_disable_quote_marks_select (editor_page);
	dom_set_links_active (document, FALSE);
	put_body_in_citation (document);
	move_elements_to_body (editor_page);
	repair_gmail_blockquotes (document);
	remove_thunderbird_signature (document);

	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (body), "data-evo-draft")) {
		/* Restore the selection how it was when the draft was saved */
		e_editor_dom_move_caret_into_element (editor_page, WEBKIT_DOM_ELEMENT (body), FALSE);
		e_editor_dom_selection_restore (editor_page);
		e_editor_dom_remove_embedded_style_sheet (editor_page);
	}

	/* The composer body could be empty in some case (loading an empty string
	 * or empty HTML. In that case create the initial paragraph. */
	if (!webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body))) {
		WebKitDOMElement *paragraph;

		paragraph = e_editor_dom_prepare_paragraph (editor_page, TRUE);
		webkit_dom_element_set_id (paragraph, "-x-evo-input-start");
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (body), WEBKIT_DOM_NODE (paragraph), NULL);
		e_editor_dom_selection_restore (editor_page);
	}

	e_editor_dom_fix_file_uri_images (editor_page);
	change_cid_images_src_to_base64 (editor_page);

 out:
	/* Register on input event that is called when the content (body) is modified */
	e_editor_dom_register_input_event_listener_on_body (editor_page);
	register_html_events_handlers (editor_page, body);

	if (e_editor_page_get_inline_spelling_enabled (editor_page))
		e_editor_dom_force_spell_check_in_viewport (editor_page);
	else
		e_editor_dom_turn_spell_check_off (editor_page);

	e_editor_dom_scroll_to_caret (editor_page);

	dom_window = webkit_dom_document_get_default_view (document);

	webkit_dom_event_target_add_event_listener (
		WEBKIT_DOM_EVENT_TARGET (dom_window),
		"scroll",
		G_CALLBACK (body_scroll_event_cb),
		FALSE,
		editor_page);

	/* Intentionally leak the WebKitDOMDOMWindow object here as otherwise the
	 * callback won't be set up. */
}

static gchar *
encode_to_base64_data (const gchar *src_uri,
		       gchar **data_name)
{
	GFile *file;
	GFileInfo *info;
	gchar *filename, *data = NULL;

	g_return_val_if_fail (src_uri != NULL, NULL);

	file = g_file_new_for_uri (src_uri);
	if (!file)
		return NULL;

	filename = g_file_get_path (file);
	if (!filename) {
		g_object_unref (file);
		return NULL;
	}

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
		G_FILE_QUERY_INFO_NONE, NULL, NULL);

	if (info) {
		gchar *mime_type, *content = NULL;
		gsize length = 0;

		mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (info));

		if (mime_type && g_file_get_contents (filename, &content, &length, NULL)) {
			gchar *base64_encoded;

			if (data_name)
				*data_name = g_strdup (g_file_info_get_display_name (info));

			base64_encoded = g_base64_encode ((const guchar *) content, length);
			data = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);
			g_free (base64_encoded);
		}

		g_clear_object (&info);
		g_free (mime_type);
		g_free (content);
	}

	g_clear_object (&file);
	g_free (filename);

	return data;
}

GVariant *
e_editor_dom_get_inline_images_data (EEditorPage *editor_page,
                                     const gchar *uid_domain)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list = NULL;
	GVariant *result = NULL;
	GVariantBuilder *builder = NULL;
	GHashTable *added = NULL;
	gint length, ii;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	list = webkit_dom_document_query_selector_all (document, "img[src]", NULL);

	length = webkit_dom_node_list_get_length (list);
	if (length == 0) {
		g_clear_object (&list);
		goto background;
	}

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a(sss)"));

	added = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	for (ii = length; ii--;) {
		const gchar *id;
		gchar *cid = NULL;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "src");

		if (!src)
			continue;

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
		} else if (g_ascii_strncasecmp (src, "data:", 5) == 0) {
			gchar *data_name = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-name");

			if (data_name) {
				gchar *new_id;

				new_id = camel_header_msgid_generate (uid_domain);
				g_variant_builder_add (
					builder, "(sss)", src, data_name, new_id);
				cid = g_strdup_printf ("cid:%s", new_id);

				g_hash_table_insert (added, g_strdup (src), new_id);

				webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "data-inline", "", NULL);
			}
			g_free (data_name);
		} else if (g_ascii_strncasecmp (src, "file://", 7) == 0) {
			gchar *data, *data_name = NULL;

			data = encode_to_base64_data (src, &data_name);

			if (data && data_name) {
				gchar *new_id;

				new_id = camel_header_msgid_generate (uid_domain);
				g_variant_builder_add (builder, "(sss)", data, data_name, new_id);
				cid = g_strdup_printf ("cid:%s", new_id);

				g_hash_table_insert (added, data, new_id);

				webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "data-name", data_name, NULL);
				webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "data-inline", "", NULL);
			} else {
				g_free (data);
			}

			g_free (data_name);
		}

		if (cid) {
			webkit_dom_element_set_attribute (WEBKIT_DOM_ELEMENT (node), "src", cid, NULL);
			g_free (cid);
		}

		g_free (src);
	}
	g_clear_object (&list);

 background:
	list = webkit_dom_document_query_selector_all (
		document, "[data-inline][background]", NULL);
	length = webkit_dom_node_list_get_length (list);
	if (length == 0)
		goto out;
	if (!builder)
		builder = g_variant_builder_new (G_VARIANT_TYPE ("a(sss)"));
	if (!added)
		added = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	for (ii = length; ii--;) {
		const gchar *id;
		gchar *cid = NULL;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		gchar *src = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (node), "background");

		if (!src)
			continue;

		if ((id = g_hash_table_lookup (added, src)) != NULL) {
			cid = g_strdup_printf ("cid:%s", id);
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			g_free (src);
		} else {
			gchar *data_name = webkit_dom_element_get_attribute (
				WEBKIT_DOM_ELEMENT (node), "data-name");

			if (data_name) {
				gchar *new_id;

				new_id = camel_header_msgid_generate (uid_domain);
				g_variant_builder_add (
					builder, "(sss)", src, data_name, new_id);
				cid = g_strdup_printf ("cid:%s", new_id);

				g_hash_table_insert (added, src, new_id);

				webkit_dom_element_set_attribute (
					WEBKIT_DOM_ELEMENT (node), "background", cid, NULL);
			}
			g_free (data_name);
		}
		g_free (cid);
	}
 out:
	g_clear_object (&list);
	if (added)
		g_hash_table_destroy (added);

	if (builder) {
		result = g_variant_new ("a(sss)", builder);
		g_variant_builder_unref (builder);
	}

	return result;
}

static gboolean
pasting_quoted_content (const gchar *content)
{
	/* Check if the content we are pasting is a quoted content from composer.
	 * If it is, we can't use WebKit to paste it as it would leave the formatting
	 * on the content. */
	return g_str_has_prefix (
		content,
		"<meta http-equiv=\"content-type\" content=\"text/html; "
		"charset=utf-8\"><blockquote type=\"cite\"") &&
		strstr (content, "\"-x-evo-");
}

static void
remove_apple_interchange_newline_elements (WebKitDOMDocument *document)
{
	gint ii;
	WebKitDOMHTMLCollection *collection = NULL;

	collection = webkit_dom_document_get_elements_by_class_name_as_html_collection (
		document, "Apple-interchange-newline");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;) {
		WebKitDOMNode *node = webkit_dom_html_collection_item (collection, ii);

		remove_node (node);
	}
	g_clear_object (&collection);
}

/*
 * e_editor_dom_insert_html:
 * @selection: an #EEditorSelection
 * @html_text: an HTML code to insert
 *
 * Insert @html_text into document at current cursor position. When a text range
 * is selected, it will be replaced by @html_text.
 */
void
e_editor_dom_insert_html (EEditorPage *editor_page,
                          const gchar *html_text)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean html_mode, undo_redo_in_progress;
	WebKitDOMDocument *document;
	WebKitDOMNode *block = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (html_text != NULL);

	document = e_editor_page_get_document (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	undo_redo_in_progress = e_editor_undo_redo_manager_is_operation_in_progress (manager);
	if (!undo_redo_in_progress) {
		gboolean collapsed;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INSERT_HTML;

		collapsed = e_editor_dom_selection_is_collapsed (editor_page);
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		if (!collapsed) {
			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}

		ev->data.string.from = NULL;
		ev->data.string.to = g_strdup (html_text);
	}

	html_mode = e_editor_page_get_html_mode (editor_page);
	if (html_mode ||
	    (e_editor_page_is_pasting_content_from_itself (editor_page) &&
	    !pasting_quoted_content (html_text))) {
		if (!e_editor_dom_selection_is_collapsed (editor_page)) {
			EEditorHistoryEvent *event;
			WebKitDOMDocumentFragment *fragment;
			WebKitDOMRange *range = NULL;

			event = g_new0 (EEditorHistoryEvent, 1);
			event->type = HISTORY_DELETE;

			range = e_editor_dom_get_current_range (editor_page);
			fragment = webkit_dom_range_clone_contents (range, NULL);
			g_clear_object (&range);
			event->data.fragment = g_object_ref (fragment);

			e_editor_dom_selection_get_coordinates (editor_page,
				&event->before.start.x,
				&event->before.start.y,
				&event->before.end.x,
				&event->before.end.y);

			event->after.start.x = event->before.start.x;
			event->after.start.y = event->before.start.y;
			event->after.end.x = event->before.start.x;
			event->after.end.y = event->before.start.y;

			e_editor_undo_redo_manager_insert_history_event (manager, event);

			event = g_new0 (EEditorHistoryEvent, 1);
			event->type = HISTORY_AND;

			e_editor_undo_redo_manager_insert_history_event (manager, event);
		} else {
			WebKitDOMElement *selection_marker;

			e_editor_dom_selection_save (editor_page);

			/* If current block contains just the BR element, remove
			 * it otherwise WebKit will create a new block (with
			 * text node that will contain '\n') on the end of inserted
			 * content. Also remember the block and remove it if it's
			 * empty after we insert the content. */
			selection_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");

			if (!e_editor_page_is_pasting_content_from_itself (editor_page)) {
				if (!webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_marker))) {
					WebKitDOMNode *sibling;

					sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_marker));
					sibling = webkit_dom_node_get_next_sibling (sibling);
					if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling))
						remove_node (sibling);
				}
			}
			block = e_editor_dom_get_parent_block_node_from_child (WEBKIT_DOM_NODE (selection_marker));

			e_editor_dom_selection_restore (editor_page);
		}

		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_HTML, html_text);

		if (block)
			remove_node_if_empty (block);

		e_editor_dom_fix_file_uri_images (editor_page);

		if (strstr (html_text, "id=\"-x-evo-selection-start-marker\""))
			e_editor_dom_selection_restore (editor_page);

		e_editor_dom_check_magic_links (editor_page, FALSE);
		e_editor_dom_scroll_to_caret (editor_page);
		e_editor_dom_force_spell_check_in_viewport (editor_page);
	} else {
		/* Don't save history in the underlying function. */
		if (!undo_redo_in_progress)
			e_editor_undo_redo_manager_set_operation_in_progress (manager, TRUE);
		e_editor_dom_convert_and_insert_html_into_selection (editor_page, html_text, TRUE);
		if (!undo_redo_in_progress)
			e_editor_undo_redo_manager_set_operation_in_progress (manager, FALSE);
	}

	remove_apple_interchange_newline_elements (document);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

static void
save_history_for_delete_or_backspace (EEditorPage *editor_page,
                                      gboolean delete_key,
                                      gboolean control_key)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection)) {
		g_clear_object (&dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	/* Check if we can delete something */
	if (webkit_dom_range_get_collapsed (range, NULL)) {
		WebKitDOMRange *tmp_range = NULL;

		webkit_dom_dom_selection_modify (
			dom_selection, "move", delete_key ? "right" : "left", "character");

		tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (webkit_dom_range_compare_boundary_points (tmp_range, WEBKIT_DOM_RANGE_END_TO_END, range, NULL) == 0) {
			g_clear_object (&dom_selection);
			g_clear_object (&range);
			g_clear_object (&tmp_range);

			return;
		}

		webkit_dom_dom_selection_modify (
			dom_selection, "move", delete_key ? "left" : "right", "character");

		g_clear_object (&tmp_range);
	}

	if (save_history_before_event_in_table (editor_page, range)) {
		g_clear_object (&range);
		g_clear_object (&dom_selection);
		return;
	}

	ev = g_new0 (EEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	e_editor_dom_selection_save (editor_page);

	e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
	g_clear_object (&range);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		gboolean removing_from_anchor = FALSE;
		WebKitDOMRange *range_clone = NULL;
		WebKitDOMNode *node, *next_block = NULL;

		e_editor_page_block_selection_changed (editor_page);

		range_clone = webkit_dom_range_clone_range (range, NULL);
		if (control_key) {
			WebKitDOMRange *tmp_range = NULL;

			/* Control + Delete/Backspace deletes previous/next word. */
			webkit_dom_dom_selection_modify (
				dom_selection, "move", delete_key ? "right" : "left", "word");
			tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
			if (delete_key)
				webkit_dom_range_set_end (
					range_clone,
					webkit_dom_range_get_end_container (tmp_range, NULL),
					webkit_dom_range_get_end_offset (tmp_range, NULL),
					NULL);
			else
				webkit_dom_range_set_start (
					range_clone,
					webkit_dom_range_get_start_container (tmp_range, NULL),
					webkit_dom_range_get_start_offset (tmp_range, NULL),
					NULL);
			g_clear_object (&tmp_range);
		} else {
			typedef WebKitDOMNode * (*GetSibling)(WebKitDOMNode *node);
			WebKitDOMNode *container, *sibling;
			WebKitDOMElement *selection_marker;

			GetSibling get_sibling = delete_key ?
				webkit_dom_node_get_next_sibling :
				webkit_dom_node_get_previous_sibling;

			container = webkit_dom_range_get_end_container (range_clone, NULL);
			sibling = get_sibling (container);

			selection_marker = webkit_dom_document_get_element_by_id (
				document,
				delete_key ?
					"-x-evo-selection-end-marker" :
					"-x-evo-selection-start-marker");

			if (selection_marker) {
				WebKitDOMNode *tmp_sibling;

				tmp_sibling = get_sibling (WEBKIT_DOM_NODE (selection_marker));
				if (!tmp_sibling || (WEBKIT_DOM_IS_HTML_BR_ELEMENT (tmp_sibling) &&
				    !element_has_class (WEBKIT_DOM_ELEMENT (tmp_sibling), "-x-evo-wrap-br")))
					sibling = WEBKIT_DOM_NODE (selection_marker);
			}

			if (e_editor_dom_is_selection_position_node (sibling)) {
				if ((node = get_sibling (sibling)))
					node = get_sibling (node);
				if (node) {
					if (WEBKIT_DOM_IS_ELEMENT (node) &&
					    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node), "data-hidden-space")) {
						fragment = webkit_dom_document_create_document_fragment (document);
						webkit_dom_node_append_child (
							WEBKIT_DOM_NODE (fragment),
							WEBKIT_DOM_NODE (
								webkit_dom_document_create_text_node (document, " ")),
							NULL);
					} else if (delete_key) {
						webkit_dom_range_set_start (
							range_clone, node, 0, NULL);
						webkit_dom_range_set_end (
							range_clone, node, 1, NULL);
					}
				} else {
					WebKitDOMRange *tmp_range = NULL, *actual_range = NULL;

					actual_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

					webkit_dom_dom_selection_modify (
						dom_selection, "move", delete_key ? "right" : "left", "character");

					tmp_range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
					if (webkit_dom_range_compare_boundary_points (tmp_range, WEBKIT_DOM_RANGE_END_TO_END, actual_range, NULL) != 0) {
						WebKitDOMNode *actual_block;
						WebKitDOMNode *tmp_block;

						actual_block = e_editor_dom_get_parent_block_node_from_child (container);

						tmp_block = delete_key ?
							webkit_dom_range_get_end_container (tmp_range, NULL) :
							webkit_dom_range_get_start_container (tmp_range, NULL);
						tmp_block = e_editor_dom_get_parent_block_node_from_child (tmp_block);

						webkit_dom_dom_selection_remove_all_ranges (dom_selection);
						webkit_dom_dom_selection_add_range (dom_selection, actual_range);

						if (tmp_block) {
							if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (actual_block))
								actual_block = webkit_dom_node_get_parent_node (actual_block);

							fragment = webkit_dom_document_create_document_fragment (document);
							if (delete_key) {
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node_with_error (actual_block, TRUE, NULL),
									NULL);
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node_with_error (tmp_block, TRUE, NULL),
									NULL);
								if (delete_key)
									next_block = tmp_block;
							} else {
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node_with_error (tmp_block, TRUE, NULL),
									NULL);
								webkit_dom_node_append_child (
									WEBKIT_DOM_NODE (fragment),
									webkit_dom_node_clone_node_with_error (actual_block, TRUE, NULL),
									NULL);
							}
							g_object_set_data (
								G_OBJECT (fragment),
								"history-concatenating-blocks",
								GINT_TO_POINTER (1));
						}
					} else {
						webkit_dom_dom_selection_remove_all_ranges (dom_selection);
						webkit_dom_dom_selection_add_range (dom_selection, actual_range);
					}
					g_clear_object (&tmp_range);
					g_clear_object (&actual_range);
				}
			} else {
				glong offset;

				/* FIXME This code is wrong for unicode smileys. */
				offset = webkit_dom_range_get_start_offset (range_clone, NULL);

				if (delete_key)
					webkit_dom_range_set_end (
						range_clone, container, offset + 1, NULL);
				else
					webkit_dom_range_set_start (
						range_clone, container, offset - 1, NULL);

				removing_from_anchor = WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (
					webkit_dom_node_get_parent_node (container));
			}
		}


		if (!fragment)
			fragment = webkit_dom_range_clone_contents (range_clone, NULL);
		if (removing_from_anchor)
			g_object_set_data (
				G_OBJECT (fragment),
				"history-removing-from-anchor",
				GINT_TO_POINTER (1));
		node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
		if (!node) {
			g_free (ev);
			e_editor_page_unblock_selection_changed (editor_page);
			g_clear_object (&range);
			g_clear_object (&range_clone);
			g_clear_object (&dom_selection);
			g_warning ("History event was not saved for %s key", delete_key ? "Delete" : "Backspace");
			e_editor_dom_selection_restore (editor_page);
			return;
		}

		if (control_key) {
			if (delete_key) {
				ev->after.start.x = ev->before.start.x;
				ev->after.start.y = ev->before.start.y;
				ev->after.end.x = ev->before.end.x;
				ev->after.end.y = ev->before.end.y;

				webkit_dom_range_collapse (range_clone, TRUE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range_clone);
			} else {
				gboolean selection_saved = FALSE;
				WebKitDOMRange *tmp_range = NULL;

				if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
					selection_saved = TRUE;

				if (selection_saved)
					e_editor_dom_selection_restore (editor_page);

				tmp_range = webkit_dom_range_clone_range (range_clone, NULL);
				/* Prepare the selection to the right position after
				 * delete and save it. */
				webkit_dom_range_collapse (range_clone, TRUE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, range_clone);
				e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				/* Restore the selection where it was before the
				 * history event was saved. */
				webkit_dom_range_collapse (tmp_range, FALSE, NULL);
				webkit_dom_dom_selection_remove_all_ranges (dom_selection);
				webkit_dom_dom_selection_add_range (dom_selection, tmp_range);
				g_clear_object (&tmp_range);

				if (selection_saved)
					e_editor_dom_selection_save (editor_page);
			}
		} else {
			gboolean selection_saved = FALSE;

			if (webkit_dom_document_get_element_by_id (document, "-x-evo-selection-start-marker"))
				selection_saved = TRUE;

			if (selection_saved)
				e_editor_dom_selection_restore (editor_page);

			if (delete_key) {
				e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
			} else {
				webkit_dom_dom_selection_modify (dom_selection, "move", "left", "character");
				e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
				webkit_dom_dom_selection_modify (dom_selection, "move", "right", "character");

				ev->after.end.x = ev->after.start.x;
				ev->after.end.y = ev->after.start.y;
			}

			if (selection_saved)
				e_editor_dom_selection_save (editor_page);
		}

		g_clear_object (&range_clone);

		if (delete_key) {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, FALSE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
				webkit_dom_node_insert_before (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, TRUE)),
					webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
					NULL);
			}
		} else {
			if (!WEBKIT_DOM_IS_ELEMENT (node)) {
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, TRUE)),
					NULL);
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					WEBKIT_DOM_NODE (
						dom_create_selection_marker (document, FALSE)),
					NULL);
			}
		}

		/* If concatenating two blocks with pressing Delete on the end
		 * of the previous one and the next node contain content that
		 * is wrapped on multiple lines, the last line will by separated
		 * by WebKit to the separate block. To avoid it let's remove
		 * all quoting and wrapping from the next paragraph. */
		if (next_block) {
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (next_block));
			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (next_block));
		}

		e_editor_page_unblock_selection_changed (editor_page);
	} else {
		WebKitDOMElement *tmp_element;
		WebKitDOMNode *sibling;

		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.start.x;
		ev->after.end.y = ev->before.start.y;

		fragment = webkit_dom_range_clone_contents (range, NULL);

		tmp_element = webkit_dom_document_fragment_query_selector (
			fragment, "#-x-evo-selection-start-marker", NULL);
		if (tmp_element)
			remove_node (WEBKIT_DOM_NODE (tmp_element));

		tmp_element = webkit_dom_document_fragment_query_selector (
			fragment, "#-x-evo-selection-end-marker", NULL);
		if (tmp_element)
			remove_node (WEBKIT_DOM_NODE (tmp_element));

		remove_empty_blocks (document);

		/* Selection starts in the beginning of blockquote. */
		tmp_element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (tmp_element));
		if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
		    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-quoted")) {
			WebKitDOMNode *child;

			tmp_element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			/* If there is no text after the selection end it means that
			 * the block will be replaced with block that is body's descendant
			 * and not the blockquote's one. Also if the selection started
			 * in the beginning of blockquote we have to insert the quote
			 * characters into the deleted content to correctly restore
			 * them during undo/redo operations. */
			if (!(tmp_element && webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (tmp_element)))) {
				child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
				while (child && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (child))
					child = webkit_dom_node_get_first_child (child);

				child = webkit_dom_node_get_first_child (child);
				if (child && (WEBKIT_DOM_IS_TEXT (child) ||
				    (WEBKIT_DOM_IS_ELEMENT (child) &&
				     !element_has_class (WEBKIT_DOM_ELEMENT (child), "-x-evo-quoted")))) {
					webkit_dom_node_insert_before (
						webkit_dom_node_get_parent_node (child),
						webkit_dom_node_clone_node_with_error (sibling, TRUE, NULL),
						child,
						NULL);
				}
			}
		}

		/* When we were cloning the range above and the range contained
		 * quoted content there will still be blockquote missing in the
		 * final range. Let's modify the fragment and add it there. */
		tmp_element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (tmp_element) {
			WebKitDOMNode *node;

			node = WEBKIT_DOM_NODE (tmp_element);
			while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (node)))
				node = webkit_dom_node_get_parent_node (node);

			if (node && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node)) {
				WebKitDOMNode *last_child;

				last_child = webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (fragment));

				if (last_child && !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child)) {
					WebKitDOMDocumentFragment *tmp_fragment;
					WebKitDOMNode *clone;

					tmp_fragment = webkit_dom_document_create_document_fragment (document);
					clone = webkit_dom_node_clone_node_with_error (node, FALSE, NULL);
					clone = webkit_dom_node_append_child (
						WEBKIT_DOM_NODE (tmp_fragment), clone, NULL);
					webkit_dom_node_append_child (clone, WEBKIT_DOM_NODE (fragment), NULL);
					fragment = tmp_fragment;
				}
			}
		}

		/* FIXME Ugly hack */
		/* If the deleted selection contained the signature (or at least its
		 * part) replace it with the unchanged signature to correctly perform
		 * undo operation. */
		tmp_element = webkit_dom_document_fragment_query_selector (fragment, ".-x-evo-signature-wrapper", NULL);
		if (tmp_element) {
			WebKitDOMElement *signature;

			signature = webkit_dom_document_query_selector (document, ".-x-evo-signature-wrapper", NULL);
			if (signature) {
				webkit_dom_node_replace_child (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp_element)),
					webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (signature), TRUE, NULL),
					WEBKIT_DOM_NODE (tmp_element),
					NULL);
			}
		}
	}

	g_clear_object (&range);
	g_clear_object (&dom_selection);

	g_object_set_data (G_OBJECT (fragment), "history-delete-key", GINT_TO_POINTER (delete_key));
	g_object_set_data (G_OBJECT (fragment), "history-control-key", GINT_TO_POINTER (control_key));

	ev->data.fragment = g_object_ref (fragment);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	e_editor_undo_redo_manager_insert_history_event (manager, ev);

	e_editor_dom_selection_restore (editor_page);
}

gboolean
e_editor_dom_fix_structure_after_delete_before_quoted_content (EEditorPage *editor_page,
                                                               glong key_code,
                                                               gboolean control_key,
                                                               gboolean delete_key)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *node;
	gboolean collapsed = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	collapsed = e_editor_dom_selection_is_collapsed (editor_page);

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return FALSE;

	if (collapsed) {
		WebKitDOMNode *next_block;

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Next block is quoted content */
		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (next_block))
			goto restore;

		/* Delete was pressed in block without any content */
		if (webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker)))
			goto restore;

		/* If there is just BR element go ahead */
		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end_marker));
		if (node && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (node))
			goto restore;
		else {
			if (key_code != ~0) {
				e_editor_dom_selection_restore (editor_page);
				save_history_for_delete_or_backspace (
					editor_page, key_code == HTML_KEY_CODE_DELETE, control_key);
			} else
				e_editor_dom_selection_restore (editor_page);

			/* Remove the empty block and move caret to the right place. */
			remove_node (block);

			if (delete_key) {
				/* To the beginning of the next block. */
				e_editor_dom_move_caret_into_element (editor_page, WEBKIT_DOM_ELEMENT (next_block), TRUE);
			} else {
				WebKitDOMNode *prev_block;

				/* On the end of previous block. */
				prev_block = webkit_dom_node_get_previous_sibling (next_block);
				while (prev_block && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block))
					prev_block = webkit_dom_node_get_last_child (prev_block);

				if (prev_block)
					e_editor_dom_move_caret_into_element (editor_page, WEBKIT_DOM_ELEMENT (prev_block), FALSE);
			}

			return TRUE;
		}
	} else {
		WebKitDOMNode *end_block, *parent;

		/* Let the quote marks be selectable to nearly correctly remove the
		 * selection. Corrections after are done in body_keyup_event_cb. */
		enable_quote_marks_select (document);

		parent = webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker));
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
		    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
			node = webkit_dom_node_get_previous_sibling (parent);
		else
			node = webkit_dom_node_get_previous_sibling (
				WEBKIT_DOM_NODE (selection_start_marker));

		if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
			goto restore;

		if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-quoted"))
			goto restore;

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		end_block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_end_marker));

		/* Situation where the start of the selection is in the beginning
		+ * of the block in quoted content and the end in the beginning of
		+ * content that is after the citation or the selection end is in
		+ * the end of the quoted content (showed by ^). We have to
		+ * mark the start block to correctly restore the structure
		+ * afterwards.
		*
		* > |xxx
		* > xxx^
		* |xxx
		*/
		if (e_editor_dom_get_citation_level (end_block, FALSE) > 0) {
			WebKitDOMNode *parent;

			if (webkit_dom_node_get_next_sibling (end_block))
				goto restore;

			parent = webkit_dom_node_get_parent_node (end_block);
			while (parent && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent)) {
				WebKitDOMNode *next_parent = webkit_dom_node_get_parent_node (parent);

				if (webkit_dom_node_get_next_sibling (parent) &&
				    !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (next_parent))
					goto restore;

				parent = next_parent;
			}
		}
	}

 restore:
	e_editor_dom_selection_restore (editor_page);

	if (key_code != ~0)
		save_history_for_delete_or_backspace (
			editor_page, key_code == HTML_KEY_CODE_DELETE, control_key);

	return FALSE;
}

static gboolean
split_citation (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		WebKitDOMElement *selection_end;
		WebKitDOMNode *sibling;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_CITATION_SPLIT;

		e_editor_dom_selection_save (editor_page);

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

		if (!e_editor_dom_selection_is_collapsed (editor_page)) {
			WebKitDOMRange *range = NULL;

			range = e_editor_dom_get_current_range (editor_page);
			insert_delete_event (editor_page, range);

			g_clear_object (&range);

			ev->before.end.x = ev->before.start.x;
			ev->before.end.y = ev->before.start.y;
		}

		selection_end = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_end));
		if (!sibling || (WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
		    !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br"))) {
			WebKitDOMDocumentFragment *fragment;

			fragment = webkit_dom_document_create_document_fragment (document);
			ev->data.fragment = g_object_ref (fragment);
		} else
			ev->data.fragment = NULL;

		e_editor_dom_selection_restore (editor_page);
	}

	element = e_editor_dom_insert_new_line_into_citation (editor_page, "");

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	return element != NULL;
}

static gboolean
delete_last_character_from_previous_line_in_quoted_block (EEditorPage *editor_page,
                                                          glong key_code,
                                                          guint state)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *beginning, *prev_sibling;
	EEditorHistoryEvent *ev = NULL;
	gboolean hidden_space = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	/* We have to be in quoted content. */
	if (!e_editor_dom_selection_is_citation (editor_page))
		return FALSE;

	/* Selection is just caret. */
	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return FALSE;

	document = e_editor_page_get_document (editor_page);

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	/* Before the caret are just quote characters */
	beginning = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (!(beginning && WEBKIT_DOM_IS_ELEMENT (beginning))) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
		if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
			beginning = webkit_dom_node_get_previous_sibling (parent);
		else
			goto out;
	}

	/* Before the text is the beginning of line. */
	if (!(element_has_class (WEBKIT_DOM_ELEMENT (beginning), "-x-evo-quoted")))
		goto out;

	/* If we are just on the beginning of the line and not on the beginning of
	 * the block we need to remove the last character ourselves as well, otherwise
	 * WebKit will put the caret to wrong position. */
	if (!(prev_sibling = webkit_dom_node_get_previous_sibling (beginning)))
		goto out;

	if (key_code != ~0) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);
	}

	if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling)) {
		if (key_code != ~0)
			webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), prev_sibling, NULL);
		else
			remove_node (prev_sibling);
	}

	prev_sibling = webkit_dom_node_get_previous_sibling (beginning);
	if (WEBKIT_DOM_IS_ELEMENT (prev_sibling) &&
	    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (prev_sibling), "data-hidden-space")) {
		hidden_space = TRUE;
		if (key_code != ~0)
			webkit_dom_node_insert_before (
				WEBKIT_DOM_NODE (fragment),
				prev_sibling,
				webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment)),
				NULL);
		else
			remove_node (prev_sibling);
	}

	node = webkit_dom_node_get_previous_sibling (beginning);

	if (key_code != ~0)
		webkit_dom_node_append_child (WEBKIT_DOM_NODE (fragment), beginning, NULL);
	else
		remove_node (beginning);

	if (!hidden_space) {
		if (key_code != ~0) {
			gchar *data;

			data = webkit_dom_character_data_substring_data (
				WEBKIT_DOM_CHARACTER_DATA (node),
				webkit_dom_character_data_get_length (
					WEBKIT_DOM_CHARACTER_DATA (node)) -1,
				1,
				NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				WEBKIT_DOM_NODE (
					webkit_dom_document_create_text_node (document, data)),
				NULL);

			g_free (data);
		}

		webkit_dom_character_data_delete_data (
			WEBKIT_DOM_CHARACTER_DATA (node),
			webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (node)) -1,
			1,
			NULL);
	}

	if (key_code != ~0) {
		EEditorUndoRedoManager *manager;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		ev->data.fragment = g_object_ref (fragment);

		manager = e_editor_page_get_undo_redo_manager (editor_page);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);

	return TRUE;
 out:
	e_editor_dom_selection_restore (editor_page);

	return FALSE;
}

gboolean
e_editor_dom_delete_last_character_on_line_in_quoted_block (EEditorPage *editor_page,
                                                            glong key_code,
                                                            gboolean control_key)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *node, *beginning, *next_sibling;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	/* We have to be in quoted content. */
	if (!e_editor_dom_selection_is_citation (editor_page))
		return FALSE;

	/* Selection is just caret. */
	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return FALSE;

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	/* selection end marker */
	node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

	/* We have to be on the end of line. */
	next_sibling = webkit_dom_node_get_next_sibling (node);
	if (next_sibling &&
	    (!WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling) ||
	     webkit_dom_node_get_next_sibling (next_sibling)))
		goto out;

	/* Before the caret is just text. */
	node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
	if (!(node && WEBKIT_DOM_IS_TEXT (node)))
		goto out;

	/* There is just one character. */
	if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)) != 1)
		goto out;

	beginning = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (node));
	if (!(beginning && WEBKIT_DOM_IS_ELEMENT (beginning)))
		goto out;

	/* Before the text is the beginning of line. */
	if (!(element_has_class (WEBKIT_DOM_ELEMENT (beginning), "-x-evo-quoted")))
		goto out;

	if (!webkit_dom_node_get_previous_sibling (beginning))
		goto out;

	if (key_code != ~0) {
		e_editor_dom_selection_restore (editor_page);
		save_history_for_delete_or_backspace (
			editor_page, key_code == HTML_KEY_CODE_DELETE, control_key);
		e_editor_dom_selection_save (editor_page);
	}

	element = webkit_dom_node_get_parent_element (beginning);
	remove_node (WEBKIT_DOM_NODE (element));

	success = TRUE;
 out:
	e_editor_dom_selection_restore (editor_page);

	if (success)
		e_editor_dom_insert_new_line_into_citation (editor_page, NULL);

	return success;
}

static gboolean
selection_is_in_empty_list_item (WebKitDOMNode *selection_start_marker)
{
	gchar *text;
	WebKitDOMNode *sibling;

	/* Selection needs to be collapsed. */
	sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_start_marker));
	if (!e_editor_dom_is_selection_position_node (sibling))
		return FALSE;

	/* After the selection end there could be just the BR element. */
	sibling = webkit_dom_node_get_next_sibling (sibling);
	if (sibling && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling))
	       return FALSE;

	if (sibling && webkit_dom_node_get_next_sibling (sibling))
		return FALSE;

	sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));

	if (!sibling)
		return TRUE;

	/* Only text node with the zero width space character is allowed. */
	if (!WEBKIT_DOM_IS_TEXT (sibling))
		return FALSE;

	if (webkit_dom_node_get_previous_sibling (sibling))
		return FALSE;

	if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (sibling)) != 1)
		return FALSE;

	text = webkit_dom_character_data_get_data (WEBKIT_DOM_CHARACTER_DATA (sibling));
	if (!(text && g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0)) {
		g_free (text);
		return FALSE;
	}

	g_free (text);

	return TRUE;
}

static gboolean
return_pressed_in_image_wrapper (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent, *block, *clone;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return FALSE;

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-resizable-wrapper")) {
		e_editor_dom_selection_restore (editor_page);
		return FALSE;
	}

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);

		g_object_set_data (
			G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));
	}

	block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	clone = webkit_dom_node_clone_node_with_error (block, FALSE, NULL);
	webkit_dom_node_append_child (
		clone, WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)), NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (block),
		clone,
		block,
		NULL);

	if (ev) {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node_with_error (clone, TRUE, NULL),
			NULL);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		ev->data.fragment = g_object_ref (fragment);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_page_emit_content_changed (editor_page);

	e_editor_dom_selection_restore (editor_page);

	return TRUE;
}

static gboolean
return_pressed_after_h_rule (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMElement *selection_marker;
	WebKitDOMNode *node, *block, *clone, *hr, *insert_before = NULL;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return FALSE;

	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");

		hr = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_marker));
		hr = webkit_dom_node_get_next_sibling (hr);
		node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (selection_marker));
		if (!node || !WEBKIT_DOM_IS_HTML_BR_ELEMENT (node) || !hr ||
		    !WEBKIT_DOM_IS_HTML_HR_ELEMENT (hr)) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		insert_before = webkit_dom_node_get_next_sibling (hr);
	} else {
		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		node = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_marker));
		hr = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_marker));
		if (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node) ||
		    !WEBKIT_DOM_IS_HTML_HR_ELEMENT (hr)) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		insert_before = WEBKIT_DOM_NODE (selection_marker);

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INPUT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);

		g_object_set_data (
			G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));
	}

	block = webkit_dom_node_get_previous_sibling (hr);

	clone = webkit_dom_node_clone_node_with_error (block, FALSE, NULL);

	webkit_dom_node_append_child (
		clone, WEBKIT_DOM_NODE (webkit_dom_document_create_element (document, "br", NULL)), NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (hr), clone, insert_before, NULL);

	dom_remove_selection_markers (document);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (clone),
		WEBKIT_DOM_NODE (
			dom_create_selection_marker (document, TRUE)),
		NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (clone),
		WEBKIT_DOM_NODE (
			dom_create_selection_marker (document, FALSE)),
		NULL);

	if (ev) {
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node_with_error (clone, TRUE, NULL),
			NULL);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		ev->data.fragment = g_object_ref (fragment);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_page_emit_content_changed (editor_page);

	e_editor_dom_selection_restore (editor_page);

	return TRUE;
}

gboolean
e_editor_dom_return_pressed_in_empty_list_item (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMNode *parent;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	if (!e_editor_dom_selection_is_collapsed (editor_page))
		return FALSE;

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	if (!WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent)) {
		e_editor_dom_selection_restore (editor_page);
		return FALSE;
	}

	if (selection_is_in_empty_list_item (WEBKIT_DOM_NODE (selection_start_marker))) {
		EEditorHistoryEvent *ev = NULL;
		EEditorUndoRedoManager *manager;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMElement *paragraph;
		WebKitDOMNode *list;

		manager = e_editor_page_get_undo_redo_manager (editor_page);

		if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
			ev = g_new0 (EEditorHistoryEvent, 1);
			ev->type = HISTORY_INPUT;

			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);

			g_object_set_data (
				G_OBJECT (fragment), "history-return-key", GINT_TO_POINTER (1));
		}

		list = split_list_into_two (parent, -1);

		if (ev) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				parent,
				NULL);
		} else {
			remove_node (parent);
		}

		paragraph = e_editor_dom_prepare_paragraph (editor_page, TRUE);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (list),
			WEBKIT_DOM_NODE (paragraph),
			list,
			NULL);

		remove_node_if_empty (list);

		if (ev) {
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = g_object_ref (fragment);

			e_editor_undo_redo_manager_insert_history_event (manager, ev);
		}

		e_editor_dom_selection_restore (editor_page);

		e_editor_page_emit_content_changed (editor_page);

		return TRUE;
	}

	e_editor_dom_selection_restore (editor_page);

	return FALSE;
}

static void
process_smiley_on_delete_or_backspace (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;
	WebKitDOMNode *parent;
	gboolean in_smiley = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (WEBKIT_DOM_IS_ELEMENT (parent) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text"))
		in_smiley = TRUE;
	else {
		if (e_editor_dom_selection_is_collapsed (editor_page)) {
			WebKitDOMNode *prev_sibling;

			prev_sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
			if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling)) {
				gchar *text = webkit_dom_character_data_get_data (
					WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

				if (g_strcmp0 (text, UNICODE_ZERO_WIDTH_SPACE) == 0) {
					WebKitDOMNode *prev_prev_sibling;

					prev_prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
					if (WEBKIT_DOM_IS_ELEMENT (prev_prev_sibling) &&
					    element_has_class (WEBKIT_DOM_ELEMENT (prev_prev_sibling), "-x-evo-smiley-wrapper")) {
						remove_node (prev_sibling);
						in_smiley = TRUE;
						parent = webkit_dom_node_get_last_child (prev_prev_sibling);
					}
				}

				g_free (text);
			}
		} else {
			element = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-end-marker");

			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
			if (WEBKIT_DOM_IS_ELEMENT (parent) &&
			    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-smiley-text"))
				in_smiley = TRUE;
		}
	}

	if (in_smiley) {
		WebKitDOMNode *wrapper;

		wrapper = webkit_dom_node_get_parent_node (parent);
		if (!e_editor_page_get_html_mode (editor_page)) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (parent)))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (wrapper),
					child,
					wrapper,
					NULL);
		}
		/* In the HTML mode the whole smiley will be removed. */
		remove_node (wrapper);
		/* FIXME history will be probably broken here */
	}

	e_editor_dom_selection_restore (editor_page);
}

gboolean
e_editor_dom_key_press_event_process_return_key (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *table = NULL;
	gboolean first_cell = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	/* Return pressed in the beginning of the first cell will insert
	 * new block before the table (and move the caret there) if none
	 * is already there, otherwise it will act as normal return. */
	if (selection_is_in_table (document, &first_cell, &table) && first_cell) {
		WebKitDOMNode *node;

		node = webkit_dom_node_get_previous_sibling (table);
		if (!node) {
			node = webkit_dom_node_get_next_sibling (table);
			node = webkit_dom_node_clone_node_with_error (node, FALSE, NULL);
			webkit_dom_node_append_child (
				node,
				WEBKIT_DOM_NODE (webkit_dom_document_create_element (
					document, "br", NULL)),
				NULL);
			dom_add_selection_markers_into_element_start (
				document, WEBKIT_DOM_ELEMENT (node), NULL, NULL);
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (table),
				node,
				table,
				NULL);
			e_editor_dom_selection_restore (editor_page);
			e_editor_page_emit_content_changed (editor_page);
			return TRUE;
		}
	}

	/* When user presses ENTER in a citation block, WebKit does
	 * not break the citation automatically, so we need to use
	 * the special command to do it. */
	if (e_editor_dom_selection_is_citation (editor_page)) {
		e_editor_dom_remove_input_event_listener_from_body (editor_page);
		if (split_citation (editor_page)) {
			e_editor_page_set_return_key_pressed (editor_page, TRUE);
			e_editor_dom_check_magic_links (editor_page, FALSE);
			e_editor_page_set_return_key_pressed (editor_page, FALSE);
			e_editor_page_emit_content_changed (editor_page);

			return TRUE;
		}
		return FALSE;
	}

	/* If the ENTER key is pressed inside an empty list item then the list
	 * is broken into two and empty paragraph is inserted between lists. */
	if (e_editor_dom_return_pressed_in_empty_list_item (editor_page))
		return TRUE;

	if (return_pressed_in_image_wrapper (editor_page))
		return TRUE;

	if (return_pressed_after_h_rule (editor_page))
		return TRUE;

	return FALSE;
}

static gboolean
remove_empty_bulleted_list_item (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start;
	WebKitDOMNode *parent;
	EEditorUndoRedoManager *manager;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start));
	while (parent && !node_is_list_or_item (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	if (!parent)
		goto out;

	if (selection_is_in_empty_list_item (WEBKIT_DOM_NODE (selection_start))) {
		EEditorHistoryEvent *ev = NULL;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *prev_item;

		prev_item = webkit_dom_node_get_previous_sibling (parent);

		if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
			/* Insert new history event for Return to have the right coordinates.
			 * The fragment will be added later. */
			ev = g_new0 (EEditorHistoryEvent, 1);
			ev->type = HISTORY_DELETE;

			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->before.start.x,
				&ev->before.start.y,
				&ev->before.end.x,
				&ev->before.end.y);

			fragment = webkit_dom_document_create_document_fragment (document);
		}

		if (ev) {
			if (prev_item)
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (fragment),
					webkit_dom_node_clone_node_with_error (prev_item, TRUE, NULL),
					NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				parent,
				NULL);
		} else
			remove_node (parent);

		if (prev_item)
			dom_add_selection_markers_into_element_end (
				document, WEBKIT_DOM_ELEMENT (prev_item), NULL, NULL);

		if (ev) {
			e_editor_dom_selection_get_coordinates (editor_page,
				&ev->after.start.x,
				&ev->after.start.y,
				&ev->after.end.x,
				&ev->after.end.y);

			ev->data.fragment = g_object_ref (fragment);

			e_editor_undo_redo_manager_insert_history_event (manager, ev);
		}

		e_editor_page_emit_content_changed (editor_page);
		e_editor_dom_selection_restore (editor_page);

		return TRUE;
	}
 out:
	e_editor_dom_selection_restore (editor_page);

	return FALSE;
}

gboolean
e_editor_dom_key_press_event_process_backspace_key (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	/* BackSpace pressed in the beginning of quoted content changes
	 * format to normal and inserts text into body */
	if (e_editor_dom_selection_is_collapsed (editor_page)) {
		e_editor_dom_selection_save (editor_page);
		if (e_editor_dom_move_quoted_block_level_up (editor_page) || delete_hidden_space (editor_page)) {
			e_editor_dom_selection_restore (editor_page);
			e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
			e_editor_page_emit_content_changed (editor_page);
			return TRUE;
		}
		e_editor_dom_selection_restore (editor_page);
	}

	/* BackSpace in indented block decrease indent level by one */
	if (e_editor_dom_selection_is_indented (editor_page) &&
	    e_editor_dom_selection_is_collapsed (editor_page)) {
		WebKitDOMDocument *document;
		WebKitDOMElement *selection_start;
		WebKitDOMNode *prev_sibling;

		document = e_editor_page_get_document (editor_page);

		e_editor_dom_selection_save (editor_page);
		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		/* Empty text node before caret */
		prev_sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start));
		if (prev_sibling && WEBKIT_DOM_IS_TEXT (prev_sibling))
			if (webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (prev_sibling)) == 0)
				prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

		e_editor_dom_selection_restore (editor_page);
		if (!prev_sibling) {
			e_editor_dom_selection_unindent (editor_page);
			e_editor_page_emit_content_changed (editor_page);
			return TRUE;
		}
	}

	/* BackSpace pressed in an empty item in the bulleted list removes it. */
	if (!e_editor_page_get_html_mode (editor_page) && e_editor_dom_selection_is_collapsed (editor_page) &&
	    remove_empty_bulleted_list_item (editor_page))
		return TRUE;


	if (prevent_from_deleting_last_element_in_body (e_editor_page_get_document (editor_page)))
		return TRUE;

	return FALSE;
}

gboolean
e_editor_dom_key_press_event_process_delete_or_backspace_key (EEditorPage *editor_page,
                                                              glong key_code,
                                                              gboolean control_key,
                                                              gboolean delete)
{
	WebKitDOMDocument *document;
	gboolean html_mode;
	gboolean local_delete;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);
	local_delete = (key_code == HTML_KEY_CODE_DELETE) || delete;

	if (e_editor_page_get_magic_smileys_enabled (editor_page)) {
		/* If deleting something in a smiley it won't be a smiley
		 * anymore (at least from Evolution' POV), so remove all
		 * the elements that are hidden in the wrapper and leave
		 * just the text. Also this ensures that when a smiley is
		 * recognized and we press the BackSpace key we won't delete
		 * the UNICODE_HIDDEN_SPACE, but we will correctly delete
		 * the last character of smiley. */
		process_smiley_on_delete_or_backspace (editor_page);
	}

	if (!local_delete && !html_mode &&
	    e_editor_dom_delete_last_character_on_line_in_quoted_block (editor_page, key_code, control_key))
		goto out;

	if (!local_delete && !html_mode &&
	    delete_last_character_from_previous_line_in_quoted_block (editor_page, key_code, control_key))
		goto out;

	if (e_editor_dom_fix_structure_after_delete_before_quoted_content (editor_page, key_code, control_key, FALSE))
		goto out;

	if (local_delete) {
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *sibling, *block, *next_block;

		/* This needs to be performed just in plain text mode
		 * and when the selection is collapsed. */
		if (html_mode || !e_editor_dom_selection_is_collapsed (editor_page))
			return FALSE;

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		sibling = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		/* Check if the key was pressed in the beginning of block. */
		if (!(sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
		      element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-quoted"))) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		sibling = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		sibling = webkit_dom_node_get_next_sibling (sibling);

		/* And also the current block was empty. */
		if (!(!sibling || (sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
		      !element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-wrap-br")))) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
		next_block = webkit_dom_node_get_next_sibling (block);

		remove_node (block);

		e_editor_dom_move_caret_into_element (editor_page, WEBKIT_DOM_ELEMENT (next_block), TRUE);

		goto out;
	} else {
		/* Concatenating a non-quoted block with Backspace key to the
		 * previous block that is inside a quoted content. */
		WebKitDOMElement *selection_start_marker;
		WebKitDOMNode *node, *block, *prev_block, *last_child, *child;

		if (html_mode || !e_editor_dom_selection_is_collapsed (editor_page) ||
		    e_editor_dom_selection_is_citation (editor_page))
			return FALSE;

		e_editor_dom_selection_save (editor_page);

		selection_start_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (selection_start_marker));
		if (node) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		remove_empty_blocks (document);

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

		prev_block = webkit_dom_node_get_previous_sibling (block);
		if (!prev_block || !WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (prev_block)) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		last_child = webkit_dom_node_get_last_child (prev_block);
		while (last_child && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (last_child))
			last_child = webkit_dom_node_get_last_child (last_child);

		if (!last_child) {
			e_editor_dom_selection_restore (editor_page);
			return FALSE;
		}

		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (last_child));
		e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (last_child));

		node = webkit_dom_node_get_last_child (last_child);
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (node))
			remove_node (node);

		while ((child = webkit_dom_node_get_first_child (block)))
			webkit_dom_node_append_child (last_child, child, NULL);

		remove_node (block);

		if (WEBKIT_DOM_IS_ELEMENT (last_child))
			e_editor_dom_wrap_and_quote_element (editor_page, WEBKIT_DOM_ELEMENT (last_child));

		e_editor_dom_selection_restore (editor_page);

		goto out;
	}

	return FALSE;
 out:
	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	e_editor_page_emit_content_changed (editor_page);

	return TRUE;
}

static gboolean
contains_forbidden_elements (WebKitDOMDocument *document)
{
	WebKitDOMElement *body, *element;

	body = WEBKIT_DOM_ELEMENT (webkit_dom_document_get_body (document));

	/* Try to find disallowed elements in the plain text mode */
	element = webkit_dom_element_query_selector (
		body,
		":not("
		/* Basic elements used as blocks allowed in the plain text mode */
		"[data-evo-paragraph], pre, ul, ol, li, blockquote[type=cite], "
		/* Other elements */
		"br, a, "
		/* Indented elements */
		".-x-evo-indented, "
		/* Signature */
		".-x-evo-signature-wrapper, .-x-evo-signature, "
		/* Smileys */
		".-x-evo-smiley-wrapper, .-x-evo-smiley-img, .-x-evo-smiley-text, "
		/* Selection markers */
		"#-x-evo-selection-start-marker, #-x-evo-selection-end-marker"
		")",
		NULL);

	if (element)
		return TRUE;

	/* Try to find disallowed elements relationship in the plain text */
	element = webkit_dom_element_query_selector (
		body,
		":not("
		/* Body descendants */
		"body > :matches(blockquote[type=cite], .-x-evo-signature-wrapper), "
		/* Main blocks and indented blocks */
		":matches(body, .-x-evo-indented) > :matches(pre, ul, ol, .-x-evo-indented, [data-evo-paragraph]), "
		/* Blockquote descendants */
		"blockquote[type=cite] > :matches(pre, [data-evo-paragraph], blockquote[type=cite]), "
		/* Block descendants */
		":matches(pre, [data-evo-paragraph], li) > :matches(br, span, a), "
		/* Lists */
		":matches(ul, ol) > :matches(ul, ol, li), "
		/* Smileys */
		".-x-evo-smiley-wrapper > :matches(.-x-evo-smiley-img, .-x-evo-smiley-text), "
		/* Signature */
		".-x-evo-signature-wrapper > .-x-evo-signature"
		")",
		NULL);

	return element ? TRUE : FALSE;
}

gboolean
e_editor_dom_check_if_conversion_needed (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	gboolean html_mode, convert = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	html_mode = e_editor_page_get_html_mode (editor_page);

	if (html_mode)
		convert = contains_forbidden_elements (document);

	return convert;
}

void
e_editor_dom_process_content_after_mode_change (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	gboolean html_mode;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	html_mode = e_editor_page_get_html_mode (editor_page);

	if (html_mode)
		process_content_to_html_changing_composer_mode (editor_page);
	else
		process_content_to_plain_text_changing_composer_mode (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	e_editor_undo_redo_manager_clean_history (manager);
}

guint
e_editor_dom_get_caret_offset (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *anchor;
	WebKitDOMRange *range = NULL;
	guint ret_val;
	gchar *text;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_clear_object (&dom_selection);
		return 0;
	}

	webkit_dom_dom_selection_collapse_to_start (dom_selection, NULL);
	/* Select the text from the current caret position to the beginning of the line. */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "left", "lineBoundary");

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	text = webkit_dom_range_to_string (range, NULL);
	ret_val = strlen (text);
	g_free (text);

	webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);

	/* In the plain text mode we need to increase the return value by 2 per
	 * citation level because of "> ". */
	if (!e_editor_page_get_html_mode (editor_page)) {
		WebKitDOMNode *parent = anchor;

		while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent))
				ret_val += 2;

			parent = webkit_dom_node_get_parent_node (parent);
		}
	}

	g_clear_object (&range);
	g_clear_object (&dom_selection);

	return ret_val;
}

guint
e_editor_dom_get_caret_position (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL, *range_clone = NULL;
	guint ret_val;
	gchar *text;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_clear_object (&dom_selection);
		return 0;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);

	body = webkit_dom_document_get_body (document);
	/* Select the text from the beginning of the body to the current caret. */
	webkit_dom_range_set_start_before (
		range_clone, webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body)), NULL);

	/* This is returning a text without new lines! */
	text = webkit_dom_range_to_string (range_clone, NULL);
	ret_val = strlen (text);
	g_free (text);

	g_clear_object (&range_clone);
	g_clear_object (&range);
	g_clear_object (&dom_selection);

	return ret_val;
}

static void
insert_nbsp_history_event (WebKitDOMDocument *document,
                           EEditorUndoRedoManager *manager,
                           gboolean delete,
                           guint x,
                           guint y)
{
	EEditorHistoryEvent *event;
	WebKitDOMDocumentFragment *fragment;

	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_AND;
	e_editor_undo_redo_manager_insert_history_event (manager, event);

	fragment = webkit_dom_document_create_document_fragment (document);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (fragment),
		WEBKIT_DOM_NODE (
			webkit_dom_document_create_text_node (document, UNICODE_NBSP)),
		NULL);

	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_DELETE;

	if (delete)
		g_object_set_data (G_OBJECT (fragment), "history-delete-key", GINT_TO_POINTER (1));

	event->data.fragment = fragment;

	event->before.start.x = x;
	event->before.start.y = y;
	event->before.end.x = x;
	event->before.end.y = y;

	event->after.start.x = x;
	event->after.start.y = y;
	event->after.end.x = x;
	event->after.end.y = y;

	e_editor_undo_redo_manager_insert_history_event (manager, event);
}
void
e_editor_dom_save_history_for_drag (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMRange *beginning_of_line = NULL;
	WebKitDOMRange *range = NULL, *range_clone = NULL;
	EEditorHistoryEvent *event;
	EEditorUndoRedoManager *manager;
	gboolean start_to_start = FALSE, end_to_end = FALSE;
	gchar *range_text;
	guint x, y;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!(dom_window = webkit_dom_document_get_default_view (document)))
		return;

	if (!(dom_selection = webkit_dom_dom_window_get_selection (dom_window))) {
		g_clear_object (&dom_window);
		return;
	}

	g_clear_object (&dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_clear_object (&dom_selection);
		return;
	}

	/* Obtain the dragged content. */
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	range_clone = webkit_dom_range_clone_range (range, NULL);

	/* Create the history event for the content that will
	 * be removed by DnD. */
	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_DELETE;

	e_editor_dom_selection_get_coordinates (editor_page,
		&event->before.start.x,
		&event->before.start.y,
		&event->before.end.x,
		&event->before.end.y);

	x = event->before.start.x;
	y = event->before.start.y;

	event->after.start.x = x;
	event->after.start.y = y;
	event->after.end.x = x;
	event->after.end.y = y;

	/* Save the content that will be removed. */
	fragment = webkit_dom_range_clone_contents (range_clone, NULL);

	/* Extend the cloned range to point one character after
	 * the selection ends to later check if there is a whitespace
	 * after it. */
	webkit_dom_range_set_end (
		range_clone,
		webkit_dom_range_get_end_container (range_clone, NULL),
		webkit_dom_range_get_end_offset (range_clone, NULL) + 1,
		NULL);
	range_text = webkit_dom_range_get_text (range_clone);

	/* Check if the current selection starts on the beginning of line. */
	webkit_dom_dom_selection_modify (
		dom_selection, "extend", "left", "lineboundary");
	beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	start_to_start = webkit_dom_range_compare_boundary_points (
		beginning_of_line, WEBKIT_DOM_RANGE_START_TO_START, range, NULL) == 0;

	/* Restore the selection to state before the check. */
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&beginning_of_line);

	/* Check if the current selection end on the end of the line. */
	webkit_dom_dom_selection_modify (
		dom_selection, "extend", "right", "lineboundary");
	beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	end_to_end = webkit_dom_range_compare_boundary_points (
		beginning_of_line, WEBKIT_DOM_RANGE_END_TO_END, range, NULL) == 0;

	/* Dragging the whole line. */
	if (start_to_start && end_to_end) {
		WebKitDOMNode *container, *actual_block, *tmp_block;

		/* Select the whole line (to the beginning of the next
		 * one so we can reuse the undo code while undoing this.
		 * Because of this we need to special mark the event
		 * with history-drag-and-drop to correct the selection
		 * after undoing it (otherwise the beginning of the next
		 * line will be selected as well. */
		webkit_dom_dom_selection_modify (
			dom_selection, "extend", "right", "character");
		g_clear_object (&beginning_of_line);
		beginning_of_line = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

		container = webkit_dom_range_get_end_container (range, NULL);
		actual_block = e_editor_dom_get_parent_block_node_from_child (container);

		tmp_block = webkit_dom_range_get_end_container (beginning_of_line, NULL);
		if ((tmp_block = e_editor_dom_get_parent_block_node_from_child (tmp_block))) {
			e_editor_dom_selection_get_coordinates (editor_page,
				&event->before.start.x,
				&event->before.start.y,
				&event->before.end.x,
				&event->before.end.y);

			/* Create the right content for the history event. */
			fragment = webkit_dom_document_create_document_fragment (document);
			/* The removed line. */
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node_with_error (actual_block, TRUE, NULL),
				NULL);
			/* The following block, but empty. */
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (fragment),
				webkit_dom_node_clone_node_with_error (tmp_block, FALSE, NULL),
				NULL);
			g_object_set_data (
				G_OBJECT (fragment),
				"history-drag-and-drop",
				GINT_TO_POINTER (1));
		}
	}
	/* It should act as a Delete key press. */
	g_object_set_data (G_OBJECT (fragment), "history-delete-key", GINT_TO_POINTER (1));

	event->data.fragment = fragment;
	e_editor_undo_redo_manager_insert_history_event (manager, event);

	/* WebKit removes the space (if presented) after selection and
	 * we need to create a new history event for it. */
	if (g_str_has_suffix (range_text, " ") ||
	    g_str_has_suffix (range_text, UNICODE_NBSP))
		insert_nbsp_history_event (document, manager, TRUE, x, y);
	else {
		/* If there is a space before the selection WebKit will remove
		 * it as well unless there is a space after the selection. */
		gchar *range_text_start;
		glong start_offset;

		start_offset = webkit_dom_range_get_start_offset (range_clone, NULL);
		webkit_dom_range_set_start (
			range_clone,
			webkit_dom_range_get_start_container (range_clone, NULL),
			start_offset > 0 ? start_offset - 1 : 0,
			NULL);

		range_text_start = webkit_dom_range_get_text (range_clone);
		if (g_str_has_prefix (range_text_start, " ") ||
		    g_str_has_prefix (range_text_start, UNICODE_NBSP)) {
			if (!end_to_end) {
				webkit_dom_dom_selection_collapse_to_start (dom_selection, NULL);
				webkit_dom_dom_selection_modify (
					dom_selection, "move", "backward", "character");
				e_editor_dom_selection_get_coordinates (editor_page, &x, &y, &x, &y);
			}
			insert_nbsp_history_event (document, manager, TRUE, x, y);
		}

		g_free (range_text_start);
	}

	g_free (range_text);

	/* Restore the selection to original state. */
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&beginning_of_line);

	/* All the things above were about removing the content,
	 * create an AND event to continue later with inserting
	 * the dropped content. */
	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_AND;
	e_editor_undo_redo_manager_insert_history_event (manager, event);

	g_clear_object (&dom_selection);

	g_clear_object (&range);
	g_clear_object (&range_clone);
}

void
e_editor_dom_save_history_for_drop (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMNodeList *list = NULL;
	WebKitDOMRange *range = NULL;
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *event;
	gint ii, length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);

	/* When the image is DnD inside the view WebKit removes the wrapper that
	 * is used for resizing the image, so we have to recreate it again. */
	list = webkit_dom_document_query_selector_all (document, ":not(span) > img[data-inline]", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMElement *element;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		element = webkit_dom_document_create_element (document, "span", NULL);
		webkit_dom_element_set_class_name (element, "-x-evo-resizable-wrapper");

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (element),
			node,
			NULL);

		webkit_dom_node_append_child (WEBKIT_DOM_NODE (element), node, NULL);
	}
	g_clear_object (&list);

	/* When the image is moved the new selection is created after after it, so
	 * lets collapse the selection to have the caret right after the image. */
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	event = g_new0 (EEditorHistoryEvent, 1);
	event->type = HISTORY_INSERT_HTML;

	/* Get the dropped content. It's easy as it is selected by WebKit. */
	fragment = webkit_dom_range_clone_contents (range, NULL);
	event->data.string.from = NULL;
	/* Get the HTML content of the dropped content. */
	event->data.string.to = dom_get_node_inner_html (WEBKIT_DOM_NODE (fragment));

	e_editor_undo_redo_manager_insert_history_event (manager, event);

	g_clear_object (&range);
	g_clear_object (&dom_selection);
}

static void
dom_set_link_color_in_document (EEditorPage *editor_page,
                                const gchar *color,
                                gboolean visited)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLHeadElement *head;
	WebKitDOMElement *style_element;
	WebKitDOMHTMLElement *body;
	gchar *color_str = NULL;
	const gchar *style_id;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (color != NULL);

	style_id = visited ? "-x-evo-a-color-style-visited" : "-x-evo-a-color-style";

	document = e_editor_page_get_document (editor_page);
	head = webkit_dom_document_get_head (document);
	body = webkit_dom_document_get_body (document);

	style_element = webkit_dom_document_get_element_by_id (document, style_id);
	if (!style_element) {
		style_element = webkit_dom_document_create_element (document, "style", NULL);
		webkit_dom_element_set_id (style_element, style_id);
		webkit_dom_element_set_attribute (style_element, "type", "text/css", NULL);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style_element), NULL);
	}

	color_str = g_strdup_printf (
		visited ? "a.-x-evo-visited-link { color: %s; }" : "a { color: %s; }", color);
	webkit_dom_element_set_inner_html (style_element, color_str, NULL);
	g_free (color_str);

	if (visited)
		webkit_dom_html_body_element_set_v_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);
	else
		webkit_dom_html_body_element_set_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);
}

void
e_editor_dom_set_link_color (EEditorPage *editor_page,
                             const gchar *color)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	dom_set_link_color_in_document (editor_page, color, FALSE);
}

void
e_editor_dom_set_visited_link_color (EEditorPage *editor_page,
                                     const gchar *color)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	dom_set_link_color_in_document (editor_page, color, TRUE);
}

void
e_editor_dom_fix_file_uri_images (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list = NULL;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	list = webkit_dom_document_query_selector_all (
		document, "img[src^=\"file://\"]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node;
		gchar *uri;

		node = webkit_dom_node_list_item (list, ii);
		uri = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "src");
		g_free (uri);
	}

	g_clear_object (&list);
}

/* ******************** Selection ******************** */

void
e_editor_dom_replace_base64_image_src (EEditorPage *editor_page,
                                       const gchar *selector,
                                       const gchar *base64_content,
                                       const gchar *filename,
                                       const gchar *uri)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	element = webkit_dom_document_query_selector (document, selector, NULL);

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (element))
		webkit_dom_html_image_element_set_src (
			WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
			base64_content);
	else
		webkit_dom_element_set_attribute (
			element, "background", base64_content, NULL);

	webkit_dom_element_set_attribute (element, "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (element, "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		element, "data-name", filename ? filename : "", NULL);
}

WebKitDOMRange *
e_editor_dom_get_current_range (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	if (!dom_window)
		return NULL;

	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	if (!WEBKIT_DOM_IS_DOM_SELECTION (dom_selection)) {
		g_clear_object (&dom_window);
		return NULL;
	}

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
		goto exit;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
 exit:
	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);

	return range;
}

void
e_editor_dom_move_caret_into_element (EEditorPage *editor_page,
                                      WebKitDOMElement *element,
                                      gboolean to_start)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!element)
		return;

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	range = webkit_dom_document_create_range (document);

	webkit_dom_range_select_node_contents (
		range, WEBKIT_DOM_NODE (element), NULL);
	webkit_dom_range_collapse (range, to_start, NULL);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	g_clear_object (&range);
	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
}

void
e_editor_dom_insert_base64_image (EEditorPage *editor_page,
                                  const gchar *base64_content,
                                  const gchar *filename,
                                  const gchar *uri)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *selection_start_marker, *resizable_wrapper;
	WebKitDOMText *text;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!e_editor_dom_selection_is_collapsed (editor_page)) {
		EEditorHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMRange *range = NULL;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_DELETE;

		range = e_editor_dom_get_current_range (editor_page);
		fragment = webkit_dom_range_clone_contents (range, NULL);
		g_clear_object (&range);
		ev->data.fragment = g_object_ref (fragment);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.start.x;
		ev->after.end.y = ev->before.start.y;

		e_editor_undo_redo_manager_insert_history_event (manager, ev);

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_AND;

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
		e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_DELETE, NULL);
	}

	e_editor_dom_selection_save (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_IMAGE;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	resizable_wrapper =
		webkit_dom_document_create_element (document, "span", NULL);
	webkit_dom_element_set_attribute (
		resizable_wrapper, "class", "-x-evo-resizable-wrapper", NULL);

	element = webkit_dom_document_create_element (document, "img", NULL);
	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (element),
		base64_content);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-uri", uri, NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-inline", "", NULL);
	webkit_dom_element_set_attribute (
		WEBKIT_DOM_ELEMENT (element), "data-name",
		filename ? filename : "", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (element),
		NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (resizable_wrapper),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	/* We have to again use UNICODE_ZERO_WIDTH_SPACE character to restore
	 * caret on right position */
	text = webkit_dom_document_create_text_node (
		document, UNICODE_ZERO_WIDTH_SPACE);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (selection_start_marker)),
		WEBKIT_DOM_NODE (text),
		WEBKIT_DOM_NODE (selection_start_marker),
		NULL);

	if (ev) {
		WebKitDOMDocumentFragment *fragment;
		WebKitDOMNode *node;

		fragment = webkit_dom_document_create_document_fragment (document);
		node = webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (resizable_wrapper), TRUE, NULL),
			NULL);

		webkit_dom_element_insert_adjacent_html (
			WEBKIT_DOM_ELEMENT (node), "afterend", "&#8203;", NULL);
		ev->data.fragment = g_object_ref (fragment);

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	e_editor_dom_scroll_to_caret (editor_page);
}

/* ************************ image_load_and_insert_async() ************************ */

typedef struct _ImageLoadContext {
	EEditorPage *editor_page;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFile *file;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	const gchar *content_type;
	const gchar *filename;
	const gchar *selector;
	gchar buffer[4096];
} ImageLoadContext;

/* Forward Declaration */
static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           ImageLoadContext *load_context);

static ImageLoadContext *
image_load_context_new (EEditorPage *editor_page)
{
	ImageLoadContext *load_context;

	load_context = g_slice_new0 (ImageLoadContext);
	load_context->editor_page = editor_page;

	return load_context;
}

static void
image_load_context_free (ImageLoadContext *load_context)
{
	if (load_context->input_stream != NULL)
		g_object_unref (load_context->input_stream);

	if (load_context->output_stream != NULL)
		g_object_unref (load_context->output_stream);

	if (load_context->file_info != NULL)
		g_object_unref (load_context->file_info);

	if (load_context->file != NULL)
		g_object_unref (load_context->file);

	g_slice_free (ImageLoadContext, load_context);
}

static void
image_load_finish (ImageLoadContext *load_context)
{
	EEditorPage *editor_page;
	GMemoryOutputStream *output_stream;
	const gchar *selector;
	gchar *base64_encoded, *mime_type, *output, *uri;
	gsize size;
	gpointer data;

	output_stream = G_MEMORY_OUTPUT_STREAM (load_context->output_stream);
	editor_page = load_context->editor_page;
	mime_type = g_content_type_get_mime_type (load_context->content_type);

	data = g_memory_output_stream_get_data (output_stream);
	size = g_memory_output_stream_get_data_size (output_stream);
	uri = g_file_get_uri (load_context->file);

	base64_encoded = g_base64_encode ((const guchar *) data, size);
	output = g_strconcat ("data:", mime_type, ";base64,", base64_encoded, NULL);
	selector = load_context->selector;
	if (selector && *selector)
		e_editor_dom_replace_base64_image_src (editor_page, selector, output, load_context->filename, uri);
	else
		e_editor_dom_insert_base64_image (editor_page, output, load_context->filename, uri);

	g_free (base64_encoded);
	g_free (output);
	g_free (mime_type);
	g_free (uri);

	image_load_context_free (load_context);
}

static void
image_load_write_cb (GOutputStream *output_stream,
                     GAsyncResult *result,
                     ImageLoadContext *load_context)
{
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	input_stream = load_context->input_stream;

	if (bytes_written < load_context->bytes_read) {
		g_memmove (
			load_context->buffer,
			load_context->buffer + bytes_written,
			load_context->bytes_read - bytes_written);
		load_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			load_context->buffer,
			load_context->bytes_read,
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_write_cb,
			load_context);
	} else
		g_input_stream_read_async (
			input_stream,
			load_context->buffer,
			sizeof (load_context->buffer),
			G_PRIORITY_DEFAULT, NULL,
			(GAsyncReadyCallback) image_load_stream_read_cb,
			load_context);
}

static void
image_load_stream_read_cb (GInputStream *input_stream,
                           GAsyncResult *result,
                           ImageLoadContext *load_context)
{
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	if (bytes_read == 0) {
		image_load_finish (load_context);
		return;
	}

	output_stream = load_context->output_stream;
	load_context->bytes_read = bytes_read;

	g_output_stream_write_async (
		output_stream,
		load_context->buffer,
		load_context->bytes_read,
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_write_cb,
		load_context);
}

static void
image_load_file_read_cb (GFile *file,
                         GAsyncResult *result,
                         ImageLoadContext *load_context)
{
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	load_context->input_stream = (GInputStream *) input_stream;

	if (error) {
		image_load_context_free (load_context);
		return;
	}

	/* Load the contents into a GMemoryOutputStream. */
	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	load_context->output_stream = output_stream;

	g_input_stream_read_async (
		load_context->input_stream,
		load_context->buffer,
		sizeof (load_context->buffer),
		G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) image_load_stream_read_cb,
		load_context);
}

static void
image_load_query_info_cb (GFile *file,
                          GAsyncResult *result,
                          ImageLoadContext *load_context)
{
	GFileInfo *file_info;
	GError *error = NULL;

	file_info = g_file_query_info_finish (file, result, &error);
	if (error) {
		image_load_context_free (load_context);
		return;
	}

	load_context->content_type = g_file_info_get_content_type (file_info);
	load_context->total_num_bytes = g_file_info_get_size (file_info);
	load_context->filename = g_file_info_get_name (file_info);

	g_file_read_async (
		file, G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_file_read_cb, load_context);
}

static void
image_load_and_insert_async (EEditorPage *editor_page,
                             const gchar *selector,
                             const gchar *uri)
{
	ImageLoadContext *load_context;
	GFile *file;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (uri && *uri);

	file = g_file_new_for_uri (uri);
	g_return_if_fail (file != NULL);

	load_context = image_load_context_new (editor_page);
	load_context->file = file;
	if (selector && *selector)
		load_context->selector = g_strdup (selector);

	g_file_query_info_async (
		file, "standard::*",
		G_FILE_QUERY_INFO_NONE,G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback)
		image_load_query_info_cb, load_context);
}

void
e_editor_dom_insert_image (EEditorPage *editor_page,
                           const gchar *uri)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (!e_editor_page_get_html_mode (editor_page))
		return;

	if (strstr (uri, ";base64,")) {
		if (g_str_has_prefix (uri, "data:"))
			e_editor_dom_insert_base64_image (editor_page, uri, "", "");
		if (strstr (uri, ";data")) {
			const gchar *base64_data = strstr (uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (uri, filename_length);

			e_editor_dom_insert_base64_image (editor_page, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (editor_page, NULL, uri);
}

void
e_editor_dom_replace_image_src (EEditorPage *editor_page,
                                const gchar *selector,
                                const gchar *uri)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (strstr (uri, ";base64,")) {
		if (g_str_has_prefix (uri, "data:"))
			e_editor_dom_replace_base64_image_src (
				editor_page, selector, uri, "", "");
		if (strstr (uri, ";data")) {
			const gchar *base64_data = strstr (uri, ";") + 1;
			gchar *filename;
			glong filename_length;

			filename_length =
				g_utf8_strlen (uri, -1) -
				g_utf8_strlen (base64_data, -1) - 1;
			filename = g_strndup (uri, filename_length);

			e_editor_dom_replace_base64_image_src (
				editor_page, selector, base64_data, filename, "");
			g_free (filename);
		}
	} else
		image_load_and_insert_async (editor_page, selector, uri);
}

/*
 * e_html_editor_selection_unlink:
 * @selection: an #EEditorSelection
 *
 * Removes any links (&lt;A&gt; elements) from current selection or at current
 * cursor position.
 */
void
e_editor_dom_selection_unlink (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;
	WebKitDOMElement *link;
	EEditorUndoRedoManager *manager;
	gchar *text;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");

	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);

	if (!link) {
		WebKitDOMNode *node;

		/* get element that was clicked on */
		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
			link = dom_node_find_parent_element (node, "A");
			if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link)) {
				g_clear_object (&range);
				return;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	}

	g_clear_object (&range);

	if (!link)
		return;

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;
		WebKitDOMDocumentFragment *fragment;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_REMOVE_LINK;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		fragment = webkit_dom_document_create_document_fragment (document);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (fragment),
			webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (link), TRUE, NULL),
			NULL);
		ev->data.fragment = g_object_ref (fragment);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	text = webkit_dom_html_element_get_inner_text (
		WEBKIT_DOM_HTML_ELEMENT (link));
	webkit_dom_element_set_outer_html (link, text, NULL);
	g_free (text);
}

/*
 * e_html_editor_selection_create_link:
 * @document: a @WebKitDOMDocument
 * @uri: destination of the new link
 *
 * Converts current selection into a link pointing to @url.
 */
void
e_editor_dom_create_link (EEditorPage *editor_page,
                          const gchar *uri)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (uri != NULL && *uri != '\0');

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_CREATE_LINK, uri);
}

static gint
get_list_level (WebKitDOMNode *node)
{
	gint level = 0;

	while (node && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node)) {
		if (node_is_list (node))
			level++;
		node = webkit_dom_node_get_parent_node (node);
	}

	return level;
}

static void
set_ordered_list_type_to_element (WebKitDOMElement *list,
                                  EContentEditorBlockFormat format)
{
	if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST)
		webkit_dom_element_remove_attribute (list, "type");
	else if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA)
		webkit_dom_element_set_attribute (list, "type", "A", NULL);
	else if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN)
		webkit_dom_element_set_attribute (list, "type", "I", NULL);
}

static const gchar *
get_css_alignment_value_class (EContentEditorAlignment alignment)
{
	if (alignment == E_CONTENT_EDITOR_ALIGNMENT_LEFT)
		return ""; /* Left is by default on ltr */

	if (alignment == E_CONTENT_EDITOR_ALIGNMENT_CENTER)
		return "-x-evo-align-center";

	if (alignment == E_CONTENT_EDITOR_ALIGNMENT_RIGHT)
		return "-x-evo-align-right";

	return "";
}

/*
 * e_html_editor_selection_get_alignment:
 * @selection: #an EEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EContentEditorAlignment
 */
static EContentEditorAlignment
dom_get_alignment (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMCSSStyleDeclaration *style = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	EContentEditorAlignment alignment;
	gchar *value;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	document = e_editor_page_get_document (editor_page);
	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	node = webkit_dom_range_get_start_container (range, NULL);
	g_clear_object (&range);
	if (!node)
		return E_CONTENT_EDITOR_ALIGNMENT_LEFT;

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = WEBKIT_DOM_ELEMENT (e_editor_dom_get_parent_block_node_from_child (node));

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (element)) {
		if (element_has_class (element, "-x-evo-align-right"))
			alignment = E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
		else if (element_has_class (element, "-x-evo-align-center"))
			alignment = E_CONTENT_EDITOR_ALIGNMENT_CENTER;
		else
			alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;

		return alignment;
	}

	dom_window = webkit_dom_document_get_default_view (document);
	style = webkit_dom_dom_window_get_computed_style (dom_window, element, NULL);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	} else {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	}

	g_clear_object (&dom_window);
	g_clear_object (&style);
	g_free (value);

	return alignment;
}

static gint
set_word_wrap_length (EEditorPage *editor_page,
                      gint user_word_wrap_length)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	/* user_word_wrap_length < 0, set block width to word_wrap_length
	 * user_word_wrap_length ==  0, no width limit set,
	 * user_word_wrap_length > 0, set width limit to given value */
	return (user_word_wrap_length < 0) ?
		e_editor_page_get_word_wrap_length (editor_page) : user_word_wrap_length;
}

void
e_editor_dom_set_paragraph_style (EEditorPage *editor_page,
                                  WebKitDOMElement *element,
                                  gint width,
                                  gint offset,
                                  const gchar *style_to_add)
{
	WebKitDOMNode *parent;
	gchar *style = NULL;
	gint word_wrap_length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	word_wrap_length = set_word_wrap_length (editor_page, width);
	webkit_dom_element_set_attribute (element, "data-evo-paragraph", "", NULL);

	/* Don't set the alignment for nodes as they are handled separately. */
	if (!node_is_list (WEBKIT_DOM_NODE (element))) {
		EContentEditorAlignment alignment;

		alignment = dom_get_alignment (editor_page);
		element_add_class (element, get_css_alignment_value_class (alignment));
	}

	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	/* Don't set the width limit to sub-blocks as the width limit is inhered
	 * from its parents. */
	if (!e_editor_page_get_html_mode (editor_page) &&
	    (!parent || WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent))) {
		style = g_strdup_printf (
			"width: %dch;%s%s",
			(word_wrap_length + offset),
			style_to_add && *style_to_add ? " " : "",
			style_to_add && *style_to_add ? style_to_add : "");
	} else {
		if (style_to_add && *style_to_add)
			style = g_strdup_printf ("%s", style_to_add);
	}
	if (style) {
		webkit_dom_element_set_attribute (element, "style", style, NULL);
		g_free (style);
	}
}

static WebKitDOMElement *
create_list_element (EEditorPage *editor_page,
                     EContentEditorBlockFormat format,
                     gint level,
                     gboolean html_mode)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *list;
	gboolean inserting_unordered_list;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	inserting_unordered_list = format == E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST;

	list = webkit_dom_document_create_element (
		document, inserting_unordered_list  ? "UL" : "OL", NULL);

	if (!inserting_unordered_list)
		set_ordered_list_type_to_element (list, format);

	if (level >= 0 && !html_mode) {
		gint offset;

		offset = (level + 1) * SPACES_PER_LIST_LEVEL;

		offset += !inserting_unordered_list ?
			SPACES_ORDERED_LIST_FIRST_LEVEL - SPACES_PER_LIST_LEVEL: 0;

		e_editor_dom_set_paragraph_style (editor_page, list, -1, -offset, NULL);
	}

	return list;
}

static gboolean
indent_list (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *item, *next_item;
	gboolean after_selection_end = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	item = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
		gboolean html_mode = e_editor_page_get_html_mode (editor_page);
		WebKitDOMElement *list;
		WebKitDOMNode *source_list = webkit_dom_node_get_parent_node (item);
		EContentEditorBlockFormat format;

		format = dom_get_list_format_from_node (source_list);

		list = create_list_element (
			editor_page, format, get_list_level (item), html_mode);

		element_add_class (list, "-x-evo-indented");

		webkit_dom_node_insert_before (
			source_list, WEBKIT_DOM_NODE (list), item, NULL);

		while (item && !after_selection_end) {
			after_selection_end = webkit_dom_node_contains (
				item, WEBKIT_DOM_NODE (selection_end_marker));

			next_item = webkit_dom_node_get_next_sibling (item);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (list), item, NULL);

			item = next_item;
		}

		merge_lists_if_possible (WEBKIT_DOM_NODE (list));
	}

	return after_selection_end;
}

static void
dom_set_indented_style (EEditorPage *editor_page,
                        WebKitDOMElement *element,
                        gint width)
{
	gchar *style;
	gint word_wrap_length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	word_wrap_length = set_word_wrap_length (editor_page, width);
	webkit_dom_element_set_class_name (element, "-x-evo-indented");

	if (e_editor_page_get_html_mode (editor_page) || word_wrap_length == 0) {
		style = g_strdup_printf ("margin-left: %dch;", SPACES_PER_INDENTATION);

		if (word_wrap_length != 0) {
			gchar *plain_text_style;

			plain_text_style = g_strdup_printf (
				"margin-left: %dch; word-wrap: normal; width: %dch;",
				SPACES_PER_INDENTATION, word_wrap_length);

			webkit_dom_element_set_attribute (
				element, "data-plain-text-style", plain_text_style, NULL);
			g_free (plain_text_style);
		}
	} else {
		style = g_strdup_printf (
			"margin-left: %dch; word-wrap: normal; width: %dch;",
			SPACES_PER_INDENTATION, word_wrap_length);
	}

	webkit_dom_element_set_attribute (element, "style", style, NULL);
	g_free (style);
}

static WebKitDOMElement *
dom_get_indented_element (EEditorPage *editor_page,
                          gint width)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	element = webkit_dom_document_create_element (document, "DIV", NULL);
	dom_set_indented_style (editor_page, element, width);

	return element;
}

static WebKitDOMNode *
indent_block (EEditorPage *editor_page,
              WebKitDOMNode *block,
              gint width)
{
	WebKitDOMElement *element;
	WebKitDOMNode *sibling, *tmp;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	sibling = webkit_dom_node_get_previous_sibling (block);
	if (WEBKIT_DOM_IS_ELEMENT (sibling) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-indented")) {
		element = WEBKIT_DOM_ELEMENT (sibling);
	} else {
		element = dom_get_indented_element (editor_page, width);

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);
	}

	/* Remove style and let the paragraph inherit it from parent */
	if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-evo-paragraph"))
		webkit_dom_element_remove_attribute (
			WEBKIT_DOM_ELEMENT (block), "style");

	tmp = webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (element),
		block,
		NULL);

	sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));

	while (WEBKIT_DOM_IS_ELEMENT (sibling) &&
	       element_has_class (WEBKIT_DOM_ELEMENT (sibling), "-x-evo-indented")) {
		WebKitDOMNode *next_sibling;
		WebKitDOMNode *child;

		next_sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (sibling));

		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (sibling)))) {
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element),
				child,
				NULL);
		}
		remove_node (sibling);
		sibling = next_sibling;
	}

	return tmp;
}

static WebKitDOMNode *
get_list_item_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (child);

	while (parent && !WEBKIT_DOM_IS_HTML_LI_ELEMENT (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}

static WebKitDOMNode *
get_list_node_from_child (WebKitDOMNode *child)
{
	WebKitDOMNode *parent = get_list_item_node_from_child (child);

	return webkit_dom_node_get_parent_node (parent);
}

static gboolean
do_format_change_list_to_block (EEditorPage *editor_page,
                                EContentEditorBlockFormat format,
                                WebKitDOMNode *item,
                                const gchar *value)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *selection_end;
	WebKitDOMNode *node, *source_list;
	gboolean after_end = FALSE;
	gint level;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	selection_end = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	source_list = webkit_dom_node_get_parent_node (item);
	while (source_list) {
		WebKitDOMNode *parent;

		parent = webkit_dom_node_get_parent_node (source_list);
		if (node_is_list (parent))
			source_list = parent;
		else
			break;
	}

	if (webkit_dom_node_contains (source_list, WEBKIT_DOM_NODE (selection_end)))
		source_list = split_list_into_two (item, -1);
	else {
		source_list = webkit_dom_node_get_next_sibling (source_list);
	}

	/* Process all nodes that are in selection one by one */
	while (item && WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
		WebKitDOMNode *next_item;

		next_item = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (item));
		if (!next_item) {
			WebKitDOMNode *parent;
			WebKitDOMNode *tmp = item;

			while (tmp) {
				parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (tmp));
				if (!node_is_list (parent))
					break;

				next_item = webkit_dom_node_get_next_sibling (parent);
				if (node_is_list (next_item)) {
					next_item = webkit_dom_node_get_first_child (next_item);
					break;
				} else if (next_item && !WEBKIT_DOM_IS_HTML_LI_ELEMENT (next_item)) {
					next_item = webkit_dom_node_get_next_sibling (next_item);
					break;
				} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (next_item)) {
					break;
				}
				tmp = parent;
			}
		} else if (node_is_list (next_item)) {
			next_item = webkit_dom_node_get_first_child (next_item);
		} else if (!WEBKIT_DOM_IS_HTML_LI_ELEMENT (next_item)) {
			next_item = webkit_dom_node_get_next_sibling (item);
			continue;
		}

		if (!after_end) {
			after_end = webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end));

			level = get_indentation_level (WEBKIT_DOM_ELEMENT (item));

			if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH) {
				element = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
			} else
				element = webkit_dom_document_create_element (
					document, value, NULL);

			while ((node = webkit_dom_node_get_first_child (item)))
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (element), node, NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (source_list),
				WEBKIT_DOM_NODE (element),
				source_list,
				NULL);

			if (level > 0) {
				gint final_width = 0;

				node = WEBKIT_DOM_NODE (element);

				if (webkit_dom_element_has_attribute (element, "data-evo-paragraph"))
					final_width = e_editor_page_get_word_wrap_length (editor_page) -
						SPACES_PER_INDENTATION * level;

				while (level--)
					node = indent_block (editor_page, node, final_width);
			}

			e_editor_dom_remove_node_and_parents_if_empty (item);
		} else
			break;

		item = next_item;
	}

	remove_node_if_empty (source_list);

	return after_end;
}

static void
format_change_list_to_block (EEditorPage *editor_page,
                             EContentEditorBlockFormat format,
                             const gchar *value)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start;
	WebKitDOMNode *item;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	selection_start = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	item = get_list_item_node_from_child (WEBKIT_DOM_NODE (selection_start));

	do_format_change_list_to_block (editor_page, format, item, value);
}

static WebKitDOMNode *
get_parent_indented_block (WebKitDOMNode *node)
{
	WebKitDOMNode *parent, *block = NULL;

	parent = webkit_dom_node_get_parent_node (node);
	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
		block = parent;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-indented"))
			block = parent;
		parent = webkit_dom_node_get_parent_node (parent);
	}

	return block;
}

static WebKitDOMElement*
get_element_for_inspection (WebKitDOMRange *range)
{
	WebKitDOMNode *node;

	node = webkit_dom_range_get_end_container (range, NULL);
	/* No selection or whole body selected */
	if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
		return NULL;

	return WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
}

static EContentEditorAlignment
dom_get_alignment_from_node (WebKitDOMNode *node)
{
	EContentEditorAlignment alignment;
	gchar *value;
	WebKitDOMCSSStyleDeclaration *style = NULL;

	style = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	} else {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	}

	g_clear_object (&style);
	g_free (value);

	return alignment;
}

/*
 * e_html_editor_selection_indent:
 * @selection: an #EEditorSelection
 *
 * Indents current paragraph by one level.
 */
void
e_editor_dom_selection_indent (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length, level, word_wrap_length, final_width = 0;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list = NULL;

		word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			after_selection_end = indent_list (editor_page);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-evo-paragraph")) {
				level = get_indentation_level (WEBKIT_DOM_ELEMENT (block));

				final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
				if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
				    !e_editor_page_get_html_mode (editor_page))
					goto next;
			}

			indent_block (editor_page, block, final_width);

			if (after_selection_end)
				goto next;
		}

		for (ii = webkit_dom_node_list_get_length (list); ii--;) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process, WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					continue;
			}

			if (webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block_to_process), "data-evo-paragraph")) {
				level = get_indentation_level (
					WEBKIT_DOM_ELEMENT (block_to_process));

				final_width = word_wrap_length - SPACES_PER_INDENTATION * (level + 1);
				if (final_width < MINIMAL_PARAGRAPH_WIDTH &&
				    !e_editor_page_get_html_mode (editor_page))
					continue;
			}

			indent_block (editor_page, block_to_process, final_width);

			if (after_selection_end)
				break;
		}

 next:
		g_clear_object (&list);

		if (!after_selection_end)
			block = next_block;
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);
	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	e_editor_page_emit_content_changed (editor_page);
}

static void
unindent_list (WebKitDOMDocument *document)
{
	gboolean after = FALSE;
	WebKitDOMElement *new_list;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;
	WebKitDOMNode *prev_item;

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Copy elements from previous block to list */
	item = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	new_list = WEBKIT_DOM_ELEMENT (
		webkit_dom_node_clone_node_with_error (source_list, FALSE, NULL));
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node_with_error (source_list, FALSE, NULL);

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	prev_item = source_list;

	while (item) {
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			if (after)
				prev_item = webkit_dom_node_append_child (
					source_list_clone, WEBKIT_DOM_NODE (item), NULL);
			else
				prev_item = webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (prev_item),
					item,
					webkit_dom_node_get_next_sibling (prev_item),
					NULL);
		}

		if (webkit_dom_node_contains (item, WEBKIT_DOM_NODE (selection_end_marker)))
			after = TRUE;

		if (!next_item) {
			if (after)
				break;

			current_list = webkit_dom_node_get_next_sibling (current_list);
			next_item = webkit_dom_node_get_first_child (current_list);
		}
		item = next_item;
	}

	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (source_list);
}

static void
unindent_block (EEditorPage *editor_page,
                WebKitDOMNode *block)
{
	WebKitDOMElement *element;
	WebKitDOMElement *prev_blockquote = NULL, *next_blockquote = NULL;
	WebKitDOMNode *block_to_process, *node_clone = NULL, *child;
	EContentEditorAlignment alignment;
	gboolean before_node = TRUE;
	gint word_wrap_length, level, width;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	block_to_process = block;

	alignment = dom_get_alignment_from_node (block_to_process);
	element = webkit_dom_node_get_parent_element (block_to_process);

	if (!WEBKIT_DOM_IS_HTML_DIV_ELEMENT (element) &&
	    !element_has_class (element, "-x-evo-indented"))
		return;

	element_add_class (WEBKIT_DOM_ELEMENT (block_to_process), "-x-evo-to-unindent");

	level = get_indentation_level (element);
	word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
	width = word_wrap_length - SPACES_PER_INDENTATION * level;

	/* Look if we have previous siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_previous_sibling (block_to_process))
		prev_blockquote = dom_get_indented_element (editor_page, width);

	/* Look if we have next siblings, if so, we have to
	 * create new blockquote that will include them */
	if (webkit_dom_node_get_next_sibling (block_to_process))
		next_blockquote = dom_get_indented_element (editor_page, width);

	/* Copy nodes that are before / after the element that we want to unindent */
	while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element)))) {
		if (webkit_dom_node_is_equal_node (child, block_to_process)) {
			before_node = FALSE;
			node_clone = webkit_dom_node_clone_node_with_error (child, TRUE, NULL);
			remove_node (child);
			continue;
		}

		webkit_dom_node_append_child (
			before_node ?
				WEBKIT_DOM_NODE (prev_blockquote) :
				WEBKIT_DOM_NODE (next_blockquote),
			child,
			NULL);
	}

	if (node_clone) {
		element_remove_class (WEBKIT_DOM_ELEMENT (node_clone), "-x-evo-to-unindent");

		/* Insert blockqoute with nodes that were before the element that we want to unindent */
		if (prev_blockquote) {
			if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (prev_blockquote))) {
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					WEBKIT_DOM_NODE (prev_blockquote),
					WEBKIT_DOM_NODE (element),
					NULL);
			}
		}

		if (level == 1 && webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (node_clone), "data-evo-paragraph")) {
			e_editor_dom_set_paragraph_style (
				editor_page, WEBKIT_DOM_ELEMENT (node_clone), word_wrap_length, 0, NULL);
			element_add_class (
				WEBKIT_DOM_ELEMENT (node_clone),
				get_css_alignment_value_class (alignment));
		}

		/* Insert the unindented element */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			node_clone,
			WEBKIT_DOM_NODE (element),
			NULL);
	} else {
		g_warn_if_reached ();
	}

	/* Insert blockqoute with nodes that were after the element that we want to unindent */
	if (next_blockquote) {
		if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (next_blockquote))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
				WEBKIT_DOM_NODE (next_blockquote),
				WEBKIT_DOM_NODE (element),
				NULL);
		}
	}

	/* Remove old blockquote */
	remove_node (WEBKIT_DOM_NODE (element));
}

/*
 * dom_unindent:
 * @selection: an #EEditorSelection
 *
 * Unindents current paragraph by one level.
 */
void
e_editor_dom_selection_unindent (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean after_selection_start = FALSE, after_selection_end = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_INDENT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
	}

	block = get_parent_indented_block (
		WEBKIT_DOM_NODE (selection_start_marker));
	if (!block)
		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		gint ii, length;
		WebKitDOMNode *next_block;
		WebKitDOMNodeList *list = NULL;

		next_block = webkit_dom_node_get_next_sibling (block);

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block),
			".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
			NULL);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		length = webkit_dom_node_list_get_length (list);
		if (length == 0 && node_is_list_or_item (block)) {
			unindent_list (document);
			goto next;
		}

		if (length == 0) {
			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block, WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					goto next;
			}

			unindent_block (editor_page, block);

			if (after_selection_end)
				goto next;
		}

		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *block_to_process;

			block_to_process = webkit_dom_node_list_item (list, ii);

			after_selection_end = webkit_dom_node_contains (
				block_to_process,
				WEBKIT_DOM_NODE (selection_end_marker));

			if (!after_selection_start) {
				after_selection_start = webkit_dom_node_contains (
					block_to_process,
					WEBKIT_DOM_NODE (selection_start_marker));
				if (!after_selection_start)
					continue;
			}

			unindent_block (editor_page, block_to_process);

			if (after_selection_end)
				break;
		}
 next:
		g_clear_object (&list);
		block = next_block;
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	e_editor_page_emit_content_changed (editor_page);
}

static void
dom_insert_selection_point (WebKitDOMNode *container,
                            glong offset,
                            WebKitDOMElement *selection_point)
{
	WebKitDOMNode *parent;

	parent = webkit_dom_node_get_parent_node (container);

	if (WEBKIT_DOM_IS_TEXT (container) ||
	    WEBKIT_DOM_IS_COMMENT (container) ||
	    WEBKIT_DOM_IS_CHARACTER_DATA (container)) {
		if (offset != 0) {
			WebKitDOMText *split_text;

			split_text = webkit_dom_text_split_text (
				WEBKIT_DOM_TEXT (container), offset, NULL);
			parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (split_text));

			webkit_dom_node_insert_before (
				parent,
				WEBKIT_DOM_NODE (selection_point),
				WEBKIT_DOM_NODE (split_text),
				NULL);
		} else {
			webkit_dom_node_insert_before (
				parent,
				WEBKIT_DOM_NODE (selection_point),
				container,
				NULL);
		}
	} else {
		gulong child_element_count = 0;

		child_element_count =
			webkit_dom_element_get_child_element_count (
				WEBKIT_DOM_ELEMENT (container));

		if (offset == 0) {
			/* Selection point is on the beginning of container */
			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (selection_point),
				webkit_dom_node_get_first_child (container),
				NULL);
		} else if (offset != 0 && (offset == child_element_count)) {
			/* Selection point is on the end of container */
			webkit_dom_node_append_child (
				container, WEBKIT_DOM_NODE (selection_point), NULL);
		} else {
			WebKitDOMElement *child;
			gint ii = 0;

			child = webkit_dom_element_get_first_element_child (WEBKIT_DOM_ELEMENT (container));
			for (ii = 1; ii < child_element_count; ii++)
				child = webkit_dom_element_get_next_element_sibling (child);

			webkit_dom_node_insert_before (
				container,
				WEBKIT_DOM_NODE (selection_point),
				WEBKIT_DOM_NODE (child),
				NULL);
		}
	}

	webkit_dom_node_normalize (parent);
}

/*
 * e_html_editor_selection_save:
 * @selection: an #EEditorSelection
 *
 * Saves current cursor position or current selection range. The selection can
 * be later restored by calling e_html_editor_selection_restore().
 *
 * Note that calling e_html_editor_selection_save() overwrites previously saved
 * position.
 *
 * Note that this method inserts special markings into the HTML code that are
 * used to later restore the selection. It can happen that by deleting some
 * segments of the document some of the markings are deleted too. In that case
 * restoring the selection by e_html_editor_selection_restore() can fail. Also by
 * moving text segments (Cut & Paste) can result in moving the markings
 * elsewhere, thus e_html_editor_selection_restore() will restore the selection
 * incorrectly.
 *
 * It is recommended to use this method only when you are not planning to make
 * bigger changes to content or structure of the document (formatting changes
 * are usually OK).
 */
void
e_editor_dom_selection_save (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMRange *range = NULL;
	WebKitDOMNode *container;
	WebKitDOMNode *anchor;
	WebKitDOMElement *start_marker = NULL, *end_marker = NULL;
	gboolean collapsed = FALSE;
	glong offset, anchor_offset;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	/* First remove all markers (if present) */
	dom_remove_selection_markers (document);

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		g_clear_object (&dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range) {
		g_clear_object (&dom_selection);
		return;
	}

	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	collapsed = webkit_dom_range_get_collapsed (range, NULL);
	start_marker = dom_create_selection_marker (document, TRUE);

	container = webkit_dom_range_get_start_container (range, NULL);
	offset = webkit_dom_range_get_start_offset (range, NULL);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (start_marker, "data-anchor", "", NULL);

	dom_insert_selection_point (container, offset, start_marker);

	end_marker = dom_create_selection_marker (document, FALSE);

	if (collapsed) {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (start_marker)),
			WEBKIT_DOM_NODE (end_marker),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (start_marker)),
			NULL);
		goto out;
	}

	container = webkit_dom_range_get_end_container (range, NULL);
	offset = webkit_dom_range_get_end_offset (range, NULL);

	if (webkit_dom_node_is_same_node (anchor, container) && offset == anchor_offset)
		webkit_dom_element_set_attribute (end_marker, "data-anchor", "", NULL);

	dom_insert_selection_point (container, offset, end_marker);

	if (!collapsed) {
		if (start_marker && end_marker) {
			webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (start_marker), NULL);
			webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (end_marker), NULL);
		} else {
			g_warn_if_reached ();
		}

		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}
 out:
	g_clear_object (&range);
	g_clear_object (&dom_selection);
}

gboolean
e_editor_dom_is_selection_position_node (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
		return FALSE;

	element = WEBKIT_DOM_ELEMENT (node);

	return element_has_id (element, "-x-evo-selection-start-marker") ||
	       element_has_id (element, "-x-evo-selection-end-marker");
}

/*
 * e_html_editor_selection_restore:
 * @selection: an #EEditorSelection
 *
 * Restores cursor position or selection range that was saved by
 * e_html_editor_selection_save().
 *
 * Note that calling this function without calling e_html_editor_selection_save()
 * before is a programming error and the behavior is undefined.
 */
void
e_editor_dom_selection_restore (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *marker;
	WebKitDOMNode *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *parent_start, *parent_end, *anchor;
	WebKitDOMRange *range = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	gboolean start_is_anchor = FALSE;
	glong offset;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	g_clear_object (&dom_window);
	if (!range) {
		WebKitDOMHTMLElement *body;

		range = webkit_dom_document_create_range (document);
		body = webkit_dom_document_get_body (document);

		webkit_dom_range_select_node_contents (range, WEBKIT_DOM_NODE (body), NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	selection_start_marker = webkit_dom_range_get_start_container (range, NULL);
	if (selection_start_marker) {
		gboolean ok = FALSE;
		selection_start_marker =
			webkit_dom_node_get_next_sibling (selection_start_marker);

		ok = e_editor_dom_is_selection_position_node (selection_start_marker);

		if (ok) {
			ok = FALSE;
			if (webkit_dom_range_get_collapsed (range, NULL)) {
				selection_end_marker = webkit_dom_node_get_next_sibling (
					selection_start_marker);

				ok = e_editor_dom_is_selection_position_node (selection_end_marker);
				if (ok) {
					WebKitDOMNode *next_sibling;

					next_sibling = webkit_dom_node_get_next_sibling (selection_end_marker);

					if (next_sibling && !WEBKIT_DOM_IS_HTML_BR_ELEMENT (next_sibling)) {
						parent_start = webkit_dom_node_get_parent_node (selection_end_marker);

						remove_node (selection_start_marker);
						remove_node (selection_end_marker);

						webkit_dom_node_normalize (parent_start);
						g_clear_object (&range);
						g_clear_object (&dom_selection);
						return;
					}
				}
			}
		}
	}

	g_clear_object (&range);
	range = webkit_dom_document_create_range (document);
	if (!range) {
		g_clear_object (&dom_selection);
		return;
	}

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_clear_object (&dom_selection);
		g_clear_object (&range);
		return;
	}

	start_is_anchor = webkit_dom_element_has_attribute (marker, "data-anchor");
	parent_start = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_range_set_start_after (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");
	if (!marker) {
		marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (marker)
			remove_node (WEBKIT_DOM_NODE (marker));
		g_clear_object (&dom_selection);
		g_clear_object (&range);
		return;
	}

	parent_end = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_range_set_end_before (range, WEBKIT_DOM_NODE (marker), NULL);
	remove_node (WEBKIT_DOM_NODE (marker));

	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	if (webkit_dom_node_is_same_node (parent_start, parent_end))
		webkit_dom_node_normalize (parent_start);
	else {
		webkit_dom_node_normalize (parent_start);
		webkit_dom_node_normalize (parent_end);
	}

	if (start_is_anchor) {
		anchor = webkit_dom_range_get_end_container (range, NULL);
		offset = webkit_dom_range_get_end_offset (range, NULL);

		webkit_dom_range_collapse (range, TRUE, NULL);
	} else {
		anchor = webkit_dom_range_get_start_container (range, NULL);
		offset = webkit_dom_range_get_start_offset (range, NULL);

		webkit_dom_range_collapse (range, FALSE, NULL);
	}
	webkit_dom_dom_selection_add_range (dom_selection, range);
	webkit_dom_dom_selection_extend (dom_selection, anchor, offset, NULL);

	g_clear_object (&dom_selection);
	g_clear_object (&range);
}

static gint
find_where_to_break_line (WebKitDOMCharacterData *node,
                          gint max_length)
{
	gboolean last_break_position_is_dash = FALSE;
	gchar *str, *text_start;
	gunichar uc;
	gint pos = 1, last_break_position = 0, ret_val = 0;

	text_start = webkit_dom_character_data_get_data (node);

	str = text_start;
	do {
		uc = g_utf8_get_char (str);
		if (!uc) {
			ret_val = pos <= max_length ? pos : last_break_position > 0 ? last_break_position - 1 : 0;
			goto out;
		}

		if ((g_unichar_isspace (uc) && !(g_unichar_break_type (uc) == G_UNICODE_BREAK_NON_BREAKING_GLUE)) ||
		     *str == '-') {
			if ((last_break_position_is_dash = *str == '-')) {
				/* There was no space before the dash */
				if (pos - 1 != last_break_position) {
					gchar *rest;

					rest = g_utf8_next_char (str);
					if (rest && *rest) {
						gunichar next_char;

						/* There is no space after the dash */
						next_char = g_utf8_get_char (rest);
						if (g_unichar_isspace (next_char))
							last_break_position_is_dash = FALSE;
						else
							last_break_position = pos;
					} else
						last_break_position_is_dash = FALSE;
				} else
					last_break_position_is_dash = FALSE;
			} else
				last_break_position = pos;
		}

		if ((pos == max_length)) {
			/* Look one character after the limit to check if there
			 * is a space (skip dash) that we are allowed to break at, if so
			 * break it there. */
			if (*str) {
				str = g_utf8_next_char (str);
				uc = g_utf8_get_char (str);

				if ((g_unichar_isspace (uc) &&
				    !(g_unichar_break_type (uc) == G_UNICODE_BREAK_NON_BREAKING_GLUE)))
					last_break_position = ++pos;
			}
			break;
		}

		pos++;
		str = g_utf8_next_char (str);
	} while (*str);

	if (last_break_position != 0)
		ret_val = last_break_position - 1;
 out:
	g_free (text_start);

	/* Always break after the dash character. */
	if (last_break_position_is_dash)
		ret_val++;

	/* No character to break at is found. We should split at max_length, but
	 * we will leave the decision on caller as it depends on context. */
	if (ret_val == 0 && last_break_position == 0)
		ret_val = -1;

	return ret_val;
}

/*
 * e_html_editor_selection_is_collapsed:
 * @selection: an #EEditorSelection
 *
 * Returns if selection is collapsed.
 *
 * Returns: Whether the selection is collapsed (just caret) or not (someting is selected).
 */
gboolean
e_editor_dom_selection_is_collapsed (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	gboolean collapsed;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
	if (!(dom_window = webkit_dom_document_get_default_view (document)))
		return FALSE;

	if (!(dom_selection = webkit_dom_dom_window_get_selection (dom_window))) {
		g_clear_object (&dom_window);
		return FALSE;
	}

	collapsed = webkit_dom_dom_selection_get_is_collapsed (dom_selection);

	g_clear_object (&dom_selection);

	return collapsed;
}

void
e_editor_dom_scroll_to_caret (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMElement *selection_start_marker;
	glong element_top, element_left;
	glong window_top, window_left, window_right, window_bottom;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!selection_start_marker)
		return;

	dom_window = webkit_dom_document_get_default_view (document);

	window_top = webkit_dom_dom_window_get_scroll_y (dom_window);
	window_left = webkit_dom_dom_window_get_scroll_x (dom_window);
	window_bottom = window_top + webkit_dom_dom_window_get_inner_height (dom_window);
	window_right = window_left + webkit_dom_dom_window_get_inner_width (dom_window);

	element_left = webkit_dom_element_get_offset_left (selection_start_marker);
	element_top = webkit_dom_element_get_offset_top (selection_start_marker);

	/* Check if caret is inside viewport, if not move to it */
	if (!(element_top >= window_top && element_top <= window_bottom &&
	     element_left >= window_left && element_left <= window_right)) {
		webkit_dom_element_scroll_into_view (selection_start_marker, TRUE);
	}

	e_editor_dom_selection_restore (editor_page);

	g_clear_object (&dom_window);
}

static void
mark_and_remove_trailing_space (WebKitDOMDocument *document,
                                WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (element, "data-hidden-space", "", NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		webkit_dom_node_get_next_sibling (node),
		NULL);
	webkit_dom_character_data_replace_data (
		WEBKIT_DOM_CHARACTER_DATA (node),
		webkit_dom_character_data_get_length (WEBKIT_DOM_CHARACTER_DATA (node)),
		1,
		"",
		NULL);
}

static void
mark_and_remove_leading_space (WebKitDOMDocument *document,
                               WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	element = webkit_dom_document_create_element (document, "SPAN", NULL);
	webkit_dom_element_set_attribute (element, "data-hidden-space", "", NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (node),
		WEBKIT_DOM_NODE (element),
		node,
		NULL);
	webkit_dom_character_data_replace_data (
		WEBKIT_DOM_CHARACTER_DATA (node), 0, 1, "", NULL);
}

static WebKitDOMElement *
wrap_lines (EEditorPage *editor_page,
            WebKitDOMNode *block,
            gboolean remove_all_br,
            gint length_to_wrap,
            gint word_wrap_length)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *node, *start_node, *block_clone = NULL;
	WebKitDOMNode *start_point = NULL, *first_child, *last_child;
	guint line_length;
	gulong length_left;
	gchar *text_content;
	gboolean compensated = FALSE;
	gboolean check_next_node = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	if (!webkit_dom_node_has_child_nodes (block))
		return WEBKIT_DOM_ELEMENT (block);

	/* Avoid wrapping when the block contains just the BR element alone
	 * or with selection markers. */
	if ((first_child = webkit_dom_node_get_first_child (block)) &&
	     WEBKIT_DOM_IS_HTML_BR_ELEMENT (first_child)) {
		WebKitDOMNode *next_sibling;

		if ((next_sibling = webkit_dom_node_get_next_sibling (first_child))) {
		       if (e_editor_dom_is_selection_position_node (next_sibling) &&
			   (next_sibling = webkit_dom_node_get_next_sibling (next_sibling)) &&
			   e_editor_dom_is_selection_position_node (next_sibling) &&
			   !webkit_dom_node_get_next_sibling (next_sibling))
				return WEBKIT_DOM_ELEMENT (block);
		} else
			return WEBKIT_DOM_ELEMENT (block);
	}

	block_clone = webkit_dom_node_clone_node_with_error (block, TRUE, NULL);

	/* When we wrap, we are wrapping just the text after caret, text
	 * before the caret is already wrapped, so unwrap the text after
	 * the caret position */
	selection_end_marker = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block_clone),
		"span#-x-evo-selection-end-marker",
		NULL);

	if (selection_end_marker) {
		WebKitDOMNode *nd = WEBKIT_DOM_NODE (selection_end_marker);

		while (nd) {
			WebKitDOMNode *parent_node;
			WebKitDOMNode *next_nd = webkit_dom_node_get_next_sibling (nd);

			parent_node = webkit_dom_node_get_parent_node (nd);
			if (!next_nd && parent_node && !webkit_dom_node_is_same_node (parent_node, block_clone))
				next_nd = webkit_dom_node_get_next_sibling (parent_node);

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (nd)) {
				if (remove_all_br)
					remove_node (nd);
				else if (element_has_class (WEBKIT_DOM_ELEMENT (nd), "-x-evo-wrap-br"))
					remove_node (nd);
			} else if (WEBKIT_DOM_IS_ELEMENT (nd) &&
				   webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (nd), "data-hidden-space"))
				webkit_dom_html_element_set_outer_text (
					WEBKIT_DOM_HTML_ELEMENT (nd), " ", NULL);

			nd = next_nd;
		}
	} else {
		gint ii;
		WebKitDOMNodeList *list = NULL;

		list = webkit_dom_element_query_selector_all (
			WEBKIT_DOM_ELEMENT (block_clone), "span[data-hidden-space]", NULL);
		for (ii = webkit_dom_node_list_get_length (list); ii--;) {
			WebKitDOMNode *hidden_space_node;

			hidden_space_node = webkit_dom_node_list_item (list, ii);
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
		}
		g_clear_object (&list);
	}

	/* We have to start from the end of the last wrapped line */
	selection_start_marker = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block_clone),
		"span#-x-evo-selection-start-marker",
		NULL);

	if (selection_start_marker) {
		gboolean first_removed = FALSE;
		WebKitDOMNode *nd;

		nd = webkit_dom_node_get_previous_sibling (
			WEBKIT_DOM_NODE (selection_start_marker));
		while (nd) {
			WebKitDOMNode *prev_nd = webkit_dom_node_get_previous_sibling (nd);

			if (!prev_nd && !webkit_dom_node_is_same_node (webkit_dom_node_get_parent_node (nd), block_clone))
				prev_nd = webkit_dom_node_get_previous_sibling (webkit_dom_node_get_parent_node (nd));

			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (nd)) {
				if (first_removed) {
					start_point = nd;
					break;
				} else {
					remove_node (nd);
					first_removed = TRUE;
				}
			} else if (WEBKIT_DOM_IS_ELEMENT (nd) &&
				   webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (nd), "data-hidden-space")) {
				webkit_dom_html_element_set_outer_text (
					WEBKIT_DOM_HTML_ELEMENT (nd), " ", NULL);
			} else if (!prev_nd) {
				WebKitDOMNode *parent;

				parent = webkit_dom_node_get_parent_node (nd);
				if (!WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent))
					start_point = nd;
			}

			nd = prev_nd;
		}
	}

	webkit_dom_node_normalize (block_clone);
	node = webkit_dom_node_get_first_child (block_clone);
	if (node) {
		text_content = webkit_dom_node_get_text_content (node);
		if (g_strcmp0 ("\n", text_content) == 0)
			node = webkit_dom_node_get_next_sibling (node);
		g_free (text_content);
	}

	if (start_point) {
		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (start_point))
			node = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (start_point));
		else
			node = start_point;
		start_node = block_clone;
	} else
		start_node = node;

	line_length = 0;
	while (node) {
		gint offset = 0;
		WebKitDOMElement *element;

		if (WEBKIT_DOM_IS_TEXT (node)) {
			const gchar *newline;
			WebKitDOMNode *next_sibling;

			/* If there is temporary hidden space we remove it */
			text_content = webkit_dom_node_get_text_content (node);
			if (strstr (text_content, UNICODE_ZERO_WIDTH_SPACE)) {
				if (g_str_has_prefix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						0,
						1,
						NULL);
				if (g_str_has_suffix (text_content, UNICODE_ZERO_WIDTH_SPACE))
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (node),
						g_utf8_strlen (text_content, -1) - 1,
						1,
						NULL);
				g_free (text_content);
				text_content = webkit_dom_node_get_text_content (node);
			}
			newline = strstr (text_content, "\n");

			next_sibling = node;
			while (newline) {
				next_sibling = WEBKIT_DOM_NODE (webkit_dom_text_split_text (
					WEBKIT_DOM_TEXT (next_sibling),
					g_utf8_pointer_to_offset (text_content, newline),
					NULL));

				if (!next_sibling)
					break;

				element = webkit_dom_document_create_element (
					document, "BR", NULL);
				element_add_class (element, "-x-evo-wrap-br");

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (next_sibling),
					WEBKIT_DOM_NODE (element),
					next_sibling,
					NULL);

				g_free (text_content);

				text_content = webkit_dom_node_get_text_content (next_sibling);
				if (g_str_has_prefix (text_content, "\n")) {
					webkit_dom_character_data_delete_data (
						WEBKIT_DOM_CHARACTER_DATA (next_sibling), 0, 1, NULL);
					g_free (text_content);
					text_content =
						webkit_dom_node_get_text_content (next_sibling);
				}
				newline = strstr (text_content, "\n");
			}
			g_free (text_content);
		} else if (WEBKIT_DOM_IS_ELEMENT (node)) {
			if (e_editor_dom_is_selection_position_node (node)) {
				if (line_length == 0) {
					WebKitDOMNode *tmp_node;

					tmp_node = webkit_dom_node_get_previous_sibling (node);
					/* Only check if there is some node before the selection marker. */
					if (tmp_node && !e_editor_dom_is_selection_position_node (tmp_node))
						check_next_node = TRUE;
				}
				node = webkit_dom_node_get_next_sibling (node);
				continue;
			}

			check_next_node = FALSE;
			/* If element is ANCHOR we wrap it separately */
			if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				glong anchor_length;
				WebKitDOMNode *next_sibling;

				text_content = webkit_dom_node_get_text_content (node);
				anchor_length = g_utf8_strlen (text_content, -1);
				g_free (text_content);

				next_sibling = webkit_dom_node_get_next_sibling (node);
				/* If the anchor doesn't fit on the line move the inner
				 * nodes out of it and start to wrap them. */
				if ((line_length + anchor_length) > length_to_wrap) {
					WebKitDOMNode *inner_node;

					while ((inner_node = webkit_dom_node_get_first_child (node))) {
						g_object_set_data (
							G_OBJECT (inner_node),
							"-x-evo-anchor-text",
							GINT_TO_POINTER (1));
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							inner_node,
							next_sibling,
							NULL);
					}
					next_sibling = webkit_dom_node_get_next_sibling (node);

					remove_node (node);
					node = next_sibling;
					continue;
				}

				line_length += anchor_length;
				node = next_sibling;
				continue;
			}

			if (element_has_class (WEBKIT_DOM_ELEMENT (node), "Apple-tab-span")) {
				WebKitDOMNode *sibling;
				gint tab_length;

				sibling = webkit_dom_node_get_previous_sibling (node);
				if (sibling && WEBKIT_DOM_IS_ELEMENT (sibling) &&
				    element_has_class (WEBKIT_DOM_ELEMENT (sibling), "Apple-tab-span"))
					tab_length = TAB_LENGTH;
				else {
					tab_length = TAB_LENGTH - (line_length + compensated ? 0 : (word_wrap_length - length_to_wrap)) % TAB_LENGTH;
					compensated = TRUE;
				}

				if (line_length + tab_length > length_to_wrap) {
					if (webkit_dom_node_get_next_sibling (node)) {
						element = webkit_dom_document_create_element (
							document, "BR", NULL);
						element_add_class (element, "-x-evo-wrap-br");
						node = webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							webkit_dom_node_get_next_sibling (node),
							NULL);
					}
					line_length = 0;
					compensated = FALSE;
				} else
					line_length += tab_length;

				sibling = webkit_dom_node_get_next_sibling (node);
				node = sibling;
				continue;
			}
			/* When we are not removing user-entered BR elements (lines wrapped by user),
			 * we need to skip those elements */
			if (!remove_all_br && WEBKIT_DOM_IS_HTML_BR_ELEMENT (node)) {
				if (!element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-wrap-br")) {
					line_length = 0;
					compensated = FALSE;
					node = webkit_dom_node_get_next_sibling (node);
					continue;
				}
			}
			goto next_node;
		} else {
			WebKitDOMNode *sibling;

			sibling = webkit_dom_node_get_next_sibling (node);
			node = sibling;
			continue;
		}

		/* If length of this node + what we already have is still less
		 * then length_to_wrap characters, then just concatenate it and
		 * continue to next node */
		length_left = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (node));

		if ((length_left + line_length) <= length_to_wrap) {
			if (check_next_node)
				goto check_node;
			line_length += length_left;
			if (line_length == length_to_wrap) {
				line_length = 0;

				element = webkit_dom_document_create_element (document, "BR", NULL);
				element_add_class (element, "-x-evo-wrap-br");

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					webkit_dom_node_get_next_sibling (node),
					NULL);
			}
			goto next_node;
		}

		/* wrap until we have something */
		while (node && (length_left + line_length) > length_to_wrap) {
			gboolean insert_and_continue;
			gint max_length;

 check_node:
			insert_and_continue = FALSE;

			if (!WEBKIT_DOM_IS_CHARACTER_DATA (node))
				goto next_node;

			element = webkit_dom_document_create_element (document, "BR", NULL);
			element_add_class (element, "-x-evo-wrap-br");

			max_length = length_to_wrap - line_length;
			if (max_length < 0)
				max_length = length_to_wrap;
			else if (max_length == 0) {
				if (check_next_node) {
					insert_and_continue = TRUE;
					goto check;
				}

				/* Break before the current node and continue. */
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (node),
					WEBKIT_DOM_NODE (element),
					node,
					NULL);
				line_length = 0;
				continue;
			}

			/* Allow anchors to break on any character. */
			if (g_object_steal_data (G_OBJECT (node), "-x-evo-anchor-text"))
				offset = max_length;
			else {
				/* Find where we can line-break the node so that it
				 * effectively fills the rest of current row. */
				offset = find_where_to_break_line (
					WEBKIT_DOM_CHARACTER_DATA (node), max_length);

				/* When pressing delete on the end of line to concatenate
				 * the last word from the line and first word from the
				 * next line we will end with the second word split
				 * somewhere in the middle (to be precise it will be
				 * split after the last character that will fit on the
				 * previous line. To avoid that we need to put the
				 * concatenated word on the next line. */
				if (offset == -1 || check_next_node) {
					WebKitDOMNode *prev_sibling;

 check:
					check_next_node = FALSE;
					prev_sibling = webkit_dom_node_get_previous_sibling (node);
					if (prev_sibling && e_editor_dom_is_selection_position_node (prev_sibling)) {
						WebKitDOMNode *prev_br = NULL;

						prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

						/* Collapsed selection */
						if (prev_sibling && e_editor_dom_is_selection_position_node (prev_sibling))
							prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);

						if (prev_sibling && WEBKIT_DOM_IS_HTML_BR_ELEMENT (prev_sibling) &&
						    element_has_class (WEBKIT_DOM_ELEMENT (prev_sibling), "-x-evo-wrap-br")) {
							prev_br = prev_sibling;
							prev_sibling = webkit_dom_node_get_previous_sibling (prev_sibling);
						}

						if (prev_sibling && WEBKIT_DOM_IS_CHARACTER_DATA (prev_sibling)) {
							gchar *data;
							glong text_length, length = 0;

							data = webkit_dom_character_data_get_data (
								WEBKIT_DOM_CHARACTER_DATA (prev_sibling));
							text_length = webkit_dom_character_data_get_length (
								WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

							/* Find the last character where we can break. */
							while (text_length - length > 0) {
								if (strchr (" ", data[text_length - length - 1])) {
									length++;
									break;
								} else if (data[text_length - length - 1] == '-' &&
								           text_length - length > 1 &&
								           !strchr (" ", data[text_length - length - 2]))
									break;
								length++;
							}

							if (text_length != length) {
								WebKitDOMNode *nd;

								webkit_dom_text_split_text (
									WEBKIT_DOM_TEXT (prev_sibling),
									text_length - length,
									NULL);

								if ((nd = webkit_dom_node_get_next_sibling (prev_sibling))) {
									gchar *nd_content;

									nd_content = webkit_dom_node_get_text_content (nd);
									if (nd_content && *nd_content) {
										if (*nd_content == ' ')
											mark_and_remove_leading_space (document, nd);

										if (!webkit_dom_node_get_next_sibling (nd) &&
										    g_str_has_suffix (nd_content, " "))
											mark_and_remove_trailing_space (document, nd);

										g_free (nd_content);
									}

									if (nd) {
										if (prev_br)
											remove_node (prev_br);
										 webkit_dom_node_insert_before (
											webkit_dom_node_get_parent_node (nd),
											WEBKIT_DOM_NODE (element),
											nd,
											NULL);

										offset = 0;
										line_length = length;
										continue;
									}
								}
							}
						}
					}
					if (insert_and_continue) {
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							node,
							NULL);

						offset = 0;
						line_length = 0;
						insert_and_continue = FALSE;
						continue;
					}

					offset = offset != -1 ? offset : max_length;
				}
			}

			if (offset >= 0) {
				WebKitDOMNode *nd;

				if (offset != length_left && offset != 0) {
					webkit_dom_text_split_text (
						WEBKIT_DOM_TEXT (node), offset, NULL);

					nd = webkit_dom_node_get_next_sibling (node);
				} else
					nd = node;

				if (nd) {
					gboolean no_sibling = FALSE;
					gchar *nd_content;

					nd_content = webkit_dom_node_get_text_content (nd);
					if (nd_content && *nd_content) {
						if (*nd_content == ' ')
							mark_and_remove_leading_space (document, nd);

						if (!webkit_dom_node_get_next_sibling (nd) &&
						    length_left <= length_to_wrap &&
						    g_str_has_suffix (nd_content, " ")) {
							mark_and_remove_trailing_space (document, nd);
							no_sibling = TRUE;
						}

						g_free (nd_content);
					}

					if (!no_sibling)
						webkit_dom_node_insert_before (
							webkit_dom_node_get_parent_node (node),
							WEBKIT_DOM_NODE (element),
							nd,
							NULL);

					offset = 0;

					nd_content = webkit_dom_node_get_text_content (nd);
					if (!*nd_content)
						remove_node (nd);
					g_free (nd_content);

					if (no_sibling)
						node = NULL;
					else
						node = webkit_dom_node_get_next_sibling (
							WEBKIT_DOM_NODE (element));
				} else {
					webkit_dom_node_append_child (
						webkit_dom_node_get_parent_node (node),
						WEBKIT_DOM_NODE (element),
						NULL);
				}
			}
			if (node && WEBKIT_DOM_IS_CHARACTER_DATA (node))
				length_left = webkit_dom_character_data_get_length (
					WEBKIT_DOM_CHARACTER_DATA (node));

			line_length = 0;
			compensated = FALSE;
		}
		line_length += length_left - offset;
 next_node:
		if (!node)
			break;

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (node)) {
			line_length = 0;
			compensated = FALSE;
		}

		/* Move to next node */
		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			WebKitDOMNode *tmp_parent;

			if (webkit_dom_node_is_equal_node (node, start_node))
				break;

			/* Find a next node that we can process. */
			tmp_parent = webkit_dom_node_get_parent_node (node);
			if (tmp_parent && webkit_dom_node_get_next_sibling (tmp_parent))
				node = webkit_dom_node_get_next_sibling (tmp_parent);
			else {
				WebKitDOMNode *tmp;

				tmp = tmp_parent;
				/* Find a node that is not a start node (that would mean
				 * that we already processed the whole block) and it has
				 * a sibling that we can process. */
				while (tmp && !webkit_dom_node_is_equal_node (tmp, start_node) &&
				       !webkit_dom_node_get_next_sibling (tmp)) {
					tmp = webkit_dom_node_get_parent_node (tmp);
				}

				/* If we found a node to process, let's process its
				 * sibling, otherwise give up. */
				if (tmp)
					node = webkit_dom_node_get_next_sibling (tmp);
				else
					break;
			}
		}
	}

	last_child = webkit_dom_node_get_last_child (block_clone);
	if (last_child && WEBKIT_DOM_IS_HTML_BR_ELEMENT (last_child) &&
	    element_has_class (WEBKIT_DOM_ELEMENT (last_child), "-x-evo-wrap-br"))
		remove_node (last_child);

	webkit_dom_node_normalize (block_clone);

	node = webkit_dom_node_get_parent_node (block);
	if (node) {
		/* Replace block with wrapped one */
		webkit_dom_node_replace_child (
			node, block_clone, block, NULL);
	}

	return WEBKIT_DOM_ELEMENT (block_clone);
}

void
e_editor_dom_remove_wrapping_from_element (WebKitDOMElement *element)
{
	WebKitDOMNodeList *list = NULL;
	gint ii;

	g_return_if_fail (element != NULL);

	list = webkit_dom_element_query_selector_all (
		element, "br.-x-evo-wrap-br", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);
		WebKitDOMNode *parent;

		parent = e_editor_dom_get_parent_block_node_from_child (node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped"))
			remove_node (node);
	}

	g_clear_object (&list);

	list = webkit_dom_element_query_selector_all (
		element, "span[data-hidden-space]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *hidden_space_node;
		WebKitDOMNode *parent;

		hidden_space_node = webkit_dom_node_list_item (list, ii);
		parent = e_editor_dom_get_parent_block_node_from_child (hidden_space_node);
		if (!webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "data-user-wrapped")) {
			webkit_dom_html_element_set_outer_text (
				WEBKIT_DOM_HTML_ELEMENT (hidden_space_node), " ", NULL);
		}
	}
	g_clear_object (&list);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

void
e_editor_dom_remove_quoting_from_element (WebKitDOMElement *element)
{
	gint ii;
	WebKitDOMHTMLCollection *collection = NULL;

	g_return_if_fail (element != NULL);

	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		element, "-x-evo-quoted");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;)
		remove_node (webkit_dom_html_collection_item (collection, ii));
	g_clear_object (&collection);

	collection = webkit_dom_element_get_elements_by_class_name_as_html_collection (
		element, "-x-evo-temp-br");
	for (ii = webkit_dom_html_collection_get_length (collection); ii--;)
		remove_node (webkit_dom_html_collection_item (collection, ii));
	g_clear_object (&collection);

	webkit_dom_node_normalize (WEBKIT_DOM_NODE (element));
}

WebKitDOMElement *
e_editor_dom_get_paragraph_element (EEditorPage *editor_page,
                                    gint width,
                                    gint offset)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	element = webkit_dom_document_create_element (document, "DIV", NULL);
	e_editor_dom_set_paragraph_style (editor_page, element, width, offset, NULL);

	return element;
}

WebKitDOMElement *
e_editor_dom_put_node_into_paragraph (EEditorPage *editor_page,
                                      WebKitDOMNode *node,
                                      gboolean with_input)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	WebKitDOMElement *container;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	range = webkit_dom_document_create_range (document);
	container = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
	webkit_dom_range_select_node (range, node, NULL);
	webkit_dom_range_surround_contents (range, WEBKIT_DOM_NODE (container), NULL);
	/* We have to move caret position inside this container */
	if (with_input)
		dom_add_selection_markers_into_element_end (document, container, NULL, NULL);

	g_clear_object (&range);

	return container;
}

static gint
selection_get_citation_level (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = webkit_dom_node_get_parent_node (node);
	gint level = 0;

	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		if (WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (parent) &&
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (parent), "type"))
			level++;

		parent = webkit_dom_node_get_parent_node (parent);
	}

	return level;
}

WebKitDOMElement *
e_editor_dom_wrap_paragraph_length (EEditorPage *editor_page,
                                    WebKitDOMElement *paragraph,
                                    gint length)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);
	g_return_val_if_fail (length >= MINIMAL_PARAGRAPH_WIDTH, NULL);

	return wrap_lines (editor_page, WEBKIT_DOM_NODE (paragraph), FALSE, length,
		e_editor_page_get_word_wrap_length (editor_page));
}

/*
 * e_html_editor_selection_wrap_lines:
 * @selection: an #EEditorSelection
 *
 * Wraps all lines in current selection to be 71 characters long.
 */

void
e_editor_dom_selection_wrap (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *next_block;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	gboolean after_selection_end = FALSE, html_mode;
	gint word_wrap_length;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);

	e_editor_dom_selection_save (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_WRAP;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = 1;
		ev->data.style.to = 1;
	}

	block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_editor_page_get_html_mode (editor_page);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean quoted = FALSE;
		gint citation_level, quote;
		WebKitDOMElement *wrapped_paragraph;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Don't try to wrap the 'Normal' blocks as they are already wrapped and*/
		/* also skip blocks that we already wrapped with this function. */
		if ((!html_mode && webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-evo-paragraph")) ||
		    webkit_dom_element_has_attribute (WEBKIT_DOM_ELEMENT (block), "data-user-wrapped")) {
			block = next_block;
			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		citation_level = selection_get_citation_level (block);
		quote = citation_level ? citation_level * 2 : 0;

		wrapped_paragraph = e_editor_dom_wrap_paragraph_length (
			editor_page, WEBKIT_DOM_ELEMENT (block), word_wrap_length - quote);

		webkit_dom_element_set_attribute (
			wrapped_paragraph, "data-user-wrapped", "", NULL);

		if (quoted && !html_mode)
			e_editor_dom_quote_plain_text_element (editor_page, wrapped_paragraph);

		block = next_block;
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_force_spell_check_in_viewport (editor_page);
}

void
e_editor_dom_wrap_paragraphs_in_document (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *list = NULL;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	list = webkit_dom_document_query_selector_all (
		document, "[data-evo-paragraph]:not(#-x-evo-input-start)", NULL);

	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		gint word_wrap_length, quote, citation_level;
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		citation_level = selection_get_citation_level (node);
		quote = citation_level ? citation_level * 2 : 0;
		word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);

		if (node_is_list (node)) {
			WebKitDOMNode *item = webkit_dom_node_get_first_child (node);

			while (item && WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
				e_editor_dom_wrap_paragraph_length (
					editor_page, WEBKIT_DOM_ELEMENT (item), word_wrap_length - quote);
				item = webkit_dom_node_get_next_sibling (item);
			}
		} else {
			e_editor_dom_wrap_paragraph_length (
				editor_page, WEBKIT_DOM_ELEMENT (node), word_wrap_length - quote);
		}
	}
	g_clear_object (&list);
}

WebKitDOMElement *
e_editor_dom_wrap_paragraph (EEditorPage *editor_page,
                             WebKitDOMElement *paragraph)
{
	gint indentation_level, citation_level, quote;
	gint word_wrap_length, final_width, offset = 0;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_ELEMENT (paragraph), NULL);

	indentation_level = get_indentation_level (paragraph);
	citation_level = selection_get_citation_level (WEBKIT_DOM_NODE (paragraph));

	if (node_is_list_or_item (WEBKIT_DOM_NODE (paragraph))) {
		gint list_level = get_list_level (WEBKIT_DOM_NODE (paragraph));
		indentation_level = 0;

		if (list_level > 0)
			offset = list_level * -SPACES_PER_LIST_LEVEL;
		else
			offset = -SPACES_PER_LIST_LEVEL;
	}

	quote = citation_level ? citation_level * 2 : 0;

	word_wrap_length = e_editor_page_get_word_wrap_length (editor_page);
	final_width = word_wrap_length - quote + offset;
	final_width -= SPACES_PER_INDENTATION * indentation_level;

	return e_editor_dom_wrap_paragraph_length (
		editor_page, WEBKIT_DOM_ELEMENT (paragraph), final_width);
}

static gboolean
get_has_style (EEditorPage *editor_page,
               const gchar *style_tag)
{
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	WebKitDOMRange *range = NULL;
	gboolean result;
	gint tag_len;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_start_container (range, NULL);
	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);
	g_clear_object (&range);

	tag_len = strlen (style_tag);
	result = FALSE;
	while (!result && element) {
		gchar *element_tag;
		gboolean accept_citation = FALSE;

		element_tag = webkit_dom_element_get_tag_name (element);

		if (g_ascii_strncasecmp (style_tag, "citation", 8) == 0) {
			accept_citation = TRUE;
			result = WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element);
			if (element_has_class (element, "-x-evo-indented"))
				result = FALSE;
		} else {
			result = ((tag_len == strlen (element_tag)) &&
				(g_ascii_strncasecmp (element_tag, style_tag, tag_len) == 0));
		}

		/* Special case: <blockquote type=cite> marks quotation, while
		 * just <blockquote> is used for indentation. If the <blockquote>
		 * has type=cite, then ignore it unless style_tag is "citation" */
		if (result && WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (element)) {
			if (webkit_dom_element_has_attribute (element, "type")) {
				gchar *type = webkit_dom_element_get_attribute (element, "type");
				if (!accept_citation && (type && g_ascii_strncasecmp (type, "cite", 4) == 0)) {
					result = FALSE;
				}
				g_free (type);
			} else {
				if (accept_citation)
					result = FALSE;
			}
		}

		g_free (element_tag);

		if (result)
			break;

		element = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (element));
	}

	return result;
}

typedef gboolean (*IsRightFormatNodeFunc) (WebKitDOMElement *element);

static gboolean
dom_selection_is_font_format (EEditorPage *editor_page,
                              IsRightFormatNodeFunc func,
                              gboolean *previous_value)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMNode *start, *end, *sibling;
	WebKitDOMRange *range = NULL;
	gboolean ret_val = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	if (!e_editor_page_get_html_mode (editor_page))
		goto out;

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection))
		goto out;

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	if (!range)
		goto out;

	if (webkit_dom_range_get_collapsed (range, NULL) && previous_value) {
		WebKitDOMNode *node;
		gchar* text_content;

		node = webkit_dom_range_get_common_ancestor_container (range, NULL);
		/* If we are changing the format of block we have to re-set the
		 * format property, otherwise it will be turned off because of no
		 * text in block. */
		text_content = webkit_dom_node_get_text_content (node);
		if (g_strcmp0 (text_content, "") == 0) {
			g_free (text_content);
			ret_val = *previous_value;
			goto out;
		}
		g_free (text_content);
	}

	/* Range without start or end point is a wrong range. */
	start = webkit_dom_range_get_start_container (range, NULL);
	end = webkit_dom_range_get_end_container (range, NULL);
	if (!start || !end)
		goto out;

	if (WEBKIT_DOM_IS_TEXT (start))
		start = webkit_dom_node_get_parent_node (start);
	while (start && WEBKIT_DOM_IS_ELEMENT (start) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (start)) {
		/* Find the start point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (start))) {
			ret_val = TRUE;
			break;
		}
		start = webkit_dom_node_get_parent_node (start);
	}

	/* Start point doesn't have the given formatting. */
	if (!ret_val)
		goto out;

	/* If the selection is collapsed, we can return early. */
	if (webkit_dom_range_get_collapsed (range, NULL))
		goto out;

	/* The selection is in the same node and that node is supposed to have
	 * the same formatting (otherwise it is split up with formatting element. */
	if (webkit_dom_node_is_same_node (
		webkit_dom_range_get_start_container (range, NULL),
		webkit_dom_range_get_end_container (range, NULL)))
		goto out;

	ret_val = FALSE;

	if (WEBKIT_DOM_IS_TEXT (end))
		end = webkit_dom_node_get_parent_node (end);
	while (end && WEBKIT_DOM_IS_ELEMENT (end) && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (end)) {
		/* Find the end point's parent node with given formatting. */
		if (func (WEBKIT_DOM_ELEMENT (end))) {
			ret_val = TRUE;
			break;
		}
		end = webkit_dom_node_get_parent_node (end);
	}

	if (!ret_val)
		goto out;

	ret_val = FALSE;

	/* Now go between the end points and check the inner nodes for format validity. */
	sibling = start;
	while ((sibling = webkit_dom_node_get_next_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, end)) {
			ret_val = TRUE;
			goto out;
		}

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	sibling = end;
	while ((sibling = webkit_dom_node_get_previous_sibling (sibling))) {
		if (webkit_dom_node_is_same_node (sibling, start))
			break;

		if (WEBKIT_DOM_IS_TEXT (sibling))
			goto out;
		else if (func (WEBKIT_DOM_ELEMENT (sibling)))
			continue;
		else if (webkit_dom_node_get_first_child (sibling)) {
			WebKitDOMNode *first_child;

			first_child = webkit_dom_node_get_first_child (sibling);
			if (!webkit_dom_node_get_next_sibling (first_child))
				if (WEBKIT_DOM_IS_ELEMENT (first_child) && func (WEBKIT_DOM_ELEMENT (first_child)))
					continue;
				else
					goto out;
			else
				goto out;
		} else
			goto out;
	}

	ret_val = TRUE;
 out:
	g_clear_object (&range);
	g_clear_object (&dom_selection);

	return ret_val;
}

static gboolean
is_underline_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "u");
}

/*
 * e_html_editor_selection_is_underline:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is underlined.
 *
 * Returns @TRUE when selection is underlined, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_underline (EEditorPage *editor_page)
{
	gboolean is_underline;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	is_underline = e_editor_page_get_underline (editor_page);
	is_underline = dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_underline_element, &is_underline);

	return is_underline;
}

static WebKitDOMElement *
set_font_style (WebKitDOMDocument *document,
                const gchar *element_name,
                gboolean value)
{
	WebKitDOMElement *element;
	WebKitDOMNode *parent, *clone = NULL;

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-selection-end-marker");
	parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
	if (value) {
		WebKitDOMNode *node;
		WebKitDOMElement *el;
		gchar *name;

		el = webkit_dom_document_create_element (document, element_name, NULL);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (el), UNICODE_ZERO_WIDTH_SPACE, NULL);

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), node, NULL);
		name = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (parent));
		if (g_ascii_strcasecmp (name, element_name) == 0 && g_ascii_strcasecmp (name, "font") != 0)
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				WEBKIT_DOM_NODE (el),
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		else
			webkit_dom_node_insert_before (
				parent,
				WEBKIT_DOM_NODE (el),
				WEBKIT_DOM_NODE (element),
				NULL);
		g_free (name);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (el), WEBKIT_DOM_NODE (element), NULL);

		return el;
	} else {
		gboolean no_sibling;
		WebKitDOMNode *node, *sibling;

		node = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (element));

		/* Turning the formatting in the middle of element. */
		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element));
		no_sibling = sibling &&
			!WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) &&
			!webkit_dom_node_get_next_sibling (sibling);

		if (no_sibling) {
			gboolean do_clone = TRUE;
			gchar *text_content = NULL;
			WebKitDOMNode *child;

			if ((text_content = webkit_dom_node_get_text_content (parent)) &&
			    (g_strcmp0 (text_content, UNICODE_ZERO_WIDTH_SPACE) == 0))
				do_clone = FALSE;

			g_free (text_content);

			if (do_clone) {
				clone = webkit_dom_node_clone_node_with_error (
					WEBKIT_DOM_NODE (parent), FALSE, NULL);

				while ((child = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element))))
					webkit_dom_node_insert_before (
						clone,
						child,
						webkit_dom_node_get_first_child (clone),
						NULL);

				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (parent),
					clone,
					webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
					NULL);
			}
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (parent),
			NULL);
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (parent),
			node,
			webkit_dom_node_get_next_sibling (parent),
			NULL);

		if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (sibling) && !no_sibling) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (parent),
				node,
				webkit_dom_node_get_next_sibling (parent),
				NULL);
		}

		if (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (webkit_dom_node_get_parent_node (parent))) {
			WebKitDOMNode *first_child;

			if ((first_child = webkit_dom_node_get_first_child (parent))) {
				gchar *text_content = NULL;

				text_content = webkit_dom_node_get_text_content (first_child);

				if (g_strcmp0 (text_content, UNICODE_ZERO_WIDTH_SPACE) != 0)
					webkit_dom_element_insert_adjacent_text (
						WEBKIT_DOM_ELEMENT (parent),
						"afterend",
						UNICODE_ZERO_WIDTH_SPACE,
						NULL);

				g_free (text_content);
			}

			remove_node_if_empty (parent);
			remove_node_if_empty (clone);
		}
	}

	return NULL;
}

static void
selection_set_font_style (EEditorPage *editor_page,
                          EContentEditorCommand command,
                          gboolean value)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		if (command == E_CONTENT_EDITOR_COMMAND_BOLD)
			ev->type = HISTORY_BOLD;
		else if (command == E_CONTENT_EDITOR_COMMAND_ITALIC)
			ev->type = HISTORY_ITALIC;
		else if (command == E_CONTENT_EDITOR_COMMAND_UNDERLINE)
			ev->type = HISTORY_UNDERLINE;
		else if (command == E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH)
			ev->type = HISTORY_STRIKETHROUGH;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = !value;
		ev->data.style.to = value;
	}

	if (e_editor_dom_selection_is_collapsed (editor_page)) {
		const gchar *element_name = NULL;

		if (command == E_CONTENT_EDITOR_COMMAND_BOLD)
			element_name = "b";
		else if (command == E_CONTENT_EDITOR_COMMAND_ITALIC)
			element_name = "i";
		else if (command == E_CONTENT_EDITOR_COMMAND_UNDERLINE)
			element_name = "u";
		else if (command == E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH)
			element_name = "strike";

		if (element_name)
			set_font_style (e_editor_page_get_document (editor_page), element_name, value);
		e_editor_dom_selection_restore (editor_page);

		goto exit;
	}
	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_exec_command (editor_page, command, NULL);
exit:
	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
}

/*
 * e_html_editor_selection_set_underline:
 * @selection: an #EEditorSelection
 * @underline: @TRUE to enable underline, @FALSE to disable
 *
 * Toggles underline formatting of current selection or letter at current
 * cursor position, depending on whether @underline is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_underline (EEditorPage *editor_page,
                                      gboolean underline)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_underline (editor_page) == underline)
		return;

	selection_set_font_style (
		editor_page, E_CONTENT_EDITOR_COMMAND_UNDERLINE, underline);
}

static gboolean
is_subscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sub");
}

/*
 * e_html_editor_selection_is_subscript:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in subscript.
 *
 * Returns @TRUE when selection is in subscript, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_subscript (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_subscript_element, NULL);
}

/*
 * e_html_editor_selection_set_subscript:
 * @selection: an #EEditorSelection
 * @subscript: @TRUE to enable subscript, @FALSE to disable
 *
 * Toggles subscript of current selection or letter at current cursor position,
 * depending on whether @subscript is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_subscript (EEditorPage *editor_page,
                                      gboolean subscript)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_subscript (editor_page) == subscript)
		return;

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_SUBSCRIPT, NULL);
}

static gboolean
is_superscript_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "sup");
}

/*
 * e_html_editor_selection_is_superscript:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is in superscript.
 *
 * Returns @TRUE when selection is in superscript, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_superscript (EEditorPage *editor_page)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	return dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_superscript_element, NULL);
}

/*
 * e_html_editor_selection_set_superscript:
 * @selection: an #EEditorSelection
 * @superscript: @TRUE to enable superscript, @FALSE to disable
 *
 * Toggles superscript of current selection or letter at current cursor position,
 * depending on whether @superscript is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_superscript (EEditorPage *editor_page,
                                        gboolean superscript)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_superscript (editor_page) == superscript)
		return;

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT, NULL);
}

static gboolean
is_strikethrough_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "strike");
}

/*
 * e_html_editor_selection_is_strikethrough:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is striked through.
 *
 * Returns @TRUE when selection is striked through, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_strikethrough (EEditorPage *editor_page)
{
	gboolean is_strikethrough;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	is_strikethrough = e_editor_page_get_strikethrough (editor_page);
	is_strikethrough = dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_strikethrough_element, &is_strikethrough);

	return is_strikethrough;
}

/*
 * e_html_editor_selection_set_strikethrough:
 * @selection: an #EEditorSelection
 * @strikethrough: @TRUE to enable strikethrough, @FALSE to disable
 *
 * Toggles strike through formatting of current selection or letter at current
 * cursor position, depending on whether @strikethrough is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_strikethrough (EEditorPage *editor_page,
                                          gboolean strikethrough)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_strikethrough (editor_page) == strikethrough)
		return;

	selection_set_font_style (
		editor_page, E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH, strikethrough);
}

static gboolean
is_monospace_element (WebKitDOMElement *element)
{
	gchar *value;
	gboolean ret_val = FALSE;

	if (!element)
		return FALSE;

	if (!WEBKIT_DOM_IS_HTML_FONT_ELEMENT (element))
		return FALSE;

	value = webkit_dom_element_get_attribute (element, "face");
	if (value && g_strcmp0 (value, "monospace") == 0)
		ret_val = TRUE;

	g_free (value);

	return ret_val;
}

/*
 * e_html_editor_selection_is_monospaced:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is monospaced.
 *
 * Returns @TRUE when selection is monospaced, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_monospace (EEditorPage *editor_page)
{
	gboolean is_monospace;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	is_monospace = e_editor_page_get_monospace (editor_page);
	is_monospace = dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_monospace_element, &is_monospace);

	return is_monospace;
}

static void
monospace_selection (EEditorPage *editor_page,
                     WebKitDOMElement *monospace_element)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *sibling, *node, *monospace, *block;
	WebKitDOMNodeList *list = NULL;
	gboolean selection_end = FALSE;
	gboolean first = TRUE;
	gint ii;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	monospace = WEBKIT_DOM_NODE (monospace_element);
	node = WEBKIT_DOM_NODE (selection_start_marker);
	/* Go through first block in selection. */
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		if (webkit_dom_node_get_next_sibling (node)) {
			/* Prepare the monospaced element. */
			monospace = webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (node),
				first ? monospace : webkit_dom_node_clone_node_with_error (monospace, FALSE, NULL),
				first ? node : webkit_dom_node_get_next_sibling (node),
				NULL);
		} else
			break;

		/* Move the nodes into monospaced element. */
		while (((sibling = webkit_dom_node_get_next_sibling (monospace)))) {
			webkit_dom_node_append_child (monospace, sibling, NULL);
			if (webkit_dom_node_is_same_node (WEBKIT_DOM_NODE (selection_end_marker), sibling)) {
				selection_end = TRUE;
				break;
			}
		}

		node = webkit_dom_node_get_parent_node (monospace);
		first = FALSE;
	}

	/* Just one block was selected. */
	if (selection_end)
		goto out;

	/* Middle blocks (blocks not containing the end of the selection. */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		monospace = webkit_dom_node_insert_before (
			block,
			webkit_dom_node_clone_node_with_error (monospace, FALSE, NULL),
			webkit_dom_node_get_first_child (block),
			NULL);

		while (((sibling = webkit_dom_node_get_next_sibling (monospace))))
			webkit_dom_node_append_child (monospace, sibling, NULL);

		block = next_block;
	}

	/* Block containing the end of selection. */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	while (block && node && !webkit_dom_node_is_same_node (block, node)) {
		monospace = webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (node),
			webkit_dom_node_clone_node_with_error (monospace, FALSE, NULL),
			webkit_dom_node_get_next_sibling (node),
			NULL);

		while (((sibling = webkit_dom_node_get_previous_sibling (monospace)))) {
			webkit_dom_node_insert_before (
				monospace,
				sibling,
				webkit_dom_node_get_first_child (monospace),
				NULL);
		}

		node = webkit_dom_node_get_parent_node (monospace);
	}
 out:
	/* Merge all the monospace elements inside other monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] > font[face=monospace]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		while ((child = webkit_dom_node_get_first_child (item))) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (item),
				child,
				item,
				NULL);
		}
		remove_node (item);
	}
	g_clear_object (&list);

	/* Merge all the adjacent monospace elements. */
	list = webkit_dom_document_query_selector_all (
		document, "font[face=monospace] + font[face=monospace]", NULL);
	for (ii = webkit_dom_node_list_get_length (list); ii--;) {
		WebKitDOMNode *item;
		WebKitDOMNode *child;

		item = webkit_dom_node_list_item (list, ii);
		/* The + CSS selector will return some false positives as it doesn't
		 * take text between elements into account so it will return this:
		 * <font face="monospace">xx</font>yy<font face="monospace">zz</font>
		 * as valid, but it isn't so we have to check if previous node
		 * is indeed element or not. */
		if (WEBKIT_DOM_IS_ELEMENT (webkit_dom_node_get_previous_sibling (item))) {
			while ((child = webkit_dom_node_get_first_child (item))) {
				webkit_dom_node_append_child (
					webkit_dom_node_get_previous_sibling (item), child, NULL);
			}
			remove_node (item);
		}
	}
	g_clear_object (&list);

	e_editor_dom_selection_restore (editor_page);
}

static void
unmonospace_selection (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker;
	WebKitDOMElement *selection_end_marker;
	WebKitDOMElement *selection_start_clone;
	WebKitDOMElement *selection_end_clone;
	WebKitDOMNode *sibling, *node;
	WebKitDOMNode *block, *clone, *monospace;
	gboolean selection_end = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	block = WEBKIT_DOM_NODE (get_parent_block_element (WEBKIT_DOM_NODE (selection_start_marker)));

	node = WEBKIT_DOM_NODE (selection_start_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_start_marker));
	while (monospace && !is_monospace_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection start node. */
	if (!monospace)
		goto out;

	/* Make a clone of current monospaced element. */
	clone = webkit_dom_node_clone_node_with_error (monospace, TRUE, NULL);

	/* First block */
	/* Remove all the nodes that are after the selection start point as they
	 * will be in the cloned node. */
	while (monospace && node && !webkit_dom_node_is_same_node (monospace, node)) {
		WebKitDOMNode *tmp;
		while (((sibling = webkit_dom_node_get_next_sibling (node))))
			remove_node (sibling);

		tmp = webkit_dom_node_get_parent_node (node);
		if (webkit_dom_node_get_next_sibling (node))
			remove_node (node);
		node = tmp;
	}

	selection_start_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-start-marker", NULL);
	selection_end_clone = webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (clone), "#-x-evo-selection-end-marker", NULL);

	/* No selection start node in the block where it is supposed to be, return. */
	if (!selection_start_clone)
		goto out;

	/* Remove all the nodes until we hit the selection start point as these
	 * nodes will stay monospaced and they are already in original element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_start_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				remove_node (node);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_start_clone)))
			break;
		else
			remove_node (node);

		node = next_sibling;
	}

	/* Insert the clone into the tree. Do it after the previous clean up. If
	 * we would do it the other way the line would contain duplicated text nodes
	 * and the block would be expading and shrinking while we would modify it. */
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (monospace),
		clone,
		webkit_dom_node_get_next_sibling (monospace),
		NULL);

	/* Move selection start point the right place. */
	remove_node (WEBKIT_DOM_NODE (selection_start_marker));
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (clone),
		WEBKIT_DOM_NODE (selection_start_clone),
		clone,
		NULL);

	/* Move all the nodes the are supposed to lose the monospace formatting
	 * out of monospaced element. */
	node = webkit_dom_node_get_first_child (clone);
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (selection_end_clone &&
			    webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_clone))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (selection_end_clone &&
			   webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_clone))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);

		node = next_sibling;
	}

	remove_node_if_empty (clone);
	remove_node_if_empty (monospace);

	/* Just one block was selected and we hit the selection end point. */
	if (selection_end)
		goto out;

	/* Middle blocks */
	block = webkit_dom_node_get_next_sibling (block);
	while (block && !selection_end) {
		WebKitDOMNode *next_block, *child, *parent;
		WebKitDOMElement *monospace_element;

		selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (selection_end)
			break;

		next_block = webkit_dom_node_get_next_sibling (block);

		/* Find the monospaced element and move all the nodes from it and
		 * finally remove it. */
		monospace_element = webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "font[face=monospace]", NULL);
		if (!monospace_element)
			break;

		parent = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (monospace_element));
		while  ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (monospace_element)))) {
			webkit_dom_node_insert_before (
				parent, child, WEBKIT_DOM_NODE (monospace_element), NULL);
		}

		remove_node (WEBKIT_DOM_NODE (monospace_element));

		block = next_block;
	}

	/* End block */
	node = WEBKIT_DOM_NODE (selection_end_marker);
	monospace = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (selection_end_marker));
	while (monospace && !is_monospace_element (WEBKIT_DOM_ELEMENT (monospace)))
		monospace = webkit_dom_node_get_parent_node (monospace);

	/* No monospaced element was found as a parent of selection end node. */
	if (!monospace)
		return;

	clone = WEBKIT_DOM_NODE (monospace);
	node = webkit_dom_node_get_first_child (clone);
	/* Move all the nodes that are supposed to lose the monospaced formatting
	 * out of the monospaced element. */
	while (node) {
		WebKitDOMNode *next_sibling;

		next_sibling = webkit_dom_node_get_next_sibling (node);
		if (webkit_dom_node_get_first_child (node)) {
			if (webkit_dom_node_contains (node, WEBKIT_DOM_NODE (selection_end_marker))) {
				node = webkit_dom_node_get_first_child (node);
				continue;
			} else
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (clone),
					node,
					clone,
					NULL);
		} else if (webkit_dom_node_is_same_node (node, WEBKIT_DOM_NODE (selection_end_marker))) {
			selection_end = TRUE;
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
			break;
		} else {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (clone),
				node,
				clone,
				NULL);
		}

		node = next_sibling;
	}

	remove_node_if_empty (clone);
 out:
	e_editor_dom_selection_restore (editor_page);
}

/*
 * e_html_editor_selection_set_monospaced:
 * @selection: an #EEditorSelection
 * @monospaced: @TRUE to enable monospaced, @FALSE to disable
 *
 * Toggles monospaced formatting of current selection or letter at current cursor
 * position, depending on whether @monospaced is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_monospace (EEditorPage *editor_page,
                                      gboolean value)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	guint font_size = 0;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if ((e_editor_dom_selection_is_monospace (editor_page) ? 1 : 0) == (value ? 1 : 0))
		return;

	document = e_editor_page_get_document (editor_page);
	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return;

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_MONOSPACE;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = !value;
		ev->data.style.to = value;
	}

	font_size = e_editor_page_get_font_size (editor_page);
	if (font_size == 0)
		font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;

	if (value) {
		WebKitDOMElement *monospace;

		monospace = webkit_dom_document_create_element (
			document, "font", NULL);
		webkit_dom_element_set_attribute (
			monospace, "face", "monospace", NULL);
		if (font_size != 0) {
			gchar *font_size_str;

			font_size_str = g_strdup_printf ("%d", font_size);
			webkit_dom_element_set_attribute (
				monospace, "size", font_size_str, NULL);
			g_free (font_size_str);
		}

		if (!webkit_dom_range_get_collapsed (range, NULL))
			monospace_selection (editor_page, monospace);
		else {
			/* https://bugs.webkit.org/show_bug.cgi?id=15256 */
			webkit_dom_element_set_inner_html (
				monospace,
				UNICODE_ZERO_WIDTH_SPACE,
				NULL);
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (monospace), NULL);

			e_editor_dom_move_caret_into_element (editor_page, monospace, FALSE);
		}
	} else {
		gboolean is_bold = FALSE, is_italic = FALSE;
		gboolean is_underline = FALSE, is_strikethrough = FALSE;
		WebKitDOMElement *tt_element;
		WebKitDOMNode *node;

		node = webkit_dom_range_get_end_container (range, NULL);
		if (WEBKIT_DOM_IS_ELEMENT (node) &&
		    is_monospace_element (WEBKIT_DOM_ELEMENT (node))) {
			tt_element = WEBKIT_DOM_ELEMENT (node);
		} else {
			tt_element = dom_node_find_parent_element (node, "FONT");

			if (!is_monospace_element (tt_element)) {
				g_clear_object (&range);
				g_free (ev);
				return;
			}
		}

		/* Save current formatting */
		is_bold = e_editor_page_get_bold (editor_page);
		is_italic = e_editor_page_get_italic (editor_page);
		is_underline = e_editor_page_get_underline (editor_page);
		is_strikethrough = e_editor_page_get_strikethrough (editor_page);

		if (!e_editor_dom_selection_is_collapsed (editor_page))
			unmonospace_selection (editor_page);
		else {
			e_editor_dom_selection_save (editor_page);
			set_font_style (document, "", FALSE);
			e_editor_dom_selection_restore (editor_page);
		}

		/* Re-set formatting */
		if (is_bold)
			e_editor_dom_selection_set_bold (editor_page, TRUE);
		if (is_italic)
			e_editor_dom_selection_set_italic (editor_page, TRUE);
		if (is_underline)
			e_editor_dom_selection_set_underline (editor_page, TRUE);
		if (is_strikethrough)
			e_editor_dom_selection_set_strikethrough (editor_page, TRUE);

		if (font_size)
			e_editor_dom_selection_set_font_size (editor_page, font_size);
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	g_clear_object (&range);
}

static gboolean
is_bold_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	if (element_has_tag (element, "b"))
		return TRUE;

	/* Headings are bold by default */
	return WEBKIT_DOM_IS_HTML_HEADING_ELEMENT (element);
}

/*
 * e_html_editor_selection_is_bold:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is bold.
 *
 * Returns @TRUE when selection is bold, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_bold (EEditorPage *editor_page)
{
	gboolean is_bold;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	is_bold = e_editor_page_get_bold (editor_page);
	is_bold = dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_bold_element, &is_bold);

	return is_bold;
}

/*
 * e_html_editor_selection_set_bold:
 * @selection: an #EEditorSelection
 * @bold: @TRUE to enable bold, @FALSE to disable
 *
 * Toggles bold formatting of current selection or letter at current cursor
 * position, depending on whether @bold is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_bold (EEditorPage *editor_page,
                                 gboolean bold)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_bold (editor_page) == bold)
		return;

	selection_set_font_style (
		editor_page, E_CONTENT_EDITOR_COMMAND_BOLD, bold);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
}

static gboolean
is_italic_element (WebKitDOMElement *element)
{
	if (!element || !WEBKIT_DOM_IS_ELEMENT (element))
		return FALSE;

	return element_has_tag (element, "i") || element_has_tag (element, "address");
}

/*
 * e_html_editor_selection_is_italic:
 * @selection: an #EEditorSelection
 *
 * Returns whether current selection or letter at current cursor position
 * is italic.
 *
 * Returns @TRUE when selection is italic, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_italic (EEditorPage *editor_page)
{
	gboolean is_italic;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	is_italic = e_editor_page_get_italic (editor_page);
	is_italic = dom_selection_is_font_format (
		editor_page, (IsRightFormatNodeFunc) is_italic_element, &is_italic);

	return is_italic;
}

/*
 * e_html_editor_selection_set_italic:
 * @selection: an #EEditorSelection
 * @italic: @TRUE to enable italic, @FALSE to disable
 *
 * Toggles italic formatting of current selection or letter at current cursor
 * position, depending on whether @italic is @TRUE or @FALSE.
 */
void
e_editor_dom_selection_set_italic (EEditorPage *editor_page,
                                   gboolean italic)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	if (e_editor_dom_selection_is_italic (editor_page) == italic)
		return;

	selection_set_font_style (
		editor_page, E_CONTENT_EDITOR_COMMAND_ITALIC, italic);
}

/*
 * e_html_editor_selection_is_indented:
 * @selection: an #EEditorSelection
 *
 * Returns whether current paragraph is indented. This does not include
 * citations.  To check, whether paragraph is a citation, use
 * e_html_editor_selection_is_citation().
 *
 * Returns: @TRUE when current paragraph is indented, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_indented (EEditorPage *editor_page)
{
	WebKitDOMElement *element;
	WebKitDOMRange *range = NULL;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return FALSE;

	if (webkit_dom_range_get_collapsed (range, NULL)) {
		element = get_element_for_inspection (range);
		g_clear_object (&range);
		return element_has_class (element, "-x-evo-indented");
	} else {
		WebKitDOMNode *node;
		gboolean ret_val;

		node = webkit_dom_range_get_end_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");
		if (!ret_val)
			goto out;

		node = webkit_dom_range_get_start_container (range, NULL);
		/* No selection or whole body selected */
		if (WEBKIT_DOM_IS_HTML_BODY_ELEMENT (node))
			goto out;

		element = WEBKIT_DOM_ELEMENT (get_parent_indented_block (node));
		ret_val = element_has_class (element, "-x-evo-indented");

		g_clear_object (&range);

		return ret_val;
	}

 out:
	g_clear_object (&range);

	return FALSE;
}

/*
 * e_html_editor_selection_is_citation:
 * @selection: an #EEditorSelection
 *
 * Returns whether current paragraph is a citation.
 *
 * Returns: @TRUE when current paragraph is a citation, @FALSE otherwise.
 */
gboolean
e_editor_dom_selection_is_citation (EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	gboolean ret_val;
	gchar *value, *text_content;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return FALSE;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_clear_object (&range);

	if (WEBKIT_DOM_IS_TEXT (node))
		return get_has_style (editor_page, "citation");

	text_content = webkit_dom_node_get_text_content (node);
	if (g_strcmp0 (text_content, "") == 0) {
		g_free (text_content);
		return FALSE;
	}
	g_free (text_content);

	value = webkit_dom_element_get_attribute (WEBKIT_DOM_ELEMENT (node), "type");
	/* citation == <blockquote type='cite'> */
	if (value && strstr (value, "cite"))
		ret_val = TRUE;
	else
		ret_val = get_has_style (editor_page, "citation");

	g_free (value);
	return ret_val;
}

static gchar *
get_font_property (EEditorPage *editor_page,
                   const gchar *font_property)
{
	WebKitDOMRange *range = NULL;
	WebKitDOMNode *node;
	WebKitDOMElement *element;
	gchar *value;

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return NULL;

	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_clear_object (&range);
	element = dom_node_find_parent_element (node, "FONT");
	while (element && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (element) &&
	       !webkit_dom_element_has_attribute (element, font_property)) {
		element = dom_node_find_parent_element (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)), "FONT");
	}

	if (!element)
		return NULL;

	g_object_get (G_OBJECT (element), font_property, &value, NULL);

	return value;
}

/*
 * e_editor_dom_selection_get_font_size:
 * @selection: an #EEditorSelection
 *
 * Returns point size of current selection or of letter at current cursor position.
 */
guint
e_editor_dom_selection_get_font_size (EEditorPage *editor_page)
{
	gchar *size;
	guint size_int;
	gboolean increment;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	size = get_font_property (editor_page, "size");
	if (!(size && *size)) {
		g_free (size);
		return E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	}

	/* We don't support increments, but when going through a content that
	 * was not written in Evolution we can find it. In this case just report
	 * the normal size. */
	/* FIXME: go through all parent and get the right value. */
	increment = size[0] == '+' || size[0] == '-';
	size_int = atoi (size);
	g_free (size);

	if (increment || size_int == 0)
		return E_CONTENT_EDITOR_FONT_SIZE_NORMAL;

	return size_int;
}

/*
 * e_html_editor_selection_set_font_size:
 * @selection: an #EEditorSelection
 * @font_size: point size to apply
 *
 * Sets font size of current selection or of letter at current cursor position
 * to @font_size.
 */
void
e_editor_dom_selection_set_font_size (EEditorPage *editor_page,
                                      EContentEditorFontSize font_size)
{
	WebKitDOMDocument *document;
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	gchar *size_str;
	guint current_font_size;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	current_font_size = e_editor_dom_selection_get_font_size (editor_page);
	if (current_font_size == font_size)
		return;

	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_FONT_SIZE;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = current_font_size;
		ev->data.style.to = font_size;
	}

	size_str = g_strdup_printf ("%d", font_size);

	if (e_editor_dom_selection_is_collapsed (editor_page)) {
		WebKitDOMElement *font;

		font = set_font_style (document, "font", font_size != 3);
		if (font)
			webkit_dom_element_set_attribute (font, "size", size_str, NULL);
		e_editor_dom_selection_restore (editor_page);
		goto exit;
	}

	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_FONT_SIZE, size_str);

	/* Text in <font size="3"></font> (size 3 is our default size) is a little
	 * bit smaller than font outsize it. So move it outside of it. */
	if (font_size == E_CONTENT_EDITOR_FONT_SIZE_NORMAL) {
		WebKitDOMElement *element;

		element = webkit_dom_document_query_selector (document, "font[size=\"3\"]", NULL);
		if (element) {
			WebKitDOMNode *child;

			while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (element))))
				webkit_dom_node_insert_before (
					webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
					child,
					WEBKIT_DOM_NODE (element),
					NULL);

			remove_node (WEBKIT_DOM_NODE (element));
		}
	}

 exit:
	g_free (size_str);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

/*
 * e_html_editor_selection_set_font_name:
 * @selection: an #EEditorSelection
 * @font_name: a font name to apply
 *
 * Sets font name of current selection or of letter at current cursor position
 * to @font_name.
 */
void
e_editor_dom_selection_set_font_name (EEditorPage *editor_page,
                                      const gchar *font_name)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_FONT_NAME, font_name);
}

/*
 * e_html_editor_selection_get_font_name:
 * @selection: an #EEditorSelection
 *
 * Returns name of font used in current selection or at letter at current cursor
 * position.
 *
 * Returns: A string with font name. [transfer-none]
 */
gchar *
e_editor_dom_selection_get_font_name (EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	WebKitDOMCSSStyleDeclaration *css = NULL;
	gchar *value;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	range = e_editor_dom_get_current_range (editor_page);
	node = webkit_dom_range_get_common_ancestor_container (range, NULL);
	g_clear_object (&range);

	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (node));
	value = webkit_dom_css_style_declaration_get_property_value (css, "fontFamily");
	g_clear_object (&css);

	return value;
}

/*
 * e_html_editor_selection_set_font_color:
 * @selection: an #EEditorSelection
 * @rgba: a #GdkRGBA
 *
 * Sets font color of current selection or letter at current cursor position to
 * color defined in @rgba.
 */
void
e_editor_dom_selection_set_font_color (EEditorPage *editor_page,
                                       const gchar *color)
{
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_FONT_COLOR;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.string.from = g_strdup (e_editor_page_get_font_color (editor_page));
		ev->data.string.to = g_strdup (color);
	}

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_FORE_COLOR, color);

	if (ev) {
		ev->after.start.x = ev->before.start.x;
		ev->after.start.y = ev->before.start.y;
		ev->after.end.x = ev->before.end.x;
		ev->after.end.y = ev->before.end.y;

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

/*
 * e_html_editor_selection_get_font_color:
 * @selection: an #EEditorSelection
 * @rgba: a #GdkRGBA object to be set to current font color
 *
 * Sets @rgba to contain color of current text selection or letter at current
 * cursor position.
 */
gchar *
e_editor_dom_selection_get_font_color (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	gchar *color;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	color = get_font_property (editor_page, "color");
	if (!(color && *color)) {
		WebKitDOMHTMLElement *body;

		body = webkit_dom_document_get_body (document);
		g_free (color);
		color = webkit_dom_html_body_element_get_text (WEBKIT_DOM_HTML_BODY_ELEMENT (body));
		if (!(color && *color)) {
			g_free (color);
			return g_strdup ("#000000");
		}
	}

	return color;
}

/*
 * e_html_editor_selection_get_block_format:
 * @selection: an #EEditorSelection
 *
 * Returns block format of current paragraph.
 *
 * Returns: #EContentEditorBlockFormat
 */
EContentEditorBlockFormat
e_editor_dom_selection_get_block_format (EEditorPage *editor_page)
{
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	WebKitDOMElement *element;
	EContentEditorBlockFormat result;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), E_CONTENT_EDITOR_BLOCK_FORMAT_NONE);

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;

	node = webkit_dom_range_get_start_container (range, NULL);

	if ((element = dom_node_find_parent_element (node, "UL"))) {
		WebKitDOMElement *tmp_element;

		tmp_element = dom_node_find_parent_element (node, "OL");
		if (tmp_element) {
			if (webkit_dom_node_contains (WEBKIT_DOM_NODE (tmp_element), WEBKIT_DOM_NODE (element)))
				result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (element));
			else
				result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (tmp_element));
		} else
			result = E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST;
	} else if ((element = dom_node_find_parent_element (node, "OL")) != NULL) {
		WebKitDOMElement *tmp_element;

		tmp_element = dom_node_find_parent_element (node, "UL");
		if (tmp_element) {
			if (webkit_dom_node_contains (WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (tmp_element)))
				result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (element));
			else
				result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (tmp_element));
		} else
			result = dom_get_list_format_from_node (WEBKIT_DOM_NODE (element));
	} else if (dom_node_find_parent_element (node, "PRE")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_PRE;
	} else if (dom_node_find_parent_element (node, "ADDRESS")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS;
	} else if (dom_node_find_parent_element (node, "H1")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H1;
	} else if (dom_node_find_parent_element (node, "H2")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H2;
	} else if (dom_node_find_parent_element (node, "H3")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H3;
	} else if (dom_node_find_parent_element (node, "H4")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H4;
	} else if (dom_node_find_parent_element (node, "H5")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H5;
	} else if (dom_node_find_parent_element (node, "H6")) {
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_H6;
	} else {
		/* Everything else is a paragraph (normal block) for us */
		result = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	}

	g_clear_object (&range);

	return result;
}

static void
change_leading_space_to_nbsp (WebKitDOMNode *block)
{
	WebKitDOMNode *child;

	if (!WEBKIT_DOM_IS_HTML_PRE_ELEMENT (block))
		return;

	if ((child = webkit_dom_node_get_first_child (block)) &&
	     WEBKIT_DOM_IS_CHARACTER_DATA (child)) {
		gchar *data;

		data = webkit_dom_character_data_substring_data (
			WEBKIT_DOM_CHARACTER_DATA (child), 0, 1, NULL);

		if (data && *data == ' ')
			webkit_dom_character_data_replace_data (
				WEBKIT_DOM_CHARACTER_DATA (child), 0, 1, UNICODE_NBSP, NULL);
		g_free (data);
	}
}

static void
change_trailing_space_in_block_to_nbsp (WebKitDOMNode *block)
{
	WebKitDOMNode *child;

	if ((child = webkit_dom_node_get_last_child (block)) &&
	    WEBKIT_DOM_IS_CHARACTER_DATA (child)) {
		gchar *tmp;
		gulong length;

		length = webkit_dom_character_data_get_length (
			WEBKIT_DOM_CHARACTER_DATA (child));

		tmp = webkit_dom_character_data_substring_data (
			WEBKIT_DOM_CHARACTER_DATA (child), length - 1, 1, NULL);
		if (tmp && *tmp == ' ') {
			webkit_dom_character_data_replace_data (
				WEBKIT_DOM_CHARACTER_DATA (child),
				length - 1,
				1,
				UNICODE_NBSP,
				NULL);
		}
		g_free (tmp);
	}
}

static void
change_space_before_selection_to_nbsp (WebKitDOMNode *node)
{
	WebKitDOMNode *prev_sibling;

	if ((prev_sibling = webkit_dom_node_get_previous_sibling (node))) {
		if (WEBKIT_DOM_IS_CHARACTER_DATA (prev_sibling)) {
			gchar *tmp;
			gulong length;

			length = webkit_dom_character_data_get_length (
				WEBKIT_DOM_CHARACTER_DATA (prev_sibling));

			tmp = webkit_dom_character_data_substring_data (
				WEBKIT_DOM_CHARACTER_DATA (prev_sibling), length - 1, 1, NULL);
			if (tmp && *tmp == ' ') {
				webkit_dom_character_data_replace_data (
					WEBKIT_DOM_CHARACTER_DATA (prev_sibling),
					length - 1,
					1,
					UNICODE_NBSP,
					NULL);
			}
			g_free (tmp);
		}
	}
}

static gboolean
process_block_to_block (EEditorPage *editor_page,
                        EContentEditorBlockFormat format,
                        const gchar *value,
                        WebKitDOMNode *block,
                        WebKitDOMNode *end_block,
                        WebKitDOMNode *blockquote,
                        gboolean html_mode)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *next_block;
	gboolean after_selection_end = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	while (!after_selection_end && block) {
		gboolean quoted = FALSE;
		gboolean empty = FALSE;
		gchar *content;
		gint citation_level = 0;
		WebKitDOMNode *child;
		WebKitDOMElement *element;

		if (e_editor_dom_node_is_citation_node (block)) {
			gboolean finished;

			next_block = webkit_dom_node_get_next_sibling (block);
			finished = process_block_to_block (
				editor_page,
				format,
				value,
				webkit_dom_node_get_first_child (block),
				end_block,
				blockquote,
				html_mode);

			if (finished)
				return TRUE;

			block = next_block;

			continue;
		}

		if (webkit_dom_element_query_selector (
			WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
			quoted = TRUE;
			e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));
		}

		if (!html_mode)
			e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));

		after_selection_end = webkit_dom_node_is_same_node (block, end_block);

		next_block = webkit_dom_node_get_next_sibling (block);

		if (node_is_list (block)) {
			WebKitDOMNode *item;

			item = webkit_dom_node_get_first_child (block);
			while (item && !WEBKIT_DOM_IS_HTML_LI_ELEMENT (item))
				item = webkit_dom_node_get_first_child (item);

			if (item && do_format_change_list_to_block (editor_page, format, item, value))
				return TRUE;

			block = next_block;

			continue;
		}

		if (format == E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH)
			element = e_editor_dom_get_paragraph_element (editor_page, -1, 0);
		else
			element = webkit_dom_document_create_element (
				document, value, NULL);

		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		change_leading_space_to_nbsp (block);
		change_trailing_space_in_block_to_nbsp (block);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), child, NULL);
		}

		if (empty) {
			WebKitDOMElement *br;

			br = webkit_dom_document_create_element (
				document, "BR", NULL);
			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (element), WEBKIT_DOM_NODE (br), NULL);
		}

		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (element),
			block,
			NULL);

		remove_node (block);

		if (!next_block && !after_selection_end) {
			citation_level = selection_get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				next_block = webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element));
				next_block = webkit_dom_node_get_next_sibling (next_block);
			}
		}

		block = next_block;

		if (!html_mode && format == E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH) {
			citation_level = selection_get_citation_level (WEBKIT_DOM_NODE (element));

			if (citation_level > 0) {
				gint quote, word_wrap_length;

				word_wrap_length =
					e_editor_page_get_word_wrap_length (editor_page);
				quote = citation_level * 2;

				element = e_editor_dom_wrap_paragraph_length (
					editor_page, element, word_wrap_length - quote);

			}
		}

		if (!html_mode && quoted) {
			if (citation_level > 0)
				e_editor_dom_quote_plain_text_element_after_wrapping (
					editor_page, element, citation_level);
			else
				e_editor_dom_quote_plain_text_element (editor_page, element);
		}
	}

	return after_selection_end;
}

static void
format_change_block_to_block (EEditorPage *editor_page,
                              EContentEditorBlockFormat format,
                              const gchar *value)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block, *end_block, *blockquote = NULL;
	gboolean html_mode = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	html_mode = e_editor_page_get_html_mode (editor_page);

	end_block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	/* Process all blocks that are in the selection one by one */
	process_block_to_block (
		editor_page, format, value, block, end_block, blockquote, html_mode);
}

static void
format_change_block_to_list (EEditorPage *editor_page,
                             EContentEditorBlockFormat format)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *item, *list;
	WebKitDOMNode *block, *next_block;
	gboolean after_selection_end = FALSE, in_quote = FALSE;
	gboolean html_mode = e_editor_page_get_html_mode (editor_page);

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	/* If the selection was not saved, move it into the first child of body */
	if (!selection_start_marker || !selection_end_marker) {
		WebKitDOMHTMLElement *body;
		WebKitDOMNode *child;

		body = webkit_dom_document_get_body (document);
		child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (body));

		dom_add_selection_markers_into_element_start (
			document,
			WEBKIT_DOM_ELEMENT (child),
			&selection_start_marker,
			&selection_end_marker);
	}

	block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	list = create_list_element (editor_page, format, 0, html_mode);

	if (webkit_dom_element_query_selector (
		WEBKIT_DOM_ELEMENT (block), "span.-x-evo-quoted", NULL)) {
		WebKitDOMElement *element;
		WebKitDOMDOMWindow *dom_window = NULL;
		WebKitDOMDOMSelection *dom_selection = NULL;
		WebKitDOMRange *range = NULL;

		in_quote = TRUE;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		range = webkit_dom_document_create_range (document);

		webkit_dom_range_select_node (range, block, NULL);
		webkit_dom_range_collapse (range, TRUE, NULL);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);

		g_clear_object (&range);
		g_clear_object (&dom_selection);
		g_clear_object (&dom_window);

		e_editor_dom_remove_input_event_listener_from_body (editor_page);
		e_editor_page_block_selection_changed (editor_page);

		e_editor_dom_exec_command (
			editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, NULL);

		e_editor_dom_register_input_event_listener_on_body (editor_page);
		e_editor_page_unblock_selection_changed (editor_page);

		element = webkit_dom_document_query_selector (
			document, "body>br", NULL);

		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (list),
			WEBKIT_DOM_NODE (element),
			NULL);

		block = e_editor_dom_get_parent_block_node_from_child (
			WEBKIT_DOM_NODE (selection_start_marker));
	} else
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (block),
			WEBKIT_DOM_NODE (list),
			block,
			NULL);

	/* Process all blocks that are in the selection one by one */
	while (block && !after_selection_end) {
		gboolean empty = FALSE, block_is_list;
		gchar *content;
		WebKitDOMNode *child, *parent;

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		next_block = webkit_dom_node_get_next_sibling (
			WEBKIT_DOM_NODE (block));

		e_editor_dom_remove_wrapping_from_element (WEBKIT_DOM_ELEMENT (block));
		e_editor_dom_remove_quoting_from_element (WEBKIT_DOM_ELEMENT (block));

		item = webkit_dom_document_create_element (document, "LI", NULL);
		content = webkit_dom_node_get_text_content (block);

		empty = !*content || (g_strcmp0 (content, UNICODE_ZERO_WIDTH_SPACE) == 0);
		g_free (content);

		change_leading_space_to_nbsp (block);
		change_trailing_space_in_block_to_nbsp (block);

		block_is_list = node_is_list_or_item (block);

		while ((child = webkit_dom_node_get_first_child (block))) {
			if (WEBKIT_DOM_IS_HTML_BR_ELEMENT (child))
				empty = FALSE;

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (block_is_list ? list : item), child, NULL);
		}

		if (!block_is_list) {
			/* We have to use again the hidden space to move caret into newly inserted list */
			if (empty) {
				WebKitDOMElement *br;

				br = webkit_dom_document_create_element (
					document, "BR", NULL);
				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (item), WEBKIT_DOM_NODE (br), NULL);
			}

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (list), WEBKIT_DOM_NODE (item), NULL);
		}

		parent = webkit_dom_node_get_parent_node (block);
		remove_node (block);

		if (in_quote) {
			/* Remove all parents if previously removed node was the
			 * only one with text content */
			content = webkit_dom_node_get_text_content (parent);
			while (parent && content && !*content) {
				WebKitDOMNode *tmp = webkit_dom_node_get_parent_node (parent);

				remove_node (parent);
				parent = tmp;

				g_free (content);
				content = webkit_dom_node_get_text_content (parent);
			}
			g_free (content);
		}

		block = next_block;
	}

	merge_lists_if_possible (WEBKIT_DOM_NODE (list));
}

static WebKitDOMElement *
do_format_change_list_to_list (WebKitDOMElement *list_to_process,
                               WebKitDOMElement *new_list_template,
                               EContentEditorBlockFormat to)
{
	EContentEditorBlockFormat current_format;

	current_format = dom_get_list_format_from_node (
		WEBKIT_DOM_NODE (list_to_process));
	if (to == current_format) {
		/* Same format, skip it. */
		return list_to_process;
	} else if (current_format >= E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST &&
		   to >= E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST) {
		/* Changing from ordered list type to another ordered list type. */
		set_ordered_list_type_to_element (list_to_process, to);
		return list_to_process;
	} else {
		WebKitDOMNode *clone, *child;

		/* Create new list from template. */
		clone = webkit_dom_node_clone_node_with_error (
			WEBKIT_DOM_NODE (new_list_template), FALSE, NULL);

		/* Insert it before the list that we are processing. */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (list_to_process)),
			clone,
			WEBKIT_DOM_NODE (list_to_process),
			NULL);

		/* Move all it children to the new one. */
		while ((child = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (list_to_process))))
			webkit_dom_node_append_child (clone, child, NULL);

		remove_node (WEBKIT_DOM_NODE (list_to_process));

		return WEBKIT_DOM_ELEMENT (clone);
	}

	return NULL;
}

static void
format_change_list_from_list (EEditorPage *editor_page,
                              EContentEditorBlockFormat to,
                              gboolean html_mode)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker, *new_list;
	WebKitDOMNode *source_list, *source_list_clone, *current_list, *item;
	gboolean after_selection_end = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker || !selection_end_marker)
		return;

	/* Copy elements from previous block to list */
	item = get_list_item_node_from_child (WEBKIT_DOM_NODE (selection_start_marker));
	source_list = webkit_dom_node_get_parent_node (item);
	current_list = source_list;
	source_list_clone = webkit_dom_node_clone_node_with_error (source_list, FALSE, NULL);

	new_list = create_list_element (editor_page, to, 0, html_mode);

	if (element_has_class (WEBKIT_DOM_ELEMENT (source_list), "-x-evo-indented"))
		element_add_class (WEBKIT_DOM_ELEMENT (new_list), "-x-evo-indented");

	while (item) {
		gboolean selection_end;
		WebKitDOMNode *next_item = webkit_dom_node_get_next_sibling (item);

		selection_end = webkit_dom_node_contains (
			item, WEBKIT_DOM_NODE (selection_end_marker));

		if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (item)) {
			/* Actual node is an item, just copy it. */
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				item,
				NULL);
		} else if (node_is_list (item) && !selection_end && !after_selection_end) {
			/* Node is a list and it doesn't contain the selection end
			 * marker, we can process the whole list. */
			gint ii;
			WebKitDOMNodeList *list = NULL;
			WebKitDOMElement *processed_list;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
			ii = webkit_dom_node_list_get_length (list);
			g_clear_object (&list);

			/* Process every sublist separately. */
			while (ii) {
				WebKitDOMElement *list_to_process;

				list_to_process = webkit_dom_element_query_selector (
					WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
				if (list_to_process)
					do_format_change_list_to_list (list_to_process, new_list, to);
				ii--;
			}

			/* Process the current list. */
			processed_list = do_format_change_list_to_list (
				WEBKIT_DOM_ELEMENT (item), new_list, to);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (new_list),
				WEBKIT_DOM_NODE (processed_list),
				NULL);
		} else if (node_is_list (item) && !after_selection_end) {
			/* Node is a list and it contains the selection end marker,
			 * thus we have to process it until we find the marker. */
			gint ii;
			WebKitDOMNodeList *list = NULL;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (item), "ol,ul", NULL);
			ii = webkit_dom_node_list_get_length (list);
			g_clear_object (&list);

			/* No nested lists - process the items. */
			if (ii == 0) {
				WebKitDOMNode *clone, *child;

				clone = webkit_dom_node_clone_node_with_error (
					WEBKIT_DOM_NODE (new_list), FALSE, NULL);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (new_list), clone, NULL);

				while ((child = webkit_dom_node_get_first_child (item))) {
					webkit_dom_node_append_child (clone, child, NULL);
					if (webkit_dom_node_contains (child, WEBKIT_DOM_NODE (selection_end_marker)))
						break;
				}

				if (webkit_dom_node_get_first_child (item))
					webkit_dom_node_append_child (
						WEBKIT_DOM_NODE (new_list), item, NULL);
				else
					remove_node (item);
			} else {
				gboolean done = FALSE;
				WebKitDOMNode *tmp_parent = WEBKIT_DOM_NODE (new_list);
				WebKitDOMNode *tmp_item = WEBKIT_DOM_NODE (item);

				while (!done) {
					WebKitDOMNode *clone, *child;

					clone = webkit_dom_node_clone_node_with_error (
						WEBKIT_DOM_NODE (new_list), FALSE, NULL);

					webkit_dom_node_append_child (
						tmp_parent, clone, NULL);

					while ((child = webkit_dom_node_get_first_child (tmp_item))) {
						if (!webkit_dom_node_contains (child, WEBKIT_DOM_NODE (selection_end_marker))) {
							webkit_dom_node_append_child (clone, child, NULL);
						} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (child)) {
							webkit_dom_node_append_child (clone, child, NULL);
							done = TRUE;
							break;
						} else {
							tmp_parent = clone;
							tmp_item = child;
							break;
						}
					}
				}
			}
		} else {
			webkit_dom_node_append_child (
				after_selection_end ?
					source_list_clone : WEBKIT_DOM_NODE (new_list),
				item,
				NULL);
		}

		if (selection_end) {
			source_list_clone = webkit_dom_node_clone_node_with_error (current_list, FALSE, NULL);
			after_selection_end = TRUE;
		}

		if (!next_item) {
			if (after_selection_end)
				break;

			current_list = webkit_dom_node_get_next_sibling (current_list);
			if (!node_is_list_or_item (current_list))
				break;
			if (node_is_list (current_list)) {
				next_item = webkit_dom_node_get_first_child (current_list);
				if (!node_is_list_or_item (next_item))
					break;
			} else if (WEBKIT_DOM_IS_HTML_LI_ELEMENT (current_list)) {
				next_item = current_list;
				current_list = webkit_dom_node_get_parent_node (next_item);
			}
		}

		item = next_item;
	}

	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (source_list),
		WEBKIT_DOM_NODE (source_list_clone),
		webkit_dom_node_get_next_sibling (source_list),
		NULL);

	if (webkit_dom_node_has_child_nodes (WEBKIT_DOM_NODE (new_list)))
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (source_list_clone),
			WEBKIT_DOM_NODE (new_list),
			source_list_clone,
			NULL);

	remove_node_if_empty (source_list);
	remove_node_if_empty (source_list_clone);
	remove_node_if_empty (current_list);

	merge_lists_if_possible (WEBKIT_DOM_NODE (new_list));
}

static void
format_change_list_to_list (EEditorPage *editor_page,
                            EContentEditorBlockFormat format,
                            gboolean html_mode)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *prev_list, *current_list, *next_list;
	EContentEditorBlockFormat prev = 0, next = 0;
	gboolean done = FALSE, indented = FALSE;
	gboolean selection_starts_in_first_child, selection_ends_in_last_child;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	current_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	prev_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	next_list = get_list_node_from_child (
		WEBKIT_DOM_NODE (selection_end_marker));

	selection_starts_in_first_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_first_child (current_list),
			WEBKIT_DOM_NODE (selection_start_marker));

	selection_ends_in_last_child =
		webkit_dom_node_contains (
			webkit_dom_node_get_last_child (current_list),
			WEBKIT_DOM_NODE (selection_end_marker));

	indented = element_has_class (WEBKIT_DOM_ELEMENT (current_list), "-x-evo-indented");

	if (!prev_list || !next_list || indented) {
		format_change_list_from_list (editor_page, format, html_mode);
		return;
	}

	if (webkit_dom_node_is_same_node (prev_list, next_list)) {
		prev_list = webkit_dom_node_get_previous_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_start_marker))));
		next_list = webkit_dom_node_get_next_sibling (
			webkit_dom_node_get_parent_node (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_end_marker))));
		if (!prev_list || !next_list) {
			format_change_list_from_list (editor_page, format, html_mode);
			return;
		}
	}

	prev = dom_get_list_format_from_node (prev_list);
	next = dom_get_list_format_from_node (next_list);

	if (format != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE) {
		if (format == prev && prev != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE) {
			if (selection_starts_in_first_child && selection_ends_in_last_child) {
				done = TRUE;
				merge_list_into_list (current_list, prev_list, FALSE);
			}
		}
		if (format == next && next != E_CONTENT_EDITOR_BLOCK_FORMAT_NONE) {
			if (selection_starts_in_first_child && selection_ends_in_last_child) {
				done = TRUE;
				merge_list_into_list (next_list, prev_list, FALSE);
			}
		}
	}

	if (done)
		return;

	format_change_list_from_list (editor_page, format, html_mode);
}

/*
 * e_html_editor_selection_set_block_format:
 * @selection: an #EEditorSelection
 * @format: an #EContentEditorBlockFormat value
 *
 * Changes block format of current paragraph to @format.
 */
void
e_editor_dom_selection_set_block_format (EEditorPage *editor_page,
                                         EContentEditorBlockFormat format)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	EContentEditorBlockFormat current_format;
	EContentEditorAlignment current_alignment;
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	const gchar *value;
	gboolean from_list = FALSE, to_list = FALSE, html_mode = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	current_format = e_editor_dom_selection_get_block_format (editor_page);
	if (current_format == format)
		return;

	switch (format) {
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H1:
			value = "H1";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H2:
			value = "H2";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H3:
			value = "H3";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H4:
			value = "H4";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H5:
			value = "H5";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_H6:
			value = "H6";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH:
			value = "DIV";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_PRE:
			value = "PRE";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS:
			value = "ADDRESS";
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST:
		case E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
		case E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
			to_list = TRUE;
			value = NULL;
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST:
			to_list = TRUE;
			value = NULL;
			break;
		case E_CONTENT_EDITOR_BLOCK_FORMAT_NONE:
		default:
			value = NULL;
			break;
	}

	html_mode = e_editor_page_get_html_mode (editor_page);

	from_list =
		current_format >= E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST;

	range = e_editor_dom_get_current_range (editor_page);
	if (!range)
		return;

	current_alignment = e_editor_page_get_alignment (editor_page);

	e_editor_dom_selection_save (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_BLOCK_FORMAT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.style.from = current_format;
		ev->data.style.to = format;
	}

	g_clear_object (&range);

	if (current_format == E_CONTENT_EDITOR_BLOCK_FORMAT_PRE) {
		WebKitDOMElement *selection_marker;

		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (selection_marker)
			change_space_before_selection_to_nbsp (WEBKIT_DOM_NODE (selection_marker));
		selection_marker = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-end-marker");
		if (selection_marker)
			change_space_before_selection_to_nbsp (WEBKIT_DOM_NODE (selection_marker));
	}

	if (from_list && to_list)
		format_change_list_to_list (editor_page, format, html_mode);

	if (!from_list && !to_list)
		format_change_block_to_block (editor_page, format, value);

	if (from_list && !to_list)
		format_change_list_to_block (editor_page, format, value);

	if (!from_list && to_list)
		format_change_block_to_list (editor_page, format);

	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	/* When changing the format we need to re-set the alignment */
	e_editor_dom_selection_set_alignment (editor_page, current_alignment);

	e_editor_page_emit_content_changed (editor_page);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

/*
 * e_html_editor_selection_get_background_color:
 * @selection: an #EEditorSelection
 *
 * Returns background color of currently selected text or letter at current
 * cursor position.
 *
 * Returns: A string with code of current background color.
 */
gchar *
e_editor_dom_selection_get_background_color (EEditorPage *editor_page)
{
	WebKitDOMNode *ancestor;
	WebKitDOMRange *range = NULL;
	WebKitDOMCSSStyleDeclaration *css = NULL;
	gchar *value;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	range = e_editor_dom_get_current_range (editor_page);
	ancestor = webkit_dom_range_get_common_ancestor_container (range, NULL);
	css = webkit_dom_element_get_style (WEBKIT_DOM_ELEMENT (ancestor));
/* FIXME WK2
	g_free (selection->priv->background_color);
	selection->priv->background_color =
		webkit_dom_css_style_declaration_get_property_value (
			css, "background-color");*/

	value = webkit_dom_css_style_declaration_get_property_value (css, "background-color");

	g_clear_object (&css);
	g_clear_object (&range);

	return value;
}

/*
 * e_html_editor_selection_set_background_color:
 * @selection: an #EEditorSelection
 * @color: code of new background color to set
 *
 * Changes background color of current selection or letter at current cursor
 * position to @color.
 */
void
e_editor_dom_selection_set_background_color (EEditorPage *editor_page,
                                             const gchar *color)
{
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR, color);
}

/*
 * e_html_editor_selection_get_alignment:
 * @selection: #an EEditorSelection
 *
 * Returns alignment of current paragraph
 *
 * Returns: #EContentEditorAlignment
 */
EContentEditorAlignment
e_editor_dom_selection_get_alignment (EEditorPage *editor_page)
{
	WebKitDOMCSSStyleDeclaration *style = NULL;
	WebKitDOMElement *element;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;
	EContentEditorAlignment alignment;
	gchar *value;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), E_CONTENT_EDITOR_ALIGNMENT_LEFT);

	range = e_editor_dom_get_current_range (editor_page);
	if (!range) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
		goto out;
	}

	node = webkit_dom_range_get_start_container (range, NULL);
	g_clear_object (&range);
	if (!node) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
		goto out;
	}

	if (WEBKIT_DOM_IS_ELEMENT (node))
		element = WEBKIT_DOM_ELEMENT (node);
	else
		element = webkit_dom_node_get_parent_element (node);

	if (element_has_class (element, "-x-evo-align-right")) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
		goto out;
	} else if (element_has_class (element, "-x-evo-align-center")) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_CENTER;
		goto out;
	}

	style = webkit_dom_element_get_style (element);
	value = webkit_dom_css_style_declaration_get_property_value (style, "text-align");

	if (!value || !*value ||
	    (g_ascii_strncasecmp (value, "left", 4) == 0)) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	} else if (g_ascii_strncasecmp (value, "center", 6) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_CENTER;
	} else if (g_ascii_strncasecmp (value, "right", 5) == 0) {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	} else {
		alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	}

	g_clear_object (&style);
	g_free (value);

 out:
	return alignment;
}

static void
set_block_alignment (WebKitDOMElement *element,
                     const gchar *class)
{
	WebKitDOMElement *parent;

	element_remove_class (element, "-x-evo-align-center");
	element_remove_class (element, "-x-evo-align-right");
	element_add_class (element, class);
	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (element));
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		element_remove_class (parent, "-x-evo-align-center");
		element_remove_class (parent, "-x-evo-align-right");
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}
}

/*
 * e_html_editor_selection_set_alignment:
 * @selection: an #EEditorSelection
 * @alignment: an #EContentEditorAlignment value to apply
 *
 * Sets alignment of current paragraph to give @alignment.
 */
void
e_editor_dom_selection_set_alignment (EEditorPage *editor_page,
                                      EContentEditorAlignment alignment)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *selection_start_marker, *selection_end_marker;
	WebKitDOMNode *block;
	EContentEditorAlignment current_alignment;
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	gboolean after_selection_end = FALSE;
	const gchar *class = "";

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	current_alignment = e_editor_page_get_alignment (editor_page);

	if (current_alignment == alignment)
		return;

	switch (alignment) {
		case E_CONTENT_EDITOR_ALIGNMENT_CENTER:
			class = "-x-evo-align-center";
			break;

		case E_CONTENT_EDITOR_ALIGNMENT_LEFT:
			break;

		case E_CONTENT_EDITOR_ALIGNMENT_RIGHT:
			class = "-x-evo-align-right";
			break;
	}

	e_editor_dom_selection_save (editor_page);

	selection_start_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	selection_end_marker = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	if (!selection_start_marker)
		return;

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_ALIGNMENT;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);
		ev->data.style.from = current_alignment;
		ev->data.style.to = alignment;
	}

	block = e_editor_dom_get_parent_block_node_from_child (
		WEBKIT_DOM_NODE (selection_start_marker));

	while (block && !after_selection_end) {
		WebKitDOMNode *next_block;

		next_block = webkit_dom_node_get_next_sibling (block);

		after_selection_end = webkit_dom_node_contains (
			block, WEBKIT_DOM_NODE (selection_end_marker));

		if (element_has_class (WEBKIT_DOM_ELEMENT (block), "-x-evo-indented")) {
			gint ii;
			WebKitDOMNodeList *list = NULL;

			list = webkit_dom_element_query_selector_all (
				WEBKIT_DOM_ELEMENT (block),
				".-x-evo-indented > *:not(.-x-evo-indented):not(li)",
				NULL);
			for (ii = webkit_dom_node_list_get_length (list); ii--;) {
				WebKitDOMNode *item = webkit_dom_node_list_item (list, ii);

				set_block_alignment (WEBKIT_DOM_ELEMENT (item), class);

				after_selection_end = webkit_dom_node_contains (
					item, WEBKIT_DOM_NODE (selection_end_marker));
				if (after_selection_end)
					break;
			}

			g_clear_object (&list);
		} else {
			set_block_alignment (WEBKIT_DOM_ELEMENT (block), class);
		}

		block = next_block;
	}

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_selection_restore (editor_page);

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);
	e_editor_page_emit_content_changed (editor_page);
}

void
e_editor_dom_insert_replace_all_history_event (EEditorPage *editor_page,
                                               const gchar *search_text,
                                               const gchar *replacement)
{
	EEditorUndoRedoManager *manager;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_REPLACE_ALL;

		ev->data.string.from = g_strdup (search_text);
		ev->data.string.to = g_strdup (replacement);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

/*
 * e_html_editor_selection_replace:
 * @selection: an #EEditorSelection
 * @replacement: a string to replace current selection with
 *
 * Replaces currently selected text with @replacement.
 */
void
e_editor_dom_selection_replace (EEditorPage *editor_page,
                                const gchar *replacement)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	manager = e_editor_page_get_undo_redo_manager (editor_page);

	if (!(range = e_editor_dom_get_current_range (editor_page)) ||
	     e_editor_dom_selection_is_collapsed (editor_page))
		return;

	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_REPLACE;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		ev->data.string.from = webkit_dom_range_get_text (range);
		ev->data.string.to = g_strdup (replacement);
	}

	g_clear_object (&range);

	e_editor_dom_exec_command (editor_page, E_CONTENT_EDITOR_COMMAND_INSERT_TEXT, replacement);

	if (ev) {
		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->after.start.x,
			&ev->after.start.y,
			&ev->after.end.x,
			&ev->after.end.y);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	e_editor_page_emit_content_changed (editor_page);
}

/*
 * e_html_editor_selection_replace_caret_word:
 * @selection: an #EEditorSelection
 * @replacement: a string to replace current caret word with
 *
 * Replaces current word under cursor with @replacement.
 */
void
e_editor_dom_replace_caret_word (EEditorPage *editor_page,
                                 const gchar *replacement)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMNode *node;
	WebKitDOMRange *range = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	e_editor_page_emit_content_changed (editor_page);
	range = e_editor_dom_get_current_range (editor_page);
	webkit_dom_range_expand (range, "word", NULL);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	fragment = webkit_dom_range_extract_contents (range, NULL);

	/* Get the text node to replace and leave other formatting nodes
	 * untouched (font color, boldness, ...). */
	webkit_dom_node_normalize (WEBKIT_DOM_NODE (fragment));
	node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (fragment));
	if (!WEBKIT_DOM_IS_TEXT (node)) {
		while (node && WEBKIT_DOM_IS_ELEMENT (node))
			node = webkit_dom_node_get_first_child (node);
	}

	if (node && WEBKIT_DOM_IS_TEXT (node)) {
		WebKitDOMText *text;

		/* Replace the word */
		text = webkit_dom_document_create_text_node (document, replacement);
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (node),
			WEBKIT_DOM_NODE (text),
			node,
			NULL);

		/* Insert the word on current location. */
		webkit_dom_range_insert_node (range, WEBKIT_DOM_NODE (fragment), NULL);

		webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
	}

	e_editor_dom_force_spell_check_for_current_paragraph (editor_page);

	g_clear_object (&range);
	g_clear_object (&dom_selection);
}

/*
 * e_html_editor_selection_get_caret_word:
 * @selection: an #EEditorSelection
 *
 * Returns word under cursor.
 *
 * Returns: A newly allocated string with current caret word or @NULL when there
 * is no text under cursor or when selection is active. [transfer-full].
 */
gchar *
e_editor_dom_get_caret_word (EEditorPage *editor_page)
{
	gchar *word;
	WebKitDOMRange *range = NULL, *range_clone = NULL;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	range = e_editor_dom_get_current_range (editor_page);

	/* Don't operate on the visible selection */
	range_clone = webkit_dom_range_clone_range (range, NULL);
	webkit_dom_range_expand (range_clone, "word", NULL);
	word = webkit_dom_range_to_string (range_clone, NULL);

	g_clear_object (&range);
	g_clear_object (&range_clone);

	return word;
}

/*
 * e_html_editor_selection_get_list_alignment_from_node:
 * @node: #an WebKitDOMNode
 *
 * Returns alignment of given list.
 *
 * Returns: #EContentEditorAlignment
 */
EContentEditorAlignment
e_editor_dom_get_list_alignment_from_node (WebKitDOMNode *node)
{
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-center"))
		return E_CONTENT_EDITOR_ALIGNMENT_CENTER;
	if (element_has_class (WEBKIT_DOM_ELEMENT (node), "-x-evo-align-right"))
		return E_CONTENT_EDITOR_ALIGNMENT_RIGHT;
	else
		return E_CONTENT_EDITOR_ALIGNMENT_LEFT;
}

WebKitDOMElement *
e_editor_dom_prepare_paragraph (EEditorPage *editor_page,
                                gboolean with_selection)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *paragraph;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	paragraph = e_editor_dom_get_paragraph_element (editor_page, -1, 0);

	if (with_selection)
		dom_add_selection_markers_into_element_start (
			document, paragraph, NULL, NULL);

	element = webkit_dom_document_create_element (document, "BR", NULL);

	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (paragraph), WEBKIT_DOM_NODE (element), NULL);

	return paragraph;
}

void
e_editor_dom_selection_set_on_point (EEditorPage *editor_page,
                                     guint x,
                                     guint y)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	range = webkit_dom_document_caret_range_from_point (document, x, y);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);

	g_clear_object (&range);
	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
}

void
e_editor_dom_selection_get_coordinates (EEditorPage *editor_page,
                                        guint *start_x,
                                        guint *start_y,
                                        guint *end_x,
                                        guint *end_y)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element, *parent;
	gboolean created_selection_markers = FALSE;
	guint local_x = 0, local_y = 0;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (start_x != NULL);
	g_return_if_fail (start_y != NULL);
	g_return_if_fail (end_x != NULL);
	g_return_if_fail (end_y != NULL);

	document = e_editor_page_get_document (editor_page);
	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	if (!element) {
		created_selection_markers = TRUE;
		e_editor_dom_selection_save (editor_page);
		element = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		if (!element)
			return;
	}

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	*start_x = local_x;
	*start_y = local_y;

	if (e_editor_dom_selection_is_collapsed (editor_page)) {
		*end_x = local_x;
		*end_y = local_y;

		if (created_selection_markers)
			e_editor_dom_selection_restore (editor_page);

		goto workaroud;
	}

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	local_x = 0;
	local_y = 0;

	parent = element;
	while (parent && !WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		local_x += (guint) webkit_dom_element_get_offset_left (parent);
		local_y += (guint) webkit_dom_element_get_offset_top (parent);
		parent = webkit_dom_element_get_offset_parent (parent);
	}

	*end_x = local_x;
	*end_y = local_y;

	if (created_selection_markers)
		e_editor_dom_selection_restore (editor_page);

 workaroud:
	/* Workaround for bug 749712 on the Evolution side. The cause of the bug
	 * is that WebKit is having problems determining the right line height
	 * for some fonts and font sizes (the right and wrong value differ by 1).
	 * To fix this we will add an extra one to the final top offset. This is
	 * safe to do even for fonts and font sizes that don't behave badly as we
	 * will still get the right element as we use fonts bigger than 1 pixel. */
	*start_y += 1;
	*end_y += 1;
}

WebKitDOMRange *
e_editor_dom_get_range_for_point (WebKitDOMDocument *document,
                                  EEditorSelectionPoint point)
{
	glong scroll_left, scroll_top;
	WebKitDOMHTMLElement *body;
	WebKitDOMRange *range = NULL;

	body = webkit_dom_document_get_body (document);
	scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
	scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));

	range = webkit_dom_document_caret_range_from_point (
		document, point.x - scroll_left, point.y - scroll_top);

	/* The point is outside the viewport, scroll to it. */
	if (!range) {
		WebKitDOMDOMWindow *dom_window = NULL;

		dom_window = webkit_dom_document_get_default_view (document);
		webkit_dom_dom_window_scroll_to (dom_window, point.x, point.y);

		scroll_left = webkit_dom_element_get_scroll_left (WEBKIT_DOM_ELEMENT (body));
		scroll_top = webkit_dom_element_get_scroll_top (WEBKIT_DOM_ELEMENT (body));
		range = webkit_dom_document_caret_range_from_point (
			document, point.x - scroll_left, point.y - scroll_top);
		g_clear_object (&dom_window);
	}

	return range;
}

void
e_editor_dom_selection_restore_to_history_event_state (EEditorPage *editor_page,
                                                       EEditorSelection selection_state)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window = NULL;
	WebKitDOMDOMSelection *dom_selection = NULL;
	WebKitDOMElement *element, *tmp;
	WebKitDOMRange *range = NULL;
	gboolean was_collapsed = FALSE;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_clear_object (&dom_window);

	/* Restore the selection how it was before the event occured. */
	range = e_editor_dom_get_range_for_point (document, selection_state.start);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&range);

	was_collapsed = selection_state.start.x == selection_state.end.x;
	was_collapsed = was_collapsed && selection_state.start.y == selection_state.end.y;
	if (was_collapsed) {
		g_clear_object (&dom_selection);
		return;
	}

	e_editor_dom_selection_save (editor_page);

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	remove_node (WEBKIT_DOM_NODE (element));

	element = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	webkit_dom_element_remove_attribute (element, "id");

	range = e_editor_dom_get_range_for_point (document, selection_state.end);
	webkit_dom_dom_selection_remove_all_ranges (dom_selection);
	webkit_dom_dom_selection_add_range (dom_selection, range);
	g_clear_object (&range);

	e_editor_dom_selection_save (editor_page);

	tmp = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");

	remove_node (WEBKIT_DOM_NODE (tmp));

	webkit_dom_element_set_id (
		element, "-x-evo-selection-start-marker");

	e_editor_dom_selection_restore (editor_page);

	g_clear_object (&dom_selection);
}

/*
 * e-html-editor-table-dialog-dom-functions.c
 *
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#include <web-extensions/e-dom-utils.h>

#include "e-html-editor-selection-dom-functions.h"

#include "e-html-editor-table-dialog-dom-functions.h"

static WebKitDOMHTMLTableElement *
get_current_table_element (WebKitDOMDocument *document)
{
	return WEBKIT_DOM_HTML_TABLE_ELEMENT (webkit_dom_document_get_element_by_id (document, "-x-evo-current-table"));
}

void
e_html_editor_table_dialog_set_row_count (WebKitDOMDocument *document,
                                          gulong expected_count)
{
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;
	gulong ii, current_count;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	rows = webkit_dom_html_table_element_get_rows (table_element);
	current_count = webkit_dom_html_collection_get_length (rows);

	if (current_count < expected_count) {
		for (ii = 0; ii < expected_count - current_count; ii++) {
			webkit_dom_html_table_element_insert_row (
				table_element, -1, NULL);
		}
	} else if (current_count > expected_count) {
		for (ii = 0; ii < current_count - expected_count; ii++) {
			webkit_dom_html_table_element_delete_row (
				table_element, -1, NULL);
		}
	}
	g_object_unref (rows);
}

gulong
e_html_editor_table_dialog_get_row_count (WebKitDOMDocument *document)
{
	glong count;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;

	table_element = get_current_table_element (document);
	if (!table_element)
		return 0;

	rows = webkit_dom_html_table_element_get_rows (table_element);

	count = webkit_dom_html_collection_get_length (rows);
	g_object_unref (rows);

	return count;
}

void
e_html_editor_table_dialog_set_column_count (WebKitDOMDocument *document,
                                             gulong expected_columns)
{
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;
	gulong ii, row_count;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	rows = webkit_dom_html_table_element_get_rows (table_element);
	row_count = webkit_dom_html_collection_get_length (rows);

	for (ii = 0; ii < row_count; ii++) {
		WebKitDOMHTMLTableRowElement *row;
		WebKitDOMHTMLCollection *cells;
		gulong jj, current_columns;

		row = WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (
			webkit_dom_html_collection_item (rows, ii));

		cells = webkit_dom_html_table_row_element_get_cells (row);
		current_columns = webkit_dom_html_collection_get_length (cells);

		if (current_columns < expected_columns) {
			for (jj = 0; jj < expected_columns - current_columns; jj++) {
				webkit_dom_html_table_row_element_insert_cell (
					row, -1, NULL);
			}
		} else if (expected_columns < current_columns) {
			for (jj = 0; jj < current_columns - expected_columns; jj++) {
				webkit_dom_html_table_row_element_delete_cell (
					row, -1, NULL);
			}
		}
		g_object_unref (row);
		g_object_unref (cells);
	}
	g_object_unref (rows);
}

gulong
e_html_editor_table_dialog_get_column_count (WebKitDOMDocument *document)
{
	glong count;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows, *columns;
	WebKitDOMNode *row;

	table_element = get_current_table_element (document);
	if (!table_element)
		return 0;

	rows = webkit_dom_html_table_element_get_rows (table_element);
	row = webkit_dom_html_collection_item (rows, 0);

	columns = webkit_dom_html_table_row_element_get_cells (
		WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	count = webkit_dom_html_collection_get_length (columns);

	g_object_unref (row);
	g_object_unref (rows);
	g_object_unref (columns);

	return count;
}

static WebKitDOMElement *
create_table (WebKitDOMDocument *document,
              EHTMLEditorWebExtension *extension)
{
	gboolean empty = FALSE;
	gchar *text_content;
	gint i;
	WebKitDOMElement *table, *br, *caret, *element, *cell;
	WebKitDOMNode *clone;

	/* Default 3x3 table */
	table = webkit_dom_document_create_element (document, "TABLE", NULL);
	for (i = 0; i < 3; i++) {
		WebKitDOMHTMLElement *row;
		gint j;

		row = webkit_dom_html_table_element_insert_row (
			WEBKIT_DOM_HTML_TABLE_ELEMENT (table), -1, NULL);

		for (j = 0; j < 3; j++) {
			webkit_dom_html_table_row_element_insert_cell (
				WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row), -1, NULL);
		}
	}

	webkit_dom_element_set_id (table, "-x-evo-current-table");

	dom_selection_save (document);
	caret = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");


	element = get_parent_block_element (WEBKIT_DOM_NODE (caret));
	text_content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (element));
	empty = text_content && !*text_content;
	g_free (text_content);

	clone = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (element), FALSE, NULL);
	br = webkit_dom_document_create_element (document, "BR", NULL);
	webkit_dom_node_append_child (clone, WEBKIT_DOM_NODE (br), NULL);
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
		clone,
		webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
		NULL);

	/* Move caret to the first cell */
	cell = webkit_dom_element_query_selector (table, "td", NULL);
	webkit_dom_node_append_child (
		WEBKIT_DOM_NODE (cell), WEBKIT_DOM_NODE (caret), NULL);
	caret = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-start-marker");
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (cell),
		WEBKIT_DOM_NODE (caret),
		webkit_dom_node_get_last_child (WEBKIT_DOM_NODE (cell)),
		NULL);

	/* Insert the table into body unred the current block (if current block is not empty)
	 * otherwise replace the current block. */
	if (empty) {
		webkit_dom_node_replace_child (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (table),
			WEBKIT_DOM_NODE (element),
			NULL);
	} else {
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (table),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
			NULL);
	}

	dom_selection_restore (document);

	e_html_editor_web_extension_set_content_changed (extension);

	return table;
}

gboolean
e_html_editor_table_dialog_show (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	gboolean created = FALSE;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *table = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	if (dom_selection && (webkit_dom_dom_selection_get_range_count (dom_selection) > 0)) {
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		table = dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "TABLE");
		g_object_unref (range);

		if (table) {
			webkit_dom_element_set_id (table, "-x-evo-current-table");
		} else {
			table = create_table (document, extension);
			created = TRUE;
		}
	}

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EHTMLEditorHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_DIALOG;

		dom_selection_get_coordinates (
			document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (!created)
			ev->data.dom.from = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (table), TRUE, NULL);
		else
			ev->data.dom.from = NULL;

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	g_object_unref (dom_selection);

	return created;
}

void
e_html_editor_table_dialog_save_history_on_exit (WebKitDOMDocument *document,
                                                 EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMElement *element;

	element = WEBKIT_DOM_ELEMENT (get_current_table_element (document));
	g_return_if_fail (element != NULL);

	webkit_dom_element_remove_attribute (element, "id");

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (element), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		dom_selection_get_coordinates (
			document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_html_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

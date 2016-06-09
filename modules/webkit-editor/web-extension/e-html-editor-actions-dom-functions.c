/*
 * e-html-editor-actions-dom-functions.c
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
#include <webkitdom/WebKitDOMDocumentFragmentUnstable.h>
#include <webkitdom/WebKitDOMRangeUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#include <web-extensions/e-dom-utils.h>

#include "e-html-editor-history-event.h"
#include "e-html-editor-selection-dom-functions.h"

#include "e-html-editor-actions-dom-functions.h"

static WebKitDOMElement *
get_table_cell_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-table-cell");
}

static void
prepare_history_for_table (WebKitDOMDocument *document,
                           WebKitDOMElement *table,
                           EHTMLEditorHistoryEvent *ev)
{
	ev->type = HISTORY_TABLE_DIALOG;

	dom_selection_get_coordinates (
		document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);

	ev->data.dom.from = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (table), TRUE, NULL);
}


static void
save_history_for_table (WebKitDOMDocument *document,
                        EHTMLEditorWebExtension *extension,
                        WebKitDOMElement *table,
                        EHTMLEditorHistoryEvent *ev)
{
	EHTMLEditorUndoRedoManager *manager;

	if (table)
		ev->data.dom.to = webkit_dom_node_clone_node_with_error (
			WEBKIT_DOM_NODE (table), TRUE, NULL);
	else
		ev->data.dom.to = NULL;

	dom_selection_get_coordinates (
		document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
}

void
e_html_editor_dialog_delete_cell_contents (WebKitDOMDocument *document,
                                           EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMNode *node;
	WebKitDOMElement *cell, *table_cell, *table;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

	while ((node = webkit_dom_node_get_first_child (WEBKIT_DOM_NODE (cell))))
		remove_node (node);

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_delete_column (WebKitDOMDocument *document,
                                    EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *cell, *table, *table_cell;
	WebKitDOMHTMLCollection *rows;
	gulong index, length, ii;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	/* Find TD in which the selection starts */
	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

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
		g_object_unref (row);
	}

	g_object_unref (rows);

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_delete_row (WebKitDOMDocument *document,
                                 EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *row, *table, *table_cell;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

	remove_node (WEBKIT_DOM_NODE (row));

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_delete_table (WebKitDOMDocument *document,
                                   EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *table, *table_cell;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

	remove_node (WEBKIT_DOM_NODE (table));

	save_history_for_table (document, extension, NULL, ev);
}

void
e_html_editor_dialog_insert_column_after (WebKitDOMDocument *document,
                                          EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	gulong index;
	WebKitDOMElement *cell, *row, *table_cell, *table;

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

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

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

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_insert_column_before (WebKitDOMDocument *document,
                                           EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	gulong index;
	WebKitDOMElement *cell, *row, *table_cell, *table;

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

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

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

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_insert_row_above (WebKitDOMDocument *document,
                                       EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	gulong index, cell_count, ii;
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells;
	WebKitDOMHTMLElement *new_row;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

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

	g_object_unref (cells);

	save_history_for_table (document, extension, table, ev);
}

void
e_html_editor_dialog_insert_row_below (WebKitDOMDocument *document,
                                       EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	gulong index, cell_count, ii;
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells;
	WebKitDOMHTMLElement *new_row;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	prepare_history_for_table (document, table, ev);

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

	g_object_unref (cells);

	save_history_for_table (document, extension, table, ev);
}

void
dom_save_history_for_cut (WebKitDOMDocument *document,
                          EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMDocumentFragment *fragment;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!webkit_dom_dom_selection_get_range_count (dom_selection) ||
	    webkit_dom_dom_selection_get_is_collapsed (dom_selection)) {
		g_object_unref (dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	ev = g_new0 (EHTMLEditorHistoryEvent, 1);
	ev->type = HISTORY_DELETE;

	dom_selection_get_coordinates (
		document,
		&ev->before.start.x,
		&ev->before.start.y,
		&ev->before.end.x,
		&ev->before.end.y);

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);

	ev->after.start.x = ev->before.start.x;
	ev->after.start.y = ev->before.start.y;
	ev->after.end.x = ev->before.start.x;
	ev->after.end.y = ev->before.start.y;

	/* Save the fragment. */
	fragment = webkit_dom_range_clone_contents (range, NULL);
	g_object_unref (range);
	g_object_unref (dom_selection);
	ev->data.fragment = g_object_ref (fragment);

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
}

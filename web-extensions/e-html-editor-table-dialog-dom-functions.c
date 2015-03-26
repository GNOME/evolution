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

#include "e-html-editor-table-dialog-dom-functions.h"

#include "e-dom-utils.h"
#include "e-html-editor-selection-dom-functions.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

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

static void
create_table (WebKitDOMDocument *document)
{
	WebKitDOMElement *table, *br, *caret, *parent, *element;
	gint i;

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

	caret = dom_save_caret_position (document);

	parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (caret));
	element = caret;

	while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
		element = parent;
		parent = webkit_dom_node_get_parent_element (
			WEBKIT_DOM_NODE (parent));
	}

	br = webkit_dom_document_create_element (document, "BR", NULL);
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (br),
		webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
		NULL);

	/* Insert the table into body below the caret */
	webkit_dom_node_insert_before (
		WEBKIT_DOM_NODE (parent),
		WEBKIT_DOM_NODE (table),
		webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
		NULL);

	dom_clear_caret_position_marker (document);

	/* FIXME WK2
	e_html_editor_view_set_changed (view, TRUE);*/
}

gboolean
e_html_editor_table_dialog_show (WebKitDOMDocument *document)
{
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;

	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (selection && (webkit_dom_dom_selection_get_range_count (selection) > 0)) {
		WebKitDOMElement *table;
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		table = dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "TABLE");

		if (table) {
			webkit_dom_element_set_id (table, "-x-evo-current-table");
			return FALSE;
		} else {
			create_table (document);
			return TRUE;
		}
	}

	return FALSE;
}

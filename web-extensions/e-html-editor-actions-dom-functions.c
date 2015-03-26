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

#include "e-html-editor-actions-dom-functions.h"

#include "e-dom-utils.h"

static WebKitDOMElement *
get_table_cell_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-table-cell");
}

void
e_html_editor_dialog_delete_cell (WebKitDOMDocument *document)
{
	WebKitDOMNode *sibling;
	WebKitDOMElement *cell, *table_cell;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	sibling = webkit_dom_node_get_previous_sibling (WEBKIT_DOM_NODE (cell));
	if (!sibling) {
		sibling = webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (cell));
	}

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (cell)),
		WEBKIT_DOM_NODE (cell), NULL);

	if (sibling) {
		webkit_dom_html_table_cell_element_set_col_span (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (sibling),
			webkit_dom_html_table_cell_element_get_col_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (sibling)) + 1);
	}
}

void
e_html_editor_dialog_delete_column (WebKitDOMDocument *document)
{
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
}

void
e_html_editor_dialog_delete_row (WebKitDOMDocument *document)
{
	WebKitDOMElement *row, *table_cell;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (row)),
		WEBKIT_DOM_NODE (row), NULL);
}

void
e_html_editor_dialog_delete_table (WebKitDOMDocument *document)
{
	WebKitDOMElement *table, *table_cell;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TABLE");
	g_return_if_fail (table != NULL);

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (table)),
		WEBKIT_DOM_NODE (table), NULL);
}

void
e_html_editor_dialog_insert_column_after (WebKitDOMDocument *document)
{
	WebKitDOMElement *cell, *row, *table_cell;
	gulong index;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell)
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	g_return_if_fail (cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TR");
	g_return_if_fail (row != NULL);

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
}

void
e_html_editor_dialog_insert_column_before (WebKitDOMDocument *document)
{
	WebKitDOMElement *cell, *row, *table_cell;
	gulong index;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TD");
	if (!cell) {
		cell = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TH");
	}
	g_return_if_fail (cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

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
}

void
e_html_editor_dialog_insert_row_above (WebKitDOMDocument *document)
{
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells;
	WebKitDOMHTMLElement *new_row;
	gulong index, cell_count, ii;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

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
}

void
e_html_editor_dialog_insert_row_below (WebKitDOMDocument *document)
{
	WebKitDOMElement *row, *table, *table_cell;
	WebKitDOMHTMLCollection *cells;
	WebKitDOMHTMLElement *new_row;
	gulong index, cell_count, ii;

	table_cell = get_table_cell_element (document);
	g_return_if_fail (table_cell != NULL);

	row = dom_node_find_parent_element (WEBKIT_DOM_NODE (table_cell), "TR");
	g_return_if_fail (row != NULL);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (row), "TABLE");
	g_return_if_fail (table != NULL);

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
}

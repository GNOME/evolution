/*
 * e-html-editor-cell-dialog-dom-functions.c
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

#include "e-html-editor-cell-dialog-dom-functions.h"

#include "e-dom-utils.h"

static WebKitDOMElement *
get_current_table_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-table");
}

static WebKitDOMElement *
e_html_editor_table_dialog_create_table (WebKitDOMDocument *document)
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

	caret = e_html_editor_selection_dom_save_caret_position (document);

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

	e_html_editor_selection_dom_clear_caret_position_marker (document);

	/* FIXME WK2
	e_html_editor_view_set_changed (view, TRUE);*/

	return table;
}

void
e_html_editor_table_dialog_set_row_count (WebKitDOMDocument *document,
                                          gulong expected_count)
{
	WebKitDOMElement *table_element;
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
}

gulong
e_html_editor_table_dialog_get_row_count (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;
	WebKitDOMHTMLCollection *rows;

	table_element = get_current_table_element (document);
	if (!table_element)
		return 0;

	rows = webkit_dom_html_table_element_get_rows (able_element);

	return webkit_dom_html_collection_get_length (rows);
}

void
e_html_editor_table_dialog_set_column_count (WebKitDOMDocument *document,
                                             gulong expected_columns)
{
	WebKitDOMElement *table_element;
	WebKitDOMHTMLCollection *rows;
	gulong ii, row_count;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);
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
	}
}

gulong
e_html_editor_table_dialog_get_column_count (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;
	WebKitDOMHTMLCollection *rows, *columns;
	WebKitDOMNode *row;

	table_element = get_current_table_element (document);
	if (!table_element)
		return 0;

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);
	row = webkit_dom_html_collection_item (rows, 0);

	columns = webkit_dom_html_table_row_element_get_cells (
		WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	return webkit_dom_html_collection_get_length (columns);
}

void
e_html_editor_table_dialog_set_width (WebKitDOMDocument *document,
                                      const gchar *width)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_width (table_element, width);
}

gchar *
e_html_editor_table_dialog_get_width (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_width (table_element);
}

void
e_html_editor_table_dialog_set_alignment (WebKitDOMDocument *document.
                                          const gchar *value)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_align (table_element, value);
}

gchar *
e_html_editor_table_dialog_get_alignment (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_align (table_element);
}

void
e_html_editor_table_dialog_set_padding (WebKitDOMDocument *document.
                                        const gchar *value)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_cell_padding (table_element, value);
}

gchar *
e_html_editor_table_dialog_get_padding (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_cell_padding (table_element);
}

void
e_html_editor_table_dialog_set_spacing (WebKitDOMDocument *document.
                                        const gchar *value)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_cell_spacing (table_element, value);
}

gchar *
e_html_editor_table_dialog_get_spacing (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_cell_spacing (table_element);
}

void
e_html_editor_table_dialog_set_border (WebKitDOMDocument *document.
                                       const gchar *value)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_border (table_element, value);
}

gchar *
e_html_editor_table_dialog_get_border (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_border (table_element);
}

void
e_html_editor_table_dialog_set_bg_color (WebKitDOMDocument *document.
                                         const gchar *value)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return;

	webkit_dom_html_table_element_set_bg_color (table_element, value);
}

gchar *
e_html_editor_table_dialog_get_bg_color (WebKitDOMDocument *document)
{
	WebKitDOMElement *table_element;

	table_element = get_current_table_element (document);
	if (!table_element)
		return NULL;

	return webkit_dom_html_table_element_get_bg_color (table_element);
}

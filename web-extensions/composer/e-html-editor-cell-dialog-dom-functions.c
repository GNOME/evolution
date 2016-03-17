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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <web-extensions/e-dom-utils.h>

#include "e-html-editor-undo-redo-manager.h"
#include "e-html-editor-selection-dom-functions.h"

#include "e-html-editor-cell-dialog-dom-functions.h"

enum {
	SCOPE_CELL,
	SCOPE_ROW,
	SCOPE_COLUMN,
	SCOPE_TABLE
};

typedef void (*DOMStrFunc) (WebKitDOMHTMLTableCellElement *cell, const gchar *val, gpointer user_data);
typedef void (*DOMUlongFunc) (WebKitDOMHTMLTableCellElement *cell, gulong val, gpointer user_data);
typedef void (*DOMBoolFunc) (WebKitDOMHTMLTableCellElement *cell, gboolean val, gpointer user_data);

static WebKitDOMElement *
get_current_cell_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-cell");
}

static void
call_cell_dom_func (WebKitDOMHTMLTableCellElement *cell,
                    gpointer func,
                    GValue *value,
                    gpointer user_data)
{
	if (G_VALUE_HOLDS_STRING (value)) {
		DOMStrFunc f = func;
		f (cell, g_value_get_string (value), user_data);
	} else if (G_VALUE_HOLDS_LONG (value)) {
		DOMUlongFunc f = func;
		f (cell, g_value_get_ulong (value), user_data);
	} else if (G_VALUE_HOLDS_BOOLEAN (value)) {
		DOMBoolFunc f = func;
		f (cell, g_value_get_boolean (value), user_data);
	}
}

static void
for_each_cell_do (WebKitDOMElement *row,
                  gpointer func,
                  GValue *value,
                  gpointer user_data)
{
	WebKitDOMHTMLCollection *cells;
	gulong ii, length;
	cells = webkit_dom_html_table_row_element_get_cells (
			WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));
	length = webkit_dom_html_collection_get_length (cells);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *cell;
		cell = webkit_dom_html_collection_item (cells, ii);
		if (!cell) {
			continue;
		}

		call_cell_dom_func (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell), func, value, user_data);
		g_object_unref (cell);
	}
	g_object_unref (cells);
}

static void
html_editor_cell_dialog_set_attribute (WebKitDOMDocument *document,
                                       guint scope,
                                       gpointer func,
                                       GValue *value,
                                       gpointer user_data)
{
	WebKitDOMElement *cell = get_current_cell_element (document);

	if (scope == SCOPE_CELL) {

		call_cell_dom_func (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell),
			func, value, user_data);

	} else if (scope == SCOPE_COLUMN) {
		gulong index, ii, length;
		WebKitDOMElement *table;
		WebKitDOMHTMLCollection *rows;

		index = webkit_dom_html_table_cell_element_get_cell_index (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell));
		table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
		if (!table) {
			return;
		}

		rows = webkit_dom_html_table_element_get_rows (
				WEBKIT_DOM_HTML_TABLE_ELEMENT (table));
		length = webkit_dom_html_collection_get_length (rows);
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *row, *cell;
			WebKitDOMHTMLCollection *cells;

			row = webkit_dom_html_collection_item (rows, ii);
			cells = webkit_dom_html_table_row_element_get_cells (
					WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));
			cell = webkit_dom_html_collection_item (cells, index);
			if (!cell) {
				g_object_unref (row);
				g_object_unref (cells);
				continue;
			}

			call_cell_dom_func (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell),
				func, value, user_data);
			g_object_unref (row);
			g_object_unref (cells);
			g_object_unref (cell);
		}
		g_object_unref (rows);

	} else if (scope == SCOPE_ROW) {
		WebKitDOMElement *row;

		row = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TR");
		if (!row) {
			return;
		}

		for_each_cell_do (row, func, value, user_data);

	} else if (scope == SCOPE_TABLE) {
		gulong ii, length;
		WebKitDOMElement *table;
		WebKitDOMHTMLCollection *rows;

		table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
		if (!table) {
			return;
		}

		rows = webkit_dom_html_table_element_get_rows (
				WEBKIT_DOM_HTML_TABLE_ELEMENT (table));
		length = webkit_dom_html_collection_get_length (rows);
		for (ii = 0; ii < length; ii++) {
			WebKitDOMNode *row;

			row = webkit_dom_html_collection_item (rows, ii);
			if (!row) {
				g_object_unref (row);
				continue;
			}

			for_each_cell_do (
				WEBKIT_DOM_ELEMENT (row), func, value, user_data);
			g_object_unref (row);
		}
		g_object_unref (rows);
	}
}

static void
cell_set_header_style (WebKitDOMHTMLTableCellElement *cell,
                       gboolean header_style,
		       gpointer user_data)
{
	WebKitDOMDocument *document;
	WebKitDOMNodeList *nodes;
	WebKitDOMElement *new_cell;
	gulong length, ii;
	gchar *tagname;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (cell));
	tagname = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (cell));

	if (header_style && (g_ascii_strncasecmp (tagname, "TD", 2) == 0)) {

		new_cell = webkit_dom_document_create_element (document, "TH", NULL);

	} else if (!header_style && (g_ascii_strncasecmp (tagname, "TH", 2) == 0)) {

		new_cell = webkit_dom_document_create_element (document, "TD", NULL);

	} else {
		g_free (tagname);
		return;
	}

	webkit_dom_element_set_id (new_cell, "-x-evo-current-cell");

	/* Move all child nodes from cell to new_cell */
	nodes = webkit_dom_node_get_child_nodes (WEBKIT_DOM_NODE (cell));
	length = webkit_dom_node_list_get_length (nodes);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node;

		node = webkit_dom_node_list_item (nodes, ii);
		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (new_cell), node, NULL);
		g_object_unref (node);
	}
	g_object_unref (nodes);

	/* Insert new_cell before cell and remove cell */
	webkit_dom_node_insert_before (
		webkit_dom_node_get_parent_node (
			WEBKIT_DOM_NODE (cell)),
		WEBKIT_DOM_NODE (new_cell),
		WEBKIT_DOM_NODE (cell), NULL);

	webkit_dom_node_remove_child (
		webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (cell)),
		WEBKIT_DOM_NODE (cell), NULL);

	g_free (tagname);
}

void
e_html_editor_cell_dialog_mark_current_cell_element (WebKitDOMDocument *document,
                                                     EHTMLEditorWebExtension *extension,
                                                     const gchar *id)
{
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMElement *element, *parent = NULL;

	element = webkit_dom_document_get_element_by_id (document, id);

	parent = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TD");
	if (!parent)
		parent = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TH");

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-current-cell");
	if (element)
		webkit_dom_element_remove_attribute (element, "id");

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EHTMLEditorHistoryEvent *ev;
		WebKitDOMElement *table;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_DIALOG;

		dom_selection_get_coordinates (
			document,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		table = dom_node_find_parent_element (
			WEBKIT_DOM_NODE (parent), "TABLE");
		ev->data.dom.from = webkit_dom_node_clone_node_with_error (
			WEBKIT_DOM_NODE (table), TRUE, NULL);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	webkit_dom_element_set_id (parent, "-x-evo-current-cell");

}

void
e_html_editor_cell_dialog_save_history_on_exit (WebKitDOMDocument *document,
                                                EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *cell, *table;

	cell = get_current_cell_element (document);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	webkit_dom_element_remove_attribute (cell, "id");

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (table), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		dom_selection_get_coordinates (
			document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_html_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

void
e_html_editor_cell_dialog_set_element_v_align (WebKitDOMDocument *document,
                                               const gchar *v_align,
                                               guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, v_align);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_v_align, &val, NULL);

	g_value_unset (&val);
}

void
e_html_editor_cell_dialog_set_element_align (WebKitDOMDocument *document,
                                             const gchar *align,
                                             guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, align);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_align, &val, NULL);

	g_value_unset (&val);
}

void
e_html_editor_cell_dialog_set_element_no_wrap (WebKitDOMDocument *document,
                                               gboolean wrap_text,
                                               guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, wrap_text);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_no_wrap, &val, NULL);
}

void
e_html_editor_cell_dialog_set_element_header_style (WebKitDOMDocument *document,
                                                    gboolean header_style,
                                                    guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, header_style);

	html_editor_cell_dialog_set_attribute (
		document, scope, cell_set_header_style, &val, NULL);
}

void
e_html_editor_cell_dialog_set_element_width (WebKitDOMDocument *document,
                                             const gchar *width,
                                             guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, width);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_width, &val, NULL);

	g_value_unset (&val);
}

void
e_html_editor_cell_dialog_set_element_col_span (WebKitDOMDocument *document,
                                                glong span,
                                                guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_LONG);
	g_value_set_long (&val, span);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_col_span, &val, NULL);
}

void
e_html_editor_cell_dialog_set_element_row_span (WebKitDOMDocument *document,
                                                glong span,
                                                guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_LONG);
	g_value_set_long (&val, span);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_row_span, &val, NULL);
}

void
e_html_editor_cell_dialog_set_element_bg_color (WebKitDOMDocument *document,
                                                const gchar *color,
                                                guint scope)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, color);

	html_editor_cell_dialog_set_attribute (
		document, scope, webkit_dom_html_table_cell_element_set_bg_color, &val, NULL);
}

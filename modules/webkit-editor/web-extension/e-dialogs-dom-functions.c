/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#undef WEBKIT_DOM_USE_UNSTABLE_API

#include "web-extensions/e-dom-utils.h"

#include "e-editor-dom-functions.h"
#include "e-editor-undo-redo-manager.h"

#include "e-dialogs-dom-functions.h"

/* ******************** Cell Dialog ***************** */

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
cell_dialog_set_attribute (WebKitDOMDocument *document,
			   EContentEditorScope scope,
			   gpointer func,
			   GValue *value,
			   gpointer user_data)
{
	WebKitDOMElement *cell = get_current_cell_element (document);

	if (scope == E_CONTENT_EDITOR_SCOPE_CELL) {

		call_cell_dom_func (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (cell),
			func, value, user_data);

	} else if (scope == E_CONTENT_EDITOR_SCOPE_COLUMN) {
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

	} else if (scope == E_CONTENT_EDITOR_SCOPE_ROW) {
		WebKitDOMElement *row;

		row = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TR");
		if (!row) {
			return;
		}

		for_each_cell_do (row, func, value, user_data);

	} else if (scope == E_CONTENT_EDITOR_SCOPE_TABLE) {
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
e_dialogs_dom_cell_mark_current_cell_element (EEditorPage *editor_page,
					      const gchar *id)
{
	EEditorUndoRedoManager *manager;
	WebKitDOMElement *element, *parent = NULL;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));
	g_return_if_fail (id != NULL);

	document = e_editor_page_get_document (editor_page);

	element = webkit_dom_document_get_element_by_id (document, id);

	parent = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TD");
	if (!parent)
		parent = dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TH");

	element = webkit_dom_document_get_element_by_id (document, "-x-evo-current-cell");
	if (element)
		webkit_dom_element_remove_attribute (element, "id");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;
		WebKitDOMElement *table;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_DIALOG;

		e_editor_dom_selection_get_coordinates (editor_page,
			&ev->before.start.x,
			&ev->before.start.y,
			&ev->before.end.x,
			&ev->before.end.y);

		table = dom_node_find_parent_element (
			WEBKIT_DOM_NODE (parent), "TABLE");
		ev->data.dom.from = webkit_dom_node_clone_node_with_error (
			WEBKIT_DOM_NODE (table), TRUE, NULL);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	webkit_dom_element_set_id (parent, "-x-evo-current-cell");
}

void
e_dialogs_dom_cell_save_history_on_exit (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *cell, *table;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	cell = get_current_cell_element (document);

	table = dom_node_find_parent_element (WEBKIT_DOM_NODE (cell), "TABLE");
	g_return_if_fail (table != NULL);

	webkit_dom_element_remove_attribute (cell, "id");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	ev = e_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (table), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

void
e_dialogs_dom_cell_set_element_v_align (EEditorPage *editor_page,
					const gchar *v_align,
					EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, v_align);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_v_align, &val, NULL);

	g_value_unset (&val);
}

void
e_dialogs_dom_cell_set_element_align (EEditorPage *editor_page,
				      const gchar *align,
				      EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, align);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_align, &val, NULL);

	g_value_unset (&val);
}

void
e_dialogs_dom_cell_set_element_no_wrap (EEditorPage *editor_page,
					gboolean wrap_text,
					EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, wrap_text);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_no_wrap, &val, NULL);
}

void
e_dialogs_dom_cell_set_element_header_style (EEditorPage *editor_page,
					     gboolean header_style,
					     EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, header_style);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, cell_set_header_style, &val, NULL);
}

void
e_dialogs_dom_cell_set_element_width (EEditorPage *editor_page,
				      const gchar *width,
				      EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, width);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_width, &val, NULL);

	g_value_unset (&val);
}

void
e_dialogs_dom_cell_set_element_col_span (EEditorPage *editor_page,
					 glong span,
					 EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_LONG);
	g_value_set_long (&val, span);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_col_span, &val, NULL);
}

void
e_dialogs_dom_cell_set_element_row_span (EEditorPage *editor_page,
					 glong span,
					 EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_LONG);
	g_value_set_long (&val, span);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_row_span, &val, NULL);
}

void
e_dialogs_dom_cell_set_element_bg_color (EEditorPage *editor_page,
					 const gchar *color,
					 EContentEditorScope scope)
{
	GValue val = { 0 };

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, color);

	cell_dialog_set_attribute (e_editor_page_get_document (editor_page),
		scope, webkit_dom_html_table_cell_element_set_bg_color, &val, NULL);
}

/* ******************** HRule Dialog ***************** */

static WebKitDOMElement *
get_current_hrule_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-hr");
}

gboolean
e_dialogs_dom_hrule_find_hrule (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	gboolean created = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *rule;
	WebKitDOMNode *node_under_mouse_click;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);

	node_under_mouse_click = e_editor_page_get_node_under_mouse_click (editor_page);

	if (node_under_mouse_click && WEBKIT_DOM_IS_HTML_HR_ELEMENT (node_under_mouse_click)) {
		rule = WEBKIT_DOM_ELEMENT (node_under_mouse_click);
		webkit_dom_element_set_id (rule, "-x-evo-current-hr");
	} else {
		WebKitDOMElement *selection_start, *parent;

		e_editor_dom_selection_save (editor_page);

		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start));

		rule = webkit_dom_document_create_element (document, "HR", NULL);
		webkit_dom_element_set_id (rule, "-x-evo-current-hr");

		/* Insert horizontal rule into body below the caret */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
			WEBKIT_DOM_NODE (rule),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
			NULL);

		e_editor_dom_selection_restore (editor_page);

		e_editor_page_emit_content_changed (editor_page);

		created = TRUE;
	}

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_HRULE_DIALOG;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (!created)
			ev->data.dom.from = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (rule), FALSE, NULL);
		else
			ev->data.dom.from = NULL;

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	return created;
}

void
e_dialogs_dom_save_history_on_exit (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	element = get_current_hrule_element (document);
	g_return_if_fail (element != NULL);

	webkit_dom_element_remove_attribute (element, "id");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	ev = e_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (element), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

/* ******************** Image Dialog ***************** */

static WebKitDOMElement *
get_current_image_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-img");
}

void
e_dialogs_dom_image_mark_image (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	WebKitDOMNode *node_under_mouse_click;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	node_under_mouse_click = e_editor_page_get_node_under_mouse_click (editor_page);

	g_return_if_fail (node_under_mouse_click && WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node_under_mouse_click));

	webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (node_under_mouse_click), "-x-evo-current-img");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_IMAGE_DIALOG;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		ev->data.dom.from = webkit_dom_node_clone_node_with_error (node_under_mouse_click, FALSE, NULL);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

void
e_dialogs_dom_image_save_history_on_exit (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	EEditorHistoryEvent *ev = NULL;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	element = get_current_image_element (document);
	g_return_if_fail (element != NULL);

	webkit_dom_element_remove_attribute (element, "id");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	ev = e_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (element), TRUE, NULL);

	e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
}

void
e_dialogs_dom_image_set_element_url (EEditorPage *editor_page,
				     const gchar *url)
{
	WebKitDOMElement *image, *link;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	image = get_current_image_element (e_editor_page_get_document (editor_page));
	link = dom_node_find_parent_element (WEBKIT_DOM_NODE (image), "A");

	if (link) {
		if (!url || !*url) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (link)),
				WEBKIT_DOM_NODE (image),
				WEBKIT_DOM_NODE (link), NULL);
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (link)),
				WEBKIT_DOM_NODE (link), NULL);
		} else {
			webkit_dom_html_anchor_element_set_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);
		}
	} else {
		if (url && *url) {
			WebKitDOMDocument *document;

			document = webkit_dom_node_get_owner_document (
					WEBKIT_DOM_NODE (image));
			link = webkit_dom_document_create_element (
					document, "A", NULL);

			webkit_dom_html_anchor_element_set_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (image)),
				WEBKIT_DOM_NODE (link),
				WEBKIT_DOM_NODE (image), NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (link),
				WEBKIT_DOM_NODE (image), NULL);
		}
	}
}

gchar *
e_dialogs_dom_image_get_element_url (EEditorPage *editor_page)
{
	gchar *value;
	WebKitDOMElement *image, *link;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	image = get_current_image_element (e_editor_page_get_document (editor_page));
	link = dom_node_find_parent_element (WEBKIT_DOM_NODE (image), "A");

	value = webkit_dom_html_anchor_element_get_href (
		WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link));

	return value;
}

/* ******************** Link Dialog ***************** */

void
e_dialogs_dom_link_commit (EEditorPage *editor_page,
			   const gchar *url,
			   const gchar *inner_text)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *link;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);
	link = webkit_dom_document_get_element_by_id (document, "-x-evo-current-anchor");

	if (link) {
		WebKitDOMElement *element;

		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (link), inner_text, NULL);

		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_element_set_id (element, "-x-evo-selection-end-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (link)),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (link)),
			NULL);

		element = webkit_dom_document_create_element (document, "SPAN", NULL);
		webkit_dom_element_set_id (element, "-x-evo-selection-start-marker");
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (link)),
			WEBKIT_DOM_NODE (element),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (link)),
			NULL);

		e_editor_dom_selection_restore (editor_page);
	} else {
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_object_unref (dom_window);

		e_editor_dom_selection_restore (editor_page);
		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		if (webkit_dom_range_get_collapsed (range, NULL)) {
			WebKitDOMElement *selection_marker;
			WebKitDOMElement *anchor;

			e_editor_dom_selection_save (editor_page);
			selection_marker = webkit_dom_document_get_element_by_id (
				document, "-x-evo-selection-start-marker");
			anchor = webkit_dom_document_create_element (document, "A", NULL);
			webkit_dom_element_set_attribute (anchor, "href", url, NULL);
			webkit_dom_element_set_id (anchor, "-x-evo-current-anchor");
			webkit_dom_html_element_set_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (anchor), inner_text, NULL);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (selection_marker)),
				WEBKIT_DOM_NODE (anchor),
				WEBKIT_DOM_NODE (selection_marker),
				NULL);
			e_editor_dom_selection_restore (editor_page);
		} else {
			gchar *text;

			text = webkit_dom_range_get_text (range);
			if (text && *text) {
				EEditorUndoRedoManager *manager;
				EEditorHistoryEvent *ev;

				e_editor_dom_create_link (editor_page, url);

				manager = e_editor_page_get_undo_redo_manager (editor_page);
				ev = e_editor_undo_redo_manager_get_current_history_event (manager);

				ev->data.dom.from =
					WEBKIT_DOM_NODE (webkit_dom_document_create_text_node (document, text));

				webkit_dom_dom_selection_collapse_to_end (dom_selection, NULL);
			}
			g_free (text);
		}

		g_object_unref (range);
		g_object_unref (dom_selection);
	}
}

void
e_dialogs_dom_link_close (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *link;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	link = webkit_dom_document_get_element_by_id (document, "-x-evo-current-anchor");
	if (link) {
		EEditorUndoRedoManager *manager;
		EEditorHistoryEvent *ev;

		manager = e_editor_page_get_undo_redo_manager (editor_page);
		ev = e_editor_undo_redo_manager_get_current_history_event (manager);
		if (ev->type == HISTORY_LINK_DIALOG) {
			ev->data.dom.to = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (link), TRUE, NULL);

			if (ev->data.dom.from && webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to))
				e_editor_undo_redo_manager_remove_current_history_event (manager);
			else
				e_editor_dom_selection_get_coordinates (
					editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
		}
		webkit_dom_element_remove_attribute (link, "id");
	}
}

void
e_dialogs_dom_link_open (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	WebKitDOMDocument *document;
	WebKitDOMElement *link = NULL;
	WebKitDOMNode *node_under_mouse_click;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	node_under_mouse_click = e_editor_page_get_node_under_mouse_click (editor_page);
	if (node_under_mouse_click && WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node_under_mouse_click)) {
		link = WEBKIT_DOM_ELEMENT (node_under_mouse_click);
	} else {
		if (!(link = webkit_dom_document_get_element_by_id (document, "-x-evo-current-anchor"))) {
			if (node_under_mouse_click) {
				link = dom_node_find_parent_element (node_under_mouse_click, "A");
			} else {
				WebKitDOMElement *selection_start;

				e_editor_dom_selection_save (editor_page);

				selection_start = webkit_dom_document_get_element_by_id (
					document, "-x-evo-selection-start-marker");

				link = dom_node_find_parent_element (WEBKIT_DOM_NODE (selection_start), "A");

				e_editor_dom_selection_restore (editor_page);
			}
		}
	}

	if (link)
		webkit_dom_element_set_id (link, "-x-evo-current-anchor");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_LINK_DIALOG;

		e_editor_dom_selection_get_coordinates (
			editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (link)
			ev->data.dom.from = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (link), TRUE, NULL);
		else
			ev->data.dom.from = NULL;
		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

GVariant *
e_dialogs_dom_link_show (EEditorPage *editor_page)
{
	GVariant *result = NULL;
	WebKitDOMDocument *document;
	WebKitDOMElement *link;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

	e_editor_dom_selection_save (editor_page);

	link = webkit_dom_document_get_element_by_id (document, "-x-evo-current-anchor");
	if (link) {
		gchar *href, *text;

		href = webkit_dom_element_get_attribute (link, "href");
		text = webkit_dom_html_element_get_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (link));

		result = g_variant_new ("(ss)", href, text);

		g_free (text);
		g_free (href);
	} else {
		gchar *text;
		WebKitDOMDOMWindow *dom_window;
		WebKitDOMDOMSelection *dom_selection;
		WebKitDOMRange *range;

		dom_window = webkit_dom_document_get_default_view (document);
		dom_selection = webkit_dom_dom_window_get_selection (dom_window);
		g_object_unref (dom_window);

		/* No selection at all */
		if (!dom_selection || webkit_dom_dom_selection_get_range_count (dom_selection) < 1)
			result = g_variant_new ("(ss)", "", "");

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		text = webkit_dom_range_get_text (range);
		if (text)
			result = g_variant_new ("(ss)", "", text);

		g_free (text);

		g_object_unref (range);
		g_object_unref (dom_selection);
	}

	return result;
}

/* ******************** Page Dialog ***************** */

void
e_dialogs_dom_page_save_history (EEditorPage *editor_page)
{
	EEditorUndoRedoManager *manager;
	WebKitDOMDocument *document;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;
		WebKitDOMHTMLElement *body;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_PAGE_DIALOG;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		body = webkit_dom_document_get_body (document);
		ev->data.dom.from = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (body), FALSE, NULL);

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

void
e_dialogs_dom_page_save_history_on_exit (EEditorPage *editor_page)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	ev = e_editor_undo_redo_manager_get_current_history_event (manager);
	body = webkit_dom_document_get_body (document);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (WEBKIT_DOM_NODE (body), FALSE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

/* ******************** Spell Check Dialog ***************** */

static gboolean
select_next_word (WebKitDOMDOMSelection *dom_selection)
{
	gulong anchor_offset, focus_offset;
	WebKitDOMNode *anchor, *focus;

	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	focus = webkit_dom_dom_selection_get_focus_node (dom_selection);
	focus_offset = webkit_dom_dom_selection_get_focus_offset (dom_selection);

	/* Jump _behind_ next word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "forward", "word");
	/* Jump before the word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "forward", "word");

	/* If the selection didn't change, then we have most probably
	 * reached the end of document - return FALSE */
	return !((anchor == webkit_dom_dom_selection_get_anchor_node (dom_selection)) &&
		 (anchor_offset == webkit_dom_dom_selection_get_anchor_offset (dom_selection)) &&
		 (focus == webkit_dom_dom_selection_get_focus_node (dom_selection)) &&
		 (focus_offset == webkit_dom_dom_selection_get_focus_offset (dom_selection)));
}

static gboolean
select_previous_word (WebKitDOMDOMSelection *dom_selection)
{
	WebKitDOMNode *old_anchor_node;
	WebKitDOMNode *new_anchor_node;
	gulong old_anchor_offset;
	gulong new_anchor_offset;

	old_anchor_node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	old_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	/* Jump on the beginning of current word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Jump before previous word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "forward", "word");

	/* If the selection start didn't change, then we have most probably
	 * reached the beginnig of document. Return FALSE */

	new_anchor_node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	new_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	return (new_anchor_node != old_anchor_node) ||
		(new_anchor_offset != old_anchor_offset);
}

static gchar *
e_dialogs_dom_spell_check_run (EEditorPage *editor_page,
			       gboolean run_next,
			       const gchar *from_word,
			       const gchar * const *languages)
{
	gulong start_offset = 0, end_offset = 0;
	WebKitDOMDocument *document;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNode *start = NULL, *end = NULL;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!from_word || !*from_word) {
		if (run_next) {
			webkit_dom_dom_selection_modify (
				dom_selection, "move", "left", "documentboundary");
		} else {
			webkit_dom_dom_selection_modify (
				dom_selection, "move", "right", "documentboundary");
			webkit_dom_dom_selection_modify (
				dom_selection, "extend", "backward", "word");
		}
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (dom_selection);
		end = webkit_dom_dom_selection_get_focus_node (dom_selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (dom_selection);
	}

	while ((run_next ? select_next_word (dom_selection) : select_previous_word (dom_selection))) {
		WebKitDOMRange *range;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		word = webkit_dom_range_get_text (range);
		g_object_unref (range);

		if (!e_editor_page_check_word_spelling (editor_page, word, languages)) {
			/* Found misspelled word! */
			return word;
		}

		g_free (word);
	}

	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the beginning/end of the document */
	if (start && end)
		webkit_dom_dom_selection_set_base_and_extent (
			dom_selection, start, start_offset, end, end_offset, NULL);

	g_object_unref (dom_selection);

	return NULL;
}

gchar *
e_dialogs_dom_spell_check_next (EEditorPage *editor_page,
				const gchar *from_word,
				const gchar * const *languages)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return e_dialogs_dom_spell_check_run (editor_page, TRUE, from_word, languages);
}

gchar *
e_dialogs_dom_spell_check_prev (EEditorPage *editor_page,
				const gchar *from_word,
				const gchar * const *languages)
{
	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	return e_dialogs_dom_spell_check_run (editor_page, FALSE, from_word, languages);
}

/* ******************** Table Dialog ***************** */

static WebKitDOMHTMLTableElement *
get_current_table_element (WebKitDOMDocument *document)
{
	return WEBKIT_DOM_HTML_TABLE_ELEMENT (webkit_dom_document_get_element_by_id (document, "-x-evo-current-table"));
}

void
e_dialogs_dom_table_set_row_count (EEditorPage *editor_page,
				   gulong expected_count)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;
	gulong ii, current_count;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

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
e_dialogs_dom_table_get_row_count (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;
	glong count;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	document = e_editor_page_get_document (editor_page);

	table_element = get_current_table_element (document);
	if (!table_element)
		return 0;

	rows = webkit_dom_html_table_element_get_rows (table_element);

	count = webkit_dom_html_collection_get_length (rows);
	g_object_unref (rows);

	return count;
}

void
e_dialogs_dom_table_set_column_count (EEditorPage *editor_page,
				      gulong expected_columns)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows;
	gulong ii, row_count;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

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
e_dialogs_dom_table_get_column_count (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMHTMLTableElement *table_element;
	WebKitDOMHTMLCollection *rows, *columns;
	WebKitDOMNode *row;
	glong count;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), 0);

	document = e_editor_page_get_document (editor_page);

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
create_table (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *table, *br, *caret, *element, *cell;
	WebKitDOMNode *clone;
	gboolean empty = FALSE;
	gchar *text_content;
	gint i;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), NULL);

	document = e_editor_page_get_document (editor_page);

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

	e_editor_dom_selection_save (editor_page);
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

	e_editor_dom_selection_restore (editor_page);

	e_editor_page_emit_content_changed (editor_page);

	return table;
}

gboolean
e_dialogs_dom_table_show (EEditorPage *editor_page)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMElement *table = NULL;
	EEditorUndoRedoManager *manager;
	gboolean created = FALSE;

	g_return_val_if_fail (E_IS_EDITOR_PAGE (editor_page), FALSE);

	document = e_editor_page_get_document (editor_page);
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
			table = create_table (editor_page);
			created = TRUE;
		}
	}

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	if (!e_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EEditorHistoryEvent *ev;

		ev = g_new0 (EEditorHistoryEvent, 1);
		ev->type = HISTORY_TABLE_DIALOG;

		e_editor_dom_selection_get_coordinates (editor_page, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (!created)
			ev->data.dom.from = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (table), TRUE, NULL);
		else
			ev->data.dom.from = NULL;

		e_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	g_object_unref (dom_selection);

	return created;
}

void
e_dialogs_dom_table_save_history_on_exit (EEditorPage *editor_page)
{
	EEditorHistoryEvent *ev = NULL;
	EEditorUndoRedoManager *manager;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	document = e_editor_page_get_document (editor_page);

	element = WEBKIT_DOM_ELEMENT (get_current_table_element (document));
	g_return_if_fail (element != NULL);

	webkit_dom_element_remove_attribute (element, "id");

	manager = e_editor_page_get_undo_redo_manager (editor_page);
	ev = e_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (element), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		e_editor_dom_selection_get_coordinates (editor_page, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}

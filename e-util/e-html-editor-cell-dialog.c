/*
 * e-html-editor-cell-dialog.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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

#include "e-html-editor-cell-dialog.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

#include "e-color-combo.h"
#include "e-dialog-widgets.h"
#include "e-html-editor-utils.h"
#include "e-image-chooser-dialog.h"
#include "e-misc-utils.h"
#include "e-misc-utils.h"

#define E_HTML_EDITOR_CELL_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_CELL_DIALOG, EHTMLEditorCellDialogPrivate))

struct _EHTMLEditorCellDialogPrivate {
	GtkWidget *scope_cell_button;
	GtkWidget *scope_table_button;
	GtkWidget *scope_row_button;
	GtkWidget *scope_column_button;

	GtkWidget *halign_combo;
	GtkWidget *valign_combo;

	GtkWidget *wrap_text_check;
	GtkWidget *header_style_check;

	GtkWidget *width_check;
	GtkWidget *width_edit;
	GtkWidget *width_units;

	GtkWidget *row_span_edit;
	GtkWidget *col_span_edit;

	GtkWidget *background_color_picker;
	GtkWidget *background_image_chooser;

	GtkWidget *remove_image_button;

	WebKitDOMElement *cell;
	guint scope;

	EHTMLEditorViewHistoryEvent *history_event;
};

enum {
	SCOPE_CELL,
	SCOPE_ROW,
	SCOPE_COLUMN,
	SCOPE_TABLE
} DialogScope;

static GdkRGBA transparent = { 0, 0, 0, 0 };

typedef void (*DOMStrFunc) (WebKitDOMHTMLTableCellElement *cell, const gchar *val, gpointer user_data);
typedef void (*DOMUlongFunc) (WebKitDOMHTMLTableCellElement *cell, gulong val, gpointer user_data);
typedef void (*DOMBoolFunc) (WebKitDOMHTMLTableCellElement *cell, gboolean val, gpointer user_data);

G_DEFINE_TYPE (
	EHTMLEditorCellDialog,
	e_html_editor_cell_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
call_cell_dom_func (WebKitDOMHTMLTableCellElement *cell,
                    gpointer func,
                    GValue *value,
                    gpointer user_data)
{
	if (G_VALUE_HOLDS_STRING (value)) {
		DOMStrFunc f = func;
		f (cell, g_value_get_string (value), user_data);
	} else if (G_VALUE_HOLDS_ULONG (value)) {
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
html_editor_cell_dialog_set_attribute (EHTMLEditorCellDialog *dialog,
                                       gpointer func,
                                       GValue *value,
                                       gpointer user_data)
{
	if (dialog->priv->scope == SCOPE_CELL) {

		call_cell_dom_func (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell),
			func, value, user_data);

	} else if (dialog->priv->scope == SCOPE_COLUMN) {
		gulong index, ii, length;
		WebKitDOMElement *table;
		WebKitDOMHTMLCollection *rows;

		index = webkit_dom_html_table_cell_element_get_cell_index (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell));
		table = e_html_editor_dom_node_find_parent_element (
				WEBKIT_DOM_NODE (dialog->priv->cell), "TABLE");
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

	} else if (dialog->priv->scope == SCOPE_ROW) {
		WebKitDOMElement *row;

		row = e_html_editor_dom_node_find_parent_element (
				WEBKIT_DOM_NODE (dialog->priv->cell), "TR");
		if (!row) {
			return;
		}

		for_each_cell_do (row, func, value, user_data);

	} else if (dialog->priv->scope == SCOPE_TABLE) {
		gulong ii, length;
		WebKitDOMElement *table;
		WebKitDOMHTMLCollection *rows;

		table = e_html_editor_dom_node_find_parent_element (
				WEBKIT_DOM_NODE (dialog->priv->cell), "TABLE");
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
html_editor_cell_dialog_set_scope (EHTMLEditorCellDialog *dialog)
{
	if (gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_cell_button))) {

		dialog->priv->scope = SCOPE_CELL;

	} else if (gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_row_button))) {

		dialog->priv->scope = SCOPE_ROW;

	} else if (gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_column_button))) {

		dialog->priv->scope = SCOPE_COLUMN;

	} else if (gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_table_button))) {

		dialog->priv->scope = SCOPE_TABLE;

	}
}

static  void
html_editor_cell_dialog_set_valign (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (
		&val,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->valign_combo)));

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_v_align, &val, NULL);

	g_value_unset (&val);
}

static void
html_editor_cell_dialog_set_halign (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (
		&val,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->halign_combo)));

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_align, &val, NULL);

	g_value_unset (&val);
}

static void
html_editor_cell_dialog_set_wrap_text (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (
		&val,
		!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->wrap_text_check)));

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_no_wrap, &val, NULL);
}

static void
cell_set_header_style (WebKitDOMHTMLTableCellElement *cell,
                       gboolean header_style,
                       gpointer user_data)
{
	EHTMLEditorCellDialog *dialog = user_data;
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

	dialog->priv->cell = new_cell;

	g_free (tagname);
}

static void
html_editor_cell_dialog_set_header_style (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (
		&val,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->header_style_check)));

	html_editor_cell_dialog_set_attribute (
		dialog, cell_set_header_style, &val, dialog);
}

static void
html_editor_cell_dialog_set_width (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };
	gchar *width;

	if (!gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->width_check))) {

		width = g_strdup ("auto");
	} else {

		width = g_strdup_printf (
			"%d%s",
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->width_edit)),
			((gtk_combo_box_get_active (
				GTK_COMBO_BOX (dialog->priv->width_units)) == 0) ?
					"px" : "%"));
	}

	g_value_init (&val, G_TYPE_STRING);
	g_value_take_string (&val, width);
	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_width, &val, NULL);

	g_free (width);
}

static void
html_editor_cell_dialog_set_column_span (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_ULONG);
	g_value_set_ulong (
		&val,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->col_span_edit)));

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_col_span, &val, NULL);
}

static void
html_editor_cell_dialog_set_row_span (EHTMLEditorCellDialog *dialog)
{
	GValue val = { 0 };

	g_value_init (&val, G_TYPE_ULONG);
	g_value_set_ulong (
		&val,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->row_span_edit)));

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_row_span, &val, NULL);
}

static void
html_editor_cell_dialog_set_background_color (EHTMLEditorCellDialog *dialog)
{
	gchar *color = NULL;
	GdkRGBA rgba;
	GValue val = { 0 };

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
	if (rgba.alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	else
		color = g_strdup ("");

	g_value_init (&val, G_TYPE_STRING);
	g_value_take_string (&val, color);

	html_editor_cell_dialog_set_attribute (
		dialog, webkit_dom_html_table_cell_element_set_bg_color, &val, NULL);

	g_free (color);
}

static void
cell_set_background_image (WebKitDOMHTMLTableCellElement *cell,
                           const gchar *uri,
                           EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	if (uri && *uri) {
		e_html_editor_selection_replace_image_src (
			e_html_editor_view_get_selection (view),
			WEBKIT_DOM_ELEMENT (cell),
			uri);
	} else
		remove_image_attributes_from_element (WEBKIT_DOM_ELEMENT (cell));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);
}

static void
html_editor_cell_dialog_set_background_image (EHTMLEditorCellDialog *dialog)
{
	gchar *uri;
	GValue val = { 0 };

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	g_value_init (&val, G_TYPE_STRING);
	g_value_take_string (&val, uri);

	html_editor_cell_dialog_set_attribute (
		dialog, cell_set_background_image, &val, dialog);
}

static void
html_editor_cell_dialog_remove_image (EHTMLEditorCellDialog *dialog)
{
	remove_image_attributes_from_element (
		WEBKIT_DOM_ELEMENT (dialog->priv->cell));

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_cell_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorCellDialog *dialog;
	EHTMLEditorView *view;
	gchar *tmp;
	GdkRGBA color;

	dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		EHTMLEditorViewHistoryEvent *ev;
		WebKitDOMElement *table;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_TABLE_DIALOG;

		e_html_editor_selection_get_selection_coordinates (
			e_html_editor_view_get_selection (view),
			&ev->before.start.x, &ev->before.start.y,
			&ev->before.end.x, &ev->before.end.y);

		table = e_html_editor_dom_node_find_parent_element (
				WEBKIT_DOM_NODE (dialog->priv->cell), "TABLE");
		ev->data.dom.from = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (table), TRUE);
		dialog->priv->history_event = ev;
	}

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_cell_button), TRUE);

	tmp = webkit_dom_html_table_cell_element_get_align (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell));
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->halign_combo),
		(tmp && *tmp) ? tmp : "left");
	g_free (tmp);

	tmp = webkit_dom_html_table_cell_element_get_v_align (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell));
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->valign_combo),
		(tmp && *tmp) ? tmp : "middle");
	g_free (tmp);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->wrap_text_check),
		!webkit_dom_html_table_cell_element_get_no_wrap (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell)));

	tmp = webkit_dom_element_get_tag_name (
		WEBKIT_DOM_ELEMENT (dialog->priv->cell));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->header_style_check),
		(g_ascii_strncasecmp (tmp, "TH", 2) == 0));
	g_free (tmp);

	tmp = webkit_dom_html_table_cell_element_get_width (
		WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell));
	if (tmp && *tmp) {
		gint val = atoi (tmp);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), val);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check), TRUE);
	} else {
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 0);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check), FALSE);
	}
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->width_units), "units-px");
	g_free (tmp);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->row_span_edit),
		webkit_dom_html_table_cell_element_get_row_span (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell)));
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->col_span_edit),
		webkit_dom_html_table_cell_element_get_col_span (
			WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell)));

	if (webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (dialog->priv->cell), "background")) {
		tmp = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (dialog->priv->cell), "data-uri");

		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser),
			tmp);

		g_free (tmp);
	} else {
		gtk_file_chooser_unselect_all (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));
	}

	tmp = webkit_dom_html_table_cell_element_get_bg_color (
		WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (dialog->priv->cell));
	if (tmp && *tmp) {
		if (gdk_rgba_parse (&color, tmp)) {
			e_color_combo_set_current_color (
				E_COLOR_COMBO (dialog->priv->background_color_picker),
				&color);
		} else {
			e_color_combo_set_current_color (
				E_COLOR_COMBO (dialog->priv->background_color_picker),
				&transparent);
		}
	} else {
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_picker),
			&transparent);
	}
	g_free (tmp);

	GTK_WIDGET_CLASS (e_html_editor_cell_dialog_parent_class)->show (widget);
}

static void
html_editor_cell_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorCellDialogPrivate *priv;
	EHTMLEditorViewHistoryEvent *ev;

	priv = E_HTML_EDITOR_CELL_DIALOG_GET_PRIVATE (widget);
	ev = priv->history_event;

	if (ev) {
		EHTMLEditorCellDialog *dialog;
		EHTMLEditor *editor;
		EHTMLEditorSelection *selection;
		EHTMLEditorView *view;
		WebKitDOMElement *table;

		dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
		editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
		view = e_html_editor_get_view (editor);
		selection = e_html_editor_view_get_selection (view);

		table = e_html_editor_dom_node_find_parent_element (
				WEBKIT_DOM_NODE (dialog->priv->cell), "TABLE");

		ev->data.dom.to = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (table), TRUE);

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	g_object_unref (priv->cell);
	priv->cell = NULL;

	GTK_WIDGET_CLASS (e_html_editor_cell_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_cell_dialog_class_init (EHTMLEditorCellDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorCellDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_cell_dialog_show;
	widget_class->hide = html_editor_cell_dialog_hide;
}

static void
e_html_editor_cell_dialog_init (EHTMLEditorCellDialog *dialog)
{
	GtkBox *box;
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;
	GtkFileFilter *file_filter;

	dialog->priv = E_HTML_EDITOR_CELL_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Scope == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Scope</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Scope: cell */
	widget = gtk_image_new_from_icon_name ("stock_select-cell", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_radio_button_new_with_mnemonic (NULL, _("C_ell"));
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->scope_cell_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: row */
	widget = gtk_image_new_from_icon_name ("stock_select-row", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		GTK_RADIO_BUTTON (dialog->priv->scope_cell_button), _("_Row"));
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);
	dialog->priv->scope_row_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: table */
	widget = gtk_image_new_from_icon_name ("stock_select-table", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		GTK_RADIO_BUTTON (dialog->priv->scope_cell_button), _("_Table"));
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	dialog->priv->scope_table_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: column */
	widget = gtk_image_new_from_icon_name ("stock_select-column", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 2, 1, 1, 1);

	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		GTK_RADIO_BUTTON (dialog->priv->scope_cell_button), _("Col_umn"));
	gtk_grid_attach (grid, widget, 3, 1, 1, 1);
	dialog->priv->scope_column_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* == Alignment & Behavior == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Alignment &amp; Behavior</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Horizontal */
	widget = gtk_combo_box_text_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "left", _("Left"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "center", _("Center"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "right", _("Right"));
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->halign_combo = widget;

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_cell_dialog_set_halign), dialog);

	widget = gtk_label_new_with_mnemonic (_("_Horizontal:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->halign_combo);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Vertical */
	widget = gtk_combo_box_text_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "top", _("Top"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "middle", _("Middle"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "bottom", _("Bottom"));
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);
	dialog->priv->valign_combo = widget;

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_cell_dialog_set_valign), dialog);

	widget = gtk_label_new_with_mnemonic (_("_Vertical:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->valign_combo);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);

	/* Wrap Text */
	widget = gtk_check_button_new_with_mnemonic (_("_Wrap Text"));
	dialog->priv->wrap_text_check = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_wrap_text), dialog);

	/* Header Style */
	widget = gtk_check_button_new_with_mnemonic (_("_Header Style"));
	dialog->priv->header_style_check = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_header_style), dialog);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start (GTK_BOX (widget), dialog->priv->wrap_text_check, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (widget), dialog->priv->header_style_check, FALSE, FALSE, 0);
	gtk_grid_attach (grid, widget, 0, 1, 4, 1);

	/* == Layout == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Layout</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 4, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 5, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Width */
	widget = gtk_check_button_new_with_mnemonic (_("_Width"));
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	dialog->priv->width_check = widget;

	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->width_edit = widget;

	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_cell_dialog_set_width), dialog);
	e_binding_bind_property (
		dialog->priv->width_check, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "unit-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "unit-percent", "%");
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	dialog->priv->width_units = widget;

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_cell_dialog_set_width), dialog);
	e_binding_bind_property (
		dialog->priv->width_check, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Row Span */
	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 4, 0, 1, 1);
	dialog->priv->row_span_edit = widget;

	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_cell_dialog_set_row_span), dialog);

	widget = gtk_label_new_with_mnemonic (_("Row S_pan:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->row_span_edit);
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);

	/* Column Span */
	widget = gtk_spin_button_new_with_range (0, G_MAXUINT, 1);
	gtk_grid_attach (grid, widget, 4, 1, 1, 1);
	dialog->priv->col_span_edit = widget;

	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_cell_dialog_set_column_span), dialog);

	widget = gtk_label_new_with_mnemonic (_("Co_lumn Span:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->col_span_edit);
	gtk_grid_attach (grid, widget, 3, 1, 1, 1);

	/* == Background == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Background</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 6, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 7, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Color */
	widget = e_color_combo_new ();
	e_color_combo_set_default_color (E_COLOR_COMBO (widget), &transparent);
	e_color_combo_set_default_label (E_COLOR_COMBO (widget), _("Transparent"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_cell_dialog_set_background_color), dialog);
	dialog->priv->background_color_picker = widget;

	widget = gtk_label_new_with_mnemonic (_("C_olor:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_color_picker);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Image */
	widget = e_image_chooser_dialog_new (
			_("Choose Background Image"),
			GTK_WINDOW (dialog));
	dialog->priv->background_image_chooser = widget;

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("Images"));
	gtk_file_filter_add_mime_type (file_filter, "image/*");

	widget = gtk_file_chooser_button_new_with_dialog (
			dialog->priv->background_image_chooser);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), file_filter);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "file-set",
		G_CALLBACK (html_editor_cell_dialog_set_background_image), dialog);
	dialog->priv->background_image_chooser = widget;

	widget =gtk_label_new_with_mnemonic (_("_Image:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_image_chooser);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));
	widget = e_dialog_button_new_with_icon (NULL, _("_Remove image"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_cell_dialog_remove_image), dialog);
	dialog->priv->remove_image_button = widget;

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	gtk_box_reorder_child (box, widget, 0);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_cell_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_CELL_DIALOG,
			"editor", editor,
			"title", _("Cell Properties"),
			NULL));
}

void
e_html_editor_cell_dialog_show (EHTMLEditorCellDialog *dialog,
                                WebKitDOMNode *cell)
{
	EHTMLEditorCellDialogClass *class;

	g_return_if_fail (E_IS_HTML_EDITOR_CELL_DIALOG (dialog));
	g_return_if_fail (cell != NULL);

	dialog->priv->cell = e_html_editor_dom_node_find_parent_element (cell, "TD");
	if (dialog->priv->cell == NULL) {
		dialog->priv->cell =
			e_html_editor_dom_node_find_parent_element (cell, "TH");
	}

	class = E_HTML_EDITOR_CELL_DIALOG_GET_CLASS (dialog);
	GTK_WIDGET_CLASS (class)->show (GTK_WIDGET (dialog));
}


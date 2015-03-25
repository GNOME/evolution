/*
 * e-html-editor-table-dialog.h
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

#include "e-html-editor-table-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-color-combo.h"
#include "e-dialog-widgets.h"
#include "e-html-editor-utils.h"
#include "e-image-chooser-dialog.h"
#include "e-misc-utils.h"

#define E_HTML_EDITOR_TABLE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_TABLE_DIALOG, EHTMLEditorTableDialogPrivate))

struct _EHTMLEditorTableDialogPrivate {
	GtkWidget *rows_edit;
	GtkWidget *columns_edit;

	GtkWidget *width_edit;
	GtkWidget *width_units;
	GtkWidget *width_check;

	GtkWidget *spacing_edit;
	GtkWidget *padding_edit;
	GtkWidget *border_edit;

	GtkWidget *alignment_combo;

	GtkWidget *background_color_button;
	GtkWidget *background_image_button;
	GtkWidget *image_chooser_dialog;

	GtkWidget *remove_image_button;

	WebKitDOMHTMLTableElement *table_element;

	EHTMLEditorViewHistoryEvent *history_event;
};

static GdkRGBA transparent = { 0, 0, 0, 0 };

G_DEFINE_TYPE (
	EHTMLEditorTableDialog,
	e_html_editor_table_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static WebKitDOMElement *
html_editor_table_dialog_create_table (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorSelection *editor_selection;
	EHTMLEditorView *view;
	gint i;
	gchar *text_content;
	gboolean empty = FALSE;
	WebKitDOMDocument *document;
	WebKitDOMElement *table, *br, *caret, *element, *cell;
	WebKitDOMNode *clone;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	editor_selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));

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

	e_html_editor_selection_save (editor_selection);
	caret = webkit_dom_document_get_element_by_id (
		document, "-x-evo-selection-end-marker");

	element = get_parent_block_element (WEBKIT_DOM_NODE (caret));
	text_content = webkit_dom_node_get_text_content (WEBKIT_DOM_NODE (element));
	empty = text_content && !*text_content;
	g_free (text_content);

	clone = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (element), FALSE);
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

	e_html_editor_selection_restore (editor_selection);

	e_html_editor_view_set_changed (view, TRUE);

	return table;
}

static void
html_editor_table_dialog_set_row_count (EHTMLEditorTableDialog *dialog)
{
	WebKitDOMHTMLCollection *rows;
	gulong ii, current_count, expected_count;

	g_return_if_fail (dialog->priv->table_element);

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);
	current_count = webkit_dom_html_collection_get_length (rows);
	expected_count = gtk_spin_button_get_value (
				GTK_SPIN_BUTTON (dialog->priv->rows_edit));

	if (current_count < expected_count) {
		for (ii = 0; ii < expected_count - current_count; ii++) {
			webkit_dom_html_table_element_insert_row (
				dialog->priv->table_element, -1, NULL);
		}
	} else if (current_count > expected_count) {
		for (ii = 0; ii < current_count - expected_count; ii++) {
			webkit_dom_html_table_element_delete_row (
				dialog->priv->table_element, -1, NULL);
		}
	}
	g_object_unref (rows);
}

static void
html_editor_table_dialog_get_row_count (EHTMLEditorTableDialog *dialog)
{
	WebKitDOMHTMLCollection *rows;

	g_return_if_fail (dialog->priv->table_element);

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->rows_edit),
		webkit_dom_html_collection_get_length (rows));
	g_object_unref (rows);
}

static void
html_editor_table_dialog_set_column_count (EHTMLEditorTableDialog *dialog)
{
	WebKitDOMHTMLCollection *rows;
	gulong ii, row_count, expected_columns;

	g_return_if_fail (dialog->priv->table_element);

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);
	row_count = webkit_dom_html_collection_get_length (rows);
	expected_columns = gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->columns_edit));

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

static void
html_editor_table_dialog_get_column_count (EHTMLEditorTableDialog *dialog)
{
	WebKitDOMHTMLCollection *rows, *columns;
	WebKitDOMNode *row;

	g_return_if_fail (dialog->priv->table_element);

	rows = webkit_dom_html_table_element_get_rows (dialog->priv->table_element);
	row = webkit_dom_html_collection_item (rows, 0);

	columns = webkit_dom_html_table_row_element_get_cells (
				WEBKIT_DOM_HTML_TABLE_ROW_ELEMENT (row));

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->columns_edit),
		webkit_dom_html_collection_get_length (columns));
	g_object_unref (row);
	g_object_unref (rows);
	g_object_unref (columns);
}

static void
html_editor_table_dialog_set_width (EHTMLEditorTableDialog *dialog)
{
	gchar *width;

	g_return_if_fail (dialog->priv->table_element);

	if (gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check))) {
		gchar *units;

		units = gtk_combo_box_text_get_active_text (
				GTK_COMBO_BOX_TEXT (dialog->priv->width_units));
		width = g_strdup_printf (
			"%d%s",
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->width_edit)),
			units);
		g_free (units);

		gtk_widget_set_sensitive (dialog->priv->width_edit, TRUE);
		gtk_widget_set_sensitive (dialog->priv->width_units, TRUE);
	} else {
		width = g_strdup ("auto");

		gtk_widget_set_sensitive (dialog->priv->width_edit, FALSE);
		gtk_widget_set_sensitive (dialog->priv->width_units, FALSE);
	}

	webkit_dom_html_table_element_set_width (
		dialog->priv->table_element, width);
	g_free (width);
}

static void
html_editor_table_dialog_get_width (EHTMLEditorTableDialog *dialog)
{
	gchar *width;

	width = webkit_dom_html_table_element_get_width (dialog->priv->table_element);
	if (!width || !*width || g_ascii_strncasecmp (width, "auto", 4) == 0) {
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check), FALSE);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 100);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->width_units), "units-percent");
	} else {
		gint width_int = atoi (width);

		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check), TRUE);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), width_int);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->width_units),
			((strstr (width, "%") == NULL) ?
				"units-px" : "units-percent"));
	}
	g_free (width);
}

static void
html_editor_table_dialog_set_alignment (EHTMLEditorTableDialog *dialog)
{
	g_return_if_fail (dialog->priv->table_element);

	webkit_dom_html_table_element_set_align (
		dialog->priv->table_element,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo)));
}

static void
html_editor_table_dialog_get_alignment (EHTMLEditorTableDialog *dialog)
{
	gchar *alignment;

	g_return_if_fail (dialog->priv->table_element);

	alignment = webkit_dom_html_table_element_get_align (
			dialog->priv->table_element);

	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo), alignment);

	g_free (alignment);
}

static void
html_editor_table_dialog_set_padding (EHTMLEditorTableDialog *dialog)
{
	gchar *padding;

	g_return_if_fail (dialog->priv->table_element);

	padding = g_strdup_printf (
		"%d",
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->padding_edit)));

	webkit_dom_html_table_element_set_cell_padding (
		dialog->priv->table_element, padding);

	g_free (padding);
}

static void
html_editor_table_dialog_get_padding (EHTMLEditorTableDialog *dialog)
{
	gchar *padding;
	gint padding_int;

	g_return_if_fail (dialog->priv->table_element);

	padding = webkit_dom_html_table_element_get_cell_padding (
			dialog->priv->table_element);
	if (!padding || !*padding) {
		padding_int = 0;
	} else {
		padding_int = atoi (padding);
	}

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->padding_edit), padding_int);

	g_free (padding);
}

static void
html_editor_table_dialog_set_spacing (EHTMLEditorTableDialog *dialog)
{
	gchar *spacing;

	g_return_if_fail (dialog->priv->table_element);

	spacing = g_strdup_printf (
		"%d",
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->spacing_edit)));

	webkit_dom_html_table_element_set_cell_spacing (
		dialog->priv->table_element, spacing);

	g_free (spacing);
}

static void
html_editor_table_dialog_get_spacing (EHTMLEditorTableDialog *dialog)
{
	gchar *spacing;
	gint spacing_int;

	g_return_if_fail (dialog->priv->table_element);

	spacing = webkit_dom_html_table_element_get_cell_spacing (
			dialog->priv->table_element);
	if (!spacing || !*spacing) {
		spacing_int = 0;
	} else {
		spacing_int = atoi (spacing);
	}

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->spacing_edit), spacing_int);

	g_free (spacing);
}

static void
html_editor_table_dialog_set_border (EHTMLEditorTableDialog *dialog)
{
	gchar *border;

	g_return_if_fail (dialog->priv->table_element);

	border = g_strdup_printf (
		"%d",
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->border_edit)));

	webkit_dom_html_table_element_set_border (
		dialog->priv->table_element, border);

	g_free (border);
}

static void
html_editor_table_dialog_get_border (EHTMLEditorTableDialog *dialog)
{
	gchar *border;
	gint border_int;

	g_return_if_fail (dialog->priv->table_element);

	border = webkit_dom_html_table_element_get_border (
			dialog->priv->table_element);
	if (!border || !*border) {
		border_int = 0;
	} else {
		border_int = atoi (border);
	}

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->border_edit), border_int);

	g_free (border);
}

static void
html_editor_table_dialog_set_background_color (EHTMLEditorTableDialog *dialog)
{
	gchar *color;
	GdkRGBA rgba;

	g_return_if_fail (dialog->priv->table_element);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_button), &rgba);

	if (rgba.alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	else
		color = g_strdup ("");

	webkit_dom_html_table_element_set_bg_color (
		dialog->priv->table_element, color);

	g_free (color);
}

static void
html_editor_table_dialog_get_background_color (EHTMLEditorTableDialog *dialog)
{
	gchar *color;
	GdkRGBA rgba;

	g_return_if_fail (dialog->priv->table_element);

	color = webkit_dom_html_table_element_get_bg_color (
			dialog->priv->table_element);

	if (color && *color) {
		gdk_rgba_parse (&rgba, color);

		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_button), &rgba);
	} else {
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_button), &transparent);
	}

	g_free (color);
}

static void
html_editor_table_dialog_set_background_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *uri;

	g_return_if_fail (dialog->priv->table_element);

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->background_image_button));

	if (uri && *uri)
		e_html_editor_selection_replace_image_src (
			e_html_editor_view_get_selection (view),
			WEBKIT_DOM_ELEMENT (dialog->priv->table_element),
			uri);
	else
		remove_image_attributes_from_element (
			WEBKIT_DOM_ELEMENT (dialog->priv->table_element));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_table_dialog_get_background_image (EHTMLEditorTableDialog *dialog)
{
	g_return_if_fail (dialog->priv->table_element);


	if (!webkit_dom_element_has_attribute (
		WEBKIT_DOM_ELEMENT (dialog->priv->table_element), "background")) {

		gtk_file_chooser_unselect_all (
			GTK_FILE_CHOOSER (dialog->priv->background_image_button));
		return;
	} else {
		gchar *value;

		value = webkit_dom_element_get_attribute (
			WEBKIT_DOM_ELEMENT (dialog->priv->table_element), "data-uri");

		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->background_image_button),
			value);

		g_free (value);
	}
}

static void
html_editor_table_dialog_get_values (EHTMLEditorTableDialog *dialog)
{
	html_editor_table_dialog_get_row_count (dialog);
	html_editor_table_dialog_get_column_count (dialog);
	html_editor_table_dialog_get_width (dialog);
	html_editor_table_dialog_get_alignment (dialog);
	html_editor_table_dialog_get_spacing (dialog);
	html_editor_table_dialog_get_padding (dialog);
	html_editor_table_dialog_get_border (dialog);
	html_editor_table_dialog_get_background_color (dialog);
	html_editor_table_dialog_get_background_image (dialog);
}

static void
html_editor_table_dialog_reset_values (EHTMLEditorTableDialog *dialog)
{
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->rows_edit), 3);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->columns_edit), 3);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo), "left");

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->width_check), TRUE);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit), 100);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->width_units), "units-percent");

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->spacing_edit), 2);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->padding_edit), 1);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->border_edit), 1);

	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_button), &transparent);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_button));

	html_editor_table_dialog_set_row_count (dialog);
	html_editor_table_dialog_set_column_count (dialog);
	html_editor_table_dialog_set_width (dialog);
	html_editor_table_dialog_set_alignment (dialog);
	html_editor_table_dialog_set_spacing (dialog);
	html_editor_table_dialog_set_padding (dialog);
	html_editor_table_dialog_set_border (dialog);
	html_editor_table_dialog_set_background_color (dialog);
	html_editor_table_dialog_set_background_image (dialog);
}

static void
html_editor_table_dialog_show (GtkWidget *widget)
{
	EHTMLEditorTableDialog *dialog;
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);
	if (dom_selection && (webkit_dom_dom_selection_get_range_count (dom_selection) > 0)) {
		WebKitDOMElement *table;
		WebKitDOMRange *range;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		table = e_html_editor_dom_node_find_parent_element (
			webkit_dom_range_get_start_container (range, NULL), "TABLE");
		g_object_unref (range);

		if (!table) {
			dialog->priv->table_element = WEBKIT_DOM_HTML_TABLE_ELEMENT (
				html_editor_table_dialog_create_table (dialog));
			html_editor_table_dialog_reset_values (dialog);
		} else {
			dialog->priv->table_element =
				WEBKIT_DOM_HTML_TABLE_ELEMENT (table);
			html_editor_table_dialog_get_values (dialog);
		}

		if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
			EHTMLEditorViewHistoryEvent *ev;

			ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
			ev->type = HISTORY_TABLE_DIALOG;

			e_html_editor_selection_get_selection_coordinates (
				e_html_editor_view_get_selection (view),
				&ev->before.start.x, &ev->before.start.y,
				&ev->before.end.x, &ev->before.end.y);
			if (table)
				ev->data.dom.from = webkit_dom_node_clone_node (
					WEBKIT_DOM_NODE (table), TRUE);
			dialog->priv->history_event = ev;
		}
	}

	g_object_unref (dom_selection);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_table_dialog_parent_class)->show (widget);
}

static void
html_editor_table_dialog_remove_image (EHTMLEditorTableDialog *dialog)
{
	remove_image_attributes_from_element (
		WEBKIT_DOM_ELEMENT (dialog->priv->table_element));

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_button));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_table_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorTableDialogPrivate *priv;
	EHTMLEditorViewHistoryEvent *ev;

	priv = E_HTML_EDITOR_TABLE_DIALOG_GET_PRIVATE (widget);
	ev = priv->history_event;

	if (ev) {
		EHTMLEditorTableDialog *dialog;
		EHTMLEditor *editor;
		EHTMLEditorSelection *selection;
		EHTMLEditorView *view;

		dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
		editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
		view = e_html_editor_get_view (editor);
		selection = e_html_editor_view_get_selection (view);

		ev->data.dom.to = webkit_dom_node_clone_node (
			WEBKIT_DOM_NODE (priv->table_element), TRUE);

		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
		e_html_editor_view_insert_new_history_event (view, ev);
	}

	g_object_unref (priv->table_element);
	priv->table_element = NULL;

	GTK_WIDGET_CLASS (e_html_editor_table_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_table_dialog_class_init (EHTMLEditorTableDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorTableDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_table_dialog_show;
	widget_class->hide = html_editor_table_dialog_hide;
}

static void
e_html_editor_table_dialog_init (EHTMLEditorTableDialog *dialog)
{
	GtkBox *box;
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;
	GtkFileFilter *file_filter;

	dialog->priv = E_HTML_EDITOR_TABLE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == General == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>General</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Rows */
	widget = gtk_image_new_from_icon_name ("stock_select-row", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_spin_button_new_with_range (1, G_MAXINT, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_row_count), dialog);
	dialog->priv->rows_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Rows:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->rows_edit);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	/* Columns */
	widget = gtk_image_new_from_icon_name ("stock_select-column", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);

	widget = gtk_spin_button_new_with_range (1, G_MAXINT, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 5, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_column_count), dialog);
	dialog->priv->columns_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("C_olumns:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->columns_edit);
	gtk_grid_attach (grid, widget, 4, 0, 1, 1);

	/* == Layout == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Layout</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Width */
	widget = gtk_check_button_new_with_mnemonic (_("_Width:"));
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_table_dialog_set_width), dialog);
	dialog->priv->width_check = widget;

	widget = gtk_spin_button_new_with_range (1, 100, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_width), dialog);
	dialog->priv->width_edit = widget;

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_table_dialog_set_width), dialog);
	dialog->priv->width_units = widget;

	/* Spacing */
	widget = gtk_spin_button_new_with_range (0, G_MAXINT, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 5, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_spacing), dialog);
	dialog->priv->spacing_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Spacing:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->spacing_edit);
	gtk_grid_attach (grid, widget, 4, 0, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 0, 1, 1);

	/* Padding */
	widget = gtk_spin_button_new_with_range (0, G_MAXINT, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 5, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_padding), dialog);
	dialog->priv->padding_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Padding:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->padding_edit);
	gtk_grid_attach (grid, widget, 4, 1, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 1, 1, 1);

	/* Border */
	widget = gtk_spin_button_new_with_range (0, G_MAXINT, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_grid_attach (grid, widget, 5, 2, 1, 1);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_table_dialog_set_border), dialog);
	dialog->priv->border_edit = widget;

	widget = gtk_label_new_with_mnemonic (_("_Border:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->border_edit);
	gtk_grid_attach (grid, widget, 4, 2, 1, 1);

	widget = gtk_label_new ("px");
	gtk_grid_attach (grid, widget, 6, 2, 1, 1);

	/* Alignment */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "left", _("Left"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "center", _("Center"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "right", _("Right"));
	gtk_grid_attach (grid, widget, 1, 1, 2, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_table_dialog_set_alignment), dialog);
	dialog->priv->alignment_combo = widget;

	widget = gtk_label_new_with_mnemonic (_("_Alignment:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->alignment_combo);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	/* == Background == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Background</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 4, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 5, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Color */
	widget = e_color_combo_new ();
	e_color_combo_set_default_color (E_COLOR_COMBO (widget), &transparent);
	e_color_combo_set_default_label (E_COLOR_COMBO (widget), _("Transparent"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_table_dialog_set_background_color), dialog);
	dialog->priv->background_color_button = widget;

	widget = gtk_label_new_with_mnemonic (_("_Color:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_color_button);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Image */
	widget = e_image_chooser_dialog_new (
			_("Choose Background Image"),
			GTK_WINDOW (dialog));
	dialog->priv->image_chooser_dialog = widget;

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("Images"));
	gtk_file_filter_add_mime_type (file_filter, "image/*");

	widget = gtk_file_chooser_button_new_with_dialog (
			dialog->priv->image_chooser_dialog);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), file_filter);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "file-set",
		G_CALLBACK (html_editor_table_dialog_set_background_image), dialog);
	dialog->priv->background_image_button = widget;

	widget =gtk_label_new_with_mnemonic (_("Image:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_image_button);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));
	widget = e_dialog_button_new_with_icon (NULL, _("_Remove image"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_table_dialog_remove_image), dialog);
	dialog->priv->remove_image_button = widget;

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	gtk_box_reorder_child (box, widget, 0);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_table_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_TABLE_DIALOG,
			"editor", editor,
			"title", _("Table Properties"),
			NULL));
}


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

#include "evolution-config.h"

#include "e-html-editor-cell-dialog.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

#include "e-color-combo.h"
#include "e-dialog-widgets.h"
#include "e-image-chooser-dialog.h"
#include "e-misc-utils.h"

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

	guint scope;
};

enum {
	SCOPE_CELL,
	SCOPE_ROW,
	SCOPE_COLUMN,
	SCOPE_TABLE
} DialogScope;

static GdkRGBA transparent = { 0, 0, 0, 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorCellDialog, e_html_editor_cell_dialog, E_TYPE_HTML_EDITOR_DIALOG)

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
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_v_align (
		cnt_editor,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->valign_combo)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_halign (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_align (
		cnt_editor,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->halign_combo)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_wrap_text (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_wrap (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->wrap_text_check)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_header_style (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_header_style (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->header_style_check)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_width (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->width_check))) {

		e_content_editor_cell_set_width (
			cnt_editor,
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->width_edit)),
			(gtk_combo_box_get_active (
				GTK_COMBO_BOX (dialog->priv->width_units)) == 0) ?
					E_CONTENT_EDITOR_UNIT_PIXEL :
					E_CONTENT_EDITOR_UNIT_PERCENTAGE,
			dialog->priv->scope);
	} else
		e_content_editor_cell_set_width (
			cnt_editor, 0, E_CONTENT_EDITOR_UNIT_AUTO, dialog->priv->scope);
}

static void
html_editor_cell_dialog_width_units_changed (GtkWidget *widget,
                                             EHTMLEditorCellDialog *dialog)
{
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->width_units)) == 0) {
		gtk_spin_button_set_range (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 0, G_MAXUINT);
	} else
		gtk_spin_button_set_range (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 0, 100);

	html_editor_cell_dialog_set_width (dialog);
}

static void
html_editor_cell_dialog_set_column_span (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_col_span (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->col_span_edit)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_row_span (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_row_span (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->row_span_edit)),
		dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_background_color (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
	e_content_editor_cell_set_background_color (cnt_editor, &rgba, dialog->priv->scope);
}

static void
html_editor_cell_dialog_set_background_image (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	e_content_editor_cell_set_background_image_uri (cnt_editor, uri);

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_cell_dialog_remove_image (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_cell_set_background_image_uri (cnt_editor, NULL);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_cell_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EContentEditorUnit unit;
	EHTMLEditorCellDialog *dialog;
	GdkRGBA rgba;
	gchar *alignment, *uri;
	gint width;

	dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_CELL);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_cell_button), TRUE);

	alignment = e_content_editor_cell_get_align (cnt_editor);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->halign_combo),
		(alignment && *alignment) ? alignment : "left");
	g_free (alignment);

	alignment = e_content_editor_cell_get_v_align (cnt_editor);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->valign_combo),
		(alignment && *alignment) ? alignment : "middle");
	g_free (alignment);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->wrap_text_check),
		e_content_editor_cell_get_wrap (cnt_editor));

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->header_style_check),
		e_content_editor_cell_is_header (cnt_editor));

	width = e_content_editor_cell_get_width (cnt_editor, &unit);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit), width);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->width_check),
		unit != E_CONTENT_EDITOR_UNIT_AUTO);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->width_units),
		(unit == E_CONTENT_EDITOR_UNIT_PIXEL) ? "units-px" : "units-percent");

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->row_span_edit),
		e_content_editor_cell_get_row_span (cnt_editor));

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->col_span_edit),
		e_content_editor_cell_get_col_span (cnt_editor));

	uri = e_content_editor_cell_get_background_image_uri (cnt_editor);
	if (uri && *uri)
		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser), uri);
	else
		gtk_file_chooser_unselect_all (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));
	g_free (uri);

	e_content_editor_cell_get_background_color (cnt_editor, &rgba);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);

	GTK_WIDGET_CLASS (e_html_editor_cell_dialog_parent_class)->show (widget);
}

static void
html_editor_cell_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorCellDialog *dialog;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_CELL);

	GTK_WIDGET_CLASS (e_html_editor_cell_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_cell_dialog_class_init (EHTMLEditorCellDialogClass *class)
{
	GtkWidgetClass *widget_class;

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

	dialog->priv = e_html_editor_cell_dialog_get_instance_private (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Scope == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Scope</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Scope: cell */
	widget = gtk_radio_button_new_with_mnemonic (NULL, _("C_ell"));
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->scope_cell_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: row */
	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		GTK_RADIO_BUTTON (dialog->priv->scope_cell_button), _("_Row"));
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);
	dialog->priv->scope_row_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: table */
	widget = gtk_radio_button_new_with_mnemonic_from_widget (
		GTK_RADIO_BUTTON (dialog->priv->scope_cell_button), _("_Table"));
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	dialog->priv->scope_table_button = widget;

	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_cell_dialog_set_scope), dialog);

	/* Scope: column */
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
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

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
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 4, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 5, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Width */
	widget = gtk_check_button_new_with_mnemonic (_("_Width"));
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	dialog->priv->width_check = widget;

	widget = gtk_spin_button_new_with_range (1, 100, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
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
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	dialog->priv->width_units = widget;

	g_signal_connect (
		widget, "changed",
		G_CALLBACK (html_editor_cell_dialog_width_units_changed), dialog);
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
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 6, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 7, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

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

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("Images"));
	gtk_file_filter_add_mime_type (file_filter, "image/*");

	/* Image */
	if (e_util_is_running_flatpak ()) {
		widget = gtk_file_chooser_button_new (_("Choose Background Image"), GTK_FILE_CHOOSER_ACTION_OPEN);
	} else {
		widget = e_image_chooser_dialog_new (
				_("Choose Background Image"),
				GTK_WINDOW (dialog));

		widget = gtk_file_chooser_button_new_with_dialog (widget);
	}

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

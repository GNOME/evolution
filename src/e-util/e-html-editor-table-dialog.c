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

#include "evolution-config.h"

#include "e-html-editor-table-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-color-combo.h"
#include "e-dialog-widgets.h"
#include "e-image-chooser-dialog.h"
#include "e-misc-utils.h"

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

	GtkWidget *background_color_picker;
	GtkWidget *background_image_chooser;

	GtkWidget *remove_image_button;
};

static GdkRGBA transparent = { 0, 0, 0, 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorTableDialog, e_html_editor_table_dialog, E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_table_dialog_set_row_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_row_count (
		cnt_editor,
		gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->rows_edit)));
}

static void
html_editor_table_dialog_get_row_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->rows_edit),
		e_content_editor_table_get_row_count (cnt_editor));
}

static void
html_editor_table_dialog_set_column_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_column_count (
		cnt_editor,
		gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->columns_edit)));
}

static void
html_editor_table_dialog_get_column_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->columns_edit),
		e_content_editor_table_get_column_count (cnt_editor));
}

static void
html_editor_table_dialog_set_width (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	if (gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->width_check))) {

		e_content_editor_table_set_width (
			cnt_editor,
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->width_edit)),
			(gtk_combo_box_get_active (
				GTK_COMBO_BOX (dialog->priv->width_units)) == 0) ?
					E_CONTENT_EDITOR_UNIT_PIXEL :
					E_CONTENT_EDITOR_UNIT_PERCENTAGE);

		gtk_widget_set_sensitive (dialog->priv->width_edit, TRUE);
		gtk_widget_set_sensitive (dialog->priv->width_units, TRUE);
	} else {
		e_content_editor_table_set_width (
			cnt_editor, 0, E_CONTENT_EDITOR_UNIT_AUTO);

		gtk_widget_set_sensitive (dialog->priv->width_edit, FALSE);
		gtk_widget_set_sensitive (dialog->priv->width_units, FALSE);
	}
}

static void
html_editor_table_dialog_width_units_changed (GtkWidget *widget,
                                              EHTMLEditorTableDialog *dialog)
{
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->width_units)) == 0) {
		gtk_spin_button_set_range (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 0, G_MAXUINT);
	} else
		gtk_spin_button_set_range (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 0, 100);

	html_editor_table_dialog_set_width (dialog);
}

static void
html_editor_table_dialog_get_width (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EContentEditorUnit unit;
	gint width;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	width = e_content_editor_table_get_width (cnt_editor, &unit);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->width_check),
		unit != E_CONTENT_EDITOR_UNIT_AUTO);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit),
		unit == E_CONTENT_EDITOR_UNIT_AUTO ? 100 : width);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->width_units),
		unit == E_CONTENT_EDITOR_UNIT_PIXEL ? "units-px" : "units-percent");
}

static void
html_editor_table_dialog_set_alignment (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_align (
		cnt_editor,
		gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo)));
}

static void
html_editor_table_dialog_get_alignment (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = e_content_editor_table_get_align (cnt_editor);
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->priv->alignment_combo),
		value && *value ? value : "left");
	g_free (value);
}

static void
html_editor_table_dialog_set_padding (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_padding (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->padding_edit)));
}

static void
html_editor_table_dialog_get_padding (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->padding_edit),
		e_content_editor_table_get_padding (cnt_editor));
}

static void
html_editor_table_dialog_set_spacing (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_spacing (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->spacing_edit)));
}

static void
html_editor_table_dialog_get_spacing (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->spacing_edit),
		e_content_editor_table_get_spacing (cnt_editor));
}

static void
html_editor_table_dialog_set_border (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_border (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->border_edit)));
}

static void
html_editor_table_dialog_get_border (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->border_edit),
		e_content_editor_table_get_border (cnt_editor));
}

static void
html_editor_table_dialog_set_background_color (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
	e_content_editor_table_set_background_color (cnt_editor, &rgba);
}

static void
html_editor_table_dialog_get_background_color (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_get_background_color (cnt_editor, &rgba);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
}

static void
html_editor_table_dialog_set_background_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	e_content_editor_table_set_background_image_uri (cnt_editor, uri);

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_table_dialog_get_background_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	uri = e_content_editor_table_get_background_image_uri (cnt_editor);
	if (uri && *uri)
		gtk_file_chooser_set_uri (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser), uri);
	else
		gtk_file_chooser_unselect_all (
			GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	g_free (uri);
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
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->rows_edit), 3);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->columns_edit), 3);
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->priv->alignment_combo), "left");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->width_check), TRUE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->width_edit), 100);
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (dialog->priv->width_units), "units-percent");

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->spacing_edit), 2);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->padding_edit), 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->border_edit), 1);

	e_color_combo_set_current_color (E_COLOR_COMBO (dialog->priv->background_color_picker), &transparent);

	gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

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
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_TABLE);

	if (!e_content_editor_table_get_row_count (cnt_editor))
		html_editor_table_dialog_reset_values (dialog);
	else
		html_editor_table_dialog_get_values (dialog);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_table_dialog_parent_class)->show (widget);
}

static void
html_editor_table_dialog_remove_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_table_set_background_image_uri (cnt_editor, NULL);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_table_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorTableDialog *dialog;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_TABLE);

	GTK_WIDGET_CLASS (e_html_editor_table_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_table_dialog_class_init (EHTMLEditorTableDialogClass *class)
{
	GtkWidgetClass *widget_class;

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

	dialog->priv = e_html_editor_table_dialog_get_instance_private (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == General == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>General</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Rows */
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
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

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
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (html_editor_table_dialog_width_units_changed), dialog);
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
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 4, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 5, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Color */
	widget = e_color_combo_new ();
	e_color_combo_set_default_color (E_COLOR_COMBO (widget), &transparent);
	e_color_combo_set_default_label (E_COLOR_COMBO (widget), _("Transparent"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_table_dialog_set_background_color), dialog);
	dialog->priv->background_color_picker = widget;

	widget = gtk_label_new_with_mnemonic (_("_Color:"));
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
		GtkWidget *image_chooser_dialog;

		image_chooser_dialog = e_image_chooser_dialog_new (
				_("Choose Background Image"),
				GTK_WINDOW (dialog));

		widget = gtk_file_chooser_button_new_with_dialog (image_chooser_dialog);
	}

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), file_filter);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "file-set",
		G_CALLBACK (html_editor_table_dialog_set_background_image), dialog);
	dialog->priv->background_image_chooser = widget;

	widget =gtk_label_new_with_mnemonic (_("Image:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_image_chooser);
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

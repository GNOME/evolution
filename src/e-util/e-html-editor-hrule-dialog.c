/*
 * e-html-editor-hrule-dialog.h
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

#include "e-html-editor-hrule-dialog.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

struct _EHTMLEditorHRuleDialogPrivate {
	GtkWidget *width_edit;
	GtkWidget *size_edit;
	GtkWidget *unit_combo;

	GtkWidget *alignment_combo;
	GtkWidget *shaded_check;
};

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorHRuleDialog, e_html_editor_hrule_dialog, E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_hrule_dialog_set_alignment (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_combo_box_get_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo));

	e_content_editor_h_rule_set_align (cnt_editor, value);
}

static void
html_editor_hrule_dialog_get_alignment (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = e_content_editor_h_rule_get_align (cnt_editor);
	if (value && *value)
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo), value);
	g_free (value);
}

static void
html_editor_hrule_dialog_set_size (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = gtk_spin_button_get_value_as_int  GTK_SPIN_BUTTON (dialog->priv->size_edit);
	e_content_editor_h_rule_set_size (cnt_editor, value);
}

static void
html_editor_hrule_dialog_get_size (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = e_content_editor_h_rule_get_size (cnt_editor);

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->size_edit), (gdouble) value);
}

static void
html_editor_hrule_dialog_set_width (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_h_rule_set_width (
		cnt_editor,
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->width_edit)),
		(gtk_combo_box_get_active (
			GTK_COMBO_BOX (dialog->priv->unit_combo)) == 0) ?
				E_CONTENT_EDITOR_UNIT_PIXEL :
				E_CONTENT_EDITOR_UNIT_PERCENTAGE);
}

static void
html_editor_hrule_dialog_get_width (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	EContentEditorUnit unit;
	gint value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = e_content_editor_h_rule_get_width (cnt_editor, &unit);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit),
		value == 0 && unit == E_CONTENT_EDITOR_UNIT_PERCENTAGE ? 100 : value);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->unit_combo),
		unit == E_CONTENT_EDITOR_UNIT_PIXEL ? "units-px" : "units-percent");
}

static void
html_editor_hrule_dialog_set_shading (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = !gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (dialog->priv->shaded_check));

	e_content_editor_h_rule_set_no_shade (cnt_editor, value);
}

static void
html_editor_hrule_dialog_get_shading (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gboolean value = FALSE;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	value = e_content_editor_h_rule_get_no_shade (cnt_editor);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->shaded_check), !value);
}

static void
html_editor_hrule_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorHRuleDialog *dialog;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_HRULE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_close (cnt_editor, E_CONTENT_EDITOR_DIALOG_HRULE);

	GTK_WIDGET_CLASS (e_html_editor_hrule_dialog_parent_class)->hide (widget);
}

static void
html_editor_hrule_dialog_show (GtkWidget *widget)
{
	EHTMLEditorHRuleDialog *dialog;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	dialog = E_HTML_EDITOR_HRULE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_on_dialog_open (cnt_editor, E_CONTENT_EDITOR_DIALOG_HRULE);

	html_editor_hrule_dialog_get_alignment (dialog);
	html_editor_hrule_dialog_get_size (dialog);
	html_editor_hrule_dialog_get_width (dialog);
	html_editor_hrule_dialog_get_shading (dialog);

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_hrule_dialog_parent_class)->show (widget);
}

static void
e_html_editor_hrule_dialog_class_init (EHTMLEditorHRuleDialogClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_hrule_dialog_show;
	widget_class->hide = html_editor_hrule_dialog_hide;
}

static void
e_html_editor_hrule_dialog_init (EHTMLEditorHRuleDialog *dialog)
{
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;

	dialog->priv = e_html_editor_hrule_dialog_get_instance_private (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Size == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Size</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);

	/* Width */
	widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 100);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_hrule_dialog_set_width), dialog);
	dialog->priv->width_edit = widget;
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

	widget = gtk_label_new_with_mnemonic (_("_Width:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "units-percent");
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_hrule_dialog_set_width), dialog);
	dialog->priv->unit_combo = widget;
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);

	/* Size */
	widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 2);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (html_editor_hrule_dialog_set_size), dialog);
	dialog->priv->size_edit = widget;
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);

	widget = gtk_label_new_with_mnemonic (_("_Size:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	/* == Style == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Style</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);

	/* Alignment */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget), "left", _("Left"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget), "center", _("Center"));
	gtk_combo_box_text_append (
		GTK_COMBO_BOX_TEXT (widget), "right", _("Right"));
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "left");
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_hrule_dialog_set_alignment), dialog);
	dialog->priv->alignment_combo = widget;
	gtk_grid_attach (grid, widget, 1, 0, 2, 1);

	widget = gtk_label_new_with_mnemonic (_("_Alignment:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), widget);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Shaded */
	widget = gtk_check_button_new_with_mnemonic (_("S_haded"));
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_hrule_dialog_set_shading), dialog);
	dialog->priv->shaded_check = widget;
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_hrule_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_HRULE_DIALOG,
			"editor", editor,
			"title", _("Rule properties"),
			NULL));
}

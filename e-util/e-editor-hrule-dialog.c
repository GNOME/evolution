/*
 * e-editor-hrule-dialog.h
 *
<<<<<<< HEAD
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
=======
>>>>>>> Make horizontal rule dialog work
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

#include "e-editor-hrule-dialog.h"
#include "e-editor-utils.h"
<<<<<<< HEAD
#include "e-editor-widget.h"
=======
>>>>>>> Make horizontal rule dialog work

#include <glib/gi18n-lib.h>
#include <webkit/webkitdom.h>
#include <stdlib.h>

<<<<<<< HEAD
#define E_EDITOR_HRULE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_HRULE_DIALOG, EEditorHRuleDialogPrivate))
=======
G_DEFINE_TYPE (
	EEditorHRuleDialog,
	e_editor_hrule_dialog,
	E_TYPE_EDITOR_DIALOG);
>>>>>>> Make horizontal rule dialog work

struct _EEditorHRuleDialogPrivate {
	GtkWidget *width_edit;
	GtkWidget *size_edit;
	GtkWidget *unit_combo;

	GtkWidget *alignment_combo;
	GtkWidget *shaded_check;

<<<<<<< HEAD
	WebKitDOMHTMLHRElement *hr_element;
};

G_DEFINE_TYPE (
	EEditorHRuleDialog,
	e_editor_hrule_dialog,
	E_TYPE_EDITOR_DIALOG);

=======
	GtkWidget *close_button;

	WebKitDOMHTMLHRElement *hr_element;
};

>>>>>>> Make horizontal rule dialog work
static void
editor_hrule_dialog_set_alignment (EEditorHRuleDialog *dialog)
{
	const gchar *alignment;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

<<<<<<< HEAD
	alignment = gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo));
=======
	alignment = gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (dialog->priv->alignment_combo));
>>>>>>> Make horizontal rule dialog work

	webkit_dom_htmlhr_element_set_align (dialog->priv->hr_element, alignment);
}

static void
editor_hrule_dialog_get_alignment (EEditorHRuleDialog *dialog)
{
	gchar *alignment;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	alignment = webkit_dom_htmlhr_element_get_align (dialog->priv->hr_element);

	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo), alignment);
	g_free (alignment);
}

static void
editor_hrule_dialog_set_size (EEditorHRuleDialog *dialog)
{
	gchar *size;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	size = g_strdup_printf (
		"%d",
		(gint) gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->size_edit)));

	webkit_dom_htmlhr_element_set_size (dialog->priv->hr_element, size);

	g_free (size);
}

static void
editor_hrule_dialog_get_size (EEditorHRuleDialog *dialog)
{
	gchar *size;
	gint size_int = 0;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	size = webkit_dom_htmlhr_element_get_size (dialog->priv->hr_element);
	if (size && *size) {
		size_int = atoi (size);
	}

	if (size_int == 0) {
		size_int = 2;
	}

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->size_edit), (gdouble) size_int);

	g_free (size);
}

static void
editor_hrule_dialog_set_width (EEditorHRuleDialog *dialog)
{
<<<<<<< HEAD
	gchar *width, *units;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	units = gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (dialog->priv->unit_combo));
=======
	gchar *width;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

>>>>>>> Make horizontal rule dialog work
	width = g_strdup_printf (
		"%d%s",
		(gint) gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit)),
<<<<<<< HEAD
		units);

	webkit_dom_htmlhr_element_set_width (dialog->priv->hr_element, width);

	g_free (units);
=======
		gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (dialog->priv->unit_combo)));

	webkit_dom_htmlhr_element_set_width (dialog->priv->hr_element, width);

>>>>>>> Make horizontal rule dialog work
	g_free (width);
}

static void
editor_hrule_dialog_get_width (EEditorHRuleDialog *dialog)
{
	gchar *width;
	const gchar *units;
	gint width_int = 0;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	width = webkit_dom_htmlhr_element_get_width (dialog->priv->hr_element);
	if (width && *width) {
		width_int = atoi (width);

		if (strstr (width, "%") != NULL) {
			units = "units-percent";
		} else {
			units = "units-px";
		}
	}

	if (width_int == 0) {
		width_int = 100;
		units = "units-percent";
	}

	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->priv->width_edit), (gdouble) width_int);
	gtk_combo_box_set_active_id (
		GTK_COMBO_BOX (dialog->priv->unit_combo), units);

	g_free (width);
}

static void
editor_hrule_dialog_set_shading (EEditorHRuleDialog *dialog)
{
	gboolean no_shade;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	no_shade = !gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->shaded_check));

	webkit_dom_htmlhr_element_set_no_shade (dialog->priv->hr_element, no_shade);
}

static void
editor_hrule_dialog_get_shading (EEditorHRuleDialog *dialog)
{
	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->shaded_check),
		!webkit_dom_htmlhr_element_get_no_shade (dialog->priv->hr_element));
}

static void
<<<<<<< HEAD
editor_hrule_dialog_hide (GtkWidget *widget)
{
	EEditorHRuleDialogPrivate *priv;

	priv = E_EDITOR_HRULE_DIALOG_GET_PRIVATE (widget);

	priv->hr_element = NULL;

	GTK_WIDGET_CLASS (e_editor_hrule_dialog_parent_class)->hide (widget);
=======
editor_hrule_dialog_close (EEditorHRuleDialog *dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));

	dialog->priv->hr_element = NULL;
>>>>>>> Make horizontal rule dialog work
}

static void
editor_hrule_dialog_show (GtkWidget *widget)
{
	EEditorHRuleDialog *dialog;
	EEditor *editor;
	EEditorWidget *editor_widget;

	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;
	WebKitDOMElement *rule;

	dialog = E_EDITOR_HRULE_DIALOG (widget);
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	editor_widget = e_editor_get_editor_widget (editor);

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (editor_widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		GTK_WIDGET_CLASS (e_editor_hrule_dialog_parent_class)->show (widget);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);

<<<<<<< HEAD
	rule = e_editor_dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "HR");
	if (!rule) {
		rule = e_editor_dom_node_find_child_element (
			webkit_dom_range_get_start_container (range, NULL), "HR");
	}

	if (!rule) {
		e_editor_widget_exec_command (
			editor_widget, E_EDITOR_WIDGET_COMMAND_INSERT_HORIZONTAL_RULE, NULL);

		rule = e_editor_dom_node_find_child_element (
			webkit_dom_range_get_start_container (range, NULL), "HR");
=======
	rule = e_editor_dom_node_get_parent_element (
		webkit_dom_range_get_start_container (range, NULL),
		WEBKIT_TYPE_DOM_HTMLHR_ELEMENT);
	if (!rule) {
		rule = e_editor_dom_node_find_child_element (
			webkit_dom_range_get_start_container (range, NULL),
			WEBKIT_TYPE_DOM_HTMLHR_ELEMENT);
	}

	if (!rule) {
		webkit_dom_document_exec_command (
			document, "insertHorizontalRule", FALSE, "");

		rule = e_editor_dom_node_find_child_element (
			webkit_dom_range_get_start_container (range, NULL),
			WEBKIT_TYPE_DOM_HTMLHR_ELEMENT);
>>>>>>> Make horizontal rule dialog work

		dialog->priv->hr_element = WEBKIT_DOM_HTMLHR_ELEMENT (rule);

		/* For new rule reset the values to default */
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit), 100.0);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->unit_combo), "units-percent");
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->size_edit), 2.0);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo), "left");
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->shaded_check), FALSE);
		editor_hrule_dialog_set_alignment (dialog);
		editor_hrule_dialog_set_size (dialog);
		editor_hrule_dialog_set_alignment (dialog);
		editor_hrule_dialog_set_shading (dialog);

	} else {
		dialog->priv->hr_element = WEBKIT_DOM_HTMLHR_ELEMENT (rule);

		editor_hrule_dialog_get_alignment (dialog);
		editor_hrule_dialog_get_size (dialog);
		editor_hrule_dialog_get_width (dialog);
		editor_hrule_dialog_get_shading (dialog);
	}

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_editor_hrule_dialog_parent_class)->show (widget);
}

static void
<<<<<<< HEAD
e_editor_hrule_dialog_class_init (EEditorHRuleDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EEditorHRuleDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = editor_hrule_dialog_show;
	widget_class->hide = editor_hrule_dialog_hide;
=======
e_editor_hrule_dialog_class_init (EEditorHRuleDialogClass *klass)
{
	GtkWidgetClass *widget_class;

	g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorHRuleDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_hrule_dialog_show;
>>>>>>> Make horizontal rule dialog work
}

static void
e_editor_hrule_dialog_init (EEditorHRuleDialog *dialog)
{
<<<<<<< HEAD
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;

	dialog->priv = E_EDITOR_HRULE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_editor_dialog_get_container (E_EDITOR_DIALOG (dialog));
=======
	GtkBox *main_layout;
	GtkGrid *grid;
	GtkWidget *widget;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_HRULE_DIALOG, EEditorHRuleDialogPrivate);

	main_layout = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

>>>>>>> Make horizontal rule dialog work

	/* == Size == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Size</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
<<<<<<< HEAD
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);
=======
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 0);
>>>>>>> Make horizontal rule dialog work

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);
<<<<<<< HEAD
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
=======
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 5);
>>>>>>> Make horizontal rule dialog work

	/* Width */
	widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 100);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (editor_hrule_dialog_set_width), dialog);
	dialog->priv->width_edit = widget;
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);

<<<<<<< HEAD
	widget = gtk_label_new_with_mnemonic (_("_Width:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
=======
	widget = gtk_label_new_with_mnemonic (_("Width:"));
>>>>>>> Make horizontal rule dialog work
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-px", "px");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "units-percent", "%");
	gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "units-percent");
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (editor_hrule_dialog_set_width), dialog);
	dialog->priv->unit_combo = widget;
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);

	/* Size */
	widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 2);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (editor_hrule_dialog_set_size), dialog);
	dialog->priv->size_edit = widget;
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);

<<<<<<< HEAD
	widget = gtk_label_new_with_mnemonic (_("_Size:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

=======
	widget = gtk_label_new_with_mnemonic (_("Size:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);


>>>>>>> Make horizontal rule dialog work
	/* == Style == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Style</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
<<<<<<< HEAD
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);
=======
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 0);
>>>>>>> Make horizontal rule dialog work

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);
<<<<<<< HEAD
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
=======
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 5);
>>>>>>> Make horizontal rule dialog work

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
		G_CALLBACK (editor_hrule_dialog_set_alignment), dialog);
	dialog->priv->alignment_combo = widget;
	gtk_grid_attach (grid, widget, 1, 0, 2, 1);

<<<<<<< HEAD
	widget = gtk_label_new_with_mnemonic (_("_Alignment:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
=======
	widget = gtk_label_new_with_mnemonic (_("Alignment:"));
>>>>>>> Make horizontal rule dialog work
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), widget);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Shaded */
<<<<<<< HEAD
	widget = gtk_check_button_new_with_mnemonic (_("S_haded"));
=======
	widget = gtk_check_button_new_with_mnemonic (_("Shaded"));
>>>>>>> Make horizontal rule dialog work
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (editor_hrule_dialog_set_shading), dialog);
	dialog->priv->shaded_check = widget;
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);

<<<<<<< HEAD
=======

	/* == Button box == */
	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (editor_hrule_dialog_close), dialog);
	dialog->priv->close_button = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (widget), dialog->priv->close_button, FALSE, FALSE, 5);

>>>>>>> Make horizontal rule dialog work
	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_editor_hrule_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_HRULE_DIALOG,
			"editor", editor,
			"title", _("Rule properties"),
			NULL));
}

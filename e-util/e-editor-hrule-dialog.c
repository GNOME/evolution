/*
 * e-editor-hrule-dialog.h
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

#include "e-editor-hrule-dialog.h"
#include "e-editor-utils.h"
#include "e-html-editor-view.h"

#include <glib/gi18n-lib.h>
#include <webkit/webkitdom.h>
#include <stdlib.h>

#define E_EDITOR_HRULE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_HRULE_DIALOG, EEditorHRuleDialogPrivate))

struct _EEditorHRuleDialogPrivate {
	GtkWidget *width_edit;
	GtkWidget *size_edit;
	GtkWidget *unit_combo;

	GtkWidget *alignment_combo;
	GtkWidget *shaded_check;

	WebKitDOMHTMLHRElement *hr_element;
};

G_DEFINE_TYPE (
	EEditorHRuleDialog,
	e_editor_hrule_dialog,
	E_TYPE_EDITOR_DIALOG);

static void
editor_hrule_dialog_set_alignment (EEditorHRuleDialog *dialog)
{
	const gchar *alignment;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	alignment = gtk_combo_box_get_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo));

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
	gchar *width, *units;

	g_return_if_fail (WEBKIT_DOM_IS_HTMLHR_ELEMENT (dialog->priv->hr_element));

	units = gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (dialog->priv->unit_combo));
	width = g_strdup_printf (
		"%d%s",
		(gint) gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit)),
		units);

	webkit_dom_htmlhr_element_set_width (dialog->priv->hr_element, width);

	g_free (units);
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
editor_hrule_dialog_hide (GtkWidget *widget)
{
	EEditorHRuleDialogPrivate *priv;

	priv = E_EDITOR_HRULE_DIALOG_GET_PRIVATE (widget);

	priv->hr_element = NULL;

	GTK_WIDGET_CLASS (e_editor_hrule_dialog_parent_class)->hide (widget);
}

static void
editor_hrule_dialog_show (GtkWidget *widget)
{
	EEditorHRuleDialog *dialog;
	EEditor *editor;
	EEditorSelection *editor_selection;
	EHTMLEditorView *view;

	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMElement *rule;

	dialog = E_EDITOR_HRULE_DIALOG (widget);
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	view = e_editor_get_html_editor_view (editor);
	editor_selection = e_html_editor_view_get_selection (view);

	document = webkit_web_view_get_dom_document (
			WEBKIT_WEB_VIEW (view));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		GTK_WIDGET_CLASS (e_editor_hrule_dialog_parent_class)->show (widget);
		return;
	}

	rule = e_html_editor_view_get_element_under_mouse_click (view);
	if (!rule) {
		WebKitDOMElement *caret, *parent, *element;

		caret = e_editor_selection_save_caret_position (editor_selection);

		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (caret));
		element = caret;

		while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			element = parent;
			parent = webkit_dom_node_get_parent_element (
				WEBKIT_DOM_NODE (parent));
		}

		rule = webkit_dom_document_create_element (document, "HR", NULL);

		/* Insert horizontal rule into body below the caret */
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (parent),
			WEBKIT_DOM_NODE (rule),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
			NULL);

		e_editor_selection_clear_caret_position_marker (editor_selection);

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

		e_html_editor_view_set_changed (view, TRUE);
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
e_editor_hrule_dialog_class_init (EEditorHRuleDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EEditorHRuleDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = editor_hrule_dialog_show;
	widget_class->hide = editor_hrule_dialog_hide;
}

static void
e_editor_hrule_dialog_init (EEditorHRuleDialog *dialog)
{
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;

	dialog->priv = E_EDITOR_HRULE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_editor_dialog_get_container (E_EDITOR_DIALOG (dialog));

	/* == Size == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Size</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);

	/* Width */
	widget = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 100);
	g_signal_connect_swapped (
		widget, "value-changed",
		G_CALLBACK (editor_hrule_dialog_set_width), dialog);
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

	widget = gtk_label_new_with_mnemonic (_("_Size:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_edit);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	/* == Style == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Style</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_set_row_spacing (grid, 5);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);
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
		G_CALLBACK (editor_hrule_dialog_set_alignment), dialog);
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
		G_CALLBACK (editor_hrule_dialog_set_shading), dialog);
	dialog->priv->shaded_check = widget;
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);

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

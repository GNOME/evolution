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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-hrule-dialog.h"
#include "e-html-editor-view.h"

#include <glib/gi18n-lib.h>
#include <stdlib.h>

#define E_HTML_EDITOR_HRULE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_HRULE_DIALOG, EHTMLEditorHRuleDialogPrivate))

struct _EHTMLEditorHRuleDialogPrivate {
	GtkWidget *width_edit;
	GtkWidget *size_edit;
	GtkWidget *unit_combo;

	GtkWidget *alignment_combo;
	GtkWidget *shaded_check;
};

G_DEFINE_TYPE (
	EHTMLEditorHRuleDialog,
	e_html_editor_hrule_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
html_editor_hrule_dialog_set_alignment (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	value = gtk_combo_box_get_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-hr", "align", value);
}

static void
html_editor_hrule_dialog_get_alignment (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-hr", "align");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo), value);
		g_variant_unref (result);
	}
}

static void
html_editor_hrule_dialog_set_size (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *size;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	size = g_strdup_printf (
		"%d",
		(gint) gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->size_edit)));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-hr", "size", size);

	g_free (size);
}

static void
html_editor_hrule_dialog_get_size (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-hr", "size");

	if (result) {
		const gchar *value;
		gint value_int = 0;

		g_variant_get (result, "(&s)", &value);
		if (value && *value)
			value_int = atoi (value);

		if (value_int == 0)
			value_int = 2;

		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->size_edit),
			(gdouble) value_int);

		g_variant_unref (result);
	}
}

static void
html_editor_hrule_dialog_set_width (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *width, *units;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	units = gtk_combo_box_text_get_active_text (
			GTK_COMBO_BOX_TEXT (dialog->priv->unit_combo));
	width = g_strdup_printf (
		"%d%s",
		(gint) gtk_spin_button_get_value (
			GTK_SPIN_BUTTON (dialog->priv->width_edit)),
		units);

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-hr", "width", width);

	g_free (units);
	g_free (width);
}

static void
html_editor_hrule_dialog_get_width (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-hr", "width");

	if (result) {
		const gchar *value, *units;
		gint value_int = 0;

		g_variant_get (result, "(&s)", &value);
		if (value && *value) {
			value_int = atoi (value);

			if (strstr (value, "%") != NULL)
				units = "units-percent";
			else
				units = "units-px";

			if (value_int == 0) {
				value_int = 100;
				units = "units-percent";
			}

			gtk_spin_button_set_value (
				GTK_SPIN_BUTTON (dialog->priv->width_edit),
				(gdouble) value_int);
			gtk_combo_box_set_active_id (
				GTK_COMBO_BOX (dialog->priv->unit_combo), units);
		}
		g_variant_unref (result);
	}
}

static void
html_editor_hrule_dialog_set_shading (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"HRElementSetNoShade",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-hr",
			!gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (dialog->priv->shaded_check))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_hrule_dialog_get_shading (EHTMLEditorHRuleDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;
	GDBusProxy *web_extension;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"HRElementGetNoShade",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-hr"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean value;

		g_variant_get (result, "(&b)", &value);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->shaded_check), !value);
		g_variant_unref (result);
	}
}

static void
html_editor_hrule_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorHRuleDialog *dialog;
	EHTMLEditorView *view;

	dialog = E_HTML_EDITOR_HRULE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_html_editor_view_call_simple_extension_function (
		view, "EHTMLEditorHRuleDialogSaveHistoryOnExit");

	GTK_WIDGET_CLASS (e_html_editor_hrule_dialog_parent_class)->hide (widget);
}

static void
html_editor_hrule_dialog_show (GtkWidget *widget)
{
	EHTMLEditorHRuleDialog *dialog;
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;
	GDBusProxy *web_extension;

	dialog = E_HTML_EDITOR_HRULE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"EHTMLEditorHRuleDialogFindHRule",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean found = FALSE;

		g_variant_get (result, "(b)", found);
		if (found) {
			html_editor_hrule_dialog_get_alignment (dialog);
			html_editor_hrule_dialog_get_size (dialog);
			html_editor_hrule_dialog_get_width (dialog);
			html_editor_hrule_dialog_get_shading (dialog);
		} else {
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

			html_editor_hrule_dialog_set_alignment (dialog);
			html_editor_hrule_dialog_set_size (dialog);
			html_editor_hrule_dialog_set_alignment (dialog);
			html_editor_hrule_dialog_set_shading (dialog);

			e_html_editor_view_set_changed (view, TRUE);
		}
		g_variant_unref (result);
	}

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_hrule_dialog_parent_class)->show (widget);
}

static void
e_html_editor_hrule_dialog_class_init (EHTMLEditorHRuleDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorHRuleDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_hrule_dialog_show;
	widget_class->hide = html_editor_hrule_dialog_hide;
}

static void
e_html_editor_hrule_dialog_init (EHTMLEditorHRuleDialog *dialog)
{
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;

	dialog->priv = E_HTML_EDITOR_HRULE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

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

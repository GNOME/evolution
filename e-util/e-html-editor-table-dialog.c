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
};

static GdkRGBA transparent = { 0, 0, 0, 0 };

G_DEFINE_TYPE (
	EHTMLEditorTableDialog,
	e_html_editor_table_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
html_editor_table_dialog_set_row_count (EHTMLEditorTableDialog *dialog)
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
		"EHTMLEditorTableDialogSetRowCount",
		g_variant_new (
			"(tu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_spin_button_get_value (
				GTK_SPIN_BUTTON (dialog->priv->rows_edit))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_table_dialog_get_row_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"EHTMLEditorTableDialogGetRowCount",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gulong value;

		g_variant_get (result, "(u)", &value);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->rows_edit), value);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_column_count (EHTMLEditorTableDialog *dialog)
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
		"EHTMLEditorTableDialogSetColumnCount",
		g_variant_new (
			"(tu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_spin_button_get_value (
				GTK_SPIN_BUTTON (dialog->priv->columns_edit))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_table_dialog_get_column_count (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"EHTMLEditorTableDialogGetColumnCount",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gulong value;

		g_variant_get (result, "(u)", &value);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->columns_edit), value);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_width (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *width;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

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

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "width", width);

	g_free (width);
}

static void
html_editor_table_dialog_get_width (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;
	const gchar *width;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "width");

	if (!result)
		return;

	g_variant_get (result, "(&s)", &width);

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

	g_variant_unref (result);
}

static void
html_editor_table_dialog_set_alignment (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	const gchar *value;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	value = gtk_combo_box_get_active_id (
		GTK_COMBO_BOX (dialog->priv->alignment_combo));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "alignment", value);

}

static void
html_editor_table_dialog_get_alignment (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "align");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->alignment_combo), value);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_padding (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *padding;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	padding = g_strdup_printf (
		"%d",
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->padding_edit)));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "cellpadding", padding);

	g_free (padding);
}

static void
html_editor_table_dialog_get_padding (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "cellpadding");

	if (result) {
		const gchar *value;
		gint value_int;

		g_variant_get (result, "(&s)", &value);
		if (value && *value)
			value_int = atoi (value);
		else
			value_int = 0;
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->padding_edit), value_int);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_spacing (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *spacing;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	spacing = g_strdup_printf (
		"%d",
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->spacing_edit)));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "cellspacing", spacing);

	g_free (spacing);
}

static void
html_editor_table_dialog_get_spacing (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "cellspacing");

	if (result) {
		const gchar *value;
		gint value_int;

		g_variant_get (result, "(&s)", &value);
		if (value && *value)
			value_int = atoi (value);
		else
			value_int = 0;
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->spacing_edit), value_int);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_border (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *border;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	border = g_strdup_printf (
		"%d",
		gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->priv->border_edit)));

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "border", border);

	g_free (border);
}

static void
html_editor_table_dialog_get_border (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "border");

	if (result) {
		const gchar *value;
		gint value_int;

		g_variant_get (result, "(&s)", &value);
		if (value && *value)
			value_int = atoi (value);
		else
			value_int = 0;
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->border_edit), value_int);
		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_background_color (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	gchar *color;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_button), &rgba);
	if (rgba.alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	else
		color = g_strdup ("");

	e_html_editor_view_set_element_attribute (
		view, "#-x-evo-current-table", "bgcolor", color);

	g_free (color);
}

static void
html_editor_table_dialog_get_background_color (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-table", "bgcolor");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (value && *value) {
			GdkRGBA rgba;

			if (gdk_rgba_parse (&rgba, value))
				e_color_combo_set_current_color (
					E_COLOR_COMBO (dialog->priv->background_color_button),
					&rgba);
			else
				e_color_combo_set_current_color (
					E_COLOR_COMBO (dialog->priv->background_color_button),
					&transparent);
		} else
			e_color_combo_set_current_color (
				E_COLOR_COMBO (dialog->priv->background_color_button),
				&transparent);

		g_variant_unref (result);
	}
}

static void
html_editor_table_dialog_set_background_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	uri = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->priv->background_image_button));

	if (uri && *uri)
		e_html_editor_view_replace_image_src (view, "#-x-evo-current-table", uri);
	else
		g_dbus_proxy_call (
			web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new (
				"(ts)",
				webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
				"#-x-evo-current-table"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_table_dialog_get_background_image (EHTMLEditorTableDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GVariant *result;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"ElementHasAttribute",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (
				WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-table",
			"background"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean has_background;

		g_variant_get (result, "(b)", &has_background);
		if (has_background) {
			g_variant_unref (result);

			result = e_html_editor_view_get_element_attribute (
				view, "#-x-evo-current-table", "data-uri");

			if (result) {
				const gchar *value;

				g_variant_get (result, "(&s)", &value);

				gtk_file_chooser_set_uri (
					GTK_FILE_CHOOSER (dialog->priv->background_image_button),
					value);

				g_variant_unref (result);
			}
		} else {
			gtk_file_chooser_unselect_all (
				GTK_FILE_CHOOSER (dialog->priv->background_image_button));
			g_variant_unref (result);
		}
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
	GDBusProxy *web_extension;
	GVariant *result;

	dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	result = g_dbus_proxy_call_sync (
		web_extension,
		"EHTMLEditorTableDialogShow",
		g_variant_new (
			"(t)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean created;

		g_variant_get (result, "(b)", &created);
		if (created)
			html_editor_table_dialog_reset_values (dialog);
		else
			html_editor_table_dialog_get_values (dialog);

		g_variant_unref (result);
	}

	/* Chain up to parent implementation */
	GTK_WIDGET_CLASS (e_html_editor_table_dialog_parent_class)->show (widget);
}

static void
html_editor_table_dialog_remove_image (EHTMLEditorTableDialog *dialog)
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
		"RemoveImageAttributesFromElementBySelector",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"#-x-evo-current-table"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_button));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_table_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorTableDialog *dialog;
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	dialog = E_HTML_EDITOR_TABLE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_html_editor_view_remove_element_attribute (
		view, "#-x-evo-current-table", "id");

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

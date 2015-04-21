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
#include "e-image-chooser-dialog.h"
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

	guint scope;
};

enum {
	SCOPE_CELL,
	SCOPE_ROW,
	SCOPE_COLUMN,
	SCOPE_TABLE
} DialogScope;

static GdkRGBA transparent = { 0, 0, 0, 0 };

G_DEFINE_TYPE (
	EHTMLEditorCellDialog,
	e_html_editor_cell_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

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
	EHTMLEditorView *view;
	GDBusProxy *web_extension;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorCellDialogSetElementVAlign",
		g_variant_new (
			"(tsu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_combo_box_get_active_id (
				GTK_COMBO_BOX (dialog->priv->valign_combo)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_halign (EHTMLEditorCellDialog *dialog)
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
		"EHTMLEditorCellDialogSetElementAlign",
		g_variant_new (
			"(tsu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_combo_box_get_active_id (
				GTK_COMBO_BOX (dialog->priv->halign_combo)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_wrap_text (EHTMLEditorCellDialog *dialog)
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
		"EHTMLEditorCellDialogSetElementNoWrap",
		g_variant_new (
			"(tbu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			!gtk_combo_box_get_active (
				GTK_COMBO_BOX (dialog->priv->wrap_text_check)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_header_style (EHTMLEditorCellDialog *dialog)
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
		"EHTMLEditorCellDialogSetElementHeaderStyle",
		g_variant_new (
			"(tbu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (
					dialog->priv->header_style_check)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_width (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	gchar *width;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

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

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorCellDialogSetElementWidth",
		g_variant_new (
			"(tsu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			width,
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (width);
}

static void
html_editor_cell_dialog_set_column_span (EHTMLEditorCellDialog *dialog)
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
		"EHTMLEditorCellDialogSetElementColSpan",
		g_variant_new (
			"(tiu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->col_span_edit)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_row_span (EHTMLEditorCellDialog *dialog)
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
		"EHTMLEditorCellDialogSetElementRowSpan",
		g_variant_new (
			"(tiu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (dialog->priv->row_span_edit)),
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

static void
html_editor_cell_dialog_set_background_color (EHTMLEditorCellDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	gchar *color;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
	if (rgba.alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	else
		color = g_strdup ("");

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorCellDialogSetElementBgColor",
		g_variant_new (
			"(tsu)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			color,
			dialog->priv->scope),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	g_free (color);
}

static void
html_editor_cell_dialog_set_background_image (EHTMLEditorCellDialog *dialog)
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
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	if (uri && *uri)
		g_dbus_proxy_call (
			web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new (
				"(ts)",
				webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
				"#-x-evo-current-cell"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);
	else
		e_html_editor_view_replace_image_src (view, "#-x-evo-current-cell", uri);

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_cell_dialog_remove_image (EHTMLEditorCellDialog *dialog)
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
			"#-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_cell_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorCellDialog *dialog;
	GDBusProxy *web_extension;
	GVariant *result;
	GdkRGBA color;

	dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	g_dbus_proxy_call (
		web_extension,
		"EHTMLEditorCellDialogMarkCurrentCellElement",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-table-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->scope_cell_button), TRUE);

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-cell", "align");

	if (result) {
		const gchar *align;

		g_variant_get (result, "(&s)", &align);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->halign_combo),
			(align && *align) ? align : "left");
		g_variant_unref (result);
	}

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-cell", "valign");

	if (result) {
		const gchar *v_align;

		g_variant_get (result, "(&s)", &v_align);
		gtk_combo_box_set_active_id (
			GTK_COMBO_BOX (dialog->priv->valign_combo),
			(v_align && *v_align) ? v_align : "middle");
		g_variant_unref (result);
	}

	result = g_dbus_proxy_call_sync (
		web_extension,
		"TableCellElementGetNoWrap",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		gboolean no_wrap;

		g_variant_get (result, "(b)", &no_wrap);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->wrap_text_check), !no_wrap);
		g_variant_unref (result);
	}

	result = g_dbus_proxy_call_sync (
		web_extension,
		"ElementGetTagName",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		const gchar *tag_name;

		g_variant_get (result, "(&s)", &tag_name);
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->priv->header_style_check),
			(g_ascii_strncasecmp (tag_name, "TH", 2) == 0));
		g_variant_unref (result);
	}

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-cell", "width");

	if (result) {
		const gchar *width;

		g_variant_get (result, "(&s)", &width);
		if (width && *width) {
			gint val = atoi (width);
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
		g_variant_unref (result);
	}

	result = g_dbus_proxy_call_sync (
		web_extension,
		"TableCellElementGetRowSpan",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		glong row_span;

		g_variant_get (result, "(i)", &row_span);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->row_span_edit), row_span);
		g_variant_unref (result);
	}

	result = g_dbus_proxy_call_sync (
		web_extension,
		"TableCellElementGetColSpan",
		g_variant_new (
			"(ts)",
			webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-cell"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL);

	if (result) {
		glong col_span;

		g_variant_get (result, "(i)", &col_span);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->priv->col_span_edit), col_span);
		g_variant_unref (result);
	}

	result = g_dbus_proxy_call_sync (
		web_extension,
		"ElementHasAttribute",
		g_variant_new (
			"(tss)",
			webkit_web_view_get_page_id (
				WEBKIT_WEB_VIEW (view)),
			"-x-evo-current-cell",
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
				view, "#-x-evo-current-cell", "data-uri");

			if (result) {
				const gchar *value;

				g_variant_get (result, "(&s)", &value);

				gtk_file_chooser_set_uri (
					GTK_FILE_CHOOSER (dialog->priv->background_image_chooser),
					value);

				g_variant_unref (result);
			}
		} else {
			gtk_file_chooser_unselect_all (
				GTK_FILE_CHOOSER (dialog->priv->background_image_chooser));
			g_variant_unref (result);
		}
	}

	result = e_html_editor_view_get_element_attribute (
		view, "#-x-evo-current-cell", "bgcolor");

	if (result) {
		const gchar *bg_color;

		g_variant_get (result, "(&s)", &bg_color);

		if (bg_color && *bg_color) {
			if (gdk_rgba_parse (&color, bg_color)) {
				e_color_combo_set_current_color (
					E_COLOR_COMBO (dialog->priv->background_color_picker),
					&color);
			} else {
				e_color_combo_set_current_color (
					E_COLOR_COMBO (dialog->priv->background_color_picker),
					&transparent);
			}
		}
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_picker),
			&transparent);

		g_variant_unref (result);
	}

	GTK_WIDGET_CLASS (e_html_editor_cell_dialog_parent_class)->show (widget);
}

static void
html_editor_cell_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorCellDialog *dialog;
	EHTMLEditorView *view;

	dialog = E_HTML_EDITOR_CELL_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_html_editor_view_call_simple_extension_function_sync (
		view, "EHTMLEditorCellDialogSaveHistoryOnExit");

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

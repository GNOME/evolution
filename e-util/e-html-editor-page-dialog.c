/*
 * e-html-editor-page-dialog.h
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

#include "e-html-editor-page-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-color-combo.h"
#include "e-misc-utils.h"
#include "e-dialog-widgets.h"

#define E_HTML_EDITOR_PAGE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_PAGE_DIALOG, EHTMLEditorPageDialogPrivate))

struct _EHTMLEditorPageDialogPrivate {
	GtkWidget *text_color_picker;
	GtkWidget *link_color_picker;
	GtkWidget *background_color_picker;

	GtkWidget *background_template_combo;
	GtkWidget *background_image_filechooser;

	GtkWidget *remove_image_button;
};

typedef struct _Template {
	const gchar *name;
	const gchar *filename;
	GdkRGBA text_color;
	GdkRGBA link_color;
	GdkRGBA background_color;
	gint left_margin;
} Template;

static const Template templates[] = {

	{
		N_("None"),
		NULL,
		{ 0.0, 0.0 , 0.0 , 1 },
		{ 0.3, 0.55, 0.85, 1 },
		{ 1.0, 1.0 , 1.0 , 1 },
		0
	},

	{
		N_("Perforated Paper"),
		"paper.png",
		{ 0.0, 0.0, 0.0, 1 },
		{ 0.0, 0.2, 0.4, 1 },
		{ 1.0, 1.0, 1.0, 1 },
		30
	},

	{
		N_("Blue Ink"),
		"texture.png",
		{ 0.1, 0.12, 0.56, 1 },
		{ 0.0, 0.0,  1.0,  1 },
		{ 1.0, 1.0,  1.0,  1 },
		0
	},

	{
		N_("Paper"),
		"rect.png",
		{ 0, 0, 0, 1 },
		{ 0, 0, 1, 1 },
		{ 1, 1, 1, 1 },
		0
	},

	{
		N_("Ribbon"),
		"ribbon.jpg",
		{ 0.0, 0.0, 0.0, 1 },
		{ 0.6, 0.2, 0.4, 1 },
		{ 1.0, 1.0, 1.0, 1 },
		70
	},

	{
		N_("Midnight"),
		"midnight-stars.jpg",
		{ 1.0, 1.0, 1.0, 1 },
		{ 1.0, 0.6, 0.0, 1 },
		{ 0.0, 0.0, 0.0, 1 },
		0
	},

	{
		N_("Confidential"),
		"confidential-stamp.jpg",
		{ 0.0, 0.0, 0.0, 1 },
		{ 0.0, 0.0, 1.0, 1 },
		{ 1.0, 1.0, 1.0, 1 },
		0
	},

	{
		N_("Draft"),
		"draft-stamp.jpg",
		{ 0.0, 0.0, 0.0, 1 },
		{ 0.0, 0.0, 1.0, 1 },
		{ 1.0, 1.0, 1.0, 1 },
		0
	},

	{
		N_("Graph Paper"),
		"draft-paper.png",
		{ 0.0 , 0.0 , 0.5,  1 },
		{ 0.88, 0.13, 0.14, 1 },
		{ 1.0 , 1.0 , 1.0 , 1 },
		0
	}
};

G_DEFINE_TYPE (
	EHTMLEditorPageDialog,
	e_html_editor_page_dialog,
	E_TYPE_HTML_EDITOR_DIALOG);

static void
html_editor_page_dialog_set_text_color (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GdkRGBA rgba;
	gchar *color;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->text_color_picker), &rgba);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));

	e_html_editor_view_set_element_attribute (
		view, "body", "text", color);

	g_free (color);
}

static void
html_editor_page_dialog_set_link_color (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GdkRGBA rgba;
	gchar *color;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->link_color_picker), &rgba);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));

	e_html_editor_view_set_element_attribute (
		view, "body", "link", color);

	g_free (color);
}

static void
html_editor_page_dialog_set_background_color (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GDBusProxy *web_extension;
	GdkRGBA rgba;
	gchar *color;

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

	e_html_editor_view_set_element_attribute (
		view, "body", "bgcolor", color);

	g_free (color);
}

static void
html_editor_page_dialog_set_background_from_template (EHTMLEditorPageDialog *dialog)
{
	const Template *tmplt;

	tmplt = &templates[
		gtk_combo_box_get_active (
			GTK_COMBO_BOX (dialog->priv->background_template_combo))];

	/* Special case - 'none' template */
	if (tmplt->filename == NULL) {
		gtk_file_chooser_unselect_all (
			GTK_FILE_CHOOSER (dialog->priv->background_image_filechooser));
	} else {
		gchar *filename;

		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->text_color_picker),
			&tmplt->text_color);
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_picker),
			&tmplt->background_color);
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->link_color_picker),
			&tmplt->link_color);

		filename = g_build_filename (EVOLUTION_IMAGESDIR, tmplt->filename, NULL);

		gtk_file_chooser_set_filename (
			GTK_FILE_CHOOSER (dialog->priv->background_image_filechooser),
			filename);
		g_free (filename);
	}

}

static void
html_editor_page_dialog_set_background_image (EHTMLEditorPageDialog *dialog)
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
		GTK_FILE_CHOOSER (dialog->priv->background_image_filechooser));

	if (uri && *uri)
		e_html_editor_view_replace_image_src (view, "body", uri);
	else
		g_dbus_proxy_call (
			web_extension,
			"RemoveImageAttributesFromElementBySelector",
			g_variant_new (
				"(ts)",
				webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)),
				"body"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL,
			NULL);

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_page_dialog_remove_image (EHTMLEditorPageDialog *dialog)
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
			"body"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);

	gtk_file_chooser_unselect_all (
		GTK_FILE_CHOOSER (dialog->priv->background_image_filechooser));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
}

static void
html_editor_page_dialog_show (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorPageDialog *dialog;
	GDBusProxy *web_extension;
	GdkRGBA rgba;
	GVariant *result;

	dialog = E_HTML_EDITOR_PAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	web_extension = e_html_editor_view_get_web_extension_proxy (view);
	if (!web_extension)
		return;

	e_html_editor_view_call_simple_extension_function (
		view, "EHTMLEditorPageDialogSaveHistory");

	result = e_html_editor_view_get_element_attribute (
		view, "body", "data-uri");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);

		if (value && *value) {
			gint ii;
			gchar *fname = g_filename_from_uri (value, NULL, NULL);
			for (ii = 0; ii < G_N_ELEMENTS (templates); ii++) {
				const Template *tmplt = &templates[ii];

				if (g_strcmp0 (tmplt->filename, fname) == 0) {
					gtk_combo_box_set_active (
						GTK_COMBO_BOX (dialog->priv->background_template_combo),
						ii);
					break;
				}
			}
			g_free (fname);
		} else {
			gtk_combo_box_set_active (
				GTK_COMBO_BOX (dialog->priv->background_template_combo), 0);
		}
		g_variant_unref (result);
	}

	result = e_html_editor_view_get_element_attribute (view, "body", "text");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (&rgba, value))
			e_utils_get_theme_color (
				widget,
				"theme_text_color",
				E_UTILS_DEFAULT_THEME_TEXT_COLOR,
				&rgba);
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->text_color_picker), &rgba);
		g_variant_unref (result);
	}

	result = e_html_editor_view_get_element_attribute (view, "body", "link");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value) {
			GdkColor *color = NULL;
			GtkStyleContext *context;

			context = gtk_widget_get_style_context (GTK_WIDGET (view));
			gtk_style_context_get_style (context, "link-color", &color, NULL);

			if (color == NULL) {
				rgba.alpha = 1;
				rgba.red = 0;
				rgba.green = 0;
				rgba.blue = 1;
			} else {
				rgba.alpha = 1;
				rgba.red = ((gdouble) color->red) / G_MAXUINT16;
				rgba.green = ((gdouble) color->green) / G_MAXUINT16;
				rgba.blue = ((gdouble) color->blue) / G_MAXUINT16;
			}
		} else {
			gdk_rgba_parse (&rgba, value);
		}
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->link_color_picker), &rgba);
		g_variant_unref (result);
	}

	result = e_html_editor_view_get_element_attribute (view, "body", "bgcolor");

	if (result) {
		const gchar *value;

		g_variant_get (result, "(&s)", &value);
		if (!value || !*value || !gdk_rgba_parse (&rgba, value))
			e_utils_get_theme_color (
			widget,
			"theme_base_color",
			E_UTILS_DEFAULT_THEME_BASE_COLOR,
			&rgba);
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
		g_variant_unref (result);
	}

	GTK_WIDGET_CLASS (e_html_editor_page_dialog_parent_class)->show (widget);
}

static void
html_editor_page_dialog_hide (GtkWidget *widget)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	EHTMLEditorPageDialog *dialog;

	dialog = E_HTML_EDITOR_PAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_html_editor_view_call_simple_extension_function (
		view, "EHTMLEditorPageDialogSaveHistoryOnExit");

	GTK_WIDGET_CLASS (e_html_editor_page_dialog_parent_class)->hide (widget);
}

static void
e_html_editor_page_dialog_class_init (EHTMLEditorPageDialogClass *class)
{
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EHTMLEditorPageDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_page_dialog_show;
	widget_class->hide = html_editor_page_dialog_hide;
}

static void
e_html_editor_page_dialog_init (EHTMLEditorPageDialog *dialog)
{
	GtkBox *box;
	GtkGrid *grid, *main_layout;
	GtkWidget *widget;
	gint ii;

	dialog->priv = E_HTML_EDITOR_PAGE_DIALOG_GET_PRIVATE (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == Colors == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Colors</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Text */
	widget = e_color_combo_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_page_dialog_set_text_color), dialog);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->text_color_picker = widget;

	widget = gtk_label_new_with_mnemonic (_("_Text:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->text_color_picker);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Link */
	widget = e_color_combo_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_page_dialog_set_link_color), dialog);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	dialog->priv->link_color_picker = widget;

	widget = gtk_label_new_with_mnemonic (_("_Link:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->link_color_picker);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	/* Background */
	widget = e_color_combo_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_page_dialog_set_background_color), dialog);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	dialog->priv->background_color_picker = widget;

	widget = gtk_label_new_with_mnemonic (_("_Background:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_color_picker);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);

	/* == Background Image == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Background Image</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Template */
	widget = gtk_combo_box_text_new ();
	for (ii = 0; ii < G_N_ELEMENTS (templates); ii++) {
		gtk_combo_box_text_append_text (
			GTK_COMBO_BOX_TEXT (widget), templates[ii].name);
	}
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_page_dialog_set_background_from_template), dialog);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->background_template_combo = widget;

	widget = gtk_label_new_with_mnemonic (_("_Template:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_template_combo);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* Custom image */
	widget = gtk_file_chooser_button_new (
		_("Select a file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	g_signal_connect_swapped (
		widget, "selection-changed",
		G_CALLBACK (html_editor_page_dialog_set_background_image), dialog);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	dialog->priv->background_image_filechooser = widget;

	widget = gtk_label_new_with_mnemonic (_("_Custom:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (widget), dialog->priv->background_image_filechooser);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	box = e_html_editor_dialog_get_button_box (E_HTML_EDITOR_DIALOG (dialog));
	widget = e_dialog_button_new_with_icon (NULL, _("_Remove image"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (html_editor_page_dialog_remove_image), dialog);
	dialog->priv->remove_image_button = widget;

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, FALSE);
	gtk_box_pack_start (box, widget, FALSE, FALSE, 5);
	gtk_box_reorder_child (box, widget, 0);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_page_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_PAGE_DIALOG,
			"editor", editor,
			"title", _("Page Properties"),
			NULL));
}

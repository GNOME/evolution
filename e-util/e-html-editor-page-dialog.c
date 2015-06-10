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

	EHTMLEditorViewHistoryEvent *history_event;
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
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	GdkRGBA rgba;
	gchar *color;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->text_color_picker), &rgba);

	color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	webkit_dom_html_body_element_set_text (
		WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);

	g_free (color);
}

static void
html_editor_page_dialog_set_link_color (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->link_color_picker), &rgba);

	e_html_editor_view_set_link_color (view, &rgba);
}

static void
html_editor_page_dialog_set_background_color (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	GdkRGBA rgba;
	gchar *color;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);
	if (rgba.alpha != 0.0)
		color = g_strdup_printf ("#%06x", e_rgba_to_value (&rgba));
	else
		color = g_strdup ("");

	webkit_dom_html_body_element_set_bg_color (
		WEBKIT_DOM_HTML_BODY_ELEMENT (body), color);

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
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	gchar *uri;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	uri = gtk_file_chooser_get_uri (
			GTK_FILE_CHOOSER (
				dialog->priv->background_image_filechooser));

	if (uri && *uri)
		e_html_editor_selection_replace_image_src (
			e_html_editor_view_get_selection (view),
			WEBKIT_DOM_ELEMENT (body),
			uri);
	else
		remove_image_attributes_from_element (
			WEBKIT_DOM_ELEMENT (body));

	gtk_widget_set_sensitive (dialog->priv->remove_image_button, uri && *uri);

	g_free (uri);
}

static void
html_editor_page_dialog_remove_image (EHTMLEditorPageDialog *dialog)
{
	EHTMLEditor *editor;
	EHTMLEditorView *view;
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	remove_image_attributes_from_element (WEBKIT_DOM_ELEMENT (body));

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
	WebKitDOMDocument *document;
	WebKitDOMHTMLElement *body;
	gchar *tmp;
	GdkRGBA rgba;

	dialog = E_HTML_EDITOR_PAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);

	/* We have to block the style changes of the view as otherwise the colors
	 * will be changed when this dialog will be shown (as the view will be
	 * unfocused). */
	e_html_editor_view_block_style_updated_callbacks (view);

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
	body = webkit_dom_document_get_body (document);

	if (!e_html_editor_view_is_undo_redo_in_progress (view)) {
		EHTMLEditorSelection *selection;
		EHTMLEditorViewHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorViewHistoryEvent, 1);
		ev->type = HISTORY_PAGE_DIALOG;

		selection = e_html_editor_view_get_selection (view);
		e_html_editor_selection_get_selection_coordinates (
			selection, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		ev->data.dom.from = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), FALSE);
		dialog->priv->history_event = ev;
	}

	tmp = webkit_dom_element_get_attribute (
		WEBKIT_DOM_ELEMENT (body), "data-uri");
	if (tmp && *tmp) {
		gint ii;
		gchar *fname = g_filename_from_uri (tmp, NULL, NULL);
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
	g_free (tmp);

	tmp = webkit_dom_html_body_element_get_text (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body));
	if (!tmp || !*tmp || !gdk_rgba_parse (&rgba, tmp))
		e_utils_get_theme_color (widget, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &rgba);
	g_free (tmp);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->text_color_picker), &rgba);

	tmp = webkit_dom_html_body_element_get_link (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body));
	if (!gdk_rgba_parse (&rgba, tmp)) {
		rgba.alpha = 1;
		rgba.red = 0;
		rgba.green = 0;
		rgba.blue = 1;
	}
	g_free (tmp);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->link_color_picker), &rgba);

	tmp = webkit_dom_html_body_element_get_bg_color (
			WEBKIT_DOM_HTML_BODY_ELEMENT (body));
	if (!tmp || !*tmp || !gdk_rgba_parse (&rgba, tmp))
		e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &rgba);
	g_free (tmp);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->background_color_picker), &rgba);

	GTK_WIDGET_CLASS (e_html_editor_page_dialog_parent_class)->show (widget);
}

static gboolean
user_changed_content (EHTMLEditorViewHistoryEvent *event)
{
	WebKitDOMElement *original, *current;
	gchar *original_value, *current_value;
	gboolean changed = TRUE;

	original = WEBKIT_DOM_ELEMENT (event->data.dom.from);
	current = WEBKIT_DOM_ELEMENT (event->data.dom.to);

	original_value = webkit_dom_element_get_attribute (original, "bgcolor");
	current_value = webkit_dom_element_get_attribute (current, "bgcolor");
	changed = g_strcmp0 (original_value, current_value) != 0;
	g_free (original_value);
	g_free (current_value);
	if (changed)
		return TRUE;

	original_value = webkit_dom_element_get_attribute (original, "text");
	current_value = webkit_dom_element_get_attribute (current, "text");
	changed = g_strcmp0 (original_value, current_value) != 0;
	g_free (original_value);
	g_free (current_value);
	if (changed)
		return TRUE;

	original_value = webkit_dom_element_get_attribute (original, "link");
	current_value = webkit_dom_element_get_attribute (current, "link");
	changed = g_strcmp0 (original_value, current_value) != 0;
	g_free (original_value);
	g_free (current_value);

	return changed;
}

static void
html_editor_page_dialog_hide (GtkWidget *widget)
{
	EHTMLEditorPageDialogPrivate *priv;
	EHTMLEditorViewHistoryEvent *ev;
	EHTMLEditorPageDialog *dialog;
	EHTMLEditor *editor;
	EHTMLEditorView *view;

	dialog = E_HTML_EDITOR_PAGE_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	view = e_html_editor_get_view (editor);
	priv = E_HTML_EDITOR_PAGE_DIALOG_GET_PRIVATE (widget);
	ev = priv->history_event;

	if (ev) {
		EHTMLEditorSelection *selection;
		WebKitDOMDocument *document;
		WebKitDOMHTMLElement *body;

		selection = e_html_editor_view_get_selection (view);

		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
		body = webkit_dom_document_get_body (document);

		ev->data.dom.to = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), FALSE);

		/* If user changed any of page colors we have to mark it to send
		 * the correct colors and to disable the color changes when the
		 * view i.e. not focused (at it would overwrite these user set colors. */
		if (user_changed_content (ev))
			webkit_dom_element_set_attribute (
				WEBKIT_DOM_ELEMENT (body), "data-user-colors", "", NULL);

		if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
			e_html_editor_selection_get_selection_coordinates (
				selection, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
			e_html_editor_view_insert_new_history_event (view, ev);
		} else {
			g_object_unref (ev->data.dom.from);
			g_object_unref (ev->data.dom.to);
			g_free (ev);
		}
	}

	e_html_editor_view_unblock_style_updated_callbacks (view);

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

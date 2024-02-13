/*
 * e-html-editor-text-dialog.c
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

#include "e-html-editor-text-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-color-combo.h"

struct _EHTMLEditorTextDialogPrivate {
	GtkWidget *bold_check;
	GtkWidget *italic_check;
	GtkWidget *underline_check;
	GtkWidget *strikethrough_check;

	GtkWidget *color_check;
	GtkWidget *size_check;
};

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorTextDialog, e_html_editor_text_dialog, E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_text_dialog_set_bold (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_bold (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->bold_check)));
}

static void
html_editor_text_dialog_set_italic (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_italic (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->italic_check)));
}

static void
html_editor_text_dialog_set_underline (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_underline (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->underline_check)));
}

static void
html_editor_text_dialog_set_strikethrough (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_strikethrough (
		cnt_editor,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->strikethrough_check)));
}

static void
html_editor_text_dialog_set_color (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GdkRGBA rgba;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->color_check), &rgba);
	e_content_editor_set_font_color (cnt_editor, &rgba);
}

static void
html_editor_text_dialog_set_size (EHTMLEditorTextDialog *dialog)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	gint size;

	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);
	size = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->size_check));

	e_content_editor_set_font_size (cnt_editor, size + 1);
}

static void
html_editor_text_dialog_show (GtkWidget *widget)
{
	EHTMLEditorTextDialog *dialog;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GdkRGBA *rgba;

	dialog = E_HTML_EDITOR_TEXT_DIALOG (widget);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));
	cnt_editor = e_html_editor_get_content_editor (editor);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->bold_check),
		e_content_editor_is_bold (cnt_editor));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->italic_check),
		e_content_editor_is_italic (cnt_editor));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->underline_check),
		e_content_editor_is_underline (cnt_editor));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->strikethrough_check),
		e_content_editor_is_strikethrough (cnt_editor));

	gtk_combo_box_set_active (
		GTK_COMBO_BOX (dialog->priv->size_check),
		e_content_editor_get_font_size (cnt_editor) - 1);

	rgba = e_content_editor_dup_font_color (cnt_editor);
	if (rgba) {
		e_color_combo_set_current_color (
			E_COLOR_COMBO (dialog->priv->color_check), rgba);
		gdk_rgba_free (rgba);
	}

	GTK_WIDGET_CLASS (e_html_editor_text_dialog_parent_class)->show (widget);
}

static void
e_html_editor_text_dialog_class_init (EHTMLEditorTextDialogClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_text_dialog_show;
}

static void
e_html_editor_text_dialog_init (EHTMLEditorTextDialog *dialog)
{
	GtkGrid *main_layout;
	GtkWidget *widget;

	dialog->priv = e_html_editor_text_dialog_get_instance_private (dialog);

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* Bold */
	widget = gtk_image_new_from_stock ("format-text-bold", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Bold"));
	gtk_grid_attach (main_layout, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_text_dialog_set_bold), dialog);
	dialog->priv->bold_check = widget;

	/* Italic */
	widget = gtk_image_new_from_stock ("format-text-italic", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Italic"));
	gtk_grid_attach (main_layout, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_text_dialog_set_italic), dialog);
	dialog->priv->italic_check = widget;

	/* Underline */
	widget = gtk_image_new_from_stock ("format-text-underline", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Underline"));
	gtk_grid_attach (main_layout, widget, 1, 2, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_text_dialog_set_underline), dialog);
	dialog->priv->underline_check = widget;

	widget = gtk_image_new_from_stock ("format-text-strikethrough", GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 3, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Strikethrough"));
	gtk_grid_attach (main_layout, widget, 1, 3, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (html_editor_text_dialog_set_strikethrough), dialog);
	dialog->priv->strikethrough_check = widget;

	/* Color */
	widget = e_color_combo_new ();
	gtk_grid_attach (main_layout, widget, 3, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (html_editor_text_dialog_set_color), dialog);
	dialog->priv->color_check = widget;

	widget = gtk_label_new_with_mnemonic (_("_Color:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->color_check);
	gtk_grid_attach (main_layout, widget, 2, 0, 1, 1);

	/* Size */
	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "minus-two", "-2");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "minus-one", "-1");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "minus-zero", "0");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "plus-one", "+1");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "plus-two", "+2");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "plus-three", "+3");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "plus-four", "+4");
	gtk_grid_attach (main_layout, widget, 3, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (html_editor_text_dialog_set_size), dialog);
	dialog->priv->size_check = widget;

	widget = gtk_label_new_with_mnemonic (_("Si_ze:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_check);
	gtk_grid_attach (main_layout, widget, 2, 1, 1, 1);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

GtkWidget *
e_html_editor_text_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_TEXT_DIALOG,
			"editor", editor,
			"title", _("Text Properties"),
			NULL));
}

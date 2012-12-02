/*
 * e-editor-text-dialog.c
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

#include "e-editor-text-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-color-combo.h"

G_DEFINE_TYPE (
	EEditorTextDialog,
	e_editor_text_dialog,
	E_TYPE_EDITOR_DIALOG);

struct _EEditorTextDialogPrivate {
	GtkWidget *bold_check;
	GtkWidget *italic_check;
	GtkWidget *underline_check;
	GtkWidget *strikethrough_check;

	GtkWidget *color_check;
	GtkWidget *size_check;
};


static void
editor_text_dialog_set_bold (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_editor_selection_set_bold (
		selection,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->bold_check)));
}

static void
editor_text_dialog_set_italic (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_editor_selection_set_italic (
		selection,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->italic_check)));
}

static void
editor_text_dialog_set_underline (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_editor_selection_set_underline (
		selection,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->underline_check)));
}

static void
editor_text_dialog_set_strikethrough (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_editor_selection_set_strike_through (
		selection,
		gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (dialog->priv->strikethrough_check)));
}

static void
editor_text_dialog_set_color (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;
	GdkRGBA rgba;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	e_color_combo_get_current_color (
		E_COLOR_COMBO (dialog->priv->color_check), &rgba);
	e_editor_selection_set_font_color (selection, &rgba);
}

static void
editor_text_dialog_set_size (EEditorTextDialog *dialog)
{
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;
	gint size;

	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);
	size = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->priv->size_check));

	e_editor_selection_set_font_size (selection, size + 1);
}

static void
editor_text_dialog_show (GtkWidget *gtk_widget)
{
	EEditorTextDialog *dialog;
	EEditor *editor;
	EEditorWidget *widget;
	EEditorSelection *selection;
	GdkRGBA rgba;

	dialog = E_EDITOR_TEXT_DIALOG (gtk_widget);
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));
	widget = e_editor_get_editor_widget (editor);
	selection = e_editor_widget_get_selection (widget);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->bold_check),
		e_editor_selection_is_bold (selection));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->italic_check),
		e_editor_selection_is_italic (selection));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->underline_check),
		e_editor_selection_is_underline (selection));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->priv->strikethrough_check),
		e_editor_selection_is_strike_through (selection));

	gtk_combo_box_set_active (
		GTK_COMBO_BOX (dialog->priv->size_check),
		e_editor_selection_get_font_size (selection));

	e_editor_selection_get_font_color (selection, &rgba);
	e_color_combo_set_current_color (
		E_COLOR_COMBO (dialog->priv->color_check), &rgba);

	GTK_WIDGET_CLASS (e_editor_text_dialog_parent_class)->show (gtk_widget);
}

static void
e_editor_text_dialog_class_init (EEditorTextDialogClass *klass)
{
	GtkWidgetClass *widget_class;

	e_editor_text_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorTextDialogPrivate));

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_text_dialog_show;
}

static void
e_editor_text_dialog_init (EEditorTextDialog *dialog)
{
	GtkGrid *main_layout;
	GtkWidget *widget;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_TEXT_DIALOG, EEditorTextDialogPrivate);

	main_layout = e_editor_dialog_get_container (E_EDITOR_DIALOG (dialog));

	/* Bold */
	widget = gtk_image_new_from_stock (GTK_STOCK_BOLD, GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Bold"));
	gtk_grid_attach (main_layout, widget, 1, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (editor_text_dialog_set_bold), dialog);
	dialog->priv->bold_check = widget;

	/* Italic */
	widget = gtk_image_new_from_stock (GTK_STOCK_ITALIC, GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 1, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Italic"));
	gtk_grid_attach (main_layout, widget, 1, 1, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (editor_text_dialog_set_italic), dialog);
	dialog->priv->italic_check = widget;

	/* Underline */
	widget = gtk_image_new_from_stock (GTK_STOCK_UNDERLINE, GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Underline"));
	gtk_grid_attach (main_layout, widget, 1, 2, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (editor_text_dialog_set_underline), dialog);
	dialog->priv->underline_check = widget;

	widget = gtk_image_new_from_stock (GTK_STOCK_STRIKETHROUGH, GTK_ICON_SIZE_BUTTON);
	gtk_grid_attach (main_layout, widget, 0, 3, 1, 1);

	widget = gtk_check_button_new_with_mnemonic (_("_Strikethrough"));
	gtk_grid_attach (main_layout, widget, 1, 3, 1, 1);
	g_signal_connect_swapped (
		widget, "toggled",
		G_CALLBACK (editor_text_dialog_set_strikethrough), dialog);
	dialog->priv->strikethrough_check = widget;

	/* Color */
	widget = e_color_combo_new ();
	gtk_grid_attach (main_layout, widget, 3, 0, 1, 1);
	g_signal_connect_swapped (
		widget, "notify::current-color",
		G_CALLBACK (editor_text_dialog_set_color), dialog);
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
		G_CALLBACK (editor_text_dialog_set_size), dialog);
	dialog->priv->size_check = widget;

	widget = gtk_label_new_with_mnemonic (_("Si_ze:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->size_check);
	gtk_grid_attach (main_layout, widget, 2, 1, 1, 1);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}


GtkWidget *
e_editor_text_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_TEXT_DIALOG,
			"editor", editor,
			"title", N_("Text Properties"),
			NULL));
}

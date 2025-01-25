/*
 * e-html-editor-paragraph-dialog.c
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

#include "e-html-editor-paragraph-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-action-combo-box.h"

struct _EHTMLEditorParagraphDialogPrivate {
	GtkWidget *style_combo;

	GtkWidget *left_button;
	GtkWidget *center_button;
	GtkWidget *right_button;
	GtkWidget *justified_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (EHTMLEditorParagraphDialog, e_html_editor_paragraph_dialog, E_TYPE_HTML_EDITOR_DIALOG)

static void
html_editor_paragraph_dialog_constructed (GObject *object)
{
	GtkGrid *main_layout, *grid;
	GtkWidget *widget;
	EHTMLEditor *editor;
	EHTMLEditorParagraphDialog *dialog;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_html_editor_paragraph_dialog_parent_class)->constructed (object);

	dialog = E_HTML_EDITOR_PARAGRAPH_DIALOG (object);
	editor = e_html_editor_dialog_get_editor (E_HTML_EDITOR_DIALOG (dialog));

	e_ui_manager_add_action_groups_to_widget (e_html_editor_get_ui_manager (editor), GTK_WIDGET (dialog));

	main_layout = e_html_editor_dialog_get_container (E_HTML_EDITOR_DIALOG (dialog));

	/* == General == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>General</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 0, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 1, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Style */
	widget = e_action_combo_box_new_with_action (e_html_editor_get_action (editor, "style-normal"));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->style_combo = widget;

	widget = gtk_label_new_with_mnemonic (_("_Style:"));
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_RIGHT);
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->style_combo);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* == Alignment == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Alignment</b>"));
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_grid_attach (main_layout, widget, 0, 2, 1, 1);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_grid_attach (main_layout, GTK_WIDGET (grid), 0, 3, 1, 1);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 10);

	/* Left */
	widget = gtk_toggle_button_new_with_label (_("_Left"));
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	e_ui_action_util_assign_to_widget (e_html_editor_get_action (editor, "justify-left"), widget);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	dialog->priv->left_button = widget;

	/* Center */
	widget = gtk_toggle_button_new_with_label (_("_Center"));
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	e_ui_action_util_assign_to_widget (e_html_editor_get_action (editor, "justify-center"), widget);
	dialog->priv->center_button = widget;

	/* Right */
	widget = gtk_toggle_button_new_with_label (_("_Right"));
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	e_ui_action_util_assign_to_widget (e_html_editor_get_action (editor, "justify-right"), widget);
	dialog->priv->right_button = widget;

	/* Justified */
	widget = gtk_toggle_button_new_with_label (_("_Justified"));
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	e_ui_action_util_assign_to_widget (e_html_editor_get_action (editor, "justify-fill"), widget);
	dialog->priv->justified_button = widget;

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

static void
e_html_editor_paragraph_dialog_class_init (EHTMLEditorParagraphDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = html_editor_paragraph_dialog_constructed;
}

static void
e_html_editor_paragraph_dialog_init (EHTMLEditorParagraphDialog *dialog)
{
	dialog->priv = e_html_editor_paragraph_dialog_get_instance_private (dialog);
}

GtkWidget *
e_html_editor_paragraph_dialog_new (EHTMLEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_HTML_EDITOR_PARAGRAPH_DIALOG,
			"editor", editor,
			"title", _("Paragraph Properties"),
			NULL));
}

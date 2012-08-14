/*
 * e-editor-text-dialog.c
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

#include "e-editor-paragraph-dialog.h"

#include <glib/gi18n-lib.h>

#include "e-action-combo-box.h"

G_DEFINE_TYPE (
	EEditorParagraphDialog,
	e_editor_paragraph_dialog,
	E_TYPE_EDITOR_DIALOG);

struct _EEditorParagraphDialogPrivate {
	GtkWidget *style_combo;

	GtkWidget *left_button;
	GtkWidget *center_button;
	GtkWidget *right_button;

	GtkWidget *close_button;
};

static void
editor_paragraph_dialog_constructed (GObject *object)
{
	GtkBox *main_layout;
	GtkGrid *grid;
	GtkWidget *widget;
	EEditor *editor;
	EEditorParagraphDialog *dialog;

	dialog = E_EDITOR_PARAGRAPH_DIALOG (object);
	editor = e_editor_dialog_get_editor (E_EDITOR_DIALOG (dialog));

	main_layout = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	/* == General == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>General</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 5);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 0);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Style */
	widget = e_action_combo_box_new_with_action (
		GTK_RADIO_ACTION (e_editor_get_action (editor, "style-normal")));
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	dialog->priv->style_combo = widget;

	widget = gtk_label_new_with_mnemonic (_("Style:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), dialog->priv->style_combo);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);

	/* == Alignment == */
	widget = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (widget), _("<b>Alignment</b>"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 5);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 5);
	gtk_grid_set_column_spacing (grid, 5);
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 0);
	gtk_widget_set_margin_left (GTK_WIDGET (grid), 10);

	/* Left */
	widget = gtk_toggle_button_new_with_label (GTK_STOCK_JUSTIFY_LEFT);
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (widget),
		e_editor_get_action (editor, "justify-left"));
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	dialog->priv->left_button = widget;

	/* Center */
	widget = gtk_toggle_button_new_with_label (GTK_STOCK_JUSTIFY_CENTER);
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (widget),
		e_editor_get_action (editor, "justify-center"));
	dialog->priv->center_button = widget;

	/* Right */
	widget = gtk_toggle_button_new_with_label (GTK_STOCK_JUSTIFY_RIGHT);
	gtk_button_set_use_stock (GTK_BUTTON (widget), TRUE);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	gtk_activatable_set_related_action (
		GTK_ACTIVATABLE (widget),
		e_editor_get_action (editor, "justify-right"));
	dialog->priv->right_button = widget;

	/* == Button box == */
	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_hide), dialog);
	dialog->priv->close_button = widget;

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_pack_start (main_layout, widget, TRUE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (widget), dialog->priv->close_button, FALSE, FALSE, 5);

	gtk_widget_show_all (GTK_WIDGET (main_layout));
}

static void
e_editor_paragraph_dialog_class_init (EEditorParagraphDialogClass *klass)
{
	GObjectClass *object_class;

	e_editor_paragraph_dialog_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorParagraphDialogPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = editor_paragraph_dialog_constructed;
}

static void
e_editor_paragraph_dialog_init (EEditorParagraphDialog *dialog)
{
	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_PARAGRAPH_DIALOG, EEditorParagraphDialogPrivate);
}

GtkWidget *
e_editor_paragraph_dialog_new (EEditor *editor)
{
	return GTK_WIDGET (
		g_object_new (
			E_TYPE_EDITOR_PARAGRAPH_DIALOG,
			"editor", editor,
			"title", N_("Paragraph Properties"),
			NULL));
}

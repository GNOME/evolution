/*
 * e-editor-dialog.h
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

#include "e-editor-dialog.h"

G_DEFINE_ABSTRACT_TYPE (
	EEditorDialog,
	e_editor_dialog,
	GTK_TYPE_WINDOW);

struct _EEditorDialogPrivate {
	EEditor *editor;

	GtkBox *button_box;
	GtkGrid *container;
};

enum {
	PROP_0,
	PROP_EDITOR,
};


static void
editor_dialog_set_editor (EEditorDialog *dialog,
			  EEditor *editor)
{
	dialog->priv->editor = g_object_ref (editor);

	g_object_notify (G_OBJECT (dialog), "editor");
}

static void
editor_dialog_get_property (GObject *object,
			    guint property_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_set_object (value,
			e_editor_dialog_get_editor (E_EDITOR_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_dialog_set_property (GObject *object,
			    guint property_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			editor_dialog_set_editor (
				E_EDITOR_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
editor_dialog_constructed (GObject *object)
{
	EEditorDialog *dialog = E_EDITOR_DIALOG (object);

	/* Chain up to parent implementation first */
	G_OBJECT_CLASS (e_editor_dialog_parent_class)->constructed (object);

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog),
		GTK_WINDOW (gtk_widget_get_toplevel (
				GTK_WIDGET (dialog->priv->editor))));
}

static void
editor_dialog_show (GtkWidget *widget)
{
	gtk_widget_show_all (GTK_WIDGET (E_EDITOR_DIALOG (widget)->priv->container));
	gtk_widget_show_all (GTK_WIDGET (E_EDITOR_DIALOG (widget)->priv->button_box));

	GTK_WIDGET_CLASS (e_editor_dialog_parent_class)->show (widget);
}

static void
editor_dialog_dispose (GObject *object)
{
	EEditorDialogPrivate *priv = E_EDITOR_DIALOG (object)->priv;

	g_clear_object (&priv->editor);

	/* Chain up to parent's implementation */
	G_OBJECT_CLASS (e_editor_dialog_parent_class)->dispose (object);
}

static void
e_editor_dialog_class_init (EEditorDialogClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorDialogPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = editor_dialog_get_property;
	object_class->set_property = editor_dialog_set_property;
	object_class->dispose = editor_dialog_dispose;
	object_class->constructed = editor_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show = editor_dialog_show;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
		        NULL,
		       	NULL,
		        E_TYPE_EDITOR,
		        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static void
e_editor_dialog_init (EEditorDialog *dialog)
{
	GtkBox *main_layout;
	GtkGrid *grid;
	GtkWidget *widget, *button_box;

	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_DIALOG, EEditorDialogPrivate);

	main_layout = GTK_BOX (gtk_vbox_new (FALSE, 5));
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 10);
	gtk_grid_set_column_spacing (grid, 10);
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 5);
	dialog->priv->container = grid;

	/* == Button box == */
	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_hide), dialog);

	button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 5);
	gtk_box_pack_start (main_layout, button_box, TRUE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (button_box), widget, FALSE, FALSE, 5);
	dialog->priv->button_box = GTK_BOX (button_box);

	gtk_widget_show_all (GTK_WIDGET (main_layout));

	g_object_set (
		G_OBJECT (dialog),
		"destroy-with-parent", TRUE,
	        "modal", TRUE,
		"resizable", FALSE,
		"type-hint", GDK_WINDOW_TYPE_HINT_POPUP_MENU,
		"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
		NULL);

	/* Don't destroy the dialog when closed! */
	g_signal_connect (
		dialog, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}



EEditor *
e_editor_dialog_get_editor (EEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->editor;
}

GtkGrid *
e_editor_dialog_get_container (EEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->container;
}

GtkBox *
e_editor_dialog_get_button_box (EEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->button_box;
}


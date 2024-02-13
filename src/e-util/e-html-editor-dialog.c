/*
 * e-html-editor-dialog.h
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

#include <glib/gi18n-lib.h>

#include "e-html-editor-dialog.h"
#include "e-dialog-widgets.h"

struct _EHTMLEditorDialogPrivate {
	EHTMLEditor *editor;

	GtkBox *button_box;
	GtkGrid *container;
};

enum {
	PROP_0,
	PROP_EDITOR,
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EHTMLEditorDialog, e_html_editor_dialog, GTK_TYPE_WINDOW)

static void
html_editor_dialog_set_editor (EHTMLEditorDialog *dialog,
                               EHTMLEditor *editor)
{
	dialog->priv->editor = g_object_ref (editor);

	g_object_notify (G_OBJECT (dialog), "editor");
}

static void
html_editor_dialog_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_set_object (
				value,
			e_html_editor_dialog_get_editor (
				E_HTML_EDITOR_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_dialog_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			html_editor_dialog_set_editor (
				E_HTML_EDITOR_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
html_editor_dialog_constructed (GObject *object)
{
	EHTMLEditorDialog *dialog = E_HTML_EDITOR_DIALOG (object);

	/* Chain up to parent implementation first */
	G_OBJECT_CLASS (e_html_editor_dialog_parent_class)->constructed (object);

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog),
		GTK_WINDOW (gtk_widget_get_toplevel (
				GTK_WIDGET (dialog->priv->editor))));
}

static void
html_editor_dialog_show (GtkWidget *widget)
{
	EHTMLEditorDialog *self = E_HTML_EDITOR_DIALOG (widget);

	gtk_widget_show_all (GTK_WIDGET (self->priv->container));
	gtk_widget_show_all (GTK_WIDGET (self->priv->button_box));

	GTK_WIDGET_CLASS (e_html_editor_dialog_parent_class)->show (widget);
}

static void
html_editor_dialog_dispose (GObject *object)
{
	EHTMLEditorDialog *self = E_HTML_EDITOR_DIALOG (object);

	g_clear_object (&self->priv->editor);

	/* Chain up to parent's implementation */
	G_OBJECT_CLASS (e_html_editor_dialog_parent_class)->dispose (object);
}

static void
e_html_editor_dialog_class_init (EHTMLEditorDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = html_editor_dialog_get_property;
	object_class->set_property = html_editor_dialog_set_property;
	object_class->dispose = html_editor_dialog_dispose;
	object_class->constructed = html_editor_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = html_editor_dialog_show;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			NULL,
			NULL,
			E_TYPE_HTML_EDITOR,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static gboolean
key_press_event_cb (GtkWidget *widget,
                    GdkEventKey *event,
                    gpointer user_data)
{
	if (event->keyval == GDK_KEY_Escape) {
		gtk_widget_hide (widget);
		return TRUE;
	}

	return FALSE;
}

static void
e_html_editor_dialog_init (EHTMLEditorDialog *dialog)
{
	GtkBox *main_layout;
	GtkGrid *grid;
	GtkWidget *widget, *button_box;

	dialog->priv = e_html_editor_dialog_get_instance_private (dialog);

	main_layout = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (main_layout));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 10);
	gtk_grid_set_column_spacing (grid, 10);
	gtk_box_pack_start (main_layout, GTK_WIDGET (grid), TRUE, TRUE, 5);
	dialog->priv->container = grid;

	/* == Button box == */
	widget = e_dialog_button_new_with_icon ("window-close", _("_Close"));
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
		"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
		NULL);

	/* Don't destroy the dialog when closed! */
	g_signal_connect (
		dialog, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	g_signal_connect (
		dialog, "key-press-event",
		G_CALLBACK (key_press_event_cb), NULL);
}

EHTMLEditor *
e_html_editor_dialog_get_editor (EHTMLEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->editor;
}

GtkGrid *
e_html_editor_dialog_get_container (EHTMLEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->container;
}

GtkBox *
e_html_editor_dialog_get_button_box (EHTMLEditorDialog *dialog)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_DIALOG (dialog), NULL);

	return dialog->priv->button_box;
}


/*
 * e-editor-dialog.h
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

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog),
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (editor))));

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

	g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EEditorDialogPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = editor_dialog_get_property;
	object_class->set_property = editor_dialog_set_property;
	object_class->dispose = editor_dialog_dispose;

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
	dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		dialog, E_TYPE_EDITOR_DIALOG, EEditorDialogPrivate);

	g_object_set (
		G_OBJECT (dialog),
		"destroy-with-parent", TRUE,
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

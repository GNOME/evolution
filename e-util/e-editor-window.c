/*
 * e-editor-window.h
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

#ifndef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-window.h"

#define E_EDITOR_WINDOW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_WINDOW, EEditorWindowPrivate))

/**
 * EEditorWindow:
 *
 * A #GtkWindow that contains main toolbars and an #EEditor. To create a
 * custom editor window, one can subclass this class and pack additional
 * widgets above and below the editor using e_editor_window_pack_above()
 * and e_editor_window_pack_below().
 */

struct _EEditorWindowPrivate {
	EEditor *editor;
	GtkGrid *main_layout;

	GtkWidget *main_menu;
	GtkWidget *main_toolbar;

	gint editor_row;
};

enum {
	PROP_0,
	PROP_EDITOR
};

G_DEFINE_TYPE (
	EEditorWindow,
	e_editor_window,
	GTK_TYPE_WINDOW)

static void
editor_window_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EDITOR:
			g_value_set_object (
				value,
				e_editor_window_get_editor (
				E_EDITOR_WINDOW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_window_class_init (EEditorWindowClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EEditorWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = editor_window_get_property;

	g_object_class_install_property (
		object_class,
		PROP_EDITOR,
		g_param_spec_object (
			"editor",
			NULL,
			NULL,
			E_TYPE_EDITOR,
			G_PARAM_READABLE));
}

static void
e_editor_window_init (EEditorWindow *window)
{
	EEditorWindowPrivate *priv;
	GtkWidget *widget;

	window->priv = E_EDITOR_WINDOW_GET_PRIVATE (window);

	priv = window->priv;
	priv->editor = E_EDITOR (e_editor_new ());

	priv->main_layout = GTK_GRID (gtk_grid_new ());
	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (priv->main_layout), GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (priv->main_layout));
	gtk_widget_show (GTK_WIDGET (priv->main_layout));

	widget = e_editor_get_managed_widget (priv->editor, "/main-menu");
	gtk_grid_attach (priv->main_layout, widget, 0, 0, 1, 1);
	gtk_widget_set_hexpand (widget, TRUE);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_editor_get_managed_widget (priv->editor, "/main-toolbar");
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (
		priv->main_layout, widget, 0, 1, 1, 1);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (widget),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	gtk_widget_set_hexpand (GTK_WIDGET (priv->editor), TRUE);
	gtk_grid_attach (
		priv->main_layout, GTK_WIDGET (priv->editor),
		0, 2, 1, 1);
	gtk_widget_show (GTK_WIDGET (priv->editor));
	priv->editor_row = 2;
}

/**
 * e_editor_window_new:
 * @type: #GtkWindowType
 *
 * Creates a new editor window.
 *
 * Returns: A newly created editor window. [transfer-full]
 */
GtkWidget *
e_editor_window_new (GtkWindowType type)
{
	return g_object_new (
		E_TYPE_EDITOR_WINDOW,
		"type", type,
		NULL);
}

/**
 * e_editor_window_get_editor:
 * @window: an #EEditorWindow
 *
 * Returns the #EEditor widget used in this @window.
 */
EEditor *
e_editor_window_get_editor (EEditorWindow *window)
{
	g_return_val_if_fail (E_IS_EDITOR_WINDOW (window), NULL);

	return window->priv->editor;
}

/**
 * e_editor_window_pack_above:
 * @window: an #EEditorWindow
 * @child: a #GtkWidget
 *
 * Inserts @child in between the mail toolbars and the editor widget. If there
 * are multiple children, the new @child is placed at the end (immediatelly
 * adjacent to the editor widget)
 */
void
e_editor_window_pack_above (EEditorWindow *window,
                            GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR_WINDOW (window));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_insert_row (
		window->priv->main_layout, window->priv->editor_row);
	window->priv->editor_row++;

	gtk_grid_attach_next_to (
		window->priv->main_layout, child,
		GTK_WIDGET (window->priv->editor),
		GTK_POS_TOP, 1, 1);
}

/**
 * e_editor_window_pack_below:
 * @window: an #EEditorWindow
 * @child: a #GtkWidget
 *
 * Inserts @child below the editor widget. If there are multiple children, the
 * new @child is placed at the end.
 */
void
e_editor_window_pack_below (EEditorWindow *window,
                            GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR_WINDOW (window));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_grid_attach_next_to (
		window->priv->main_layout, child,
		NULL, GTK_POS_BOTTOM, 1, 1);
}

/**
 * e_editor_window_pack_inside:
 * @window: an #EEditorWindow
 * @child: a #GtkWidget
 *
 * Inserts @child between the editor's toolbars and the editor itself.
 * If there are multiple children, the new @child is places at the end
 * (immediatelly adjacent to the editor itself).
 */
void
e_editor_window_pack_inside (EEditorWindow *window,
                             GtkWidget *child)
{
	g_return_if_fail (E_IS_EDITOR_WINDOW (window));
	g_return_if_fail (GTK_IS_WIDGET (child));

	e_editor_pack_above (e_editor_window_get_editor (window), child);
}

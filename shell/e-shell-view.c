/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-view.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell-view.h"

#include <glib/gi18n.h>

#include "e-shell-window.h"
#include "e-shell-window-actions.h"

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

struct _EShellViewPrivate {
	gchar *title;
	gpointer window;  /* weak pointer */
};

enum {
	PROP_0,
	PROP_TITLE,
	PROP_WINDOW
};

static gpointer parent_class;

static void
shell_view_set_window (EShellView *shell_view,
                       GtkWidget *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));

	shell_view->priv->window = window;

	g_object_add_weak_pointer (
		G_OBJECT (window), &shell_view->priv->window);
}

static void
shell_view_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			e_shell_view_set_title (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_WINDOW:
			shell_view_set_window (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			g_value_set_string (
				value, e_shell_view_get_title (
				E_SHELL_VIEW (object)));
			return;

		case PROP_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_window (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_dispose (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	if (priv->window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->window), &priv->window);
		priv->window = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	g_free (priv->title);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_view_class_init (EShellViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_view_set_property;
	object_class->get_property = shell_view_get_property;
	object_class->dispose = shell_view_dispose;
	object_class->finalize = shell_view_finalize;

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			_("Title"),
			_("The title of the shell view"),
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_WINDOW,
		g_param_spec_object (
			"window",
			_("Window"),
			_("The window to which the shell view belongs"),
			GTK_TYPE_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_view_init (EShellView *shell_view)
{
	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);
}

GType
e_shell_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellView",
			&type_info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

const gchar *
e_shell_view_get_name (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	/* A shell view's name is taken from the name of the
	 * module that registered the shell view subclass. */

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->module != NULL, NULL);
	g_return_val_if_fail (class->module->name != NULL, NULL);

	return class->module->name;
}

const gchar *
e_shell_view_get_title (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->title;
}

void
e_shell_view_set_title (EShellView *shell_view,
                        const gchar *title)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

GtkWidget *
e_shell_view_get_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->window;
}

GtkWidget *
e_shell_view_get_content_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_content_widget != NULL, NULL);

	return class->get_content_widget (shell_view);
}

GtkWidget *
e_shell_view_get_sidebar_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_sidebar_widget != NULL, NULL);

	return class->get_sidebar_widget (shell_view);
}

GtkWidget *
e_shell_view_get_status_widget (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_CLASS (shell_view);
	g_return_val_if_fail (class->get_status_widget != NULL, NULL);

	return class->get_status_widget (shell_view);
}

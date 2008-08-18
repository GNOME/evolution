/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-window.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-window-private.h"

#include "e-util/e-plugin-ui.h"
#include "e-util/e-util-private.h"
#include "e-util/gconf-bridge.h"
#include "widgets/misc/e-online-button.h"

#include "e-sidebar.h"
#include "es-menu.h"
#include "es-event.h"

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <string.h>

enum {
	PROP_0,
	PROP_SAFE_MODE
};

static gpointer parent_class;

static void
shell_window_set_property (GObject *object,
                           guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SAFE_MODE:
			e_shell_window_set_safe_mode (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			break;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_window_get_property (GObject *object,
                           guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SAFE_MODE:
			g_value_set_boolean (
				value, e_shell_window_get_safe_mode (
				E_SHELL_WINDOW (object)));
			break;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_window_dispose (GObject *object)
{
	e_shell_window_private_dispose (E_SHELL_WINDOW (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_window_finalize (GObject *object)
{
	e_shell_window_private_finalize (E_SHELL_WINDOW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_window_class_init (EShellWindowClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_window_set_property;
	object_class->get_property = shell_window_get_property;
	object_class->dispose = shell_window_dispose;
	object_class->finalize = shell_window_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SAFE_MODE,
		g_param_spec_boolean (
			"safe-mode",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
shell_window_init (EShellWindow *window)
{
	GtkUIManager *manager;

	window->priv = E_SHELL_WINDOW_GET_PRIVATE (window);

	e_shell_window_private_init (window);

	manager = e_shell_window_get_ui_manager (window);

	e_plugin_ui_register_manager (
		"org.gnome.evolution.shell", manager, window);
}

GType
e_shell_window_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellWindowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_window_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellWindow),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_window_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_WINDOW, "EShellWindow", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_shell_window_new (gboolean safe_mode)
{
	return g_object_new (
		E_TYPE_SHELL_WINDOW, "safe-mode", safe_mode, NULL);
}

GtkUIManager *
e_shell_window_get_ui_manager (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);

	return window->priv->manager;
}

GtkAction *
e_shell_window_get_action (EShellWindow *window,
                           const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

GtkActionGroup *
e_shell_window_get_action_group (EShellWindow *window,
                                 const gchar *group_name)
{
	GtkUIManager *manager;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	iter = gtk_ui_manager_get_action_groups (manager);

	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;

		iter = g_list_next (iter);
	}

	g_return_val_if_reached (NULL);
}

GtkWidget *
e_shell_window_get_managed_widget (EShellWindow *window,
                                   const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = e_shell_window_get_ui_manager (window);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

gboolean
e_shell_window_get_safe_mode (EShellWindow *window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (window), FALSE);

	return window->priv->safe_mode;
}

void
e_shell_window_set_safe_mode (EShellWindow *window,
                              gboolean safe_mode)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (window));

	window->priv->safe_mode = safe_mode;

	g_object_notify (G_OBJECT (window), "safe-mode");
}

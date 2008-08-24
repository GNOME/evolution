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

#include "e-shell-window-private.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include <e-sidebar.h>
#include <es-event.h>
#include <es-menu.h>

#include <e-util/e-plugin-ui.h>
#include <e-util/e-util-private.h>
#include <e-util/gconf-bridge.h>
#include <widgets/misc/e-online-button.h>

enum {
	PROP_0,
	PROP_CURRENT_VIEW,
	PROP_SAFE_MODE,
	PROP_SHELL
};

static gpointer parent_class;

static void
shell_window_online_mode_notify_cb (EShell *shell,
                                    GParamSpec *pspec,
                                    EShellWindow *shell_window)
{
	GtkAction *action;
	EOnlineButton *online_button;
	gboolean online_mode;

	online_mode = e_shell_get_online_mode (shell);

	action = ACTION (WORK_OFFLINE);
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, online_mode);

	action = ACTION (WORK_ONLINE);
	gtk_action_set_sensitive (action, TRUE);
	gtk_action_set_visible (action, !online_mode);

	online_button = E_ONLINE_BUTTON (shell_window->priv->online_button);
	e_online_button_set_online (online_button, online_mode);
}

static void
shell_window_set_shell (EShellWindow *shell_window,
                        EShell *shell)
{
	g_return_if_fail (shell_window->priv->shell == NULL);
	shell_window->priv->shell = g_object_ref (shell);

	g_signal_connect (
		shell, "notify::online-mode",
		G_CALLBACK (shell_window_online_mode_notify_cb),
		shell_window);

	g_object_notify (G_OBJECT (shell), "online-mode");
}

static void
shell_window_set_property (GObject *object,
                           guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW:
			e_shell_window_set_current_view (
				E_SHELL_WINDOW (object),
				g_value_get_string (value));
			return;

		case PROP_SAFE_MODE:
			e_shell_window_set_safe_mode (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHELL:
			shell_window_set_shell (
				E_SHELL_WINDOW (object),
				g_value_get_object (value));
			return;
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
		case PROP_CURRENT_VIEW:
			g_value_set_string (
				value, e_shell_window_get_current_view (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_SAFE_MODE:
			g_value_set_boolean (
				value, e_shell_window_get_safe_mode (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_SHELL:
			g_value_set_object (
				value, e_shell_window_get_shell (
				E_SHELL_WINDOW (object)));
			return;
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
		PROP_CURRENT_VIEW,
		g_param_spec_string (
			"current-view",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

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

	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			NULL,
			NULL,
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_window_init (EShellWindow *shell_window)
{
	GtkUIManager *manager;

	shell_window->priv = E_SHELL_WINDOW_GET_PRIVATE (shell_window);

	e_shell_window_private_init (shell_window);

	manager = e_shell_window_get_ui_manager (shell_window);

	e_plugin_ui_register_manager (
		"org.gnome.evolution.shell", manager, shell_window);
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
e_shell_window_new (EShell *shell,
                    gboolean safe_mode)
{
	return g_object_new (
		E_TYPE_SHELL_WINDOW,
		"shell", shell, "safe-mode", safe_mode, NULL);
}

EShell *
e_shell_window_get_shell (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->shell;
}

GtkUIManager *
e_shell_window_get_ui_manager (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->manager;
}

GtkAction *
e_shell_window_get_action (EShellWindow *shell_window,
                           const gchar *action_name)
{
	GtkUIManager *manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (shell_window);
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
e_shell_window_get_action_group (EShellWindow *shell_window,
                                 const gchar *group_name)
{
	GtkUIManager *manager;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	manager = e_shell_window_get_ui_manager (shell_window);
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
e_shell_window_get_managed_widget (EShellWindow *shell_window,
                                   const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = e_shell_window_get_ui_manager (shell_window);
	widget = gtk_ui_manager_get_widget (manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

const gchar *
e_shell_window_get_current_view (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->current_view;
}

void
e_shell_window_set_current_view (EShellWindow *shell_window,
                                 const gchar *name_or_alias)
{
	const gchar *current_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (name_or_alias != NULL)
		current_view =
			e_shell_registry_get_canonical_name (name_or_alias);

	if (current_view == NULL)
		current_view = shell_window->priv->default_view;

	shell_window->priv->current_view = current_view;

	g_object_notify (G_OBJECT (shell_window), "current-view");
}

gboolean
e_shell_window_get_safe_mode (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->safe_mode;
}

void
e_shell_window_set_safe_mode (EShellWindow *shell_window,
                              gboolean safe_mode)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	shell_window->priv->safe_mode = safe_mode;

	g_object_notify (G_OBJECT (shell_window), "safe-mode");
}

void
e_shell_window_register_new_item_actions (EShellWindow *shell_window,
                                          const gchar *module_name,
                                          const GtkActionEntry *entries,
                                          guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = shell_window->priv->new_item_actions;
	manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (manager);
	module_name = g_intern_string (module_name);

	gtk_action_group_add_actions (
		action_group, entries, n_entries, shell_window);

	/* Tag each action with the name of the shell module that
	 * registered it.  This is used to help sort actions in the
	 * "New" menu. */

	for (ii = 0; ii < n_entries; ii++) {
		const gchar *action_name;
		GtkAction *action;

		action_name = entries[ii].name;

		action = gtk_action_group_get_action (
			action_group, action_name);

		gtk_action_set_accel_group (action, accel_group);

		g_object_set_data (
			G_OBJECT (action),
			"module-name", (gpointer) module_name);
	}

	/* Force a rebuild of the "New" menu. */
	g_object_notify (G_OBJECT (shell_window), "current-view");
}

void
e_shell_window_register_new_source_actions (EShellWindow *shell_window,
                                            const gchar *module_name,
                                            const GtkActionEntry *entries,
                                            guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = shell_window->priv->new_source_actions;
	manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (manager);
	module_name = g_intern_string (module_name);

	gtk_action_group_add_actions (
		action_group, entries, n_entries, shell_window);

	/* Tag each action with the name of the shell module that
	 * registered it.  This is used to help sort actions in the
	 * "New" menu. */

	for (ii = 0; ii < n_entries; ii++) {
		const gchar *action_name;
		GtkAction *action;

		action_name = entries[ii].name;

		action = gtk_action_group_get_action (
			action_group, action_name);

		gtk_action_set_accel_group (action, accel_group);

		g_object_set_data (
			G_OBJECT (action),
			"module-name", (gpointer) module_name);
	}

	/* Force a rebuild of the "New" menu. */
	g_object_notify (G_OBJECT (shell_window), "current-view");
}

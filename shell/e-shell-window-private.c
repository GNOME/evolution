/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 * e-shell-window-private.c
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

#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"

#ifdef NM_SUPPORT_GLIB
void e_shell_nm_glib_initialise (EShellWindow *window);
void e_shell_nm_glib_dispose (EShellWindow *window);
#elif NM_SUPPORT
void e_shell_dbus_initialise (EShellWindow *window);
void e_shell_dbus_dispose (EShellWindow *window);
#endif

static void
shell_window_menu_item_select_cb (EShellWindow *window,
                                  GtkWidget *menu_item)
{
	GtkAction *action;
	GtkLabel *label;
	gchar *tooltip;

	action = g_object_get_data (G_OBJECT (menu_item), "action");
	g_return_if_fail (GTK_IS_ACTION (action));

	g_object_get (action, "tooltip", &tooltip, NULL);

	if (tooltip == NULL)
		return;

	label = GTK_LABEL (window->priv->tooltip_label);
	gtk_label_set_text (label, tooltip);
	g_free (tooltip);

	gtk_widget_show (window->priv->tooltip_label);
	gtk_widget_hide (window->priv->status_notebook);
}

static void
shell_window_menu_item_deselect_cb (EShellWindow *window)
{
	gtk_widget_hide (window->priv->tooltip_label);
	gtk_widget_show (window->priv->status_notebook);
}

static void
shell_window_connect_proxy_cb (EShellWindow *window,
                               GtkAction *action,
                               GtkWidget *proxy)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_object_set_data_full (
		G_OBJECT (proxy),
		"action", g_object_ref (action),
		(GDestroyNotify) g_object_unref);

	g_signal_connect_swapped (
		proxy, "select",
		G_CALLBACK (shell_window_menu_item_select_cb), window);

	g_signal_connect_swapped (
		proxy, "deselect",
		G_CALLBACK (shell_window_menu_item_deselect_cb), window);
}

static void
shell_window_online_button_clicked_cb (EOnlineButton *button,
                                       EShellWindow *window)
{
	if (e_online_button_get_online (button))
		gtk_action_activate (ACTION (WORK_OFFLINE));
	else
		gtk_action_activate (ACTION (WORK_ONLINE));
}

void
e_shell_window_private_init (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *object;
	const gchar *key;
	gint height;

	priv->manager = gtk_ui_manager_new ();
	priv->shell_actions = gtk_action_group_new ("shell");
	priv->new_item_actions = gtk_action_group_new ("new-item");
	priv->new_group_actions = gtk_action_group_new ("new-group");
	priv->new_source_actions = gtk_action_group_new ("new-source");
	priv->shell_view_actions = gtk_action_group_new ("shell-view");

	priv->shell_views = g_ptr_array_new ();

	e_load_ui_definition (priv->manager, "evolution-shell.ui");

	e_shell_window_actions_init (window);

	gtk_window_add_accel_group (
		GTK_WINDOW (window),
		gtk_ui_manager_get_accel_group (priv->manager));

	g_signal_connect_swapped (
		priv->manager, "connect-proxy",
		G_CALLBACK (shell_window_connect_proxy_cb), window);

	/* Construct window widgets. */

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_shell_window_get_managed_widget (window, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_shell_window_get_managed_widget (window, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->content_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->status_area = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Make the status area as large as the task bar. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (widget, -1, height * 2);

	container = priv->content_pane;

	widget = e_sidebar_new ();
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->sidebar = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->content_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->sidebar;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->sidebar_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->status_area;

	widget = e_online_button_new ();
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (shell_window_online_button_clicked_cb), window);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	priv->online_button = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->tooltip_label = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->status_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	key = "/apps/evolution/shell/view_defaults/window";
	gconf_bridge_bind_window (
		bridge, key, GTK_WINDOW (window), TRUE, FALSE);

	object = G_OBJECT (priv->content_pane);
	key = "/apps/evolution/shell/view_defaults/folder_bar/width";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");

	object = G_OBJECT (ACTION (SHOW_SIDEBAR));
	key = "/apps/evolution/shell/view_defaults/sidebar_visible";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (SHOW_STATUSBAR));
	key = "/apps/evolution/shell/view_defaults/statusbar_visible";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (SHOW_SWITCHER));
	key = "/apps/evolution/shell/view_defaults/buttons_visible";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (SHOW_TOOLBAR));
	key = "/apps/evolution/shell/view_defaults/toolbar_visible";
	gconf_bridge_bind_property (bridge, key, object, "active");

	/* NetworkManager integration. */

#ifdef NM_SUPPORT_GLIB
	e_shell_nm_glib_initialise (window);
#elif NM_SUPPORT
	e_shell_dbus_initialise (window);
#endif

	/* Initialize shell views */

	e_shell_window_create_shell_view_actions (window);
}

void
e_shell_window_private_dispose (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;

	DISPOSE (priv->manager);
	DISPOSE (priv->shell_actions);
	DISPOSE (priv->new_item_actions);
	DISPOSE (priv->new_group_actions);
	DISPOSE (priv->new_source_actions);
	DISPOSE (priv->shell_view_actions);

	DISPOSE (priv->main_menu);
	DISPOSE (priv->main_toolbar);
	DISPOSE (priv->content_pane);
	DISPOSE (priv->content_notebook);
	DISPOSE (priv->sidebar);
	DISPOSE (priv->sidebar_notebook);
	DISPOSE (priv->status_area);
	DISPOSE (priv->online_button);
	DISPOSE (priv->tooltip_label);
	DISPOSE (priv->status_notebook);

#ifdef NM_SUPPORT_GLIB
	e_shell_nm_glib_dispose (E_SHELL_WINDOW (object));
#elif NM_SUPPORT
	e_shell_dbus_dispose (E_SHELL_WINDOW (object));
#endif

	priv->destroyed = TRUE;
}

void
e_shell_window_private_finalize (EShellWindow *window)
{
	EShellWindowPrivate *priv = window->priv;

	g_ptr_array_foreach (priv->shell_views, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (priv->shell_views, TRUE);
}

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

#include <string.h>
#include <e-util/e-util.h>
#include <e-util/gconf-bridge.h>

static void
shell_window_notify_current_view_cb (EShellWindow *shell_window)
{
	GtkWidget *menu;
	GtkWidget *widget;
	const gchar *path;

	/* Update the "File -> New" submenu. */
	path = "/main-menu/file-menu/new-menu";
	menu = e_shell_window_create_new_menu (shell_window);
	widget = e_shell_window_get_managed_widget (shell_window, path);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), menu);
	gtk_widget_show (widget);

	/* Update the "New" menu tool button submenu. */
	menu = e_shell_window_create_new_menu (shell_window);
	widget = shell_window->priv->menu_tool_button;
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (widget), menu);
}

static void
shell_window_save_switcher_style_cb (GtkRadioAction *action,
                                     GtkRadioAction *current,
                                     EShellWindow *shell_window)
{
	GConfClient *client;
	GtkToolbarStyle style;
	const gchar *key;
	const gchar *string;
	GError *error = NULL;

	client = gconf_client_get_default ();
	style = gtk_radio_action_get_current_value (action);
	key = "/apps/evolution/shell/view_defaults/buttons_style";

	switch (style) {
		case GTK_TOOLBAR_ICONS:
			string = "icons";
			break;

		case GTK_TOOLBAR_TEXT:
			string = "text";
			break;

		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			string = "both";
			break;

		default:
			string = "toolbar";
			break;
	}

	if (!gconf_client_set_string (client, key, string, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (client);
}

static void
shell_window_init_switcher_style (EShellWindow *shell_window)
{
	GtkAction *action;
	GConfClient *client;
	GtkToolbarStyle style;
	const gchar *key;
	gchar *string;
	GError *error = NULL;

	/* XXX GConfBridge doesn't let you convert between numeric properties
	 *     and string keys, so we have to create the binding manually. */

	client = gconf_client_get_default ();
	action = ACTION (SWITCHER_STYLE_ICONS);
	key = "/apps/evolution/shell/view_defaults/buttons_style";
	string = gconf_client_get_string (client, key, &error);

	if (string != NULL) {
		if (strcmp (string, "icons") == 0)
			style = GTK_TOOLBAR_ICONS;
		else if (strcmp (string, "text") == 0)
			style = GTK_TOOLBAR_TEXT;
		else if (strcmp (string, "both") == 0)
			style = GTK_TOOLBAR_BOTH_HORIZ;
		else
			style = -1;

		gtk_radio_action_set_current_value (
			GTK_RADIO_ACTION (action), style);
	}

	g_signal_connect (
		action, "changed",
		G_CALLBACK (shell_window_save_switcher_style_cb),
		shell_window);

	g_object_unref (client);
}

static void
shell_window_menu_item_select_cb (EShellWindow *shell_window,
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

	label = GTK_LABEL (shell_window->priv->tooltip_label);
	gtk_label_set_text (label, tooltip);
	g_free (tooltip);

	gtk_widget_show (shell_window->priv->tooltip_label);
	gtk_widget_hide (shell_window->priv->status_notebook);
}

static void
shell_window_menu_item_deselect_cb (EShellWindow *shell_window)
{
	gtk_widget_hide (shell_window->priv->tooltip_label);
	gtk_widget_show (shell_window->priv->status_notebook);
}

static void
shell_window_connect_proxy_cb (EShellWindow *shell_window,
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
		G_CALLBACK (shell_window_menu_item_select_cb),
		shell_window);

	g_signal_connect_swapped (
		proxy, "deselect",
		G_CALLBACK (shell_window_menu_item_deselect_cb),
		shell_window);
}

static void
shell_window_online_button_clicked_cb (EOnlineButton *button,
                                       EShellWindow *shell_window)
{
	if (e_online_button_get_online (button))
		gtk_action_activate (ACTION (WORK_OFFLINE));
	else
		gtk_action_activate (ACTION (WORK_ONLINE));
}

void
e_shell_window_private_init (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;
	GHashTable *loaded_views;
	GConfBridge *bridge;
	GtkToolItem *item;
	GtkWidget *container;
	GtkWidget *widget;
	GObject *object;
	const gchar *key;
	guint merge_id;
	gint height;

	loaded_views = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	priv->ui_manager = gtk_ui_manager_new ();
	priv->shell_actions = gtk_action_group_new ("shell");
	priv->gal_view_actions = gtk_action_group_new ("gal-view");
	priv->new_item_actions = gtk_action_group_new ("new-item");
	priv->new_source_actions = gtk_action_group_new ("new-source");
	priv->shell_view_actions = gtk_action_group_new ("shell-view");
	priv->loaded_views = loaded_views;

	merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);
	priv->gal_view_merge_id = merge_id;

	e_shell_window_actions_init (shell_window);

	gtk_window_add_accel_group (
		GTK_WINDOW (shell_window),
		gtk_ui_manager_get_accel_group (priv->ui_manager));

	g_signal_connect_swapped (
		priv->ui_manager, "connect-proxy",
		G_CALLBACK (shell_window_connect_proxy_cb), shell_window);

	/* Construct window widgets. */

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (shell_window), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_shell_window_get_managed_widget (
		shell_window, "/main-menu");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_menu = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_shell_window_get_managed_widget (
		shell_window, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->main_toolbar = g_object_ref (widget);
	gtk_widget_show (widget);

	/* XXX Having this separator in the UI definition doesn't work
	 *     because GtkUIManager is unaware of the "New" button, so
	 *     it makes the separator invisible.  One possibility is to
	 *     define a GtkAction subclass for which create_tool_item()
	 *     returns an EMenuToolButton.  Then both this separator
	 *     and the "New" button could be added to the UI definition.
	 *     Tempting, but the "New" button and its dynamically
	 *     generated menu is already a complex beast, and I'm not
	 *     convinced having it proxy some new type of GtkAction
	 *     is worth the extra effort. */
	item = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (GTK_TOOLBAR (widget), item, 0);
	gtk_widget_show (GTK_WIDGET (item));

	item = e_menu_tool_button_new (_("New"));
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (widget), item, 0);
	priv->menu_tool_button = g_object_ref (item);
	gtk_widget_show (GTK_WIDGET (item));

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

	widget = gtk_vbox_new (FALSE, 6);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	gtk_widget_show (widget);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->content_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	container = gtk_paned_get_child1 (GTK_PANED (priv->content_pane));

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->sidebar_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_shell_switcher_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->switcher = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->status_area;

	widget = e_online_button_new ();
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (shell_window_online_button_clicked_cb),
		shell_window);
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
		bridge, key, GTK_WINDOW (shell_window), TRUE, FALSE);

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

	shell_window_init_switcher_style (shell_window);

	/* Fine tuning. */

	g_object_set (ACTION (SEND_RECEIVE), "is-important", TRUE, NULL);

	g_signal_connect (
		shell_window, "notify::current-view",
		G_CALLBACK (shell_window_notify_current_view_cb), NULL);

	/* Initialize shell views. */

	e_shell_window_create_shell_view_actions (shell_window);
}

void
e_shell_window_private_dispose (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	DISPOSE (priv->shell);

	DISPOSE (priv->ui_manager);
	DISPOSE (priv->shell_actions);
	DISPOSE (priv->gal_view_actions);
	DISPOSE (priv->new_item_actions);
	DISPOSE (priv->new_source_actions);
	DISPOSE (priv->shell_view_actions);

	g_hash_table_remove_all (priv->loaded_views);

	DISPOSE (priv->main_menu);
	DISPOSE (priv->main_toolbar);
	DISPOSE (priv->menu_tool_button);
	DISPOSE (priv->content_pane);
	DISPOSE (priv->content_notebook);
	DISPOSE (priv->sidebar_notebook);
	DISPOSE (priv->switcher);
	DISPOSE (priv->status_area);
	DISPOSE (priv->online_button);
	DISPOSE (priv->tooltip_label);
	DISPOSE (priv->status_notebook);

	priv->destroyed = TRUE;
}

void
e_shell_window_private_finalize (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	g_hash_table_destroy (priv->loaded_views);
}

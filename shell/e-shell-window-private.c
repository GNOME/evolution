/*
 * e-shell-window-private.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-shell-window-private.h"

static void
shell_window_save_switcher_style_cb (GtkRadioAction *action,
                                     GtkRadioAction *current,
                                     EShellWindow *shell_window)
{
	EShell *shell;
	GConfClient *client;
	GtkToolbarStyle style;
	const gchar *key;
	const gchar *string;
	GError *error = NULL;

	shell = e_shell_window_get_shell (shell_window);
	client = e_shell_get_gconf_client (shell);

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
}

static void
shell_window_init_switcher_style (EShellWindow *shell_window)
{
	EShell *shell;
	GtkAction *action;
	GConfClient *client;
	GtkToolbarStyle style;
	const gchar *key;
	gchar *string;
	GError *error = NULL;

	/* XXX GConfBridge doesn't let you convert between numeric properties
	 *     and string keys, so we have to create the binding manually. */

	shell = e_shell_window_get_shell (shell_window);
	client = e_shell_get_gconf_client (shell);

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
}

static void
shell_window_menu_item_select_cb (EShellWindow *shell_window,
                                  GtkWidget *widget)
{
	GtkAction *action;
	GtkActivatable *activatable;
	GtkLabel *label;
	const gchar *tooltip;

	activatable = GTK_ACTIVATABLE (widget);
	action = gtk_activatable_get_related_action (activatable);
	tooltip = gtk_action_get_tooltip (action);

	if (tooltip == NULL)
		return;

	label = GTK_LABEL (shell_window->priv->tooltip_label);
	gtk_label_set_text (label, tooltip);

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
	GArray *signal_handler_ids;
	GtkAccelGroup *accel_group;
	GtkToolItem *item;
	GtkWidget *container;
	GtkWidget *widget;
	guint merge_id;
	gint height;

	loaded_views = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	signal_handler_ids = g_array_new (FALSE, FALSE, sizeof (gulong));

	priv->ui_manager = gtk_ui_manager_new ();
	priv->loaded_views = loaded_views;
	priv->active_view = "unknown";
	priv->signal_handler_ids = signal_handler_ids;

	e_shell_window_add_action_group (shell_window, "shell");
	e_shell_window_add_action_group (shell_window, "gal-view");
	e_shell_window_add_action_group (shell_window, "new-item");
	e_shell_window_add_action_group (shell_window, "new-source");
	e_shell_window_add_action_group (shell_window, "custom-rules");
	e_shell_window_add_action_group (shell_window, "switcher");
	e_shell_window_add_action_group (shell_window, "lockdown-application-handlers");
	e_shell_window_add_action_group (shell_window, "lockdown-printing");
	e_shell_window_add_action_group (shell_window, "lockdown-print-setup");
	e_shell_window_add_action_group (shell_window, "lockdown-save-to-disk");

	merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);
	priv->custom_rule_merge_id = merge_id;

	merge_id = gtk_ui_manager_new_merge_id (priv->ui_manager);
	priv->gal_view_merge_id = merge_id;

	gtk_window_set_title (GTK_WINDOW (shell_window), _("Evolution"));

	e_shell_window_actions_init (shell_window);

	accel_group = gtk_ui_manager_get_accel_group (priv->ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (shell_window), accel_group);

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
	gtk_widget_add_accelerator (
		GTK_WIDGET (item), "clicked",
		gtk_ui_manager_get_accel_group (priv->ui_manager),
		GDK_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_toolbar_insert (GTK_TOOLBAR (widget), item, 0);
	priv->menu_tool_button = g_object_ref (item);
	gtk_widget_show (GTK_WIDGET (item));

	widget = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->content_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_hbox_new (FALSE, 3);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 3);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	priv->status_area = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Make the status area as large as the task bar. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (widget, -1, (height * 2) + 6);

	container = priv->content_pane;

	widget = e_shell_switcher_new ();
	gtk_paned_pack1 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->switcher = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, FALSE);
	priv->content_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->switcher;

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
}

void
e_shell_window_private_constructed (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;
	EShellSettings *shell_settings;
	EShell *shell;
	GConfBridge *bridge;
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkWindow *window;
	GObject *object;
	const gchar *key;
	const gchar *id;

	window = GTK_WINDOW (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_watch_window (shell, window);

	/* Create the switcher actions before we set the initial
	 * shell view, because the shell view relies on them for
	 * default settings during construction. */
	e_shell_window_create_switcher_actions (shell_window);

	/* Support lockdown. */

	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);

	e_binding_new_with_negation (
		shell_settings, "disable-printing",
		action_group, "visible");

	action_group = ACTION_GROUP (LOCKDOWN_PRINT_SETUP);

	e_binding_new_with_negation (
		shell_settings, "disable-print-setup",
		action_group, "visible");

	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);

	e_binding_new_with_negation (
		shell_settings, "disable-save-to-disk",
		action_group, "visible");

	/* Bind GObject properties to GObject properties. */

	action = ACTION (SEND_RECEIVE);

	e_binding_new (
		shell, "online",
		action, "sensitive");

	action = ACTION (WORK_OFFLINE);

	e_binding_new (
		shell, "online",
		action, "visible");

	e_binding_new (
		shell, "network-available",
		action, "sensitive");

	action = ACTION (WORK_ONLINE);

	e_binding_new_with_negation (
		shell, "online",
		action, "visible");

	e_binding_new (
		shell, "network-available",
		action, "sensitive");

	e_binding_new (
		shell, "online",
		priv->online_button, "online");

	e_binding_new (
		shell, "network-available",
		priv->online_button, "sensitive");

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (shell_window);
	key = "/apps/evolution/shell/view_defaults/component_id";
	gconf_bridge_bind_property (bridge, key, object, "active-view");

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

	/* Configure the initial size and position of the window by way
	 * of either a user-supplied geometry string or the last recorded
	 * values.  Note that if a geometry string is applied, the window
	 * size and position are -not- recorded. */
	if (priv->geometry != NULL) {
		if (!gtk_window_parse_geometry (window, priv->geometry))
			g_printerr (
				"Failed to parse geometry '%s'\n",
				priv->geometry);
		g_free (priv->geometry);
		priv->geometry = NULL;
	} else {
		key = "/apps/evolution/shell/view_defaults/window";
		gconf_bridge_bind_window (bridge, key, window, TRUE, TRUE);
	}

	shell_window_init_switcher_style (shell_window);

	id = "org.gnome.evolution.shell";
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	e_plugin_ui_register_manager (ui_manager, id, shell_window);
	e_plugin_ui_enable_manager (ui_manager, id);
}

void
e_shell_window_private_dispose (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	/* Need to disconnect handlers before we unref the shell. */
	if (priv->signal_handler_ids != NULL) {
		GArray *array = priv->signal_handler_ids;
		gulong handler_id;
		guint ii;

		for (ii = 0; ii < array->len; ii++) {
			handler_id = g_array_index (array, gulong, ii);
			g_signal_handler_disconnect (priv->shell, handler_id);
		}

		g_array_free (array, TRUE);
		priv->signal_handler_ids = NULL;
	}

	if (priv->shell != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell), &priv->shell);
		priv->shell = NULL;
	}

	DISPOSE (priv->ui_manager);

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

	g_free (priv->geometry);
}

void
e_shell_window_switch_to_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	GtkNotebook *notebook;
	EShellView *shell_view;
	gint page_num;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	page_num = e_shell_view_get_page_num (shell_view);
	g_return_if_fail (page_num >= 0);

	notebook = GTK_NOTEBOOK (shell_window->priv->content_notebook);
	gtk_notebook_set_current_page (notebook, page_num);

	notebook = GTK_NOTEBOOK (shell_window->priv->sidebar_notebook);
	gtk_notebook_set_current_page (notebook, page_num);

	notebook = GTK_NOTEBOOK (shell_window->priv->status_notebook);
	gtk_notebook_set_current_page (notebook, page_num);

	shell_window->priv->active_view = view_name;
	g_object_notify (G_OBJECT (shell_window), "active-view");

	e_shell_window_update_icon (shell_window);
	e_shell_window_update_title (shell_window);
	e_shell_window_update_new_menu (shell_window);
	e_shell_window_update_view_menu (shell_window);
	e_shell_window_update_search_menu (shell_window);

	e_shell_view_update_actions (shell_view);
}

void
e_shell_window_update_icon (EShellWindow *shell_window)
{
	EShellView *shell_view;
	GtkAction *action;
	const gchar *view_name;
	gchar *icon_name = NULL;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	action = e_shell_view_get_action (shell_view);
	g_object_get (action, "icon-name", &icon_name, NULL);
	gtk_window_set_icon_name (GTK_WINDOW (shell_window), icon_name);
	g_free (icon_name);
}

void
e_shell_window_update_title (EShellWindow *shell_window)
{
	EShellView *shell_view;
	const gchar *view_title;
	const gchar *view_name;
	gchar *window_title;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_title = e_shell_view_get_title (shell_view);

	/* Translators: This is used for the main window title. */
	window_title = g_strdup_printf (_("%s - Evolution"), view_title);
	gtk_window_set_title (GTK_WINDOW (shell_window), window_title);
	g_free (window_title);
}

void
e_shell_window_update_new_menu (EShellWindow *shell_window)
{
	GtkWidget *menu;
	GtkWidget *widget;
	const gchar *path;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

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

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

		g_free (string);
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

static GtkWidget *
shell_window_construct_menubar (EShellWindow *shell_window)
{
	EShellWindowClass *class;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	if (class->construct_menubar == NULL)
		return NULL;

	return class->construct_menubar (shell_window);
}

static GtkWidget *
shell_window_construct_toolbar (EShellWindow *shell_window)
{
	EShellWindowClass *class;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	if (class->construct_toolbar == NULL)
		return NULL;

	return class->construct_toolbar (shell_window);
}

static GtkWidget *
shell_window_construct_sidebar (EShellWindow *shell_window)
{
	EShellWindowClass *class;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	if (class->construct_sidebar == NULL)
		return NULL;

	return class->construct_sidebar (shell_window);
}

static GtkWidget *
shell_window_construct_content (EShellWindow *shell_window)
{
	EShellWindowClass *class;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	if (class->construct_content == NULL)
		return NULL;

	return class->construct_content (shell_window);
}

static GtkWidget *
shell_window_construct_taskbar (EShellWindow *shell_window)
{
	EShellWindowClass *class;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	if (class->construct_taskbar == NULL)
		return NULL;

	return class->construct_taskbar (shell_window);
}

void
e_shell_window_private_init (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;
	GHashTable *loaded_views;
	GArray *signal_handler_ids;

	loaded_views = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	signal_handler_ids = g_array_new (FALSE, FALSE, sizeof (gulong));

	priv->ui_manager = e_ui_manager_new ();
	priv->loaded_views = loaded_views;
	priv->signal_handler_ids = signal_handler_ids;

	/* XXX This kind of violates the shell window being unaware
	 *     of specific shell views, but we need a sane fallback. */
	priv->active_view = "mail";

	e_shell_window_add_action_group (shell_window, "shell");
	e_shell_window_add_action_group (shell_window, "gal-view");
	e_shell_window_add_action_group (shell_window, "new-item");
	e_shell_window_add_action_group (shell_window, "new-source");
	e_shell_window_add_action_group (shell_window, "custom-rules");
	e_shell_window_add_action_group (shell_window, "switcher");
	e_shell_window_add_action_group (shell_window, "new-window");
	e_shell_window_add_action_group (shell_window, "lockdown-application-handlers");
	e_shell_window_add_action_group (shell_window, "lockdown-printing");
	e_shell_window_add_action_group (shell_window, "lockdown-print-setup");
	e_shell_window_add_action_group (shell_window, "lockdown-save-to-disk");

	gtk_window_set_title (GTK_WINDOW (shell_window), _("Evolution"));

	g_signal_connect_swapped (
		priv->ui_manager, "connect-proxy",
		G_CALLBACK (shell_window_connect_proxy_cb), shell_window);
}

void
e_shell_window_private_constructed (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;
	EShellSettings *shell_settings;
	EShell *shell;
	GConfBridge *bridge;
	GtkAction *action;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkBox *box;
	GtkPaned *paned;
	GtkWidget *widget;
	GtkWindow *window;
	GObject *object;
	guint merge_id;
	const gchar *key;
	const gchar *id;

	window = GTK_WINDOW (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	e_shell_configure_ui_manager (shell, E_UI_MANAGER (ui_manager));

	/* Defer actions and menu merging until we have set express mode */

	e_shell_window_actions_init (shell_window);

	/* Do this after intializing actions because it
	 * triggers shell_window_update_close_action_cb(). */
	e_shell_watch_window (shell, window);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (shell_window), accel_group);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->custom_rule_merge_id = merge_id;

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->gal_view_merge_id = merge_id;

	/* Construct window widgets. */

	widget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (shell_window), widget);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	widget = shell_window_construct_menubar (shell_window);
	if (widget != NULL)
		gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = shell_window_construct_toolbar (shell_window);
	if (widget != NULL)
		gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_hpaned_new ();
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	priv->content_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = shell_window_construct_taskbar (shell_window);
	if (widget != NULL)
		gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	paned = GTK_PANED (priv->content_pane);

	widget = shell_window_construct_sidebar (shell_window);
	if (widget != NULL)
		gtk_paned_pack1 (paned, widget, FALSE, FALSE);

	widget = shell_window_construct_content (shell_window);
	if (widget != NULL)
		gtk_paned_pack2 (paned, widget, TRUE, FALSE);

	/* Create the switcher actions before we set the initial
	 * shell view, because the shell view relies on them for
	 * default settings during construction. */
	e_shell_window_create_switcher_actions (shell_window);

	/* Bunch of chores to do when the active view changes. */

	g_signal_connect (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_icon), NULL);

	g_signal_connect (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_title), NULL);

	g_signal_connect (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_view_menu), NULL);

	g_signal_connect (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_search_menu), NULL);

#ifndef G_OS_WIN32
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
#endif

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

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (shell_window);
	key = "/apps/evolution/shell/view_defaults/component_id";
	gconf_bridge_bind_property (bridge, key, object, "active-view");

	object = G_OBJECT (priv->content_pane);
	key = "/apps/evolution/shell/view_defaults/folder_bar/width";
	gconf_bridge_bind_property_delayed (bridge, key, object, "position");

	object = G_OBJECT (shell_window);
	key = "/apps/evolution/shell/view_defaults/sidebar_visible";
	gconf_bridge_bind_property (bridge, key, object, "sidebar-visible");

	if (e_shell_get_express_mode (shell)) {
		const gchar *active_view;
		gboolean taskbar_visible;

		active_view = e_shell_window_get_active_view (shell_window);
		taskbar_visible = (g_strcmp0 (active_view, "mail") == 0);
		e_shell_window_set_switcher_visible (shell_window, FALSE);
		e_shell_window_set_taskbar_visible (shell_window, taskbar_visible);
	} else {
		object = G_OBJECT (shell_window);
		key = "/apps/evolution/shell/view_defaults/statusbar_visible";
		gconf_bridge_bind_property (bridge, key, object, "taskbar-visible");

		object = G_OBJECT (shell_window);
		key = "/apps/evolution/shell/view_defaults/buttons_visible";
		gconf_bridge_bind_property (bridge, key, object, "switcher-visible");
	}

	object = G_OBJECT (shell_window);
	key = "/apps/evolution/shell/view_defaults/toolbar_visible";
	gconf_bridge_bind_property (bridge, key, object, "toolbar-visible");

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

	DISPOSE (priv->focus_tracker);
	DISPOSE (priv->ui_manager);

	g_hash_table_remove_all (priv->loaded_views);

	DISPOSE (priv->content_pane);
	DISPOSE (priv->content_notebook);
	DISPOSE (priv->sidebar_notebook);
	DISPOSE (priv->switcher);
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
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	shell_window->priv->active_view = view_name;
	g_object_notify (G_OBJECT (shell_window), "active-view");

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

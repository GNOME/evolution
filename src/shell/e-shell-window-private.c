/*
 * e-shell-window-private.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-shell-window-private.h"

static void
shell_window_save_switcher_style_cb (GtkRadioAction *action,
                                     GtkRadioAction *current,
                                     EShellWindow *shell_window)
{
	GSettings *settings;
	GtkToolbarStyle style;
	const gchar *string;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	style = gtk_radio_action_get_current_value (action);

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

	g_settings_set_string (settings, "buttons-style", string);
	g_object_unref (settings);
}

static void
shell_window_init_switcher_style (EShellWindow *shell_window)
{
	GtkAction *action;
	GSettings *settings;
	GtkToolbarStyle style;
	gchar *string;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	action = ACTION (SWITCHER_STYLE_ICONS);
	string = g_settings_get_string (settings, "buttons-style");
	g_object_unref (settings);

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

static gboolean
shell_window_active_view_to_prefer_item (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	GObject *source_object;
	EShell *shell;
	EShellBackend *shell_backend;
	const gchar *active_view;
	const gchar *prefer_item;

	active_view = g_value_get_string (source_value);

	source_object = g_binding_get_source (binding);
	shell = e_shell_window_get_shell (E_SHELL_WINDOW (source_object));
	shell_backend = e_shell_get_backend_by_name (shell, active_view);
	prefer_item = e_shell_backend_get_prefer_new_item (shell_backend);

	g_value_set_string (target_value, prefer_item);

	return TRUE;
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

	priv->ui_manager = gtk_ui_manager_new ();
	priv->loaded_views = loaded_views;
	priv->signal_handler_ids = signal_handler_ids;
	priv->action_groups_by_view = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

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

static gboolean
e_shell_window_key_press_event_cb (GtkWidget *widget,
				   GdkEventKey *event)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (widget), FALSE);

	if ((event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0 ||
	    event->keyval == GDK_KEY_Tab ||
	    event->keyval == GDK_KEY_Return ||
	    event->keyval == GDK_KEY_Escape ||
	    event->keyval == GDK_KEY_KP_Tab ||
	    event->keyval == GDK_KEY_KP_Enter)
		return FALSE;

	if (e_shell_window_get_need_input (E_SHELL_WINDOW (widget), event)) {
		GtkWidget *focused;

		focused = gtk_window_get_focus (GTK_WINDOW (widget));
		if (focused)
			gtk_widget_event (focused, (GdkEvent *) event);

		return TRUE;
	}

	return FALSE;
}

static gboolean
shell_window_check_is_main_instance (GtkApplication *application,
				     GtkWindow *window)
{
	GList *windows, *link;

	g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
	g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

	windows = gtk_application_get_windows (application);
	for (link = windows; link; link = g_list_next (link)) {
		GtkWindow *wnd = link->data;

		if (E_IS_SHELL_WINDOW (wnd) && wnd != window)
			return FALSE;
	}

	return TRUE;
}

void
e_shell_window_private_constructed (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;
	EShell *shell;
	GtkAction *action;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	GtkBox *box;
	GtkPaned *paned;
	GtkWidget *widget, *menubar, *menu_button = NULL;
	GtkWindow *window;
	guint merge_id;
	const gchar *id;
	GSettings *settings;

#ifndef G_OS_WIN32
	GtkActionGroup *action_group;
#endif

	window = GTK_WINDOW (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_window->priv->is_main_instance = shell_window_check_is_main_instance (GTK_APPLICATION (shell), window);

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	/* Defer actions and menu merging until we have set express mode */

	e_shell_window_actions_init (shell_window);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (shell_window), accel_group);

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->custom_rule_merge_id = merge_id;

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->gal_view_merge_id = merge_id;

	/* Construct window widgets. */

	menubar = shell_window_construct_menubar (shell_window);
	if (menubar)
		shell_window->priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (menubar), window, &menu_button);

	if (e_util_get_use_header_bar ()) {
		priv->headerbar = e_shell_header_bar_new (shell_window, menu_button);
		gtk_window_set_titlebar (window, priv->headerbar);
		gtk_widget_show (priv->headerbar);
	} else if (menu_button) {
		g_object_ref_sink (menu_button);
		gtk_widget_destroy (menu_button);
	}

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (shell_window), widget);
	gtk_widget_show (widget);

	box = GTK_BOX (widget);

	if (menubar)
		gtk_box_pack_start (box, menubar, FALSE, FALSE, 0);

	widget = shell_window_construct_toolbar (shell_window);
	if (widget != NULL)
		gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	widget = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
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

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_icon), NULL);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_title), NULL);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_view_menu), NULL);

#ifndef G_OS_WIN32
	/* Support lockdown. */

	settings = e_util_ref_settings ("org.gnome.desktop.lockdown");

	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);

	g_settings_bind (
		settings, "disable-printing",
		action_group, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action_group = ACTION_GROUP (LOCKDOWN_PRINT_SETUP);

	g_settings_bind (
		settings, "disable-print-setup",
		action_group, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);

	g_settings_bind (
		settings, "disable-save-to-disk",
		action_group, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_object_unref (settings);
#endif /* G_OS_WIN32 */

	/* Bind GObject properties to GObject properties. */

	action = ACTION (WORK_OFFLINE);

	e_binding_bind_property (
		shell, "online",
		action, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell, "network-available",
		action, "sensitive",
		G_BINDING_SYNC_CREATE);

	action = ACTION (WORK_ONLINE);

	e_binding_bind_property (
		shell, "online",
		action, "visible",
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		shell, "network-available",
		action, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Bind GObject properties to GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	/* Use G_SETTINGS_BIND_GET_NO_CHANGES so shell windows
	 * are initialized to the most recently used shell view,
	 * but still allows different windows to show different
	 * views at once. */
	g_settings_bind (
		settings, "default-component-id",
		shell_window, "active-view",
		G_SETTINGS_BIND_DEFAULT |
		G_SETTINGS_BIND_GET_NO_CHANGES);

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "folder-bar-width",
			priv->content_pane, "position",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "menubar-visible",
			shell_window, "menubar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "sidebar-visible",
			shell_window, "sidebar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "statusbar-visible",
			shell_window, "taskbar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "buttons-visible",
			shell_window, "switcher-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "toolbar-visible",
			shell_window, "toolbar-visible",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "menubar-visible-sub",
			shell_window, "menubar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "folder-bar-width-sub",
			priv->content_pane, "position",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "sidebar-visible-sub",
			shell_window, "sidebar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "statusbar-visible-sub",
			shell_window, "taskbar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "buttons-visible-sub",
			shell_window, "switcher-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "toolbar-visible-sub",
			shell_window, "toolbar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);
	}

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
		gtk_window_set_default_size (window, 640, 480);
		e_restore_window (
			window, "/org/gnome/evolution/shell/window/",
			E_RESTORE_WINDOW_SIZE | E_RESTORE_WINDOW_POSITION);
	}

	shell_window_init_switcher_style (shell_window);

	id = "org.gnome.evolution.shell";
	e_plugin_ui_register_manager (ui_manager, id, shell_window);
	e_plugin_ui_enable_manager (ui_manager, id);

	gtk_application_add_window (GTK_APPLICATION (shell), window);

	g_object_unref (settings);

	g_signal_connect (shell_window, "key-press-event",
		G_CALLBACK (e_shell_window_key_press_event_cb), NULL);

	if (e_util_get_use_header_bar ()) {
		/* XXX The ECalShellBackend has a hack where it forces the
		 *     EMenuButton to update its button image by forcing
		 *     a "notify::active-view" signal emission on the window.
		 *     This will trigger the property binding, which will set
		 *     EMenuButton's "prefer-item" property, which will
		 *     invoke header_bar_update_new_menu(), which
		 *     will cause EMenuButton to update its button image.
		 *
		 *     It's a bit of a Rube Goldberg machine and should be
		 *     reworked, but it's just serving one (now documented)
		 *     corner case and works for now. */
		e_binding_bind_property_full (
			shell_window, "active-view",
			e_shell_header_bar_get_new_button (E_SHELL_HEADER_BAR (priv->headerbar)),
			"prefer-item",
			G_BINDING_SYNC_CREATE,
			shell_window_active_view_to_prefer_item,
			(GBindingTransformFunc) NULL,
			NULL, (GDestroyNotify) NULL);
	}
}

void
e_shell_window_private_dispose (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	if (priv->active_view && *priv->active_view) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		g_settings_set_string (settings, "default-component-id", priv->active_view);
		g_clear_object (&settings);
	}

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

	g_clear_object (&priv->focus_tracker);
	g_clear_object (&priv->ui_manager);

	g_hash_table_remove_all (priv->loaded_views);

	g_clear_object (&priv->alert_bar);
	g_clear_object (&priv->content_pane);
	g_clear_object (&priv->content_notebook);
	g_clear_object (&priv->sidebar_notebook);
	g_clear_object (&priv->switcher);
	g_clear_object (&priv->tooltip_label);
	g_clear_object (&priv->status_notebook);
	g_clear_object (&priv->menu_bar);
}

void
e_shell_window_private_finalize (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	g_hash_table_destroy (priv->loaded_views);
	g_hash_table_destroy (priv->action_groups_by_view);

	g_slist_free_full (priv->postponed_alerts, g_object_unref);
	g_free (priv->geometry);
}

static void
e_shell_window_activate_action_groups_for_view (EShellWindow *shell_window,
						const gchar *view_name)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (!e_shell_window_get_ui_manager (shell_window))
		return;

	g_hash_table_iter_init (&iter, shell_window->priv->action_groups_by_view);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gboolean is_active = g_strcmp0 (key, view_name) == 0;
		GPtrArray *action_groups = value;
		guint ii;

		/* The 'calendar' view uses actions from 'memos' and 'tasks',
		   thus make sure these are active too. */
		if (!is_active && g_strcmp0 (view_name, "calendar") == 0 &&
		    (g_strcmp0 (key, "memos") == 0 || g_strcmp0 (key, "tasks") == 0))
			is_active = TRUE;

		for (ii = 0; ii < action_groups->len; ii++) {
			GtkActionGroup *action_group = g_ptr_array_index (action_groups, ii);

			/* Set both, because using 'visible' doesn't work for the first key press,
			   while 'sensitive' does, even it is used by some 'update-actions' handlers. */
			gtk_action_group_set_visible (action_group, is_active);
			gtk_action_group_set_sensitive (action_group, is_active);
		}
	}
}

void
e_shell_window_switch_to_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	if (shell_window->priv->active_view == view_name)
		return;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	e_shell_window_activate_action_groups_for_view (shell_window, view_name);

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

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_title = e_shell_view_get_title (shell_view);

	if (e_util_get_use_header_bar ()) {
		gtk_window_set_title (GTK_WINDOW (shell_window), view_title);
	} else {
		gchar *window_title;

		/* Translators: This is used for the main window title. */
		window_title = g_strdup_printf (_("%s — Evolution"), view_title);
		gtk_window_set_title (GTK_WINDOW (shell_window), window_title);
		g_free (window_title);
	}
}

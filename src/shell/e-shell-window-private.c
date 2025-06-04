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

	priv->loaded_views = loaded_views;
	priv->signal_handler_ids = signal_handler_ids;
	priv->action_groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* XXX This kind of violates the shell window being unaware
	 *     of specific shell views, but we need a sane fallback. */
	g_warn_if_fail (g_snprintf (priv->active_view, sizeof (priv->active_view), "mail") < sizeof (priv->active_view));

	gtk_window_set_title (GTK_WINDOW (shell_window), _("Evolution"));
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
	EUIAction *action;
	GtkBox *box;
	GtkWidget *widget;
	GtkWindow *window;
	GSettings *settings;

#ifndef G_OS_WIN32
	EUIActionGroup *action_group;
#endif

	window = GTK_WINDOW (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_window->priv->is_main_instance = shell_window_check_is_main_instance (GTK_APPLICATION (shell), window);
	shell_window->priv->switch_to_menu = g_menu_new ();

	e_shell_window_actions_constructed (shell_window);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (shell_window), widget);
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	/* it draws some children differently with it in some themes */
	gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "vertical");

	box = GTK_BOX (widget);

	if (e_util_get_use_header_bar ()) {
		GtkStyleContext *style_context;
		GtkCssProvider *provider;
		GError *local_error = NULL;

		widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_visible (widget, TRUE);
		shell_window->priv->headerbar_box = GTK_BOX (g_object_ref_sink (widget));

		provider = gtk_css_provider_new ();

		if (!gtk_css_provider_load_from_data (provider, "#evo-titlebar-box { padding:0px; margin:0px; border:0px; min-height:0px; }", -1, &local_error))
			g_critical ("%s: Failed to load CSS data: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

		g_clear_error (&local_error);

		gtk_widget_set_name (widget, "evo-titlebar-box");
		style_context = gtk_widget_get_style_context (widget);
		gtk_style_context_add_provider (style_context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_style_context_remove_class (style_context, "vertical");
		g_clear_object (&provider);

		gtk_window_set_titlebar (window, widget);
	}

	widget = e_alert_bar_new ();
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
	shell_window->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	widget = gtk_notebook_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		"show-tabs", FALSE,
		"show-border", FALSE,
		NULL);
	gtk_box_pack_start (box, widget, TRUE, TRUE, 0);
	shell_window->priv->views_notebook = g_object_ref (GTK_NOTEBOOK (widget));

	/* Bunch of chores to do when the active view changes. */

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_icon), NULL);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (e_shell_window_update_title), NULL);

#ifndef G_OS_WIN32
	/* Support lockdown. */

	settings = e_util_ref_settings ("org.gnome.desktop.lockdown");

	action_group = g_hash_table_lookup (shell_window->priv->action_groups, "lockdown-printing");

	g_settings_bind (
		settings, "disable-printing",
		action_group, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action_group = g_hash_table_lookup (shell_window->priv->action_groups, "lockdown-print-setup");

	g_settings_bind (
		settings, "disable-print-setup",
		action_group, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action_group = g_hash_table_lookup (shell_window->priv->action_groups, "lockdown-save-to-disk");

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

	/* claim the window before the view is created, thus the shell backend has
	   added actions into the "new-item" and the "new-source" action groups,
	   ready for the New header bar or the New tool bar buttons */
	gtk_application_add_window (GTK_APPLICATION (shell), window);

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
		gtk_window_set_default_size (window, 1440, 900);
		e_restore_window (
			window, "/org/gnome/evolution/shell/window/",
			E_RESTORE_WINDOW_SIZE | E_RESTORE_WINDOW_POSITION);
	}

	g_object_unref (settings);
}

void
e_shell_window_private_dispose (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	if (*priv->active_view) {
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

	g_hash_table_remove_all (priv->loaded_views);
	g_hash_table_remove_all (priv->action_groups);

	g_clear_object (&priv->alert_bar);
	g_clear_object (&priv->headerbar_box);
	g_clear_object (&priv->switch_to_menu);
}

void
e_shell_window_private_finalize (EShellWindow *shell_window)
{
	EShellWindowPrivate *priv = shell_window->priv;

	g_hash_table_destroy (priv->loaded_views);
	g_hash_table_destroy (priv->action_groups);

	g_slist_free_full (priv->postponed_alerts, g_object_unref);
	g_free (priv->geometry);
}

void
e_shell_window_switch_to_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	EShellView *shell_view;
	GtkWidget *headerbar;
	gint page_num, visible_page_num;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	if (g_strcmp0 (shell_window->priv->active_view, view_name) == 0)
		return;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	if (!shell_view) {
		GHashTableIter iter;
		gpointer value;

		g_warning ("%s: Shell view '%s' not found among %u loaded views", G_STRFUNC,
			view_name, g_hash_table_size (shell_window->priv->loaded_views));

		g_hash_table_iter_init (&iter, shell_window->priv->loaded_views);
		if (!g_hash_table_iter_next (&iter, NULL, &value))
			return;

		shell_view = value;
	}

	page_num = e_shell_view_get_page_num (shell_view);
	visible_page_num = gtk_notebook_get_current_page (shell_window->priv->views_notebook);
	if (page_num != visible_page_num && visible_page_num >= 0 &&
	    visible_page_num < gtk_notebook_get_n_pages (shell_window->priv->views_notebook)) {
		GtkWidget *page;

		page = gtk_notebook_get_nth_page (shell_window->priv->views_notebook, visible_page_num);
		if (page) {
			EShellView *visible_view = E_SHELL_VIEW (page);

			if (visible_view) {
				headerbar = e_shell_view_get_headerbar (visible_view);
				if (headerbar)
					gtk_widget_set_visible (headerbar, FALSE);
			}
		}
	}

	gtk_notebook_set_current_page (shell_window->priv->views_notebook, page_num);

	headerbar = e_shell_view_get_headerbar (shell_view);
	if (headerbar)
		gtk_widget_set_visible (headerbar, TRUE);

	g_warn_if_fail (g_snprintf (shell_window->priv->active_view, sizeof (shell_window->priv->active_view), "%s", view_name) < sizeof (shell_window->priv->active_view));
	g_object_notify (G_OBJECT (shell_window), "active-view");

	e_shell_view_update_actions (shell_view);
}

void
e_shell_window_update_icon (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EUIAction *action;
	const gchar *view_name;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	action = e_shell_view_get_switcher_action (shell_view);
	gtk_window_set_icon_name (GTK_WINDOW (shell_window), e_ui_action_get_icon_name (action));
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

GMenuModel *
e_shell_window_ref_switch_to_menu_model (EShellWindow *self)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (self), NULL);

	if (!self->priv->switch_to_menu)
		return NULL;

	return G_MENU_MODEL (g_object_ref (self->priv->switch_to_menu));
}

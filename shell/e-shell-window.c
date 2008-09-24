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

#include <gconf/gconf-client.h>

#include <es-event.h>

#include <e-util/e-plugin-ui.h>
#include <e-util/e-util-private.h>

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_SAFE_MODE,
	PROP_SHELL
};

static gpointer parent_class;

static EShellView *
shell_window_new_view (EShellWindow *shell_window,
                       GType shell_view_type,
                       const gchar *view_name,
                       const gchar *title)
{
	GHashTable *loaded_views;
	EShellView *shell_view;
	GtkNotebook *notebook;
	GtkAction *action;
	GtkWidget *widget;
	gchar *action_name;
	gint page_num;

	/* Determine the page number for the new shell view. */
	notebook = GTK_NOTEBOOK (shell_window->priv->content_notebook);
	page_num = gtk_notebook_get_n_pages (notebook);

	/* Get the switcher action for this view. */
	action_name = g_strdup_printf (SWITCHER_FORMAT, view_name);
	action = e_shell_window_get_action (shell_window, action_name);
	g_free (action_name);

	/* Create the shell view. */
	shell_view = g_object_new (
		shell_view_type, "action", action, "page-num",
		page_num, "shell-window", shell_window, NULL);

	/* Register the shell view. */
	loaded_views = shell_window->priv->loaded_views;
	g_hash_table_insert (loaded_views, g_strdup (view_name), shell_view);

	g_message ("Creating view \"%s\" on page %d", view_name, page_num);

	/* Add pages to the various shell window notebooks. */

	notebook = GTK_NOTEBOOK (shell_window->priv->content_notebook);
	widget = GTK_WIDGET (e_shell_view_get_shell_content (shell_view));
	gtk_notebook_append_page (notebook, widget, NULL);

	notebook = GTK_NOTEBOOK (shell_window->priv->sidebar_notebook);
	widget = GTK_WIDGET (e_shell_view_get_shell_sidebar (shell_view));
	gtk_notebook_append_page (notebook, widget, NULL);

	notebook = GTK_NOTEBOOK (shell_window->priv->status_notebook);
	widget = GTK_WIDGET (e_shell_view_get_shell_taskbar (shell_view));
	gtk_notebook_append_page (notebook, widget, NULL);

	/* Listen for changes that affect the shell window. */

	g_signal_connect_swapped (
		action, "notify::icon-name",
		G_CALLBACK (e_shell_window_update_icon), shell_window);

	g_signal_connect_swapped (
		shell_view, "notify::title",
		G_CALLBACK (e_shell_window_update_title), shell_window);

	g_signal_connect_swapped (
		shell_view, "notify::view-id",
		G_CALLBACK (e_shell_window_update_view_menu), shell_window);

	return shell_view;
}

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
		case PROP_ACTIVE_VIEW:
			e_shell_window_set_active_view (
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
		case PROP_ACTIVE_VIEW:
			g_value_set_string (
				value, e_shell_window_get_active_view (
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

	/**
	 * EShellWindow:active-view
	 *
	 * Name of the active #EShellView.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ACTIVE_VIEW,
		g_param_spec_string (
			"active-view",
			_("Active Shell View"),
			_("Name of the active shell view"),
			NULL,
			G_PARAM_READWRITE));

	/**
	 * EShellWindow:safe-mode
	 *
	 * Whether the shell window is in safe mode.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SAFE_MODE,
		g_param_spec_boolean (
			"safe-mode",
			_("Safe Mode"),
			_("Whether the shell window is in safe mode"),
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/**
	 * EShellWindow:shell
	 *
	 * The #EShell singleton.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL,
		g_param_spec_object (
			"shell",
			_("Shell"),
			_("The EShell singleton"),
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_window_init (EShellWindow *shell_window)
{
	GtkUIManager *ui_manager;

	shell_window->priv = E_SHELL_WINDOW_GET_PRIVATE (shell_window);

	e_shell_window_private_init (shell_window);

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	e_plugin_ui_register_manager (
		"org.gnome.evolution.shell", ui_manager, shell_window);
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

/**
 * e_shell_window_new:
 * @shell: an #EShell
 * @safe_mode: whether to initialize the window to "safe mode"
 *
 * Returns a new #EShellWindow.
 *
 * It's up to the various #EShellView<!-- -->'s to define exactly
 * what "safe mode" means, but the #EShell usually puts the initial
 * #EShellWindow into "safe mode" if detects the previous Evolution
 * session crashed.
 *
 * The initial view for the window is determined by GConf key
 * <filename>/apps/evolution/shell/view_defaults/component_id</filename>.
 * Or, if the GConf key is not set or can't be read, the first view
 * in the switcher is used.
 *
 * Returns: a new #EShellWindow
 **/
GtkWidget *
e_shell_window_new (EShell *shell,
                    gboolean safe_mode)
{
	return g_object_new (
		E_TYPE_SHELL_WINDOW,
		"shell", shell, "safe-mode", safe_mode, NULL);
}

/**
 * e_shell_window_get_shell:
 * @shell_window: an #EShellWindow
 *
 * Returns the #EShell that was passed to e_shell_window_new().
 *
 * Returns: the #EShell
 **/
EShell *
e_shell_window_get_shell (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->shell;
}

/**
 * e_shell_window_get_shell_view:
 * @shell_window: an #EShellWindow
 * @view_name: name of a shell view
 *
 * Returns the #EShellView named @view_name (see the
 * <structfield>name</structfield> field in #EShellModuleInfo).  This
 * will also instantiate the #EShellView the first time it's requested.
 * To reduce resource consumption, Evolution tries to delay instantiating
 * shell views until the user switches to them.  So in general, only the
 * active view name, as returned by e_shell_window_get_active_view(),
 * should be requested.
 *
 * Returns: the requested #EShellView, or %NULL if no such view is
 *          registered
 **/
gpointer
e_shell_window_get_shell_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	GHashTable *loaded_views;
	EShellView *shell_view;
	GType *children;
	guint n_children, ii;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	loaded_views = shell_window->priv->loaded_views;
	shell_view = g_hash_table_lookup (loaded_views, view_name);

	if (shell_view != NULL)
		return shell_view;

	children = g_type_children (E_TYPE_SHELL_VIEW, &n_children);

	for (ii = 0; ii < n_children && shell_view == NULL; ii++) {
		GType shell_view_type = children[ii];
		EShellViewClass *class;

		class = g_type_class_ref (shell_view_type);

		if (class->type_module == NULL) {
			g_critical (
				"Module member not set on %s",
				G_OBJECT_CLASS_NAME (class));
			continue;
		}

		if (strcmp (view_name, class->type_module->name) == 0)
			shell_view = shell_window_new_view (
				shell_window, shell_view_type,
				view_name, class->label);

		g_type_class_unref (class);
	}

	if (shell_view == NULL)
		g_critical ("Unknown shell view name: %s", view_name);

	return shell_view;
}

/**
 * e_shell_window_get_ui_manager:
 * @shell_window: an #EShellWindow
 *
 * Returns @shell_window<!-- -->'s user interface manager, which
 * manages the window's menus and toolbars via #GtkAction<!-- -->s.
 * This is the mechanism by which shell views and plugins can extend
 * Evolution's menus and toolbars.
 *
 * Returns: the #GtkUIManager for @shell_window
 **/
GtkUIManager *
e_shell_window_get_ui_manager (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->ui_manager;
}

/**
 * e_shell_window_get_action:
 * @shell_window: an #EShellWindow
 * @action_name: the name of an action
 *
 * Returns the #GtkAction named @action_name in @shell_window<!-- -->'s
 * user interface manager, or %NULL if no such action exists.
 *
 * Returns: the #GtkAction named @action_name
 **/
GtkAction *
e_shell_window_get_action (EShellWindow *shell_window,
                           const gchar *action_name)
{
	GtkUIManager *ui_manager;
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	iter = gtk_ui_manager_get_action_groups (ui_manager);

	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

/**
 * e_shell_window_get_action_group:
 * @shell_window: an #EShellWindow
 * @group_name: the name of an action group
 *
 * Returns the #GtkActionGroup named @group_name in
 * @shell_window<!-- -->'s user interface manager, or %NULL if no
 * such action group exists.
 *
 * Returns: the #GtkActionGroup named @group_name
 **/
GtkActionGroup *
e_shell_window_get_action_group (EShellWindow *shell_window,
                                 const gchar *group_name)
{
	GtkUIManager *ui_manager;
	GList *iter;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	iter = gtk_ui_manager_get_action_groups (ui_manager);

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

/**
 * e_shell_window_get_managed_widget:
 * @shell_window: an #EShellWindow
 * @widget_path: path in the UI definintion
 *
 * Looks up a widget in @shell_window<!-- -->'s user interface manager by
 * following a path.  See gtk_ui_manager_get_widget() for more information
 * about paths.
 *
 * Returns: the widget found by following the path, or %NULL if no widget
 *          was found
 **/
GtkWidget *
e_shell_window_get_managed_widget (EShellWindow *shell_window,
                                   const gchar *widget_path)
{
	GtkUIManager *ui_manager;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	widget = gtk_ui_manager_get_widget (ui_manager, widget_path);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

/**
 * e_shell_window_get_active_view:
 * @shell_window: an #EShellWindow
 *
 * Returns the name of the active #EShellView.
 *
 * Returns: the name of the active view
 **/
const gchar *
e_shell_window_get_active_view (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->active_view;
}

/**
 * e_shell_window_set_active_view:
 * @shell_window: an #EShellWindow
 * @view_name: the name of the shell view to switch to
 *
 * Switches @shell_window to the #EShellView named @view_name, causing
 * the entire content of @shell_window to change.  This is typically
 * called as a result of the user clicking one of the switcher buttons.
 *
 * The name of the newly activated shell view is also written to GConf key
 * <filename>/apps/evolution/shell/view_defaults/component_id</filename>.
 * This makes the active shell view persistent across Evolution sessions.
 * It also causes new shell windows created within the current Evolution
 * session to open to the most recently selected shell view.
 **/
void
e_shell_window_set_active_view (EShellWindow *shell_window,
                                const gchar *view_name)
{
	GtkAction *action;
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	action = e_shell_view_get_action (shell_view);
	gtk_action_activate (action);

	/* XXX Radio actions refuse to activate if they're already active.
	 *     This causes problems during intialization if we're trying to
	 *     switch to the shell view whose corresponding radio action is
	 *     already active.  Fortunately we can detect that and force
	 *     the switch. */
	if (shell_window->priv->active_view == NULL)
		e_shell_window_switch_to_view (shell_window, view_name);
}

/**
 * e_shell_window_get_safe_mode:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window is in "safe mode".
 *
 * It's up to the various #EShellView<!-- -->'s to define exactly
 * what "safe mode" means.  The @shell_window simply manages the
 * "safe mode" state.
 *
 * Returns: %TRUE if @shell_window is in "safe mode"
 **/
gboolean
e_shell_window_get_safe_mode (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->safe_mode;
}

/**
 * e_shell_window_set_safe_mode:
 * @shell_window: an #EShellWindow
 * @safe_mode: whether to put @shell_window into "safe mode"
 *
 * If %TRUE, puts @shell_window into "safe mode".
 *
 * It's up to the various #EShellView<!-- -->'s to define exactly
 * what "safe mode" means.  The @shell_window simply manages the
 * "safe mode" state.
 **/
void
e_shell_window_set_safe_mode (EShellWindow *shell_window,
                              gboolean safe_mode)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	shell_window->priv->safe_mode = safe_mode;

	g_object_notify (G_OBJECT (shell_window), "safe-mode");
}

/**
 * e_shell_window_show_popup_menu:
 * @shell_window: an #EShellWindow
 * @widget_path: path in the UI definition
 * @event: a #GdkEventButton
 *
 * Displays a context-sensitive (or "popup") menu that is described in
 * the UI definition loaded into @shell_window<!-- -->'s user interface
 * manager.  The menu will be shown at the current mouse cursor position.
 **/
void
e_shell_window_show_popup_menu (EShellWindow *shell_window,
                                const gchar *widget_path,
                                GdkEventButton *event)
{
	GtkWidget *menu;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	menu = e_shell_window_get_managed_widget (shell_window, widget_path);
	g_return_if_fail (GTK_IS_MENU (menu));

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

/**
 * e_shell_window_register_new_item_actions:
 * @shell_window: an #EShellWindow
 * @module_name: name of an #EShellModule
 * @entries: an array of #GtkActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #GtkAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShellModule<!-- -->'s
 * #EShell::window-created signal handler.  The #EShellModule calling
 * this function should pass its own name for the @module_name argument
 * (i.e. the <structfield>name</structfield> field from its own
 * #EShellModuleInfo).
 *
 * The registered #GtkAction<!-- -->s should be for creating individual
 * items such as an email message or a calendar appointment.
 **/
void
e_shell_window_register_new_item_actions (EShellWindow *shell_window,
                                          const gchar *module_name,
                                          const GtkActionEntry *entries,
                                          guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = shell_window->priv->new_item_actions;
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
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

	e_shell_window_update_new_menu (shell_window);
}

/**
 * e_shell_window_register_new_source_actions:
 * @shell_window: an #EShellWindow
 * @module_name: name of an #EShellModule
 * @entries: an array of #GtkActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #GtkAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShellModule<!-- -->'s
 * #EShell::window-created signal handler.  The #EShellModule calling
 * this function should pass its own name for the @module_name argument
 * (i.e. the <structfield>name</structfield> field from its own
 * #EShellModuleInfo).
 *
 * The registered #GtkAction<!-- -->s should be for creating item
 * containers such as an email folder or a calendar.
 **/
void
e_shell_window_register_new_source_actions (EShellWindow *shell_window,
                                            const gchar *module_name,
                                            const GtkActionEntry *entries,
                                            guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (module_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = shell_window->priv->new_source_actions;
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
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

	e_shell_window_update_new_menu (shell_window);
}

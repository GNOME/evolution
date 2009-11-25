/*
 * e-shell-window.c
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

#include <gconf/gconf-client.h>

#include <e-util/e-plugin-ui.h>
#include <e-util/e-util-private.h>

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_GEOMETRY,
	PROP_SAFE_MODE,
	PROP_SHELL,
	PROP_UI_MANAGER
};

static gpointer parent_class;

static EShellView *
shell_window_new_view (EShellBackend *shell_backend,
                       EShellWindow *shell_window)
{
	GHashTable *loaded_views;
	EShellView *shell_view;
	GtkUIManager *ui_manager;
	GtkNotebook *notebook;
	GtkAction *action;
	GtkWidget *widget;
	const gchar *name;
	const gchar *id;
	gint page_num;
	GType type;

	name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;
	type = E_SHELL_BACKEND_GET_CLASS (shell_backend)->shell_view_type;

	/* First off, start the shell backend. */
	e_shell_backend_start (shell_backend);

	/* Determine the page number for the new shell view. */
	notebook = GTK_NOTEBOOK (shell_window->priv->content_notebook);
	page_num = gtk_notebook_get_n_pages (notebook);

	/* Get the switcher action for this view. */
	action = e_shell_window_get_shell_view_action (shell_window, name);

	/* Create the shell view. */
	shell_view = g_object_new (
		type, "action", action, "page-num",page_num,
		"shell-window", shell_window, NULL);

	/* Register the shell view. */
	loaded_views = shell_window->priv->loaded_views;
	g_hash_table_insert (loaded_views, g_strdup (name), shell_view);

	/* Register the GtkUIManager ID for the shell view. */
	id = E_SHELL_VIEW_GET_CLASS (shell_view)->ui_manager_id;
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	e_plugin_ui_register_manager (ui_manager, id, shell_view);

	/* Add pages to the various shell window notebooks. */

	/* We can't determine the shell view's page number until after the
	 * shell view is fully initialized because the shell view may load
	 * other shell views during initialization, and those other shell
	 * views will append their widgets to the notebooks before us. */
	page_num = gtk_notebook_get_n_pages (notebook);
	e_shell_view_set_page_num (shell_view, page_num);

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

	/* Execute an initial search. */
	e_shell_view_execute_search (shell_view);

	return shell_view;
}

static void
shell_window_update_close_action_cb (EShellWindow *shell_window)
{
	EShell *shell;
	GList *watched_windows;
	gint n_shell_windows = 0;

	shell = e_shell_window_get_shell (shell_window);
	watched_windows = e_shell_get_watched_windows (shell);

	/* Count the shell windows. */
	while (watched_windows != NULL) {
		if (E_IS_SHELL_WINDOW (watched_windows->data))
			n_shell_windows++;
		watched_windows = g_list_next (watched_windows);
	}

	/* Disable Close Window if there's only one shell window.
	 * Helps prevent users from accidentally quitting. */
	gtk_action_set_sensitive (ACTION (CLOSE), n_shell_windows > 1);
}

static void
shell_window_set_geometry (EShellWindow *shell_window,
                           const gchar *geometry)
{
	g_return_if_fail (shell_window->priv->geometry == NULL);

	shell_window->priv->geometry = g_strdup (geometry);
}

static void
shell_window_set_shell (EShellWindow *shell_window,
                        EShell *shell)
{
	GArray *array;
	gulong handler_id;

	g_return_if_fail (shell_window->priv->shell == NULL);

	shell_window->priv->shell = shell;

	g_object_add_weak_pointer (
		G_OBJECT (shell), &shell_window->priv->shell);

	/* Need to disconnect these when the window is closing. */

	array = shell_window->priv->signal_handler_ids;

	handler_id = g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (shell_window_update_close_action_cb),
		shell_window);

	g_array_append_val (array, handler_id);

	handler_id = g_signal_connect_swapped (
		shell, "window-destroyed",
		G_CALLBACK (shell_window_update_close_action_cb),
		shell_window);

	g_array_append_val (array, handler_id);

	g_object_notify (G_OBJECT (shell), "online");
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

		case PROP_GEOMETRY:
			shell_window_set_geometry (
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

		case PROP_UI_MANAGER:
			g_value_set_object (
				value, e_shell_window_get_ui_manager (
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
shell_window_constructed (GObject *object)
{
	e_shell_window_private_constructed (E_SHELL_WINDOW (object));
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
	object_class->constructed = shell_window_constructed;

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
	 * EShellWindow:geometry
	 *
	 * User-specified initial window geometry string.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_GEOMETRY,
		g_param_spec_string (
			"geometry",
			_("Geometry"),
			_("Initial window geometry string"),
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

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

	/**
	 * EShellWindow:ui-manager
	 *
	 * The shell window's #GtkUIManager.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_UI_MANAGER,
		g_param_spec_object (
			"ui-manager",
			_("UI Manager"),
			_("The shell window's GtkUIManager"),
			GTK_TYPE_UI_MANAGER,
			G_PARAM_READABLE));
}

static void
shell_window_init (EShellWindow *shell_window)
{
	shell_window->priv = E_SHELL_WINDOW_GET_PRIVATE (shell_window);

	e_shell_window_private_init (shell_window);
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
 * @geometry: initial window geometry string, or %NULL
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
                    gboolean safe_mode,
                    const gchar *geometry)
{
	return g_object_new (
		E_TYPE_SHELL_WINDOW,
		"shell", shell, "geometry", geometry,
		"safe-mode", safe_mode, NULL);
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

	return E_SHELL (shell_window->priv->shell);
}

/**
 * e_shell_window_get_shell_view:
 * @shell_window: an #EShellWindow
 * @view_name: name of a shell view
 *
 * Returns the #EShellView named @view_name (see the
 * <structfield>name</structfield> field in #EShellBackendInfo).  This
 * will also instantiate the #EShellView the first time it's requested.
 * To reduce resource consumption, Evolution tries to delay instantiating
 * shell views until the user switches to them.  So in general, only the
 * active view name, as returned by e_shell_window_get_active_view(),
 * should be requested.
 *
 * Returns: the requested #EShellView, or %NULL if no such view is
 *          registered
 **/
EShellView *
e_shell_window_get_shell_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	GHashTable *loaded_views;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	loaded_views = shell_window->priv->loaded_views;
	shell_view = g_hash_table_lookup (loaded_views, view_name);

	if (shell_view != NULL)
		return shell_view;

	shell = e_shell_window_get_shell (shell_window);
	shell_backend = e_shell_get_backend_by_name (shell, view_name);

	if (shell_backend == NULL) {
		g_critical ("Unknown shell view name: %s", view_name);
		return NULL;
	}

	return shell_window_new_view (shell_backend, shell_window);
}

/**
 * e_shell_window_get_shell_view_action:
 * @shell_window: an #EShellWindow
 * @view_name: name of a shell view
 *
 * Returns the switcher action for @view_name.
 *
 * An #EShellWindow creates a #GtkRadioAction for each registered subclass
 * of #EShellView.  This action gets passed to the #EShellSwitcher, which
 * displays a button that proxies the action.  When the #EShellView named
 * @view_name is active, the action's icon becomes the @shell_window icon.
 *
 * Returns: the switcher action for the #EShellView named @view_name,
 *          or %NULL if no such shell view exists
 **/
GtkAction *
e_shell_window_get_shell_view_action (EShellWindow *shell_window,
                                      const gchar *view_name)
{
	GtkAction *action;
	gchar *action_name;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	action_name = g_strdup_printf (SWITCHER_FORMAT, view_name);
	action = e_shell_window_get_action (shell_window, action_name);
	g_free (action_name);

	return action;
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

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	return e_lookup_action (ui_manager, action_name);
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

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	return e_lookup_action_group (ui_manager, group_name);
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
 * e_shell_window_add_action_group:
 * @shell_window: an #EShellWindow
 * @group_name: the name of the new action group
 *
 * Creates a new #GtkActionGroup and adds it to @shell_window<!-- -->'s
 * user interface manager.  This also takes care of details like setting
 * the translation domain.
 **/
void
e_shell_window_add_action_group (EShellWindow *shell_window,
                                 const gchar *group_name)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	const gchar *domain;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (group_name != NULL);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	action_group = gtk_action_group_new (group_name);
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);
}

/**
 * e_shell_window_register_new_item_actions:
 * @shell_window: an #EShellWindow
 * @backend_name: name of an #EShellBackend
 * @entries: an array of #GtkActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #GtkAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShell<!-- -->'s
 * #EShell::window-created signal handler.  The #EShellBackend calling
 * this function should pass its own name for the @backend_name argument
 * (i.e. the <structfield>name</structfield> field from its own
 * #EShellBackendInfo).
 *
 * The registered #GtkAction<!-- -->s should be for creating individual
 * items such as an email message or a calendar appointment.  The action
 * labels should be marked for translation with the "New" context using
 * the NC_() macro.
 **/
void
e_shell_window_register_new_item_actions (EShellWindow *shell_window,
                                          const gchar *backend_name,
                                          GtkActionEntry *entries,
                                          guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (backend_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = ACTION_GROUP (NEW_ITEM);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	backend_name = g_intern_string (backend_name);

	/* XXX The action label translations are retrieved from the
	 *     message context "New", but gtk_action_group_add_actions()
	 *     does not support message contexts.  So we have to fetch
	 *     the label translations ourselves before adding them to
	 *     the action group.
	 *
	 *     gtk_action_group_set_translate_func() does not help here
	 *     because the action tooltips do not use a message context
	 *     (though I suppose they could). */
	for (ii = 0; ii < n_entries; ii++)
		entries[ii].label = g_dpgettext2 (
			GETTEXT_PACKAGE, "New", entries[ii].label);

	gtk_action_group_add_actions (
		action_group, entries, n_entries, shell_window);

	/* Tag each action with the name of the shell backend that
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
			"backend-name", (gpointer) backend_name);

		/* The first action becomes the first item in the "New"
		 * menu, and consequently its icon is shown in the "New"
		 * button when the shell backend's view is active.  This
		 * is all sorted out in shell_window_extract_actions().
		 * Note, the data value just needs to be non-zero. */
		if (ii == 0)
			g_object_set_data (
				G_OBJECT (action),
				"primary", GINT_TO_POINTER (TRUE));
	}

	e_shell_window_update_new_menu (shell_window);
}

/**
 * e_shell_window_register_new_source_actions:
 * @shell_window: an #EShellWindow
 * @backend_name: name of an #EShellBackend
 * @entries: an array of #GtkActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #GtkAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShell<!-- -->'s
 * #EShell::window-created signal handler.  The #EShellBackend calling
 * this function should pass its own name for the @backend_name argument
 * (i.e. the <structfield>name</structfield> field from its own
 * #EShellBackendInfo).
 *
 * The registered #GtkAction<!-- -->s should be for creating item
 * containers such as an email folder or a calendar.  The action labels
 * should be marked for translation with the "New" context using the
 * NC_() macro.
 **/
void
e_shell_window_register_new_source_actions (EShellWindow *shell_window,
                                            const gchar *backend_name,
                                            GtkActionEntry *entries,
                                            guint n_entries)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	guint ii;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (backend_name != NULL);
	g_return_if_fail (entries != NULL);

	action_group = ACTION_GROUP (NEW_SOURCE);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	backend_name = g_intern_string (backend_name);

	/* XXX The action label translations are retrieved from the
	 *     message context "New", but gtk_action_group_add_actions()
	 *     does not support message contexts.  So we have to fetch
	 *     the label translations ourselves before adding them to
	 *     the action group.
	 *
	 *     gtk_action_group_set_translate_func() does not help here
	 *     because the action tooltips do not use a message context
	 *     (though I suppose they could). */
	for (ii = 0; ii < n_entries; ii++)
		entries[ii].label = g_dpgettext2 (
			GETTEXT_PACKAGE, "New", entries[ii].label);

	gtk_action_group_add_actions (
		action_group, entries, n_entries, shell_window);

	/* Tag each action with the name of the shell backend that
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
			"backend-name", (gpointer) backend_name);
	}

	e_shell_window_update_new_menu (shell_window);
}

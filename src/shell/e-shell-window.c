/*
 * e-shell-window.c
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

/**
 * SECTION: e-shell-window
 * @short_description: the main window
 * @include: shell/e-shell-window.h
 **/

#include "evolution-config.h"

#include "e-shell-window-private.h"

enum {
	PROP_0,
	PROP_ACTIVE_VIEW,
	PROP_ALERT_BAR,
	PROP_FOCUS_TRACKER,
	PROP_GEOMETRY,
	PROP_SAFE_MODE,
	PROP_SHELL
};

enum {
	CLOSE_ALERT,
	SHELL_VIEW_CREATED,
	UPDATE_NEW_MENU,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_shell_window_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EShellWindow, e_shell_window, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (EShellWindow)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, e_shell_window_alert_sink_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static const char *css =
".table-header {\
	border-bottom: 1px solid @borders;\
}\
.button {\
	padding: 3px 5px;\
}\
.table-header .button {\
	border-right: 1px solid @borders;\
}\
.table-header .button.last {\
	border-right: none;\
}\
toolbar {\
	border-bottom: 1px solid @borders;\
}\
.taskbar border {\
	border-width: 1px 0 0 0;\
}\
.header-box {\
	border-bottom: 1px solid @borders;\
	padding: 3px;\
}\
#e-attachment-bar {\
	border-top: 1px solid @borders;\
}\
";

static void
shell_window_update_close_action_cb (EShellWindow *shell_window)
{
	EShell *shell;
	EUIAction *action;
	GtkApplication *application;
	GList *list;
	gint n_shell_windows = 0;

	shell = e_shell_window_get_shell (shell_window);

	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Count the shell windows. */
	while (list != NULL && n_shell_windows <= 1) {
		if (E_IS_SHELL_WINDOW (list->data))
			n_shell_windows++;
		list = g_list_next (list);
	}

	action = e_shell_window_get_ui_action (shell_window, "close");

	/* Disable Close Window if there's only one shell window.
	 * Helps prevent users from accidentally quitting. */
	e_ui_action_set_sensitive (action, n_shell_windows > 1);
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
		shell, "window-added",
		G_CALLBACK (shell_window_update_close_action_cb),
		shell_window);

	g_array_append_val (array, handler_id);

	handler_id = g_signal_connect_swapped (
		shell, "window-removed",
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

		case PROP_ALERT_BAR:
			g_value_set_object (
				value, e_shell_window_get_alert_bar (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_FOCUS_TRACKER:
			g_value_set_object (
				value, e_shell_window_get_focus_tracker (
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
	G_OBJECT_CLASS (e_shell_window_parent_class)->dispose (object);
}

static void
shell_window_finalize (GObject *object)
{
	e_shell_window_private_finalize (E_SHELL_WINDOW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_window_parent_class)->finalize (object);
}

static void
shell_window_constructed (GObject *object)
{
	EShellWindow *shell_window = E_SHELL_WINDOW (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_window_parent_class)->constructed (object);

	e_shell_window_private_constructed (shell_window);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
shell_window_close_alert (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *alert_bar;
	const gchar *view_name;

	/* Close view-specific alerts first, followed by global alerts. */

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);
	alert_bar = e_shell_content_get_alert_bar (shell_content);

	if (!e_alert_bar_close_alert (E_ALERT_BAR (alert_bar))) {
		alert_bar = e_shell_window_get_alert_bar (shell_window);
		e_alert_bar_close_alert (E_ALERT_BAR (alert_bar));
	}
}

static void
e_shell_window_update_icon_for_active_view (EUIAction *action,
					    GParamSpec *param,
					    gpointer user_data)
{
	EShellWindow *self = user_data;
	EUIAction *active_view_action;

	active_view_action = e_shell_window_get_shell_view_action (self, e_shell_window_get_active_view (self));
	if (active_view_action == action)
		e_shell_window_update_icon (self);
}

static void
e_shell_window_update_title_for_active_view (EShellView *shell_view,
					     GParamSpec *param,
					     gpointer user_data)
{
	EShellWindow *self = user_data;

	if (e_shell_view_is_active (shell_view))
		e_shell_window_update_title (self);
}

static EShellView *
shell_window_create_shell_view (EShellWindow *shell_window,
                                const gchar *view_name)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	GHashTable *loaded_views;
	GtkNotebook *notebook;
	GSettings *settings;
	EUIAction *action;
	const gchar *name;
	gint page_num;
	GType type;

	shell = e_shell_window_get_shell (shell_window);
	shell_backend = e_shell_get_backend_by_name (shell, view_name);

	if (shell_backend == NULL) {
		GList *backends;

		g_critical ("Unknown shell view name: %s", view_name);

		backends = e_shell_get_shell_backends (shell);
		if (!backends) {
			notebook = GTK_NOTEBOOK (shell_window->priv->views_notebook);

			if (!gtk_notebook_get_n_pages (notebook)) {
				GtkWidget *widget;

				if (shell_window->priv->headerbar_box) {
					widget = gtk_header_bar_new ();

					gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (widget), TRUE);
					gtk_widget_set_visible (widget, TRUE);

					gtk_box_pack_start (shell_window->priv->headerbar_box, widget, FALSE, FALSE, 0);

					e_binding_bind_property (widget, "title",
						shell_window, "title",
						G_BINDING_DEFAULT);

					gtk_header_bar_set_title (GTK_HEADER_BAR (widget), _("Evolution"));
				}

				widget = gtk_label_new ("Failed to load any view. Is installation broken?");
				gtk_widget_set_visible (widget, TRUE);

				gtk_notebook_set_current_page (notebook, gtk_notebook_append_page (notebook, widget, NULL));
			}

			return NULL;
		}

		/* fallback to one of the existing shell views, to not have shown an empty window */
		shell_backend = E_SHELL_BACKEND (backends->data);

		/* does the fallback already exist? */
		shell_view = g_hash_table_lookup (shell_window->priv->loaded_views, E_SHELL_BACKEND_GET_CLASS (shell_backend)->name);
		if (shell_view)
			return shell_view;
	}

	name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;
	type = E_SHELL_BACKEND_GET_CLASS (shell_backend)->shell_view_type;

	/* First off, start the shell backend. */
	e_shell_backend_start (shell_backend);

	/* Determine the page number for the new shell view. */
	notebook = GTK_NOTEBOOK (shell_window->priv->views_notebook);
	page_num = gtk_notebook_get_n_pages (notebook);

	/* Get the switcher action for this view. */
	action = e_shell_window_get_shell_view_action (shell_window, name);

	/* Create the shell view. */
	shell_view = g_object_new (type,
		"switcher-action", action,
		"page-num", page_num,
		"shell-window", shell_window,
		NULL);

	/* Register the shell view. */
	loaded_views = shell_window->priv->loaded_views;
	g_hash_table_insert (loaded_views, g_strdup (name), g_object_ref_sink (shell_view));

	/* We can't determine the shell view's page number until after the
	 * shell view is fully initialized because the shell view may load
	 * other shell views during initialization, and those other shell
	 * views will append their widgets to the notebooks before us. */
	page_num = gtk_notebook_append_page (notebook, GTK_WIDGET (shell_view), NULL);
	e_shell_view_set_page_num (shell_view, page_num);

	if (e_shell_view_get_headerbar (shell_view) &&
	    shell_window->priv->headerbar_box) {
		GtkWidget *headerbar;

		headerbar = g_object_ref (e_shell_view_get_headerbar (shell_view));
		gtk_widget_unparent (headerbar);

		gtk_box_pack_start (shell_window->priv->headerbar_box, headerbar, FALSE, FALSE, 0);
		gtk_widget_set_visible (headerbar, g_hash_table_size (loaded_views) == 1);

		e_binding_bind_property (shell_window, "title",
			headerbar, "title",
			G_BINDING_SYNC_CREATE);

		g_clear_object (&headerbar);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "folder-bar-width",
			shell_view, "sidebar-width",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "menubar-visible",
			shell_view, "menubar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "sidebar-visible",
			shell_view, "sidebar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "statusbar-visible",
			shell_view, "taskbar-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "buttons-visible",
			shell_view, "switcher-visible",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "toolbar-visible",
			shell_view, "toolbar-visible",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "folder-bar-width-sub",
			shell_view, "sidebar-width",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "menubar-visible-sub",
			shell_view, "menubar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "sidebar-visible-sub",
			shell_view, "sidebar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "statusbar-visible-sub",
			shell_view, "taskbar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "buttons-visible-sub",
			shell_view, "switcher-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "toolbar-visible-sub",
			shell_view, "toolbar-visible",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);
	}

	g_clear_object (&settings);

	/* Listen for changes that affect the shell window. */

	e_signal_connect_notify_object (
		action, "notify::icon-name",
		G_CALLBACK (e_shell_window_update_icon_for_active_view), shell_window, 0);

	e_signal_connect_notify_object (
		shell_view, "notify::title",
		G_CALLBACK (e_shell_window_update_title_for_active_view), shell_window, 0);

	return shell_view;
}

static void
shell_window_submit_alert (EAlertSink *alert_sink,
                           EAlert *alert)
{
	EShellWindow *shell_window;
	GtkWidget *alert_bar;

	shell_window = E_SHELL_WINDOW (alert_sink);

	if (!gtk_widget_get_mapped (GTK_WIDGET (shell_window)) ||
	    shell_window->priv->postponed_alerts) {
		shell_window->priv->postponed_alerts = g_slist_prepend (
			shell_window->priv->postponed_alerts, g_object_ref (alert));
		return;
	}

	alert_bar = e_shell_window_get_alert_bar (shell_window);

	e_alert_bar_submit_alert (E_ALERT_BAR (alert_bar), alert);
}

static gboolean
shell_window_submit_postponed_alerts_idle_cb (gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EAlertSink *alert_sink;
	GSList *postponed_alerts, *link;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	postponed_alerts = g_slist_reverse (shell_window->priv->postponed_alerts);
	shell_window->priv->postponed_alerts = NULL;

	alert_sink = E_ALERT_SINK (shell_window);

	for (link = postponed_alerts; link; link = g_slist_next (link)) {
		EAlert *alert = link->data;

		shell_window_submit_alert (alert_sink, alert);
	}

	g_slist_free_full (postponed_alerts, g_object_unref);

	return FALSE;
}

static void
shell_window_map (GtkWidget *widget)
{
	EShellWindow *shell_window;
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (widget));

	shell_window = E_SHELL_WINDOW (widget);

	/* Do this before the parent's map() is called, to distinguish from search and from window open */
	shell_view = e_shell_window_peek_shell_view (shell_window, e_shell_window_get_active_view (shell_window));
	if (shell_view) {
		EShellContent *shell_content;

		shell_content = e_shell_view_get_shell_content (shell_view);
		if (shell_content)
			e_shell_content_focus_search_results (shell_content);
	}

	/* Chain up to parent's method */
	GTK_WIDGET_CLASS (e_shell_window_parent_class)->map (widget);

	g_idle_add_full (
		G_PRIORITY_LOW,
		shell_window_submit_postponed_alerts_idle_cb,
		g_object_ref (shell_window), g_object_unref);
}

static gboolean
shell_window_delete_event_cb (GtkWidget *widget,
			      GdkEventAny *event)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (widget), FALSE);

	e_alert_bar_clear (E_ALERT_BAR (E_SHELL_WINDOW (widget)->priv->alert_bar));

	return FALSE;
}

static void
e_shell_window_class_init (EShellWindowClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_window_set_property;
	object_class->get_property = shell_window_get_property;
	object_class->dispose = shell_window_dispose;
	object_class->finalize = shell_window_finalize;
	object_class->constructed = shell_window_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = shell_window_map;

	class->close_alert = shell_window_close_alert;

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
			"Active Shell View",
			"Name of the active shell view",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:alert-bar
	 *
	 * Displays informational and error messages.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ALERT_BAR,
		g_param_spec_object (
			"alert-bar",
			"Alert Bar",
			"Displays informational and error messages",
			E_TYPE_ALERT_BAR,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:focus-tracker
	 *
	 * The shell window's #EFocusTracker.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_FOCUS_TRACKER,
		g_param_spec_object (
			"focus-tracker",
			"Focus Tracker",
			"The shell window's EFocusTracker",
			E_TYPE_FOCUS_TRACKER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

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
			"Geometry",
			"Initial window geometry string",
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

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
			"Safe Mode",
			"Whether the shell window is in safe mode",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

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
			"Shell",
			"The EShell singleton",
			E_TYPE_SHELL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow::close-alert
	 * @shell_window: the #EShellWindow which emitted the signal
	 *
	 * Closes either one #EShellView-specific #EAlert or else one
	 * global #EAlert.  This signal is bound to the Escape key.
	 **/
	signals[CLOSE_ALERT] = g_signal_new (
		"close-alert",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EShellWindowClass, close_alert),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EShellWindow::shell-view-created
	 * @shell_window: the #EShellWindow which emitted the signal
	 * @shell_view: the new #EShellView
	 *
	 * Emitted when a new #EShellView is instantiated by way of
	 * e_shell_window_get_shell_view().  The signal detail denotes
	 * the new view name, which can be used to obtain notification
	 * of when a particular #EShellView is created.
	 **/
	signals[SHELL_VIEW_CREATED] = g_signal_new (
		"shell-view-created",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		G_STRUCT_OFFSET (EShellWindowClass, shell_view_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SHELL_VIEW);

	/**
	 * EShellWindow::update-new-menu
	 * @shell_window: the #EShellWindow
	 *
	 * Emitted when the 'New' menu should be updated.
	 *
	 * Since: 3.46
	 **/
	signals[UPDATE_NEW_MENU] = g_signal_new (
		"update-new-menu",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_Escape, 0, "close-alert", 0);
}

static void
e_shell_window_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = shell_window_submit_alert;
}

static void
e_shell_window_init (EShellWindow *shell_window)
{
	GtkCssProvider *css_provider;

	shell_window->priv = e_shell_window_get_instance_private (shell_window);

	e_shell_window_private_init (shell_window);

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider, css, -1, NULL);
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
		GTK_STYLE_PROVIDER (css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_clear_object (&css_provider);

	g_signal_connect (shell_window, "delete-event",
		G_CALLBACK (shell_window_delete_event_cb), NULL);
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
 * The initial view for the window is determined by GSettings key
 * <filename>/org/gnome/evolution/shell/default-component-id</filename>.
 * Or, if the GSettings key is not set or can't be read, the first view
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
 * e_shell_window_is_main_instance:
 * @shell_window: an #EShellWindow
 *
 * Returns, whether the @shell_window is the main instance, which is
 * the window which was created as the first @shell_window.
 *
 * Returns: whether the @shell_window is the main instance
 **/
gboolean
e_shell_window_is_main_instance (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->is_main_instance;
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
 * The function emits an #EShellWindow::shell-view-created signal with
 * @view_name as the signal detail when it instantiates an #EShellView.
 *
 * Returns: the requested #EShellView, or %NULL if no such view is
 *          registered
 **/
EShellView *
e_shell_window_get_shell_view (EShellWindow *shell_window,
                               const gchar *view_name)
{
	EShellView *shell_view;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	shell_view = e_shell_window_peek_shell_view (shell_window, view_name);
	if (shell_view != NULL)
		return shell_view;

	shell_view = shell_window_create_shell_view (shell_window, view_name);

	if (shell_view) {
		/* it can fallback to a different view, if the @view_name does not exist */
		view_name = e_shell_view_get_name (shell_view);
	}

	g_signal_emit (
		shell_window, signals[SHELL_VIEW_CREATED],
		g_quark_from_string (view_name), shell_view);

	return shell_view;
}

/**
 * e_shell_window_peek_shell_view:
 * @shell_window: an #EShellWindow
 * @view_name: name of a shell view
 *
 * Returns the #EShellView named @view_name (see the
 * <structfield>name</structfield> field in #EShellBackendInfo), or
 * %NULL if the requested view has not yet been instantiated.  Unlike
 * e_shell_window_get_shell_view(), this function will not instantiate
 * the view itself.
 *
 * Returns: the requested #EShellView, or %NULL if no such view is
 *          instantiated
 **/
EShellView *
e_shell_window_peek_shell_view (EShellWindow *shell_window,
                                const gchar *view_name)
{
	GHashTable *loaded_views;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	loaded_views = shell_window->priv->loaded_views;

	return g_hash_table_lookup (loaded_views, view_name);
}

/**
 * e_shell_window_get_shell_view_action:
 * @shell_window: an #EShellWindow
 * @view_name: name of a shell view
 *
 * Returns the switcher action for @view_name.
 *
 * An #EShellWindow creates an #EUIAction for each registered subclass
 * of #EShellView.  This action gets passed to the #EShellSwitcher, which
 * displays a button that proxies the action.  When the #EShellView named
 * @view_name is active, the action's icon becomes the @shell_window icon.
 *
 * Returns: the switcher action for the #EShellView named @view_name,
 *          or %NULL if no such shell view exists
 **/
EUIAction *
e_shell_window_get_shell_view_action (EShellWindow *shell_window,
                                      const gchar *view_name)
{
	gchar action_name[128];

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), E_SHELL_SWITCHER_FORMAT, view_name) < sizeof (action_name));

	return e_shell_window_get_ui_action (shell_window, action_name);
}

/**
 * e_shell_window_get_alert_bar:
 * @shell_window: an #EShellWindow
 *
 * Returns the #EAlertBar used to display informational and error messages.
 *
 * Returns: the #EAlertBar for @shell_window
 **/
GtkWidget *
e_shell_window_get_alert_bar (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->alert_bar;
}

/**
 * e_shell_window_get_focus_tracker:
 * @shell_window: an #EShellWindow
 *
 * Returns @shell_window<!-- -->'s focus tracker, which directs
 * Cut, Copy, Paste and Select All main menu activations to the
 * appropriate editable or selectable widget.
 *
 * Returns: the #EFocusTracker for @shell_window
 **/
EFocusTracker *
e_shell_window_get_focus_tracker (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);

	return shell_window->priv->focus_tracker;
}

/**
 * e_shell_window_get_ui_action:
 * @shell_window: an #EShellWindow
 * @action_name: the name of an action
 *
 * Returns the #EUIAction named @action_name provided by the @shell_window<!-- -->
 * itself, or %NULL if no such action exists.
 *
 * Returns: (transfer none) (nullable): the #EUIAction named @action_name, or %NULL if not found
 *
 * Since: 3.56
 **/
EUIAction *
e_shell_window_get_ui_action (EShellWindow *shell_window,
			      const gchar *action_name)
{
	GHashTableIter iter;
	gpointer value = NULL;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	g_hash_table_iter_init (&iter, shell_window->priv->action_groups);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		EUIActionGroup *group = value;
		EUIAction *action;

		action = e_ui_action_group_get_action (group, action_name);
		if (action)
			return action;
	}

	return NULL;
}

/**
 * e_shell_window_get_ui_action_group:
 * @shell_window: an #EShellWindow
 * @group_name: the name of an action group
 *
 * Returns the #EUIActionGroup named @group_name provided by
 * the @shell_window<!-- --> itself, or %NULL if no such action
 * group exists.
 *
 * Returns: (transfer none) (nullable): the #EUIActionGroup named @group_name, or %NULL if not found
 *
 * Since: 3.56
 **/
EUIActionGroup *
e_shell_window_get_ui_action_group (EShellWindow *shell_window,
				    const gchar *group_name)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	return g_hash_table_lookup (shell_window->priv->action_groups, group_name);
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
 * The name of the newly activated shell view is also written to GSettings key
 * <filename>/org/gnome/evolution/shell/default-component-id</filename>.
 * This makes the active shell view persistent across Evolution sessions.
 * It also causes new shell windows created within the current Evolution
 * session to open to the most recently selected shell view.
 **/
void
e_shell_window_set_active_view (EShellWindow *shell_window,
                                const gchar *view_name)
{
	EUIAction *action;
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	/* the shell_view might not necessarily be the view_name, if such does not exist,
	   thus use the name from the view itself here */
	e_shell_window_switch_to_view (shell_window, e_shell_view_get_name (shell_view));

	action = e_shell_view_get_switcher_action (shell_view);
	e_ui_action_set_active (action, TRUE);

	/* Renegotiate the shell window size in case a newly-created
	 * shell view needs tweaked to accommodate a smaller screen. */
	gtk_widget_queue_resize (GTK_WIDGET (shell_window));
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

	if (shell_window->priv->safe_mode == safe_mode)
		return;

	shell_window->priv->safe_mode = safe_mode;

	g_object_notify (G_OBJECT (shell_window), "safe-mode");
}

typedef struct {
	EShellWindow *shell_window;
	ESource *source;
	gchar *extension_name;
	EShellWindowConnetClientFunc connected_cb;
	gpointer user_data;
	GDestroyNotify destroy_user_data;

	EClient *client;
} ConnectClientData;

static void
connect_client_data_free (gpointer ptr)
{
	ConnectClientData *cc_data = ptr;

	if (cc_data) {
		if (cc_data->client && cc_data->connected_cb)
			cc_data->connected_cb (cc_data->shell_window, cc_data->client, cc_data->user_data);

		g_clear_object (&cc_data->shell_window);
		g_clear_object (&cc_data->source);
		g_clear_object (&cc_data->client);
		g_free (cc_data->extension_name);

		if (cc_data->destroy_user_data)
			cc_data->destroy_user_data (cc_data->user_data);

		g_slice_free (ConnectClientData, cc_data);
	}
}

static void
shell_window_connect_client_thread (EAlertSinkThreadJobData *job_data,
				    gpointer user_data,
				    GCancellable *cancellable,
				    GError **error)
{
	ConnectClientData *cc_data = user_data;
	EShell *shell;
	EClientCache *client_cache;
	GError *local_error = NULL;

	g_return_if_fail (cc_data != NULL);

	shell = e_shell_window_get_shell (cc_data->shell_window);
	client_cache = e_shell_get_client_cache (shell);

	cc_data->client = e_client_cache_get_client_sync (client_cache,
		cc_data->source, cc_data->extension_name, (guint32) -1, cancellable, &local_error);

	e_util_propagate_open_source_job_error (job_data, cc_data->extension_name, local_error, error);
}

/**
 * e_shell_window_connect_client:
 * @shell_window: an #EShellWindow
 * @source: an #ESource to connect to
 * @extension_name: an extension name
 * @connected_cb: a callback to be called when the client is opened
 * @user_data: a user data passed to @connected_cb
 * @destroy_user_data: (allow none): callback to free @user_data when no longer needed
 *
 * Get's an #EClient from shell's #EClientCache in a dedicated thread, thus
 * the operation doesn't block UI. The @connected_cb is called in the main thread,
 * but only when the operation succeeded. Any failure is propageted to UI.
 *
 * Since: 3.16
 **/
void
e_shell_window_connect_client (EShellWindow *shell_window,
			       ESource *source,
			       const gchar *extension_name,
			       EShellWindowConnetClientFunc connected_cb,
			       gpointer user_data,
			       GDestroyNotify destroy_user_data)
{
	ConnectClientData *cc_data;
	EShellView *shell_view;
	EActivity *activity;
	gchar *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL, *display_name;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (extension_name != NULL);
	g_return_if_fail (connected_cb != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window,
		e_shell_window_get_active_view (shell_window));

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	display_name = e_util_get_source_full_name (e_shell_get_registry (e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view))), source);

	if (!e_util_get_open_source_job_info (extension_name, display_name,
		&description, &alert_ident, &alert_arg_0)) {
		g_free (display_name);
		g_warn_if_reached ();
		return;
	}

	g_free (display_name);

	cc_data = g_slice_new0 (ConnectClientData);
	cc_data->shell_window = g_object_ref (shell_window);
	cc_data->source = g_object_ref (source);
	cc_data->extension_name = g_strdup (extension_name);
	cc_data->connected_cb = connected_cb;
	cc_data->user_data = user_data;
	cc_data->destroy_user_data = destroy_user_data;
	cc_data->client = NULL;

	activity = e_shell_view_submit_thread_job (shell_view, description, alert_ident, alert_arg_0,
		shell_window_connect_client_thread, cc_data, connect_client_data_free);

	g_clear_object (&activity);
	g_free (description);
	g_free (alert_ident);
	g_free (alert_arg_0);
}

static void
shell_window_register_actions (EShellWindow *shell_window,
			       const gchar *backend_name,
			       const EUIActionEntry *entries,
			       guint n_entries,
			       EUIActionGroup *dest_action_group,
			       gboolean with_primary)
{
	EUIActionGroup *tmp_action_group;
	EUIManager *ui_manager;
	guint ii;
	EUIAction *primary_action = NULL;

	g_return_if_fail (E_IS_UI_ACTION_GROUP (dest_action_group));

	backend_name = g_intern_string (backend_name);

	ui_manager = e_ui_manager_new (NULL);

	e_ui_manager_add_actions (ui_manager, e_ui_action_group_get_name (dest_action_group), NULL, entries, n_entries, shell_window);

	tmp_action_group = e_ui_manager_get_action_group (ui_manager, e_ui_action_group_get_name (dest_action_group));

	for (ii = 0; ii < n_entries; ii++) {
		EUIAction *action;

		action = e_ui_action_group_get_action (tmp_action_group, entries[ii].name);

		/* XXX The action label translations are retrieved from the
		 *     message context "New", but e_ui_action_group_add_actions()
		 *     does not support message contexts. */
		e_ui_action_set_label (action, g_dpgettext2 (GETTEXT_PACKAGE, "New", entries[ii].label));

		g_object_set_data (G_OBJECT (action), "backend-name", (gpointer) backend_name);

		/* The first action becomes the first item in the "New"
		 * menu, and consequently its icon is shown in the "New"
		 * button when the shell backend's view is active.  This
		 * is all sorted out in shell_view_extract_actions().
		 * Note, the data value just needs to be non-zero. */
		if (with_primary && ii == 0) {
			g_object_set_data (G_OBJECT (action), "primary", GINT_TO_POINTER (TRUE));
			primary_action = g_object_ref (action);
		}

		/* "copy" the action to the permanent action group */
		e_ui_action_group_add (dest_action_group, action);
	}

	g_clear_object (&ui_manager);

	if (primary_action) {
		EShellBackend *shell_backend;

		shell_backend = e_shell_get_backend_by_name (e_shell_window_get_shell (shell_window), backend_name);

		/* set it only if not set from the settings */
		if (!e_shell_backend_get_prefer_new_item (shell_backend))
			e_shell_backend_set_prefer_new_item (shell_backend, g_action_get_name (G_ACTION (primary_action)));

		g_clear_object (&primary_action);
	}

	g_signal_emit (shell_window, signals[UPDATE_NEW_MENU], 0, NULL);
}

/**
 * e_shell_window_register_new_item_actions:
 * @shell_window: an #EShellWindow
 * @backend_name: name of an #EShellBackend
 * @entries: an array of #EUIActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #EUIAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShell<!-- -->'s
 * #GtkApplication::window-added signal handler.  The #EShellBackend
 * calling this function should pass its own name for the @backend_name
 * argument (i.e. the <structfield>name</structfield> field from its own
 * #EShellBackendInfo).
 *
 * The registered #EUIAction<!-- -->s should be for creating individual
 * items such as an email message or a calendar appointment.  The action
 * labels should be marked for translation with the "New" context using
 * the NC_() macro.
 **/
void
e_shell_window_register_new_item_actions (EShellWindow *shell_window,
                                          const gchar *backend_name,
                                          const EUIActionEntry *entries,
                                          guint n_entries)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (backend_name != NULL);
	g_return_if_fail (entries != NULL);

	shell_window_register_actions (shell_window, backend_name, entries, n_entries, ACTION_GROUP (NEW_ITEM), TRUE);
}

/**
 * e_shell_window_register_new_source_actions:
 * @shell_window: an #EShellWindow
 * @backend_name: name of an #EShellBackend
 * @entries: an array of #EUIActionEntry<!-- -->s
 * @n_entries: number of elements in the array
 *
 * Registers a list of #EUIAction<!-- -->s to appear in
 * @shell_window<!-- -->'s "New" menu and toolbar button.  This
 * function should be called from an #EShell<!-- -->'s
 * #GtkApplication::window-added signal handler.  The #EShellBackend
 * calling this function should pass its own name for the @backend_name
 * argument (i.e. the <structfield>name</structfield> field from its own
 * #EShellBackendInfo).
 *
 * The registered #EUIAction<!-- -->s should be for creating item
 * containers such as an email folder or a calendar.  The action labels
 * should be marked for translation with the "New" context using the
 * NC_() macro.
 **/
void
e_shell_window_register_new_source_actions (EShellWindow *shell_window,
                                            const gchar *backend_name,
                                            const EUIActionEntry *entries,
                                            guint n_entries)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (backend_name != NULL);
	g_return_if_fail (entries != NULL);

	shell_window_register_actions (shell_window, backend_name, entries, n_entries, ACTION_GROUP (NEW_SOURCE), FALSE);
}

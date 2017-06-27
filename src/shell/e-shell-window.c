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
	PROP_SHELL,
	PROP_MENUBAR_VISIBLE,
	PROP_SIDEBAR_VISIBLE,
	PROP_SWITCHER_VISIBLE,
	PROP_TASKBAR_VISIBLE,
	PROP_TOOLBAR_VISIBLE,
	PROP_UI_MANAGER
};

enum {
	CLOSE_ALERT,
	SHELL_VIEW_CREATED,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_shell_window_alert_sink_init
					(EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EShellWindow,
	e_shell_window,
	GTK_TYPE_WINDOW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_ALERT_SINK, e_shell_window_alert_sink_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

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
shell_window_menubar_update_new_menu (EShellWindow *shell_window)
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
}

static void
shell_window_toolbar_update_new_menu (GtkMenuToolButton *menu_tool_button,
                                      GParamSpec *pspec,
                                      EShellWindow *shell_window)
{
	GtkWidget *menu;

	/* Update the "New" menu tool button submenu. */
	menu = e_shell_window_create_new_menu (shell_window);
	gtk_menu_tool_button_set_menu (menu_tool_button, menu);
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

static void
shell_window_set_notebook_page (EShellWindow *shell_window,
                                GParamSpec *pspec,
                                GtkNotebook *notebook)
{
	EShellView *shell_view;
	const gchar *view_name;
	gint page_num;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	page_num = e_shell_view_get_page_num (shell_view);
	g_return_if_fail (page_num >= 0);

	gtk_notebook_set_current_page (notebook, page_num);
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

static void
shell_window_update_close_action_cb (EShellWindow *shell_window)
{
	EShell *shell;
	GtkApplication *application;
	GList *list;
	gint n_shell_windows = 0;

	shell = e_shell_window_get_shell (shell_window);

	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Count the shell windows. */
	while (list != NULL) {
		if (E_IS_SHELL_WINDOW (list->data))
			n_shell_windows++;
		list = g_list_next (list);
	}

	/* Disable Close Window if there's only one shell window.
	 * Helps prevent users from accidentally quitting. */
	gtk_action_set_sensitive (ACTION (CLOSE), n_shell_windows > 1);
}

static void
shell_window_tweak_for_small_screen (EShellWindow *shell_window)
{
	EShellView *shell_view;
	GtkWidget *shell_searchbar;
	const gchar *active_view;

	active_view = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, active_view);
	shell_searchbar = e_shell_view_get_searchbar (shell_view);

	e_shell_searchbar_set_filter_visible (
		E_SHELL_SEARCHBAR (shell_searchbar), FALSE);
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

		case PROP_MENUBAR_VISIBLE:
			e_shell_window_set_menubar_visible (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SIDEBAR_VISIBLE:
			e_shell_window_set_sidebar_visible (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SWITCHER_VISIBLE:
			e_shell_window_set_switcher_visible (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			return;

		case PROP_TASKBAR_VISIBLE:
			e_shell_window_set_taskbar_visible (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
			return;

		case PROP_TOOLBAR_VISIBLE:
			e_shell_window_set_toolbar_visible (
				E_SHELL_WINDOW (object),
				g_value_get_boolean (value));
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

		case PROP_MENUBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_window_get_menubar_visible (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_SIDEBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_window_get_sidebar_visible (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_SWITCHER_VISIBLE:
			g_value_set_boolean (
				value, e_shell_window_get_switcher_visible (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_TASKBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_window_get_taskbar_visible (
				E_SHELL_WINDOW (object)));
			return;

		case PROP_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_window_get_toolbar_visible (
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

	e_shell_window_private_constructed (shell_window);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_window_parent_class)->constructed (object);
}

static void
shell_window_get_preferred_width (GtkWidget *widget,
                                  gint *out_minimum_width,
                                  gint *out_natural_width)
{
	GdkScreen *screen;
	gint screen_width;
	gint minimum_width = 0;
	gint natural_width = 0;
	gboolean tweaked = FALSE;

	screen = gtk_widget_get_screen (widget);
	screen_width = gdk_screen_get_width (screen);

try_again:
	/* Chain up to parent's get_preferred_width() method. */
	GTK_WIDGET_CLASS (e_shell_window_parent_class)->
		get_preferred_width (widget, &minimum_width, &natural_width);

	if (!tweaked && minimum_width > screen_width) {
		EShellWindow *shell_window;

		shell_window = E_SHELL_WINDOW (widget);
		shell_window_tweak_for_small_screen (shell_window);

		tweaked = TRUE;  /* prevents looping */

		goto try_again;
	}

	*out_minimum_width = minimum_width;
	*out_natural_width = natural_width;
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
shell_window_menubar_deactivate_cb (GtkWidget *main_menu,
				    gpointer user_data)
{
	EShellWindow *shell_window = user_data;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (!e_shell_window_get_menubar_visible (shell_window))
		gtk_widget_hide (main_menu);
}

static GtkWidget *
shell_window_construct_menubar (EShellWindow *shell_window)
{
	GtkWidget *main_menu;

	main_menu = e_shell_window_get_managed_widget (
		shell_window, "/main-menu");

	g_signal_connect (main_menu, "deactivate",
		G_CALLBACK (shell_window_menubar_deactivate_cb), shell_window);

	e_binding_bind_property (
		shell_window, "menubar-visible",
		main_menu, "visible",
		G_BINDING_SYNC_CREATE);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (shell_window_menubar_update_new_menu), NULL);

	return main_menu;
}

static GtkWidget *
shell_window_construct_toolbar (EShellWindow *shell_window)
{
	GtkUIManager *ui_manager;
	GtkWidget *toolbar;
	GtkWidget *box;
	GtkToolItem *item;

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (box);

	e_binding_bind_property (
		shell_window, "toolbar-visible",
		box, "visible",
		G_BINDING_SYNC_CREATE);

	toolbar = e_shell_window_get_managed_widget (
		shell_window, "/main-toolbar");

	gtk_style_context_add_class (
		gtk_widget_get_style_context (toolbar),
		GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	/* XXX Having this separator in the UI definition doesn't work
	 *     because GtkUIManager is unaware of the "New" button, so
	 *     it makes the separator invisible.  One possibility is to
	 *     define a GtkAction subclass for which create_tool_item()
	 *     return an EMenuToolButton.  Then both this separator
	 *     and the "New" button could be added to the UI definition.
	 *     Tempting, but the "New" button and its dynamically
	 *     generated menu is already a complex beast, and I'm not
	 *     convinced having it proxy some new type of GtkAction
	 *     is worth the extra effort. */
	item = gtk_separator_tool_item_new ();
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, 0);
	gtk_widget_show (GTK_WIDGET (item));

	/* Translators: a 'New' toolbar button caption which is context sensitive and
	   runs one of the actions under File->New menu */
	item = e_menu_tool_button_new (C_("toolbar-button", "New"));
	gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
	gtk_widget_add_accelerator (
		GTK_WIDGET (item), "clicked",
		gtk_ui_manager_get_accel_group (ui_manager),
		GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, 0);
	gtk_widget_show (GTK_WIDGET (item));

	/* XXX The ECalShellBackend has a hack where it forces the
	 *     EMenuToolButton to update its button image by forcing
	 *     a "notify::active-view" signal emission on the window.
	 *     This will trigger the property binding, which will set
	 *     EMenuToolButton's "prefer-item" property, which will
	 *     invoke shell_window_toolbar_update_new_menu(), which
	 *     will cause EMenuToolButton to update its button image.
	 *
	 *     It's a bit of a Rube Goldberg machine and should be
	 *     reworked, but it's just serving one (now documented)
	 *     corner case and works for now. */
	e_binding_bind_property_full (
		shell_window, "active-view",
		item, "prefer-item",
		G_BINDING_SYNC_CREATE,
		shell_window_active_view_to_prefer_item,
		(GBindingTransformFunc) NULL,
		NULL, (GDestroyNotify) NULL);

	g_signal_connect (
		item, "notify::prefer-item",
		G_CALLBACK (shell_window_toolbar_update_new_menu),
		shell_window);

	gtk_box_pack_start (GTK_BOX (box), toolbar, TRUE, TRUE, 0);

	toolbar = e_shell_window_get_managed_widget (
		shell_window, "/search-toolbar");
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), FALSE);

	toolbar = e_shell_window_get_managed_widget (
		shell_window, "/close-toolbar");
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), FALSE);

	return box;
}

static GtkWidget *
shell_window_construct_sidebar (EShellWindow *shell_window)
{
	GtkWidget *notebook;
	GtkWidget *switcher;

	switcher = e_shell_switcher_new ();
	shell_window->priv->switcher = g_object_ref_sink (switcher);

	e_binding_bind_property (
		shell_window, "sidebar-visible",
		switcher, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell_window, "switcher-visible",
		switcher, "toolbar-visible",
		G_BINDING_SYNC_CREATE);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_container_add (GTK_CONTAINER (switcher), notebook);
	shell_window->priv->sidebar_notebook = g_object_ref (notebook);
	gtk_widget_show (notebook);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (shell_window_set_notebook_page), notebook);

	return switcher;
}

static GtkWidget *
shell_window_construct_content (EShellWindow *shell_window)
{
	GtkWidget *box;
	GtkWidget *widget;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (box);

	widget = e_alert_bar_new ();
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
	shell_window->priv->alert_bar = g_object_ref (widget);
	/* EAlertBar controls its own visibility. */

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
	shell_window->priv->content_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (shell_window_set_notebook_page), widget);

	return box;
}

static GtkWidget *
shell_window_construct_taskbar (EShellWindow *shell_window)
{
	EShell *shell;
	GtkWidget *box;
	GtkWidget *notebook;
	GtkWidget *status_area;
	GtkWidget *online_button;
	GtkWidget *tooltip_label;
	GtkStyleContext *style_context;
	gint height;

	shell = e_shell_window_get_shell (shell_window);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width (GTK_CONTAINER (box), 3);
	gtk_widget_show (box);

	status_area = gtk_frame_new (NULL);
	style_context = gtk_widget_get_style_context (status_area);
	gtk_style_context_add_class (style_context, "taskbar");
	gtk_container_add (GTK_CONTAINER (status_area), box);

	e_binding_bind_property (
		shell_window, "taskbar-visible",
		status_area, "visible",
		G_BINDING_SYNC_CREATE);

	/* Make the status area as large as the task bar. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (status_area, -1, (height * 2) + 6);

	online_button = e_online_button_new ();
	gtk_box_pack_start (
		GTK_BOX (box), online_button, FALSE, TRUE, 0);
	gtk_widget_show (online_button);

	e_binding_bind_property (
		shell, "online",
		online_button, "online",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell, "network-available",
		online_button, "sensitive",
		G_BINDING_SYNC_CREATE);

	g_signal_connect (
		online_button, "clicked",
		G_CALLBACK (shell_window_online_button_clicked_cb),
		shell_window);

	tooltip_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (tooltip_label), 0.0, 0.5);
	gtk_box_pack_start (
		GTK_BOX (box), tooltip_label, TRUE, TRUE, 0);
	shell_window->priv->tooltip_label = g_object_ref (tooltip_label);
	gtk_widget_hide (tooltip_label);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (box), notebook, TRUE, TRUE, 0);
	shell_window->priv->status_notebook = g_object_ref (notebook);
	gtk_widget_show (notebook);

	e_signal_connect_notify (
		shell_window, "notify::active-view",
		G_CALLBACK (shell_window_set_notebook_page), notebook);

	return status_area;
}

static EShellView *
shell_window_create_shell_view (EShellWindow *shell_window,
                                const gchar *view_name)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	GHashTable *loaded_views;
	GtkUIManager *ui_manager;
	GtkNotebook *notebook;
	GtkAction *action;
	GtkWidget *widget;
	const gchar *name;
	const gchar *id;
	gint page_num;
	GType type;

	shell = e_shell_window_get_shell (shell_window);
	shell_backend = e_shell_get_backend_by_name (shell, view_name);

	if (shell_backend == NULL) {
		g_critical ("Unknown shell view name: %s", view_name);
		return NULL;
	}

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
		type, "action", action, "page-num", page_num,
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

	e_binding_bind_property (
		widget, "height-request",
		shell_window->priv->tooltip_label, "height-request",
		G_BINDING_SYNC_CREATE);

	/* Listen for changes that affect the shell window. */

	e_signal_connect_notify_swapped (
		action, "notify::icon-name",
		G_CALLBACK (e_shell_window_update_icon), shell_window);

	e_signal_connect_notify_swapped (
		shell_view, "notify::title",
		G_CALLBACK (e_shell_window_update_title), shell_window);

	e_signal_connect_notify_swapped (
		shell_view, "notify::view-id",
		G_CALLBACK (e_shell_window_update_view_menu), shell_window);

	return shell_view;
}

static void
shell_window_submit_alert (EAlertSink *alert_sink,
                           EAlert *alert)
{
	EShellWindow *shell_window;
	GtkWidget *alert_bar;
	GtkWidget *dialog;

	shell_window = E_SHELL_WINDOW (alert_sink);

	if (!gtk_widget_get_mapped (GTK_WIDGET (shell_window)) ||
	    shell_window->priv->postponed_alerts) {
		shell_window->priv->postponed_alerts = g_slist_prepend (
			shell_window->priv->postponed_alerts, g_object_ref (alert));
		return;
	}

	alert_bar = e_shell_window_get_alert_bar (shell_window);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_ERROR:
			e_alert_bar_add_alert (
				E_ALERT_BAR (alert_bar), alert);
			break;

		default:
			dialog = e_alert_dialog_new (
				GTK_WINDOW (shell_window), alert);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			break;
	}
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

static gboolean
shell_window_map_event (GtkWidget *widget,
			GdkEventAny *event)
{
	EShellWindow *shell_window;
	gboolean res;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (widget), FALSE);

	shell_window = E_SHELL_WINDOW (widget);

	/* Chain up to parent's method */
	res = GTK_WIDGET_CLASS (e_shell_window_parent_class)->map_event (widget, event);

	g_idle_add_full (
		G_PRIORITY_LOW,
		shell_window_submit_postponed_alerts_idle_cb,
		g_object_ref (shell_window), g_object_unref);

	return res;
}

static void
e_shell_window_class_init (EShellWindowClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

	g_type_class_add_private (class, sizeof (EShellWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_window_set_property;
	object_class->get_property = shell_window_get_property;
	object_class->dispose = shell_window_dispose;
	object_class->finalize = shell_window_finalize;
	object_class->constructed = shell_window_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = shell_window_get_preferred_width;
	widget_class->map_event = shell_window_map_event;

	class->close_alert = shell_window_close_alert;
	class->construct_menubar = shell_window_construct_menubar;
	class->construct_toolbar = shell_window_construct_toolbar;
	class->construct_sidebar = shell_window_construct_sidebar;
	class->construct_content = shell_window_construct_content;
	class->construct_taskbar = shell_window_construct_taskbar;
	class->create_shell_view = shell_window_create_shell_view;

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
	 * EShellWindow:menubar-visible
	 *
	 * Whether the shell window's menu bar is visible.
	 *
	 * Since: 3.24
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MENUBAR_VISIBLE,
		g_param_spec_boolean (
			"menubar-visible",
			"Menubar Visible",
			"Whether the shell window's menu bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:sidebar-visible
	 *
	 * Whether the shell window's side bar is visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SIDEBAR_VISIBLE,
		g_param_spec_boolean (
			"sidebar-visible",
			"Sidebar Visible",
			"Whether the shell window's side bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:switcher-visible
	 *
	 * Whether the shell window's switcher buttons are visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SWITCHER_VISIBLE,
		g_param_spec_boolean (
			"switcher-visible",
			"Switcher Visible",
			"Whether the shell window's "
			"switcher buttons are visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:taskbar-visible
	 *
	 * Whether the shell window's task bar is visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TASKBAR_VISIBLE,
		g_param_spec_boolean (
			"taskbar-visible",
			"Taskbar Visible",
			"Whether the shell window's task bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellWindow:toolbar-visible
	 *
	 * Whether the shell window's tool bar is visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"toolbar-visible",
			"Toolbar Visible",
			"Whether the shell window's tool bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

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
			"UI Manager",
			"The shell window's GtkUIManager",
			GTK_TYPE_UI_MANAGER,
			G_PARAM_READABLE |
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

	shell_window->priv = E_SHELL_WINDOW_GET_PRIVATE (shell_window);

	e_shell_window_private_init (shell_window);

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider, css, -1, NULL);
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
		GTK_STYLE_PROVIDER (css_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
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
 * The function emits a #EShellWindow::shell-view-created signal with
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
	EShellWindowClass *class;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), NULL);
	g_return_val_if_fail (view_name != NULL, NULL);

	shell_view = e_shell_window_peek_shell_view (shell_window, view_name);
	if (shell_view != NULL)
		return shell_view;

	class = E_SHELL_WINDOW_GET_CLASS (shell_window);
	g_return_val_if_fail (class->create_shell_view != NULL, NULL);

	shell_view = class->create_shell_view (shell_window, view_name);

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

	action_name = g_strdup_printf (E_SHELL_SWITCHER_FORMAT, view_name);
	action = e_shell_window_get_action (shell_window, action_name);
	g_free (action_name);

	return action;
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
	GtkAction *action;
	EShellView *shell_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (view_name != NULL);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	action = e_shell_view_get_action (shell_view);
	gtk_action_activate (action);

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
 * e_shell_window_get_menubar_visible:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window<!-- -->'s menu bar is visible.
 *
 * Returns: %TRUE is the menu bar is visible
 *
 * Since: 3.24
 **/
gboolean
e_shell_window_get_menubar_visible (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->menubar_visible;
}

/**
 * e_shell_window_set_menubar_visible:
 * @shell_window: an #EShellWindow
 * @menubar_visible: whether the menu bar should be visible
 *
 * Makes @shell_window<!-- -->'s menu bar visible or invisible.
 *
 * Since: 3.24
 **/
void
e_shell_window_set_menubar_visible (EShellWindow *shell_window,
				    gboolean menubar_visible)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_window->priv->menubar_visible == menubar_visible)
		return;

	shell_window->priv->menubar_visible = menubar_visible;

	g_object_notify (G_OBJECT (shell_window), "menubar-visible");
}

/**
 * e_shell_window_get_sidebar_visible:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window<!-- -->'s side bar is visible.
 *
 * Returns: %TRUE is the side bar is visible
 **/
gboolean
e_shell_window_get_sidebar_visible (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->sidebar_visible;
}

/**
 * e_shell_window_set_sidebar_visible:
 * @shell_window: an #EShellWindow
 * @sidebar_visible: whether the side bar should be visible
 *
 * Makes @shell_window<!-- -->'s side bar visible or invisible.
 **/
void
e_shell_window_set_sidebar_visible (EShellWindow *shell_window,
                                    gboolean sidebar_visible)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_window->priv->sidebar_visible == sidebar_visible)
		return;

	shell_window->priv->sidebar_visible = sidebar_visible;

	g_object_notify (G_OBJECT (shell_window), "sidebar-visible");
}

/**
 * e_shell_window_get_switcher_visible:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window<!-- -->'s switcher buttons are visible.
 *
 * Returns: %TRUE is the switcher buttons are visible
 **/
gboolean
e_shell_window_get_switcher_visible (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->switcher_visible;
}

/**
 * e_shell_window_set_switcher_visible:
 * @shell_window: an #EShellWindow
 * @switcher_visible: whether the switcher buttons should be visible
 *
 * Makes @shell_window<!-- -->'s switcher buttons visible or invisible.
 **/
void
e_shell_window_set_switcher_visible (EShellWindow *shell_window,
                                     gboolean switcher_visible)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_window->priv->switcher_visible == switcher_visible)
		return;

	shell_window->priv->switcher_visible = switcher_visible;

	g_object_notify (G_OBJECT (shell_window), "switcher-visible");
}

/**
 * e_shell_window_get_taskbar_visible:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window<!-- -->'s task bar is visible.
 *
 * Returns: %TRUE is the task bar is visible
 **/
gboolean
e_shell_window_get_taskbar_visible (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->taskbar_visible;
}

/**
 * e_shell_window_set_taskbar_visible:
 * @shell_window: an #EShellWindow
 * @taskbar_visible: whether the task bar should be visible
 *
 * Makes @shell_window<!-- -->'s task bar visible or invisible.
 **/
void
e_shell_window_set_taskbar_visible (EShellWindow *shell_window,
                                    gboolean taskbar_visible)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_window->priv->taskbar_visible == taskbar_visible)
		return;

	shell_window->priv->taskbar_visible = taskbar_visible;

	g_object_notify (G_OBJECT (shell_window), "taskbar-visible");
}

/**
 * e_shell_window_get_toolbar_visible:
 * @shell_window: an #EShellWindow
 *
 * Returns %TRUE if @shell_window<!-- -->'s tool bar is visible.
 *
 * Returns: %TRUE if the tool bar is visible
 **/
gboolean
e_shell_window_get_toolbar_visible (EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	return shell_window->priv->toolbar_visible;
}

/**
 * e_shell_window_set_toolbar_visible:
 * @shell_window: an #EShellWindow
 * @toolbar_visible: whether the tool bar should be visible
 *
 * Makes @shell_window<!-- -->'s tool bar visible or invisible.
 **/
void
e_shell_window_set_toolbar_visible (EShellWindow *shell_window,
                                    gboolean toolbar_visible)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (shell_window->priv->toolbar_visible == toolbar_visible)
		return;

	shell_window->priv->toolbar_visible = toolbar_visible;

	g_object_notify (G_OBJECT (shell_window), "toolbar-visible");
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

		g_free (cc_data);
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
		cc_data->source, cc_data->extension_name, 30, cancellable, &local_error);

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

	cc_data = g_new0 (ConnectClientData, 1);
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
 * #GtkApplication::window-added signal handler.  The #EShellBackend
 * calling this function should pass its own name for the @backend_name
 * argument (i.e. the <structfield>name</structfield> field from its own
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
 * #GtkApplication::window-added signal handler.  The #EShellBackend
 * calling this function should pass its own name for the @backend_name
 * argument (i.e. the <structfield>name</structfield> field from its own
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
}

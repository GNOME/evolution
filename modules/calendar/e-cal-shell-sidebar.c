/*
 * e-cal-shell-sidebar.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "calendar/gui/e-calendar-selector.h"
#include "calendar/gui/misc.h"

#include "e-cal-shell-content.h"
#include "e-cal-shell-sidebar.h"

#define E_CAL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarPrivate))

typedef struct _ConnectClosure ConnectClosure;

struct _ECalShellSidebarPrivate {
	GtkWidget *paned;
	GtkWidget *selector;
	GtkWidget *date_navigator;

	/* The default client is for ECalModel.  It follows the
	 * sidebar's primary selection, even if the highlighted
	 * source is not selected.  The tricky part is we don't
	 * update the property until the client is successfully
	 * opened.  So the user first highlights a source, then
	 * sometime later we update our default-client property
	 * which is bound by an EBinding to ECalModel. */
	EClient *default_client;

	/* Not referenced, only for pointer comparison. */
	ESource *connecting_default_source_instance;

	EActivity *connecting_default_client;
};

struct _ConnectClosure {
	ECalShellSidebar *cal_shell_sidebar;
	EActivity *activity;

	/* For error messages. */
	gchar *unique_display_name;
};

enum {
	PROP_0,
	PROP_DATE_NAVIGATOR,
	PROP_DEFAULT_CLIENT,
	PROP_SELECTOR
};

enum {
	CLIENT_ADDED,
	CLIENT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_DYNAMIC_TYPE (
	ECalShellSidebar,
	e_cal_shell_sidebar,
	E_TYPE_SHELL_SIDEBAR)

static ConnectClosure *
connect_closure_new (ECalShellSidebar *cal_shell_sidebar,
                     ESource *source)
{
	ConnectClosure *closure;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	gchar *text;

	shell_sidebar = E_SHELL_SIDEBAR (cal_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	registry = e_source_selector_get_registry (selector);

	closure = g_slice_new0 (ConnectClosure);
	closure->cal_shell_sidebar = g_object_ref (cal_shell_sidebar);
	closure->activity = e_activity_new ();
	closure->unique_display_name =
		e_source_registry_dup_unique_display_name (
		registry, source, E_SOURCE_EXTENSION_CALENDAR);

	text = g_strdup_printf (
		_("Opening calendar '%s'"),
		closure->unique_display_name);
	e_activity_set_text (closure->activity, text);
	g_free (text);

	alert_sink = E_ALERT_SINK (shell_content);
	e_activity_set_alert_sink (closure->activity, alert_sink);

	cancellable = g_cancellable_new ();
	e_activity_set_cancellable (closure->activity, cancellable);
	g_object_unref (cancellable);

	e_shell_backend_add_activity (shell_backend, closure->activity);

	return closure;
}

static void
connect_closure_free (ConnectClosure *closure)
{
	g_clear_object (&closure->cal_shell_sidebar);
	g_clear_object (&closure->activity);

	g_free (closure->unique_display_name);

	g_slice_free (ConnectClosure, closure);
}

static gboolean
cal_shell_sidebar_map_uid_to_source (GValue *value,
                                     GVariant *variant,
                                     gpointer user_data)
{
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	registry = E_SOURCE_REGISTRY (user_data);
	uid = g_variant_get_string (variant, NULL);
	if (uid != NULL && *uid != '\0')
		source = e_source_registry_ref_source (registry, uid);
	else
		source = e_source_registry_ref_default_calendar (registry);
	g_value_take_object (value, source);

	return (source != NULL);
}

static GVariant *
cal_shell_sidebar_map_source_to_uid (const GValue *value,
                                     const GVariantType *expected_type,
                                     gpointer user_data)
{
	GVariant *variant = NULL;
	ESource *source;

	source = g_value_get_object (value);

	if (source != NULL) {
		const gchar *uid;

		uid = e_source_get_uid (source);
		variant = g_variant_new_string (uid);
	}

	return variant;
}

static void
cal_shell_sidebar_emit_client_added (ECalShellSidebar *cal_shell_sidebar,
                                     EClient *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (cal_shell_sidebar, signal_id, 0, client);
}

static void
cal_shell_sidebar_emit_client_removed (ECalShellSidebar *cal_shell_sidebar,
                                       EClient *client)
{
	guint signal_id = signals[CLIENT_REMOVED];

	g_signal_emit (cal_shell_sidebar, signal_id, 0, client);
}

static void
cal_shell_sidebar_handle_connect_error (EActivity *activity,
                                        const gchar *unique_display_name,
                                        const GError *error)
{
	EAlertSink *alert_sink;
	gboolean offline_error;

	alert_sink = e_activity_get_alert_sink (activity);

	offline_error = g_error_matches (
		error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE);

	if (e_activity_handle_cancellation (activity, error)) {
		/* do nothing */
	} else if (offline_error) {
		e_alert_submit (
			alert_sink,
			"calendar:prompt-no-contents-offline-calendar",
			unique_display_name,
			NULL);
	} else {
		e_alert_submit (
			alert_sink,
			"calendar:failed-open-calendar",
			unique_display_name,
			error->message,
			NULL);
	}
}

static void
cal_shell_sidebar_client_connect_cb (GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
	EClient *client;
	ConnectClosure *closure = user_data;
	GError *error = NULL;

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		cal_shell_sidebar_handle_connect_error (
			closure->activity,
			closure->unique_display_name,
			error);
		g_error_free (error);
		goto exit;
	}

	e_activity_set_state (closure->activity, E_ACTIVITY_COMPLETED);

	e_cal_shell_sidebar_add_client (closure->cal_shell_sidebar, client);

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
cal_shell_sidebar_default_connect_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	EClient *client;
	ESource *source;
	ConnectClosure *closure = user_data;
	ECalShellSidebarPrivate *priv;
	GError *error = NULL;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (closure->cal_shell_sidebar);

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	g_clear_object (&priv->connecting_default_client);

	if (error != NULL) {
		cal_shell_sidebar_handle_connect_error (
			closure->activity,
			closure->unique_display_name,
			error);
		g_error_free (error);
		goto exit;
	}

	e_activity_set_state (closure->activity, E_ACTIVITY_COMPLETED);

	source = e_client_get_source (client);

	if (source == priv->connecting_default_source_instance)
		priv->connecting_default_source_instance = NULL;

	if (priv->default_client != NULL)
		g_object_unref (priv->default_client);

	priv->default_client = g_object_ref (client);

	g_object_notify (
		G_OBJECT (closure->cal_shell_sidebar), "default-client");

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
cal_shell_sidebar_set_default (ECalShellSidebar *cal_shell_sidebar,
                               ESource *source)
{
	ECalShellSidebarPrivate *priv;
	ESourceSelector *selector;
	ConnectClosure *closure;

	priv = cal_shell_sidebar->priv;

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	/* already loading that source as default source */
	if (source == priv->connecting_default_source_instance)
		return;

	/* Cancel the previous request if unfinished. */
	if (priv->connecting_default_client != NULL) {
		e_activity_cancel (priv->connecting_default_client);
		g_object_unref (priv->connecting_default_client);
		priv->connecting_default_client = NULL;
	}

	closure = connect_closure_new (cal_shell_sidebar, source);

	/* it's only for pointer comparison, no need to ref it */
	priv->connecting_default_source_instance = source;
	priv->connecting_default_client = g_object_ref (closure->activity);

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source,
		e_activity_get_cancellable (closure->activity),
		cal_shell_sidebar_default_connect_cb, closure);
}

static void
cal_shell_sidebar_row_changed_cb (ECalShellSidebar *cal_shell_sidebar,
                                  GtkTreePath *tree_path,
                                  GtkTreeIter *tree_iter,
                                  GtkTreeModel *tree_model)
{
	ESourceSelector *selector;
	ESource *source;

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_ref_source_by_path (selector, tree_path);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (source == NULL)
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_cal_shell_sidebar_add_source (cal_shell_sidebar, source);
	else
		e_cal_shell_sidebar_remove_source (cal_shell_sidebar, source);

	g_object_unref (source);
}

static void
cal_shell_sidebar_primary_selection_changed_cb (ECalShellSidebar *cal_shell_sidebar,
                                                ESourceSelector *selector)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	cal_shell_sidebar_set_default (cal_shell_sidebar, source);

	g_object_unref (source);
}

static void
cal_shell_sidebar_restore_state_cb (EShellWindow *shell_window,
                                    EShellView *shell_view,
                                    EShellSidebar *shell_sidebar)
{
	ECalShellSidebarPrivate *priv;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GSettings *settings;
	GtkTreeModel *model;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	g_signal_handlers_disconnect_by_func (
		shell_window,
		cal_shell_sidebar_restore_state_cb, shell_sidebar);

	selector = E_SOURCE_SELECTOR (priv->selector);
	registry = e_source_selector_get_registry (selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	g_signal_connect_swapped (
		registry, "source-removed",
		G_CALLBACK (e_cal_shell_sidebar_remove_source), shell_sidebar);

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (cal_shell_sidebar_row_changed_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (cal_shell_sidebar_primary_selection_changed_cb),
		shell_sidebar);

	/* This will trigger our "row-changed" signal handler for each
	 * calendar source, so the appropriate ECalClients get added to
	 * the ECalModel, which will then create view objects to display
	 * the calendar content.  This all happens asynchronously. */
	e_source_selector_update_all_rows (selector);

	/* Bind GObject properties to settings keys. */

	settings = g_settings_new ("org.gnome.evolution.calendar");

	g_settings_bind_with_mapping (
		settings, "primary-calendar",
		selector, "primary-selection",
		G_SETTINGS_BIND_DEFAULT,
		cal_shell_sidebar_map_uid_to_source,
		cal_shell_sidebar_map_source_to_uid,
		g_object_ref (registry),
		(GDestroyNotify) g_object_unref);

	g_settings_bind (
		settings, "date-navigator-pane-position",
		priv->paned, "vposition",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
}

static void
cal_shell_sidebar_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATE_NAVIGATOR:
			g_value_set_object (
				value,
				e_cal_shell_sidebar_get_date_navigator (
				E_CAL_SHELL_SIDEBAR (object)));
			return;

		case PROP_DEFAULT_CLIENT:
			g_value_set_object (
				value,
				e_cal_shell_sidebar_get_default_client (
				E_CAL_SHELL_SIDEBAR (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value,
				e_cal_shell_sidebar_get_selector (
				E_CAL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_sidebar_dispose (GObject *object)
{
	ECalShellSidebarPrivate *priv;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	if (priv->date_navigator != NULL) {
		g_object_unref (priv->date_navigator);
		priv->date_navigator = NULL;
	}

	if (priv->default_client != NULL) {
		g_object_unref (priv->default_client);
		priv->default_client = NULL;
	}

	if (priv->connecting_default_client != NULL) {
		e_activity_cancel (priv->connecting_default_client);
		g_object_unref (priv->connecting_default_client);
		priv->connecting_default_client = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_shell_sidebar_parent_class)->dispose (object);
}

static void
cal_shell_sidebar_constructed (GObject *object)
{
	ECalShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EClientCache *client_cache;
	ECalendarItem *calitem;
	GtkWidget *container;
	GtkWidget *widget;
	AtkObject *a11y;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_sidebar_parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	container = GTK_WIDGET (shell_sidebar);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, TRUE);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	client_cache = e_shell_get_client_cache (shell);
	widget = e_calendar_selector_new (client_cache, shell_view);
	gtk_container_add (GTK_CONTAINER (container), widget);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("Calendar Selector"));
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->paned;

	widget = e_calendar_new ();
	calitem = E_CALENDAR (widget)->calitem;
	e_calendar_item_set_days_start_week_sel (calitem, 9);
	e_calendar_item_set_max_days_sel (calitem, 42);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->date_navigator = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Restore widget state from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::calendar",
		G_CALLBACK (cal_shell_sidebar_restore_state_cb),
		shell_sidebar);
}

static guint32
cal_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	ECalShellSidebar *cal_shell_sidebar;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source;
	gboolean is_writable = FALSE;
	gboolean is_removable = FALSE;
	gboolean is_remote_creatable = FALSE;
	gboolean is_remote_deletable = FALSE;
	gboolean in_collection = FALSE;
	gboolean refresh_supported = FALSE;
	gboolean has_primary_source = FALSE;
	guint32 state = 0;

	cal_shell_sidebar = E_CAL_SHELL_SIDEBAR (shell_sidebar);
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	registry = e_source_selector_get_registry (selector);

	if (source != NULL) {
		EClient *client;
		ESource *collection;

		has_primary_source = TRUE;
		is_writable = e_source_get_writable (source);
		is_removable = e_source_get_removable (source);
		is_remote_creatable = e_source_get_remote_creatable (source);
		is_remote_deletable = e_source_get_remote_deletable (source);

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			in_collection = TRUE;
			g_object_unref (collection);
		}

		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);

		if (client != NULL) {
			refresh_supported =
				e_client_check_refresh_supported (client);
			g_object_unref (client);
		}

		g_object_unref (source);
	}

	if (has_primary_source)
		state |= E_CAL_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (is_writable)
		state |= E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE;
	if (is_removable)
		state |= E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE;
	if (is_remote_creatable)
		state |= E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE;
	if (is_remote_deletable)
		state |= E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE;
	if (in_collection)
		state |= E_CAL_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION;
	if (refresh_supported)
		state |= E_CAL_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
cal_shell_sidebar_client_removed (ECalShellSidebar *cal_shell_sidebar,
                                  ECalClient *client)
{
	ESourceSelector *selector;
	ESource *source;

	source = e_client_get_source (E_CLIENT (client));

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	e_source_selector_unselect_source (selector, source);
}

static void
e_cal_shell_sidebar_class_init (ECalShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	g_type_class_add_private (class, sizeof (ECalShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_sidebar_get_property;
	object_class->dispose = cal_shell_sidebar_dispose;
	object_class->constructed = cal_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = cal_shell_sidebar_check_state;

	class->client_removed = cal_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_DATE_NAVIGATOR,
		g_param_spec_object (
			"date-navigator",
			"Date Navigator Widget",
			"This widget displays a miniature calendar",
			E_TYPE_CALENDAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default Calendar ECalClient",
			"Default client for calendar operations",
			E_TYPE_CAL_CLIENT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			"Source Selector Widget",
			"This widget displays groups of calendars",
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	signals[CLIENT_ADDED] = g_signal_new (
		"client-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalShellSidebarClass, client_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);
}

static void
e_cal_shell_sidebar_class_finalize (ECalShellSidebarClass *class)
{
}

static void
e_cal_shell_sidebar_init (ECalShellSidebar *cal_shell_sidebar)
{
	cal_shell_sidebar->priv =
		E_CAL_SHELL_SIDEBAR_GET_PRIVATE (cal_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

void
e_cal_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_cal_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ECalendar *
e_cal_shell_sidebar_get_date_navigator (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return E_CALENDAR (cal_shell_sidebar->priv->date_navigator);
}

ECalClient *
e_cal_shell_sidebar_get_default_client (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return (ECalClient *) cal_shell_sidebar->priv->default_client;
}

ESourceSelector *
e_cal_shell_sidebar_get_selector (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (cal_shell_sidebar->priv->selector);
}

void
e_cal_shell_sidebar_add_client (ECalShellSidebar *cal_shell_sidebar,
                                EClient *client)
{
	ESource *source;
	ESourceSelector *selector;

	g_return_if_fail (E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	source = e_client_get_source (client);

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	e_source_selector_select_source (selector, source);

	cal_shell_sidebar_emit_client_added (cal_shell_sidebar, client);
}

void
e_cal_shell_sidebar_add_source (ECalShellSidebar *cal_shell_sidebar,
                                ESource *source)
{
	ESourceSelector *selector;
	ConnectClosure *closure;

	g_return_if_fail (E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	e_source_selector_select_source (selector, source);

	closure = connect_closure_new (cal_shell_sidebar, source);

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source,
		e_activity_get_cancellable (closure->activity),
		cal_shell_sidebar_client_connect_cb, closure);
}

void
e_cal_shell_sidebar_remove_source (ECalShellSidebar *cal_shell_sidebar,
                                   ESource *source)
{
	ESourceSelector *selector;
	EClient *client;

	g_return_if_fail (E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	client = e_client_selector_ref_cached_client (E_CLIENT_SELECTOR (selector), source);
	if (!client) {
		EShellView *shell_view;
		EShellContent *shell_content;
		ECalModel *model;
		GList *clients, *link;

		shell_view = e_shell_sidebar_get_shell_view (E_SHELL_SIDEBAR (cal_shell_sidebar));
		shell_content = e_shell_view_get_shell_content (shell_view);
		model = e_cal_shell_content_get_model (E_CAL_SHELL_CONTENT (shell_content));

		clients = e_cal_model_list_clients (model);
		for (link = clients; link; link = g_list_next (link)) {
			EClient *adept = link->data;

			if (adept && g_strcmp0 (e_source_get_uid (source), e_source_get_uid (e_client_get_source (adept))) == 0) {
				client = g_object_ref (adept);
				break;
			}
		}

		g_list_free_full (clients, g_object_unref);
	}

	if (client != NULL) {
		cal_shell_sidebar_emit_client_removed (
			cal_shell_sidebar, client);
		g_object_unref (client);
	}
}

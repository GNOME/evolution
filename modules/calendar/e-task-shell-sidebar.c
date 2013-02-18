/*
 * e-task-shell-sidebar.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-task-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "calendar/gui/e-task-list-selector.h"
#include "calendar/gui/misc.h"

#include "e-task-shell-view.h"
#include "e-task-shell-backend.h"
#include "e-task-shell-content.h"

#define E_TASK_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarPrivate))

typedef struct _ConnectClosure ConnectClosure;

struct _ETaskShellSidebarPrivate {
	GtkWidget *selector;

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

	GCancellable *connecting_default_client;
	GCancellable *loading_clients;
};

struct _ConnectClosure {
	ETaskShellSidebar *task_shell_sidebar;

	/* For error messages. */
	gchar *source_display_name;
	gchar *parent_display_name;
};

enum {
	PROP_0,
	PROP_DEFAULT_CLIENT,
	PROP_SELECTOR
};

enum {
	CLIENT_ADDED,
	CLIENT_REMOVED,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_DYNAMIC_TYPE (
	ETaskShellSidebar,
	e_task_shell_sidebar,
	E_TYPE_SHELL_SIDEBAR)

static ConnectClosure *
connect_closure_new (ETaskShellSidebar *task_shell_sidebar,
                     ESource *source)
{
	ConnectClosure *closure;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	ESource *parent;
	const gchar *parent_uid;

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	registry = e_source_selector_get_registry (selector);
	parent_uid = e_source_get_parent (source);
	parent = e_source_registry_ref_source (registry, parent_uid);

	closure = g_slice_new0 (ConnectClosure);
	closure->task_shell_sidebar = g_object_ref (task_shell_sidebar);
	closure->source_display_name = e_source_dup_display_name (source);
	closure->parent_display_name = e_source_dup_display_name (parent);

	g_object_unref (parent);

	return closure;
}

static void
connect_closure_free (ConnectClosure *closure)
{
	g_object_unref (closure->task_shell_sidebar);

	g_free (closure->source_display_name);
	g_free (closure->parent_display_name);

	g_slice_free (ConnectClosure, closure);
}

static void
task_shell_sidebar_emit_client_added (ETaskShellSidebar *task_shell_sidebar,
                                      EClient *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (task_shell_sidebar, signal_id, 0, client);
}

static void
task_shell_sidebar_emit_client_removed (ETaskShellSidebar *task_shell_sidebar,
                                        EClient *client)
{
	guint signal_id = signals[CLIENT_REMOVED];

	g_signal_emit (task_shell_sidebar, signal_id, 0, client);
}

static void
task_shell_sidebar_emit_status_message (ETaskShellSidebar *task_shell_sidebar,
                                        const gchar *status_message)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (task_shell_sidebar, signal_id, 0, status_message, -1.0);
}

static void
task_shell_sidebar_handle_connect_error (ETaskShellSidebar *task_shell_sidebar,
                                         const gchar *parent_display_name,
                                         const gchar *source_display_name,
                                         const GError *error)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	gboolean cancelled = FALSE;
	gboolean offline_error;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);

	cancelled |= g_error_matches (
		error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	cancelled |= g_error_matches (
		error, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED);

	offline_error = g_error_matches (
		error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE);

	if (cancelled) {
		/* do nothing */
	} else if (offline_error) {
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"calendar:prompt-no-contents-offline-calendar",
			parent_display_name,
			source_display_name,
			NULL);
	} else {
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"calendar:failed-open-calendar",
			parent_display_name,
			source_display_name,
			error->message,
			NULL);
	}
}

static void
task_shell_sidebar_client_connect_cb (GObject *source_object,
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
		task_shell_sidebar_handle_connect_error (
			closure->task_shell_sidebar,
			closure->parent_display_name,
			closure->source_display_name,
			error);
		g_error_free (error);
		goto exit;
	}

	e_task_shell_sidebar_add_client (closure->task_shell_sidebar, client);

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
task_shell_sidebar_default_connect_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EClient *client;
	ESource *source;
	ConnectClosure *closure = user_data;
	ETaskShellSidebarPrivate *priv;
	GError *error = NULL;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (closure->task_shell_sidebar);

	client = e_client_selector_get_client_finish (
		E_CLIENT_SELECTOR (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (priv->connecting_default_client) {
		g_object_unref (priv->connecting_default_client);
		priv->connecting_default_client = NULL;
	}

	if (error != NULL) {
		task_shell_sidebar_handle_connect_error (
			closure->task_shell_sidebar,
			closure->parent_display_name,
			closure->source_display_name,
			error);
		g_error_free (error);
		goto exit;
	}

	source = e_client_get_source (client);

	if (source == priv->connecting_default_source_instance)
		priv->connecting_default_source_instance = NULL;

	if (priv->default_client != NULL)
		g_object_unref (priv->default_client);

	priv->default_client = g_object_ref (client);

	g_object_notify (
		G_OBJECT (closure->task_shell_sidebar), "default-client");

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
task_shell_sidebar_set_default (ETaskShellSidebar *task_shell_sidebar,
                                ESource *source)
{
	ETaskShellSidebarPrivate *priv;
	ESourceSelector *selector;

	priv = task_shell_sidebar->priv;

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	/* already loading that source as default source */
	if (source == priv->connecting_default_source_instance)
		return;

	/* Cancel any unfinished previous request. */
	if (priv->connecting_default_client != NULL) {
		g_cancellable_cancel (priv->connecting_default_client);
		g_object_unref (priv->connecting_default_client);
		priv->connecting_default_client = NULL;
	}

	/* it's only for pointer comparison, no need to ref it */
	priv->connecting_default_source_instance = source;
	priv->connecting_default_client = g_cancellable_new ();

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source,
		priv->connecting_default_client,
		task_shell_sidebar_default_connect_cb,
		connect_closure_new (task_shell_sidebar, source));
}

static void
task_shell_sidebar_row_changed_cb (ETaskShellSidebar *task_shell_sidebar,
                                   GtkTreePath *tree_path,
                                   GtkTreeIter *tree_iter,
                                   GtkTreeModel *tree_model)
{
	ESourceSelector *selector;
	ESource *source;

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	source = e_source_selector_ref_source_by_path (selector, tree_path);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (source == NULL)
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_task_shell_sidebar_add_source (task_shell_sidebar, source);
	else
		e_task_shell_sidebar_remove_source (task_shell_sidebar, source);

	g_object_unref (source);
}

static void
task_shell_sidebar_primary_selection_changed_cb (ETaskShellSidebar *task_shell_sidebar,
                                                 ESourceSelector *selector)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	task_shell_sidebar_set_default (task_shell_sidebar, source);

	g_object_unref (source);
}

static void
task_shell_sidebar_restore_state_cb (EShellWindow *shell_window,
                                     EShellView *shell_view,
                                     EShellSidebar *shell_sidebar)
{
	ETaskShellSidebarPrivate *priv;
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GtkTreeModel *model;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	g_return_if_fail (E_IS_TASK_SHELL_BACKEND (shell_backend));

	selector = E_SOURCE_SELECTOR (priv->selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	registry = e_shell_get_registry (shell);

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (task_shell_sidebar_row_changed_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (task_shell_sidebar_primary_selection_changed_cb),
		shell_sidebar);

	g_object_bind_property_full (
		shell_settings, "cal-primary-task-list",
		selector, "primary-selection",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		(GBindingTransformFunc) e_binding_transform_uid_to_source,
		(GBindingTransformFunc) e_binding_transform_source_to_uid,
		g_object_ref (registry),
		(GDestroyNotify) g_object_unref);
}

static void
task_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_CLIENT:
			g_value_set_object (
				value,
				e_task_shell_sidebar_get_default_client (
				E_TASK_SHELL_SIDEBAR (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value,
				e_task_shell_sidebar_get_selector (
				E_TASK_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_sidebar_dispose (GObject *object)
{
	ETaskShellSidebarPrivate *priv;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	if (priv->default_client != NULL) {
		g_object_unref (priv->default_client);
		priv->default_client = NULL;
	}

	if (priv->connecting_default_client != NULL) {
		g_cancellable_cancel (priv->connecting_default_client);
		g_object_unref (priv->connecting_default_client);
		priv->connecting_default_client = NULL;
	}

	if (priv->loading_clients != NULL) {
		g_cancellable_cancel (priv->loading_clients);
		g_object_unref (priv->loading_clients);
		priv->loading_clients = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_task_shell_sidebar_parent_class)->dispose (object);
}

static void
task_shell_sidebar_constructed (GObject *object)
{
	ETaskShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EClientCache *client_cache;
	GtkContainer *container;
	GtkWidget *widget;
	AtkObject *a11y;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_task_shell_sidebar_parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	client_cache = e_shell_get_client_cache (shell);
	widget = e_task_list_selector_new (client_cache);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("Task List Selector"));
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Restore widget state from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::tasks",
		G_CALLBACK (task_shell_sidebar_restore_state_cb),
		shell_sidebar);
}

static guint32
task_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	ETaskShellSidebar *task_shell_sidebar;
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

	task_shell_sidebar = E_TASK_SHELL_SIDEBAR (shell_sidebar);
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
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
		state |= E_TASK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (is_writable)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE;
	if (is_removable)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE;
	if (is_remote_creatable)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE;
	if (is_remote_deletable)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE;
	if (in_collection)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION;
	if (refresh_supported)
		state |= E_TASK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
task_shell_sidebar_client_removed (ETaskShellSidebar *task_shell_sidebar,
                                   ECalClient *client)
{
	ESourceSelector *selector;
	ESource *source;

	source = e_client_get_source (E_CLIENT (client));

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	e_source_selector_unselect_source (selector, source);

	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);
}

static void
e_task_shell_sidebar_class_init (ETaskShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	g_type_class_add_private (class, sizeof (ETaskShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = task_shell_sidebar_get_property;
	object_class->dispose = task_shell_sidebar_dispose;
	object_class->constructed = task_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = task_shell_sidebar_check_state;

	class->client_removed = task_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default Task ECalClient",
			"Default client for task operations",
			E_TYPE_CAL_CLIENT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			"Source Selector Widget",
			"This widget displays groups of task lists",
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	signals[CLIENT_ADDED] = g_signal_new (
		"client-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETaskShellSidebarClass, client_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETaskShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ETaskShellSidebarClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_DOUBLE);
}

static void
e_task_shell_sidebar_class_finalize (ETaskShellSidebarClass *class)
{
}

static void
e_task_shell_sidebar_init (ETaskShellSidebar *task_shell_sidebar)
{
	task_shell_sidebar->priv =
		E_TASK_SHELL_SIDEBAR_GET_PRIVATE (task_shell_sidebar);

	task_shell_sidebar->priv->loading_clients = g_cancellable_new ();

	/* Postpone widget construction until we have a shell view. */
}

void
e_task_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_task_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_task_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_TASK_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ECalClient *
e_task_shell_sidebar_get_default_client (ETaskShellSidebar *task_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	return (ECalClient *) task_shell_sidebar->priv->default_client;
}

ESourceSelector *
e_task_shell_sidebar_get_selector (ETaskShellSidebar *task_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (task_shell_sidebar->priv->selector);
}

void
e_task_shell_sidebar_add_client (ETaskShellSidebar *task_shell_sidebar,
                                 EClient *client)
{
	ESource *source;
	ESourceSelector *selector;
	const gchar *message;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	source = e_client_get_source (client);

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	e_source_selector_select_source (selector, source);

	message = _("Loading task list");
	task_shell_sidebar_emit_status_message (task_shell_sidebar, message);
	task_shell_sidebar_emit_client_added (task_shell_sidebar, client);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);
}

void
e_task_shell_sidebar_add_source (ETaskShellSidebar *task_shell_sidebar,
                                 ESource *source)
{
	ESourceSelector *selector;
	const gchar *display_name;
	gchar *message;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	e_source_selector_select_source (selector, source);

	display_name = e_source_get_display_name (source);
	message = g_strdup_printf (_("Opening task list '%s'"), display_name);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, message);
	g_free (message);

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source,
		task_shell_sidebar->priv->loading_clients,
		task_shell_sidebar_client_connect_cb,
		connect_closure_new (task_shell_sidebar, source));
}

void
e_task_shell_sidebar_remove_source (ETaskShellSidebar *task_shell_sidebar,
                                    ESource *source)
{
	ESourceSelector *selector;
	EClient *client;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	client = e_client_selector_ref_cached_client (
		E_CLIENT_SELECTOR (selector), source);

	if (client != NULL) {
		task_shell_sidebar_emit_client_removed (
			task_shell_sidebar, client);
		g_object_unref (client);
	}
}

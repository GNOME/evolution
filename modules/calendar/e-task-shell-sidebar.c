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
#include <libedataserverui/libedataserverui.h>

#include "libevolution-utils/e-alert-dialog.h"
#include "e-util/e-util.h"
#include "calendar/gui/e-task-list-selector.h"
#include "calendar/gui/misc.h"

#include "e-task-shell-view.h"
#include "e-task-shell-backend.h"
#include "e-task-shell-content.h"

#define E_TASK_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarPrivate))

struct _ETaskShellSidebarPrivate {
	GtkWidget *selector;

	/* UID -> Client */
	GHashTable *client_table;

	/* The default client is for ECalModel.  It follows the
	 * sidebar's primary selection, even if the highlighted
	 * source is not selected.  The tricky part is we don't
	 * update the property until the client is successfully
	 * opened.  So the user first highlights a source, then
	 * sometime later we update our default-client property
	 * which is bound by an EBinding to ECalModel. */
	ECalClient *default_client;

	GCancellable *loading_default_client;
	GCancellable *loading_clients;
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

static void
task_shell_sidebar_emit_client_added (ETaskShellSidebar *task_shell_sidebar,
                                      ECalClient *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (task_shell_sidebar, signal_id, 0, client);
}

static void
task_shell_sidebar_emit_client_removed (ETaskShellSidebar *task_shell_sidebar,
                                        ECalClient *client)
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
task_shell_sidebar_backend_died_cb (ETaskShellSidebar *task_shell_sidebar,
                                    ECalClient *client)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = task_shell_sidebar->priv->client_table;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);

	source = e_client_get_source (E_CLIENT (client));
	uid = e_source_get_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);

	e_alert_submit (
		E_ALERT_SINK (shell_content),
		"calendar:tasks-crashed", NULL);

	g_object_unref (source);
}

static void
task_shell_sidebar_backend_error_cb (ETaskShellSidebar *task_shell_sidebar,
                                     const gchar *message,
                                     ECalClient *client)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	ESourceRegistry *registry;
	ESource *parent;
	ESource *source;
	const gchar *parent_uid;
	const gchar *parent_display_name;
	const gchar *source_display_name;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	source = e_client_get_source (E_CLIENT (client));

	parent_uid = e_source_get_parent (source);
	parent = e_source_registry_ref_source (registry, parent_uid);
	g_return_if_fail (parent != NULL);

	parent_display_name = e_source_get_display_name (parent);
	source_display_name = e_source_get_display_name (source);

	e_alert_submit (
		E_ALERT_SINK (shell_content),
		"calendar:backend-error",
		parent_display_name,
		source_display_name,
		message, NULL);

	g_object_unref (parent);
}

static void
task_shell_sidebar_retrieve_capabilies_cb (GObject *source_object,
                                           GAsyncResult *result,
                                           gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source_object);
	ETaskShellSidebar *task_shell_sidebar = user_data;
	gchar *capabilities = NULL;

	g_return_if_fail (client != NULL);
	g_return_if_fail (task_shell_sidebar != NULL);

	e_client_retrieve_capabilities_finish (
		E_CLIENT (client), result, &capabilities, NULL);
	g_free (capabilities);

	task_shell_sidebar_emit_status_message (
		task_shell_sidebar, _("Loading tasks"));
	task_shell_sidebar_emit_client_added (task_shell_sidebar, client);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);
}

static gboolean task_shell_sidebar_retry_open_timeout_cb (gpointer user_data);

struct RetryOpenData
{
	EClient *client;
	ETaskShellSidebar *task_shell_sidebar;
	GCancellable *cancellable;
};

static void
free_retry_open_data (gpointer data)
{
	struct RetryOpenData *rod = data;

	if (!rod)
		return;

	g_object_unref (rod->client);
	g_object_unref (rod->cancellable);
	g_free (rod);
}

static void
task_shell_sidebar_client_opened_cb (GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source_object);
	ETaskShellSidebar *task_shell_sidebar = user_data;
	ESource *source;
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	GError *error = NULL;

	source = e_client_get_source (E_CLIENT (client));

	e_client_open_finish (E_CLIENT (client), result, &error);

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED) ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_BUSY)) {
		struct RetryOpenData *rod;

		rod = g_new0 (struct RetryOpenData, 1);
		rod->client = g_object_ref (client);
		rod->task_shell_sidebar = task_shell_sidebar;
		rod->cancellable = g_object_ref (task_shell_sidebar->priv->loading_clients);

		/* postpone for 1/2 of a second, backend is busy now */
		g_timeout_add_full (
			G_PRIORITY_DEFAULT, 500,
			task_shell_sidebar_retry_open_timeout_cb,
			rod, free_retry_open_data);

		g_clear_error (&error);
		return;
	}

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);

	/* Handle errors. */
	switch ((error && error->domain == E_CLIENT_ERROR) ? error->code : -1) {
		case -1:
			break;

		case E_CLIENT_ERROR_BUSY:
			g_warning (
				"%s: Cannot open '%s', it's busy (%s)",
				G_STRFUNC, e_source_get_display_name (source),
				error->message);
			g_clear_error (&error);
			return;

		case E_CLIENT_ERROR_REPOSITORY_OFFLINE:
			e_alert_submit (
				E_ALERT_SINK (shell_content),
				"calendar:prompt-no-contents-offline-tasks",
				NULL);
			/* fall through */

		default:
			if (error->code != E_CLIENT_ERROR_REPOSITORY_OFFLINE) {
				e_alert_submit (
					E_ALERT_SINK (shell_content),
					"calendar:failed-open-tasks",
					error->message, NULL);
			}

			e_task_shell_sidebar_remove_source (
				task_shell_sidebar,
				e_client_get_source (E_CLIENT (client)));
			g_clear_error (&error);
			return;
	}

	g_clear_error (&error);

	/* to have them ready for later use */
	e_client_retrieve_capabilities (
		E_CLIENT (client), NULL,
		task_shell_sidebar_retrieve_capabilies_cb,
		task_shell_sidebar);
}

static gboolean
task_shell_sidebar_retry_open_timeout_cb (gpointer user_data)
{
	struct RetryOpenData *rod = user_data;

	g_return_val_if_fail (rod != NULL, FALSE);
	g_return_val_if_fail (rod->client != NULL, FALSE);
	g_return_val_if_fail (rod->task_shell_sidebar != NULL, FALSE);
	g_return_val_if_fail (rod->cancellable != NULL, FALSE);

	if (g_cancellable_is_cancelled (rod->cancellable))
		return FALSE;

	e_client_open (
		rod->client, FALSE,
		rod->task_shell_sidebar->priv->loading_clients,
		task_shell_sidebar_client_opened_cb,
		rod->task_shell_sidebar);

	return FALSE;
}

static void
task_shell_sidebar_default_loaded_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	ESource *source = E_SOURCE (source_object);
	EShellSidebar *shell_sidebar = user_data;
	ETaskShellSidebarPrivate *priv;
	EShellContent *shell_content;
	EShellView *shell_view;
	ETaskShellContent *task_shell_content;
	ECalModel *model;
	EClient *client = NULL;
	GError *error = NULL;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);
	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	model = e_task_shell_content_get_task_model (task_shell_content);

	e_client_utils_open_new_finish (source, result, &client, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_CANCELLED) ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (client == NULL);
		g_error_free (error);
		goto exit;

	} else if (error != NULL) {
		g_warn_if_fail (client == NULL);
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"calendar:failed-open-tasks",
			error->message, NULL);
		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_CAL_CLIENT (client));

	if (priv->default_client != NULL)
		g_object_unref (priv->default_client);

	priv->default_client = E_CAL_CLIENT (client);

	e_cal_client_set_default_timezone (
		priv->default_client, e_cal_model_get_timezone (model));

	g_object_notify (G_OBJECT (shell_sidebar), "default-client");

 exit:
	g_object_unref (shell_sidebar);
}

static void
task_shell_sidebar_set_default (ETaskShellSidebar *task_shell_sidebar,
                                ESource *source)
{
	ETaskShellSidebarPrivate *priv;
	EShellSidebar *shell_sidebar;
	ECalClient *client;
	const gchar *uid;

	priv = task_shell_sidebar->priv;

	/* FIXME Sidebar should not be accessing the EShellContent.
	 *       This probably needs to be moved to ETaskShellView. */
	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);

	/* Cancel any unfinished previous request. */
	if (priv->loading_default_client != NULL) {
		g_cancellable_cancel (priv->loading_default_client);
		g_object_unref (priv->loading_default_client);
		priv->loading_default_client = NULL;
	}

	uid = e_source_get_uid (source);
	client = g_hash_table_lookup (priv->client_table, uid);

	/* If we already have an open connection for
	 * this UID, we can finish immediately. */
	if (client != NULL) {
		if (priv->default_client != NULL)
			g_object_unref (priv->default_client);
		priv->default_client = g_object_ref (client);
		g_object_notify (G_OBJECT (shell_sidebar), "default-client");
		return;
	}

	priv->loading_default_client = g_cancellable_new ();

	e_client_utils_open_new (
		source, E_CLIENT_SOURCE_TYPE_TASKS,
		FALSE, priv->loading_default_client,
		task_shell_sidebar_default_loaded_cb,
		g_object_ref (shell_sidebar));
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

	if (priv->loading_default_client != NULL) {
		g_cancellable_cancel (priv->loading_default_client);
		g_object_unref (priv->loading_default_client);
		priv->loading_default_client = NULL;
	}

	if (priv->loading_clients != NULL) {
		g_cancellable_cancel (priv->loading_clients);
		g_object_unref (priv->loading_clients);
		priv->loading_clients = NULL;
	}

	g_hash_table_remove_all (priv->client_table);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_task_shell_sidebar_parent_class)->dispose (object);
}

static void
task_shell_sidebar_finalize (GObject *object)
{
	ETaskShellSidebarPrivate *priv;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->client_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_task_shell_sidebar_parent_class)->finalize (object);
}

static void
task_shell_sidebar_constructed (GObject *object)
{
	ETaskShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	ESourceRegistry *registry;
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

	registry = e_shell_get_registry (shell);
	widget = e_task_list_selector_new (registry);
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
		const gchar *uid;

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

		uid = e_source_get_uid (source);
		client = g_hash_table_lookup (
			task_shell_sidebar->priv->client_table, uid);
		refresh_supported =
			client != NULL &&
			e_client_check_refresh_supported (client);

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
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = task_shell_sidebar->priv->client_table;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	g_signal_handlers_disconnect_matched (
		client, G_SIGNAL_MATCH_DATA, 0, 0,
		NULL, NULL, task_shell_sidebar);

	source = e_client_get_source (E_CLIENT (client));
	uid = e_source_get_uid (source);

	g_hash_table_remove (client_table, uid);
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
	object_class->finalize = task_shell_sidebar_finalize;
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
	GHashTable *client_table;

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	task_shell_sidebar->priv =
		E_TASK_SHELL_SIDEBAR_GET_PRIVATE (task_shell_sidebar);

	task_shell_sidebar->priv->client_table = client_table;
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

GList *
e_task_shell_sidebar_get_clients (ETaskShellSidebar *task_shell_sidebar)
{
	GHashTable *client_table;

	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	client_table = task_shell_sidebar->priv->client_table;

	return g_hash_table_get_values (client_table);
}

ECalClient *
e_task_shell_sidebar_get_default_client (ETaskShellSidebar *task_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	return task_shell_sidebar->priv->default_client;
}

ESourceSelector *
e_task_shell_sidebar_get_selector (ETaskShellSidebar *task_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (task_shell_sidebar->priv->selector);
}

void
e_task_shell_sidebar_add_source (ETaskShellSidebar *task_shell_sidebar,
                                 ESource *source)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	ETaskShellContent *task_shell_content;
	ECalClientSourceType source_type;
	ESourceSelector *selector;
	GHashTable *client_table;
	ECalModel *model;
	ECalClient *default_client;
	ECalClient *client;
	icaltimezone *timezone;
	const gchar *display_name;
	const gchar *uid;
	gchar *message;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
	client_table = task_shell_sidebar->priv->client_table;
	default_client = task_shell_sidebar->priv->default_client;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	uid = e_source_get_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return;

	if (default_client != NULL) {
		ESource *default_source;
		const gchar *default_uid;

		default_source = e_client_get_source (E_CLIENT (default_client));
		default_uid = e_source_get_uid (default_source);

		if (g_strcmp0 (uid, default_uid) == 0)
			client = g_object_ref (default_client);
	}

	if (client == NULL)
		client = e_cal_client_new (source, source_type, NULL);

	g_return_if_fail (client != NULL);

	g_signal_connect_swapped (
		client, "backend-died",
		G_CALLBACK (task_shell_sidebar_backend_died_cb),
		task_shell_sidebar);

	g_signal_connect_swapped (
		client, "backend-error",
		G_CALLBACK (task_shell_sidebar_backend_error_cb),
		task_shell_sidebar);

	g_hash_table_insert (client_table, g_strdup (uid), client);
	e_source_selector_select_source (selector, source);

	display_name = e_source_get_display_name (source);
	message = g_strdup_printf (_("Opening task list '%s'"), display_name);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, message);
	g_free (message);

	/* FIXME Sidebar should not be accessing the EShellContent.
	 *       This probably needs to be moved to ETaskShellView. */
	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	model = e_task_shell_content_get_task_model (task_shell_content);
	timezone = e_cal_model_get_timezone (model);

	e_cal_client_set_default_timezone (client, timezone);

	e_client_open (
		E_CLIENT (client), FALSE,
		task_shell_sidebar->priv->loading_clients,
		task_shell_sidebar_client_opened_cb, task_shell_sidebar);
}

void
e_task_shell_sidebar_remove_source (ETaskShellSidebar *task_shell_sidebar,
                                    ESource *source)
{
	GHashTable *client_table;
	ECalClient *client;
	const gchar *uid;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = task_shell_sidebar->priv->client_table;

	uid = e_source_get_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client == NULL)
		return;

	task_shell_sidebar_emit_client_removed (task_shell_sidebar, client);
}

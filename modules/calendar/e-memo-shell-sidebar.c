/*
 * e-memo-shell-sidebar.c
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

#include "e-memo-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "calendar/gui/e-memo-list-selector.h"
#include "calendar/gui/misc.h"

#include "e-memo-shell-view.h"
#include "e-memo-shell-backend.h"
#include "e-memo-shell-content.h"

#define E_MEMO_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarPrivate))

typedef struct _ConnectClosure ConnectClosure;

struct _EMemoShellSidebarPrivate {
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
	EMemoShellSidebar *memo_shell_sidebar;

	/* For error messages. */
	gchar *unique_display_name;
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
	EMemoShellSidebar,
	e_memo_shell_sidebar,
	E_TYPE_SHELL_SIDEBAR)

static ConnectClosure *
connect_closure_new (EMemoShellSidebar *memo_shell_sidebar,
                     ESource *source)
{
	ConnectClosure *closure;
	ESourceRegistry *registry;
	ESourceSelector *selector;

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	registry = e_source_selector_get_registry (selector);

	closure = g_slice_new0 (ConnectClosure);
	closure->memo_shell_sidebar = g_object_ref (memo_shell_sidebar);
	closure->unique_display_name =
		e_source_registry_dup_unique_display_name (
		registry, source, E_SOURCE_EXTENSION_MEMO_LIST);

	return closure;
}

static void
connect_closure_free (ConnectClosure *closure)
{
	g_object_unref (closure->memo_shell_sidebar);

	g_free (closure->unique_display_name);

	g_slice_free (ConnectClosure, closure);
}

static void
memo_shell_sidebar_emit_client_added (EMemoShellSidebar *memo_shell_sidebar,
                                      EClient *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, client);
}

static void
memo_shell_sidebar_emit_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                        EClient *client)
{
	guint signal_id = signals[CLIENT_REMOVED];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, client);
}

static void
memo_shell_sidebar_emit_status_message (EMemoShellSidebar *memo_shell_sidebar,
                                        const gchar *status_message)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, status_message, -1.0);
}

static void
memo_shell_sidebar_handle_connect_error (EMemoShellSidebar *memo_shell_sidebar,
                                         const gchar *unique_display_name,
                                         const GError *error)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	gboolean cancelled = FALSE;
	gboolean offline_error;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
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
			"calendar:prompt-no-contents-offline-memos",
			unique_display_name,
			NULL);
	} else {
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"calendar:failed-open-memos",
			unique_display_name,
			error->message,
			NULL);
	}
}

static void
memo_shell_sidebar_client_connect_cb (GObject *source_object,
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
		memo_shell_sidebar_handle_connect_error (
			closure->memo_shell_sidebar,
			closure->unique_display_name,
			error);
		g_error_free (error);
		goto exit;
	}

	e_memo_shell_sidebar_add_client (closure->memo_shell_sidebar, client);

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
memo_shell_sidebar_default_connect_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EClient *client;
	ESource *source;
	ConnectClosure *closure = user_data;
	EMemoShellSidebarPrivate *priv;
	GError *error = NULL;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (closure->memo_shell_sidebar);

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
		memo_shell_sidebar_handle_connect_error (
			closure->memo_shell_sidebar,
			closure->unique_display_name,
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
		G_OBJECT (closure->memo_shell_sidebar), "default-client");

	g_object_unref (client);

exit:
	connect_closure_free (closure);
}

static void
memo_shell_sidebar_set_default (EMemoShellSidebar *memo_shell_sidebar,
                                ESource *source)
{
	EMemoShellSidebarPrivate *priv;
	ESourceSelector *selector;

	priv = memo_shell_sidebar->priv;

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

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
		memo_shell_sidebar_default_connect_cb,
		connect_closure_new (memo_shell_sidebar, source));
}

static void
memo_shell_sidebar_row_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
                                   GtkTreePath *tree_path,
                                   GtkTreeIter *tree_iter,
                                   GtkTreeModel *tree_model)
{
	ESourceSelector *selector;
	ESource *source;

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	source = e_source_selector_ref_source_by_path (selector, tree_path);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (source == NULL)
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_memo_shell_sidebar_add_source (memo_shell_sidebar, source);
	else
		e_memo_shell_sidebar_remove_source (memo_shell_sidebar, source);

	g_object_unref (source);
}

static void
memo_shell_sidebar_primary_selection_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
                                                 ESourceSelector *selector)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	if (source == NULL)
		return;

	memo_shell_sidebar_set_default (memo_shell_sidebar, source);

	g_object_unref (source);
}

static void
memo_shell_sidebar_restore_state_cb (EShellWindow *shell_window,
                                     EShellView *shell_view,
                                     EShellSidebar *shell_sidebar)
{
	EMemoShellSidebarPrivate *priv;
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GtkTreeModel *model;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	g_return_if_fail (E_IS_MEMO_SHELL_BACKEND (shell_backend));

	selector = E_SOURCE_SELECTOR (priv->selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	registry = e_shell_get_registry (shell);

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (memo_shell_sidebar_row_changed_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (memo_shell_sidebar_primary_selection_changed_cb),
		shell_sidebar);

	g_object_bind_property_full (
		shell_settings, "cal-primary-memo-list",
		selector, "primary-selection",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		(GBindingTransformFunc) e_binding_transform_uid_to_source,
		(GBindingTransformFunc) e_binding_transform_source_to_uid,
		g_object_ref (registry),
		(GDestroyNotify) g_object_unref);
}

static void
memo_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_CLIENT:
			g_value_set_object (
				value,
				e_memo_shell_sidebar_get_default_client (
				E_MEMO_SHELL_SIDEBAR (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value,
				e_memo_shell_sidebar_get_selector (
				E_MEMO_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_shell_sidebar_dispose (GObject *object)
{
	EMemoShellSidebarPrivate *priv;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

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
	G_OBJECT_CLASS (e_memo_shell_sidebar_parent_class)->dispose (object);
}

static void
memo_shell_sidebar_constructed (GObject *object)
{
	EMemoShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EClientCache *client_cache;
	GtkContainer *container;
	GtkWidget *widget;
	AtkObject *a11y;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_memo_shell_sidebar_parent_class)->constructed (object);

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
	widget = e_memo_list_selector_new (client_cache);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("Memo List Selector"));
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Restore widget state from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::memos",
		G_CALLBACK (memo_shell_sidebar_restore_state_cb),
		shell_sidebar);
}

static guint32
memo_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	EMemoShellSidebar *memo_shell_sidebar;
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

	memo_shell_sidebar = E_MEMO_SHELL_SIDEBAR (shell_sidebar);
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
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
		state |= E_MEMO_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (is_writable)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE;
	if (is_removable)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE;
	if (is_remote_creatable)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE;
	if (is_remote_deletable)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE;
	if (in_collection)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION;
	if (refresh_supported)
		state |= E_MEMO_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
memo_shell_sidebar_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                   ECalClient *client)
{
	ESourceSelector *selector;
	ESource *source;

	source = e_client_get_source (E_CLIENT (client));

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	e_source_selector_unselect_source (selector, source);

	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);
}

static void
e_memo_shell_sidebar_class_init (EMemoShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	g_type_class_add_private (class, sizeof (EMemoShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = memo_shell_sidebar_get_property;
	object_class->dispose = memo_shell_sidebar_dispose;
	object_class->constructed = memo_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = memo_shell_sidebar_check_state;

	class->client_removed = memo_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default Memo ECalClient",
			"Default client for memo operations",
			E_TYPE_CAL_CLIENT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			"Source Selector Widget",
			"This widget displays groups of memo lists",
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	signals[CLIENT_ADDED] = g_signal_new (
		"client-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, client_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, status_message),
		NULL, NULL,
		e_marshal_VOID__STRING_DOUBLE,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_DOUBLE);
}

static void
e_memo_shell_sidebar_class_finalize (EMemoShellSidebarClass *class)
{
}

static void
e_memo_shell_sidebar_init (EMemoShellSidebar *memo_shell_sidebar)
{
	memo_shell_sidebar->priv =
		E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (memo_shell_sidebar);

	memo_shell_sidebar->priv->loading_clients = g_cancellable_new ();

	/* Postpone widget construction until we have a shell view. */
}

void
e_memo_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_memo_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_memo_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ECalClient *
e_memo_shell_sidebar_get_default_client (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return (ECalClient *) memo_shell_sidebar->priv->default_client;
}

ESourceSelector *
e_memo_shell_sidebar_get_selector (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);
}

void
e_memo_shell_sidebar_add_client (EMemoShellSidebar *memo_shell_sidebar,
                                 EClient *client)
{
	ESource *source;
	ESourceSelector *selector;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	source = e_client_get_source (client);

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	e_source_selector_select_source (selector, source);

	memo_shell_sidebar_emit_client_added (memo_shell_sidebar, client);
}

void
e_memo_shell_sidebar_add_source (EMemoShellSidebar *memo_shell_sidebar,
                                 ESource *source)
{
	ESourceRegistry *registry;
	ESourceSelector *selector;
	gchar *display_name;
	gchar *message;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	registry = e_source_selector_get_registry (selector);

	e_source_selector_select_source (selector, source);

	display_name = e_source_registry_dup_unique_display_name (
		registry, source, E_SOURCE_EXTENSION_MEMO_LIST);

	message = g_strdup_printf (_("Opening memo list '%s'"), display_name);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, message);
	g_free (message);

	g_free (display_name);

	e_client_selector_get_client (
		E_CLIENT_SELECTOR (selector), source,
		memo_shell_sidebar->priv->loading_clients,
		memo_shell_sidebar_client_connect_cb,
		connect_closure_new (memo_shell_sidebar, source));
}

void
e_memo_shell_sidebar_remove_source (EMemoShellSidebar *memo_shell_sidebar,
                                    ESource *source)
{
	ESourceSelector *selector;
	EClient *client;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	client = e_client_selector_ref_cached_client (
		E_CLIENT_SELECTOR (selector), source);

	if (client != NULL) {
		memo_shell_sidebar_emit_client_removed (
			memo_shell_sidebar, client);
		g_object_unref (client);
	}
}

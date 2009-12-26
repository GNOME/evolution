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

#include "e-task-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>

#include "e-util/e-alert-dialog.h"
#include "e-util/e-util.h"
#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-task-list-selector.h"
#include "calendar/gui/misc.h"

#include "e-task-shell-backend.h"
#include "e-task-shell-view.h"

#define E_TASK_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_SIDEBAR, ETaskShellSidebarPrivate))

struct _ETaskShellSidebarPrivate {
	GtkWidget *selector;

	/* UID -> Client */
	GHashTable *client_table;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

enum {
	CLIENT_ADDED,
	CLIENT_REMOVED,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];
static GType task_shell_sidebar_type;

static void
task_shell_sidebar_emit_client_added (ETaskShellSidebar *task_shell_sidebar,
                                      ECal *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (task_shell_sidebar, signal_id, 0, client);
}

static void
task_shell_sidebar_emit_client_removed (ETaskShellSidebar *task_shell_sidebar,
                                        ECal *client)
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
                                    ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = task_shell_sidebar->priv->client_table;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);

	e_alert_run_dialog_for_args (
		GTK_WINDOW (shell_window),
		"calendar:tasks-crashed", NULL);

	g_object_unref (source);
}

static void
task_shell_sidebar_backend_error_cb (ETaskShellSidebar *task_shell_sidebar,
                                     const gchar *message,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GtkWidget *dialog;
	const gchar *uri;
	gchar *uri_no_passwd;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	uri = e_cal_get_uri (client);
	uri_no_passwd = get_uri_without_password (uri);

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		_("Error on %s\n%s"),
		uri_no_passwd, message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (uri_no_passwd);
}

static void
task_shell_sidebar_client_opened_cb (ETaskShellSidebar *task_shell_sidebar,
                                     ECalendarStatus status,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	ESource *source;

	source = e_cal_get_source (client);

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (status == E_CALENDAR_STATUS_AUTHENTICATION_FAILED ||
		status == E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED)
		auth_cal_forget_password (client);

	switch (status) {
		case E_CALENDAR_STATUS_OK:
			g_signal_handlers_disconnect_matched (
				client, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
				task_shell_sidebar_client_opened_cb, NULL);

			task_shell_sidebar_emit_status_message (
				task_shell_sidebar, _("Loading tasks"));
			task_shell_sidebar_emit_client_added (
				task_shell_sidebar, client);
			task_shell_sidebar_emit_status_message (
				task_shell_sidebar, NULL);
			break;

		case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
			e_cal_open_async (client, FALSE);
			break;

		case E_CALENDAR_STATUS_BUSY:
			break;

		case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
			e_alert_run_dialog_for_args (
				GTK_WINDOW (shell_window),
				"calendar:prompt-no-contents-offline-tasks",
				NULL);
			break;

		default:
			task_shell_sidebar_emit_client_removed (
				task_shell_sidebar, client);
			break;
	}
}

static void
task_shell_sidebar_row_changed_cb (ETaskShellSidebar *task_shell_sidebar,
                                   GtkTreePath *tree_path,
                                   GtkTreeIter *tree_iter,
                                   GtkTreeModel *tree_model)
{
	ESourceSelector *selector;
	ESource *source;

	/* XXX ESourceSelector's underlying tree store has only one
	 *     column: ESource objects.  While we're not supposed to
	 *     know this, listening for "row-changed" signals from
	 *     the model is easier to deal with than the selector's
	 *     "selection-changed" signal, which doesn't tell you
	 *     _which_ row changed. */

	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	gtk_tree_model_get (tree_model, tree_iter, 0, &source, -1);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (!E_IS_SOURCE (source))
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_task_shell_sidebar_add_source (task_shell_sidebar, source);
	else
		e_task_shell_sidebar_remove_source (task_shell_sidebar, source);
}

static void
task_shell_sidebar_selection_changed_cb (ETaskShellSidebar *task_shell_sidebar,
                                         ESourceSelector *selector)
{
	GSList *list, *iter;

	/* This signal is emitted less frequently than "row-changed",
	 * especially when the model is being rebuilt.  So we'll take
	 * it easy on poor GConf. */

	list = e_source_selector_get_selection (selector);

	for (iter = list; iter != NULL; iter = iter->next) {
		ESource *source = iter->data;

		iter->data = (gpointer) e_source_peek_uid (source);
		g_object_unref (source);
	}

	calendar_config_set_tasks_selected (list);

	g_slist_free (list);
}

static void
task_shell_sidebar_primary_selection_changed_cb (ETaskShellSidebar *task_shell_sidebar,
                                                 ESourceSelector *selector)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EShellSettings *shell_settings;
	ESource *source;

	/* XXX ESourceSelector needs a "primary-selection-uid" property
	 *     so we can just bind the property with GConfBridge. */

	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		return;

	shell_sidebar = E_SHELL_SIDEBAR (task_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_set_string (
		shell_settings, "cal-primary-task-list",
		e_source_peek_uid (source));
}

static void
task_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_task_shell_sidebar_get_selector (
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

	g_hash_table_remove_all (priv->client_table);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
task_shell_sidebar_finalize (GObject *object)
{
	ETaskShellSidebarPrivate *priv;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->client_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
task_shell_sidebar_constructed (GObject *object)
{
	ETaskShellSidebarPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EShellSettings *shell_settings;
	ESourceSelector *selector;
	ESourceList *source_list;
	ESource *source;
	GtkContainer *container;
	GtkTreeModel *model;
	GtkWidget *widget;
	AtkObject *a11y;
	GSList *list, *iter;
	gchar *uid;

	priv = E_TASK_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	source_list = e_task_shell_backend_get_source_list (
		E_TASK_SHELL_BACKEND (shell_backend));

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

	widget = e_task_list_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("Task List Selector"));
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Restore the selector state from the last session. */

	selector = E_SOURCE_SELECTOR (priv->selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (task_shell_sidebar_row_changed_cb),
		object);

	source = NULL;
	uid = e_shell_settings_get_string (
		shell_settings, "cal-primary-task-list");
	if (uid != NULL)
		source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source == NULL)
		source = e_source_list_peek_source_any (source_list);
	if (source != NULL)
		e_source_selector_set_primary_selection (selector, source);
	g_free (uid);

	list = calendar_config_get_tasks_selected ();
	for (iter = list; iter != NULL; iter = iter->next) {
		uid = iter->data;
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);

		if (source == NULL)
			continue;

		e_source_selector_select_source (selector, source);
	}
	g_slist_free (list);

	/* Listen for subsequent changes to the selector. */

	g_signal_connect_swapped (
		widget, "selection-changed",
		G_CALLBACK (task_shell_sidebar_selection_changed_cb),
		object);

	g_signal_connect_swapped (
		widget, "primary-selection-changed",
		G_CALLBACK (task_shell_sidebar_primary_selection_changed_cb),
		object);
}

static guint32
task_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	ETaskShellSidebar *task_shell_sidebar;
	ESourceSelector *selector;
	ESource *source;
	gboolean can_delete = FALSE;
	gboolean is_system = FALSE;
	gboolean refresh_supported = FALSE;
	guint32 state = 0;

	task_shell_sidebar = E_TASK_SHELL_SIDEBAR (shell_sidebar);
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	source = e_source_selector_peek_primary_selection (selector);

	if (source != NULL) {
		ECal *client;
		const gchar *uri;
		const gchar *delete;

		uri = e_source_peek_relative_uri (source);
		is_system = (uri == NULL || strcmp (uri, "system") == 0);

		can_delete = !is_system;
		delete = e_source_get_property (source, "delete");
		can_delete &= (delete == NULL || strcmp (delete, "no") != 0);

		client = g_hash_table_lookup (
			task_shell_sidebar->priv->client_table,
			e_source_peek_uid (source));
		refresh_supported =
			client && e_cal_get_refresh_supported (client);
	}

	if (source != NULL)
		state |= E_TASK_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (can_delete)
		state |= E_TASK_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE;
	if (is_system)
		state |= E_TASK_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM;
	if (refresh_supported)
		state |= E_TASK_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
task_shell_sidebar_client_removed (ETaskShellSidebar *task_shell_sidebar,
                                   ECal *client)
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

	source = e_cal_get_source (client);
	e_source_selector_unselect_source (selector, source);

	uid = e_source_peek_uid (source);
	g_hash_table_remove (client_table, uid);

	task_shell_sidebar_emit_status_message (task_shell_sidebar, NULL);
}

static void
task_shell_sidebar_class_init (ETaskShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	parent_class = g_type_class_peek_parent (class);
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
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of task lists"),
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
		E_TYPE_CAL);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETaskShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL);

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
task_shell_sidebar_init (ETaskShellSidebar *task_shell_sidebar)
{
	GHashTable *client_table;

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	task_shell_sidebar->priv =
		E_TASK_SHELL_SIDEBAR_GET_PRIVATE (task_shell_sidebar);

	task_shell_sidebar->priv->client_table = client_table;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_task_shell_sidebar_get_type (void)
{
	return task_shell_sidebar_type;
}

void
e_task_shell_sidebar_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (ETaskShellSidebarClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) task_shell_sidebar_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ETaskShellSidebar),
		0,     /* n_preallocs */
		(GInstanceInitFunc) task_shell_sidebar_init,
		NULL   /* value_table */
	};

	task_shell_sidebar_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_SIDEBAR,
		"ETaskShellSidebar", &type_info, 0);
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
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;
	const gchar *uri;
	gchar *message;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = task_shell_sidebar->priv->client_table;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return;

	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
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

	uri = e_cal_get_uri (client);
	message = g_strdup_printf (_("Opening tasks at %s"), uri);
	task_shell_sidebar_emit_status_message (task_shell_sidebar, message);
	g_free (message);

	g_signal_connect_swapped (
		client, "cal-opened",
		G_CALLBACK (task_shell_sidebar_client_opened_cb),
		task_shell_sidebar);

	e_cal_open_async (client, FALSE);
}

void
e_task_shell_sidebar_remove_source (ETaskShellSidebar *task_shell_sidebar,
                                    ESource *source)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;

	g_return_if_fail (E_IS_TASK_SHELL_SIDEBAR (task_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = task_shell_sidebar->priv->client_table;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client == NULL)
		return;

	task_shell_sidebar_emit_client_removed (task_shell_sidebar, client);
}

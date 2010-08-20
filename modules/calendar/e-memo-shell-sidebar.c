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

#include "e-memo-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>

#include "e-util/e-alert-dialog.h"
#include "e-util/e-util.h"
#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-memo-list-selector.h"
#include "calendar/gui/misc.h"

#include "e-memo-shell-view.h"
#include "e-memo-shell-backend.h"
#include "e-memo-shell-content.h"

#define E_MEMO_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarPrivate))

struct _EMemoShellSidebarPrivate {
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
	ECal *default_client;

	GCancellable *loading_default_client;
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

static gpointer parent_class;
static guint signals[LAST_SIGNAL];
static GType memo_shell_sidebar_type;

static void
memo_shell_sidebar_emit_client_added (EMemoShellSidebar *memo_shell_sidebar,
                                      ECal *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, client);
}

static void
memo_shell_sidebar_emit_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                        ECal *client)
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
memo_shell_sidebar_backend_died_cb (EMemoShellSidebar *memo_shell_sidebar,
                                    ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = memo_shell_sidebar->priv->client_table;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);

	e_alert_run_dialog_for_args (
		GTK_WINDOW (shell_window),
		"calendar:memos-crashed", NULL);

	g_object_unref (source);
}

static void
memo_shell_sidebar_backend_error_cb (EMemoShellSidebar *memo_shell_sidebar,
                                     const gchar *message,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GtkWidget *dialog;
	const gchar *uri;
	gchar *uri_no_passwd;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	uri = e_cal_get_uri (client);
	uri_no_passwd = get_uri_without_password (uri);

	/* Translators: This string is displayed in a message dialog when
	 *              our connection to the calendar service detects an
	 *              out-of-band error.  The first string is a URI for
	 *              the source of the error, the second string is the
	 *              error message. */
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
memo_shell_sidebar_client_opened_cb (EMemoShellSidebar *memo_shell_sidebar,
                                     const GError *error,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	const gchar *message;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (g_error_matches (error, E_CALENDAR_ERROR,
		E_CALENDAR_STATUS_AUTHENTICATION_FAILED) ||
	    g_error_matches (error, E_CALENDAR_ERROR,
		E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED))
		e_auth_cal_forget_password (client);

	/* Handle errors. */
	switch (error ? error->code : E_CALENDAR_STATUS_OK) {
		case E_CALENDAR_STATUS_OK:
			break;

		case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
			e_cal_open_async (client, FALSE);
			return;

		case E_CALENDAR_STATUS_BUSY:
			return;

		case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
			e_alert_run_dialog_for_args (
				GTK_WINDOW (shell_window),
				"calendar:prompt-no-contents-offline-memos",
				NULL);
			/* fall through */

		default:
			if (error->code != E_CALENDAR_STATUS_REPOSITORY_OFFLINE) {
				e_alert_run_dialog_for_args (
					GTK_WINDOW (shell_window),
					"calendar:failed-open-memos",
					error->message, NULL);
			}

			e_memo_shell_sidebar_remove_source (
				memo_shell_sidebar,
				e_cal_get_source (client));
			return;
	}

	g_assert (error == NULL);

	g_signal_handlers_disconnect_matched (
		client, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
		memo_shell_sidebar_client_opened_cb, NULL);

	message = _("Loading memos");
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, message);
	memo_shell_sidebar_emit_client_added (memo_shell_sidebar, client);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);
}

static void
memo_shell_sidebar_default_loaded_cb (ESource *source,
                                      GAsyncResult *result,
                                      EShellSidebar *shell_sidebar)
{
	EMemoShellSidebarPrivate *priv;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ECal *client;
	GError *error = NULL;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	client = e_load_cal_source_finish (source, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;

	} else if (error != NULL) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:failed-open-memos",
			error->message, NULL);
		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_CAL (client));

	if (priv->default_client != NULL)
		g_object_unref (priv->default_client);

	priv->default_client = client;

	g_object_notify (G_OBJECT (shell_sidebar), "default-client");

exit:
	g_object_unref (shell_sidebar);
}

static void
memo_shell_sidebar_set_default (EMemoShellSidebar *memo_shell_sidebar,
                                ESource *source)
{
	EMemoShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EMemoShellContent *memo_shell_content;
	ECalSourceType source_type;
	ECalModel *model;
	ECal *client;
	icaltimezone *timezone;
	const gchar *uid;

	priv = memo_shell_sidebar->priv;
	source_type = E_CAL_SOURCE_TYPE_JOURNAL;

	/* FIXME Sidebar should not be accessing the EShellContent.
	 *       This probably needs to be moved to EMemoShellView. */
	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memo_shell_content = E_MEMO_SHELL_CONTENT (shell_content);
	model = e_memo_shell_content_get_memo_model (memo_shell_content);
	timezone = e_cal_model_get_timezone (model);

	/* Cancel any unfinished previous request. */
	if (priv->loading_default_client != NULL) {
		g_cancellable_cancel (priv->loading_default_client);
		g_object_unref (priv->loading_default_client);
		priv->loading_default_client = NULL;
	}

	uid = e_source_peek_uid (source);
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

	e_load_cal_source_async (
		source, source_type, timezone,
		GTK_WINDOW (shell_window), priv->loading_default_client,
		(GAsyncReadyCallback) memo_shell_sidebar_default_loaded_cb,
		g_object_ref (shell_sidebar));
}

static void
memo_shell_sidebar_row_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
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

	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
	gtk_tree_model_get (tree_model, tree_iter, 0, &source, -1);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (!E_IS_SOURCE (source))
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_memo_shell_sidebar_add_source (memo_shell_sidebar, source);
	else
		e_memo_shell_sidebar_remove_source (memo_shell_sidebar, source);
}

static void
memo_shell_sidebar_selection_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
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

	calendar_config_set_memos_selected (list);

	g_slist_free (list);
}

static void
memo_shell_sidebar_primary_selection_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
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

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_set_string (
		shell_settings, "cal-primary-memo-list",
		e_source_peek_uid (source));

	memo_shell_sidebar_set_default (memo_shell_sidebar, source);
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
	ESourceSelector *selector;
	ESourceList *source_list;
	ESource *source;
	GtkTreeModel *model;
	GSList *list, *iter;
	gchar *uid;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (shell_sidebar);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	g_return_if_fail (E_IS_MEMO_SHELL_BACKEND (shell_backend));

	selector = E_SOURCE_SELECTOR (priv->selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (selector));

	source_list = e_memo_shell_backend_get_source_list (
		E_MEMO_SHELL_BACKEND (shell_backend));

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (memo_shell_sidebar_row_changed_cb),
		shell_sidebar);

	g_signal_connect_swapped (
		selector, "primary-selection-changed",
		G_CALLBACK (memo_shell_sidebar_primary_selection_changed_cb),
		shell_sidebar);

	source = NULL;
	uid = e_shell_settings_get_string (
		shell_settings, "cal-primary-memo-list");
	if (uid != NULL)
		source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source == NULL)
		source = e_source_list_peek_source_any (source_list);
	if (source != NULL)
		e_source_selector_set_primary_selection (selector, source);
	g_free (uid);

	list = calendar_config_get_memos_selected ();
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
		selector, "selection-changed",
		G_CALLBACK (memo_shell_sidebar_selection_changed_cb),
		shell_sidebar);
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

	if (priv->loading_default_client != NULL) {
		g_cancellable_cancel (priv->loading_default_client);
		g_object_unref (priv->loading_default_client);
		priv->loading_default_client = NULL;
	}

	g_hash_table_remove_all (priv->client_table);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_shell_sidebar_finalize (GObject *object)
{
	EMemoShellSidebarPrivate *priv;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->client_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
memo_shell_sidebar_constructed (GObject *object)
{
	EMemoShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;
	AtkObject *a11y;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source_list = e_memo_shell_backend_get_source_list (
		E_MEMO_SHELL_BACKEND (shell_backend));

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

	widget = e_memo_list_selector_new (source_list);
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
	ESource *source;
	gboolean can_delete = FALSE;
	gboolean is_system = FALSE;
	gboolean refresh_supported = FALSE;
	guint32 state = 0;

	memo_shell_sidebar = E_MEMO_SHELL_SIDEBAR (shell_sidebar);
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);
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
			memo_shell_sidebar->priv->client_table,
			e_source_peek_uid (source));
		refresh_supported =
			client && e_cal_get_refresh_supported (client);
	}

	if (source != NULL)
		state |= E_MEMO_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (can_delete)
		state |= E_MEMO_SHELL_SIDEBAR_CAN_DELETE_PRIMARY_SOURCE;
	if (is_system)
		state |= E_MEMO_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_SYSTEM;
	if (refresh_supported)
		state |= E_MEMO_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static void
memo_shell_sidebar_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                   ECal *client)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = memo_shell_sidebar->priv->client_table;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	g_signal_handlers_disconnect_matched (
		client, G_SIGNAL_MATCH_DATA, 0, 0,
		NULL, NULL, memo_shell_sidebar);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_hash_table_remove (client_table, uid);
	e_source_selector_unselect_source (selector, source);

	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);
}

static void
memo_shell_sidebar_class_init (EMemoShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = memo_shell_sidebar_get_property;
	object_class->dispose = memo_shell_sidebar_dispose;
	object_class->finalize = memo_shell_sidebar_finalize;
	object_class->constructed = memo_shell_sidebar_constructed;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = memo_shell_sidebar_check_state;

	class->client_removed = memo_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_CLIENT,
		g_param_spec_object (
			"default-client",
			"Default Memo Client",
			"Default client for memo operations",
			E_TYPE_CAL,
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
		E_TYPE_CAL);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL);

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
memo_shell_sidebar_init (EMemoShellSidebar *memo_shell_sidebar)
{
	GHashTable *client_table;

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	memo_shell_sidebar->priv =
		E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (memo_shell_sidebar);

	memo_shell_sidebar->priv->client_table = client_table;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_memo_shell_sidebar_get_type (void)
{
	return memo_shell_sidebar_type;
}

void
e_memo_shell_sidebar_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMemoShellSidebarClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) memo_shell_sidebar_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMemoShellSidebar),
		0,     /* n_preallocs */
		(GInstanceInitFunc) memo_shell_sidebar_init,
		NULL   /* value_table */
	};

	memo_shell_sidebar_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_SIDEBAR,
		"EMemoShellSidebar", &type_info, 0);
}

GtkWidget *
e_memo_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

GList *
e_memo_shell_sidebar_get_clients (EMemoShellSidebar *memo_shell_sidebar)
{
	GHashTable *client_table;

	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	client_table = memo_shell_sidebar->priv->client_table;

	return g_hash_table_get_values (client_table);
}

ECal *
e_memo_shell_sidebar_get_default_client (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return memo_shell_sidebar->priv->default_client;
}

ESourceSelector *
e_memo_shell_sidebar_get_selector (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);
}

void
e_memo_shell_sidebar_add_source (EMemoShellSidebar *memo_shell_sidebar,
                                 ESource *source)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EMemoShellContent *memo_shell_content;
	ECalSourceType source_type;
	ESourceSelector *selector;
	GHashTable *client_table;
	ECalModel *model;
	ECal *default_client;
	ECal *client;
	icaltimezone *timezone;
	const gchar *uid;
	const gchar *uri;
	gchar *message;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	source_type = E_CAL_SOURCE_TYPE_JOURNAL;
	client_table = memo_shell_sidebar->priv->client_table;
	default_client = memo_shell_sidebar->priv->default_client;
	selector = e_memo_shell_sidebar_get_selector (memo_shell_sidebar);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return;

	if (default_client != NULL) {
		ESource *default_source;
		const gchar *default_uid;

		default_source = e_cal_get_source (default_client);
		default_uid = e_source_peek_uid (default_source);

		if (g_strcmp0 (uid, default_uid) == 0)
			client = g_object_ref (default_client);
	}

	if (client == NULL)
		client = e_auth_new_cal_from_source (source, source_type);

	g_return_if_fail (client != NULL);

	g_signal_connect_swapped (
		client, "backend-died",
		G_CALLBACK (memo_shell_sidebar_backend_died_cb),
		memo_shell_sidebar);

	g_signal_connect_swapped (
		client, "backend-error",
		G_CALLBACK (memo_shell_sidebar_backend_error_cb),
		memo_shell_sidebar);

	g_hash_table_insert (client_table, g_strdup (uid), client);
	e_source_selector_select_source (selector, source);

	uri = e_cal_get_uri (client);
	/* Translators: The string field is a URI. */
	message = g_strdup_printf (_("Opening memos at %s"), uri);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, message);
	g_free (message);

	g_signal_connect_swapped (
		client, "cal-opened-ex",
		G_CALLBACK (memo_shell_sidebar_client_opened_cb),
		memo_shell_sidebar);

	/* FIXME Sidebar should not be accessing the EShellContent.
	 *       This probably needs to be moved to EMemoShellView. */
	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_content = e_shell_view_get_shell_content (shell_view);

	memo_shell_content = E_MEMO_SHELL_CONTENT (shell_content);
	model = e_memo_shell_content_get_memo_model (memo_shell_content);
	timezone = e_cal_model_get_timezone (model);

	e_cal_set_default_timezone (client, timezone, NULL);
	e_cal_open_async (client, FALSE);
}

void
e_memo_shell_sidebar_remove_source (EMemoShellSidebar *memo_shell_sidebar,
                                    ESource *source)
{
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = memo_shell_sidebar->priv->client_table;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client == NULL)
		return;

	memo_shell_sidebar_emit_client_removed (memo_shell_sidebar, client);
}

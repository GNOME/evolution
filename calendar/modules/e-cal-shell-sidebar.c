/*
 * e-cal-shell-sidebar.c
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

#include "e-cal-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-error.h"
#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-calendar-selector.h"
#include "calendar/gui/e-mini-calendar-config.h"
#include "calendar/gui/misc.h"

#include "e-cal-shell-view.h"

#define E_CAL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarPrivate))

struct _ECalShellSidebarPrivate {
	GtkWidget *paned;
	GtkWidget *selector;
	GtkWidget *mini_calendar;

	/* UID -> Client */
	GHashTable *client_table;

	EMiniCalendarConfig *mini_calendar_config;
};

enum {
	PROP_0,
	PROP_MINI_CALENDAR,
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

static void
cal_shell_sidebar_emit_client_added (ECalShellSidebar *cal_shell_sidebar,
                                     ECal *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (cal_shell_sidebar, signal_id, 0, client);
}

static void
cal_shell_sidebar_emit_client_removed (ECalShellSidebar *cal_shell_sidebar,
                                       ECal *client)
{
	guint signal_id = signals[CLIENT_REMOVED];

	g_signal_emit (cal_shell_sidebar, signal_id, 0, client);
}

static void
cal_shell_sidebar_emit_status_message (ECalShellSidebar *cal_shell_sidebar,
                                       const gchar *status_message)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (cal_shell_sidebar, signal_id, 0, status_message);
}

static void
cal_shell_sidebar_backend_died_cb (ECalShellSidebar *cal_shell_sidebar,
                                   ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = cal_shell_sidebar->priv->client_table;

	shell_sidebar = E_SHELL_SIDEBAR (cal_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	cal_shell_sidebar_emit_status_message (cal_shell_sidebar, NULL);

	e_error_run (
		GTK_WINDOW (shell_window),
		"calendar:calendar-crashed", NULL);

	g_object_unref (source);
}

static void
cal_shell_sidebar_backend_error_cb (ECalShellSidebar *cal_shell_sidebar,
                                    const gchar *message,
                                    ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GtkWidget *dialog;
	const gchar *uri;
	gchar *uri_no_passwd;

	shell_sidebar = E_SHELL_SIDEBAR (cal_shell_sidebar);
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
cal_shell_sidebar_client_opened_cb (ECalShellSidebar *cal_shell_sidebar,
                                    ECalendarStatus status,
                                    ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	ESource *source;

	source = e_cal_get_source (client);

	shell_sidebar = E_SHELL_SIDEBAR (cal_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	switch (status) {
		case E_CALENDAR_STATUS_OK:
			g_signal_handlers_disconnect_matched (
				client, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
				cal_shell_sidebar_client_opened_cb, NULL);

			cal_shell_sidebar_emit_status_message (
				cal_shell_sidebar, _("Loading calendars"));
			cal_shell_sidebar_emit_client_added (
				cal_shell_sidebar, client);
			cal_shell_sidebar_emit_status_message (
				cal_shell_sidebar, NULL);
			break;

		case E_CALENDAR_STATUS_BUSY:
			break;

		case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
			e_error_run (
				GTK_WINDOW (shell_window),
				"calendar:prompt-no-contents-offline-calendar",
				NULL);
			break;

		default:
			cal_shell_sidebar_emit_client_removed (
				cal_shell_sidebar, client);
			break;
	}
}

static void
cal_shell_sidebar_row_changed_cb (ECalShellSidebar *cal_shell_sidebar,
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

	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	gtk_tree_model_get (tree_model, tree_iter, 0, &source, -1);

	/* XXX This signal gets emitted a lot while the model is being
	 *     rebuilt, during which time we won't get a valid ESource.
	 *     ESourceSelector should probably block this signal while
	 *     rebuilding the model, but we'll be forgiving and not
	 *     emit a warning. */
	if (!E_IS_SOURCE (source))
		return;

	if (e_source_selector_source_is_selected (selector, source))
		e_cal_shell_sidebar_add_source (cal_shell_sidebar, source);
	else
		e_cal_shell_sidebar_remove_source (cal_shell_sidebar, source);
}

static void
cal_shell_sidebar_selection_changed_cb (ECalShellSidebar *cal_shell_sidebar,
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

	calendar_config_set_calendars_selected (list);

	g_slist_free (list);
}

static void
cal_shell_sidebar_primary_selection_changed_cb (ECalShellSidebar *cal_shell_sidebar,
                                                ESourceSelector *selector)
{
	ESource *source;
	const gchar *uid;

	/* XXX ESourceSelector needs a "primary-selection-uid" property
	 *     so we can just bind the property with GConfBridge. */

	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		return;

	uid = e_source_peek_uid (source);
	calendar_config_set_primary_calendar (uid);
}

static void
cal_shell_sidebar_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MINI_CALENDAR:
			g_value_set_object (
				value, e_cal_shell_sidebar_get_mini_calendar (
				E_CAL_SHELL_SIDEBAR (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value, e_cal_shell_sidebar_get_selector (
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

	if (priv->mini_calendar != NULL) {
		g_object_unref (priv->mini_calendar);
		priv->mini_calendar = NULL;
	}

	g_hash_table_remove_all (priv->client_table);

	if (priv->mini_calendar_config != NULL) {
		g_object_unref (priv->mini_calendar_config);
		priv->mini_calendar = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_sidebar_finalize (GObject *object)
{
	ECalShellSidebarPrivate *priv;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->client_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_shell_sidebar_constructed (GObject *object)
{
	ECalShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalShellView *cal_shell_view;
	ESourceSelector *selector;
	ESourceList *source_list;
	ESource *source;
	ECalendarItem *calitem;
	GtkTreeModel *model;
	GtkWidget *container;
	GtkWidget *widget;
	AtkObject *a11y;
	GSList *list, *iter;
	gchar *uid;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	cal_shell_view = E_CAL_SHELL_VIEW (shell_view);
	source_list = e_cal_shell_view_get_source_list (cal_shell_view);

	container = GTK_WIDGET (shell_sidebar);

	widget = gtk_vpaned_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_add1 (GTK_PANED (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_calendar_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
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
	gtk_paned_add2 (GTK_PANED (container), widget);
	priv->mini_calendar = g_object_ref (widget);
	gtk_widget_show (widget);

	priv->mini_calendar_config =
		e_mini_calendar_config_new (E_CALENDAR (widget));

	/* Restore the selector state from the last session. */

	selector = E_SOURCE_SELECTOR (priv->selector);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		model, "row-changed",
		G_CALLBACK (cal_shell_sidebar_row_changed_cb),
		object);

	source = NULL;
	uid = calendar_config_get_primary_calendar ();
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
		G_CALLBACK (cal_shell_sidebar_selection_changed_cb),
		object);

	g_signal_connect_swapped (
		widget, "primary-selection-changed",
		G_CALLBACK (cal_shell_sidebar_primary_selection_changed_cb),
		object);
}

static void
cal_shell_sidebar_client_added (ECalShellSidebar *cal_shell_sidebar,
                                ECal *client)
{
        /* FIXME */
	/*cal_shell_sidebar_update_timezone (cal_shell_sidebar);*/
}

static void
cal_shell_sidebar_client_removed (ECalShellSidebar *cal_shell_sidebar,
                                  ECal *client)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = cal_shell_sidebar->priv->client_table;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	g_signal_handlers_disconnect_matched (
		client, G_SIGNAL_MATCH_DATA, 0, 0,
		NULL, NULL, cal_shell_sidebar);

	source = e_cal_get_source (client);
	e_source_selector_unselect_source (selector, source);

	uid = e_source_peek_uid (source);
	g_hash_table_remove (client_table, uid);

	cal_shell_sidebar_emit_status_message (cal_shell_sidebar, NULL);
}

static void
cal_shell_sidebar_class_init (ECalShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_sidebar_get_property;
	object_class->dispose = cal_shell_sidebar_dispose;
	object_class->finalize = cal_shell_sidebar_finalize;
	object_class->constructed = cal_shell_sidebar_constructed;

	class->client_added = cal_shell_sidebar_client_added;
	class->client_removed = cal_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_MINI_CALENDAR,
		g_param_spec_object (
			"mini-calendar",
			_("Mini-Calendar Widget"),
			_("This widget displays a miniature calendar"),
			E_TYPE_CALENDAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of calendars"),
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
		E_TYPE_CAL);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalShellSidebarClass, status_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
cal_shell_sidebar_init (ECalShellSidebar *cal_shell_sidebar)
{
	GHashTable *client_table;

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	cal_shell_sidebar->priv =
		E_CAL_SHELL_SIDEBAR_GET_PRIVATE (cal_shell_sidebar);

	cal_shell_sidebar->priv->client_table = client_table;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_cal_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) cal_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "ECalShellSidebar",
			&type_info, 0);
	}

	return type;
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
e_cal_shell_sidebar_get_mini_calendar (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return E_CALENDAR (cal_shell_sidebar->priv->mini_calendar);
}

ESourceSelector *
e_cal_shell_sidebar_get_selector (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (cal_shell_sidebar->priv->selector);
}

void
e_cal_shell_sidebar_add_source (ECalShellSidebar *cal_shell_sidebar,
                                ESource *source)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;
	const gchar *uri;
	gchar *message;

	g_return_if_fail (E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = cal_shell_sidebar->priv->client_table;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return;

	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	g_return_if_fail (client != NULL);

	g_signal_connect_swapped (
		client, "backend-died",
		G_CALLBACK (cal_shell_sidebar_backend_died_cb),
		cal_shell_sidebar);

	g_signal_connect_swapped (
		client, "backend-error",
		G_CALLBACK (cal_shell_sidebar_backend_error_cb),
		cal_shell_sidebar);

	g_hash_table_insert (client_table, g_strdup (uid), client);
	e_source_selector_select_source (selector, source);

	uri = e_cal_get_uri (client);
	message = g_strdup_printf (_("Opening calendar at %s"), uri);
	cal_shell_sidebar_emit_status_message (cal_shell_sidebar, message);
	g_free (message);

	g_signal_connect_swapped (
		client, "cal-opened",
		G_CALLBACK (cal_shell_sidebar_client_opened_cb),
		cal_shell_sidebar);

	e_cal_open_async (client, FALSE);
}

void
e_cal_shell_sidebar_remove_source (ECalShellSidebar *cal_shell_sidebar,
                                   ESource *source)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;

	g_return_if_fail (E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = cal_shell_sidebar->priv->client_table;
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client == NULL)
		return;

	cal_shell_sidebar_emit_client_removed (cal_shell_sidebar, client);
}

void
e_cal_shell_sidebar_update_timezone (ECalShellSidebar *cal_shell_sidebar)
{
	GHashTable *client_table;
	icaltimezone *timezone;
	GList *values;

	g_return_if_fail (E_CAL_SHELL_SIDEBAR (cal_shell_sidebar));

	timezone = calendar_config_get_icaltimezone ();
	client_table = cal_shell_sidebar->priv->client_table;
	values = g_hash_table_get_values (client_table);

	while (values != NULL) {
		ECal *client = values->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			e_cal_set_default_timezone (client, timezone, NULL);

		values = g_list_delete_link (values, values);
	}
}

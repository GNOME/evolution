/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "calendar/gui/comp-util.h"

#include "e-cal-base-shell-view.h"
#include "e-cal-base-shell-sidebar.h"

struct _ECalBaseShellSidebarPrivate {
	ECalendar *date_navigator; /* not referenced, is inside itself */
	GtkWidget *paned; /* not referenced, is inside itself */
	ESourceSelector *selector; /* not referenced, is inside itself */

	gulong date_navigator_scroll_event_handler_id;

	GHashTable *selected_uids; /* source UID -> cancellable */
};

enum {
	PROP_0,
	PROP_DATE_NAVIGATOR,
	PROP_SELECTOR
};

enum {
	CLIENT_OPENED,
	CLIENT_CLOSED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ECalBaseShellSidebar, e_cal_base_shell_sidebar, E_TYPE_SHELL_SIDEBAR, 0,
	G_ADD_PRIVATE_DYNAMIC (ECalBaseShellSidebar))

static gboolean
cal_base_shell_sidebar_map_uid_to_source (GValue *value,
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

	return TRUE;
}

static GVariant *
cal_base_shell_sidebar_map_source_to_uid (const GValue *value,
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
cal_base_shell_sidebar_restore_state_cb (EShellWindow *shell_window,
					 EShellView *shell_view,
					 EShellSidebar *shell_sidebar)
{
	ECalBaseShellSidebarPrivate *priv;
	ESourceRegistry *registry;
	ESourceSelector *selector;
	GSettings *settings;
	const gchar *primary_source_key = NULL;

	priv = E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar)->priv;

	g_signal_handlers_disconnect_by_func (
		shell_window,
		cal_base_shell_sidebar_restore_state_cb, shell_sidebar);

	switch (e_cal_base_shell_view_get_source_type (shell_view)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			primary_source_key = "primary-calendar";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			primary_source_key = "primary-memos";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			primary_source_key = "primary-tasks";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			return;
	}

	selector = E_SOURCE_SELECTOR (priv->selector);
	registry = e_source_selector_get_registry (selector);

	/* Bind GObject properties to settings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind_with_mapping (
		settings, primary_source_key,
		selector, "primary-selection",
		G_SETTINGS_BIND_DEFAULT,
		cal_base_shell_sidebar_map_uid_to_source,
		cal_base_shell_sidebar_map_source_to_uid,
		g_object_ref (registry),
		(GDestroyNotify) g_object_unref);

	if (priv->date_navigator) {
		if (e_shell_window_is_main_instance (shell_window)) {
			g_settings_bind (
				settings, "date-navigator-pane-position",
				priv->paned, "vposition",
				G_SETTINGS_BIND_DEFAULT);
		} else {
			g_settings_bind (
				settings, "date-navigator-pane-position-sub",
				priv->paned, "vposition",
				G_SETTINGS_BIND_DEFAULT |
				G_SETTINGS_BIND_GET_NO_CHANGES);
		}
	}

	g_object_unref (settings);
}

static guint32
cal_base_shell_sidebar_check_state (EShellSidebar *shell_sidebar)
{
	ECalBaseShellSidebar *cal_base_shell_sidebar;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source, *clicked_source;
	gboolean is_writable = FALSE;
	gboolean is_removable = FALSE;
	gboolean is_remote_creatable = FALSE;
	gboolean is_remote_deletable = FALSE;
	gboolean in_collection = FALSE;
	gboolean refresh_supported = FALSE;
	gboolean has_primary_source = FALSE;
	guint32 state = 0;

	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar);
	selector = e_cal_base_shell_sidebar_get_selector (cal_base_shell_sidebar);
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

	clicked_source = e_cal_base_shell_view_get_clicked_source (e_shell_sidebar_get_shell_view (shell_sidebar));
	if (clicked_source && clicked_source == source)
		state |= E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_PRIMARY;
	if (clicked_source && e_source_has_extension (clicked_source, E_SOURCE_EXTENSION_COLLECTION))
		state |= E_CAL_BASE_SHELL_SIDEBAR_CLICKED_SOURCE_IS_COLLECTION;
	if (e_source_selector_count_total (selector) == e_source_selector_count_selected (selector))
		state |= E_CAL_BASE_SHELL_SIDEBAR_ALL_SOURCES_SELECTED;
	if (has_primary_source)
		state |= E_CAL_BASE_SHELL_SIDEBAR_HAS_PRIMARY_SOURCE;
	if (is_writable)
		state |= E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_WRITABLE;
	if (is_removable)
		state |= E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOVABLE;
	if (is_remote_creatable)
		state |= E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_CREATABLE;
	if (is_remote_deletable)
		state |= E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IS_REMOTE_DELETABLE;
	if (in_collection)
		state |= E_CAL_BASE_SHELL_SIDEBAR_PRIMARY_SOURCE_IN_COLLECTION;
	if (refresh_supported)
		state |= E_CAL_BASE_SHELL_SIDEBAR_SOURCE_SUPPORTS_REFRESH;

	return state;
}

static gboolean
cal_base_shell_sidebar_date_navigator_scroll_event_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
						       GdkEventScroll *event,
						       ECalendar *date_navigator)
{
	ECalendarItem *calitem;
	gint year = -1, month = -1;
	GdkScrollDirection direction;

	calitem = e_calendar_get_item (date_navigator);
	e_calendar_item_get_first_month (calitem, &year, &month);
	if (year == -1 || month == -1)
		return FALSE;

	direction = event->direction;

	if (direction == GDK_SCROLL_SMOOTH) {
		static gdouble total_delta_y = 0.0;

		total_delta_y += event->delta_y;

		if (total_delta_y >= 1.0) {
			total_delta_y = 0.0;
			direction = GDK_SCROLL_DOWN;
		} else if (total_delta_y <= -1.0) {
			total_delta_y = 0.0;
			direction = GDK_SCROLL_UP;
		} else {
			return FALSE;
		}
	}

	switch (direction) {
		case GDK_SCROLL_UP:
			month--;
			if (month < 0) {
				year--;
				month += 12;
			}
			break;

		case GDK_SCROLL_DOWN:
			month++;
			if (month >= 12) {
				year++;
				month -= 12;
			}
			break;

		default:
			g_return_val_if_reached (FALSE);
	}

	e_calendar_item_set_first_month (calitem, year, month);

	return TRUE;
}

typedef struct _OpenClientData {
	const gchar *extension_name;
	ECalBaseShellSidebar *sidebar;
	ESource *source;
	EClient *client;
	gboolean was_cancelled;
	ECalBaseShellSidebarOpenFunc cb;
	gpointer cb_user_data;
} OpenClientData;

static void
open_client_data_free (gpointer pdata)
{
	OpenClientData *data = pdata;

	if (data) {
		if (!data->client || (data->cb && g_hash_table_lookup (data->sidebar->priv->selected_uids, e_source_get_uid (data->source)))) {
			g_hash_table_remove (data->sidebar->priv->selected_uids, e_source_get_uid (data->source));
		} else {
			/* To free the cancellable in the 'value' pair, which is useless now */
			g_hash_table_insert (data->sidebar->priv->selected_uids,
				g_strdup (e_source_get_uid (data->source)),
				NULL);
		}

		if (data->cb) {
			if (data->client)
				data->cb (data->sidebar, data->client, data->cb_user_data);
		} else if (data->client) {
			g_signal_emit (data->sidebar, signals[CLIENT_OPENED], 0, data->client);
		} else if (!data->was_cancelled) {
			ESourceSelector *selector = e_cal_base_shell_sidebar_get_selector (data->sidebar);
			e_source_selector_unselect_source (selector, data->source);
		}

		g_clear_object (&data->sidebar);
		g_clear_object (&data->source);
		g_clear_object (&data->client);
		g_slice_free (OpenClientData, data);
	}
}

static void
e_cal_base_shell_sidebar_open_client_thread (EAlertSinkThreadJobData *job_data,
					     gpointer user_data,
					     GCancellable *cancellable,
					     GError **error)
{
	EClientSelector *selector;
	OpenClientData *data = user_data;
	GError *local_error = NULL;

	g_return_if_fail (data != NULL);

	selector = E_CLIENT_SELECTOR (e_cal_base_shell_sidebar_get_selector (data->sidebar));
	data->client = e_client_selector_get_client_sync (
		selector, data->source, TRUE, (guint32) -1, cancellable, &local_error);
	data->was_cancelled = g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

	e_util_propagate_open_source_job_error (job_data, data->extension_name, local_error, error);
}

static void
e_cal_base_shell_sidebar_ensure_source_opened (ECalBaseShellSidebar *sidebar,
					       ESource *source,
					       ECalBaseShellSidebarOpenFunc cb,
					       gpointer cb_user_data)
{
	OpenClientData *data;
	EShellView *shell_view;
	EActivity *activity;
	gchar *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL, *display_name;
	const gchar *extension_name = NULL;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Skip it when it's already opening or opened and the callback is not set */
	if (!cb && g_hash_table_contains (sidebar->priv->selected_uids, e_source_get_uid (source)))
		return;

	shell_view = e_shell_sidebar_get_shell_view (E_SHELL_SIDEBAR (sidebar));

	switch (e_cal_base_shell_view_get_source_type (shell_view)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			return;
	}

	display_name = e_util_get_source_full_name (e_shell_get_registry (e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view))), source);

	if (!e_util_get_open_source_job_info (extension_name, display_name,
		&description, &alert_ident, &alert_arg_0)) {
		g_free (display_name);
		g_warn_if_reached ();
		return;
	}

	g_free (display_name);

	data = g_slice_new0 (OpenClientData);
	data->extension_name = extension_name; /* no need to copy, it's a static string */
	data->sidebar = g_object_ref (sidebar);
	data->source = g_object_ref (source);
	data->cb = cb;
	data->cb_user_data = cb_user_data;

	activity = e_shell_view_submit_thread_job (
		shell_view, description, alert_ident, alert_arg_0,
		e_cal_base_shell_sidebar_open_client_thread, data,
		open_client_data_free);

	if (activity) {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (activity);

		g_hash_table_insert (sidebar->priv->selected_uids,
			g_strdup (e_source_get_uid (source)),
			g_object_ref (cancellable));

		g_object_unref (activity);
	}

	g_free (description);
	g_free (alert_ident);
	g_free (alert_arg_0);
}

static void
e_cal_base_shell_sidebar_primary_selection_changed_cb (ESourceSelector *selector,
						       EShellSidebar *sidebar)
{
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (sidebar));

	e_shell_view_update_actions (e_shell_sidebar_get_shell_view (sidebar));
}

static void
e_cal_base_shell_sidebar_source_selected (ESourceSelector *selector,
					  ESource *source,
					  ECalBaseShellSidebar *sidebar)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (sidebar));

	if (!g_hash_table_contains (sidebar->priv->selected_uids, e_source_get_uid (source))) {
		e_cal_base_shell_sidebar_ensure_source_opened (sidebar, source, NULL, NULL);
	}
}

static void
e_cal_base_shell_sidebar_source_unselected (ESourceSelector *selector,
					    ESource *source,
					    ECalBaseShellSidebar *sidebar)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (sidebar));

	if (g_hash_table_remove (sidebar->priv->selected_uids, e_source_get_uid (source)))
		g_signal_emit (sidebar, signals[CLIENT_CLOSED], 0, source);
}

typedef struct {
	ESource *source;
	ESource *destination;
	gboolean do_copy;
	ICalComponent *icomp;
	EClientSelector *selector;
} TransferItemToData;

static void
transfer_item_to_data_free (gpointer ptr)
{
	TransferItemToData *titd = ptr;

	if (titd) {
		g_clear_object (&titd->source);
		g_clear_object (&titd->destination);
		g_clear_object (&titd->selector);
		g_clear_object (&titd->icomp);
		g_slice_free (TransferItemToData, titd);
	}
}

static void
cal_base_shell_sidebar_transfer_thread (EAlertSinkThreadJobData *job_data,
					gpointer user_data,
					GCancellable *cancellable,
					GError **error)
{
	TransferItemToData *titd = user_data;
	EClient *source_client, *destination_client;

	g_return_if_fail (titd != NULL);
	g_return_if_fail (E_IS_SOURCE (titd->source));
	g_return_if_fail (E_IS_SOURCE (titd->destination));
	g_return_if_fail (E_IS_CLIENT_SELECTOR (titd->selector));
	g_return_if_fail (titd->icomp != NULL);

	source_client = e_client_selector_get_client_sync (
		titd->selector, titd->source, FALSE, (guint32) -1, cancellable, error);
	if (!source_client)
		return;

	destination_client = e_client_selector_get_client_sync (
		titd->selector, titd->destination, FALSE, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, error);
	if (!destination_client) {
		g_object_unref (source_client);
		return;
	}

	cal_comp_transfer_item_to_sync (E_CAL_CLIENT (source_client), E_CAL_CLIENT (destination_client),
		titd->icomp, titd->do_copy, cancellable, error);

	g_clear_object (&source_client);
	g_clear_object (&destination_client);
}

static gboolean
e_cal_base_shell_sidebar_selector_data_dropped (ESourceSelector *selector,
						GtkSelectionData *selection_data,
						ESource *destination,
						GdkDragAction action,
						guint info,
						ECalBaseShellSidebar *sidebar)
{
	ICalComponent *icomp = NULL;
	EActivity *activity;
	EShellView *shell_view;
	ESource *source = NULL;
	ESourceRegistry *registry;
	gchar **segments;
	gchar *source_uid = NULL;
	gchar *message = NULL;
	gchar *display_name = NULL;
	const gchar *alert_ident = NULL;
	const guchar *data;
	gboolean do_copy;
	TransferItemToData *titd;

	g_return_val_if_fail (E_IS_SOURCE_SELECTOR (selector), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (destination), FALSE);
	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (sidebar), FALSE);

	data = gtk_selection_data_get_data (selection_data);
	g_return_val_if_fail (data != NULL, FALSE);

	segments = g_strsplit ((const gchar *) data, "\n", 2);
	if (g_strv_length (segments) != 2)
		goto exit;

	source_uid = g_strdup (segments[0]);
	icomp = i_cal_parser_parse_string (segments[1]);

	if (!icomp)
		goto exit;

	registry = e_source_selector_get_registry (selector);
	source = e_source_registry_ref_source (registry, source_uid);
	if (!source)
		goto exit;

	display_name = e_util_get_source_full_name (registry, destination);
	do_copy = action == GDK_ACTION_COPY ? TRUE : FALSE;
	shell_view = e_shell_sidebar_get_shell_view (E_SHELL_SIDEBAR (sidebar));

	switch (e_cal_base_shell_view_get_source_type (shell_view)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			message = do_copy ?
				g_strdup_printf (_("Copying an event into the calendar “%s”"), display_name) :
				g_strdup_printf (_("Moving an event into the calendar “%s”"), display_name);
			alert_ident = do_copy ? "calendar:failed-copy-event" : "calendar:failed-move-event";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			message = do_copy ?
				g_strdup_printf (_("Copying a memo into the memo list “%s”"), display_name) :
				g_strdup_printf (_("Moving a memo into the memo list “%s”"), display_name);
			alert_ident = do_copy ? "calendar:failed-copy-memo" : "calendar:failed-move-memo";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			message = do_copy ?
				g_strdup_printf (_("Copying a task into the task list “%s”"), display_name) :
				g_strdup_printf (_("Moving a task into the task list “%s”"), display_name);
			alert_ident = do_copy ? "calendar:failed-copy-task" : "calendar:failed-move-task";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			goto exit;
	}

	titd = g_slice_new0 (TransferItemToData);
	titd->source = g_object_ref (source);
	titd->destination = g_object_ref (destination);
	titd->do_copy = do_copy;
	titd->icomp = icomp;
	titd->selector = E_CLIENT_SELECTOR (g_object_ref (selector));

	icomp = NULL;

	activity = e_shell_view_submit_thread_job (shell_view, message,
		alert_ident, display_name, cal_base_shell_sidebar_transfer_thread,
		titd, transfer_item_to_data_free);

	g_clear_object (&activity);

 exit:
	g_clear_object (&icomp);
	g_clear_object (&source);
	g_free (message);
	g_free (source_uid);
	g_free (display_name);
	g_strfreev (segments);

	return TRUE;
}

static void
cancel_and_unref (gpointer data)
{
	GCancellable *cancellable = data;

	if (cancellable) {
		g_cancellable_cancel (cancellable);
		g_object_unref (cancellable);
	}
}

static void
cal_base_shell_sidebar_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATE_NAVIGATOR:
			g_value_set_object (
				value,
				e_cal_base_shell_sidebar_get_date_navigator (
				E_CAL_BASE_SHELL_SIDEBAR (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value,
				e_cal_base_shell_sidebar_get_selector (
				E_CAL_BASE_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_cal_base_shell_sidebar_update_calendar_margin_cb (GObject *object,
						    GParamSpec *pspec,
						    gpointer *user_data)
{
	EShellView *shell_view = E_SHELL_VIEW (object);
	GtkWidget *calendar = GTK_WIDGET (user_data);
	gboolean switcher_visible;

	switcher_visible = e_shell_view_get_switcher_visible (shell_view);

	if (switcher_visible)
		gtk_widget_set_margin_bottom (calendar, 0);
	else
		gtk_widget_set_margin_bottom (calendar, 6);
}

static void
cal_base_shell_sidebar_constructed (GObject *object)
{
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShell *shell;
	EClientCache *client_cache;
	const gchar *source_extension = NULL, *selector_name = NULL, *restore_state_signal = NULL;
	ECalBaseShellSidebar *cal_base_shell_sidebar;
	GtkWidget *container, *widget;
	AtkObject *a11y;
	gboolean add_navigator = FALSE;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_base_shell_sidebar_parent_class)->constructed (object);

	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (E_SHELL_SIDEBAR (object));
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	switch (e_cal_base_shell_view_get_source_type (shell_view)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			source_extension = E_SOURCE_EXTENSION_CALENDAR;
			selector_name = _("Calendar Selector");
			restore_state_signal = "shell-view-created::calendar";
			add_navigator = TRUE;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			source_extension = E_SOURCE_EXTENSION_MEMO_LIST;
			selector_name = _("Memo List Selector");
			restore_state_signal = "shell-view-created::memos";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			source_extension = E_SOURCE_EXTENSION_TASK_LIST;
			selector_name = _("Task List Selector");
			restore_state_signal = "shell-view-created::tasks";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			return;
	}

	client_cache = e_shell_get_client_cache (shell);

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	cal_base_shell_sidebar->priv->paned = widget;

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, TRUE);

	container = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	container = widget;

	widget = e_client_selector_new (client_cache, source_extension);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, selector_name);
	cal_base_shell_sidebar->priv->selector = E_SOURCE_SELECTOR (widget);
	gtk_container_add (GTK_CONTAINER (container), widget);

	e_source_selector_load_groups_setup (cal_base_shell_sidebar->priv->selector,
		e_shell_view_get_state_key_file (shell_view));

	if (add_navigator) {
		ECalendarItem *calitem;

		container = cal_base_shell_sidebar->priv->paned;

		widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
		gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (widget), FALSE);
		gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
		gtk_widget_show (widget);

		container = widget;

		widget = e_calendar_new ();
		gtk_widget_set_margin_top (widget, 6);
		gtk_widget_set_margin_start (widget, 6);
		gtk_widget_set_margin_end (widget, 6);
		calitem = e_calendar_get_item (E_CALENDAR (widget));
		e_calendar_item_set_days_start_week_sel (calitem, 9);
		e_calendar_item_set_max_days_sel (calitem, 42);
		gtk_container_add (GTK_CONTAINER (container), widget);
		cal_base_shell_sidebar->priv->date_navigator = E_CALENDAR (widget);
		gtk_widget_show (widget);

		e_binding_bind_property (
			widget, "visible",
			container, "visible",
			G_BINDING_SYNC_CREATE);

		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (e_calendar_get_item (cal_base_shell_sidebar->priv->date_navigator)),
			"move-selection-when-moving", FALSE,
			NULL);

		cal_base_shell_sidebar->priv->date_navigator_scroll_event_handler_id = g_signal_connect_swapped (
			cal_base_shell_sidebar->priv->date_navigator, "scroll-event",
			G_CALLBACK (cal_base_shell_sidebar_date_navigator_scroll_event_cb), cal_base_shell_sidebar);
	}

	gtk_widget_show_all (GTK_WIDGET (object));

	gtk_drag_dest_set (
		GTK_WIDGET (cal_base_shell_sidebar->priv->selector), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_drag_dest_add_calendar_targets (GTK_WIDGET (cal_base_shell_sidebar->priv->selector));

	g_signal_connect (shell_view,
		"notify::switcher-visible", G_CALLBACK (e_cal_base_shell_sidebar_update_calendar_margin_cb),
		widget);

	g_signal_connect (cal_base_shell_sidebar->priv->selector,
		"data-dropped", G_CALLBACK (e_cal_base_shell_sidebar_selector_data_dropped),
		cal_base_shell_sidebar);

	g_signal_connect (cal_base_shell_sidebar->priv->selector,
		"primary-selection-changed", G_CALLBACK (e_cal_base_shell_sidebar_primary_selection_changed_cb),
		cal_base_shell_sidebar);

	g_signal_connect (cal_base_shell_sidebar->priv->selector,
		"source-selected", G_CALLBACK (e_cal_base_shell_sidebar_source_selected),
		cal_base_shell_sidebar);

	g_signal_connect (cal_base_shell_sidebar->priv->selector,
		"source-unselected", G_CALLBACK (e_cal_base_shell_sidebar_source_unselected),
		cal_base_shell_sidebar);

	/* Restore widget state from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, restore_state_signal,
		G_CALLBACK (cal_base_shell_sidebar_restore_state_cb),
		cal_base_shell_sidebar);
}

static void
cal_base_shell_sidebar_dispose (GObject *object)
{
	ECalBaseShellSidebar *cal_base_shell_sidebar;

	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (object);

	if (cal_base_shell_sidebar->priv->date_navigator_scroll_event_handler_id > 0 &&
	    cal_base_shell_sidebar->priv->date_navigator) {
		g_signal_handler_disconnect (cal_base_shell_sidebar->priv->date_navigator,
			cal_base_shell_sidebar->priv->date_navigator_scroll_event_handler_id);
		cal_base_shell_sidebar->priv->date_navigator_scroll_event_handler_id = 0;
	}

	cal_base_shell_sidebar->priv->date_navigator = NULL;
	cal_base_shell_sidebar->priv->selector = NULL;
	cal_base_shell_sidebar->priv->paned = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_base_shell_sidebar_parent_class)->dispose (object);
}

static void
cal_base_shell_sidebar_finalize (GObject *object)
{
	ECalBaseShellSidebar *cal_base_shell_sidebar;

	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (object);

	g_hash_table_destroy (cal_base_shell_sidebar->priv->selected_uids);
	cal_base_shell_sidebar->priv->selected_uids = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_base_shell_sidebar_parent_class)->finalize (object);
}

static void
e_cal_base_shell_sidebar_class_init (ECalBaseShellSidebarClass *class)
{
	GObjectClass *object_class;
	EShellSidebarClass *shell_sidebar_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_base_shell_sidebar_get_property;
	object_class->constructed = cal_base_shell_sidebar_constructed;
	object_class->dispose = cal_base_shell_sidebar_dispose;
	object_class->finalize = cal_base_shell_sidebar_finalize;

	shell_sidebar_class = E_SHELL_SIDEBAR_CLASS (class);
	shell_sidebar_class->check_state = cal_base_shell_sidebar_check_state;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			"Source Selector Widget",
			"This widget displays groups of calendars",
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_DATE_NAVIGATOR,
		g_param_spec_object (
			"date-navigator",
			"Date Navigator Widget",
			"This widget displays a miniature calendar",
			E_TYPE_CALENDAR,
			G_PARAM_READABLE));

	signals[CLIENT_OPENED] = g_signal_new (
		"client-opened",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalBaseShellSidebarClass, client_opened),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL_CLIENT);

	signals[CLIENT_CLOSED] = g_signal_new (
		"client-closed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalBaseShellSidebarClass, client_closed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);
}

static void
e_cal_base_shell_sidebar_class_finalize (ECalBaseShellSidebarClass *class)
{
}

static void
e_cal_base_shell_sidebar_init (ECalBaseShellSidebar *cal_base_shell_sidebar)
{
	cal_base_shell_sidebar->priv = e_cal_base_shell_sidebar_get_instance_private (cal_base_shell_sidebar);
	cal_base_shell_sidebar->priv->selected_uids =
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cancel_and_unref);
}

void
e_cal_base_shell_sidebar_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_base_shell_sidebar_register_type (type_module);
}

GtkWidget *
e_cal_base_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_BASE_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ECalendar *
e_cal_base_shell_sidebar_get_date_navigator (ECalBaseShellSidebar *cal_base_shell_sidebar)
{
	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (cal_base_shell_sidebar), NULL);

	return cal_base_shell_sidebar->priv->date_navigator;
}

ESourceSelector *
e_cal_base_shell_sidebar_get_selector (ECalBaseShellSidebar *cal_base_shell_sidebar)
{
	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (cal_base_shell_sidebar), NULL);

	return cal_base_shell_sidebar->priv->selector;
}

void
e_cal_base_shell_sidebar_ensure_sources_open (ECalBaseShellSidebar *cal_base_shell_sidebar)
{
	GList *selected, *link;
	ESourceSelector *selector;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (cal_base_shell_sidebar));

	selector = cal_base_shell_sidebar->priv->selector;
	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));

	selected = e_source_selector_get_selection (selector);
	for (link = selected; link; link = g_list_next (link)) {
		ESource *source = link->data;

		e_cal_base_shell_sidebar_ensure_source_opened (cal_base_shell_sidebar, source, NULL, NULL);
	}

	g_list_free_full (selected, g_object_unref);
}

/* Opens the client with given uid. Calls the cb only if it succeeded */
void
e_cal_base_shell_sidebar_open_source (ECalBaseShellSidebar *cal_base_shell_sidebar,
				      ESource *source,
				      ECalBaseShellSidebarOpenFunc cb,
				      gpointer cb_user_data)
{
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (cal_base_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (cb != NULL);

	e_cal_base_shell_sidebar_ensure_source_opened (cal_base_shell_sidebar, source, cb, cb_user_data);
}

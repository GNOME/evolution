/*
 * e-cal-shell-view-private.c
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

#include "evolution-config.h"

#include "e-util/e-util-private.h"
#include "calendar/gui/e-cal-ops.h"
#include "calendar/gui/e-year-view.h"

#include "e-cal-shell-view-private.h"

#define CHECK_NB	5

/* be compatible with older e-d-s for MeeGo */
#ifndef ETC_TIMEZONE
#  define ETC_TIMEZONE        "/etc/timezone"
#  define ETC_TIMEZONE_MAJ    "/etc/TIMEZONE"
#  define ETC_RC_CONF         "/etc/rc.conf"
#  define ETC_SYSCONFIG_CLOCK "/etc/sysconfig/clock"
#  define ETC_CONF_D_CLOCK    "/etc/conf.d/clock"
#  define ETC_LOCALTIME       "/etc/localtime"
#endif

static const gchar * files_to_check[CHECK_NB] = {
        ETC_TIMEZONE,
        ETC_TIMEZONE_MAJ,
        ETC_SYSCONFIG_CLOCK,
        ETC_CONF_D_CLOCK,
        ETC_LOCALTIME
};

static struct tm
cal_shell_view_get_current_time (ECalendarItem *calitem,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ICalTime *tt;
	ICalTimezone *timezone;
	ECalModel *model;
	struct tm tm;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	timezone = e_cal_model_get_timezone (model);

	tt = i_cal_time_new_from_timet_with_zone (time (NULL), FALSE, timezone);
	tm = e_cal_util_icaltime_to_tm (tt);
	g_clear_object (&tt);

	return tm;
}

static void
cal_shell_view_popup_event_cb (EShellView *shell_view,
                               GdkEvent *button_event)
{
	ECalShellView *self = E_CAL_SHELL_VIEW (shell_view);
	ECalendarView *view;
	GSList *selected;
	const gchar *widget_path;
	gint n_selected;

	view = e_cal_shell_content_get_current_calendar_view (self->priv->cal_shell_content);

	selected = e_calendar_view_get_selected_events (view);
	n_selected = g_slist_length (selected);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);

	if (n_selected <= 0)
		widget_path = "calendar-empty-popup";
	else
		widget_path = "calendar-event-popup";

	e_cal_base_shell_view_show_popup_menu (shell_view, widget_path, button_event, NULL);
}

static gboolean
cal_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                        ESource *clicked_source,
                                        GdkEvent *button_event)
{
	e_cal_base_shell_view_show_popup_menu (shell_view, "calendar-popup", button_event, clicked_source);

	return TRUE;
}

static void
cal_shell_view_memopad_popup_event_cb (EShellView *shell_view,
                                       GdkEvent *button_event)
{
	e_cal_shell_view_memopad_actions_update (E_CAL_SHELL_VIEW (shell_view));

	e_cal_base_shell_view_show_popup_menu (shell_view, "calendar-memopad-popup", button_event, NULL);
}

static void
cal_shell_view_taskpad_popup_event_cb (EShellView *shell_view,
                                       GdkEvent *button_event)
{
	e_cal_shell_view_taskpad_actions_update (E_CAL_SHELL_VIEW (shell_view));

	e_cal_base_shell_view_show_popup_menu (shell_view, "calendar-taskpad-popup", button_event, NULL);
}

static void
cal_shell_view_backend_error_cb (EClientCache *client_cache,
                                 EClient *client,
                                 EAlert *alert,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ESource *source;
	const gchar *extension_name;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;

	source = e_client_get_source (client);
	extension_name = E_SOURCE_EXTENSION_CALENDAR;

	/* Only submit alerts from calendar backends. */
	if (e_source_has_extension (source, extension_name)) {
		EAlertSink *alert_sink;

		alert_sink = E_ALERT_SINK (cal_shell_content);
		e_alert_sink_submit_alert (alert_sink, alert);
	}
}

static void
cal_shell_view_save_last_list_view (EShellView *shell_view,
				    const gchar *view_id)
{
	GKeyFile *state_key_file;
	gchar *saved_view_id;

	state_key_file = e_shell_view_get_state_key_file (shell_view);
	saved_view_id = g_key_file_get_string (state_key_file, "Calendar", "LastListView", NULL);

	if (!view_id)
		view_id = "";

	if (g_strcmp0 (saved_view_id, view_id) != 0) {
		g_key_file_set_string (state_key_file, "Calendar", "LastListView", view_id);
		e_shell_view_set_state_dirty (shell_view);
	}

	g_free (saved_view_id);
}

static void
cal_shell_view_set_custom_view (GalViewInstance *view_instance)
{
	gint ii, count;

	g_return_if_fail (view_instance != NULL);

	count = gal_view_collection_get_count (view_instance->collection);

	for (ii = 0; ii < count; ii++) {
		GalViewCollectionItem *item;

		item = gal_view_collection_get_view_item (view_instance->collection, ii);

		if (item && g_strcmp0 (item->id, "List_View") == 0) {
			GalView *view;

			view = gal_view_clone (item->view);
			gal_view_load (view, view_instance->custom_filename);
			gal_view_instance_set_custom_view (view_instance, view);
			g_clear_object (&view);
			break;
		}
	}
}

static void
cal_shell_view_notify_view_id_cb (EShellView *shell_view)
{
	static gboolean inside = FALSE;
	ECalShellContent *cal_shell_content;
	GalViewInstance *view_instance;
	GKeyFile *state_key_file;
	gchar *last_list_view = NULL;
	gboolean was_list_view, content_initialized;
	gchar *was_view_id = NULL;
	const gchar *view_id;

	/* Avoid recursion */
	if (inside)
		return;

	inside = TRUE;

	view_id = e_shell_view_get_view_id (shell_view);

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL) {
		/* Custom view is always List_View */
		cal_shell_view_save_last_list_view (shell_view, "");
		inside = FALSE;
		return;
	}

	view_instance = e_shell_view_get_view_instance (shell_view);
	state_key_file = e_shell_view_get_state_key_file (shell_view);

	was_list_view = g_strcmp0 (view_instance->current_type, "etable") == 0;
	if (was_list_view)
		was_view_id = g_strdup (view_instance->current_id);

	cal_shell_content = E_CAL_SHELL_CONTENT (e_shell_view_get_shell_content (shell_view));
	content_initialized = e_cal_shell_content_get_initialized (cal_shell_content);

	if ((!content_initialized || !was_list_view) && g_strcmp0 (view_id, "List_View") == 0) {
		GError *local_error = NULL;

		last_list_view = g_key_file_get_string (state_key_file, "Calendar", "LastListView", &local_error);

		if (!g_error_matches (local_error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND) &&
		    !g_error_matches (local_error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
			if (last_list_view && !*last_list_view) {
				g_clear_pointer (&last_list_view, g_free);
			}

			if (!last_list_view ||
			    gal_view_collection_get_view_index_by_id (view_instance->collection, last_list_view) >= 0) {
				view_id = last_list_view;
			}
		}

		g_clear_error (&local_error);
	}

	if (view_id && g_strcmp0 (view_id, e_shell_view_get_view_id (shell_view)) != 0)
		e_shell_view_set_view_id (shell_view, view_id);
	else if (view_id)
		gal_view_instance_set_current_view_id (view_instance, view_id);
	else
		cal_shell_view_set_custom_view (view_instance);

	if (g_strcmp0 (view_instance->current_type, "etable") == 0) {
		cal_shell_view_save_last_list_view (shell_view, view_instance->current_id);
	} else if (was_list_view) {
		cal_shell_view_save_last_list_view (shell_view, was_view_id);
	}

	g_free (last_list_view);
	g_free (was_view_id);

	inside = FALSE;
}

static void
cal_shell_view_taskpad_settings_changed_cb (GSettings *settings,
					    const gchar *key,
					    ECalShellView *cal_shell_view)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (cal_shell_view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (cal_shell_view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (cal_shell_view->priv->old_settings, key);

		if ((g_strcmp0 (key, "hide-completed-tasks-units") == 0 ||
		    g_strcmp0 (key, "hide-completed-tasks-value") == 0) &&
		    !calendar_config_get_hide_completed_tasks ())
			return;

		if (cal_shell_view->priv->cal_shell_content) {
			ECalModel *model;
			gchar *cal_filter;

			model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_view->priv->cal_shell_content));
			cal_filter = e_cal_data_model_dup_filter (e_cal_model_get_data_model (model));

			e_cal_shell_content_update_tasks_filter (cal_shell_view->priv->cal_shell_content, cal_filter);

			g_free (cal_filter);
		}
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

void
e_cal_shell_view_private_init (ECalShellView *cal_shell_view)
{
	cal_shell_view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	e_signal_connect_notify (
		cal_shell_view, "notify::view-id",
		G_CALLBACK (cal_shell_view_notify_view_id_cb), NULL);
}

static void
system_timezone_monitor_changed (GFileMonitor *handle,
                                 GFile *file,
                                 GFile *other_file,
                                 GFileMonitorEvent event,
                                 gpointer user_data)
{
	GSettings *settings;

	if (event != G_FILE_MONITOR_EVENT_CHANGED &&
	    event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
	    event != G_FILE_MONITOR_EVENT_DELETED &&
	    event != G_FILE_MONITOR_EVENT_CREATED)
		return;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	/* GSettings Bindings rely on quarks */
	g_signal_emit_by_name (settings, "changed::timezone",
		g_quark_to_string (g_quark_from_string ("timezone")));
	g_object_unref (settings);
}

static void
init_timezone_monitors (ECalShellView *view)
{
	ECalShellViewPrivate *priv = view->priv;
	gint i;

	for (i = 0; i < CHECK_NB; i++) {
		GFile *file;

		file = g_file_new_for_path (files_to_check[i]);
		priv->monitors[i] = g_file_monitor_file (
			file, G_FILE_MONITOR_NONE, NULL, NULL);
		g_object_unref (file);

		if (priv->monitors[i])
			g_signal_connect (
				priv->monitors[i], "changed",
				G_CALLBACK (system_timezone_monitor_changed),
				NULL);
	}
}

static void
cal_shell_view_task_view_notify_state_cb (GObject *object,
					  GParamSpec *param,
					  gpointer user_data)
{
	GAction *action = G_ACTION (object);
	ECalShellView *cal_shell_view = user_data;
	EYearView *year_view;
	GtkOrientation orientation;
	GVariant *state;

	year_view = E_YEAR_VIEW (cal_shell_view->priv->views[E_CAL_VIEW_KIND_YEAR].calendar_view);

	state = g_action_get_state (action);

	switch (g_variant_get_int32 (state)) {
		case 0:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case 1:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_return_if_reached ();
	}

	e_year_view_set_preview_orientation (year_view, orientation);
	g_clear_pointer (&state, g_variant_unref);
}

void
e_cal_shell_view_private_constructed (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShell *shell;
	EShellSearchbar *searchbar;
	ECalendar *calendar;
	EUIAction *action;
	GSettings *settings;
	gulong handler_id;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	/* Cache these to avoid lots of awkward casting. */
	priv->cal_shell_backend = E_CAL_SHELL_BACKEND (g_object_ref (shell_backend));
	priv->cal_shell_content = E_CAL_SHELL_CONTENT (g_object_ref (shell_content));
	priv->cal_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (g_object_ref (shell_sidebar));

	calendar = e_cal_base_shell_sidebar_get_date_navigator (priv->cal_shell_sidebar);

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->client_cache = e_shell_get_client_cache (shell);
	g_object_ref (priv->client_cache);

	handler_id = g_signal_connect (
		priv->client_cache, "backend-error",
		G_CALLBACK (cal_shell_view_backend_error_cb),
		cal_shell_view);
	priv->backend_error_handler_id = handler_id;

	g_signal_connect_swapped (
		e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (priv->cal_shell_content)),
		"time-range-changed", G_CALLBACK (e_cal_shell_view_update_sidebar), cal_shell_view);

	for (ii = E_CAL_VIEW_KIND_DAY; ii < E_CAL_VIEW_KIND_LAST; ii++) {
		ECalendarView *calendar_view;

		/* Keep our own reference to this so we can
		 * disconnect our signal handlers in dispose(). */
		calendar_view = e_cal_shell_content_get_calendar_view (priv->cal_shell_content, ii);
		priv->views[ii].calendar_view = g_object_ref (calendar_view);

		handler_id = g_signal_connect_swapped (
			calendar_view, "popup-event",
			G_CALLBACK (cal_shell_view_popup_event_cb),
			cal_shell_view);
		priv->views[ii].popup_event_handler_id = handler_id;

		handler_id = g_signal_connect_swapped (
			calendar_view, "selection-changed",
			G_CALLBACK (e_shell_view_update_actions_in_idle),
			cal_shell_view);
		priv->views[ii].selection_changed_handler_id = handler_id;

		if (ii == E_CAL_VIEW_KIND_LIST) {
			ECalModel *model;

			model = e_calendar_view_get_model (calendar_view);

			g_signal_connect_object (calendar_view, "selection-changed",
				G_CALLBACK (e_cal_shell_view_update_sidebar), cal_shell_view,
				G_CONNECT_SWAPPED);

			g_signal_connect_object (model, "model-changed",
				G_CALLBACK (e_cal_shell_view_update_sidebar), cal_shell_view,
				G_CONNECT_SWAPPED);

			g_signal_connect_object (model, "model-rows-inserted",
				G_CALLBACK (e_cal_shell_view_update_sidebar), cal_shell_view,
				G_CONNECT_SWAPPED);

			g_signal_connect_object (model, "model-rows-deleted",
				G_CALLBACK (e_cal_shell_view_update_sidebar), cal_shell_view,
				G_CONNECT_SWAPPED);
		}
	}

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->model = e_cal_base_shell_content_get_model (
		E_CAL_BASE_SHELL_CONTENT (shell_content));
	g_object_ref (priv->model);

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->selector = e_cal_base_shell_sidebar_get_selector (
		E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_object_ref (priv->selector);

	handler_id = g_signal_connect_swapped (
		priv->selector, "popup-event",
		G_CALLBACK (cal_shell_view_selector_popup_event_cb),
		cal_shell_view);
	priv->selector_popup_event_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->memo_table = e_cal_shell_content_get_memo_table (
		E_CAL_SHELL_CONTENT (shell_content));
	g_object_ref (priv->memo_table);

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "popup-event",
		G_CALLBACK (cal_shell_view_memopad_popup_event_cb),
		cal_shell_view);
	priv->memo_table_popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "selection-change",
		G_CALLBACK (e_cal_shell_view_memopad_actions_update),
		cal_shell_view);
	priv->memo_table_selection_change_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->task_table = e_cal_shell_content_get_task_table (
		E_CAL_SHELL_CONTENT (shell_content));
	g_object_ref (priv->task_table);

	handler_id = g_signal_connect_swapped (
		priv->task_table, "popup-event",
		G_CALLBACK (cal_shell_view_taskpad_popup_event_cb),
		cal_shell_view);
	priv->task_table_popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->task_table, "selection-change",
		G_CALLBACK (e_cal_shell_view_taskpad_actions_update),
		cal_shell_view);
	priv->task_table_selection_change_handler_id = handler_id;

	e_categories_add_change_hook (
		(GHookFunc) e_cal_shell_view_update_search_filter,
		cal_shell_view);

	e_calendar_item_set_get_time_callback (
		e_calendar_get_item (calendar), (ECalendarItemGetTimeCallback)
		cal_shell_view_get_current_time, cal_shell_view, NULL);

	priv->settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	priv->settings_hide_completed_tasks_handler_id =
		g_signal_connect (priv->settings, "changed::hide-completed-tasks",
		G_CALLBACK (cal_shell_view_taskpad_settings_changed_cb), cal_shell_view);
	priv->settings_hide_completed_tasks_units_handler_id =
		g_signal_connect (priv->settings, "changed::hide-completed-tasks-units",
		G_CALLBACK (cal_shell_view_taskpad_settings_changed_cb), cal_shell_view);
	priv->settings_hide_completed_tasks_value_handler_id =
		g_signal_connect (priv->settings, "changed::hide-completed-tasks-value",
		G_CALLBACK (cal_shell_view_taskpad_settings_changed_cb), cal_shell_view);
	priv->settings_hide_cancelled_tasks_handler_id =
		g_signal_connect (priv->settings, "changed::hide-cancelled-tasks",
		G_CALLBACK (cal_shell_view_taskpad_settings_changed_cb), cal_shell_view);

	init_timezone_monitors (cal_shell_view);

	/* Advanced Search Action */
	action = ACTION (CALENDAR_SEARCH_ADVANCED_HIDDEN);
	e_ui_action_set_visible (action, FALSE);
	searchbar = e_cal_shell_content_get_searchbar (cal_shell_view->priv->cal_shell_content);
	e_shell_searchbar_set_search_option (searchbar, action);

	e_binding_bind_property (
		ACTION (CALENDAR_PREVIEW), "active",
		cal_shell_view->priv->views[E_CAL_VIEW_KIND_YEAR].calendar_view, "preview-visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_cal_shell_view_update_sidebar (cal_shell_view);
	e_cal_shell_view_update_search_filter (cal_shell_view);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "show-tag-vpane",
		ACTION (CALENDAR_SHOW_TAG_VPANE), "active",
		G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	action = ACTION (CALENDAR_PREVIEW);

	g_settings_bind (
		settings, "year-show-preview",
		action, "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	e_binding_bind_property (
		action, "active",
		priv->views[E_CAL_VIEW_KIND_YEAR].calendar_view, "preview-visible",
		G_BINDING_SYNC_CREATE);

	/* use the "horizontal" action, because it's the first in the group and
	   the group is not set yet, due to the UI manager being frozen */
	action = ACTION (CALENDAR_PREVIEW_HORIZONTAL);

	g_settings_bind_with_mapping (
		settings, "year-layout",
		action, "state",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
		e_shell_view_util_layout_to_state_cb,
		e_shell_view_util_state_to_layout_cb, NULL, NULL);

	g_clear_object (&settings);

	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (cal_shell_view_task_view_notify_state_cb), cal_shell_view, 0);

	/* to propagate the loaded state */
	cal_shell_view_task_view_notify_state_cb (G_OBJECT (action), NULL, cal_shell_view);
}

void
e_cal_shell_view_private_dispose (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	gint ii;

	e_cal_shell_view_search_stop (cal_shell_view);

	/* Calling ECalShellContent's save state from here,
	 * because it is too late in its own dispose(). */
	if (priv->cal_shell_content != NULL)
		e_cal_shell_content_save_state (priv->cal_shell_content);

	if (priv->prepare_for_quit_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->shell,
			priv->prepare_for_quit_handler_id);
		priv->prepare_for_quit_handler_id = 0;
	}

	if (priv->backend_error_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_error_handler_id);
		priv->backend_error_handler_id = 0;
	}

	if (priv->selector_popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->selector,
			priv->selector_popup_event_handler_id);
		priv->selector_popup_event_handler_id = 0;
	}

	if (priv->memo_table_popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->memo_table_popup_event_handler_id);
		priv->memo_table_popup_event_handler_id = 0;
	}

	if (priv->memo_table_selection_change_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->memo_table_selection_change_handler_id);
		priv->memo_table_selection_change_handler_id = 0;
	}

	if (priv->task_table_popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->task_table_popup_event_handler_id);
		priv->task_table_popup_event_handler_id = 0;
	}

	if (priv->task_table_selection_change_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->task_table_selection_change_handler_id);
		priv->task_table_selection_change_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_handler_id);
		priv->settings_hide_completed_tasks_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_units_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_units_handler_id);
		priv->settings_hide_completed_tasks_units_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_value_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_value_handler_id);
		priv->settings_hide_completed_tasks_value_handler_id = 0;
	}

	if (priv->settings_hide_cancelled_tasks_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_cancelled_tasks_handler_id);
		priv->settings_hide_cancelled_tasks_handler_id = 0;
	}

	for (ii = 0; ii < E_CAL_VIEW_KIND_LAST; ii++) {
		if (priv->views[ii].popup_event_handler_id > 0) {
			g_signal_handler_disconnect (
				priv->views[ii].calendar_view,
				priv->views[ii].popup_event_handler_id);
			priv->views[ii].popup_event_handler_id = 0;
		}

		if (priv->views[ii].selection_changed_handler_id > 0) {
			g_signal_handler_disconnect (
				priv->views[ii].calendar_view,
				priv->views[ii].selection_changed_handler_id);
			priv->views[ii].selection_changed_handler_id = 0;
		}

		g_clear_object (&priv->views[ii].calendar_view);
	}

	g_clear_object (&priv->cal_shell_backend);
	g_clear_object (&priv->cal_shell_content);
	g_clear_object (&priv->cal_shell_sidebar);

	g_clear_object (&priv->shell);
	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->model);
	g_clear_object (&priv->selector);
	g_clear_object (&priv->settings);
	g_clear_object (&priv->memo_table);
	g_clear_object (&priv->task_table);

	for (ii = 0; ii < CHECK_NB; ii++)
		g_clear_object (&priv->monitors[ii]);
}

void
e_cal_shell_view_private_finalize (ECalShellView *cal_shell_view)
{
	g_clear_pointer (&cal_shell_view->priv->old_settings, g_hash_table_destroy);
}

void
e_cal_shell_view_update_sidebar (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	gchar *description;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;

	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	description = e_calendar_view_get_description_text (calendar_view);

	e_shell_sidebar_set_secondary_text (shell_sidebar, description ? description : "");

	g_free (description);
}

static gint
cal_searching_get_search_range_years (ECalShellView *cal_shell_view)
{
	GSettings *settings;
	gint search_range_years;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	search_range_years =
		g_settings_get_int (settings, "search-range-years");
	if (search_range_years <= 0)
		search_range_years = 10;

	g_object_unref (settings);

	return search_range_years;
}

static gint
cal_time_t_ptr_compare (gconstpointer a,
                        gconstpointer b)
{
	const time_t *ta = a, *tb = b;

	return (ta ? *ta : 0) - (tb ? *tb : 0);
}

static void cal_iterate_searching (ECalShellView *cal_shell_view);

struct GenerateInstancesData {
	ECalClient *client;
	ECalShellView *cal_shell_view;
	GCancellable *cancellable;
};

static void
cal_searching_instances_done_cb (gpointer user_data)
{
	struct GenerateInstancesData *gid = user_data;

	g_return_if_fail (gid != NULL);
	g_return_if_fail (gid->cal_shell_view != NULL);

	if (!g_cancellable_is_cancelled (gid->cancellable)) {
		gid->cal_shell_view->priv->search_pending_count--;
		if (!gid->cal_shell_view->priv->search_pending_count)
			cal_iterate_searching (gid->cal_shell_view);
	}

	g_object_unref (gid->cancellable);
	g_slice_free (struct GenerateInstancesData, gid);
}

static gboolean
cal_searching_got_instance_cb (ICalComponent *icomp,
			       ICalTime *instance_start,
			       ICalTime *instance_end,
			       gpointer user_data,
			       GCancellable *cancellable,
			       GError **error)
{
	struct GenerateInstancesData *gid = user_data;
	ECalShellViewPrivate *priv;
	ICalTime *dtstart = NULL;
	ICalProperty *prop;
	time_t *value, start = (time_t) 0;

	g_return_val_if_fail (gid != NULL, FALSE);

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	g_return_val_if_fail (gid->cal_shell_view != NULL, FALSE);
	g_return_val_if_fail (gid->cal_shell_view->priv != NULL, FALSE);

	prop = i_cal_component_get_first_property (icomp, I_CAL_DTSTART_PROPERTY);
	dtstart = i_cal_component_get_dtstart (icomp);

	if (dtstart && prop) {
		ICalParameter *param;
		const gchar *tzid = NULL;

		param = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);
		if (param)
			tzid = i_cal_parameter_get_tzid (param);

		if (tzid && *tzid) {
			ICalTimezone *zone = NULL;

			if (!e_cal_client_get_timezone_sync (gid->client, tzid, &zone, cancellable, NULL))
				zone = NULL;

			if (g_cancellable_is_cancelled (cancellable)) {
				g_object_unref (dtstart);
				g_clear_object (&param);
				return FALSE;
			}

			if (zone)
				start = i_cal_time_as_timet_with_zone (dtstart, zone);
		}

		g_clear_object (&param);
	}

	g_clear_object (&dtstart);

	if (!start)
		start = i_cal_time_as_timet (instance_start);

	priv = gid->cal_shell_view->priv;
	value = g_new (time_t, 1);
	*value = start;
	if (!g_slist_find_custom (priv->search_hit_cache, value, cal_time_t_ptr_compare))
		priv->search_hit_cache = g_slist_append (priv->search_hit_cache, value);
	else
		g_free (value);

	return TRUE;
}

static void
cal_search_get_object_list_cb (GObject *source,
                               GAsyncResult *result,
                               gpointer user_data)
{
	ECalClient *client = E_CAL_CLIENT (source);
	ECalShellView *cal_shell_view = user_data;
	GSList *icomps = NULL;
	GError *error = NULL;

	g_return_if_fail (client != NULL);
	g_return_if_fail (result != NULL);
	g_return_if_fail (cal_shell_view != NULL);

	e_cal_client_get_object_list_finish (
		client, result, &icomps, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (icomps == NULL);
		g_error_free (error);

	} else if (error != NULL || !icomps) {
		g_warn_if_fail (icomps == NULL);
		g_clear_error (&error);

		cal_shell_view->priv->search_pending_count--;
		if (!cal_shell_view->priv->search_pending_count) {
			cal_iterate_searching (cal_shell_view);
		}

	} else if (cal_shell_view->priv->searching_activity) {
		GSList *iter;
		GCancellable *cancellable;
		time_t start, end;

		cancellable = e_activity_get_cancellable (cal_shell_view->priv->searching_activity);
		start = time_add_day (cal_shell_view->priv->search_time, (-1) * cal_shell_view->priv->search_direction);
		end = cal_shell_view->priv->search_time;
		if (start > end) {
			time_t tmp = start;
			start = end;
			end = tmp;
		}

		for (iter = icomps; iter; iter = iter->next) {
			ICalComponent *icomp = iter->data;
			struct GenerateInstancesData *gid;

			gid = g_slice_new0 (struct GenerateInstancesData);
			gid->client = client;
			gid->cal_shell_view = cal_shell_view;
			gid->cancellable = g_object_ref (cancellable);

			e_cal_client_generate_instances_for_object (
				client, icomp, start, end, cancellable,
				cal_searching_got_instance_cb, gid,
				cal_searching_instances_done_cb);
		}

		e_util_free_nullable_object_slist (icomps);
	} else {
		e_util_free_nullable_object_slist (icomps);
	}
}

static gboolean
cal_searching_check_candidates (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	ECalViewKind view_kind;
	GSList *iter;
	time_t value, candidate = -1;

	g_return_val_if_fail (cal_shell_view != NULL, FALSE);
	g_return_val_if_fail (cal_shell_view->priv != NULL, FALSE);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	view_kind = e_cal_shell_content_get_current_view_id (cal_shell_content);

	if (!e_calendar_view_get_selected_time_range (calendar_view, &value, NULL))
		return FALSE;

	if (cal_shell_view->priv->search_direction > 0 &&
	    (view_kind == E_CAL_VIEW_KIND_WEEK ||
	     view_kind == E_CAL_VIEW_KIND_MONTH))
		value = time_add_day (value, 1);

	cal_shell_view->priv->search_hit_cache =
		g_slist_sort (
			cal_shell_view->priv->search_hit_cache,
			cal_time_t_ptr_compare);

	for (iter = cal_shell_view->priv->search_hit_cache; iter; iter = iter->next) {
		time_t cache = *((time_t *) iter->data);

		/* list is sorted before traversing it */
		if (cache > value) {
			if (cal_shell_view->priv->search_direction > 0)
				candidate = cache;
			break;
		} else if (cal_shell_view->priv->search_direction < 0 && cache != value)
			candidate = cache;
	}

	if (candidate > 0) {
		ICalTime *tt;
		ICalTimezone *zone;
		ECalDataModel *data_model;
		ECalendar *calendar;

		calendar = e_cal_base_shell_sidebar_get_date_navigator (cal_shell_view->priv->cal_shell_sidebar);
		data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_view->priv->cal_shell_content));
		zone = e_cal_data_model_get_timezone (data_model);

		tt = i_cal_time_new_from_timet_with_zone (candidate, FALSE, zone);

		if (tt && i_cal_time_is_valid_time (tt) && !i_cal_time_is_null_time (tt)) {
			ECalendarView *cal_view;
			GDate *dt;

			dt = g_date_new_dmy (i_cal_time_get_day (tt), i_cal_time_get_month (tt), i_cal_time_get_year (tt));
			e_calendar_item_set_selection (e_calendar_get_item (calendar), dt, dt);
			g_signal_emit_by_name (e_calendar_get_item (calendar), "selection-changed", 0);
			g_date_free (dt);

			cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_view->priv->cal_shell_content);
			e_calendar_view_set_selected_time_range (cal_view, candidate, candidate);
		}

		g_clear_object (&tt);

		return TRUE;
	}

	return FALSE;
}

static void
cal_searching_update_alert (ECalShellView *cal_shell_view,
                            const gchar *message)
{
	ECalShellViewPrivate *priv;
	EShellContent *shell_content;
	EAlert *alert;

	g_return_if_fail (cal_shell_view != NULL);
	g_return_if_fail (cal_shell_view->priv != NULL);

	priv = cal_shell_view->priv;

	if (priv->search_alert) {
		e_alert_response (
			priv->search_alert,
			e_alert_get_default_response (priv->search_alert));
		priv->search_alert = NULL;
	}

	if (!message)
		return;

	alert = e_alert_new ("calendar:search-error-generic", message, NULL);
	g_return_if_fail (alert != NULL);

	priv->search_alert = alert;
	g_object_add_weak_pointer (G_OBJECT (alert), &priv->search_alert);
	e_alert_start_timer (priv->search_alert, 5);

	shell_content = e_shell_view_get_shell_content (
		E_SHELL_VIEW (cal_shell_view));
	e_alert_sink_submit_alert (
		E_ALERT_SINK (shell_content), priv->search_alert);
	g_object_unref (priv->search_alert);
}

static void
cal_iterate_searching (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv;
	GList *list, *link;
	ECalDataModel *data_model;
	time_t new_time, range1, range2;
	ICalTimezone *timezone;
	const gchar *default_tzloc = NULL;
	GCancellable *cancellable;
	gchar *sexp, *start, *end, *data_filter;

	g_return_if_fail (cal_shell_view != NULL);
	g_return_if_fail (cal_shell_view->priv != NULL);

	priv = cal_shell_view->priv;
	g_return_if_fail (priv->search_direction != 0);
	g_return_if_fail (priv->search_pending_count == 0);

	cal_searching_update_alert (cal_shell_view, NULL);

	if (cal_searching_check_candidates (cal_shell_view)) {
		if (priv->searching_activity) {
			e_activity_set_state (
				priv->searching_activity,
				E_ACTIVITY_COMPLETED);
			g_object_unref (priv->searching_activity);
			priv->searching_activity = NULL;
		}

		e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));

		return;
	}

	if (!priv->searching_activity) {
		EShellBackend *shell_backend;

		shell_backend = e_shell_view_get_shell_backend (
			E_SHELL_VIEW (cal_shell_view));

		cancellable = g_cancellable_new ();
		priv->searching_activity = e_activity_new ();
		e_activity_set_cancellable (
			priv->searching_activity, cancellable);
		e_activity_set_state (
			priv->searching_activity, E_ACTIVITY_RUNNING);
		e_activity_set_text (
			priv->searching_activity,
			priv->search_direction > 0 ?
			_("Searching next matching event") :
			_("Searching previous matching event"));

		e_shell_backend_add_activity (
			shell_backend, priv->searching_activity);
	}

	new_time = time_add_day (priv->search_time, priv->search_direction);
	if (new_time > priv->search_max_time || new_time < priv->search_min_time) {
		gchar *alert_msg;
		gint range_years;

		/* would get out of bounds, stop searching */
		e_activity_set_state (
			priv->searching_activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->searching_activity);
		priv->searching_activity = NULL;

		range_years = cal_searching_get_search_range_years (cal_shell_view);
		alert_msg = g_strdup_printf (
			priv->search_direction > 0 ?
			ngettext (
			"Cannot find matching event in the next %d year",
			"Cannot find matching event in the next %d years",
			range_years) :
			ngettext (
			"Cannot find matching event in the previous %d year",
			"Cannot find matching event in the previous %d years",
			range_years),
			range_years);
		cal_searching_update_alert (cal_shell_view, alert_msg);
		g_free (alert_msg);

		e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));

		return;
	}

	data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_view->priv->cal_shell_content));
	list = e_cal_data_model_get_clients (data_model);

	if (list == NULL) {
		e_activity_set_state (
			priv->searching_activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->searching_activity);
		priv->searching_activity = NULL;

		cal_searching_update_alert (
			cal_shell_view,
			_("Cannot search with no active calendar"));

		e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));

		return;
	}

	timezone = e_cal_data_model_get_timezone (data_model);
	range1 = priv->search_time;
	range2 = time_add_day (range1, priv->search_direction);
	if (range1 < range2) {
		start = isodate_from_time_t (time_day_begin (range1));
		end = isodate_from_time_t (time_day_end (range2));
	} else {
		start = isodate_from_time_t (time_day_begin (range2));
		end = isodate_from_time_t (time_day_end (range1));
	}

	if (timezone && timezone != i_cal_timezone_get_utc_timezone ())
		default_tzloc = i_cal_timezone_get_location (timezone);
	if (!default_tzloc)
		default_tzloc = "";

	data_filter = e_cal_data_model_dup_filter (data_model);
	sexp = g_strdup_printf (
		"(and %s (occur-in-time-range? "
		"(make-time \"%s\") "
		"(make-time \"%s\") \"%s\"))",
		data_filter, start, end, default_tzloc);

	g_free (data_filter);
	g_free (start);
	g_free (end);

	cancellable = e_activity_get_cancellable (priv->searching_activity);
	priv->search_pending_count = g_list_length (list);
	priv->search_time = new_time;

	for (link = list; link != NULL; link = g_list_next (link)) {
		ECalClient *client = E_CAL_CLIENT (link->data);

		e_cal_client_get_object_list (
			client, sexp, cancellable,
			cal_search_get_object_list_cb, cal_shell_view);
	}

	g_list_free_full (list, g_object_unref);
	g_free (sexp);

	e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));
}

void
e_cal_shell_view_search_events (ECalShellView *cal_shell_view,
                                gboolean search_forward)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	time_t start_time = 0;
	gint range_years;

	if (priv->searching_activity || !priv->search_direction)
		e_cal_shell_view_search_stop (cal_shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	if (!e_calendar_view_get_selected_time_range (calendar_view, &start_time, NULL)) {
		e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));
		return;
	}

	start_time = time_day_begin (start_time);
	if (priv->search_direction) {
		time_t cached_start, cached_end, tmp;

		cached_start = priv->search_time;
		cached_end = time_add_day (
			cached_start, (-1) * priv->search_direction);

		if (priv->search_direction > 0) {
			tmp = cached_start;
			cached_start = cached_end;
			cached_end = tmp;
		}

		/* clear cached results if searching out of cached bounds */
		if (start_time < cached_start || start_time > cached_end)
			e_cal_shell_view_search_stop (cal_shell_view);
	}

	priv->search_direction = search_forward ? +30 : -30;

	if (cal_searching_check_candidates (cal_shell_view)) {
		e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));
		return;
	}

	range_years = cal_searching_get_search_range_years (cal_shell_view);

	priv->search_pending_count = 0;
	priv->search_time = start_time;
	priv->search_min_time = start_time - (range_years * 365 * 24 * 60 * 60);
	priv->search_max_time = start_time + (range_years * 365 * 24 * 60 * 60);

	if (priv->search_min_time < 0)
		priv->search_min_time = 0;
	if (priv->search_hit_cache) {
		g_slist_free_full (priv->search_hit_cache, g_free);
		priv->search_hit_cache = NULL;
	}

	cal_iterate_searching (cal_shell_view);
}

void
e_cal_shell_view_search_stop (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv;

	g_return_if_fail (cal_shell_view != NULL);
	g_return_if_fail (cal_shell_view->priv != NULL);

	priv = cal_shell_view->priv;

	cal_searching_update_alert (cal_shell_view, NULL);

	if (priv->searching_activity) {
		g_cancellable_cancel (
			e_activity_get_cancellable (priv->searching_activity));
		e_activity_set_state (
			priv->searching_activity, E_ACTIVITY_CANCELLED);
		g_object_unref (priv->searching_activity);
		priv->searching_activity = NULL;
	}

	if (priv->search_hit_cache) {
		g_slist_free_full (priv->search_hit_cache, g_free);
		priv->search_hit_cache = NULL;
	}

	priv->search_direction = 0;
}

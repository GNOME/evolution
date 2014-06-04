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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-util/e-util-private.h"

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
	struct icaltimetype tt;
	icaltimezone *timezone;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	model = e_cal_shell_content_get_model (cal_shell_content);
	timezone = e_cal_model_get_timezone (model);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, timezone);

	return icaltimetype_to_tm (&tt);
}

static void
cal_shell_view_date_navigator_date_range_changed_cb (ECalShellView *cal_shell_view,
                                                     ECalendarItem *calitem)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	gnome_calendar_update_query (calendar);
}

static void
cal_shell_view_date_navigator_selection_changed_cb (ECalShellView *cal_shell_view,
                                                    ECalendarItem *calitem)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType switch_to;
	GnomeCalendarViewType view_type;
	GnomeCalendar *calendar;
	ECalModel *model;
	GDate start_date, end_date;
	GDate new_start_date, new_end_date;
	icaltimetype tt;
	icaltimezone *timezone;
	time_t start, end, new_time;
	gboolean starts_on_week_start_day;
	gint new_days_shown, old_days_shown;
	GDateWeekday week_start_day;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	model = gnome_calendar_get_model (calendar);
	view_type = gnome_calendar_get_view (calendar);
	switch_to = view_type;

	timezone = e_cal_model_get_timezone (model);
	week_start_day = e_cal_model_get_week_start_day (model);
	e_cal_model_get_time_range (model, &start, &end);

	time_to_gdate_with_zone (&start_date, start, timezone);
	time_to_gdate_with_zone (&end_date, end, timezone);

	if (view_type == GNOME_CAL_MONTH_VIEW) {
		EWeekView *week_view;
		ECalendarView *calendar_view;
		gboolean multi_week_view;
		gboolean compress_weekend;

		calendar_view = gnome_calendar_get_calendar_view (
			calendar, GNOME_CAL_MONTH_VIEW);

		week_view = E_WEEK_VIEW (calendar_view);
		multi_week_view = e_week_view_get_multi_week_view (week_view);
		compress_weekend = e_week_view_get_compress_weekend (week_view);

		if (week_start_day == G_DATE_SUNDAY &&
				(!multi_week_view || compress_weekend))
			g_date_add_days (&start_date, 1);
	}

	g_date_subtract_days (&end_date, 1);

	e_calendar_item_get_selection (
		calitem, &new_start_date, &new_end_date);

	/* If the selection hasn't changed, just return. */
	if (g_date_compare (&start_date, &new_start_date) == 0 &&
		g_date_compare (&end_date, &new_end_date) == 0)
		return;

	old_days_shown =
		g_date_get_julian (&end_date) -
		g_date_get_julian (&start_date) + 1;
	new_days_shown =
		g_date_get_julian (&new_end_date) -
		g_date_get_julian (&new_start_date) + 1;

	/* If a complete week is selected we show the week view.
	 * Note that if weekends are compressed and the week start
	 * day is set to Sunday, we don't actually show complete
	 * weeks in the week view, so this may need tweaking. */
	starts_on_week_start_day =
		(g_date_get_weekday (&new_start_date) == week_start_day);

	/* Update selection to be in the new time range. */
	tt = icaltime_null_time ();
	tt.year = g_date_get_year (&new_start_date);
	tt.month = g_date_get_month (&new_start_date);
	tt.day = g_date_get_day (&new_start_date);
	new_time = icaltime_as_timet_with_zone (tt, timezone);

	/* Switch views as appropriate, and change the number of
	 * days or weeks shown. */
	if (view_type == GNOME_CAL_WORK_WEEK_VIEW && old_days_shown == new_days_shown) {
		/* keep the work week view when has same days shown */
		switch_to = GNOME_CAL_WORK_WEEK_VIEW;
	} else if (new_days_shown > 9) {
		if (view_type != GNOME_CAL_LIST_VIEW) {
			ECalendarView *calendar_view;

			calendar_view = gnome_calendar_get_calendar_view (
				calendar, GNOME_CAL_MONTH_VIEW);
			e_week_view_set_weeks_shown (
				E_WEEK_VIEW (calendar_view),
				(new_days_shown + 6) / 7);
			switch_to = GNOME_CAL_MONTH_VIEW;
		}
	} else if (new_days_shown == 7 && starts_on_week_start_day) {
		switch_to = GNOME_CAL_WEEK_VIEW;
	} else if (new_days_shown == 1 && (view_type == GNOME_CAL_WORK_WEEK_VIEW || view_type == GNOME_CAL_WEEK_VIEW)) {
		/* preserve week views when clicking on one day */
		switch_to = view_type;
	} else {
		ECalendarView *calendar_view;

		calendar_view = gnome_calendar_get_calendar_view (
			calendar, GNOME_CAL_DAY_VIEW);
		e_day_view_set_days_shown (
			E_DAY_VIEW (calendar_view), new_days_shown);

		if (new_days_shown != 5 || !starts_on_week_start_day)
			switch_to = GNOME_CAL_DAY_VIEW;

		else if (view_type != GNOME_CAL_WORK_WEEK_VIEW)
			switch_to = GNOME_CAL_DAY_VIEW;
	}

	/* Make the views display things properly. */
	gnome_calendar_update_view_times (calendar, new_time);
	gnome_calendar_set_view (calendar, switch_to);
	gnome_calendar_set_range_selected (calendar, TRUE);

	gnome_calendar_notify_dates_shown_changed (calendar);

	g_signal_handlers_block_by_func (
		calitem,
		cal_shell_view_date_navigator_selection_changed_cb, cal_shell_view);

	/* make sure the selected days in the calendar matches shown days */
	e_cal_model_get_time_range (model, &start, &end);

	time_to_gdate_with_zone (&start_date, start, timezone);
	time_to_gdate_with_zone (&end_date, end, timezone);

	g_date_subtract_days (&end_date, 1);

	e_calendar_item_set_selection (calitem, &start_date, &end_date);

	g_signal_handlers_unblock_by_func (
		calitem,
		cal_shell_view_date_navigator_selection_changed_cb, cal_shell_view);
}

static gboolean
cal_shell_view_date_navigator_scroll_event_cb (ECalShellView *cal_shell_view,
                                               GdkEventScroll *event,
                                               ECalendar *date_navigator)
{
	ECalendarItem *calitem;
	GDate start_date, end_date;
	GdkScrollDirection direction;

	calitem = date_navigator->calitem;
	if (!e_calendar_item_get_selection (calitem, &start_date, &end_date))
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
			g_date_subtract_months (&start_date, 1);
			g_date_subtract_months (&end_date, 1);
			break;

		case GDK_SCROLL_DOWN:
			g_date_add_months (&start_date, 1);
			g_date_add_months (&end_date, 1);
			break;

		default:
			g_return_val_if_reached (FALSE);
	}

	/* XXX Does ECalendarItem emit a signal for this?  If so, maybe
	 *     we could move this handler into ECalShellSidebar. */
	e_calendar_item_set_selection (calitem, &start_date, &end_date);

	cal_shell_view_date_navigator_selection_changed_cb (
		cal_shell_view, calitem);

	return TRUE;
}

static void
cal_shell_view_popup_event_cb (EShellView *shell_view,
                               GdkEvent *button_event)
{
	GList *list;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	ECalendarView *view;
	ECalShellViewPrivate *priv;
	const gchar *widget_path;
	gint n_selected;

	priv = E_CAL_SHELL_VIEW_GET_PRIVATE (shell_view);

	calendar = e_cal_shell_content_get_calendar (priv->cal_shell_content);

	view_type = gnome_calendar_get_view (calendar);
	view = gnome_calendar_get_calendar_view (calendar, view_type);

	list = e_calendar_view_get_selected_events (view);
	n_selected = g_list_length (list);
	g_list_free (list);

	if (n_selected <= 0)
		widget_path = "/calendar-empty-popup";
	else
		widget_path = "/calendar-event-popup";

	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);
}

static gboolean
cal_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                        ESource *primary_source,
                                        GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/calendar-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	return TRUE;
}

static void
cal_shell_view_selector_client_added_cb (ECalShellView *cal_shell_view,
                                         ECalClient *client)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	model = gnome_calendar_get_model (calendar);

	if (e_cal_model_add_client (model, client))
		gnome_calendar_update_query (calendar);
}

static void
cal_shell_view_selector_client_removed_cb (ECalShellView *cal_shell_view,
                                           ECalClient *client)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	model = gnome_calendar_get_model (calendar);

	if (e_cal_model_remove_client (model, client))
		gnome_calendar_update_query (calendar);
}

static void
cal_shell_view_memopad_popup_event_cb (EShellView *shell_view,
                                       GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/calendar-memopad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);
}

static void
cal_shell_view_taskpad_popup_event_cb (EShellView *shell_view,
                                       GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/calendar-taskpad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);
}

static void
cal_shell_view_user_created_cb (ECalShellView *cal_shell_view,
                                ECalClient *client,
                                ECalendarView *calendar_view)
{
	ECalShellSidebar *cal_shell_sidebar;

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;

	e_cal_shell_sidebar_add_client (cal_shell_sidebar, E_CLIENT (client));
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
cal_shell_view_notify_view_id_cb (EShellView *shell_view)
{
	GalViewInstance *view_instance;
	const gchar *view_id;

	view_id = e_shell_view_get_view_id (shell_view);
	view_instance = e_shell_view_get_view_instance (shell_view);

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_cal_shell_view_private_init (ECalShellView *cal_shell_view)
{
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

	settings = g_settings_new ("org.gnome.evolution.calendar");
	g_signal_emit_by_name (settings, "changed::timezone", "timezone");
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
	gulong handler_id;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	e_shell_window_add_action_group (shell_window, "calendar");
	e_shell_window_add_action_group (shell_window, "calendar-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->cal_shell_backend = g_object_ref (shell_backend);
	priv->cal_shell_content = g_object_ref (shell_content);
	priv->cal_shell_sidebar = g_object_ref (shell_sidebar);

	handler_id = g_signal_connect_swapped (
		priv->cal_shell_sidebar, "client-added",
		G_CALLBACK (cal_shell_view_selector_client_added_cb),
		cal_shell_view);
	priv->client_added_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->cal_shell_sidebar, "client-removed",
		G_CALLBACK (cal_shell_view_selector_client_removed_cb),
		cal_shell_view);
	priv->client_removed_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->client_cache = e_shell_get_client_cache (shell);
	g_object_ref (priv->client_cache);

	handler_id = g_signal_connect (
		priv->client_cache, "backend-error",
		G_CALLBACK (cal_shell_view_backend_error_cb),
		cal_shell_view);
	priv->backend_error_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->calendar = e_cal_shell_content_get_calendar (
		E_CAL_SHELL_CONTENT (shell_content));
	g_object_ref (priv->calendar);

	handler_id = g_signal_connect_swapped (
		priv->calendar, "dates-shown-changed",
		G_CALLBACK (e_cal_shell_view_update_sidebar),
		cal_shell_view);
	priv->dates_shown_changed_handler_id = handler_id;

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		ECalendarView *calendar_view;

		/* Keep our own reference to this so we can
		 * disconnect our signal handlers in dispose(). */
		calendar_view =
			gnome_calendar_get_calendar_view (priv->calendar, ii);
		priv->views[ii].calendar_view = g_object_ref (calendar_view);

		handler_id = g_signal_connect_swapped (
			calendar_view, "popup-event",
			G_CALLBACK (cal_shell_view_popup_event_cb),
			cal_shell_view);
		priv->views[ii].popup_event_handler_id = handler_id;

		handler_id = g_signal_connect_swapped (
			calendar_view, "selection-changed",
			G_CALLBACK (e_shell_view_update_actions),
			cal_shell_view);
		priv->views[ii].selection_changed_handler_id = handler_id;

		handler_id = g_signal_connect_swapped (
			calendar_view, "user-created",
			G_CALLBACK (cal_shell_view_user_created_cb),
			cal_shell_view);
		priv->views[ii].user_created_handler_id = handler_id;
	}

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->model = e_cal_shell_content_get_model (
		E_CAL_SHELL_CONTENT (shell_content));
	g_object_ref (priv->model);

	handler_id = g_signal_connect_swapped (
		priv->model, "status-message",
		G_CALLBACK (e_cal_shell_view_set_status_message),
		cal_shell_view);
	priv->status_message_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->date_navigator = e_cal_shell_sidebar_get_date_navigator (
		E_CAL_SHELL_SIDEBAR (shell_sidebar));

	handler_id = g_signal_connect_swapped (
		priv->date_navigator, "scroll-event",
		G_CALLBACK (cal_shell_view_date_navigator_scroll_event_cb),
		cal_shell_view);
	priv->scroll_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->date_navigator->calitem, "date-range-changed",
		G_CALLBACK (cal_shell_view_date_navigator_date_range_changed_cb),
		cal_shell_view);
	priv->date_range_changed_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->date_navigator->calitem, "selection-changed",
		G_CALLBACK (cal_shell_view_date_navigator_selection_changed_cb),
		cal_shell_view);
	priv->selection_changed_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->selector = e_cal_shell_sidebar_get_selector (
		E_CAL_SHELL_SIDEBAR (shell_sidebar));
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

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "status-message",
		G_CALLBACK (e_cal_shell_view_memopad_set_status_message),
		cal_shell_view);
	priv->memo_table_status_message_handler_id = handler_id;

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

	handler_id = g_signal_connect_swapped (
		priv->task_table, "status-message",
		G_CALLBACK (e_cal_shell_view_taskpad_set_status_message),
		cal_shell_view);
	priv->task_table_status_message_handler_id = handler_id;

	e_categories_add_change_hook (
		(GHookFunc) e_cal_shell_view_update_search_filter,
		cal_shell_view);

	/* Give GnomeCalendar a handle to the date navigator,
	 * memo and task table. */
	gnome_calendar_set_date_navigator (
		priv->calendar, priv->date_navigator);
	gnome_calendar_set_memo_table (
		priv->calendar, GTK_WIDGET (priv->memo_table));
	gnome_calendar_set_task_table (
		priv->calendar, GTK_WIDGET (priv->task_table));

	e_calendar_item_set_get_time_callback (
		priv->date_navigator->calitem, (ECalendarItemGetTimeCallback)
		cal_shell_view_get_current_time, cal_shell_view, NULL);

	init_timezone_monitors (cal_shell_view);
	e_cal_shell_view_actions_init (cal_shell_view);
	e_cal_shell_view_update_sidebar (cal_shell_view);
	e_cal_shell_view_update_search_filter (cal_shell_view);

	/* Keep the ECalModel in sync with the sidebar. */
	g_object_bind_property (
		shell_sidebar, "default-client",
		priv->model, "default-client",
		G_BINDING_SYNC_CREATE);

	/* Keep the toolbar view buttons in sync with the calendar. */
	g_object_bind_property (
		priv->calendar, "view",
		ACTION (CALENDAR_VIEW_DAY), "current-value",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Force the main calendar to update its default source. */
	g_signal_emit_by_name (priv->selector, "primary-selection-changed");
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

	if (priv->client_added_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->cal_shell_sidebar,
			priv->client_added_handler_id);
		priv->client_added_handler_id = 0;
	}

	if (priv->client_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->cal_shell_sidebar,
			priv->client_removed_handler_id);
		priv->client_removed_handler_id = 0;
	}

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

	if (priv->dates_shown_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar,
			priv->dates_shown_changed_handler_id);
		priv->dates_shown_changed_handler_id = 0;
	}

	if (priv->status_message_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->status_message_handler_id);
		priv->status_message_handler_id = 0;
	}

	if (priv->scroll_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->date_navigator,
			priv->scroll_event_handler_id);
		priv->scroll_event_handler_id = 0;
	}

	if (priv->date_range_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->date_navigator->calitem,
			priv->date_range_changed_handler_id);
		priv->date_range_changed_handler_id = 0;
	}

	if (priv->selection_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->date_navigator->calitem,
			priv->selection_changed_handler_id);
		priv->selection_changed_handler_id = 0;
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

	if (priv->memo_table_status_message_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->memo_table_status_message_handler_id);
		priv->memo_table_status_message_handler_id = 0;
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

	if (priv->task_table_status_message_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->task_table_status_message_handler_id);
		priv->task_table_status_message_handler_id = 0;
	}

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
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

		if (priv->views[ii].user_created_handler_id > 0) {
			g_signal_handler_disconnect (
				priv->views[ii].calendar_view,
				priv->views[ii].user_created_handler_id);
			priv->views[ii].user_created_handler_id = 0;
		}

		g_clear_object (&priv->views[ii].calendar_view);
	}

	g_clear_object (&priv->cal_shell_backend);
	g_clear_object (&priv->cal_shell_content);
	g_clear_object (&priv->cal_shell_sidebar);

	g_clear_object (&priv->shell);
	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->calendar);
	g_clear_object (&priv->model);
	g_clear_object (&priv->date_navigator);
	g_clear_object (&priv->selector);
	g_clear_object (&priv->memo_table);
	g_clear_object (&priv->task_table);

	if (priv->calendar_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_set_state (
			priv->calendar_activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->calendar_activity);
		priv->calendar_activity = NULL;
	}

	if (priv->memopad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_set_state (
			priv->memopad_activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->memopad_activity);
		priv->memopad_activity = NULL;
	}

	if (priv->taskpad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_set_state (
			priv->taskpad_activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->taskpad_activity);
		priv->taskpad_activity = NULL;
	}

	for (ii = 0; ii < CHECK_NB; ii++)
		g_clear_object (&priv->monitors[ii]);
}

void
e_cal_shell_view_private_finalize (ECalShellView *cal_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_cal_shell_view_open_event (ECalShellView *cal_shell_view,
                             ECalModelComponent *comp_data)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceRegistry *registry;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	icalproperty *prop;
	const gchar *uid;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (prop != NULL)
		flags |= COMP_EDITOR_MEETING;

	if (itip_organizer_is_user (registry, comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (itip_sentby_is_user (registry, comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = event_editor_new (comp_data->client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_cal_shell_view_set_status_message (ECalShellView *cal_shell_view,
                                     const gchar *status_message,
                                     gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	activity = cal_shell_view->priv->calendar_activity;

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
			g_object_unref (activity);
			activity = NULL;
		}
	} else if (activity == NULL) {
		activity = e_activity_new ();
		e_activity_set_percent (activity, percent);
		e_activity_set_text (activity, status_message);
		e_shell_backend_add_activity (shell_backend, activity);
	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_text (activity, status_message);
	}

	cal_shell_view->priv->calendar_activity = activity;
}

static void
cal_transferring_update_alert (ECalShellView *cal_shell_view,
                               const gchar *domain,
                               const gchar *calendar,
                               const gchar *message)
{
	ECalShellViewPrivate *priv;
	EShellContent *shell_content;
	EAlert *alert;

	g_return_if_fail (cal_shell_view != NULL);
	g_return_if_fail (cal_shell_view->priv != NULL);

	priv = cal_shell_view->priv;

	if (priv->transfer_alert) {
		e_alert_response (
			priv->transfer_alert,
			e_alert_get_default_response (priv->transfer_alert));
		priv->transfer_alert = NULL;
	}

	if (!message)
		return;

	alert = e_alert_new (domain, calendar, message, NULL);
	g_return_if_fail (alert != NULL);

	priv->transfer_alert = alert;
	g_object_add_weak_pointer (G_OBJECT (alert), &priv->transfer_alert);
	e_alert_start_timer (priv->transfer_alert, 300);

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (cal_shell_view));
	e_alert_sink_submit_alert (E_ALERT_SINK (shell_content), priv->transfer_alert);
	g_object_unref (priv->transfer_alert);
}

typedef struct _TransferItemToData {
	ECalShellView *cal_shell_view;
	EActivity *activity;
	const gchar *display_name;
	gboolean remove;
} TransferItemToData;

static void
transfer_item_to_cb (GObject *src_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	TransferItemToData *titd = user_data;
	GError *error = NULL;
	GCancellable *cancellable;
	gboolean success;

	success = cal_comp_transfer_item_to_finish (E_CAL_CLIENT (src_object), result, &error);

	cancellable = e_activity_get_cancellable (titd->activity);
	e_activity_set_state (
		titd->activity,
		g_cancellable_is_cancelled (cancellable) ? E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	if (!success) {
		cal_transferring_update_alert (
			titd->cal_shell_view,
			titd->remove ? "calendar:failed-move-event" : "calendar:failed-copy-event",
			titd->display_name,
			error->message);
		g_clear_error (&error);
	}

	g_object_unref (titd->activity);
	g_free (titd);
}

void
e_cal_shell_view_transfer_item_to (ECalShellView *cal_shell_view,
                                   ECalendarViewEvent *event,
                                   ECalClient *destination_client,
                                   gboolean remove)
{
	EActivity *activity;
	EShellBackend *shell_backend;
	ESource *source;
	GCancellable *cancellable = NULL;
	gchar *message;
	const gchar *display_name;
	TransferItemToData *titd;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (event != NULL);
	g_return_if_fail (is_comp_data_valid (event) != FALSE);
	g_return_if_fail (E_IS_CAL_CLIENT (destination_client));

	if (!is_comp_data_valid (event))
		return;

	source = e_client_get_source (E_CLIENT (destination_client));
	display_name = e_source_get_display_name (source);

	message = remove ?
		g_strdup_printf (_("Moving an event into the calendar %s"), display_name) :
		g_strdup_printf (_("Copying an event into the calendar %s"), display_name);

	shell_backend = e_shell_view_get_shell_backend (E_SHELL_VIEW (cal_shell_view));

	cancellable = g_cancellable_new ();
	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_state (activity, E_ACTIVITY_RUNNING);
	e_activity_set_text (activity, message);
	g_free (message);

	e_shell_backend_add_activity (shell_backend, activity);

	titd = g_new0 (TransferItemToData, 1);

	titd->cal_shell_view = cal_shell_view;
	titd->activity = activity;
	titd->display_name = display_name;
	titd->remove = remove;

	cal_comp_transfer_item_to (
		event->comp_data->client, destination_client,
		event->comp_data->icalcomp, !remove, cancellable, transfer_item_to_cb, titd);
}

void
e_cal_shell_view_update_sidebar (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	ECalModel *model;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	struct icaltimetype start_tt, end_tt;
	icaltimezone *timezone;
	gchar buffer[512] = { 0 };
	gchar end_buffer[512] = { 0 };

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);

	model = gnome_calendar_get_model (calendar);
	timezone = e_cal_model_get_timezone (model);

	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	if (!e_calendar_view_get_visible_time_range (
		calendar_view, &start_time, &end_time)) {
		e_shell_sidebar_set_secondary_text (shell_sidebar, "");
		return;
	}

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, timezone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (
		start_tt.day, start_tt.month - 1, start_tt.year);

	/* Subtract one from end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, timezone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (
		end_tt.day, end_tt.month - 1, end_tt.year);

	switch (view_type) {
		case GNOME_CAL_DAY_VIEW:
		case GNOME_CAL_WORK_WEEK_VIEW:
		case GNOME_CAL_WEEK_VIEW:
			if (start_tm.tm_year == end_tm.tm_year &&
				start_tm.tm_mon == end_tm.tm_mon &&
				start_tm.tm_mday == end_tm.tm_mday) {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%A %d %b %Y"), &start_tm);
			} else if (start_tm.tm_year == end_tm.tm_year) {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%a %d %b"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%a %d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%a %d %b %Y"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%a %d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
			break;

		case GNOME_CAL_MONTH_VIEW:
		case GNOME_CAL_LIST_VIEW:
			if (start_tm.tm_year == end_tm.tm_year) {
				if (start_tm.tm_mon == end_tm.tm_mon) {
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						"%d", &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
						_("%d %b %Y"), &end_tm);
					strcat (buffer, " - ");
					strcat (buffer, end_buffer);
				} else {
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						_("%d %b"), &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
						_("%d %b %Y"), &end_tm);
					strcat (buffer, " - ");
					strcat (buffer, end_buffer);
				}
			} else {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%d %b %Y"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
			break;

		default:
			g_return_if_reached ();
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer);
}

static gint
cal_searching_get_search_range_years (ECalShellView *cal_shell_view)
{
	GSettings *settings;
	gint search_range_years;

	settings = g_settings_new ("org.gnome.evolution.calendar");

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
	g_free (gid);
}

static gboolean
cal_searching_got_instance_cb (ECalComponent *comp,
                               time_t instance_start,
                               time_t instance_end,
                               gpointer user_data)
{
	struct GenerateInstancesData *gid = user_data;
	ECalShellViewPrivate *priv;
	ECalComponentDateTime dt;
	time_t *value;

	g_return_val_if_fail (gid != NULL, FALSE);

	if (g_cancellable_is_cancelled (gid->cancellable))
		return FALSE;

	g_return_val_if_fail (gid->cal_shell_view != NULL, FALSE);
	g_return_val_if_fail (gid->cal_shell_view->priv != NULL, FALSE);

	e_cal_component_get_dtstart (comp, &dt);

	if (dt.tzid && dt.value) {
		icaltimezone *zone = NULL;

		e_cal_client_get_timezone_sync (
			gid->client, dt.tzid, &zone, gid->cancellable, NULL);

		if (g_cancellable_is_cancelled (gid->cancellable))
			return FALSE;

		if (zone)
			instance_start = icaltime_as_timet_with_zone (*dt.value, zone);
	}

	e_cal_component_free_datetime (&dt);

	priv = gid->cal_shell_view->priv;
	value = g_new (time_t, 1);
	*value = instance_start;
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
	GSList *icalcomps = NULL;
	GError *error = NULL;

	g_return_if_fail (client != NULL);
	g_return_if_fail (result != NULL);
	g_return_if_fail (cal_shell_view != NULL);

	e_cal_client_get_object_list_finish (
		client, result, &icalcomps, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (icalcomps == NULL);
		g_error_free (error);

	} else if (error != NULL || !icalcomps) {
		g_warn_if_fail (icalcomps == NULL);
		g_clear_error (&error);

		cal_shell_view->priv->search_pending_count--;
		if (!cal_shell_view->priv->search_pending_count) {
			cal_iterate_searching (cal_shell_view);
		}

	} else {
		GSList *iter;
		GCancellable *cancellable;
		time_t start, end;

		cancellable = e_activity_get_cancellable (
			cal_shell_view->priv->searching_activity);
		start = time_add_day (
			cal_shell_view->priv->search_time,
			(-1) * cal_shell_view->priv->search_direction);
		end = cal_shell_view->priv->search_time;
		if (start > end) {
			time_t tmp = start;
			start = end;
			end = tmp;
		}

		for (iter = icalcomps; iter; iter = iter->next) {
			icalcomponent *icalcomp = iter->data;
			struct GenerateInstancesData *gid;

			gid = g_new0 (struct GenerateInstancesData, 1);
			gid->client = client;
			gid->cal_shell_view = cal_shell_view;
			gid->cancellable = g_object_ref (cancellable);

			e_cal_client_generate_instances_for_object (
				client, icalcomp, start, end, cancellable,
				cal_searching_got_instance_cb, gid,
				cal_searching_instances_done_cb);
		}

		e_cal_client_free_icalcomp_slist (icalcomps);
	}
}

static gboolean
cal_searching_check_candidates (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;
	GSList *iter;
	time_t value, candidate = -1;

	g_return_val_if_fail (cal_shell_view != NULL, FALSE);
	g_return_val_if_fail (cal_shell_view->priv != NULL, FALSE);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

	if (!e_calendar_view_get_selected_time_range (calendar_view, &value, NULL))
		return FALSE;

	if (cal_shell_view->priv->search_direction > 0 &&
	    (view_type == GNOME_CAL_WEEK_VIEW ||
	     view_type == GNOME_CAL_MONTH_VIEW))
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
		gnome_calendar_goto (calendar, candidate);
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
	ECalModel *model;
	time_t new_time, range1, range2;
	icaltimezone *timezone;
	const gchar *default_tzloc = NULL;
	GCancellable *cancellable;
	gchar *sexp, *start, *end;

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

	model = gnome_calendar_get_model (
		e_cal_shell_content_get_calendar (
		cal_shell_view->priv->cal_shell_content));
	list = e_cal_model_list_clients (model);

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

	timezone = e_cal_model_get_timezone (model);
	range1 = priv->search_time;
	range2 = time_add_day (range1, priv->search_direction);
	if (range1 < range2) {
		start = isodate_from_time_t (time_day_begin (range1));
		end = isodate_from_time_t (time_day_end (range2));
	} else {
		start = isodate_from_time_t (time_day_begin (range2));
		end = isodate_from_time_t (time_day_end (range1));
	}

	if (timezone && timezone != icaltimezone_get_utc_timezone ())
		default_tzloc = icaltimezone_get_location (timezone);
	if (!default_tzloc)
		default_tzloc = "";

	sexp = g_strdup_printf (
		"(and %s (occur-in-time-range? "
		"(make-time \"%s\") "
		"(make-time \"%s\") \"%s\"))",
		e_cal_model_get_search_query (model), start, end, default_tzloc);

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

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
	g_free (sexp);

	e_shell_view_update_actions (E_SHELL_VIEW (cal_shell_view));
}

void
e_cal_shell_view_search_events (ECalShellView *cal_shell_view,
                                gboolean search_forward)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	ECalShellContent *cal_shell_content;
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	GnomeCalendar *calendar;
	time_t start_time = 0;
	gint range_years;

	if (priv->searching_activity || !priv->search_direction)
		e_cal_shell_view_search_stop (cal_shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	view_type = gnome_calendar_get_view (calendar);
	calendar_view = gnome_calendar_get_calendar_view (calendar, view_type);

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

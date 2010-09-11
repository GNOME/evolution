/*
 * e-cal-shell-view-private.c
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

#include "e-util/e-util-private.h"

#include "e-cal-shell-view-private.h"

#include "calendar/gui/calendar-view-factory.h"
#include "widgets/menus/gal-view-factory-etable.h"

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

static void
cal_shell_view_process_completed_tasks (ECalShellView *cal_shell_view,
                                        gboolean config_changed)
{
#if 0
	ECalShellContent *cal_shell_content;
	ECalendarTable *task_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	e_calendar_table_process_completed_tasks (
		task_table, clients, config_changed);
#endif
}

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
	gint new_days_shown;
	gint week_start_day;

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

		if (week_start_day == 0 && (!multi_week_view || compress_weekend))
			g_date_add_days (&start_date, 1);
	}

	g_date_subtract_days (&end_date, 1);

	e_calendar_item_get_selection (
		calitem, &new_start_date, &new_end_date);

	/* If the selection hasn't changed, just return. */
	if (g_date_compare (&start_date, &new_start_date) == 0 &&
		g_date_compare (&end_date, &new_end_date) == 0)
		return;

	new_days_shown =
		g_date_get_julian (&new_end_date) -
		g_date_get_julian (&new_start_date) + 1;

	/* If a complete week is selected we show the week view.
	 * Note that if weekends are compressed and the week start
	 * day is set to Sunday, we don't actually show complete
	 * weeks in the week view, so this may need tweaking. */
	starts_on_week_start_day =
		(g_date_get_weekday (&new_start_date) % 7 == week_start_day);

	/* Update selection to be in the new time range. */
	tt = icaltime_null_time ();
	tt.year = g_date_get_year (&new_start_date);
	tt.month = g_date_get_month (&new_start_date);
	tt.day = g_date_get_day (&new_start_date);
	new_time = icaltime_as_timet_with_zone (tt, timezone);

	/* Switch views as appropriate, and change the number of
	 * days or weeks shown. */
	if (new_days_shown > 9) {
		if (view_type != GNOME_CAL_LIST_VIEW) {
			ECalendarView *calendar_view;

			calendar_view = gnome_calendar_get_calendar_view (
				calendar, GNOME_CAL_MONTH_VIEW);
			e_week_view_set_weeks_shown (
				E_WEEK_VIEW (calendar_view),
				(new_days_shown + 6) / 7);
			switch_to = GNOME_CAL_MONTH_VIEW;
		}
	} else if (new_days_shown == 7 && starts_on_week_start_day)
		switch_to = GNOME_CAL_WEEK_VIEW;
	else {
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
}

static void
cal_shell_view_date_navigator_scroll_event_cb (ECalShellView *cal_shell_view,
                                               GdkEventScroll *event,
                                               ECalendar *date_navigator)
{
	ECalendarItem *calitem;
	GDate start_date, end_date;

	calitem = date_navigator->calitem;
	if (!e_calendar_item_get_selection (calitem, &start_date, &end_date))
		return;

	switch (event->direction) {
		case GDK_SCROLL_UP:
			g_date_subtract_months (&start_date, 1);
			g_date_subtract_months (&end_date, 1);
			break;

		case GDK_SCROLL_DOWN:
			g_date_add_months (&start_date, 1);
			g_date_add_months (&end_date, 1);
			break;

		default:
			g_return_if_reached ();
	}

	/* XXX Does ECalendarItem emit a signal for this?  If so, maybe
	 *     we could move this handler into ECalShellSidebar. */
	e_calendar_item_set_selection (calitem, &start_date, &end_date);

	cal_shell_view_date_navigator_selection_changed_cb (
		cal_shell_view, calitem);
}

static void
cal_shell_view_popup_event_cb (EShellView *shell_view,
                               GdkEventButton *event)
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

	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static gboolean
cal_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                        ESource *primary_source,
                                        GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static void
cal_shell_view_selector_client_added_cb (ECalShellView *cal_shell_view,
                                         ECal *client)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	model = gnome_calendar_get_model (calendar);

	e_cal_model_add_client (model, client);

	gnome_calendar_update_query (calendar);
}

static void
cal_shell_view_selector_client_removed_cb (ECalShellView *cal_shell_view,
                                           ECal *client)
{
	ECalShellContent *cal_shell_content;
	GnomeCalendar *calendar;
	ECalModel *model;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	model = gnome_calendar_get_model (calendar);

	e_cal_model_remove_client (model, client);

	gnome_calendar_update_query (calendar);
}

static void
cal_shell_view_memopad_popup_event_cb (EShellView *shell_view,
                                       GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-memopad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
cal_shell_view_taskpad_popup_event_cb (EShellView *shell_view,
                                       GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/calendar-taskpad-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
cal_shell_view_user_created_cb (ECalShellView *cal_shell_view,
                                ECalendarView *calendar_view)
{
	ECalShellSidebar *cal_shell_sidebar;
	ECalModel *model;
	ECal *client;
	ESource *source;

	model = e_calendar_view_get_model (calendar_view);
	client = e_cal_model_get_default_client (model);
	source = e_cal_get_source (client);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	e_cal_shell_sidebar_add_source (cal_shell_sidebar, source);
}

static void
cal_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for calendars");
	g_free (filename);

	factory = calendar_view_factory_new (GNOME_CAL_DAY_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_WORK_WEEK_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_WEEK_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = calendar_view_factory_new (GNOME_CAL_MONTH_VIEW);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
cal_shell_view_notify_view_id_cb (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	view_instance =
		e_cal_shell_content_get_view_instance (cal_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (cal_shell_view));

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
e_cal_shell_view_private_init (ECalShellView *cal_shell_view,
                               EShellViewClass *shell_view_class)
{
	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		cal_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
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
        ECalShellView  *view = E_CAL_SHELL_VIEW (user_data);
	ECalShellViewPrivate *priv = view->priv;
	ECalShellContent *cal_shell_content;
	icaltimezone *timezone = NULL, *current_zone = NULL;
	EShellSettings *settings;
	EShellBackend *shell_backend;
	EShell *shell;
	ECalModel *model;
	const gchar *location;

	if (event != G_FILE_MONITOR_EVENT_CHANGED &&
	    event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
	    event != G_FILE_MONITOR_EVENT_DELETED &&
	    event != G_FILE_MONITOR_EVENT_CREATED)
		return;

	cal_shell_content = priv->cal_shell_content;
	model = e_cal_shell_content_get_model (cal_shell_content);
	current_zone = e_cal_model_get_timezone (model);
	timezone = e_cal_util_get_system_timezone ();

	if (!g_strcmp0 (
		icaltimezone_get_tzid (timezone),
		icaltimezone_get_tzid (current_zone)))
		return;

	shell_backend = e_shell_view_get_shell_backend ((EShellView *) view);
	shell = e_shell_backend_get_shell (shell_backend);
	settings = e_shell_get_shell_settings (shell);
	location = icaltimezone_get_location (timezone);
	if (location == NULL)
		location = "UTC";

	g_object_set (settings, "cal-timezone-string", location, NULL);
	g_object_set (settings, "cal-timezone", timezone, NULL);
}

static void
init_timezone_monitors (ECalShellView *view)
{
	ECalShellViewPrivate *priv = view->priv;
	gint i;

	for (i = 0; i < CHECK_NB; i++) {
		GFile *file;

		file = g_file_new_for_path (files_to_check[i]);
		priv->monitors[i] = g_file_monitor_file (file,
				G_FILE_MONITOR_NONE,
				NULL, NULL);
		g_object_unref (file);

		if (priv->monitors[i])
			g_signal_connect_object (G_OBJECT (priv->monitors[i]),
						 "changed",
						 G_CALLBACK (system_timezone_monitor_changed),
						 view, 0);
	}
}

void
e_cal_shell_view_private_constructed (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GnomeCalendar *calendar;
	ECalendar *date_navigator;
	EMemoTable *memo_table;
	ETaskTable *task_table;
	ESourceSelector *selector;
	ECalModel *model;
	gint ii;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	e_shell_window_add_action_group (shell_window, "calendar");
	e_shell_window_add_action_group (shell_window, "calendar-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->cal_shell_backend = g_object_ref (shell_backend);
	priv->cal_shell_content = g_object_ref (shell_content);
	priv->cal_shell_sidebar = g_object_ref (shell_sidebar);

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);
	model = e_cal_shell_content_get_model (cal_shell_content);
	calendar = e_cal_shell_content_get_calendar (cal_shell_content);
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	cal_shell_sidebar = E_CAL_SHELL_SIDEBAR (shell_sidebar);
	selector = e_cal_shell_sidebar_get_selector (cal_shell_sidebar);
	date_navigator = e_cal_shell_sidebar_get_date_navigator (cal_shell_sidebar);

	/* Give GnomeCalendar a handle to the date navigator, memo and task table. */
	gnome_calendar_set_date_navigator (calendar, date_navigator);
	gnome_calendar_set_memo_table (calendar, memo_table ? GTK_WIDGET (memo_table) : NULL);
	gnome_calendar_set_task_table (calendar, task_table ? GTK_WIDGET (task_table) : NULL);

	e_calendar_item_set_get_time_callback (
		date_navigator->calitem, (ECalendarItemGetTimeCallback)
		cal_shell_view_get_current_time, cal_shell_view, NULL);

	for (ii = 0; ii < GNOME_CAL_LAST_VIEW; ii++) {
		ECalendarView *calendar_view;

		calendar_view =
			gnome_calendar_get_calendar_view (calendar, ii);

		g_signal_connect_object (
			calendar_view, "popup-event",
			G_CALLBACK (cal_shell_view_popup_event_cb),
			cal_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			calendar_view, "selection-changed",
			G_CALLBACK (e_shell_view_update_actions),
			cal_shell_view, G_CONNECT_SWAPPED);

		g_signal_connect_object (
			calendar_view, "user-created",
			G_CALLBACK (cal_shell_view_user_created_cb),
			cal_shell_view, G_CONNECT_SWAPPED);
	}

	g_signal_connect_object (
		calendar, "dates-shown-changed",
		G_CALLBACK (e_cal_shell_view_update_sidebar),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		model, "status-message",
		G_CALLBACK (e_cal_shell_view_set_status_message),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		model, "notify::timezone",
		G_CALLBACK (e_cal_shell_view_update_timezone),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		date_navigator, "scroll-event",
		G_CALLBACK (cal_shell_view_date_navigator_scroll_event_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		date_navigator->calitem, "date-range-changed",
		G_CALLBACK (cal_shell_view_date_navigator_date_range_changed_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		date_navigator->calitem, "selection-changed",
		G_CALLBACK (cal_shell_view_date_navigator_selection_changed_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		selector, "popup-event",
		G_CALLBACK (cal_shell_view_selector_popup_event_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		cal_shell_sidebar, "client-added",
		G_CALLBACK (cal_shell_view_selector_client_added_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		cal_shell_sidebar, "client-removed",
		G_CALLBACK (cal_shell_view_selector_client_removed_cb),
		cal_shell_view, G_CONNECT_SWAPPED);

	if (memo_table)
		g_signal_connect_object (
			memo_table, "popup-event",
			G_CALLBACK (cal_shell_view_memopad_popup_event_cb),
			cal_shell_view, G_CONNECT_SWAPPED);

	if (memo_table)
		g_signal_connect_object (
			memo_table, "selection-change",
			G_CALLBACK (e_cal_shell_view_memopad_actions_update),
			cal_shell_view, G_CONNECT_SWAPPED);

	if (memo_table)
		g_signal_connect_object (
			memo_table, "status-message",
			G_CALLBACK (e_cal_shell_view_memopad_set_status_message),
			cal_shell_view, G_CONNECT_SWAPPED);

	if (task_table)
		g_signal_connect_object (
			task_table, "popup-event",
			G_CALLBACK (cal_shell_view_taskpad_popup_event_cb),
			cal_shell_view, G_CONNECT_SWAPPED);

	if (task_table)
		g_signal_connect_object (
			task_table, "status-message",
			G_CALLBACK (e_cal_shell_view_taskpad_set_status_message),
			cal_shell_view, G_CONNECT_SWAPPED);

	if (task_table)
		g_signal_connect_object (
			task_table, "selection-change",
			G_CALLBACK (e_cal_shell_view_taskpad_actions_update),
			cal_shell_view, G_CONNECT_SWAPPED);

	e_categories_add_change_hook (
		(GHookFunc) e_cal_shell_view_update_search_filter,
		cal_shell_view);

	init_timezone_monitors (cal_shell_view);
	e_cal_shell_view_actions_init (cal_shell_view);
	e_cal_shell_view_update_sidebar (cal_shell_view);
        e_cal_shell_view_update_search_filter (cal_shell_view);
	e_cal_shell_view_update_timezone (cal_shell_view);

	/* Keep the ECalModel in sync with the sidebar. */
	e_binding_new (
		shell_sidebar, "default-client",
		model, "default-client");

	/* Keep the toolbar view buttons in sync with the calendar. */
	e_mutual_binding_new (
		calendar, "view",
		ACTION (CALENDAR_VIEW_DAY), "current-value");

	/* Force the main calendar to update its default source. */
	g_signal_emit_by_name (selector, "primary-selection-changed");
}

void
e_cal_shell_view_private_dispose (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	gint i;

	/* Calling calendar's save state from here, because it is too late in its dispose */
	if (priv->cal_shell_content)
		e_cal_shell_content_save_state (priv->cal_shell_content);

	/* Calling calendar's save state from here, because it is too late in its dispose */
	if (priv->cal_shell_content)
		e_cal_shell_content_save_state (priv->cal_shell_content);

	DISPOSE (priv->cal_shell_backend);
	DISPOSE (priv->cal_shell_content);
	DISPOSE (priv->cal_shell_sidebar);

	if (priv->calendar_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->calendar_activity);
		g_object_unref (priv->calendar_activity);
		priv->calendar_activity = NULL;
	}

	if (priv->memopad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->memopad_activity);
		g_object_unref (priv->memopad_activity);
		priv->memopad_activity = NULL;
	}

	if (priv->taskpad_activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->taskpad_activity);
		g_object_unref (priv->taskpad_activity);
		priv->taskpad_activity = NULL;
	}

	for (i = 0; i < CHECK_NB; i++) {
		g_object_unref (priv->monitors[i]);
		priv->monitors[i] = NULL;
	}
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

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (itip_sentby_is_user (comp, comp_data->client))
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
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}
	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);
	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	cal_shell_view->priv->calendar_activity = activity;
}

void
e_cal_shell_view_transfer_item_to (ECalShellView *cal_shell_view,
                                   ECalendarViewEvent *event,
                                   ECal *destination_client,
                                   gboolean remove)
{
	icalcomponent *icalcomp;
	icalcomponent *icalcomp_clone;
	icalcomponent *icalcomp_event;
	gboolean success;
	const gchar *uid;

	/* XXX This function should be split up into
	 *     smaller, more understandable pieces. */

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (event != NULL);
	g_return_if_fail (E_IS_CAL (destination_client));

	if (!is_comp_data_valid (event))
		return;

	icalcomp_event = event->comp_data->icalcomp;
	uid = icalcomponent_get_uid (icalcomp_event);

	/* Put the new object into the destination calendar. */

	success = e_cal_get_object (
		destination_client, uid, NULL, &icalcomp, NULL);

	if (success) {
		icalcomponent_free (icalcomp);
		success = e_cal_modify_object (
			destination_client, icalcomp_event,
			CALOBJ_MOD_ALL, NULL);

		/* do not delete the event when it was found in the calendar */
		return;
	} else {
		icalproperty *icalprop;
		gchar *new_uid;

		if (e_cal_util_component_is_instance (icalcomp_event)) {
			success = e_cal_get_object (
				event->comp_data->client,
				uid, NULL, &icalcomp, NULL);
			if (success) {
				/* Use master object when working
				 * with a recurring event ... */
				icalcomp_clone =
					icalcomponent_new_clone (icalcomp);
				icalcomponent_free (icalcomp);
			} else {
				/* ... or remove the recurrence ID ... */
				icalcomp_clone =
					icalcomponent_new_clone (icalcomp_event);
				if (e_cal_util_component_has_recurrences (icalcomp_clone)) {
					/* ... for non-detached instances,
					 * to make it a master object. */
					icalprop = icalcomponent_get_first_property (
						icalcomp_clone, ICAL_RECURRENCEID_PROPERTY);
					if (icalprop != NULL)
						icalcomponent_remove_property (
							icalcomp_clone, icalprop);
				}
			}
		} else
			icalcomp_clone =
				icalcomponent_new_clone (icalcomp_event);

		icalprop = icalproperty_new_x ("1");
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
		icalcomponent_add_property (icalcomp_clone, icalprop);

		if (!remove) {
			/* Change the UID to avoid problems with
			 * duplicated UIDs. */
			new_uid = e_cal_component_gen_uid ();
			icalcomponent_set_uid (icalcomp_clone, new_uid);
			g_free (new_uid);
		}

		new_uid = NULL;
		success = e_cal_create_object (
			destination_client, icalcomp_clone, &new_uid, NULL);
		if (!success) {
			icalcomponent_free (icalcomp_clone);
			return;
		}

		icalcomponent_free (icalcomp_clone);
		g_free (new_uid);
	}

	if (remove) {
		ECal *source_client = event->comp_data->client;

		/* Remove the item from the source calendar. */
		if (e_cal_util_component_is_instance (icalcomp_event) ||
			e_cal_util_component_has_recurrences (icalcomp_event)) {
			icaltimetype icaltime;
			gchar *rid;

			icaltime =
				icalcomponent_get_recurrenceid (icalcomp_event);
			if (!icaltime_is_null_time (icaltime))
				rid = icaltime_as_ical_string_r (icaltime);
			else
				rid = NULL;
			e_cal_remove_object_with_mod (
				source_client, uid, rid, CALOBJ_MOD_ALL, NULL);
			g_free (rid);
		} else
			e_cal_remove_object (source_client, uid, NULL);
	}
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

void
e_cal_shell_view_update_timezone (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalShellSidebar *cal_shell_sidebar;
	icaltimezone *timezone;
	ECalModel *model;
	GList *clients, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	model = e_cal_shell_content_get_model (cal_shell_content);
	timezone = e_cal_model_get_timezone (model);

	cal_shell_sidebar = cal_shell_view->priv->cal_shell_sidebar;
	clients = e_cal_shell_sidebar_get_clients (cal_shell_sidebar);

	for (iter = clients; iter != NULL; iter = iter->next) {
		ECal *client = iter->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			e_cal_set_default_timezone (client, timezone, NULL);
	}

	g_list_free (clients);
}

/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include "evolution-config.h"

#include <string.h>
#include <math.h>
#include <glib/gi18n-lib.h>

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/calendar-view.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-cal-list-view.h"
#include "calendar/gui/e-cal-model-calendar.h"
#include "calendar/gui/e-cal-model-memos.h"
#include "calendar/gui/e-cal-model-tasks.h"
#include "calendar/gui/e-calendar-view.h"
#include "calendar/gui/e-day-view.h"
#include "calendar/gui/e-month-view.h"
#include "calendar/gui/e-week-view.h"
#include "calendar/gui/e-year-view.h"
#include "calendar/gui/itip-utils.h"
#include "calendar/gui/tag-calendar.h"

#include "e-cal-base-shell-sidebar.h"
#include "e-cal-shell-view-private.h"
#include "e-cal-shell-content.h"

struct _ECalShellContentPrivate {
	GtkWidget *hpaned;
	GtkWidget *vpaned;

	GtkWidget *calendar_notebook;
	GtkWidget *task_table;
	ECalModel *task_model;
	ECalDataModel *task_data_model;
	gulong task_table_update_id;

	GtkWidget *memo_table;
	ECalModel *memo_model;
	ECalDataModel *memo_data_model;

	ECalModel *list_view_model;
	ECalDataModel *list_view_data_model;

	ETagCalendar *tag_calendar;
	gulong datepicker_selection_changed_id;
	gulong datepicker_range_moved_id;

	ECalViewKind current_view;
	ECalendarView *views[E_CAL_VIEW_KIND_LAST];
	GDate view_start, view_end;
	guint32 view_start_range_day_offset;
	GDate last_range_start; /* because "date-range-changed" can be emit with no real change */

	time_t previous_selected_start_time;
	time_t previous_selected_end_time;

	gulong current_view_id_changed_id;

	gboolean initialized;
};

enum {
	PROP_0,
	PROP_CALENDAR_NOTEBOOK,
	PROP_MEMO_TABLE,
	PROP_TASK_TABLE,
	PROP_CURRENT_VIEW_ID,
	PROP_CURRENT_VIEW,
	PROP_SHOW_TAG_VPANE
};

/* Used to indicate who has the focus within the calendar view. */
typedef enum {
	FOCUS_CALENDAR_NOTEBOOK,
	FOCUS_MEMO_TABLE,
	FOCUS_TASK_TABLE,
	FOCUS_OTHER
} FocusLocation;

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ECalShellContent, e_cal_shell_content, E_TYPE_CAL_BASE_SHELL_CONTENT, 0,
	G_ADD_PRIVATE_DYNAMIC (ECalShellContent))

static time_t
convert_to_local_zone (time_t tm,
		       ICalTimezone *from_zone)
{
	ICalTime *itt;
	time_t tt;

	itt = i_cal_time_new_from_timet_with_zone (tm, FALSE, from_zone);
	tt = i_cal_time_as_timet (itt);
	g_clear_object (&itt);

	return tt;
}

static void
cal_shell_content_update_model_and_current_view_times (ECalShellContent *cal_shell_content,
						       ECalModel *model,
						       ECalendarItem *calitem,
						       time_t view_start_tt,
						       time_t view_end_tt,
						       const GDate *view_start,
						       const GDate *view_end)
{
	ECalendarView *current_view;
	EDayView *day_view = NULL;
	gint day_view_selection_start_day = -1, day_view_selection_end_day = -1;
	gint day_view_selection_start_row = -1, day_view_selection_end_row = -1;
	gdouble day_view_scrollbar_position = 0.0;
	gint syy, smm, sdd, eyy, emm, edd;
	time_t visible_range_start, visible_range_end;
	gboolean filters_updated = FALSE;
	ICalTimezone *zone;
	gchar *cal_filter;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	current_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
	g_return_if_fail (current_view != NULL);

	zone = e_cal_model_get_timezone (model);
	cal_filter = e_cal_data_model_dup_filter (e_cal_model_get_data_model (model));

	if (E_IS_DAY_VIEW (current_view)) {
		GtkAdjustment *adjustment;

		day_view = E_DAY_VIEW (current_view);
		day_view_selection_start_day = day_view->selection_start_day;
		day_view_selection_end_day = day_view->selection_end_day;
		day_view_selection_start_row = day_view->selection_start_row;
		day_view_selection_end_row = day_view->selection_end_row;

		adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->main_canvas));
		day_view_scrollbar_position = gtk_adjustment_get_value (adjustment);
	}

	g_signal_handler_block (calitem, cal_shell_content->priv->datepicker_range_moved_id);
	g_signal_handler_block (calitem, cal_shell_content->priv->datepicker_selection_changed_id);

	visible_range_start = view_start_tt;
	visible_range_end = view_end_tt;

	e_calendar_view_precalc_visible_time_range (current_view, view_start_tt, view_end_tt, &visible_range_start, &visible_range_end);
	if (view_start_tt != visible_range_start || view_end_tt != visible_range_end) {
		time_t cmp_range_start = convert_to_local_zone (visible_range_start, zone);
		time_t cmp_range_end = convert_to_local_zone (visible_range_end, zone);

		if (view_start_tt != cmp_range_start ||
		    view_end_tt != cmp_range_end - 1) {
			/* Calendar views update their inner time range during e_cal_model_set_time_range() call,
			   while they can change it if needed (like a clamp of a week view with a week start day
			   not being Monday */
			GDate new_view_start, new_view_end;

			/* Midnight means the next day, which is not desired here */
			cmp_range_end--;
			visible_range_end--;

			/* These times are in the correct zone already */
			time_to_gdate_with_zone (&new_view_start, cmp_range_start, NULL);
			time_to_gdate_with_zone (&new_view_end, cmp_range_end, NULL);

			e_calendar_item_set_selection (calitem, &new_view_start, &new_view_end);
			e_cal_shell_content_update_filters (cal_shell_content, cal_filter, visible_range_start, visible_range_end);
			e_calendar_view_set_selected_time_range (current_view, cmp_range_start, cmp_range_start);
			filters_updated = TRUE;
			view_start_tt = cmp_range_start;
			view_end_tt = cmp_range_end;
		}
	}

	if (!filters_updated) {
		e_calendar_item_set_selection (calitem, view_start, view_end);
		e_cal_shell_content_update_filters (cal_shell_content, cal_filter, view_start_tt, view_end_tt);
		e_calendar_view_set_selected_time_range (current_view, view_start_tt, view_start_tt);
	}

	if (day_view && day_view_selection_start_day != -1 && day_view_selection_end_day != -1 &&
	    day_view_selection_start_row != -1 && day_view_selection_end_row != -1) {
		GtkAdjustment *adjustment;

		day_view->selection_start_day = day_view_selection_start_day;
		day_view->selection_end_day = day_view_selection_end_day;
		day_view->selection_start_row = day_view_selection_start_row;
		day_view->selection_end_row = day_view_selection_end_row;

		/* This is better than e_day_view_ensure_rows_visible(), because it keeps both
		   selection and the exact scrollbar position in the main canvas, which may not
		   always correspond to each other. */
		adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (day_view->main_canvas));
		gtk_adjustment_set_value (adjustment, day_view_scrollbar_position);
	}

	gtk_widget_queue_draw (GTK_WIDGET (current_view));

	g_free (cal_filter);

	g_signal_handler_unblock (calitem, cal_shell_content->priv->datepicker_range_moved_id);
	g_signal_handler_unblock (calitem, cal_shell_content->priv->datepicker_selection_changed_id);

	if (e_calendar_item_get_date_range (calitem, &syy, &smm, &sdd, &eyy, &emm, &edd)) {
		GDate range_start;

		g_date_set_dmy (&range_start, sdd, smm + 1, syy);

		cal_shell_content->priv->view_start_range_day_offset =
			g_date_get_julian (&cal_shell_content->priv->view_start) - g_date_get_julian (&range_start);
	}
}

static void
e_cal_shell_content_change_view (ECalShellContent *cal_shell_content,
				 ECalViewKind to_view,
				 const GDate *view_start,
				 const GDate *view_end,
				 gboolean force_change)
{
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	ECalendar *calendar;
	ECalModel *model;
	ICalTimezone *zone;
	time_t view_start_tt, view_end_tt;
	gboolean view_changed = FALSE;
	gint selected_days;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (to_view >= E_CAL_VIEW_KIND_DAY && to_view < E_CAL_VIEW_KIND_LAST);
	g_return_if_fail (view_start != NULL);
	g_return_if_fail (g_date_valid (view_start));
	g_return_if_fail (view_end != NULL);
	g_return_if_fail (g_date_valid (view_end));

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	calendar = e_cal_base_shell_sidebar_get_date_navigator (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_return_if_fail (E_IS_CALENDAR (calendar));

	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	zone = e_cal_model_get_timezone (model);
	view_start_tt = cal_comp_gdate_to_timet (view_start, zone);
	view_end_tt = cal_comp_gdate_to_timet (view_end, zone);

	if (to_view != cal_shell_content->priv->current_view) {
		g_signal_handler_block (cal_shell_content, cal_shell_content->priv->current_view_id_changed_id);
		e_cal_shell_content_set_current_view_id (cal_shell_content, to_view);
		g_signal_handler_unblock (cal_shell_content, cal_shell_content->priv->current_view_id_changed_id);
		view_changed = TRUE;
	}

	selected_days = g_date_get_julian (view_end) - g_date_get_julian (view_start) + 1;

	if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_DAY) {
		EDayView *day_view;

		day_view = E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_DAY]);
		e_day_view_set_days_shown (day_view, selected_days);
	} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH) {
		EWeekView *month_view;

		month_view = E_WEEK_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_MONTH]);
		e_week_view_set_first_day_shown (month_view, view_start);
		e_week_view_set_weeks_shown (month_view, selected_days / 7);
	}

	if (!force_change &&
	    g_date_valid (&cal_shell_content->priv->view_start) &&
	    g_date_valid (&cal_shell_content->priv->view_end) &&
	    g_date_compare (&cal_shell_content->priv->view_start, view_start) == 0 &&
	    g_date_compare (&cal_shell_content->priv->view_end, view_end) == 0) {
		ECalendarItem *calitem = e_calendar_get_item (calendar);

		if (view_changed) {
			cal_shell_content_update_model_and_current_view_times (
				cal_shell_content, model, calitem, view_start_tt, view_end_tt, view_start, view_end);
		}

		g_signal_handler_block (calitem, cal_shell_content->priv->datepicker_range_moved_id);
		g_signal_handler_block (calitem, cal_shell_content->priv->datepicker_selection_changed_id);

		e_calendar_item_set_selection (calitem, view_start, view_end);

		g_signal_handler_unblock (calitem, cal_shell_content->priv->datepicker_range_moved_id);
		g_signal_handler_unblock (calitem, cal_shell_content->priv->datepicker_selection_changed_id);
	} else {
		cal_shell_content->priv->view_start = *view_start;
		cal_shell_content->priv->view_end = *view_end;

		cal_shell_content_update_model_and_current_view_times (
			cal_shell_content, model, e_calendar_get_item (calendar), view_start_tt, view_end_tt, view_start, view_end);
	}
}

static void
cal_shell_content_clamp_for_whole_weeks (GDateWeekday week_start_day,
					 GDate *sel_start,
					 GDate *sel_end,
					 gboolean saturday_as_sunday)
{
	GDateWeekday wday;
	guint32 julian_start, julian_end;

	g_return_if_fail (sel_start != NULL);
	g_return_if_fail (sel_end != NULL);

	wday = g_date_get_weekday (sel_start);

	/* This is because the month/week view doesn't split weekends */
	if (saturday_as_sunday && wday == G_DATE_SATURDAY && week_start_day == G_DATE_SUNDAY)
		wday = G_DATE_SUNDAY;

	if (week_start_day > wday) {
		g_date_subtract_days (sel_start, wday);
		wday = g_date_get_weekday (sel_start);
	}

	if (week_start_day < wday)
		g_date_subtract_days (sel_start, wday - week_start_day);

	julian_start = g_date_get_julian (sel_start);
	julian_end = g_date_get_julian (sel_end);

	if (((julian_end - julian_start + 1) % 7) != 0)
		g_date_add_days (sel_end, 7 - ((julian_end - julian_start + 1) % 7));

	julian_end = g_date_get_julian (sel_end);

	/* Can show only up to 6 weeks */
	if ((julian_end - julian_start + 1) / 7 > 6) {
		*sel_end = *sel_start;
		g_date_add_days (sel_end, (7 * 6) - 1);
	}

	if (g_date_compare (sel_start, sel_end) == 0)
		g_date_add_days (sel_end, 6);
}

static gboolean
cal_shell_content_weekday_within (GDateWeekday start_wday,
				  GDateWeekday end_wday,
				  GDateWeekday test_wday)
{
	gint ii;

	if (start_wday <= end_wday)
		return start_wday <= test_wday && test_wday <= end_wday;

	for (ii = 0; ii < 7; ii++) {
		if (start_wday == test_wday)
			return TRUE;

		if (start_wday == end_wday)
			break;

		start_wday = e_weekday_get_next (start_wday);
	}

	return FALSE;
}

static void
cal_shell_content_change_selection_in_current_view (ECalShellContent *cal_shell_content,
						    time_t sel_start_tt,
						    time_t sel_end_tt,
						    ICalTimezone *zone)
{
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (cal_shell_content->priv->current_view >= E_CAL_VIEW_KIND_DAY &&
	    cal_shell_content->priv->current_view < E_CAL_VIEW_KIND_LAST) {
		ECalendarView *view;

		view = cal_shell_content->priv->views[cal_shell_content->priv->current_view];

		/* Preserve selected time (change only date) for these views */
		if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_DAY ||
		    cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WORKWEEK) {
			time_t current_sel_start = (time_t) -1, current_sel_end = (time_t) -1;

			if (e_calendar_view_get_selected_time_range (view, &current_sel_start, &current_sel_end)) {
				ICalTime *itt;

				itt = i_cal_time_new_from_timet_with_zone (current_sel_start, 0, zone);
				current_sel_start = i_cal_time_as_timet_with_zone (itt, NULL);
				g_clear_object (&itt);

				itt = i_cal_time_new_from_timet_with_zone (current_sel_end, 0, zone);
				current_sel_end = i_cal_time_as_timet_with_zone (itt, NULL);
				g_clear_object (&itt);

				sel_start_tt += current_sel_start % (24 * 60 * 60);
				sel_end_tt += current_sel_end % (24 * 60 * 60);
			}
		}

		e_calendar_view_set_selected_time_range (view, sel_start_tt, sel_end_tt);
	}
}

static void
cal_shell_content_datepicker_selection_changed_cb (ECalendarItem *calitem,
						   ECalShellContent *cal_shell_content)
{
	GDate sel_start, sel_end;
	guint32 selected_days, start_julian, end_julian;
	ICalTimezone *zone;
	time_t sel_start_tt, sel_end_tt;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_YEAR)
		return;

	g_date_clear (&sel_start, 1);
	g_date_clear (&sel_end, 1);

	if (!e_calendar_item_get_selection (calitem, &sel_start, &sel_end))
		return;

	start_julian = g_date_get_julian (&sel_start);
	end_julian = g_date_get_julian (&sel_end);

	g_return_if_fail (start_julian <= end_julian);

	if (g_date_compare (&cal_shell_content->priv->view_start, &sel_start) == 0 &&
	    g_date_compare (&cal_shell_content->priv->view_end, &sel_end) == 0) {
		/* No change in the selection range */
		return;
	}

	zone = e_cal_model_get_timezone (e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content)));
	sel_start_tt = cal_comp_gdate_to_timet (&sel_start, zone);
	sel_end_tt = cal_comp_gdate_to_timet (&sel_end, zone);

	selected_days = end_julian - start_julian + 1;
	if (selected_days == 1) {
		GDateWeekday sel_start_wday, sel_end_wday, cur_start_wday, cur_end_wday;

		/* Clicked inside currently selected view range; do not do anything,
		   just make sure the days are selected again */
		if (g_date_compare (&cal_shell_content->priv->view_start, &sel_start) <= 0 &&
		    g_date_compare (&sel_start, &cal_shell_content->priv->view_end) <= 0) {
			sel_start = cal_shell_content->priv->view_start;
			sel_end = cal_shell_content->priv->view_end;

			e_calendar_item_set_selection (calitem, &sel_start, &sel_end);

			cal_shell_content_change_selection_in_current_view (cal_shell_content, sel_start_tt, sel_end_tt, zone);
			return;
		}

		sel_start_wday = g_date_get_weekday (&sel_start);
		sel_end_wday = g_date_get_weekday (&sel_end);
		cur_start_wday = g_date_get_weekday (&cal_shell_content->priv->view_start);
		cur_end_wday = g_date_get_weekday (&cal_shell_content->priv->view_end);

		if ((cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WORKWEEK ||
		    (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_DAY &&
		    e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_DAY])) != 1)) &&
		    cal_shell_content_weekday_within (cur_start_wday, cur_end_wday, sel_start_wday)) {
			if (cur_start_wday < sel_start_wday) {
				g_date_subtract_days (&sel_start, sel_start_wday - cur_start_wday);
			} else if (cur_start_wday > sel_start_wday) {
				g_date_subtract_days (&sel_start, 7 - (cur_start_wday - sel_start_wday));
			}
			sel_end = sel_start;
			if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_DAY)
				g_date_add_days (&sel_end, e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_DAY])) - 1);
			else
				g_date_add_days (&sel_end, e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_WORKWEEK])) - 1);

			e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &sel_start, &sel_end, FALSE);
		} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WEEK &&
		    cal_shell_content_weekday_within (cur_start_wday, cur_end_wday, sel_start_wday) &&
		    cal_shell_content_weekday_within (cur_start_wday, cur_end_wday, sel_end_wday)) {
			if (cur_start_wday < sel_start_wday)
				g_date_subtract_days (&sel_start, sel_start_wday - cur_start_wday);
			sel_end = sel_start;
			cal_shell_content_clamp_for_whole_weeks (calitem->week_start_day, &sel_start, &sel_end, TRUE);

			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_WEEK, &sel_start, &sel_end, FALSE);
		} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH ||
			   cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_LIST) {
			sel_end = sel_start;
			if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH) {
				g_date_add_days (&sel_end, 7 * e_week_view_get_weeks_shown (E_WEEK_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_MONTH])));
			} else {
				/* whole month */
				g_date_set_day (&sel_start, 1);
				g_date_set_day (&sel_end, g_date_get_days_in_month (g_date_get_month (&sel_start), g_date_get_year (&sel_start)) - 1);
			}
			cal_shell_content_clamp_for_whole_weeks (calitem->week_start_day, &sel_start, &sel_end, cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH);

			e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &sel_start, &sel_end, FALSE);
		} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WORKWEEK) {
			cal_shell_content_clamp_for_whole_weeks (calitem->week_start_day, &sel_start, &sel_end, TRUE);
			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_WEEK, &sel_start, &sel_end, FALSE);
		} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_YEAR) {
			e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &sel_start, &sel_end, FALSE);
		} else {
			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_DAY, &sel_start, &sel_end, FALSE);
		}

		cal_shell_content_change_selection_in_current_view (cal_shell_content, sel_start_tt, sel_end_tt, zone);
	} else if (selected_days < 7) {
		GDateWeekday first_work_wday;

		first_work_wday = e_cal_model_get_work_day_first (e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content)));

		if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WORKWEEK &&
		    first_work_wday == g_date_get_weekday (&sel_start) &&
		    selected_days == e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_WORKWEEK])))
			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_WORKWEEK, &sel_start, &sel_end, FALSE);
		else
			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_DAY, &sel_start, &sel_end, FALSE);
	} else if (selected_days == 7) {
		GDateWeekday sel_start_wday;
		ECalViewKind set_kind = E_CAL_VIEW_KIND_WEEK;

		sel_start_wday = g_date_get_weekday (&sel_start);

		if (sel_start_wday == calitem->week_start_day &&
		    cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_DAY &&
		    e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_DAY])) == 7) {
			set_kind = E_CAL_VIEW_KIND_DAY;
		} else if (sel_start_wday == calitem->week_start_day &&
			   cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WORKWEEK &&
			   e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_WORKWEEK])) == 7) {
			set_kind = E_CAL_VIEW_KIND_WORKWEEK;
		}

		e_cal_shell_content_change_view (cal_shell_content, set_kind, &sel_start, &sel_end, FALSE);
	} else {
		if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_LIST) {
			/* whole month */
			g_date_set_day (&sel_start, 1);
			sel_end = sel_start;
			g_date_set_day (&sel_end, g_date_get_days_in_month (g_date_get_month (&sel_start), g_date_get_year (&sel_start)));
			cal_shell_content_clamp_for_whole_weeks (calitem->week_start_day, &sel_start, &sel_end, FALSE);

			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_LIST, &sel_start, &sel_end, FALSE);
		} else if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_YEAR) {
			e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &sel_start, &sel_end, FALSE);
		} else {
			cal_shell_content_clamp_for_whole_weeks (calitem->week_start_day, &sel_start, &sel_end,
				cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH || cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_WEEK);
			e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_MONTH, &sel_start, &sel_end, FALSE);
		}
	}
}

static void
cal_shell_content_datepicker_range_moved_cb (ECalendarItem *calitem,
					     ECalShellContent *cal_shell_content)
{
	gint start_year, start_month, start_day, end_year, end_month, end_day;
	GDate sel_start_date, sel_end_date, range_start_date;

	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (!e_calendar_item_get_date_range (calitem, &start_year, &start_month, &start_day, &end_year, &end_month, &end_day))
		return;

	g_date_set_dmy (&range_start_date, start_day, start_month + 1, start_year);

	if (g_date_valid (&cal_shell_content->priv->last_range_start) &&
	    g_date_compare (&cal_shell_content->priv->last_range_start, &range_start_date) == 0) {
		return;
	}

	cal_shell_content->priv->last_range_start = range_start_date;

	g_date_clear (&sel_start_date, 1);
	g_date_clear (&sel_end_date, 1);

	if (cal_shell_content->priv->view_start_range_day_offset == (guint32) -1) {
		sel_start_date = cal_shell_content->priv->view_start;
		sel_end_date = cal_shell_content->priv->view_end;
		cal_shell_content->priv->view_start_range_day_offset =
			g_date_get_julian (&cal_shell_content->priv->view_start) - g_date_get_julian (&range_start_date);
	} else {
		gint view_days;

		view_days = g_date_get_julian (&cal_shell_content->priv->view_end) - g_date_get_julian (&cal_shell_content->priv->view_start);

		sel_start_date = range_start_date;
		g_date_add_days (&sel_start_date, cal_shell_content->priv->view_start_range_day_offset);

		sel_end_date = sel_start_date;
		g_date_add_days (&sel_end_date, view_days);
	}

	g_signal_handler_block (calitem, cal_shell_content->priv->datepicker_range_moved_id);

	e_calendar_item_set_selection (calitem, &sel_start_date, &sel_end_date);

	g_signal_handler_unblock (calitem, cal_shell_content->priv->datepicker_range_moved_id);
}

static gboolean
cal_shell_content_datepicker_button_press_cb (ECalendar *calendar,
					      GdkEvent *event,
					      ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), FALSE);

	if (!event)
		return FALSE;

	if (event->type == GDK_2BUTTON_PRESS) {
		ECalendarItem *calitem = e_calendar_get_item (calendar);
		gdouble xwin = 0.0, ywin = 0.0;
		GDate sel_start, sel_end;

		/* Do that only if the double-click was above a day cell */
		if (!gdk_event_get_coords (event, &xwin, &ywin) ||
		    !e_calendar_item_convert_position_to_date (calitem, xwin, ywin, &sel_start)) {
			return FALSE;
		}

		g_date_clear (&sel_start, 1);
		g_date_clear (&sel_end, 1);

		if (!e_calendar_item_get_selection (calitem, &sel_start, &sel_end))
			return FALSE;

		/* Switch to a day view on a double-click */
		e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_DAY, &sel_start, &sel_start, FALSE);
	}

	return FALSE;
}

static void
cal_shell_content_current_view_id_changed_cb (ECalShellContent *cal_shell_content)
{
	GDate sel_start, sel_end;
	GDateWeekday work_day_first, week_start_day;
	ECalModel *model;
	gint ii;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	work_day_first = e_cal_model_get_work_day_first (model);
	week_start_day = e_cal_model_get_week_start_day (model);

	if (cal_shell_content->priv->previous_selected_start_time != -1 &&
	    cal_shell_content->priv->previous_selected_end_time != -1) {
		ICalTimezone *zone;

		zone = e_cal_model_get_timezone (model);
		time_to_gdate_with_zone (&sel_start, cal_shell_content->priv->previous_selected_start_time, zone);
		time_to_gdate_with_zone (&sel_end, cal_shell_content->priv->previous_selected_end_time, zone);
	} else {
		sel_start = cal_shell_content->priv->view_start;
		sel_end = cal_shell_content->priv->view_end;
	}

	switch (cal_shell_content->priv->current_view) {
		case E_CAL_VIEW_KIND_DAY:
		case E_CAL_VIEW_KIND_YEAR:
			/* Left the start & end being the current view start */
			sel_end = sel_start;
			break;
		case E_CAL_VIEW_KIND_WORKWEEK:
			cal_shell_content_clamp_for_whole_weeks (week_start_day, &sel_start, &sel_end, FALSE);
			ii = 0;
			while (g_date_get_weekday (&sel_start) != work_day_first && ii < 7) {
				g_date_add_days (&sel_start, 1);
				ii++;
			}

			sel_end = sel_start;
			g_date_add_days (&sel_end, e_day_view_get_days_shown (E_DAY_VIEW (cal_shell_content->priv->views[E_CAL_VIEW_KIND_WORKWEEK])) - 1);
			break;
		case E_CAL_VIEW_KIND_WEEK:
			sel_end = sel_start;
			cal_shell_content_clamp_for_whole_weeks (week_start_day, &sel_start, &sel_end, TRUE);
			break;
		case E_CAL_VIEW_KIND_MONTH:
		case E_CAL_VIEW_KIND_LIST:
			if (!calendar_config_get_month_start_with_current_week ()) {
				if (g_date_get_days_in_month (g_date_get_month (&sel_start), g_date_get_year (&sel_start)) - g_date_get_day (&sel_start) <= 7) {
					/* Keep the sel_start unchanged, because it's within the last week of the month,
					   which can be covered by the mini-calendar. Setting to the first day of the month
					   may mean the mini-calendar would go back by one month. */
				} else if (g_date_get_day (&sel_start) != 1 &&
					   (g_date_get_julian (&sel_end) - g_date_get_julian (&sel_start) + 1) / 7 >= 3 &&
					   g_date_get_month (&sel_start) != g_date_get_month (&sel_end)) {
					g_date_set_day (&sel_start, 1);
					g_date_add_months (&sel_start, 1);
				} else {
					g_date_set_day (&sel_start, 1);
				}
			}
			sel_end = sel_start;
			g_date_add_months (&sel_end, 1);
			g_date_subtract_days (&sel_end, 1);
			cal_shell_content_clamp_for_whole_weeks (week_start_day, &sel_start, &sel_end, cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_MONTH);
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	/* Ensure a change */
	e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &sel_start, &sel_end, TRUE);

	/* Try to preserve selection between the views */
	if (cal_shell_content->priv->previous_selected_start_time != -1 &&
	    cal_shell_content->priv->previous_selected_end_time != -1) {
		if (cal_shell_content->priv->current_view >= E_CAL_VIEW_KIND_DAY &&
		    cal_shell_content->priv->current_view < E_CAL_VIEW_KIND_LAST) {
			ECalendarView *cal_view = cal_shell_content->priv->views[cal_shell_content->priv->current_view];

			e_calendar_view_set_selected_time_range (cal_view,
				cal_shell_content->priv->previous_selected_start_time,
				cal_shell_content->priv->previous_selected_end_time);
		}
	}

	cal_shell_content->priv->previous_selected_start_time = -1;
	cal_shell_content->priv->previous_selected_end_time = -1;
}

static void
cal_shell_content_display_view_cb (ECalShellContent *cal_shell_content,
                                   GalView *gal_view)
{
	ECalViewKind view_kind;
	GType gal_view_type;

	gal_view_type = G_OBJECT_TYPE (gal_view);

	if (gal_view_type == GAL_TYPE_VIEW_ETABLE) {
		ECalendarView *calendar_view;

		view_kind = E_CAL_VIEW_KIND_LIST;
		calendar_view = cal_shell_content->priv->views[view_kind];
		gal_view_etable_attach_table (
			GAL_VIEW_ETABLE (gal_view),
			e_cal_list_view_get_table (E_CAL_LIST_VIEW (calendar_view)));

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_DAY) {
		view_kind = E_CAL_VIEW_KIND_DAY;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_WORK_WEEK) {
		view_kind = E_CAL_VIEW_KIND_WORKWEEK;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_WEEK) {
		view_kind = E_CAL_VIEW_KIND_WEEK;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_MONTH) {
		view_kind = E_CAL_VIEW_KIND_MONTH;

	} else if (gal_view_type == GAL_TYPE_VIEW_CALENDAR_YEAR) {
		view_kind = E_CAL_VIEW_KIND_YEAR;

	} else {
		g_return_if_reached ();
	}

	if (view_kind != E_CAL_VIEW_KIND_LIST) {
		EShellView *shell_view;

		shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));

		/* Reset these two filters, because they force the List View */
		if (e_ui_action_get_active (ACTION (CALENDAR_FILTER_ACTIVE_APPOINTMENTS)) ||
		    e_ui_action_get_active (ACTION (CALENDAR_FILTER_NEXT_7_DAYS_APPOINTMENTS)))
			e_ui_action_set_active (ACTION (CALENDAR_FILTER_ANY_CATEGORY), TRUE);
	}

	e_cal_shell_content_set_current_view_id (cal_shell_content, view_kind);
}

static void
cal_shell_content_notify_view_id_cb (ECalShellContent *cal_shell_content)
{
	EShellContent *shell_content;
	EShellView *shell_view;
	GSettings *settings;
	GtkWidget *paned;
	const gchar *key;
	const gchar *view_id;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	paned = cal_shell_content->priv->hpaned;

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	view_id = e_shell_view_get_view_id (shell_view);

	if (view_id != NULL && strcmp (view_id, "Month_View") == 0)
		key = "month-hpane-position";
	else
		key = "hpane-position";

	g_settings_unbind (paned, "hposition");

	g_settings_bind (
		settings, key,
		paned, "hposition",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
}

static void
cal_shell_content_is_editing_changed_cb (gpointer cal_view_tasks_memos_table,
                                         GParamSpec *param,
                                         EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	e_shell_view_update_actions (shell_view);
}

static gchar *
cal_shell_content_get_pad_state_filename (EShellContent *shell_content,
                                          ETable *table)
{
	EShellBackend *shell_backend;
	EShellView *shell_view;
	const gchar *config_dir, *nick = NULL;

	g_return_val_if_fail (shell_content != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL_CONTENT (shell_content), NULL);
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE (table), NULL);

	if (E_IS_TASK_TABLE (table))
		nick = "TaskPad";
	else if (E_IS_MEMO_TABLE (table))
		nick = "MemoPad";

	g_return_val_if_fail (nick != NULL, NULL);

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	config_dir = e_shell_backend_get_config_dir (shell_backend);

	return g_build_filename (config_dir, nick, NULL);
}

static void
cal_shell_content_save_table_state (EShellContent *shell_content,
                                    ETable *table)
{
	gchar *filename;

	filename = cal_shell_content_get_pad_state_filename (
		shell_content, table);
	g_return_if_fail (filename != NULL);

	e_table_save_state (table, filename);
	g_free (filename);
}

static void
cal_shell_content_load_table_state (EShellContent *shell_content,
                                    ETable *table)
{
	gchar *filename;

	filename = cal_shell_content_get_pad_state_filename (shell_content, table);
	g_return_if_fail (filename != NULL);

	e_table_load_state (table, filename);
	g_free (filename);
}

static ICalProperty *
cal_shell_content_get_attendee_prop (ICalComponent *icomp,
                                     const gchar *address)
{
	ICalProperty *prop;

	if (address == NULL || *address == '\0')
		return NULL;

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee;

		attendee = e_cal_util_get_property_email (prop);

		if (e_cal_util_email_addresses_equal (attendee, address))
			return prop;
	}

	return NULL;
}

static gboolean
cal_shell_content_icomp_is_delegated (ICalComponent *icomp,
				      const gchar *user_email)
{
	ICalProperty *prop;
	ICalParameter *param;
	gchar *delto = NULL;
	gboolean is_delegated = FALSE;

	prop = cal_shell_content_get_attendee_prop (icomp, user_email);

	if (prop) {
		param = i_cal_property_get_first_parameter (prop, I_CAL_DELEGATEDTO_PARAMETER);
		if (param) {
			delto = g_strdup (e_cal_util_strip_mailto (i_cal_parameter_get_delegatedto (param)));
			g_object_unref (param);
		}

		g_object_unref (prop);
	} else
		return FALSE;

	prop = cal_shell_content_get_attendee_prop (icomp, delto);

	if (prop) {
		gchar *delfrom = NULL;
		ICalParameterPartstat partstat = I_CAL_PARTSTAT_NONE;

		param = i_cal_property_get_first_parameter (prop, I_CAL_DELEGATEDFROM_PARAMETER);
		if (param) {
			delfrom = g_strdup (e_cal_util_strip_mailto (i_cal_parameter_get_delegatedfrom (param)));
			g_object_unref (param);
		}

		param = i_cal_property_get_first_parameter (prop, I_CAL_PARTSTAT_PARAMETER);
		if (param) {
			partstat = i_cal_parameter_get_partstat (param);
			g_object_unref (param);
		}

		is_delegated = delfrom && user_email &&
			partstat != I_CAL_PARTSTAT_DECLINED &&
			g_ascii_strcasecmp (delfrom, user_email) == 0;

		g_object_unref (prop);
		g_free (delfrom);
	}

	g_free (delto);

	return is_delegated;
}

static guint32
cal_shell_content_check_state (EShellContent *shell_content)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	ESourceRegistry *registry;
	ECalShellContent *cal_shell_content;
	ECalendarView *calendar_view;
	gboolean selection_is_editable = FALSE;
	gboolean selection_is_instance = FALSE;
	gboolean selection_is_meeting = FALSE;
	gboolean selection_is_organizer = FALSE;
	gboolean selection_is_attendee = FALSE;
	gboolean selection_is_recurring = FALSE;
	gboolean selection_can_delegate = FALSE;
	gboolean this_and_future_supported = FALSE;
	guint32 state = 0;
	GSList *selected, *link;
	guint n_selected;

	cal_shell_content = E_CAL_SHELL_CONTENT (shell_content);

	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	calendar_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

	selected = e_calendar_view_get_selected_events (calendar_view);
	n_selected = g_slist_length (selected);

	/* If we have a selection, assume it's
	 * editable until we learn otherwise. */
	if (n_selected > 0)
		selection_is_editable = TRUE;

	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;
		ECalClient *client;
		ECalComponent *comp;
		gchar *user_email;
		ICalComponent *icomp;
		const gchar *capability;
		gboolean cap_delegate_supported;
		gboolean cap_delegate_to_many;
		gboolean icomp_is_delegated;
		gboolean read_only;

		client = sel_data->client;
		icomp = sel_data->icalcomp;

		read_only = e_client_is_readonly (E_CLIENT (client));
		selection_is_editable &= !read_only;

		selection_is_instance |=
			e_cal_util_component_is_instance (icomp);

		selection_is_meeting =
			(n_selected == 1) &&
			e_cal_util_component_has_attendee (icomp);

		selection_is_recurring |=
			e_cal_util_component_is_instance (icomp) ||
			e_cal_util_component_has_recurrences (icomp);

		/* XXX The rest of this is rather expensive and
		 *     only applies if a single event is selected,
		 *     so continue with the loop iteration if the
		 *     rest of this is not applicable. */
		if (n_selected > 1)
			continue;

		/* XXX This probably belongs in comp-util.c. */

		comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
		user_email = itip_get_comp_attendee (registry, comp, client);

		selection_is_organizer =
			e_cal_util_component_has_organizer (icomp) &&
			itip_organizer_is_user (registry, comp, client);

		capability = E_CAL_STATIC_CAPABILITY_DELEGATE_SUPPORTED;
		cap_delegate_supported =
			e_client_check_capability (
			E_CLIENT (client), capability);

		capability = E_CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY;
		cap_delegate_to_many =
			e_client_check_capability (
			E_CLIENT (client), capability);

		this_and_future_supported = !e_client_check_capability (
			E_CLIENT (client), E_CAL_STATIC_CAPABILITY_NO_THISANDFUTURE);

		icomp_is_delegated = user_email != NULL &&
			cal_shell_content_icomp_is_delegated (icomp, user_email);

		selection_can_delegate =
			cap_delegate_supported &&
			(cap_delegate_to_many ||
			(!selection_is_organizer &&
			 !icomp_is_delegated));

		selection_is_attendee = !selection_is_organizer &&
			selection_is_meeting &&
			!icomp_is_delegated &&
			itip_attendee_is_user (registry, comp, client);

		g_free (user_email);
		g_object_unref (comp);
	}

	g_slist_free_full (selected, e_calendar_view_selection_data_free);

	if (n_selected == 1)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (selection_is_editable)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_EDITABLE;
	if (selection_is_instance)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_INSTANCE;
	if (selection_is_meeting)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_MEETING;
	if (selection_is_organizer)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_ORGANIZER;
	if (selection_is_attendee)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_ATTENDEE;
	if (selection_is_recurring)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_IS_RECURRING;
	if (selection_can_delegate)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_CAN_DELEGATE;
	if (this_and_future_supported)
		state |= E_CAL_BASE_SHELL_CONTENT_SELECTION_THIS_AND_FUTURE_SUPPORTED;

	return state;
}

static void
cal_shell_content_focus_search_results (EShellContent *shell_content)
{
	ECalendarView *calendar_view;

	calendar_view = e_cal_shell_content_get_current_calendar_view (E_CAL_SHELL_CONTENT (shell_content));

	gtk_widget_grab_focus (GTK_WIDGET (calendar_view));
}

static time_t
cal_shell_content_get_default_time (ECalModel *model,
				    gpointer user_data)
{
	ECalShellContent *cal_shell_content = user_data;
	ICalTimezone *zone;
	ICalTime *itt;
	time_t tt;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), 0);

	if (e_cal_shell_content_get_current_view_id (cal_shell_content) != E_CAL_VIEW_KIND_LIST) {
		ECalendarView *cal_view;
		time_t selected_start = (time_t) 0, selected_end = (time_t) 0;

		cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);

		if (cal_view && e_calendar_view_get_selected_time_range (cal_view, &selected_start, &selected_end))
			return selected_start;
	}

	zone = e_cal_model_get_timezone (model);
	itt = i_cal_time_new_current_with_zone (zone);
	tt = i_cal_time_as_timet_with_zone (itt, zone);
	g_clear_object (&itt);

	return tt;
}

static void
update_adjustment (ECalShellContent *cal_shell_content,
                   GtkAdjustment *adjustment,
                   EWeekView *week_view,
		   gboolean move_by_week)
{
	GDate start_date, end_date;
	GDate first_day_shown;
	ECalModel *model;
	gint week_offset;
	ICalTime *start_tt = NULL;
	ICalTimezone *timezone;
	time_t lower;
	guint32 old_first_day_julian, new_first_day_julian;
	gdouble value;

	e_week_view_get_first_day_shown (week_view, &first_day_shown);

	/* If we don't have a valid date set yet, just return. */
	if (!g_date_valid (&first_day_shown))
		return;

	value = gtk_adjustment_get_value (adjustment);

	/* Determine the first date shown. */
	start_date = week_view->base_date;
	week_offset = floor (value + 0.5);

	if (week_offset > 0)
		g_date_add_days (&start_date, week_offset * 7);
	else
		g_date_subtract_days (&start_date, week_offset * (-7));

	/* Convert the old & new first days shown to julian values. */
	old_first_day_julian = g_date_get_julian (&first_day_shown);
	new_first_day_julian = g_date_get_julian (&start_date);

	/* If we are already showing the date, just return. */
	if (old_first_day_julian == new_first_day_julian)
		return;

	/* Convert it to a time_t. */
	start_tt = i_cal_time_new_null_time ();
	i_cal_time_set_date (start_tt,
		g_date_get_year (&start_date),
		g_date_get_month (&start_date),
		g_date_get_day (&start_date));

	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	timezone = e_cal_model_get_timezone (model);
	lower = i_cal_time_as_timet_with_zone (start_tt, timezone);
	g_clear_object (&start_tt);

	end_date = start_date;
	if (move_by_week) {
		g_date_add_days (&end_date, 7 - 1);
	} else {
		g_date_add_days (&end_date, 7 * e_week_view_get_weeks_shown (week_view) - 1);
	}

	e_week_view_set_update_base_date (week_view, FALSE);
	e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &start_date, &end_date, FALSE);
	e_calendar_view_set_selected_time_range (E_CALENDAR_VIEW (week_view), lower, lower);
	e_week_view_set_update_base_date (week_view, TRUE);
}

static void
week_view_adjustment_changed_cb (GtkAdjustment *adjustment,
				 ECalShellContent *cal_shell_content)
{
	ECalendarView *view;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	view = cal_shell_content->priv->views[E_CAL_VIEW_KIND_WEEK];
	update_adjustment (cal_shell_content, adjustment, E_WEEK_VIEW (view), TRUE);
}

static void
month_view_adjustment_changed_cb (GtkAdjustment *adjustment,
				  ECalShellContent *cal_shell_content)
{
	ECalendarView *view;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	view = cal_shell_content->priv->views[E_CAL_VIEW_KIND_MONTH];
	update_adjustment (cal_shell_content, adjustment, E_WEEK_VIEW (view), FALSE);
}

static void
cal_shell_content_notify_work_day_cb (ECalModel *model,
				      GParamSpec *param,
				      ECalShellContent *cal_shell_content)
{
	GDateWeekday work_day_first, work_day_last;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (cal_shell_content->priv->current_view != E_CAL_VIEW_KIND_WORKWEEK)
		return;

	work_day_first = e_cal_model_get_work_day_first (model);
	work_day_last = e_cal_model_get_work_day_last (model);

	if (work_day_first == g_date_get_weekday (&cal_shell_content->priv->view_start) &&
	    work_day_last == g_date_get_weekday (&cal_shell_content->priv->view_end))
		return;

	cal_shell_content->priv->previous_selected_start_time = -1;
	cal_shell_content->priv->previous_selected_end_time = -1;

	/* This makes sure that the selection in the datepicker corresponds
	   to the time range used in the Work Week view */
	cal_shell_content_current_view_id_changed_cb (cal_shell_content);
}

static void
cal_shell_content_notify_week_start_day_cb (ECalModel *model,
					    GParamSpec *param,
					    ECalShellContent *cal_shell_content)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	cal_shell_content->priv->previous_selected_start_time = -1;
	cal_shell_content->priv->previous_selected_end_time = -1;

	/* This makes sure that the selection in the datepicker corresponds
	   to the time range used in the current view */
	cal_shell_content_current_view_id_changed_cb (cal_shell_content);
}

static void
cal_shell_content_move_view_range_cb (ECalendarView *cal_view,
				      ECalendarViewMoveType move_type,
				      gint64 exact_date,
				      ECalShellContent *cal_shell_content)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (!cal_view->in_focus)
		return;

	e_cal_shell_content_move_view_range (cal_shell_content, move_type, (time_t) exact_date);
}

static void
cal_shell_content_clear_all_in_list_view (ECalShellContent *cal_shell_content)
{
	ECalDataModelSubscriber *subscriber;

	subscriber = E_CAL_DATA_MODEL_SUBSCRIBER (cal_shell_content->priv->list_view_model);

	e_cal_data_model_unsubscribe (cal_shell_content->priv->list_view_data_model, subscriber);
	e_cal_model_remove_all_objects (cal_shell_content->priv->list_view_model);
	e_cal_data_model_remove_all_clients (cal_shell_content->priv->list_view_data_model);
	e_cal_data_model_subscribe (cal_shell_content->priv->list_view_data_model, subscriber, 0, 0);
}

static void
cal_shell_content_client_opened_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
				    EClient *client,
				    gpointer user_data)
{
	ECalShellContent *cal_shell_content = user_data;
	ESourceSelector *source_selector;
	ESource *source;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (cal_shell_content->priv->current_view != E_CAL_VIEW_KIND_LIST || !E_IS_CAL_CLIENT (client))
		return;

	source_selector = e_cal_base_shell_sidebar_get_selector (cal_base_shell_sidebar);
	source = e_source_selector_ref_primary_selection (source_selector);

	/* It can happen that the previously opening calendar finished its open
	   after the current calendar, in which case this would replace the data,
	   thus ensure the opened calendar is the correct calendar. */
	if (source == e_client_get_source (client)) {
		cal_shell_content_clear_all_in_list_view (cal_shell_content);
		e_cal_data_model_add_client (cal_shell_content->priv->list_view_data_model, E_CAL_CLIENT (client));
	}

	g_clear_object (&source);
}

static void
cal_shell_content_update_list_view (ECalShellContent *cal_shell_content)
{
	ECalBaseShellSidebar *cal_base_shell_sidebar;
	ESourceSelector *source_selector;
	ECalClient *client;
	ESource *source;

	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (e_shell_view_get_shell_sidebar (
		e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content))));

	source_selector = e_cal_base_shell_sidebar_get_selector (cal_base_shell_sidebar);

	source = e_source_selector_ref_primary_selection (source_selector);
	if (!source)
		return;

	e_cal_model_set_default_source_uid (cal_shell_content->priv->list_view_model, e_source_get_uid (source));

	client = e_cal_data_model_ref_client (cal_shell_content->priv->list_view_data_model, e_source_get_uid (source));

	if (!client)
		e_cal_base_shell_sidebar_open_source (cal_base_shell_sidebar, source, cal_shell_content_client_opened_cb, cal_shell_content);

	g_clear_object (&client);
	g_clear_object (&source);
}

static void
cal_shell_content_primary_selection_changed_cb (ESourceSelector *selector,
						gpointer user_data)
{
	ECalShellContent *cal_shell_content = user_data;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_LIST)
		cal_shell_content_update_list_view (cal_shell_content);
}

static void
cal_shell_content_foreign_client_opened_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
					    ECalClient *client,
					    ECalModel *model)
{
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	e_cal_data_model_add_client (e_cal_model_get_data_model (model), client);
}

static void
cal_shell_content_foreign_client_closed_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
					    ESource *source,
					    ECalModel *model)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	e_cal_data_model_remove_client (e_cal_model_get_data_model (model), e_source_get_uid (source));
}

static void
cal_shell_content_setup_foreign_sources (EShellWindow *shell_window,
					 const gchar *view_name,
					 const gchar *extension_name,
					 ECalModel *model)
{
	EShellSidebar *foreign_sidebar;
	EShellContent *foreign_content;
	EShellView *foreign_view;
	ECalModel *foreign_model;
	GList *clients;
	gboolean is_new_view;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	is_new_view = e_shell_window_peek_shell_view (shell_window, view_name) == NULL;

	foreign_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (E_IS_SHELL_VIEW (foreign_view));

	foreign_sidebar = e_shell_view_get_shell_sidebar (foreign_view);
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (foreign_sidebar));

	if (is_new_view) {
		/* Preselect default source, when the view was not created yet */
		ESourceSelector *source_selector;
		ESourceRegistry *registry;
		ESource *source;

		source_selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (foreign_sidebar));
		registry = e_source_selector_get_registry (source_selector);
		source = e_source_registry_ref_default_for_extension_name (registry, extension_name);

		if (source)
			e_source_selector_set_primary_selection (source_selector, source);

		g_clear_object (&source);
	}

	g_signal_connect_object (foreign_sidebar, "client-opened",
		G_CALLBACK (cal_shell_content_foreign_client_opened_cb), model, 0);
	g_signal_connect_object (foreign_sidebar, "client-closed",
		G_CALLBACK (cal_shell_content_foreign_client_closed_cb), model, 0);

	foreign_content = e_shell_view_get_shell_content (foreign_view);
	foreign_model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (foreign_content));

	e_binding_bind_property (
		foreign_model, "default-source-uid",
		model, "default-source-uid",
		G_BINDING_SYNC_CREATE);

	g_signal_connect_object (model, "row-appended",
		G_CALLBACK (e_cal_base_shell_view_model_row_appended), foreign_view, G_CONNECT_SWAPPED);

	/* Add clients already opened in the foreign view */
	clients = e_cal_data_model_get_clients (e_cal_model_get_data_model (foreign_model));
	if (clients) {
		ECalDataModel *data_model;
		GList *link;

		data_model = e_cal_model_get_data_model (model);

		for (link = clients; link; link = g_list_next (link)) {
			ECalClient *client = link->data;
			e_cal_data_model_add_client (data_model, client);
		}

		g_list_free_full (clients, g_object_unref);
	}

	/* This makes sure that the local models for memos and tasks
	   in the calendar view get populated with the same sources
	   as those in the respective views. */

	e_cal_base_shell_sidebar_ensure_sources_open (E_CAL_BASE_SHELL_SIDEBAR (foreign_sidebar));
}

static gboolean
cal_shell_content_update_tasks_table_cb (gpointer user_data)
{
	ECalShellContent *self = user_data;

	if (self->priv->task_table)
		e_task_table_process_completed_tasks (E_TASK_TABLE (self->priv->task_table), FALSE);
	if (self->priv->task_model)
		e_cal_model_tasks_update_due_tasks (E_CAL_MODEL_TASKS (self->priv->task_model));

	return G_SOURCE_CONTINUE;
}

static void
cal_shell_content_view_created (ECalBaseShellContent *cal_base_shell_content)
{
	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GalViewInstance *view_instance;
	ECalendar *calendar;
	ECalModel *model;
	ECalDataModel *data_model;
	GDate date;
	time_t today;

	cal_shell_content = E_CAL_SHELL_CONTENT (cal_base_shell_content);
	cal_shell_content->priv->current_view = E_CAL_VIEW_KIND_DAY;

	today = time (NULL);
	g_date_clear (&date, 1);
	g_date_set_time_t (&date, today);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	calendar = e_cal_base_shell_sidebar_get_date_navigator (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_return_if_fail (E_IS_CALENDAR (calendar));

	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	e_calendar_item_set_selection (e_calendar_get_item (calendar), &date, &date);
	e_cal_model_set_time_range (model, today, today);

	/* Show everything known by default in the task and memo pads */
	e_cal_model_set_time_range (cal_shell_content->priv->memo_model, 0, 0);
	e_cal_model_set_time_range (cal_shell_content->priv->task_model, 0, 0);
	e_cal_model_set_time_range (cal_shell_content->priv->list_view_model, 0, 0);

	g_signal_connect (e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar)), "primary-selection-changed",
		G_CALLBACK (cal_shell_content_primary_selection_changed_cb), cal_shell_content);

	cal_shell_content->priv->datepicker_selection_changed_id =
		g_signal_connect (e_calendar_get_item (calendar), "selection-changed",
		G_CALLBACK (cal_shell_content_datepicker_selection_changed_cb), cal_shell_content);
	cal_shell_content->priv->datepicker_range_moved_id =
		g_signal_connect (e_calendar_get_item (calendar), "date-range-moved",
		G_CALLBACK (cal_shell_content_datepicker_range_moved_cb), cal_shell_content);

	g_signal_connect_after (calendar, "button-press-event",
		G_CALLBACK (cal_shell_content_datepicker_button_press_cb), cal_shell_content);

	data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	cal_shell_content->priv->tag_calendar = e_tag_calendar_new (calendar);
	e_tag_calendar_subscribe (cal_shell_content->priv->tag_calendar, data_model);

	/* Intentionally not using e_signal_connect_notify() here, no need to filter
	   out "false" notifications, it's dealt with them in another way */
	cal_shell_content->priv->current_view_id_changed_id = g_signal_connect (
		cal_shell_content, "notify::current-view-id",
		G_CALLBACK (cal_shell_content_current_view_id_changed_cb), NULL);

	/* List of selected Task/Memo sources is taken from respective views,
	   which are loaded if necessary. */
	cal_shell_content_setup_foreign_sources (shell_window, "memos", E_SOURCE_EXTENSION_MEMO_LIST,
		cal_shell_content->priv->memo_model);

	cal_shell_content_setup_foreign_sources (shell_window, "tasks", E_SOURCE_EXTENSION_TASK_LIST,
		cal_shell_content->priv->task_model);

	/* Finally load the view instance */
	view_instance = e_shell_view_get_view_instance (shell_view);
	gal_view_instance_load (view_instance);

	/* Keep the toolbar view buttons in sync with the calendar. */
	e_binding_bind_property_full (
		cal_shell_content, "current-view-id",
		ACTION (CALENDAR_VIEW_DAY), "state",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_ui_action_util_gvalue_to_enum_state,
		e_ui_action_util_enum_state_to_gvalue,
		NULL, NULL);

	e_signal_connect_notify (
		model, "notify::work-day-monday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-tuesday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-wednesday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-thursday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-friday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-saturday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::work-day-sunday",
		G_CALLBACK (cal_shell_content_notify_work_day_cb), cal_shell_content);

	e_signal_connect_notify (
		model, "notify::week-start-day",
		G_CALLBACK (cal_shell_content_notify_week_start_day_cb), cal_shell_content);

	cal_shell_content->priv->initialized = TRUE;
}

static void
e_cal_shell_content_create_calendar_views (ECalShellContent *cal_shell_content)
{
	EShellView *shell_view;
	ECalModel *model;
	ECalendarView *calendar_view;
	GtkAdjustment *adjustment;
	GSettings *settings;
	time_t today;
	gint ii;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (cal_shell_content->priv->calendar_notebook != NULL);
	g_return_if_fail (cal_shell_content->priv->views[0] == NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));

	/* Day View */
	calendar_view = e_day_view_new (model);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_DAY] = calendar_view;
	g_object_ref_sink (calendar_view);

	/* Work Week View */
	calendar_view = e_day_view_new (model);
	e_day_view_set_work_week_view (E_DAY_VIEW (calendar_view), TRUE);
	e_day_view_set_days_shown (E_DAY_VIEW (calendar_view), 5);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_WORKWEEK] = calendar_view;
	g_object_ref_sink (calendar_view);

	/* Week View */
	calendar_view = e_week_view_new (model);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_WEEK] = calendar_view;
	g_object_ref_sink (calendar_view);

	adjustment = gtk_range_get_adjustment (
		GTK_RANGE (E_WEEK_VIEW (calendar_view)->vscrollbar));
	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (week_view_adjustment_changed_cb), cal_shell_content);

	/* Month View */
	calendar_view = e_month_view_new (model);
	e_week_view_set_multi_week_view (E_WEEK_VIEW (calendar_view), TRUE);
	e_week_view_set_weeks_shown (E_WEEK_VIEW (calendar_view), 6);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_MONTH] = calendar_view;
	g_object_ref_sink (calendar_view);

	adjustment = gtk_range_get_adjustment (
		GTK_RANGE (E_WEEK_VIEW (calendar_view)->vscrollbar));
	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (month_view_adjustment_changed_cb), cal_shell_content);

	/* Year View */
	calendar_view = e_year_view_new (model);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_YEAR] = calendar_view;
	g_object_ref_sink (calendar_view);

	/* List View */
	calendar_view = e_cal_list_view_new (cal_shell_content->priv->list_view_model);
	cal_shell_content->priv->views[E_CAL_VIEW_KIND_LIST] = calendar_view;
	g_object_ref_sink (calendar_view);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));
	today = time (NULL);

	for (ii = 0; ii < E_CAL_VIEW_KIND_LAST; ii++) {
		calendar_view = cal_shell_content->priv->views[ii];

		calendar_view->in_focus = ii == cal_shell_content->priv->current_view;

		e_calendar_view_set_selected_time_range (calendar_view, today, today);

		e_signal_connect_notify (
			calendar_view, "notify::is-editing",
			G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

		g_signal_connect (
			calendar_view, "move-view-range",
			G_CALLBACK (cal_shell_content_move_view_range_cb), cal_shell_content);

		gtk_notebook_append_page (
			GTK_NOTEBOOK (cal_shell_content->priv->calendar_notebook),
			GTK_WIDGET (calendar_view), NULL);
		gtk_widget_show (GTK_WIDGET (calendar_view));
	}

	g_object_unref (settings);
}

static void
cal_shell_content_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_VIEW_ID:
			e_cal_shell_content_set_current_view_id (E_CAL_SHELL_CONTENT (object),
				g_value_get_int (value));
			return;
		case PROP_SHOW_TAG_VPANE:
			e_cal_shell_content_set_show_tag_vpane (E_CAL_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CALENDAR_NOTEBOOK:
			g_value_set_object (
				value, e_cal_shell_content_get_calendar_notebook (
				E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_MEMO_TABLE:
			g_value_set_object (
				value, e_cal_shell_content_get_memo_table (
				E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_TASK_TABLE:
			g_value_set_object (
				value, e_cal_shell_content_get_task_table (
				E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_CURRENT_VIEW_ID:
			g_value_set_int (value,
				e_cal_shell_content_get_current_view_id (E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_CURRENT_VIEW:
			g_value_set_object (value,
				e_cal_shell_content_get_current_calendar_view (E_CAL_SHELL_CONTENT (object)));
			return;

		case PROP_SHOW_TAG_VPANE:
			g_value_set_boolean (value,
				e_cal_shell_content_get_show_tag_vpane (E_CAL_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_dispose (GObject *object)
{
	ECalShellContent *cal_shell_content = E_CAL_SHELL_CONTENT (object);
	gint ii;

	if (cal_shell_content->priv->task_table_update_id) {
		g_source_remove (cal_shell_content->priv->task_table_update_id);
		cal_shell_content->priv->task_table_update_id = 0;
	}

	if (cal_shell_content->priv->task_data_model) {
		e_cal_data_model_set_disposing (cal_shell_content->priv->task_data_model, TRUE);
		e_cal_data_model_unsubscribe (cal_shell_content->priv->task_data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (cal_shell_content->priv->task_model));
	}

	if (cal_shell_content->priv->memo_data_model) {
		e_cal_data_model_set_disposing (cal_shell_content->priv->memo_data_model, TRUE);
		e_cal_data_model_unsubscribe (cal_shell_content->priv->memo_data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (cal_shell_content->priv->memo_model));
	}

	if (cal_shell_content->priv->list_view_data_model) {
		e_cal_data_model_set_disposing (cal_shell_content->priv->list_view_data_model, TRUE);
		e_cal_data_model_unsubscribe (cal_shell_content->priv->list_view_data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (cal_shell_content->priv->list_view_model));
	}

	if (cal_shell_content->priv->tag_calendar) {
		ECalDataModel *data_model;

		data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
		e_cal_data_model_set_disposing (data_model, TRUE);
		e_tag_calendar_unsubscribe (cal_shell_content->priv->tag_calendar, data_model);
		g_clear_object (&cal_shell_content->priv->tag_calendar);
	}

	for (ii = 0; ii < E_CAL_VIEW_KIND_LAST; ii++) {
		g_clear_object (&(cal_shell_content->priv->views[ii]));
	}

	g_clear_object (&cal_shell_content->priv->hpaned);
	g_clear_object (&cal_shell_content->priv->vpaned);
	g_clear_object (&cal_shell_content->priv->calendar_notebook);
	g_clear_object (&cal_shell_content->priv->task_table);
	g_clear_object (&cal_shell_content->priv->task_model);
	g_clear_object (&cal_shell_content->priv->task_data_model);
	g_clear_object (&cal_shell_content->priv->memo_table);
	g_clear_object (&cal_shell_content->priv->memo_model);
	g_clear_object (&cal_shell_content->priv->memo_data_model);
	g_clear_object (&cal_shell_content->priv->list_view_model);
	g_clear_object (&cal_shell_content->priv->list_view_data_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_shell_content_parent_class)->dispose (object);
}

static void
cal_shell_content_constructed (GObject *object)
{
	ECalShellContent *cal_shell_content;
	EShellContent *shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShell *shell;
	GalViewInstance *view_instance;
	GSettings *settings;
	GtkWidget *container;
	GtkWidget *widget;
	gchar *markup;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_content_parent_class)->constructed (object);

	cal_shell_content = E_CAL_SHELL_CONTENT (object);
	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content->priv->memo_data_model =
		e_cal_base_shell_content_create_new_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	cal_shell_content->priv->memo_model =
		e_cal_model_memos_new (cal_shell_content->priv->memo_data_model, e_shell_get_registry (shell), shell);

	cal_shell_content->priv->task_data_model =
		e_cal_base_shell_content_create_new_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	cal_shell_content->priv->task_model =
		e_cal_model_tasks_new (cal_shell_content->priv->task_data_model, e_shell_get_registry (shell), shell);

	cal_shell_content->priv->list_view_data_model =
		e_cal_base_shell_content_create_new_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	cal_shell_content->priv->list_view_model =
		e_cal_model_calendar_new (cal_shell_content->priv->list_view_data_model, e_shell_get_registry (shell), shell);

	e_binding_bind_property (
		cal_shell_content->priv->memo_model, "timezone",
		cal_shell_content->priv->memo_data_model, "timezone",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cal_shell_content->priv->task_model, "timezone",
		cal_shell_content->priv->task_data_model, "timezone",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		cal_shell_content->priv->list_view_model, "timezone",
		cal_shell_content->priv->list_view_data_model, "timezone",
		G_BINDING_SYNC_CREATE);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	cal_shell_content->priv->hpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	container = cal_shell_content->priv->hpaned;

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	cal_shell_content->priv->calendar_notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	e_paned_set_fixed_resize (E_PANED (widget), FALSE);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, TRUE);
	cal_shell_content->priv->vpaned = g_object_ref (widget);
	gtk_widget_show (widget);

	e_cal_shell_content_create_calendar_views (cal_shell_content);

	e_binding_bind_property (
		cal_shell_content, "current-view-id",
		cal_shell_content->priv->calendar_notebook, "page",
		G_BINDING_SYNC_CREATE);

	container = cal_shell_content->priv->vpaned;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, TRUE);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (NULL);
	markup = g_strdup_printf ("<b>%s</b>", _("Tasks"));
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_task_table_new (shell_view, cal_shell_content->priv->task_model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	cal_shell_content->priv->task_table = g_object_ref (widget);
	gtk_widget_show (widget);

	cal_shell_content_load_table_state (shell_content, E_TABLE (widget));

	g_signal_connect_swapped (
		widget, "open-component",
		G_CALLBACK (e_cal_shell_view_taskpad_open_task),
		shell_view);

	e_signal_connect_notify (
		widget, "notify::is-editing",
		G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

	container = cal_shell_content->priv->vpaned;

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (container), widget, TRUE, TRUE);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (NULL);
	markup = g_strdup_printf ("<b>%s</b>", _("Memos"));
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, TRUE, 0);
	gtk_widget_show (widget);
	g_free (markup);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_memo_table_new (shell_view, cal_shell_content->priv->memo_model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	cal_shell_content->priv->memo_table = g_object_ref (widget);
	gtk_widget_show (widget);

	cal_shell_content_load_table_state (shell_content, E_TABLE (widget));

	e_cal_model_set_default_time_func (cal_shell_content->priv->memo_model, cal_shell_content_get_default_time, cal_shell_content);

	g_signal_connect_swapped (
		widget, "open-component",
		G_CALLBACK (e_cal_shell_view_memopad_open_memo),
		shell_view);

	e_signal_connect_notify (
		widget, "notify::is-editing",
		G_CALLBACK (cal_shell_content_is_editing_changed_cb), shell_view);

	/* Prepare the view instance. */

	view_instance = e_shell_view_new_view_instance (shell_view, NULL);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (cal_shell_content_display_view_cb),
		object);
	/* Actual load happens at cal_shell_content_view_created() */
	e_shell_view_set_view_instance (shell_view, view_instance);
	g_object_unref (view_instance);

	e_signal_connect_notify_swapped (
		shell_view, "notify::view-id",
		G_CALLBACK (cal_shell_content_notify_view_id_cb),
		cal_shell_content);

	cal_shell_content->priv->task_table_update_id = e_named_timeout_add_seconds_full (
		G_PRIORITY_LOW, 60,
		cal_shell_content_update_tasks_table_cb,
		cal_shell_content, NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "tag-vpane-position",
		cal_shell_content->priv->vpaned, "proportion",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "show-tag-vpane",
		cal_shell_content, "show-tag-vpane",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Cannot access shell sidebar here, thus rely on cal_shell_content_view_created()
	   with exact widget settings which require it */
}

static void
e_cal_shell_content_class_init (ECalShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;
	ECalBaseShellContentClass *cal_base_shell_content_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_shell_content_set_property;
	object_class->get_property = cal_shell_content_get_property;
	object_class->dispose = cal_shell_content_dispose;
	object_class->constructed = cal_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = cal_shell_content_check_state;
	shell_content_class->focus_search_results = cal_shell_content_focus_search_results;

	cal_base_shell_content_class = E_CAL_BASE_SHELL_CONTENT_CLASS (class);
	cal_base_shell_content_class->new_cal_model = e_cal_model_calendar_new;
	cal_base_shell_content_class->view_created = cal_shell_content_view_created;

	g_object_class_install_property (
		object_class,
		PROP_CALENDAR_NOTEBOOK,
		g_param_spec_object (
			"calendar-notebook",
			NULL,
			NULL,
			GTK_TYPE_NOTEBOOK,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MEMO_TABLE,
		g_param_spec_object (
			"memo-table",
			NULL,
			NULL,
			E_TYPE_MEMO_TABLE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_TASK_TABLE,
		g_param_spec_object (
			"task-table",
			NULL,
			NULL,
			E_TYPE_TASK_TABLE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VIEW_ID,
		g_param_spec_int (
			"current-view-id",
			"Current Calendar View ID",
			NULL,
			E_CAL_VIEW_KIND_DAY,
			E_CAL_VIEW_KIND_LAST - 1,
			E_CAL_VIEW_KIND_DAY,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_VIEW,
		g_param_spec_object (
			"current-view",
			"Current Calendar View",
			NULL,
			E_TYPE_CALENDAR_VIEW,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_TAG_VPANE,
		g_param_spec_boolean (
			"show-tag-vpane",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));
}

static void
e_cal_shell_content_class_finalize (ECalShellContentClass *class)
{
}

static void
e_cal_shell_content_init (ECalShellContent *cal_shell_content)
{
	time_t now;

	cal_shell_content->priv = e_cal_shell_content_get_instance_private (cal_shell_content);
	g_date_clear (&cal_shell_content->priv->view_start, 1);
	g_date_clear (&cal_shell_content->priv->view_end, 1);
	g_date_clear (&cal_shell_content->priv->last_range_start, 1);

	now = time (NULL);
	g_date_set_time_t (&cal_shell_content->priv->view_start, now);
	g_date_set_time_t (&cal_shell_content->priv->view_end, now);

	cal_shell_content->priv->view_start_range_day_offset = (guint32) -1;
	cal_shell_content->priv->previous_selected_start_time = -1;
	cal_shell_content->priv->previous_selected_end_time = -1;
	cal_shell_content->priv->initialized = FALSE;
}

void
e_cal_shell_content_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_shell_content_register_type (type_module);
}

GtkWidget *
e_cal_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

gboolean
e_cal_shell_content_get_initialized (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), FALSE);

	return cal_shell_content->priv->initialized;
}

GtkNotebook *
e_cal_shell_content_get_calendar_notebook (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return GTK_NOTEBOOK (cal_shell_content->priv->calendar_notebook);
}

EMemoTable *
e_cal_shell_content_get_memo_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_MEMO_TABLE (cal_shell_content->priv->memo_table);
}

ETaskTable *
e_cal_shell_content_get_task_table (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return E_TASK_TABLE (cal_shell_content->priv->task_table);
}

EShellSearchbar *
e_cal_shell_content_get_searchbar (ECalShellContent *cal_shell_content)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	shell_content = E_SHELL_CONTENT (cal_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);
}

static void
cal_shell_content_resubscribe (ECalendarView *cal_view,
			       ECalModel *model)
{
	ECalDataModel *data_model;
	ECalDataModelSubscriber *subscriber;
	time_t range_start, range_end;
	gboolean is_tasks_or_memos;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	data_model = e_cal_model_get_data_model (model);
	subscriber = E_CAL_DATA_MODEL_SUBSCRIBER (model);
	is_tasks_or_memos = e_cal_model_get_component_kind (model) == I_CAL_VJOURNAL_COMPONENT ||
		e_cal_model_get_component_kind (model) == I_CAL_VTODO_COMPONENT;

	if ((!is_tasks_or_memos && e_calendar_view_get_visible_time_range (cal_view, &range_start, &range_end)) ||
	    e_cal_data_model_get_subscriber_range (data_model, subscriber, &range_start, &range_end)) {
		e_cal_data_model_unsubscribe (data_model, subscriber);
		e_cal_model_remove_all_objects (model);

		if (is_tasks_or_memos)
			e_cal_data_model_subscribe (data_model, subscriber, range_start, range_end);
	}
}

/* Only helper function */
static void
cal_shell_content_switch_list_view (ECalShellContent *cal_shell_content,
				    ECalViewKind from_view_kind,
				    ECalViewKind to_view_kind)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalBaseShellSidebar *cal_base_shell_sidebar;
	ECalendar *date_navigator;
	ESourceSelector *source_selector;
	ECalModel *model;
	gchar *cal_filter;

	g_return_if_fail (from_view_kind != to_view_kind);

	if (to_view_kind != E_CAL_VIEW_KIND_LIST &&
	    to_view_kind != E_CAL_VIEW_KIND_YEAR &&
	    from_view_kind != E_CAL_VIEW_KIND_LIST &&
	    from_view_kind != E_CAL_VIEW_KIND_YEAR)
		return;

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	cal_base_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar);
	date_navigator = e_cal_base_shell_sidebar_get_date_navigator (cal_base_shell_sidebar);
	source_selector = e_cal_base_shell_sidebar_get_selector (cal_base_shell_sidebar);

	gtk_widget_set_visible (GTK_WIDGET (date_navigator), to_view_kind != E_CAL_VIEW_KIND_LIST && to_view_kind != E_CAL_VIEW_KIND_YEAR);
	e_source_selector_set_show_toggles (source_selector, to_view_kind != E_CAL_VIEW_KIND_LIST);

	if (to_view_kind == E_CAL_VIEW_KIND_LIST || from_view_kind == E_CAL_VIEW_KIND_LIST) {
		model = e_calendar_view_get_model (cal_shell_content->priv->views[from_view_kind]);
		cal_filter = e_cal_data_model_dup_filter (e_cal_model_get_data_model (model));
		if (cal_filter) {
			model = e_calendar_view_get_model (cal_shell_content->priv->views[to_view_kind]);
			e_cal_data_model_set_filter (e_cal_model_get_data_model (model), cal_filter);
			g_free (cal_filter);
		}
	}

	/* The list view is activated */
	if (to_view_kind == E_CAL_VIEW_KIND_LIST) {
		cal_shell_content_update_list_view (cal_shell_content);
	/* The list view is deactivated */
	} else if (from_view_kind == E_CAL_VIEW_KIND_LIST) {
		cal_shell_content_clear_all_in_list_view (cal_shell_content);
		e_cal_base_shell_sidebar_ensure_sources_open (cal_base_shell_sidebar);
	}
}

void
e_cal_shell_content_set_current_view_id (ECalShellContent *cal_shell_content,
					 ECalViewKind view_kind)
{
	EShellView *shell_view;
	time_t start_time = -1, end_time = -1;
	gint ii;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (view_kind >= E_CAL_VIEW_KIND_DAY && view_kind < E_CAL_VIEW_KIND_LAST);

	if (cal_shell_content->priv->current_view == view_kind)
		return;

	if (cal_shell_content->priv->current_view >= E_CAL_VIEW_KIND_DAY &&
	    cal_shell_content->priv->current_view < E_CAL_VIEW_KIND_LAST) {
		ECalendarView *cal_view = cal_shell_content->priv->views[cal_shell_content->priv->current_view];

		if (!e_calendar_view_get_selected_time_range (cal_view, &start_time, &end_time)) {
			start_time = -1;
			end_time = -1;
		}
	}

	cal_shell_content->priv->previous_selected_start_time = start_time;
	cal_shell_content->priv->previous_selected_end_time = end_time;

	for (ii = 0; ii < E_CAL_VIEW_KIND_LAST; ii++) {
		ECalendarView *cal_view = cal_shell_content->priv->views[ii];
		gboolean in_focus = ii == view_kind;
		gboolean focus_changed;

		if (!cal_view) {
			g_warn_if_reached ();
			continue;
		}

		focus_changed = (cal_view->in_focus ? 1 : 0) != (in_focus ? 1 : 0);

		cal_view->in_focus = in_focus;

		if (ii == E_CAL_VIEW_KIND_LIST)
			continue;

		if (focus_changed && in_focus) {
			/* Currently focused view changed. Any events within the common time
			   range are not shown in the newly focused view, thus make sure it'll
			   contain all what it should have. */
			ECalModel *model;

			model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));

			/* This may not cause any queries to backends with events,
			   because the time range should be always within the one
			   shown in the date picker. */
			cal_shell_content_resubscribe (cal_view, model);

			if (cal_shell_content->priv->task_table) {
				ETaskTable *task_table;

				task_table = E_TASK_TABLE (cal_shell_content->priv->task_table);
				cal_shell_content_resubscribe (cal_view, e_task_table_get_model (task_table));
			}

			if (cal_shell_content->priv->memo_table) {
				EMemoTable *memo_table;

				memo_table = E_MEMO_TABLE (cal_shell_content->priv->memo_table);
				cal_shell_content_resubscribe (cal_view, e_memo_table_get_model (memo_table));
			}
		}
	}

	cal_shell_content_switch_list_view (cal_shell_content, cal_shell_content->priv->current_view, view_kind);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));

	e_ui_action_set_visible (ACTION (CALENDAR_PREVIEW_MENU), view_kind == E_CAL_VIEW_KIND_YEAR);

	cal_shell_content->priv->current_view = view_kind;

	g_object_notify (G_OBJECT (cal_shell_content), "current-view-id");

	gtk_widget_queue_draw (GTK_WIDGET (cal_shell_content->priv->views[cal_shell_content->priv->current_view]));

	e_cal_shell_view_set_view_id_from_view_kind (E_CAL_SHELL_VIEW (shell_view), cal_shell_content->priv->current_view);
	e_shell_view_update_actions (shell_view);
	e_cal_shell_view_update_sidebar (E_CAL_SHELL_VIEW (shell_view));
}

ECalViewKind
e_cal_shell_content_get_current_view_id (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), E_CAL_VIEW_KIND_LAST);

	return cal_shell_content->priv->current_view;
}

ECalendarView *
e_cal_shell_content_get_calendar_view (ECalShellContent *cal_shell_content,
				       ECalViewKind view_kind)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);
	g_return_val_if_fail (view_kind >= E_CAL_VIEW_KIND_DAY && view_kind < E_CAL_VIEW_KIND_LAST, NULL);

	return cal_shell_content->priv->views[view_kind];
}

ECalendarView *
e_cal_shell_content_get_current_calendar_view (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return e_cal_shell_content_get_calendar_view (cal_shell_content,
		e_cal_shell_content_get_current_view_id (cal_shell_content));
}

void
e_cal_shell_content_save_state (ECalShellContent *cal_shell_content)
{
	ECalShellContentPrivate *priv;

	g_return_if_fail (cal_shell_content != NULL);
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	priv = cal_shell_content->priv;

	if (priv->task_table != NULL)
		cal_shell_content_save_table_state (
			E_SHELL_CONTENT (cal_shell_content),
			E_TABLE (priv->task_table));

	if (priv->memo_table != NULL)
		cal_shell_content_save_table_state (
			E_SHELL_CONTENT (cal_shell_content),
			E_TABLE (priv->memo_table));
}

void
e_cal_shell_content_get_current_range (ECalShellContent *cal_shell_content,
				       time_t *range_start,
				       time_t *range_end)
{
	ICalTimezone *zone;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (range_start != NULL);
	g_return_if_fail (range_end != NULL);

	zone = e_cal_data_model_get_timezone (e_cal_base_shell_content_get_data_model (
		E_CAL_BASE_SHELL_CONTENT (cal_shell_content)));

	*range_start = cal_comp_gdate_to_timet (&(cal_shell_content->priv->view_start), zone);
	*range_end = cal_comp_gdate_to_timet (&(cal_shell_content->priv->view_end), zone);
}

void
e_cal_shell_content_get_current_range_dates (ECalShellContent *cal_shell_content,
					     GDate *range_start,
					     GDate *range_end)
{
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (range_start != NULL);
	g_return_if_fail (range_end != NULL);

	*range_start = cal_shell_content->priv->view_start;
	*range_end = cal_shell_content->priv->view_end;
}

static void
cal_shell_content_move_view_range_relative (ECalShellContent *cal_shell_content,
					    gint direction)
{
	GDate start, end;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));
	g_return_if_fail (direction != 0);

	start = cal_shell_content->priv->view_start;
	end = cal_shell_content->priv->view_end;

	switch (cal_shell_content->priv->current_view) {
		case E_CAL_VIEW_KIND_DAY:
			if (direction > 0) {
				g_date_add_days (&start, direction);
				g_date_add_days (&end, direction);
			} else {
				g_date_subtract_days (&start, direction * -1);
				g_date_subtract_days (&end, direction * -1);
			}
			break;
		case E_CAL_VIEW_KIND_WORKWEEK:
		case E_CAL_VIEW_KIND_WEEK:
			if (direction > 0) {
				g_date_add_days (&start, direction * 7);
				g_date_add_days (&end, direction * 7);
			} else {
				g_date_subtract_days (&start, direction * -7);
				g_date_subtract_days (&end, direction * -7);
			}
			break;
		case E_CAL_VIEW_KIND_MONTH:
		case E_CAL_VIEW_KIND_LIST:
			if (g_date_get_day (&start) != 1) {
				g_date_add_months (&start, 1);
				g_date_set_day (&start, 1);
			}
			if (direction > 0)
				g_date_add_months (&start, direction);
			else
				g_date_subtract_months (&start, direction * -1);
			end = start;
			g_date_set_day (&end, g_date_get_days_in_month (g_date_get_month (&start), g_date_get_year (&start)));
			g_date_add_days (&end, 6);
			break;
		case E_CAL_VIEW_KIND_YEAR:
			if (direction > 0) {
				g_date_add_years (&start, direction);
				g_date_add_years (&end, direction);
			} else {
				g_date_subtract_years (&start, direction * -1);
				g_date_subtract_years (&end, direction * -1);
			}
			break;
		case E_CAL_VIEW_KIND_LAST:
			return;
	}

	e_cal_shell_content_change_view (cal_shell_content, cal_shell_content->priv->current_view, &start, &end, FALSE);
}

void
e_cal_shell_content_move_view_range (ECalShellContent *cal_shell_content,
				     ECalendarViewMoveType move_type,
				     time_t exact_date)
{
	ECalendar *calendar;
	ECalDataModel *data_model;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	ICalTime *tt;
	ICalTimezone *zone;
	GDate date;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_shell_content));
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	calendar = e_cal_base_shell_sidebar_get_date_navigator (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_return_if_fail (E_IS_CALENDAR (calendar));
	g_return_if_fail (e_calendar_get_item (calendar) != NULL);

	data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	zone = e_cal_data_model_get_timezone (data_model);

	switch (move_type) {
		case E_CALENDAR_VIEW_MOVE_PREVIOUS:
			cal_shell_content_move_view_range_relative (cal_shell_content, -1);
			break;
		case E_CALENDAR_VIEW_MOVE_NEXT:
			cal_shell_content_move_view_range_relative (cal_shell_content, +1);
			break;
		case E_CALENDAR_VIEW_MOVE_TO_TODAY:
			tt = i_cal_time_new_current_with_zone (zone);
			g_date_set_dmy (&date, i_cal_time_get_day (tt), i_cal_time_get_month (tt), i_cal_time_get_year (tt));
			if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_YEAR) {
				ECalendarView *cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
				time_t tmt;

				tmt = i_cal_time_as_timet (tt);
				e_calendar_view_set_selected_time_range (cal_view, tmt, tmt);
				cal_shell_content->priv->view_start = date;
				cal_shell_content->priv->view_end = date;
			}
			g_clear_object (&tt);
			/* one-day selection takes care of the view range move with left view kind */
			e_calendar_item_set_selection (e_calendar_get_item (calendar), &date, &date);
			break;
		case E_CALENDAR_VIEW_MOVE_TO_EXACT_DAY:
			time_to_gdate_with_zone (&date, exact_date, zone);
			if (cal_shell_content->priv->current_view == E_CAL_VIEW_KIND_YEAR) {
				ECalendarView *cal_view = e_cal_shell_content_get_current_calendar_view (cal_shell_content);
				e_calendar_view_set_selected_time_range (cal_view, exact_date, exact_date);
				cal_shell_content->priv->view_start = date;
				cal_shell_content->priv->view_end = date;
			} else {
				e_cal_shell_content_change_view (cal_shell_content, E_CAL_VIEW_KIND_DAY, &date, &date, FALSE);
			}
			break;
	}
}

static void
cal_shell_content_update_model_filter (ECalDataModel *data_model,
				       ECalModel *model,
				       const gchar *filter,
				       time_t range_start,
				       time_t range_end)
{
	time_t tmp_start, tmp_end;

	g_return_if_fail (E_IS_CAL_DATA_MODEL (data_model));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	e_cal_data_model_freeze_views_update (data_model);
	if (filter != NULL)
		e_cal_data_model_set_filter (data_model, filter);
	e_cal_model_set_time_range (model, range_start, range_end);

	if (!e_cal_data_model_get_subscriber_range (data_model, E_CAL_DATA_MODEL_SUBSCRIBER (model), &tmp_start, &tmp_end)) {
		e_cal_data_model_subscribe (data_model, E_CAL_DATA_MODEL_SUBSCRIBER (model), range_start, range_end);
	}

	e_cal_data_model_thaw_views_update (data_model);
}

void
e_cal_shell_content_update_tasks_filter (ECalShellContent *cal_shell_content,
					 const gchar *cal_filter)
{
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (cal_shell_content->priv->task_table) {
		ETaskTable *task_table;
		ECalDataModel *data_model;
		ECalModel *model;
		gchar *hide_completed_tasks_sexp;
		gboolean hide_cancelled_tasks;

		/* Set the query on the task pad. */

		task_table = E_TASK_TABLE (cal_shell_content->priv->task_table);
		model = e_task_table_get_model (task_table);
		data_model = e_cal_model_get_data_model (model);

		hide_completed_tasks_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE);
		hide_cancelled_tasks = calendar_config_get_hide_cancelled_tasks ();

		if (hide_completed_tasks_sexp != NULL) {
			if (cal_filter && *cal_filter) {
				gchar *filter;

				filter = g_strdup_printf ("(and %s %s%s%s)", hide_completed_tasks_sexp,
					hide_cancelled_tasks ? CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP : "",
					hide_cancelled_tasks ? " " : "",
					cal_filter);
				cal_shell_content_update_model_filter (data_model, model, filter, 0, 0);
				g_free (filter);
			} else if (hide_cancelled_tasks) {
				gchar *filter;

				filter = g_strdup_printf ("(and %s %s)", hide_completed_tasks_sexp,
					CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP);
				cal_shell_content_update_model_filter (data_model, model, filter, 0, 0);
				g_free (filter);
			} else {
				cal_shell_content_update_model_filter (data_model, model, hide_completed_tasks_sexp, 0, 0);
			}
		} else if (hide_cancelled_tasks) {
			if (cal_filter && *cal_filter) {
				gchar *filter;

				filter = g_strdup_printf ("(and %s %s)", CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP, cal_filter);
				cal_shell_content_update_model_filter (data_model, model, filter, 0, 0);
				g_free (filter);
			} else {
				cal_shell_content_update_model_filter (data_model, model, CALENDAR_CONFIG_NOT_CANCELLED_TASKS_SEXP, 0, 0);
			}
		} else {
			cal_shell_content_update_model_filter (data_model, model, (cal_filter && *cal_filter) ? cal_filter : "#t", 0, 0);
		}

		g_free (hide_completed_tasks_sexp);
	}
}

void
e_cal_shell_content_update_filters (ECalShellContent *cal_shell_content,
				    const gchar *cal_filter,
				    time_t start_range,
				    time_t end_range)
{
	ECalDataModel *data_model;
	ECalModel *model;

	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if (!cal_filter)
		return;

	if (e_cal_shell_content_get_current_view_id (cal_shell_content) == E_CAL_VIEW_KIND_LIST) {
		data_model = cal_shell_content->priv->list_view_data_model;
		model = cal_shell_content->priv->list_view_model;
		start_range = 0;
		end_range = 0;
	} else {
		data_model = e_cal_base_shell_content_get_data_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
		model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (cal_shell_content));
	}

	cal_shell_content_update_model_filter (data_model, model, cal_filter, start_range, end_range);
	e_cal_shell_content_update_tasks_filter (cal_shell_content, cal_filter);

	if (cal_shell_content->priv->memo_table) {
		EMemoTable *memo_table;

		/* Set the query on the memo pad. */

		memo_table = E_MEMO_TABLE (cal_shell_content->priv->memo_table);
		model = e_memo_table_get_model (memo_table);
		data_model = e_cal_model_get_data_model (model);

		if (start_range != 0 && end_range != 0) {
			ICalTimezone *zone;
			const gchar *default_tzloc = NULL;
			time_t end = end_range;
			gchar *filter;
			gchar *iso_start;
			gchar *iso_end;

			zone = e_cal_data_model_get_timezone (data_model);
			if (zone && zone != i_cal_timezone_get_utc_timezone ())
				default_tzloc = i_cal_timezone_get_location (zone);
			if (!default_tzloc)
				default_tzloc = "";

			if (start_range != (time_t) 0 && end_range != (time_t) 0) {
				end = time_day_end_with_zone (end_range, zone);
			}

			iso_start = isodate_from_time_t (start_range);
			iso_end = isodate_from_time_t (end);

			filter = g_strdup_printf (
				"(and (or (not (has-start?)) "
				"(occur-in-time-range? (make-time \"%s\") "
				"(make-time \"%s\") \"%s\")) %s)",
				iso_start, iso_end, default_tzloc, cal_filter);

			cal_shell_content_update_model_filter (data_model, model, filter, 0, 0);

			g_free (filter);
			g_free (iso_start);
			g_free (iso_end);
		} else {
			cal_shell_content_update_model_filter (data_model, model, *cal_filter ? cal_filter : "#t", 0, 0);
		}
	}
}

ECalDataModel *
e_cal_shell_content_get_list_view_data_model (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), NULL);

	return cal_shell_content->priv->list_view_data_model;
}

void
e_cal_shell_content_set_show_tag_vpane (ECalShellContent *cal_shell_content,
					gboolean show)
{
	g_return_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content));

	if ((gtk_widget_get_visible (cal_shell_content->priv->vpaned) ? 1 : 0) == (show ? 1 : 0))
		return;

	gtk_widget_set_visible (cal_shell_content->priv->vpaned, show);

	if (show) {
		if (cal_shell_content->priv->task_data_model)
			e_cal_data_model_thaw_views_update (cal_shell_content->priv->task_data_model);

		if (cal_shell_content->priv->memo_data_model)
			e_cal_data_model_thaw_views_update (cal_shell_content->priv->memo_data_model);
	} else {
		if (cal_shell_content->priv->task_data_model)
			e_cal_data_model_freeze_views_update (cal_shell_content->priv->task_data_model);

		if (cal_shell_content->priv->memo_data_model)
			e_cal_data_model_freeze_views_update (cal_shell_content->priv->memo_data_model);
	}

	g_object_notify (G_OBJECT (cal_shell_content), "show-tag-vpane");
}

gboolean
e_cal_shell_content_get_show_tag_vpane (ECalShellContent *cal_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_SHELL_CONTENT (cal_shell_content), FALSE);

	return gtk_widget_get_visible (cal_shell_content->priv->vpaned);
}

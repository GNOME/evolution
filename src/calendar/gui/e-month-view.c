/*
 * e-month-view.c
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

#include "e-month-view.h"

struct _EMonthViewPrivate {
	gint placeholder;
};

G_DEFINE_TYPE_WITH_PRIVATE (EMonthView, e_month_view, E_TYPE_WEEK_VIEW)

static void
month_view_cursor_key_up (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	if (week_view->selection_start_day < 7) {
		/* No easy way to calculate new selection_start_day, so
		 * calculate a time_t value and set_selected_time_range. */
		time_t current;

		if (e_calendar_view_get_selected_time_range (
			E_CALENDAR_VIEW (week_view), &current, NULL)) {

			current = time_add_week (current, -1);
			e_week_view_scroll_a_step (
				week_view, E_CAL_VIEW_MOVE_PAGE_UP);
			e_week_view_set_selected_time_range_visible (
				week_view, current, current);
		}
	} else {
		week_view->selection_start_day -= 7;
		week_view->selection_end_day = week_view->selection_start_day;
	}

	g_signal_emit_by_name (week_view, "selected-time-changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
month_view_cursor_key_down (EWeekView *week_view)
{
	gint weeks_shown;

	if (week_view->selection_start_day == -1)
		return;

	weeks_shown = e_week_view_get_weeks_shown (week_view);

	if (week_view->selection_start_day >= (weeks_shown - 1) * 7) {
		/* No easy way to calculate new selection_start_day, so
		 * calculate a time_t value and set_selected_time_range. */
		time_t current;

		if (e_calendar_view_get_selected_time_range (
			E_CALENDAR_VIEW (week_view), &current, NULL)) {

			current = time_add_week (current, 1);
			e_week_view_scroll_a_step (
				week_view, E_CAL_VIEW_MOVE_PAGE_DOWN);
			e_week_view_set_selected_time_range_visible (
				week_view, current, current);
		}
	} else {
		week_view->selection_start_day += 7;
		week_view->selection_end_day = week_view->selection_start_day;
	}

	g_signal_emit_by_name (week_view, "selected-time-changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
month_view_cursor_key_left (EWeekView *week_view)
{
	if (week_view->selection_start_day == -1)
		return;

	if (week_view->selection_start_day == 0) {
		/* No easy way to calculate new selection_start_day, so
		 * calculate a time_t value and set_selected_time_range. */
		time_t current;

		if (e_calendar_view_get_selected_time_range (
			E_CALENDAR_VIEW (week_view), &current, NULL)) {

			current = time_add_day (current, -1);
			e_week_view_scroll_a_step (
				week_view, E_CAL_VIEW_MOVE_PAGE_UP);
			e_week_view_set_selected_time_range_visible (
				week_view, current, current);
		}
	} else {
		week_view->selection_start_day--;
		week_view->selection_end_day = week_view->selection_start_day;
	}

	g_signal_emit_by_name (week_view, "selected-time-changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
month_view_cursor_key_right (EWeekView *week_view)
{
	gint weeks_shown;

	if (week_view->selection_start_day == -1)
		return;

	weeks_shown = e_week_view_get_weeks_shown (week_view);

	if (week_view->selection_start_day == weeks_shown * 7 - 1) {
		/* No easy way to calculate new selection_start_day, so
		 * calculate a time_t value and set_selected_time_range. */
		time_t current;

		if (e_calendar_view_get_selected_time_range (
			E_CALENDAR_VIEW (week_view), &current, NULL)) {

			current = time_add_day (current, 1);
			e_week_view_scroll_a_step (
				week_view, E_CAL_VIEW_MOVE_PAGE_DOWN);
			e_week_view_set_selected_time_range_visible (
				week_view, current, current);
		}
	} else {
		week_view->selection_start_day++;
		week_view->selection_end_day = week_view->selection_start_day;
	}

	g_signal_emit_by_name (week_view, "selected-time-changed");
	gtk_widget_queue_draw (week_view->main_canvas);
}

static void
e_month_view_class_init (EMonthViewClass *class)
{
	EWeekViewClass *week_view_class;

	week_view_class = E_WEEK_VIEW_CLASS (class);
	week_view_class->cursor_key_up = month_view_cursor_key_up;
	week_view_class->cursor_key_down = month_view_cursor_key_down;
	week_view_class->cursor_key_left = month_view_cursor_key_left;
	week_view_class->cursor_key_right = month_view_cursor_key_right;
}

static void
e_month_view_init (EMonthView *month_view)
{
	month_view->priv = e_month_view_get_instance_private (month_view);
}

ECalendarView *
e_month_view_new (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (E_TYPE_MONTH_VIEW, "model", model, NULL);
}

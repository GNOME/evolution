/*
 * e-month-view.c
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

#include "e-month-view.h"

#include <libecal/e-cal-time-util.h>

#define E_MONTH_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MONTH_VIEW, EMonthViewPrivate))

struct _EMonthViewPrivate {
	gint placeholder;
};

static gpointer parent_class;

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

			current = time_add_week (current, -1);
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
month_view_class_init (EMonthViewClass *class)
{
	EWeekViewClass *week_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMonthViewPrivate));

	week_view_class = E_WEEK_VIEW_CLASS (class);
	week_view_class->cursor_key_up = month_view_cursor_key_up;
	week_view_class->cursor_key_down = month_view_cursor_key_down;
	week_view_class->cursor_key_left = month_view_cursor_key_left;
	week_view_class->cursor_key_right = month_view_cursor_key_right;
}

static void
month_view_init (EMonthView *month_view)
{
	month_view->priv = E_MONTH_VIEW_GET_PRIVATE (month_view);
}

GType
e_month_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMonthViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) month_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMonthView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) month_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_WEEK_VIEW, "EMonthView", &type_info, 0);
	}

	return type;
}

ECalendarView *
e_month_view_new (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return g_object_new (E_TYPE_MONTH_VIEW, "model", model, NULL);
}

/*
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
 * Authors:
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ea-calendar-helpers.h"
#include "ea-cal-view-event.h"
#include "ea-jump-button.h"
#include "e-day-view.h"
#include "e-week-view.h"

#include <libgnomecanvas/libgnomecanvas.h>

/**
 * ea_calendar_helpers_get_accessible_for
 * @canvas_item: the canvas item for a event or a jump button
 *
 * Returns: the atk object for the canvas_item
 **/
AtkObject *
ea_calendar_helpers_get_accessible_for (GnomeCanvasItem *canvas_item)
{
	AtkObject *atk_obj = NULL;
	GObject *g_obj;

	g_return_val_if_fail ((E_IS_TEXT (canvas_item)) ||
		(GNOME_IS_CANVAS_ITEM (canvas_item)), NULL);

	g_obj = G_OBJECT (canvas_item);
	/* we cannot use atk_gobject_accessible_for_object here,
	 * EaDayViewEvent/EaWeekViewEvent cannot be created by the
	 * registered facotry of E_TEXT
	 */
	atk_obj = g_object_get_data (g_obj, "accessible-object");
	if (!atk_obj) {
		if (E_IS_TEXT (canvas_item)) {
		atk_obj = ea_cal_view_event_new (g_obj);
		}
		else if (GNOME_IS_CANVAS_PIXBUF (canvas_item)) {
			atk_obj = ea_jump_button_new (g_obj);
		}
		else
			return NULL;
	}
	return atk_obj;
}

/**
 * ea_calendar_helpers_get_view_widget_from:
 * @canvas_item: the canvas item for a event or a jump button
 *
 * Get the cal view widget contains the canvas_item.
 *
 * Returns: the cal view widget if exists
 **/
ECalendarView *
ea_calendar_helpers_get_cal_view_from (GnomeCanvasItem *canvas_item)
{
	GnomeCanvas *canvas;
	GtkWidget *view_widget = NULL;

	g_return_val_if_fail (canvas_item, NULL);
	g_return_val_if_fail ((E_IS_TEXT (canvas_item)) ||
		(GNOME_IS_CANVAS_ITEM (canvas_item)), NULL);

	/* canvas_item is the e_text for the event */
	/* canvas_item->canvas is the ECanvas for day view */
	/* parent of canvas_item->canvas is the EDayView or EWeekView widget */
	canvas = canvas_item->canvas;
	view_widget = gtk_widget_get_parent (GTK_WIDGET (canvas));

	if (view_widget && GTK_IS_BOX (view_widget))
		view_widget = gtk_widget_get_parent (view_widget);

	if (!view_widget || !E_IS_CALENDAR_VIEW (view_widget))
		return NULL;

	return E_CALENDAR_VIEW (view_widget);
}

/**
 * ea_calendar_helpers_get_cal_view_event_from
 * @canvas_item: the cavas_item (e_text) for the event
 *
 * Get the ECalendarViewEvent for the canvas_item.
 *
 * Returns: the ECalendarViewEvent
 **/
ECalendarViewEvent *
ea_calendar_helpers_get_cal_view_event_from (GnomeCanvasItem *canvas_item)
{
	ECalendarView *cal_view;
	gboolean event_found;
	ECalendarViewEvent *cal_view_event;

	g_return_val_if_fail (E_IS_TEXT (canvas_item), NULL);

	cal_view = ea_calendar_helpers_get_cal_view_from (canvas_item);

	if (!cal_view)
		return NULL;

	if (E_IS_DAY_VIEW (cal_view)) {
		gint event_day, event_num;
		EDayViewEvent *day_view_event;
		EDayView *day_view = E_DAY_VIEW (cal_view);
		event_found = e_day_view_find_event_from_item (
			day_view, canvas_item,
			&event_day, &event_num);
		if (!event_found)
			return NULL;
		if (event_day == E_DAY_VIEW_LONG_EVENT) {
			/* a long event */
			day_view_event = &g_array_index (day_view->long_events,
							 EDayViewEvent, event_num);
		}
		else {
			/* a main canvas event */
			day_view_event = &g_array_index (day_view->events[event_day],
							 EDayViewEvent, event_num);
		}
		cal_view_event = (ECalendarViewEvent *) day_view_event;
	}
	else if (E_IS_WEEK_VIEW (cal_view)) {
		gint event_num, span_num;
		EWeekViewEvent *week_view_event;
		EWeekView *week_view = E_WEEK_VIEW (cal_view);
		event_found = e_week_view_find_event_from_item (
			week_view,
			canvas_item,
			&event_num,
			&span_num);
		if (!event_found)
			return NULL;

		week_view_event = &g_array_index (
			week_view->events, EWeekViewEvent, event_num);

		cal_view_event = (ECalendarViewEvent *) week_view_event;
	}
	else {
		g_return_val_if_reached (NULL);
	}
	return cal_view_event;
}

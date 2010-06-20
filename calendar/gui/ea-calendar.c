/*
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
 * Authors:
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <text/e-text.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>
#include "ea-calendar-helpers.h"
#include "a11y/ea-factory.h"
#include "ea-calendar.h"

#include "calendar/gui/ea-cal-view.h"
#include "calendar/gui/ea-cal-view-event.h"
#include "calendar/gui/ea-day-view.h"
#include "calendar/gui/ea-day-view-main-item.h"
#include "calendar/gui/ea-week-view.h"
#include "calendar/gui/ea-week-view-main-item.h"
#include "calendar/gui/ea-gnome-calendar.h"

EA_FACTORY (EA_TYPE_CAL_VIEW, ea_cal_view, ea_cal_view_new)
EA_FACTORY (EA_TYPE_DAY_VIEW, ea_day_view, ea_day_view_new)
EA_FACTORY_GOBJECT (
	EA_TYPE_DAY_VIEW_MAIN_ITEM,
	ea_day_view_main_item, ea_day_view_main_item_new)
EA_FACTORY (EA_TYPE_WEEK_VIEW, ea_week_view, ea_week_view_new)
EA_FACTORY_GOBJECT (
	EA_TYPE_WEEK_VIEW_MAIN_ITEM,
	ea_week_view_main_item, ea_week_view_main_item_new)
EA_FACTORY (EA_TYPE_GNOME_CALENDAR, ea_gnome_calendar, ea_gnome_calendar_new)

static gboolean ea_calendar_focus_watcher (GSignalInvocationHint *ihint,
					   guint n_param_values,
					   const GValue *param_values,
					   gpointer data);

static gpointer e_text_type, pixbuf_type, e_day_view_type, e_week_view_type;
static gpointer e_day_view_main_item_type, e_week_view_main_item_type;

void
gnome_calendar_a11y_init (void)
{
	/* we only add focus watcher when accessibility is enabled
	 */
	if (atk_get_root ()) {
		EA_SET_FACTORY (gnome_calendar_get_type(), ea_gnome_calendar);

		/* force loading some types */
		e_text_type = g_type_class_ref (E_TYPE_TEXT);
		pixbuf_type = g_type_class_ref (GNOME_TYPE_CANVAS_PIXBUF);
		e_day_view_type = g_type_class_ref (e_day_view_get_type ());
		e_week_view_type = g_type_class_ref (E_TYPE_WEEK_VIEW);
		e_day_view_main_item_type = g_type_class_ref (e_day_view_main_item_get_type ());
		e_week_view_main_item_type = g_type_class_ref (e_week_view_main_item_get_type ());

		g_signal_add_emission_hook (g_signal_lookup ("event", E_TYPE_TEXT),
					    0, ea_calendar_focus_watcher,
					    NULL, (GDestroyNotify) NULL);
		g_signal_add_emission_hook (g_signal_lookup ("event", GNOME_TYPE_CANVAS_PIXBUF),
                                            0, ea_calendar_focus_watcher,
                                            NULL, (GDestroyNotify) NULL);
		g_signal_add_emission_hook (g_signal_lookup ("event-after",
							     e_day_view_get_type()),
					    0, ea_calendar_focus_watcher,
					    NULL, (GDestroyNotify) NULL);
		g_signal_add_emission_hook (g_signal_lookup ("event",
							     e_day_view_main_item_get_type()),
					    0, ea_calendar_focus_watcher,
					    NULL, (GDestroyNotify) NULL);
		g_signal_add_emission_hook (g_signal_lookup ("event-after",
							     E_TYPE_WEEK_VIEW),
					    0, ea_calendar_focus_watcher,
					    NULL, (GDestroyNotify) NULL);
		g_signal_add_emission_hook (g_signal_lookup ("event",
							     e_week_view_main_item_get_type()),
					    0, ea_calendar_focus_watcher,
					    NULL, (GDestroyNotify) NULL);

	}
}

void
e_cal_view_a11y_init (void)
{
	EA_SET_FACTORY (e_calendar_view_get_type(), ea_cal_view);
}

void
e_day_view_a11y_init (void)
{
	EA_SET_FACTORY (e_day_view_get_type(), ea_day_view);
}

void
e_day_view_main_item_a11y_init (void)
{
	EA_SET_FACTORY (e_day_view_main_item_get_type (), ea_day_view_main_item);
}

void
e_week_view_a11y_init (void)
{
	EA_SET_FACTORY (E_TYPE_WEEK_VIEW, ea_week_view);
}

void
e_week_view_main_item_a11y_init (void)
{
	EA_SET_FACTORY (e_week_view_main_item_get_type (), ea_week_view_main_item);
}

static gboolean
ea_calendar_focus_watcher (GSignalInvocationHint *ihint,
                           guint n_param_values,
                           const GValue *param_values,
                           gpointer data)
{
	GObject *object;
	GdkEvent *event;
        AtkObject *ea_event = NULL;

	object = g_value_get_object (param_values + 0);
	event = g_value_get_boxed (param_values + 1);

	if ((E_IS_TEXT (object)) || (GNOME_IS_CANVAS_PIXBUF (object))) {
		/* "event" signal on canvas item
		 */
		GnomeCanvasItem *canvas_item;

		canvas_item = GNOME_CANVAS_ITEM (object);
		if (event->type == GDK_FOCUS_CHANGE) {
			if (event->focus_change.in) {
				ea_event =
					ea_calendar_helpers_get_accessible_for (canvas_item);
				if (!ea_event)
					/* not canvas item we want */
					return TRUE;

			}
			atk_focus_tracker_notify (ea_event);
		}
	}
	else if (E_IS_DAY_VIEW (object)) {
		EDayView *day_view = E_DAY_VIEW (object);
		if (event->type == GDK_FOCUS_CHANGE) {
			if (event->focus_change.in) {
				/* give main item chance to emit focus */
				gnome_canvas_item_grab_focus (day_view->main_canvas_item);
			}
		}
	}
	else if (E_IS_DAY_VIEW_MAIN_ITEM (object)) {
		if (event->type == GDK_FOCUS_CHANGE) {
			if (event->focus_change.in) {
				/* we should emit focus on main item */
				ea_event = atk_gobject_accessible_for_object (object);
			}
			else
				/* focus out */
				ea_event = NULL;
#ifdef ACC_DEBUG
			printf ("EvoAcc: focus notify on day main item %p\n", (gpointer)object);
#endif
			atk_focus_tracker_notify (ea_event);
		}
	} else if (E_IS_WEEK_VIEW (object)) {
		EWeekView *week_view = E_WEEK_VIEW (object);
		if (event->type == GDK_FOCUS_CHANGE) {
			if (event->focus_change.in) {
				/* give main item chance to emit focus */
				gnome_canvas_item_grab_focus (week_view->main_canvas_item);
			}
		}
	}
	else if (E_IS_WEEK_VIEW_MAIN_ITEM (object)) {
		if (event->type == GDK_FOCUS_CHANGE) {
			if (event->focus_change.in) {
				/* we should emit focus on main item */
				ea_event = atk_gobject_accessible_for_object (object);
			}
			else
				/* focus out */
				ea_event = NULL;
#ifdef ACC_DEBUG
			printf ("EvoAcc: focus notify on week main item %p\n", (gpointer)object);
#endif
			atk_focus_tracker_notify (ea_event);
		}
	}
	return TRUE;
}

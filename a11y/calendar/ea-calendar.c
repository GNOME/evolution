/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-calendar.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include <gal/e-text/e-text.h>
#include "ea-calendar-helpers.h"
#include "ea-factory.h"
#include "ea-calendar.h"

#include "calendar/ea-cal-view.h"
#include "calendar/ea-cal-view-event.h"
#include "calendar/ea-day-view.h"
#include "calendar/ea-week-view.h"
#include "calendar/ea-gnome-calendar.h"


EA_FACTORY (EA_TYPE_CAL_VIEW, ea_cal_view, ea_cal_view_new);
EA_FACTORY (EA_TYPE_DAY_VIEW, ea_day_view, ea_day_view_new);
EA_FACTORY (EA_TYPE_WEEK_VIEW, ea_week_view, ea_week_view_new);
EA_FACTORY (EA_TYPE_GNOME_CALENDAR, ea_gnome_calendar, ea_gnome_calendar_new);

static gboolean ea_calendar_focus_watcher (GSignalInvocationHint *ihint,
                                           guint n_param_values,
                                           const GValue *param_values,
                                           gpointer data);

void
gnome_calendar_a11y_init (void)
{
    EA_SET_FACTORY (gnome_calendar_get_type(), ea_gnome_calendar);
    /* we only add focus watcher when accessibility is enabled
     */
    if (atk_get_root ())
	    g_signal_add_emission_hook (g_signal_lookup ("event", E_TYPE_TEXT),
					0, ea_calendar_focus_watcher,
					NULL, (GDestroyNotify) NULL);
}

void
e_cal_view_a11y_init (void)
{
    EA_SET_FACTORY (e_cal_view_get_type(), ea_cal_view);
    /* we only add focus watcher when accessibility is enabled
     */
#if 0
    if (atk_get_root ())
	    g_signal_add_emission_hook (g_signal_lookup ("selection_time_changed",
							 e_cal_view_get_type ()),
					0, ea_calendar_focus_watcher,
					NULL, (GDestroyNotify) NULL);
#endif
}

void
e_day_view_a11y_init (void)
{
    EA_SET_FACTORY (e_day_view_get_type(), ea_day_view);
}

void
e_week_view_a11y_init (void)
{
    EA_SET_FACTORY (e_week_view_get_type(), ea_week_view);
}

gboolean
ea_calendar_focus_watcher (GSignalInvocationHint *ihint,
                           guint n_param_values,
                           const GValue *param_values,
                           gpointer data)
{
    GObject *object;
    GdkEvent *event;

    object = g_value_get_object (param_values + 0);
    event = g_value_get_boxed (param_values + 1);

    if (E_IS_TEXT (object)) {
        /* "event" signal on canvas item
         */
        GnomeCanvasItem *canvas_item;
        AtkObject *ea_event;

        canvas_item = GNOME_CANVAS_ITEM (object);
        if (event->type == GDK_FOCUS_CHANGE) {
            if (event->focus_change.in)
                ea_event =
                    ea_calendar_helpers_get_accessible_for (canvas_item);
            else
                /* focus out */
                ea_event = NULL;
            atk_focus_tracker_notify (ea_event);

        }
    }
#if 0
    else if (E_IS_DAY_VIEW (object)) {
        /* "selection_time_changed" signal on day_view
         */
        if (ATK_IS_SELECTION (object)) {
            AtkSelection *atk_selection;
            AtkObject *atk_obj;
            atk_selection = ATK_SELECTION (object);
            atk_obj = atk_selection_ref_selection (atk_selection, 0);

#ifdef ACC_DEBUG
            printf ("EvoAcc: ref a selection %p\n", atk_selection);
#endif
            atk_focus_tracker_notify (atk_obj);
        }
    }
#endif
    return TRUE;
}

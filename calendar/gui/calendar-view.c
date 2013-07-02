/*
 * Evolution calendar - Generic view object for calendar views
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
 * Authors:
 * Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "calendar-view.h"

#define CALENDAR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_CALENDAR_VIEW, CalendarViewPrivate))

struct _CalendarViewPrivate {
	/* Type of the view */
	GnomeCalendarViewType view_type;
};

static const gchar *calendar_view_get_type_code (GalView *view);
static GalView *calendar_view_clone (GalView *view);

G_DEFINE_TYPE (CalendarView, calendar_view, GAL_TYPE_VIEW)

/* Class initialization function for the calendar view */
static void
calendar_view_class_init (CalendarViewClass *class)
{
	GalViewClass *gal_view_class;

	g_type_class_add_private (class, sizeof (CalendarViewPrivate));

	gal_view_class = (GalViewClass *) class;

	gal_view_class->get_type_code = calendar_view_get_type_code;
	gal_view_class->clone = calendar_view_clone;
}

/* Object initialization function for the calendar view */
static void
calendar_view_init (CalendarView *cal_view)
{
	cal_view->priv = CALENDAR_VIEW_GET_PRIVATE (cal_view);
}

/* get_type_code method for the calendar view */
static const gchar *
calendar_view_get_type_code (GalView *view)
{
	CalendarView *cal_view;
	CalendarViewPrivate *priv;

	cal_view = CALENDAR_VIEW (view);
	priv = cal_view->priv;

	switch (priv->view_type) {
	case GNOME_CAL_DAY_VIEW:
		return "day_view";

	case GNOME_CAL_WORK_WEEK_VIEW:
		return "work_week_view";

	case GNOME_CAL_WEEK_VIEW:
		return "week_view";

	case GNOME_CAL_MONTH_VIEW:
		return "month_view";

	default:
		g_return_val_if_reached (NULL);
	}
}

/* clone method for the calendar view */
static GalView *
calendar_view_clone (GalView *view)
{
	CalendarView *cal_view;
	GalView *clone;

	/* Chain up to parent's clone() method. */
	clone = GAL_VIEW_CLASS (calendar_view_parent_class)->clone (view);

	cal_view = CALENDAR_VIEW (view);
	CALENDAR_VIEW (clone)->priv->view_type = cal_view->priv->view_type;

	return clone;
}

/**
 * calendar_view_new:
 * @view_type: The type of calendar view that this object will represent.
 * @title: Title for the view.
 *
 * Creates a new calendar view object.
 *
 * Return value: A newly-created calendar view.
 **/
CalendarView *
calendar_view_new (GnomeCalendarViewType view_type,
                   const gchar *title)
{
	CalendarView *cal_view;

	cal_view = g_object_new (TYPE_CALENDAR_VIEW, "title", title, NULL);

	cal_view->priv->view_type = view_type;

	return cal_view;
}

/**
 * calendar_view_get_view_type:
 * @cal_view: A calendar view.
 *
 * Queries the calendar view type of a calendar view.
 *
 * Return value: Type of calendar view.
 **/
GnomeCalendarViewType
calendar_view_get_view_type (CalendarView *cal_view)
{
	CalendarViewPrivate *priv;

	g_return_val_if_fail (cal_view != NULL, GNOME_CAL_DAY_VIEW);
	g_return_val_if_fail (IS_CALENDAR_VIEW (cal_view), GNOME_CAL_DAY_VIEW);

	priv = cal_view->priv;
	return priv->view_type;
}

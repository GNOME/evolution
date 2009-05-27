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



/* Private part of the CalendarView structure */
struct _CalendarViewPrivate {
	/* Type of the view */
	GnomeCalendarViewType view_type;

	/* Title of the view */
	gchar *title;
};



static void calendar_view_finalize (GObject *object);

static void calendar_view_load (GalView *view, const gchar *filename);
static void calendar_view_save (GalView *view, const gchar *filename);
static const gchar *calendar_view_get_title (GalView *view);
static void calendar_view_set_title (GalView *view, const gchar *title);
static const gchar *calendar_view_get_type_code (GalView *view);
static GalView *calendar_view_clone (GalView *view);

G_DEFINE_TYPE (CalendarView, calendar_view, GAL_VIEW_TYPE)

/* Class initialization function for the calendar view */
static void
calendar_view_class_init (CalendarViewClass *class)
{
	GalViewClass *gal_view_class;
	GObjectClass *object_class;

	gal_view_class = (GalViewClass *) class;
	object_class = (GObjectClass *) class;

	gal_view_class->edit = NULL;
	gal_view_class->load = calendar_view_load;
	gal_view_class->save = calendar_view_save;
	gal_view_class->get_title = calendar_view_get_title;
	gal_view_class->set_title = calendar_view_set_title;
	gal_view_class->get_type_code = calendar_view_get_type_code;
	gal_view_class->clone = calendar_view_clone;

	object_class->finalize = calendar_view_finalize;
}

/* Object initialization function for the calendar view */
static void
calendar_view_init (CalendarView *cal_view)
{
	CalendarViewPrivate *priv;

	priv = g_new0 (CalendarViewPrivate, 1);
	cal_view->priv = priv;

	priv->title = NULL;
}

/* Destroy method for the calendar view */
static void
calendar_view_finalize (GObject *object)
{
	CalendarView *cal_view;
	CalendarViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CALENDAR_VIEW (object));

	cal_view = CALENDAR_VIEW (object);
	priv = cal_view->priv;

	if (priv->title) {
		g_free (priv->title);
		priv->title = NULL;
	}

	g_free (priv);
	cal_view->priv = NULL;

	if (G_OBJECT_CLASS (calendar_view_parent_class)->finalize)
		(* G_OBJECT_CLASS (calendar_view_parent_class)->finalize) (object);
}



/* load method of the calendar view */
static void
calendar_view_load (GalView *view, const gchar *filename)
{
	/* nothing */
}

/* save method of the calendar view */
static void
calendar_view_save (GalView *view, const gchar *filename)
{
	/* nothing */
}

/* get_title method of the calendar view */
static const gchar *
calendar_view_get_title (GalView *view)
{
	CalendarView *cal_view;
	CalendarViewPrivate *priv;

	cal_view = CALENDAR_VIEW (view);
	priv = cal_view->priv;

	return (const gchar *) priv->title;
}

/* set_title method of the calendar view */
static void
calendar_view_set_title (GalView *view, const gchar *title)
{
	CalendarView *cal_view;
	CalendarViewPrivate *priv;

	cal_view = CALENDAR_VIEW (view);
	priv = cal_view->priv;

	if (priv->title)
		g_free (priv->title);

	priv->title = g_strdup (title);
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
	CalendarViewPrivate *priv;
	CalendarView *new_view;
	CalendarViewPrivate *new_priv;

	cal_view = CALENDAR_VIEW (view);
	priv = cal_view->priv;

	new_view = g_object_new (TYPE_CALENDAR_VIEW, NULL);
	new_priv = new_view->priv;

	new_priv->view_type = priv->view_type;
	new_priv->title = g_strdup (priv->title);

	return GAL_VIEW (new_view);
}



/**
 * calendar_view_construct:
 * @cal_view: A calendar view.
 * @view_type: The type of calendar view that this object will represent.
 * @title: Title for the view.
 *
 * Constructs a calendar view by setting its view type and title.
 *
 * Return value: The same value as @cal_view.
 **/
CalendarView *
calendar_view_construct (CalendarView *cal_view,
			 GnomeCalendarViewType view_type,
			 const gchar *title)
{
	CalendarViewPrivate *priv;

	g_return_val_if_fail (cal_view != NULL, NULL);
	g_return_val_if_fail (IS_CALENDAR_VIEW (cal_view), NULL);
	g_return_val_if_fail (title != NULL, NULL);

	priv = cal_view->priv;

	priv->view_type = view_type;
	priv->title = g_strdup (title);

	return cal_view;
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
calendar_view_new (GnomeCalendarViewType view_type, const gchar *title)
{
	CalendarView *cal_view;

	cal_view = g_object_new (TYPE_CALENDAR_VIEW, NULL);
	return calendar_view_construct (cal_view, view_type, title);
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

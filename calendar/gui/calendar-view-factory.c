/* Evolution calendar - Generic view factory for calendar views
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include "calendar-view-factory.h"
#include "calendar-view.h"



/* Private part of the CalendarViewFactory structure */
struct _CalendarViewFactoryPrivate {
	/* Type of views created by this factory */
	GnomeCalendarViewType view_type;
};



static void calendar_view_factory_finalize (GObject *object);

static const char *calendar_view_factory_get_title (GalViewFactory *factory);
static const char *calendar_view_factory_get_type_code (GalViewFactory *factory);
static GalView *calendar_view_factory_new_view (GalViewFactory *factory, const char *name);

G_DEFINE_TYPE (CalendarViewFactory, calendar_view_factory, GAL_VIEW_FACTORY_TYPE);

/* Class initialization function for the calendar view factory */
static void
calendar_view_factory_class_init (CalendarViewFactoryClass *class)
{
	GalViewFactoryClass *gal_view_factory_class;
	GObjectClass *gobject_class;

	gal_view_factory_class = (GalViewFactoryClass *) class;
	gobject_class = (GObjectClass *) class;

	gal_view_factory_class->get_title = calendar_view_factory_get_title;
	gal_view_factory_class->get_type_code = calendar_view_factory_get_type_code;
	gal_view_factory_class->new_view = calendar_view_factory_new_view;

	gobject_class->finalize = calendar_view_factory_finalize;
}

/* Object initialization class for the calendar view factory */
static void
calendar_view_factory_init (CalendarViewFactory *cal_view_factory)
{
	CalendarViewFactoryPrivate *priv;

	priv = g_new0 (CalendarViewFactoryPrivate, 1);
	cal_view_factory->priv = priv;
}

/* Finalize method for the calendar view factory */
static void
calendar_view_factory_finalize (GObject *object)
{
	CalendarViewFactory *cal_view_factory;
	CalendarViewFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CALENDAR_VIEW_FACTORY (object));

	cal_view_factory = CALENDAR_VIEW_FACTORY (object);
	priv = cal_view_factory->priv;

	g_free (priv);
	cal_view_factory->priv = NULL;

	if (G_OBJECT_CLASS (calendar_view_factory_parent_class)->finalize)
		(* G_OBJECT_CLASS (calendar_view_factory_parent_class)->finalize) (object);
}



/* get_title method for the calendar view factory */
static const char *
calendar_view_factory_get_title (GalViewFactory *factory)
{
	CalendarViewFactory *cal_view_factory;
	CalendarViewFactoryPrivate *priv;

	cal_view_factory = CALENDAR_VIEW_FACTORY (factory);
	priv = cal_view_factory->priv;

	switch (priv->view_type) {
	case GNOME_CAL_DAY_VIEW:
		return _("Day View");

	case GNOME_CAL_WORK_WEEK_VIEW:
		return _("Work Week View");

	case GNOME_CAL_WEEK_VIEW:
		return _("Week View");

	case GNOME_CAL_MONTH_VIEW:
		return _("Month View");

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* get_type_code method for the calendar view factory */
static const char *
calendar_view_factory_get_type_code (GalViewFactory *factory)
{
	CalendarViewFactory *cal_view_factory;
	CalendarViewFactoryPrivate *priv;

	cal_view_factory = CALENDAR_VIEW_FACTORY (factory);
	priv = cal_view_factory->priv;

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
		g_assert_not_reached ();
		return NULL;
	}
}

/* new_view method for the calendar view factory */
static GalView *
calendar_view_factory_new_view (GalViewFactory *factory, const char *name)
{
	CalendarViewFactory *cal_view_factory;
	CalendarViewFactoryPrivate *priv;
	CalendarView *cal_view;

	cal_view_factory = CALENDAR_VIEW_FACTORY (factory);
	priv = cal_view_factory->priv;

	cal_view = calendar_view_new (priv->view_type, name);
	return GAL_VIEW (cal_view);
}



/**
 * calendar_view_factory_construct:
 * @cal_view_factory: A calendar view factory.
 * @view_type: Type of calendar views that the factory will create.
 * 
 * Constructs a calendar view factory by setting the type of views it will
 * create.
 * 
 * Return value: The same value as @cal_view_factory.
 **/
CalendarViewFactory *
calendar_view_factory_construct (CalendarViewFactory *cal_view_factory,
				 GnomeCalendarViewType view_type)
{
	CalendarViewFactoryPrivate *priv;

	g_return_val_if_fail (cal_view_factory != NULL, NULL);
	g_return_val_if_fail (IS_CALENDAR_VIEW_FACTORY (cal_view_factory), NULL);

	priv = cal_view_factory->priv;

	priv->view_type = view_type;

	return cal_view_factory;
}

/**
 * calendar_view_factory_new:
 * @view_type: Type of calendar views that the factory will create.
 * 
 * Creates a new factory for calendar views.
 * 
 * Return value: A newly-created calendar view factory.
 **/
CalendarViewFactory *
calendar_view_factory_new (GnomeCalendarViewType view_type)
{
	CalendarViewFactory *cal_view_factory;

	cal_view_factory = g_object_new (TYPE_CALENDAR_VIEW_FACTORY, NULL);
	return calendar_view_factory_construct (cal_view_factory, view_type);
}

/*
 * Evolution calendar - Generic view factory for calendar views
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "calendar-view-factory.h"
#include "calendar-view.h"

#define CALENDAR_VIEW_FACTORY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_CALENDAR_VIEW_FACTORY, CalendarViewFactoryPrivate))

struct _CalendarViewFactoryPrivate {
	/* Type of views created by this factory */
	GnomeCalendarViewType view_type;
};

static const gchar *
		calendar_view_factory_get_type_code
						(GalViewFactory *factory);
static GalView *
		calendar_view_factory_new_view	(GalViewFactory *factory,
						 const gchar *name);

G_DEFINE_TYPE (
	CalendarViewFactory,
	calendar_view_factory,
	GAL_TYPE_VIEW_FACTORY)

static void
calendar_view_factory_class_init (CalendarViewFactoryClass *class)
{
	GalViewFactoryClass *gal_view_factory_class;

	g_type_class_add_private (class, sizeof (CalendarViewFactoryPrivate));

	gal_view_factory_class = GAL_VIEW_FACTORY_CLASS (class);
	gal_view_factory_class->get_type_code = calendar_view_factory_get_type_code;
	gal_view_factory_class->new_view = calendar_view_factory_new_view;
}

static void
calendar_view_factory_init (CalendarViewFactory *cal_view_factory)
{
	cal_view_factory->priv =
		CALENDAR_VIEW_FACTORY_GET_PRIVATE (cal_view_factory);
}

/* get_type_code method for the calendar view factory */
static const gchar *
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
		g_return_val_if_reached (NULL);
	}
}

/* new_view method for the calendar view factory */
static GalView *
calendar_view_factory_new_view (GalViewFactory *factory,
                                const gchar *title)
{
	CalendarViewFactory *cal_view_factory;
	GType type;

	cal_view_factory = CALENDAR_VIEW_FACTORY (factory);

	switch (cal_view_factory->priv->view_type) {
		case GNOME_CAL_DAY_VIEW:
			type = GAL_TYPE_VIEW_CALENDAR_DAY;
			break;
		case GNOME_CAL_WORK_WEEK_VIEW:
			type = GAL_TYPE_VIEW_CALENDAR_WORK_WEEK;
			break;
		case GNOME_CAL_WEEK_VIEW:
			type = GAL_TYPE_VIEW_CALENDAR_WEEK;
			break;
		case GNOME_CAL_MONTH_VIEW:
			type = GAL_TYPE_VIEW_CALENDAR_MONTH;
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return g_object_new (type, "title", title, NULL);
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
GalViewFactory *
calendar_view_factory_construct (CalendarViewFactory *cal_view_factory,
                                 GnomeCalendarViewType view_type)
{
	CalendarViewFactoryPrivate *priv;

	g_return_val_if_fail (cal_view_factory != NULL, NULL);
	g_return_val_if_fail (IS_CALENDAR_VIEW_FACTORY (cal_view_factory), NULL);

	priv = cal_view_factory->priv;

	priv->view_type = view_type;

	return GAL_VIEW_FACTORY (cal_view_factory);
}

/**
 * calendar_view_factory_new:
 * @view_type: Type of calendar views that the factory will create.
 *
 * Creates a new factory for calendar views.
 *
 * Return value: A newly-created calendar view factory.
 **/
GalViewFactory *
calendar_view_factory_new (GnomeCalendarViewType view_type)
{
	CalendarViewFactory *cal_view_factory;

	cal_view_factory = g_object_new (TYPE_CALENDAR_VIEW_FACTORY, NULL);
	return calendar_view_factory_construct (cal_view_factory, view_type);
}

/* Evolution calendar - Generic view factory for calendar views
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "calendar-view-factory.h"
#include "calendar-view.h"



/* Private part of the CalendarViewFactory structure */
struct _CalendarViewFactoryPrivate {
	/* Type of views created by this factory */
	GnomeCalendarViewType view_type;
};



static void calendar_view_factory_class_init (CalendarViewFactoryClass *class);
static void calendar_view_factory_init (CalendarViewFactory *cal_view_factory);
static void calendar_view_factory_destroy (GtkObject *object);

static const char *calendar_view_factory_get_title (GalViewFactory *factory);
static const char *calendar_view_factory_get_type_code (GalViewFactory *factory);
static GalView *calendar_view_factory_new_view (GalViewFactory *factory, const char *name);

static GalViewFactoryClass *parent_class = NULL;



/**
 * calendar_view_factory_get_type:
 * 
 * Registers the #CalendarViewFactory class if necessary, and returns the type
 * ID associated to it.
 * 
 * Return value: The type ID of the #CalendarViewFactory class.
 **/
GtkType
calendar_view_factory_get_type (void)
{
	static GtkType calendar_view_factory_type;

	if (!calendar_view_factory_type) {
		static const GtkTypeInfo calendar_view_factory_info = {
			"CalendarViewFactory",
			sizeof (CalendarViewFactory),
			sizeof (CalendarViewFactoryClass),
			(GtkClassInitFunc) calendar_view_factory_class_init,
			(GtkObjectInitFunc) calendar_view_factory_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		calendar_view_factory_type = gtk_type_unique (GAL_VIEW_FACTORY_TYPE,
							      &calendar_view_factory_info);
	}

	return calendar_view_factory_type;
}

/* Class initialization function for the calendar view factory */
static void
calendar_view_factory_class_init (CalendarViewFactoryClass *class)
{
	GalViewFactoryClass *gal_view_factory_class;
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (GAL_VIEW_FACTORY_TYPE);

	gal_view_factory_class = (GalViewFactoryClass *) class;
	object_class = (GtkObjectClass *) class;

	gal_view_factory_class->get_title = calendar_view_factory_get_title;
	gal_view_factory_class->get_type_code = calendar_view_factory_get_type_code;
	gal_view_factory_class->new_view = calendar_view_factory_new_view;

	object_class->destroy = calendar_view_factory_destroy;
}

/* Object initialization class for the calendar view factory */
static void
calendar_view_factory_init (CalendarViewFactory *cal_view_factory)
{
	CalendarViewFactoryPrivate *priv;

	priv = g_new0 (CalendarViewFactoryPrivate, 1);
	cal_view_factory->priv = priv;
}

/* Destroy method for the calendar view factory */
static void
calendar_view_factory_destroy (GtkObject *object)
{
	CalendarViewFactory *cal_view_factory;
	CalendarViewFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CALENDAR_VIEW_FACTORY (object));

	cal_view_factory = CALENDAR_VIEW_FACTORY (object);
	priv = cal_view_factory->priv;

	g_free (priv);
	cal_view_factory->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

	cal_view_factory = gtk_type_new (TYPE_CALENDAR_VIEW_FACTORY);
	return calendar_view_factory_construct (cal_view_factory, view_type);
}

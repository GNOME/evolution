/*
 *
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

#ifndef CALENDAR_VIEW_FACTORY_H
#define CALENDAR_VIEW_FACTORY_H

#include <menus/gal-view-factory.h>
#include "gnome-cal.h"

G_BEGIN_DECLS



#define TYPE_CALENDAR_VIEW_FACTORY            (calendar_view_factory_get_type ())
#define CALENDAR_VIEW_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CALENDAR_VIEW_FACTORY,  \
					       CalendarViewFactory))
#define CALENDAR_VIEW_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),			   \
					       TYPE_CALENDAR_VIEW_FACTORY, CalendarViewClass))
#define IS_CALENDAR_VIEW_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CALENDAR_VIEW_FACTORY))
#define IS_CALENDAR_VIEW_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),			   \
					       TYPE_CALENDAR_VIEW_FACTORY))

typedef struct _CalendarViewFactoryPrivate CalendarViewFactoryPrivate;

typedef struct {
	GalViewFactory factory;

	/* Private data */
	CalendarViewFactoryPrivate *priv;
} CalendarViewFactory;

typedef struct {
	GalViewFactoryClass parent_class;
} CalendarViewFactoryClass;

GType calendar_view_factory_get_type (void);

GalViewFactory *calendar_view_factory_construct (CalendarViewFactory *cal_view_factory,
						      GnomeCalendarViewType view_type);

GalViewFactory *calendar_view_factory_new (GnomeCalendarViewType view_type);



G_END_DECLS

#endif

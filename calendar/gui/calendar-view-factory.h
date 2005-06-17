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

#ifndef CALENDAR_VIEW_FACTORY_H
#define CALENDAR_VIEW_FACTORY_H

#include <widgets/menus/gal-view-factory.h>
#include "gnome-cal.h"

G_BEGIN_DECLS



#define TYPE_CALENDAR_VIEW_FACTORY            (calendar_view_factory_get_type ())
#define CALENDAR_VIEW_FACTORY(obj)            (GTK_CHECK_CAST ((obj), TYPE_CALENDAR_VIEW_FACTORY,  \
					       CalendarViewFactory))
#define CALENDAR_VIEW_FACTORY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),			   \
					       TYPE_CALENDAR_VIEW_FACTORY, CalendarViewClass))
#define IS_CALENDAR_VIEW_FACTORY(obj)         (GTK_CHECK_TYPE ((obj), TYPE_CALENDAR_VIEW_FACTORY))
#define IS_CALENDAR_VIEW_FACTORY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),			   \
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

GtkType calendar_view_factory_get_type (void);

CalendarViewFactory *calendar_view_factory_construct (CalendarViewFactory *cal_view_factory,
						      GnomeCalendarViewType view_type);

CalendarViewFactory *calendar_view_factory_new (GnomeCalendarViewType view_type);



G_END_DECLS

#endif

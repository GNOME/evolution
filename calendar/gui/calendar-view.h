/*
 *
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CALENDAR_VIEW_H
#define CALENDAR_VIEW_H

#include <widgets/menus/gal-view.h>
#include "gnome-cal.h"

G_BEGIN_DECLS



#define TYPE_CALENDAR_VIEW            (calendar_view_get_type ())
#define CALENDAR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CALENDAR_VIEW, CalendarView))
#define CALENDAR_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CALENDAR_VIEW,	\
				       CalendarViewClass))
#define IS_CALENDAR_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CALENDAR_VIEW))
#define IS_CALENDAR_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CALENDAR_VIEW))

typedef struct _CalendarViewPrivate CalendarViewPrivate;

typedef struct {
	GalView view;

	/* Private data */
	CalendarViewPrivate *priv;
} CalendarView;

typedef struct {
	GalViewClass parent_class;
} CalendarViewClass;

GType calendar_view_get_type (void);

CalendarView *calendar_view_construct (CalendarView *cal_view,
				       GnomeCalendarViewType view_type,
				       const gchar *title);

CalendarView *calendar_view_new (GnomeCalendarViewType view_type,
				 const gchar *title);

GnomeCalendarViewType calendar_view_get_view_type (CalendarView *cal_view);



G_END_DECLS

#endif

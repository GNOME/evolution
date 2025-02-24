/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CALENDAR_H_
#define _E_CALENDAR_H_

#include <gtk/gtk.h>
#include <e-util/e-canvas.h>
#include <e-util/e-calendar-item.h>

G_BEGIN_DECLS

/*
 * ECalendar - displays a table of monthly calendars, allowing highlighting
 * and selection of one or more days. Like GtkCalendar with more features.
 * Most of the functionality is in the ECalendarItem canvas item, though
 * we also add GnomeCanvasWidget buttons to go to the previous/next month and
 * to got to the current day.
 */

/* Standard GObject macros */
#define E_TYPE_CALENDAR \
	(e_calendar_get_type ())
#define E_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALENDAR, ECalendar))
#define E_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALENDAR, ECalendarClass))
#define E_IS_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALENDAR))
#define E_IS_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALENDAR))
#define E_CALENDAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALENDAR, ECalendarClass))

typedef struct _ECalendar ECalendar;
typedef struct _ECalendarClass ECalendarClass;
typedef struct _ECalendarPrivate ECalendarPrivate;

struct _ECalendar {
	ECanvas parent;

	ECalendarPrivate *priv;
};

struct _ECalendarClass {
	ECanvasClass parent_class;
};

GType		e_calendar_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_calendar_new			(void);
ECalendarItem *	e_calendar_get_item		(ECalendar *cal);
void		e_calendar_set_minimum_size	(ECalendar *cal,
						 gint rows,
						 gint cols);

G_END_DECLS

#endif /* _E_CALENDAR_H_ */

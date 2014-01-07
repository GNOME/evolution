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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EA_GNOME_CALENDAR_H__
#define __EA_GNOME_CALENDAR_H__

#include <gtk/gtk.h>
#include "gnome-cal.h"

G_BEGIN_DECLS

#define EA_TYPE_GNOME_CALENDAR            (ea_gnome_calendar_get_type ())
#define EA_GNOME_CALENDAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_GNOME_CALENDAR, EaGnomeCalendar))
#define EA_GNOME_CALENDAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_GNOME_CALENDAR, EaGnomeCalendarClass))
#define EA_IS_GNOME_CALENDAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_GNOME_CALENDAR))
#define EA_IS_GNOME_CALENDAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_GNOME_CALENDAR))
#define EA_GNOME_CALENDAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_GNOME_CALENDAR, EaGnomeCalendarClass))

typedef struct _EaGnomeCalendar                   EaGnomeCalendar;
typedef struct _EaGnomeCalendarClass              EaGnomeCalendarClass;

struct _EaGnomeCalendar
{
	GtkAccessible parent;
};

GType ea_gnome_calendar_get_type (void);

struct _EaGnomeCalendarClass
{
	GtkAccessibleClass parent_class;
};

AtkObject *     ea_gnome_calendar_new         (GtkWidget       *widget);

const gchar *	ea_gnome_calendar_get_label_description (GnomeCalendar *gcal);

G_END_DECLS

#endif /* __EA_GNOME_CALENDAR_H__ */

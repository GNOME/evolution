/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-gnome-calendar.h
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 *
 */

#ifndef __EA_GNOME_CALENDAR_H__
#define __EA_GNOME_CALENDAR_H__

#include <gtk/gtkaccessible.h>
#include "gnome-cal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

AtkObject*     ea_gnome_calendar_new         (GtkWidget       *widget);

const gchar *	ea_gnome_calendar_get_label_description (GnomeCalendar *gcal);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EA_GNOME_CALENDAR_H__ */

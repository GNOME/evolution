/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */

#ifndef GNOME_CALENDAR_APP_H
#define GNOME_CALENDAR_APP_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-app.h>

BEGIN_GNOME_DECLS

#define GNOME_CALENDAR(obj)         GTK_CHECK_CAST(obj, gnome_calendar_get_type(), GnomeCalendar)
#define GNOME_CALENDAR_CLASS(class) GTK_CHECK_CAST_CLASS(class, gnome_calendar_get_type(), GnomeCalendarClass)
#define GNOME_IS_CALENDAR(obj)      GTK_CHECK_TYPE(obj, gnome_calendar_get_type())

typedef struct {
	GnomeApp gnome_app;
	Calendar *cal;
} GnomeCalendar;

typedef struct {
	GnomeAppClass parent_class;
} GnomeCalendarClass;

guint     gnome_calendar_get_type    (void);
GtkWidget *gnome_calendar_new        (char *title);
void      gnome_calendar_load        (GnomeCalendar *gcal, char *file);

END_GNOME_DECLS

#endif

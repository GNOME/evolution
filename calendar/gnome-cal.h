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

#include "calendar.h"

BEGIN_GNOME_DECLS

#define GNOME_CALENDAR(obj)         GTK_CHECK_CAST(obj, gnome_calendar_get_type(), GnomeCalendar)
#define GNOME_CALENDAR_CLASS(class) GTK_CHECK_CAST_CLASS(class, gnome_calendar_get_type(), GnomeCalendarClass)
#define GNOME_IS_CALENDAR(obj)      GTK_CHECK_TYPE(obj, gnome_calendar_get_type())

typedef struct {
	GnomeApp    gnome_app;
	Calendar    *cal;
	time_t      current_display;
	
	GtkWidget   *notebook;
	GtkWidget   *day_view;
	GtkWidget   *week_view;
	GtkWidget   *year_view;
	GtkWidget   *task_view;
	void        *event_editor;
} GnomeCalendar;

typedef struct {
	GnomeAppClass parent_class;
} GnomeCalendarClass;

guint      gnome_calendar_get_type         	(void);
GtkWidget *gnome_calendar_new              	(char *title);
int        gnome_calendar_load             	(GnomeCalendar *gcal,
						 char *file);
void       gnome_calendar_add_object       	(GnomeCalendar *gcal,
						 iCalObject *obj);
void       gnome_calendar_remove_object    	(GnomeCalendar *gcal,
						 iCalObject *obj);
void       gnome_calendar_next             	(GnomeCalendar *gcal);
void       gnome_calendar_previous         	(GnomeCalendar *gcal);
void       gnome_calendar_goto             	(GnomeCalendar *gcal,
						 time_t new_time);
void       gnome_calendar_dayjump          	(GnomeCalendar *gcal,
						 time_t time);
void       gnome_calendar_tag_calendar          (GnomeCalendar *cal,
						 GtkCalendar *gtk_cal);
char      *gnome_calendar_get_current_view_name (GnomeCalendar *gcal);
void       gnome_calendar_set_view              (GnomeCalendar *gcal,
						 char *page_name);

/* Flags is a bitmask of CalObjectChange values */
void       gnome_calendar_object_changed        (GnomeCalendar *gcal,
						 iCalObject *obj,
						 int flags);

END_GNOME_DECLS

#endif

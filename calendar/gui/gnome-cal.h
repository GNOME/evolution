/* Evolution calendar - Main calendar view widget
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
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

#ifndef GNOME_CALENDAR_APP_H
#define GNOME_CALENDAR_APP_H

#include <time.h>
#include <libgnome/gnome-defs.h>
#include <gtk/gtkvbox.h>
#include <bonobo.h>
#include <widgets/misc/e-calendar.h>
#include <cal-client/cal-client.h>

BEGIN_GNOME_DECLS



#define GNOME_TYPE_CALENDAR            (gnome_calendar_get_type ())
#define GNOME_CALENDAR(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_CALENDAR, GnomeCalendar))
#define GNOME_CALENDAR_CLASS(klass)    (GTK_CHECK_CAST_CLASS ((klass), GNOME_TYPE_CALENDAR,	\
					GnomeCalendarClass))
#define GNOME_IS_CALENDAR(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_CALENDAR))
#define GNOME_IS_CALENDAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CALENDAR))

typedef struct _GnomeCalendar GnomeCalendar;
typedef struct _GnomeCalendarClass GnomeCalendarClass;
typedef struct _GnomeCalendarPrivate GnomeCalendarPrivate;

struct _GnomeCalendar {
	GtkVBox vbox;

	/* Private data */
	GnomeCalendarPrivate *priv;
};

struct _GnomeCalendarClass {
	GtkVBoxClass parent_class;
};


GtkType    gnome_calendar_get_type         	(void);
GtkWidget *gnome_calendar_construct		(GnomeCalendar *gcal);

GtkWidget *gnome_calendar_new			(void);

CalClient *gnome_calendar_get_cal_client	(GnomeCalendar *gcal);

gboolean   gnome_calendar_open                  (GnomeCalendar *gcal, const char *str_uri);
/*
int	   gnome_calendar_create		(GnomeCalendar *gcal,
						 char *file);
*/
void       gnome_calendar_next             	(GnomeCalendar *gcal);
void       gnome_calendar_previous         	(GnomeCalendar *gcal);
void       gnome_calendar_goto             	(GnomeCalendar *gcal,
						 time_t new_time);
void       gnome_calendar_dayjump          	(GnomeCalendar *gcal,
						 time_t time);
/* Jumps to the current day */
void       gnome_calendar_goto_today            (GnomeCalendar *gcal);
char      *gnome_calendar_get_current_view_name (GnomeCalendar *gcal);
void	   gnome_calendar_set_view		(GnomeCalendar	*gcal,
						 char		*page_name,
						 gboolean	 reset_range_shown,
						 gboolean	 focus);

void	   gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
						   time_t	  start_time,
						   time_t	  end_time);
void	   gnome_calendar_get_selected_time_range (GnomeCalendar *gcal,
						   time_t	 *start_time,
						   time_t	 *end_time);

void       gnome_calendar_edit_object           (GnomeCalendar *gcal,
						 CalComponent  *comp);

void       gnome_calendar_new_appointment       (GnomeCalendar *gcal);
void       gnome_calendar_new_appointment_for   (GnomeCalendar *cal,
						 time_t dtstart, time_t dtend,
						 gboolean all_day);

/* Returns the selected time range for the current view. Note that this may be
   different from the fields in the GnomeCalendar, since the view may clip
   this or choose a more appropriate time. */
void	   gnome_calendar_get_current_time_range (GnomeCalendar *gcal,
						  time_t	 *start_time,
						  time_t	 *end_time);

/* Tells the calendar to reload all config settings. initializing should be
   TRUE when we are setting the config settings for the first time. */
void	   gnome_calendar_update_config_settings (GnomeCalendar *gcal,
						  gboolean	 initializing);

void	   gnome_calendar_set_view_buttons	(GnomeCalendar	*gcal,
						 GtkWidget	*day_button,
						 GtkWidget	*work_week_button,
						 GtkWidget	*week_button,
						 GtkWidget	*month_button);

/* This makes the appropriate radio button in the toolbar active.
   It sets the ignore_view_button_clicks flag so the "clicked" signal handlers
   just return without doing anything. */
void	   gnome_calendar_update_view_buttons	(GnomeCalendar	*gcal);



END_GNOME_DECLS

#endif

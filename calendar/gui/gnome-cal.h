/*
 * GnomeCalendar widget
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors: Miguel de Icaza (miguel@kernel.org)
 *          Federico Mena-Quintero <federico@helixcode.com>
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


typedef enum {
  CALENDAR_OPEN,
  CALENDAR_OPEN_OR_CREATE
} GnomeCalendarOpenMode;

GtkType    gnome_calendar_get_type         	(void);
GtkWidget *gnome_calendar_construct		(GnomeCalendar *gcal);

GtkWidget *gnome_calendar_new			(void);

CalClient *gnome_calendar_get_cal_client	(GnomeCalendar *gcal);

int        gnome_calendar_open                  (GnomeCalendar *gcal,
						 char *file,
						 GnomeCalendarOpenMode gcom);
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
void       gnome_calendar_tag_calendar          (GnomeCalendar *gcal,
						 ECalendar     *ecal);
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

/* Returns the selected time range for the current view. Note that this may be
   different from the fields in the GnomeCalendar, since the view may clip
   this or choose a more appropriate time. */
void	   gnome_calendar_get_current_time_range (GnomeCalendar *gcal,
						  time_t	 *start_time,
						  time_t	 *end_time);


/* Notifies the calendar that the time format has changed and it must update
   all its views */
void	   gnome_calendar_time_format_changed	(GnomeCalendar *gcal);

/* Notifies the calendar that the todo list properties have changed and its
   time to update the views. */
void	   gnome_calendar_colors_changed	(GnomeCalendar *gcal);

/* Notifies the calendar that the todo list properties have changed and its
   time to update the views. */
void	   gnome_calendar_todo_properties_changed (GnomeCalendar *gcal);


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

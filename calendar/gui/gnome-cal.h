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



/* These must match the page numbers in the GtkNotebook. */
typedef enum {
	GNOME_CALENDAR_VIEW_DAY		= 0,
	GNOME_CALENDAR_VIEW_WORK_WEEK	= 1,
	GNOME_CALENDAR_VIEW_WEEK	= 2,
	GNOME_CALENDAR_VIEW_MONTH	= 3,

	GNOME_CALENDAR_VIEW_NOT_SET	= 9
} GnomeCalendarViewType;


#define GNOME_CALENDAR(obj)         GTK_CHECK_CAST(obj, gnome_calendar_get_type(), GnomeCalendar)
#define GNOME_CALENDAR_CLASS(class) GTK_CHECK_CAST_CLASS(class, gnome_calendar_get_type(), GnomeCalendarClass)
#define GNOME_IS_CALENDAR(obj)      GTK_CHECK_TYPE(obj, gnome_calendar_get_type())

typedef struct {
	GtkVBox      vbox;

	CalClient   *client;

        BonoboPropertyBag *properties;
	BonoboControl *control;

	GHashTable  *object_editor_hash;

	/* This is the last selection explicitly selected by the user. We try
	   to keep it the same when we switch views, but we may have to alter
	   it depending on the view (e.g. the week views only select days, so
	   any times are lost. */
	time_t      selection_start_time;
	time_t      selection_end_time;

	GtkWidget   *hpane;
	GtkWidget   *notebook;
	GtkWidget   *vpane;
	ECalendar   *date_navigator;
	GtkWidget   *todo;

	GtkWidget   *day_view;
	GtkWidget   *work_week_view;
	GtkWidget   *week_view;
	GtkWidget   *month_view;

	/* These are the toolbar radio buttons for switching views. */
	GtkWidget   *day_button;
	GtkWidget   *work_week_button;
	GtkWidget   *week_button;
	GtkWidget   *month_button;

	/* This is the view currently shown. We use it to keep track of the
	   positions of the panes. range_selected is TRUE if a range of dates
	   was selected in the date navigator to show the view. */
	GnomeCalendarViewType current_view_type;
	gboolean range_selected;

	/* These are the saved positions of the panes. They are multiples of
	   calendar month widths & heights in the date navigator, so that they
	   will work OK after theme changes. */
	gfloat	     hpane_pos;
	gfloat	     vpane_pos;
	gfloat	     hpane_pos_month_view;
	gfloat	     vpane_pos_month_view;

	/* This is TRUE when we just want to set the state of the toolbar
	   radio buttons without causing any related code to be executed.
	   The "clicked" signal handlers just return when this is set. */
	gboolean     ignore_view_button_clicks;

	/* The signal handler id for our GtkCalendar "day_selected" handler. */
	guint	     day_selected_id;

	/* Alarm ID for the midnight refresh function */
	gpointer midnight_alarm_refresh_id;

	/* UID->alarms hash */
	GHashTable *alarms;
} GnomeCalendar;

typedef struct {
	GtkVBoxClass parent_class;
} GnomeCalendarClass;


typedef enum {
  CALENDAR_OPEN,
  CALENDAR_OPEN_OR_CREATE
} GnomeCalendarOpenMode;

guint      gnome_calendar_get_type         	(void);
GtkWidget *gnome_calendar_new			(char *title);
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
						 gboolean	 reset_range_shown);

void	   gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
						   time_t	  start_time,
						   time_t	  end_time);

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


/* This makes the appropriate radio button in the toolbar active.
   It sets the ignore_view_button_clicks flag so the "clicked" signal handlers
   just return without doing anything. */
void	   gnome_calendar_update_view_buttons	(GnomeCalendar	*gcal);



END_GNOME_DECLS

#endif

/* Evolution calendar - Main calendar view widget
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Miguel de Icaza <miguel@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
 *          Seth Alves <alves@hungry.com>
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

#ifndef GNOME_CALENDAR_APP_H
#define GNOME_CALENDAR_APP_H

#include <time.h>
#include <gtk/gtkvbox.h>
#include <bonobo/bonobo-ui-component.h>
#include <widgets/misc/e-calendar.h>
#include <libecal/e-cal.h>

#include "e-calendar-table.h"

G_BEGIN_DECLS



struct _EPopup;

#define GNOME_TYPE_CALENDAR            (gnome_calendar_get_type ())
#define GNOME_CALENDAR(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_CALENDAR, GnomeCalendar))
#define GNOME_CALENDAR_CLASS(klass)    (GTK_CHECK_CAST_CLASS ((klass), GNOME_TYPE_CALENDAR,	\
					GnomeCalendarClass))
#define GNOME_IS_CALENDAR(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_CALENDAR))
#define GNOME_IS_CALENDAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CALENDAR))

typedef struct _GnomeCalendar GnomeCalendar;
typedef struct _GnomeCalendarClass GnomeCalendarClass;
typedef struct _GnomeCalendarPrivate GnomeCalendarPrivate;

/* View types */
typedef enum {
	GNOME_CAL_DAY_VIEW,
	GNOME_CAL_WORK_WEEK_VIEW,
	GNOME_CAL_WEEK_VIEW,
	GNOME_CAL_MONTH_VIEW,
	GNOME_CAL_LIST_VIEW,
	GNOME_CAL_LAST_VIEW
} GnomeCalendarViewType;

typedef enum {
	GNOME_CAL_GOTO_TODAY,
	GNOME_CAL_GOTO_DATE,
	GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH,
	GNOME_CAL_GOTO_LAST_DAY_OF_MONTH,
	GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK,
	GNOME_CAL_GOTO_LAST_DAY_OF_WEEK,
	GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK,
	GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK
} GnomeCalendarGotoDateType;

struct _GnomeCalendar {
	GtkVBox vbox;

	/* Private data */
	GnomeCalendarPrivate *priv;
};

struct _GnomeCalendarClass {
	GtkVBoxClass parent_class;

	/* Notification signals */
	void (* dates_shown_changed)    (GnomeCalendar *gcal);

	void (* calendar_selection_changed) (GnomeCalendar *gcal);
	void (* taskpad_selection_changed) (GnomeCalendar *gcal);

	void (* calendar_focus_change)  (GnomeCalendar *gcal, gboolean in);
	void (* taskpad_focus_change)   (GnomeCalendar *gcal, gboolean in);

        void (* source_added)           (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source);
        void (* source_removed)         (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source);

	/* Action signals */
        void (* goto_date)              (GnomeCalendar *gcal, GnomeCalendarGotoDateType date);
};


GtkType    gnome_calendar_get_type         	(void);
GtkWidget *gnome_calendar_construct		(GnomeCalendar *gcal);

GtkWidget *gnome_calendar_new			(void);

void gnome_calendar_set_activity_handler (GnomeCalendar *cal, EActivityHandler *activity_handler);
void gnome_calendar_set_ui_component (GnomeCalendar *cal, BonoboUIComponent *ui_component);

ECalendarTable *gnome_calendar_get_task_pad	(GnomeCalendar *gcal);

ECalModel *gnome_calendar_get_calendar_model    (GnomeCalendar *gcal);
ECal *gnome_calendar_get_default_client    (GnomeCalendar *gcal);

gboolean   gnome_calendar_add_source      (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source);
gboolean   gnome_calendar_remove_source   (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source);
gboolean   gnome_calendar_remove_source_by_uid   (GnomeCalendar *gcal, ECalSourceType source_type, const char *uid);
gboolean   gnome_calendar_set_default_source (GnomeCalendar *gcal, ECalSourceType source_type, ESource *source);

void       gnome_calendar_next             	(GnomeCalendar *gcal);
void       gnome_calendar_previous         	(GnomeCalendar *gcal);
void       gnome_calendar_goto             	(GnomeCalendar *gcal,
						 time_t new_time);
void       gnome_calendar_dayjump          	(GnomeCalendar *gcal,
						 time_t time);
/* Jumps to the current day */
void       gnome_calendar_goto_today            (GnomeCalendar *gcal);

GnomeCalendarViewType gnome_calendar_get_view (GnomeCalendar *gcal);
void gnome_calendar_set_view (GnomeCalendar *gcal, GnomeCalendarViewType view_type);

GtkWidget *gnome_calendar_get_current_view_widget (GnomeCalendar *gcal);

ECalendarTable *gnome_calendar_get_task_pad	(GnomeCalendar *gcal);
GtkWidget *gnome_calendar_get_e_calendar_widget (GnomeCalendar *gcal); 
GtkWidget *gnome_calendar_get_search_bar_widget (GnomeCalendar *gcal);
GtkWidget *gnome_calendar_get_view_notebook_widget (GnomeCalendar *gcal);

struct _ECalMenu *gnome_calendar_get_taskpad_menu (GnomeCalendar *gcal);
struct _ECalMenu *gnome_calendar_get_calendar_menu (GnomeCalendar *gcal);

void gnome_calendar_setup_view_menus (GnomeCalendar *gcal, BonoboUIComponent *uic);
void gnome_calendar_discard_view_menus (GnomeCalendar *gcal);

void gnome_calendar_view_popup_factory (GnomeCalendar *gcal, struct _EPopup *ep, const char *prefix);

void	   gnome_calendar_set_selected_time_range (GnomeCalendar *gcal,
						   time_t	  start_time,
						   time_t	  end_time);
void	   gnome_calendar_get_selected_time_range (GnomeCalendar *gcal,
						   time_t	 *start_time,
						   time_t	 *end_time);

void       gnome_calendar_new_task		(GnomeCalendar *gcal);

/* Returns the selected time range for the current view. Note that this may be
   different from the fields in the GnomeCalendar, since the view may clip
   this or choose a more appropriate time. */
void	   gnome_calendar_get_current_time_range (GnomeCalendar *gcal,
						  time_t	 *start_time,
						  time_t	 *end_time);

/* Gets the visible time range for the current view. Returns FALSE if no
   time range has been set yet. */
gboolean   gnome_calendar_get_visible_time_range (GnomeCalendar *gcal,
						  time_t	 *start_time,
						  time_t	 *end_time);

/* Returns the number of selected events (0 or 1 at present). */
gint	   gnome_calendar_get_num_events_selected (GnomeCalendar *gcal);

/* Returns the number of selected tasks */
gint       gnome_calendar_get_num_tasks_selected (GnomeCalendar *gcal);

/* Get the current timezone. */
icaltimezone *gnome_calendar_get_timezone	(GnomeCalendar	*gcal);


/* Clipboard operations */
void       gnome_calendar_cut_clipboard         (GnomeCalendar  *gcal);
void       gnome_calendar_copy_clipboard        (GnomeCalendar  *gcal);
void       gnome_calendar_paste_clipboard       (GnomeCalendar  *gcal);

void       gnome_calendar_delete_selection	(GnomeCalendar  *gcal);
void       gnome_calendar_delete_selected_occurrence (GnomeCalendar *gcal);
void       gnome_calendar_purge                 (GnomeCalendar  *gcal,
						 time_t older_than);



G_END_DECLS

#endif

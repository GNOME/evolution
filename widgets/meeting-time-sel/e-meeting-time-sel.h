/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_MEETING_TIME_SELECTOR_H_
#define _E_MEETING_TIME_SELECTOR_H_

#include <gtk/gtktable.h>
#include <libgnomeui/gnome-canvas.h>
#include "../e-text/e-text.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EMeetingTimeSelector displays a list of attendees for a meeting and a
 * graphical summary of the times which they are free and busy, allowing the
 * user to select an appropriate time for a meeting.
 */

/* Define this to include the debugging functions. */
#undef E_MEETING_TIME_SELECTOR_DEBUG

/* This is the width of the icon column in the attendees list. */
#define E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH	24

#define E_MEETING_TIME_SELECTOR_TEXT_Y_PAD		3
#define E_MEETING_TIME_SELECTOR_TEXT_X_PAD		2

/* These specify the type of attendee. Either a person or a resource (e.g. a
   meeting room). These are used for the Autopick options, where the user can
   ask for a time when, for example, all people and one resource are free.
   The default is E_MEETING_TIME_SELECTOR_REQUIRED_PERSON. */
typedef enum
{
	E_MEETING_TIME_SELECTOR_REQUIRED_PERSON,
	E_MEETING_TIME_SELECTOR_OPTIONAL_PERSON,
	E_MEETING_TIME_SELECTOR_RESOURCE
} EMeetingTimeSelectorAttendeeType;


/* These are used to specify whether an attendee is free or busy at a
   particular time. We'll probably replace this with a global calendar type.
   These should be ordered in increasing order of preference. Higher precedence
   busy periods will be painted over lower precedence ones. These are also
   used as for loop counters, so they should start at 0 and be ordered. */
typedef enum
{
	E_MEETING_TIME_SELECTOR_BUSY_TENTATIVE		= 0,
	E_MEETING_TIME_SELECTOR_BUSY_OUT_OF_OFFICE	= 1,
	E_MEETING_TIME_SELECTOR_BUSY_BUSY		= 2,

	E_MEETING_TIME_SELECTOR_BUSY_LAST		= 3
} EMeetingTimeSelectorBusyType;


/* This is used to specify the format used when displaying the dates.
   The full format is like 'Sunday, September 12, 1999'. The abbreviated format
   is like 'Sun 12/9/99'. The short format is like '12/9/99'. The actual
   format used is determined in e_meeting_time_selector_style_set(), once we
   know the font being used. */
typedef enum
{
	E_MEETING_TIME_SELECTOR_DATE_FULL,
	E_MEETING_TIME_SELECTOR_DATE_ABBREVIATED_DAY,
	E_MEETING_TIME_SELECTOR_DATE_SHORT
} EMeetingTimeSelectorDateFormat;


/* This is used to specify a position regarding the vertical bars around the
   current meeting time, so we know which one is being dragged. */
typedef enum
{
	E_MEETING_TIME_SELECTOR_POS_NONE,
	E_MEETING_TIME_SELECTOR_POS_START,
	E_MEETING_TIME_SELECTOR_POS_END
} EMeetingTimeSelectorPosition;


/* This is used to specify the autopick option, which determines how we choose
   the previous/next appropriate meeting time. */
typedef enum
{
	E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_RESOURCES,
	E_MEETING_TIME_SELECTOR_ALL_PEOPLE_AND_ONE_RESOURCE,
	E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE,
	E_MEETING_TIME_SELECTOR_REQUIRED_PEOPLE_AND_ONE_RESOURCE
} EMeetingTimeSelectorAutopickOption;


/* This is our representation of a time. We use a GDate to store the day,
   and guint8s for the hours and minutes. */
typedef struct _EMeetingTimeSelectorTime       EMeetingTimeSelectorTime;
struct _EMeetingTimeSelectorTime
{
	GDate	date;
	guint8	hour;
	guint8	minute;
};


/* This represents a busy period. */
typedef struct _EMeetingTimeSelectorPeriod     EMeetingTimeSelectorPeriod;
struct _EMeetingTimeSelectorPeriod
{
	EMeetingTimeSelectorTime start;
	EMeetingTimeSelectorTime end;
	EMeetingTimeSelectorBusyType busy_type;
};


/* This contains information on one attendee. */
typedef struct _EMeetingTimeSelectorAttendee   EMeetingTimeSelectorAttendee;
struct _EMeetingTimeSelectorAttendee
{
	gchar *name;

	/* The type of attendee, e.g. a person or a resource. */
	EMeetingTimeSelectorAttendeeType type;

	/* This is TRUE if the attendee has calendar information available.
	   It is set to TRUE when a busy period is added, but can also be set
	   to TRUE explicitly to indicate that the attendee has calendar
	   information available, but no current busy periods. If it is FALSE
	   then a diagonal stipple pattern is used to fill the entire row in
	   the main graphical display. */
	gboolean has_calendar_info;

	/* This is TRUE if the meeting request is sent to this attendee. */
	gboolean send_meeting_to;

	/* This is the period for which free/busy data for the attendee is
	   available. */
	EMeetingTimeSelectorTime busy_periods_start;
	EMeetingTimeSelectorTime busy_periods_end;

	/* This is an array of EMeetingTimeSelectorPeriod elements. When it is
	   updated busy_periods_sorted is set to FALSE, and if a function
	   needs them sorted, it should call this to re-sort them if needed:
	   e_meeting_time_selector_attendee_ensure_periods_sorted(). Note that
	   they are sorted by the start times. */
	GArray	*busy_periods;
	gboolean busy_periods_sorted;

	/* This holds the length of the longest busy period in days, rounded
	   up. It is used to determine where to start looking in the
	   busy_periods array. If we didn't use this we'd have to go through
	   most of the busy_periods array every time we wanted to paint part
	   of the display. */
	gint longest_period_in_days;

	/* This is the canvas text item where the name is edited. */
	GnomeCanvasItem *text_item;

	/* This is supposed to be something like an address book id. */
	gpointer data;
};

/* An array of hour strings, "0:00" .. "23:00". */
extern const gchar *EMeetingTimeSelectorHours[24];


#define E_MEETING_TIME_SELECTOR(obj)          GTK_CHECK_CAST (obj, e_meeting_time_selector_get_type (), EMeetingTimeSelector)
#define E_MEETING_TIME_SELECTOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_meeting_time_selector_get_type (), EMeetingTimeSelectorClass)
#define IS_E_MEETING_TIME_SELECTOR(obj)       GTK_CHECK_TYPE (obj, e_meeting_time_selector_get_type ())


typedef struct _EMeetingTimeSelector       EMeetingTimeSelector;
typedef struct _EMeetingTimeSelectorClass  EMeetingTimeSelectorClass;

struct _EMeetingTimeSelector
{
	/* We subclass a GtkTable which makes it easy to add extra widgets
	   if neccesary. */
	GtkTable table;

	/*
	 * User Interface stuff - widgets, colors etc.
	 */

	/* This contains our keyboard accelerators, which need to be added to
	   the toplevel window. */
	GtkAccelGroup *accel_group;

	/* The vbox in the top-left corner, containing the 'All Attendees'
	   title bar packed at the end. Extra widgets can be added here
	   with PACK_START if necessary. */
	GtkWidget *attendees_title_bar_vbox;

	/* The 'All Attendees' title bar above the list of attendees. */
	GtkWidget *attendees_title_bar;

	/* The list of attendees. */
	GtkWidget *attendees_list;

	/* The canvas displaying the dates, times, and the summary
	   'All Attendees' free/busy display. */
	GtkWidget *display_top;

	/* The canvas containing the free/busy displays of individual
	   attendees. This is separate from display_top since it also scrolls
	   vertically. */
	GtkWidget *display_main;

	/* This is the 'Options' button & menu. */
	GtkWidget *options_button;
	GtkWidget *options_menu;

	/* This is the 'Autopick' button, menu & radio menu items. */
	GtkWidget *autopick_button;
	GtkWidget *autopick_menu;
	GtkWidget *autopick_all_item;
	GtkWidget *autopick_all_people_one_resource_item;
	GtkWidget *autopick_required_people_item;
	GtkWidget *autopick_required_people_one_resource_item;

	/* The horizontal scrollbar which scrolls display_top & display_main.*/
	GtkWidget *hscrollbar;

	/* The vertical scrollbar which scrolls attendees & display_main. */
	GtkWidget *vscrollbar;

	/* The 2 GnomeDateEdit widgets for the meeting start & end times. */
	GtkWidget *start_date_edit;
	GtkWidget *end_date_edit;

	/* Colors. */
	GdkColorContext *color_context;
	GdkColor bg_color;
	GdkColor all_attendees_bg_color;
	GdkColor meeting_time_bg_color;
	GdkColor stipple_bg_color;
	GdkColor attendee_list_bg_color;
	GdkColor grid_color;
	GdkColor grid_shadow_color;
	GdkColor grid_unused_color;
	GdkColor busy_colors[E_MEETING_TIME_SELECTOR_BUSY_LAST];

	/* The stipple used for attendees with no data. */
	GdkPixmap *stipple;

	/* GC for drawing the color key. */
	GdkGC *color_key_gc;

	/* Width of the hours strings (e.g. "1:00") in the current font. */
	gint hour_widths[24];

	/* Whether we are using the full, abbreviated or short date format. */
	EMeetingTimeSelectorDateFormat date_format;


	/*
	 * Attendee Data.
	 */

	/* This is an array of EMeetingTimeSelectorAttendee elements. */
	GArray *attendees;


	/*
	 * Option Settings.
	 */

	/* If this is TRUE we only show hours between day_start_hour and
	   day_end_hour, defaults to TRUE (9am-6pm). */
	gboolean working_hours_only;
	gint day_start_hour;
	gint day_start_minute;
	gint day_end_hour;
	gint day_end_minute;

	/* If TRUE, view is compressed, with one cell for every 3 hours rather
	   than every hour. Defaults to FALSE. */
	gboolean zoomed_out;


	/*
	 * Internal Data.
	 */

	/* These are the first & last dates shown in the current scroll area.
	   We show E_MEETING_TIME_SELECTOR_DAYS_SHOWN days at a time. */
	GDate first_date_shown;
	GDate last_date_shown;

	/* This is the current selection of the meeting time. */
	EMeetingTimeSelectorTime meeting_start_time;
	EMeetingTimeSelectorTime meeting_end_time;

	/* These are the x pixel coordinates in the entire scroll region of
	   the start and end times. Set to meeting_positions_valid to FALSE to
	   invalidate. They will then be recomputed when needed. Always access
	   with e_meeting_time_selector_get_meeting_time_positions(). */
	gint meeting_positions_valid;
	gint meeting_positions_in_scroll_area;
	gint meeting_start_x;
	gint meeting_end_x;

	/* These are the width and height of the cells, including the grid
	   lines which are displayed on the right and top or bottom of cells.*/
	gint row_height;
	gint col_width;

	/* This is the width of a day in the display, which depends on
	   col_width, working_hours_only and zoomed_out. */
	gint day_width;

	/* These are the first and last hour of each day we display, depending
	   on working_hours_only and zoomed_out. */
	gint first_hour_shown;
	gint last_hour_shown;

	/* The id of the source function for auto-scroll timeouts. */
	guint auto_scroll_timeout_id;

	/* This specifies if we are dragging one of the vertical bars around
	   the meeting time. */
	EMeetingTimeSelectorPosition dragging_position;

	/* The last x coordinate of the mouse, relative to either the left or
	   right edge of the canvas. Used in the auto_scroll_timeout function
	   to determine which way to scroll and how fast. */
	gint last_drag_x;

	/* This is used to determine the delay between scrolls. */
	gint scroll_count;
};


struct _EMeetingTimeSelectorClass
{
	GtkTableClass parent_class;
};


/*
 * PUBLIC INTERFACE - note that this interface will probably change, when I
 * know where the data is coming from. This is mainly just for testing for now.
 */

GtkType e_meeting_time_selector_get_type (void);
GtkWidget* e_meeting_time_selector_new (void);

/* This returns the currently selected meeting time.
   Note that months are 1-12 and days are 1-31. The start time is guaranteed to
   be before or equal to the end time. You may want to check if they are equal
   if that if it is a problem. */
void e_meeting_time_selector_get_meeting_time (EMeetingTimeSelector *mts,
					       gint *start_year,
					       gint *start_month,
					       gint *start_day,
					       gint *start_hour,
					       gint *start_minute,
					       gint *end_year,
					       gint *end_month,
					       gint *end_day,
					       gint *end_hour,
					       gint *end_minute);

/* This sets the meeting time, returning TRUE if it is valid. */
gboolean e_meeting_time_selector_set_meeting_time (EMeetingTimeSelector *mts,
						   gint start_year,
						   gint start_month,
						   gint start_day,
						   gint start_hour,
						   gint start_minute,
						   gint end_year,
						   gint end_month,
						   gint end_day,
						   gint end_hour,
						   gint end_minute);

void e_meeting_time_selector_set_working_hours_only (EMeetingTimeSelector *mts,
						     gboolean working_hours_only);
void e_meeting_time_selector_set_working_hours (EMeetingTimeSelector *mts,
						gint day_start_hour,
						gint day_start_minute,
						gint day_end_hour,
						gint day_end_minute);

void e_meeting_time_selector_set_zoomed_out (EMeetingTimeSelector *mts,
					     gboolean zoomed_out);

EMeetingTimeSelectorAutopickOption e_meeting_time_selector_get_autopick_option (EMeetingTimeSelector *mts);
void e_meeting_time_selector_set_autopick_option (EMeetingTimeSelector *mts,
						  EMeetingTimeSelectorAutopickOption autopick_option);

/* Adds an attendee to the list, returning the row. The data is meant for
   something like an address book id, though if the user edits the name this
   will become invalid. We'll probably have to handle address book lookup
   ourself. */
gint e_meeting_time_selector_attendee_add (EMeetingTimeSelector *mts,
					   gchar *attendee_name,
					   gpointer data);
gint e_meeting_time_selector_attendee_find_by_name (EMeetingTimeSelector *mts,
						    gchar *attendee_name,
						    gint start_row);
gint e_meeting_time_selector_attendee_find_by_data (EMeetingTimeSelector *mts,
						    gpointer data,
						    gint start_row);
void e_meeting_time_selector_attendee_remove (EMeetingTimeSelector *mts,
					      gint row);

void e_meeting_time_selector_attendee_set_type (EMeetingTimeSelector *mts,
						gint row,
						EMeetingTimeSelectorAttendeeType type);
void e_meeting_time_selector_attendee_set_has_calendar_info (EMeetingTimeSelector *mts,
							     gint row,
							     gboolean has_calendar_info);
void e_meeting_time_selector_attendee_set_send_meeting_to (EMeetingTimeSelector *mts,
							   gint row,
							   gboolean send_meeting_to);

gboolean e_meeting_time_selector_attendee_set_busy_range	(EMeetingTimeSelector *mts,
								 gint row,
								 gint start_year,
								 gint start_month,
								 gint start_day,
								 gint start_hour,
								 gint start_minute,
								 gint end_year,
								 gint end_month,
								 gint end_day,
								 gint end_hour,
								 gint end_minute);


/* Clears all busy times for the given attendee. */
void e_meeting_time_selector_attendee_clear_busy_periods (EMeetingTimeSelector *mts,
							  gint row);
/* Adds one busy time for the given attendee. */
gboolean e_meeting_time_selector_attendee_add_busy_period (EMeetingTimeSelector *mts,
							   gint row,
							   gint start_year,
							   gint start_month,
							   gint start_day,
							   gint start_hour,
							   gint start_minute,
							   gint end_year,
							   gint end_month,
							   gint end_day,
							   gint end_hour,
							   gint end_minute,
							   EMeetingTimeSelectorBusyType busy_type);



/*
 * INTERNAL ROUTINES - functions to communicate with the canvas items within
 *		       the EMeetingTimeSelector.
 */

/* This returns the x pixel coordinates of the meeting start and end times,
   in the entire canvas scroll area. If it returns FALSE, then the meeting
   time isn't in the current scroll area (which shouldn't really happen). */
gboolean e_meeting_time_selector_get_meeting_time_positions (EMeetingTimeSelector *mts,
							     gint *start_x,
							     gint *end_x);

void e_meeting_time_selector_drag_meeting_time (EMeetingTimeSelector *mts,
						gint x);

void e_meeting_time_selector_remove_timeout (EMeetingTimeSelector *mts);

void e_meeting_time_selector_fix_time_overflows (EMeetingTimeSelectorTime *mtstime);

gint e_meeting_time_selector_find_first_busy_period (EMeetingTimeSelector *mts,
						     EMeetingTimeSelectorAttendee *attendee,
						     GDate *date);

/* Makes sure the busy periods are sorted, so we can do binary searches. */
void e_meeting_time_selector_attendee_ensure_periods_sorted (EMeetingTimeSelector *mts,
							     EMeetingTimeSelectorAttendee *attendee);

void e_meeting_time_selector_calculate_day_and_position (EMeetingTimeSelector *mts,
							 gint x,
							 GDate *date,
							 gint *day_position);
void e_meeting_time_selector_convert_day_position_to_hours_and_mins (EMeetingTimeSelector *mts, gint day_position, guint8 *hours, guint8 *minutes);
void e_meeting_time_selector_calculate_time (EMeetingTimeSelector *mts,
					     gint x,
					     EMeetingTimeSelectorTime *time);
gint e_meeting_time_selector_calculate_time_position (EMeetingTimeSelector *mts,
						      EMeetingTimeSelectorTime *mtstime);

/* Debugging function to dump information on all attendees. */
#ifdef E_MEETING_TIME_SELECTOR_DEBUG
void e_meeting_time_selector_dump (EMeetingTimeSelector *mts);
gchar* e_meeting_time_selector_dump_time (EMeetingTimeSelectorTime *mtstime);
gchar* e_meeting_time_selector_dump_date (GDate *date);
#endif /* E_MEETING_TIME_SELECTOR_DEBUG */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_MEETING_TIME_SELECTOR_H_ */

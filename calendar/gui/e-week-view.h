/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
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
#ifndef _E_WEEK_VIEW_H_
#define _E_WEEK_VIEW_H_

#include <gtk/gtktable.h>
#include <libgnomeui/gnome-canvas.h>

#include "gnome-cal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EWeekView - displays the Week & Month views of the calendar.
 */

/* The maximum number of weeks we show. 5 is usually enough for 1 month. */
#define E_WEEK_VIEW_MAX_WEEKS		5

/* The size of the reminder & recurrence icons, and padding around them. */
#define E_WEEK_VIEW_ICON_WIDTH		16
#define E_WEEK_VIEW_ICON_HEIGHT		16
#define E_WEEK_VIEW_ICON_X_PAD		0
#define E_WEEK_VIEW_ICON_Y_PAD		0

/* The space on the left & right of the event. (The triangle to indicate the
   event continues is displayed in this space). */
#define E_WEEK_VIEW_EVENT_L_PAD		2
#define E_WEEK_VIEW_EVENT_R_PAD		3

/* The vertical spacing between rows of events. */
#define E_WEEK_VIEW_EVENT_Y_SPACING	1

/* The size of the border around the event. */
#define E_WEEK_VIEW_EVENT_BORDER_WIDTH	1
#define E_WEEK_VIEW_EVENT_BORDER_HEIGHT	1

/* The padding on each side of the event text. */
#define E_WEEK_VIEW_EVENT_TEXT_X_PAD	4
#define E_WEEK_VIEW_EVENT_TEXT_Y_PAD	1

/* The space on the right of the time string, if it is shown. */
#define E_WEEK_VIEW_EVENT_TIME_R_PAD	2

/* The padding above and on the right of the date string at the top of each
   cell. */
#define E_WEEK_VIEW_DATE_T_PAD		2
#define E_WEEK_VIEW_DATE_R_PAD		4

/* The padding above and below the line under the date string, in the Week
   view, and also the space on the left of it. */
#define E_WEEK_VIEW_DATE_LINE_T_PAD	1
#define E_WEEK_VIEW_DATE_LINE_B_PAD	1
#define E_WEEK_VIEW_DATE_LINE_L_PAD	10

/* The padding below the date string in the Month view. */
#define E_WEEK_VIEW_DATE_B_PAD		1

/* These index our colors array. */
typedef enum
{
	E_WEEK_VIEW_COLOR_EVEN_MONTHS,
	E_WEEK_VIEW_COLOR_ODD_MONTHS,
	E_WEEK_VIEW_COLOR_EVENT_BACKGROUND,
	E_WEEK_VIEW_COLOR_EVENT_BORDER,
	
	E_WEEK_VIEW_COLOR_LAST
} EWeekViewColors;

/* These specify which part of the selection we are dragging, if any. */
typedef enum
{
	E_WEEK_VIEW_DRAG_NONE,
	E_WEEK_VIEW_DRAG_START,
	E_WEEK_VIEW_DRAG_END
} EWeekViewDragPosition;

/* These specify which times are shown for the 1-day events. We use the small
   font for the minutes if it can be loaded and the option is on. */
typedef enum
{
	E_WEEK_VIEW_TIME_NONE,
	E_WEEK_VIEW_TIME_START,
	E_WEEK_VIEW_TIME_BOTH,
	E_WEEK_VIEW_TIME_START_SMALL_MIN,
	E_WEEK_VIEW_TIME_BOTH_SMALL_MIN
} EWeekViewTimeFormat;

/* Specifies the position of the mouse. */
typedef enum
{
	E_WEEK_VIEW_POS_OUTSIDE,
	E_WEEK_VIEW_POS_NONE,
	E_WEEK_VIEW_POS_EVENT,
	E_WEEK_VIEW_POS_LEFT_EDGE,
	E_WEEK_VIEW_POS_RIGHT_EDGE
} EWeekViewPosition;


typedef struct _EWeekViewEventSpan EWeekViewEventSpan;
struct _EWeekViewEventSpan {
	guint start_day : 6;
	guint num_days : 3;
	guint row : 7;
	GnomeCanvasItem *background_item;
	GnomeCanvasItem *text_item;
};

typedef struct _EWeekViewEvent EWeekViewEvent;
struct _EWeekViewEvent {
	iCalObject *ico;
	time_t start;
	time_t end;
	guint16 start_minute;	/* Minutes from the start of the day. */
	guint16 end_minute;
	gint spans_index;
	guint num_spans;
};


#define E_WEEK_VIEW(obj)          GTK_CHECK_CAST (obj, e_week_view_get_type (), EWeekView)
#define E_WEEK_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_week_view_get_type (), EWeekViewClass)
#define E_IS_WEEK_VIEW(obj)       GTK_CHECK_TYPE (obj, e_week_view_get_type ())


typedef struct _EWeekView       EWeekView;
typedef struct _EWeekViewClass  EWeekViewClass;

struct _EWeekView
{
	GtkTable table;

	/* The top canvas where the dates are shown. */
	GtkWidget *titles_canvas;
	GnomeCanvasItem *titles_canvas_item;

	/* The main canvas where the appointments are shown. */
	GtkWidget *main_canvas;
	GnomeCanvasItem *main_canvas_item;

	GtkWidget *vscrollbar;

	/* The calendar we are associated with. */
	GnomeCalendar *calendar;

	/* The array of EWeekViewEvent elements. */
	GArray *events;
	gboolean events_sorted;
	gboolean events_need_layout;
	gboolean events_need_reshape;

	/* An array of EWeekViewEventSpan elements. Each event has its own
	   space within this array, and uses the spans_index and num_spans
	   fields of the EWeekViewEvent struct to access it. */
	GArray *spans;

	/* The start of each day displayed. */
	time_t day_starts[E_WEEK_VIEW_MAX_WEEKS * 7 + 1];

	/* The base date, where the adjustment value is 0. */
	GDate base_date;

	/* The first day shown in the view. */
	GDate first_day_shown;

	/* If we are displaying 1 week or 1 month. */
	gboolean display_month;

	/* If Sat & Sun are compressed. Only applicable in month view, since
	   they are always compressed into 1 cell in the week view. */
	gboolean compress_weekend;

	/* The vertical offset of the events from the top of the cells. */
	gint events_y_offset;

	/* The height of the events, not including spacing between them. */
	gint row_height;

	/* The number of rows of events in each cell. */
	gint rows_per_cell;
	gint rows_per_compressed_cell;

	/* If the small font is used for displaying the minutes. */
	gboolean use_small_font;

	/* Small font to display the minutes. */
	GdkFont *small_font;

	/* The widths of various pieces of text, used to determine which of
	   several date formats to display, set in e_week_view_style_set(). */
	gint space_width;		/* One space character ' '. */
	gint colon_width;		/* Size of ':' in the font. */
	gint slash_width;		/* Size of '/' in the font. */
	gint digit_width;		/* Size of a '0' digit. */
	gint small_digit_width;		/* Size of a small_font '0' digit. */
	gint day_widths[7];		/* Monday first. */
	gint max_day_width;
	gint abbr_day_widths[7];
	gint max_abbr_day_width;
	gint month_widths[12];
	gint max_month_width;
	gint abbr_month_widths[12];
	gint max_abbr_month_width;

	/* The size of the main grid of days and of the cells. Note that the
	   offsets arrays have one more element than the widths/heights arrays
	   since they also contain the right/bottom edge. */
	gint rows;
	gint columns;
	gint col_widths[7];
	gint col_offsets[8];
	gint row_heights[10];
	gint row_offsets[11];

	/* This specifies which times we are showing for the events, depending
	   on how much room is available. */
	EWeekViewTimeFormat time_format;

	/* The GC used for painting in different colors. */
	GdkGC *main_gc;

	/* The icons. */
	GdkPixmap *reminder_icon;
	GdkBitmap *reminder_mask;
	GdkPixmap *recurrence_icon;
	GdkBitmap *recurrence_mask;

	/* Colors for drawing. */
	GdkColor colors[E_WEEK_VIEW_COLOR_LAST];

	/* The normal & resizing cursors. */
	GdkCursor *normal_cursor;
	GdkCursor *move_cursor;
	GdkCursor *resize_width_cursor;

	/* This remembers the last cursor set on the window. */
	GdkCursor *last_cursor_set;

	/* The currently selected region, in days from the first day shown.
	   If selection_start_day is -1 there is no current selection. */
	gint selection_start_day;
	gint selection_end_day;

	/* This specifies which end of the selection is being dragged, or is
	   E_WEEK_VIEW_DRAG_NONE if the selection isn't being dragged. */
	EWeekViewDragPosition selection_drag_pos;

	/* This is the event the mouse button was pressed on. If the button
	   is released we start editing it, but if the mouse is dragged we set
	   this to -1. */
	gint pressed_event_num;
	gint pressed_span_num;

	/* The event span currently being edited. The num is -1 if no event is
	   being edited. */
	gint editing_event_num;
	gint editing_span_num;

	/* This is TRUE if we are editing an event which we have just created.
	   We ignore the "update_event" callback which we will get from the
	   server when the event is added. */
	gboolean editing_new_event;

	/* The event that the context menu is for. */
	gint popup_event_num;

	/* The last mouse position when dragging, in the entire canvas. */
	gint drag_event_x;
	gint drag_event_y;
};

struct _EWeekViewClass
{
	GtkTableClass parent_class;
};


GtkType	   e_week_view_get_type			(void);
GtkWidget* e_week_view_new			(void);

void       e_week_view_set_calendar		(EWeekView	*week_view,
						 GnomeCalendar	*calendar);

/* This sets the selected time range. The EWeekView will show the corresponding
   month and the days between start_time and end_time will be selected.
   To select a single day, use the same value for start_time & end_time. */
void	   e_week_view_set_selected_time_range	(EWeekView	*week_view,
						 time_t		 start_time,
						 time_t		 end_time);

/* Whether to display 1 week or 1 month (5 weeks). It defaults to 1 week. */
gboolean   e_week_view_get_display_month	(EWeekView	*week_view);
void       e_week_view_set_display_month	(EWeekView	*week_view,
						 gboolean	 display_month);

/* Whether the weekend (Sat/Sun) should be compressed into 1 cell in the Month
   view. In the Week view they are always compressed. */
gboolean   e_week_view_get_compress_weekend	(EWeekView	*week_view);
void       e_week_view_set_compress_weekend	(EWeekView	*week_view,
						 gboolean	 compress);

/* This reloads all calendar events. */
void       e_week_view_update_all_events	(EWeekView	*week_view);

/* This is called when one event has been added or updated. */
void       e_week_view_update_event		(EWeekView	*week_view,
						 const gchar	*uid);

/* This removes all the events associated with the given uid. Note that for
   recurring events there may be more than one. If any events are found and
   removed we need to layout the events again. */
void	   e_week_view_remove_event		(EWeekView	*week_view,
						 const gchar	*uid);


/*
 * Internal functions called by the associated canvas items.
 */
void       e_week_view_get_day_position		(EWeekView	*week_view,
						 gint		 day,
						 gint		*day_x,
						 gint		*day_y,
						 gint		*day_w,
						 gint		*day_h);
gboolean   e_week_view_get_span_position	(EWeekView	*week_view,
						 gint		 event_num,
						 gint		 span_num,
						 gint		*span_x,
						 gint		*span_y,
						 gint		*span_w);
gboolean   e_week_view_is_one_day_event		(EWeekView	*week_view,
						 gint		 event_num);
void	   e_week_view_start_editing_event	(EWeekView	*week_view,
						 gint		 event_num,
						 gint		 span_num,
						 gchar		*initial_text);
void	   e_week_view_stop_editing_event	(EWeekView	*week_view);

void	   e_week_view_show_popup_menu		(EWeekView	*week_view,
						 GdkEventButton *event,
						 gint		 event_num);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_WEEK_VIEW_H_ */

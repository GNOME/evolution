/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 2001, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_WEEK_VIEW_H_
#define _E_WEEK_VIEW_H_

#include <gtk/gtktable.h>
#include <libgnomecanvas/gnome-canvas.h>

#include "e-calendar-view.h"
#include "gnome-cal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EWeekView - displays the Week & Month views of the calendar.
 */

/* The maximum number of weeks we show. 5 is usually enough for 1 month,
   but we allow 6 for longer selections. */
#define E_WEEK_VIEW_MAX_WEEKS		6

/* The size of the reminder & recurrence icons, and padding around them.
   X_PAD is the padding between icons. R_PAD is the padding on the right of
   the last icon, before the event text. */
#define E_WEEK_VIEW_ICON_WIDTH		16
#define E_WEEK_VIEW_ICON_HEIGHT		16
#define E_WEEK_VIEW_ICON_X_PAD		1
#define E_WEEK_VIEW_ICON_Y_PAD		1
#define E_WEEK_VIEW_ICON_R_PAD		8

/* The space on the left & right outside of the event. (The triangle to
   indicate the event continues is displayed in this space). */
#define E_WEEK_VIEW_EVENT_L_PAD		2
#define E_WEEK_VIEW_EVENT_R_PAD		2

/* The vertical spacing between rows of events. */
#define E_WEEK_VIEW_EVENT_Y_SPACING	1

/* The size of the border around long events. */
#define E_WEEK_VIEW_EVENT_BORDER_WIDTH	1
#define E_WEEK_VIEW_EVENT_BORDER_HEIGHT	1

/* The padding on the top and bottom of the event text. */
#define E_WEEK_VIEW_EVENT_TEXT_Y_PAD	1

/* The space between the start and end times. */
#define E_WEEK_VIEW_EVENT_TIME_SPACING	2

/* The space between the time and the event text or icons. */
#define E_WEEK_VIEW_EVENT_TIME_X_PAD	8

/* The space between the borders of long events and any text of icons. */
#define E_WEEK_VIEW_EVENT_EDGE_X_PAD	2

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

/* We use a 7-bit field to store row numbers in EWeekViewEventSpan, so the
   maximum number or rows we can allow is 127. It is very unlikely to be
   reached anyway. */
#define E_WEEK_VIEW_MAX_ROWS_PER_CELL	127

/* These index our colors array. */
typedef enum
{
	E_WEEK_VIEW_COLOR_EVEN_MONTHS,
	E_WEEK_VIEW_COLOR_ODD_MONTHS,
	E_WEEK_VIEW_COLOR_EVENT_BACKGROUND,
	E_WEEK_VIEW_COLOR_EVENT_BORDER,
	E_WEEK_VIEW_COLOR_EVENT_TEXT,
	E_WEEK_VIEW_COLOR_GRID,
	E_WEEK_VIEW_COLOR_SELECTED,
	E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED,
	E_WEEK_VIEW_COLOR_DATES,
	E_WEEK_VIEW_COLOR_DATES_SELECTED,
	E_WEEK_VIEW_COLOR_TODAY,
	
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
	E_CALENDAR_VIEW_EVENT_FIELDS
	gint spans_index;
	guint8 num_spans;
};


#define E_WEEK_VIEW(obj)          GTK_CHECK_CAST (obj, e_week_view_get_type (), EWeekView)
#define E_WEEK_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_week_view_get_type (), EWeekViewClass)
#define E_IS_WEEK_VIEW(obj)       GTK_CHECK_TYPE (obj, e_week_view_get_type ())


typedef struct _EWeekView       EWeekView;
typedef struct _EWeekViewClass  EWeekViewClass;

struct _EWeekView
{
	ECalendarView cal_view;

	/* The top canvas where the dates are shown. */
	GtkWidget *titles_canvas;
	GnomeCanvasItem *titles_canvas_item;

	/* The main canvas where the appointments are shown. */
	GtkWidget *main_canvas;
	GnomeCanvasItem *main_canvas_item;

	GnomeCanvasItem *jump_buttons[E_WEEK_VIEW_MAX_WEEKS * 7];
	gint focused_jump_button;

	GtkWidget *vscrollbar;

	/* The query object */
	ECalView *query;

	/* The array of EWeekViewEvent elements. */
	GArray *events;
	gboolean events_sorted;
	gboolean events_need_layout;
	gboolean events_need_reshape;

	/* The ID of the timeout function for doing a new layout. */
	gint layout_timeout_id;

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

	/* If we are displaying multiple weeks in rows. If this is FALSE only
	   one week is shown, with a different layout. */
	gboolean multi_week_view;

	gboolean update_base_date;
	
	/* How many weeks we are showing. This is only relevant if
	   display_month is TRUE. */
	gint weeks_shown;

	/* If Sat & Sun are compressed. Only applicable in month view, since
	   they are always compressed into 1 cell in the week view. */
	gboolean compress_weekend;

	/* Whether we use show event end times. */
	gboolean show_event_end_times;

	/* The first day of the week, 0 (Monday) to 6 (Sunday). */
	gint week_start_day;

	/* The first day of the week we display, 0 (Monday) to 6 (Sunday).
	   This will usually be week_start_day, but if the weekend is
	   compressed, and week_start_day is Sunday we have to use Saturday. */
	gint display_start_day;

	/* The vertical offset of the events from the top of the cells. */
	gint events_y_offset;

	/* The height of the events, not including spacing between them. */
	gint row_height;

	/* The number of rows of events in each cell. */
	gint rows_per_cell;
	gint rows_per_compressed_cell;

	/* The number of rows we have used for each day (i.e. each cell) */
	gint rows_per_day[E_WEEK_VIEW_MAX_WEEKS * 7];

	/* If the small font is used for displaying the minutes. */
	gboolean use_small_font;

	/* Small font to display the minutes. */
	PangoFontDescription *small_font_desc;

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

	/* The size of the main grid of days and of the cells. A row
	   corresponds to a compressed day, so normal days usually take
	   up 2 rows. Note that the offsets arrays have one more element
	   than the widths/heights arrays since they also contain the
	   right/bottom edge. */
	gint rows;
	gint columns;
	gint col_widths[7];
	gint col_offsets[8];
	gint row_heights[E_WEEK_VIEW_MAX_WEEKS * 2];
	gint row_offsets[E_WEEK_VIEW_MAX_WEEKS * 2 + 1];

	/* This specifies which times we are showing for the events, depending
	   on how much room is available. */
	EWeekViewTimeFormat time_format;

	/* The GC used for painting in different colors. */
	GdkGC *main_gc;

	/* The icons. */
	GdkPixbuf *reminder_icon;
	GdkPixbuf *recurrence_icon;
	GdkPixbuf *timezone_icon;

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

	/* This is used to remember the last edited event. */
	gchar *last_edited_comp_string;

	/* The event that the context menu is for. */
	gint popup_event_num;

	/* The last mouse position when dragging, in the entire canvas. */
	gint drag_event_x;
	gint drag_event_y;

	/* "am" and "pm" in the current locale, and their widths. */
	gchar *am_string;
	gchar *pm_string;
	gint am_string_width;
	gint pm_string_width;
};

struct _EWeekViewClass
{
	ECalendarViewClass parent_class;
};


GtkType	   e_week_view_get_type			(void);
GtkWidget* e_week_view_new			(void);

/* The first day shown. Note that it will be rounded down to the start of a
   week when set. The returned value will be invalid if no date has been set
   yet. */
void	   e_week_view_get_first_day_shown	(EWeekView	*week_view,
						 GDate		*date);
void	   e_week_view_set_first_day_shown	(EWeekView	*week_view,
						 GDate		*date);

/* The selected time range. The EWeekView will show the corresponding
   month and the days between start_time and end_time will be selected.
   To select a single day, use the same value for start_time & end_time. */
void       e_week_view_set_selected_time_range_visible	(EWeekView	*week_view,
							 time_t		 start_time,
							 time_t		 end_time);

/* Whether to display 1 week or 1 month (5 weeks). It defaults to 1 week. */
gboolean   e_week_view_get_multi_week_view	(EWeekView	*week_view);
void       e_week_view_set_multi_week_view	(EWeekView	*week_view,
						 gboolean	 multi_week_view);

/* Whether to update the base date when the time range changes */
gboolean e_week_view_get_update_base_date (EWeekView *week_view);
void e_week_view_set_update_base_date (EWeekView *week_view, gboolean update_base_date);

/* The number of weeks shown in the multi-week view. */
gint	   e_week_view_get_weeks_shown		(EWeekView	*week_view);
void	   e_week_view_set_weeks_shown		(EWeekView	*week_view,
						 gint		 weeks_shown);

/* Whether the weekend (Sat/Sun) should be compressed into 1 cell in the Month
   view. In the Week view they are always compressed. */
gboolean   e_week_view_get_compress_weekend	(EWeekView	*week_view);
void       e_week_view_set_compress_weekend	(EWeekView	*week_view,
						 gboolean	 compress);

/* Whether we display event end times. */
gboolean   e_week_view_get_show_event_end_times	(EWeekView	*week_view);
void	   e_week_view_set_show_event_end_times	(EWeekView	*week_view,
						 gboolean	 show);

/* The first day of the week, 0 (Monday) to 6 (Sunday). */
gint	   e_week_view_get_week_start_day	(EWeekView	*week_view);
void	   e_week_view_set_week_start_day	(EWeekView	*week_view,
						 gint		 week_start_day);

void       e_week_view_delete_occurrence        (EWeekView      *week_view);

/* Returns the number of selected events (0 or 1 at present). */
gint	   e_week_view_get_num_events_selected	(EWeekView	*week_view);

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
gboolean   e_week_view_start_editing_event	(EWeekView	*week_view,
						 gint		 event_num,
						 gint		 span_num,
						 gchar		*initial_text);
void	   e_week_view_stop_editing_event	(EWeekView	*week_view);

void	   e_week_view_show_popup_menu		(EWeekView	*week_view,
						 GdkEventButton *event,
						 gint		 event_num);

void	   e_week_view_convert_time_to_display	(EWeekView	*week_view,
						 gint		 hour,
						 gint		*display_hour,
						 gchar	       **suffix,
						 gint		*suffix_width);
gint	   e_week_view_get_time_string_width	(EWeekView	*week_view);

gint	   e_week_view_event_sort_func		(const void	*arg1,
						 const void	*arg2);

gboolean e_week_view_find_event_from_item (EWeekView	  *week_view,
 					   GnomeCanvasItem *item,
 					   gint		  *event_num_return,
 					   gint		  *span_num_return);

gboolean e_week_view_is_jump_button_visible (EWeekView *week_view,
					     gint day);
void e_week_view_jump_to_button_item (EWeekView *week_view, GnomeCanvasItem *item);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_WEEK_VIEW_H_ */

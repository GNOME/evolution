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
#ifndef _E_DAY_VIEW_H_
#define _E_DAY_VIEW_H_

#include <time.h>
#include <gtk/gtktable.h>
#include <libgnomeui/gnome-canvas.h>

#include "gnome-cal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EDayView - displays the Day & Work-Week views of the calendar.
 */

/* The maximum number of days shown. We use 7 since we only show 1 week max. */
#define E_DAY_VIEW_MAX_DAYS		7

/* This is used as a special code to signify a long event instead of the day
   of a normal event. */
#define E_DAY_VIEW_LONG_EVENT		E_DAY_VIEW_MAX_DAYS

/* The maximum number of columns of appointments within a day. */
#define E_DAY_VIEW_MAX_COLUMNS		6

/* The width of the gap between appointments. This should be at least
   E_DAY_VIEW_BAR_WIDTH. */
#define E_DAY_VIEW_GAP_WIDTH		6

/* The width of the bars down the left of each column and appointment.
   This includes the borders on each side of it. */
#define E_DAY_VIEW_BAR_WIDTH		6

/* The height of the horizontal bar above & beneath the selected event.
   This includes the borders on the top and bottom. */
#define E_DAY_VIEW_BAR_HEIGHT		6

/* The size of the reminder & recurrence icons, and padding around them. */
#define E_DAY_VIEW_ICON_WIDTH		16
#define E_DAY_VIEW_ICON_HEIGHT		16
#define E_DAY_VIEW_ICON_X_PAD		0
#define E_DAY_VIEW_ICON_Y_PAD		0

/* The size of the border around the event. */
#define E_DAY_VIEW_EVENT_BORDER_WIDTH	1
#define E_DAY_VIEW_EVENT_BORDER_HEIGHT	1

/* The padding on each side of the event text. */
#define E_DAY_VIEW_EVENT_X_PAD		2
#define E_DAY_VIEW_EVENT_Y_PAD		1

/* The padding on each side of the event text for events in the top canvas. */
#define E_DAY_VIEW_LONG_EVENT_X_PAD	2
#define E_DAY_VIEW_LONG_EVENT_Y_PAD	2

/* The size of the border around the long events in the top canvas. */
#define E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH	1
#define E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT	1

/* The space between the time and the icon/text in the top canvas. */
#define E_DAY_VIEW_LONG_EVENT_TIME_X_PAD	2

/* The gap between rows in the top canvas. */
#define E_DAY_VIEW_TOP_CANVAS_Y_GAP	2


/* These are used to specify the type of an appointment. They match those
   used in EMeetingTimeSelector. */
typedef enum
{
	E_DAY_VIEW_BUSY_TENTATIVE	= 0,
	E_DAY_VIEW_BUSY_OUT_OF_OFFICE	= 1,
	E_DAY_VIEW_BUSY_BUSY		= 2,

	E_DAY_VIEW_BUSY_LAST		= 3
} EDayViewBusyType;

/* This is used to specify the format used when displaying the dates.
   The full format is like 'September 12'. The abbreviated format is like
   'Sep 12'. The short format is like '12'. The actual format used is
   determined in style_set(), once we know the font being used. */
typedef enum
{
	E_DAY_VIEW_DATE_FULL,
	E_DAY_VIEW_DATE_ABBREVIATED,
	E_DAY_VIEW_DATE_SHORT
} EDayViewDateFormat;

/* These index our colors array. */
typedef enum
{
	E_DAY_VIEW_COLOR_BG_WORKING,
	E_DAY_VIEW_COLOR_BG_NOT_WORKING,
	E_DAY_VIEW_COLOR_EVENT_VBAR,

	E_DAY_VIEW_COLOR_EVENT_BACKGROUND,
	E_DAY_VIEW_COLOR_EVENT_BORDER,
	
	E_DAY_VIEW_COLOR_LAST
} EDayViewColors;

/* These specify which part of the selection we are dragging, if any. */
typedef enum
{
	E_DAY_VIEW_DRAG_NONE,
	E_DAY_VIEW_DRAG_START,
	E_DAY_VIEW_DRAG_END
} EDayViewDragPosition;

/* Specifies the position of the mouse. */
typedef enum
{
	E_DAY_VIEW_POS_OUTSIDE,
	E_DAY_VIEW_POS_NONE,
	E_DAY_VIEW_POS_EVENT,
	E_DAY_VIEW_POS_LEFT_EDGE,
	E_DAY_VIEW_POS_RIGHT_EDGE,
	E_DAY_VIEW_POS_TOP_EDGE,
	E_DAY_VIEW_POS_BOTTOM_EDGE
} EDayViewPosition;

typedef struct _EDayViewEvent EDayViewEvent;
struct _EDayViewEvent {
	iCalObject *ico;
	time_t start;
	time_t end;
	guint8 start_row_or_col;/* The start column for normal events, or the
				   start row for long events. */
	guint8 num_columns;	/* 0 indicates not displayed. For long events
				   this is just 1 if the event is shown. */
	guint16 start_minute;	/* Offsets from the start of the display. */
	guint16 end_minute;
	GnomeCanvasItem *canvas_item;
};


#define E_DAY_VIEW(obj)          GTK_CHECK_CAST (obj, e_day_view_get_type (), EDayView)
#define E_DAY_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_day_view_get_type (), EDayViewClass)
#define E_IS_DAY_VIEW(obj)       GTK_CHECK_TYPE (obj, e_day_view_get_type ())


typedef struct _EDayView       EDayView;
typedef struct _EDayViewClass  EDayViewClass;

struct _EDayView
{
	GtkTable table;

	/* The top canvas where the dates and long appointments are shown. */
	GtkWidget *top_canvas;
	GnomeCanvasItem *top_canvas_item;

	/* The main canvas where the rest of the appointments are shown. */
	GtkWidget *main_canvas;
	GnomeCanvasItem *main_canvas_item;

	/* The canvas displaying the times of the day. */
	GtkWidget *time_canvas;
	GnomeCanvasItem *time_canvas_item;

	GtkWidget *vscrollbar;

	/* The calendar we are associated with. */
	GnomeCalendar *calendar;

	/* The start and end of the day shown. */
	time_t lower;
	time_t upper;

	/* The number of days we are showing. Usually 1 or 5. Maybe 6 or 7. */
	gint days_shown;

	/* The start of each day & an extra element to hold the last time. */
	time_t day_starts[E_DAY_VIEW_MAX_DAYS + 1];


	/* An array of EDayViewEvent elements for the top view and each day. */
	GArray *long_events;
	GArray *events[E_DAY_VIEW_MAX_DAYS];

	/* These are set to FALSE whenever an event in the corresponding array
	   is changed. Any function that needs the events sorted calls
	   e_day_view_ensure_events_sorted(). */
	gboolean long_events_sorted;
	gboolean events_sorted[E_DAY_VIEW_MAX_DAYS];

	/* This is TRUE if we need to relayout the events before drawing. */
	gboolean long_events_need_layout;
	gboolean need_layout[E_DAY_VIEW_MAX_DAYS];

	/* This is TRUE if we need to reshape the canvas items, but a full
	   layout is not necessary. */
	gboolean long_events_need_reshape;
	gboolean need_reshape[E_DAY_VIEW_MAX_DAYS];

	/* The number of minutes per row. 5, 10, 15, 30 or 60. */
	gint mins_per_row;

	/* The number of rows needed, depending on the times shown and the
	   minutes per row. */
	gint rows;

	/* The height of each row. */
	gint row_height;

	/* The number of rows in the top display. */
	gint rows_in_top_display;

	/* The height of each row in the top canvas. */
	gint top_row_height;

	/* The first and last times shown in the display. The last time isn't
	   included in the range. Default is 0:00-24:00 */
	gint first_hour_shown;
	gint first_minute_shown;
	gint last_hour_shown;
	gint last_minute_shown;

	/* The start and end of the work day, rounded to the nearest row. */
	gint work_day_start_hour;
	gint work_day_start_minute;
	gint work_day_end_hour;
	gint work_day_end_minute;

	/* This is set to TRUE when the widget is created, so it scrolls to
	   the start of the working day when first shown. */
	gboolean scroll_to_work_day;

	/* This is the width & offset of each of the day columns in the
	   display. */
	gint day_widths[E_DAY_VIEW_MAX_DAYS];
	gint day_offsets[E_DAY_VIEW_MAX_DAYS + 1];

	/* An array holding the number of columns in each row, in each day. */
	guint8 cols_per_row[E_DAY_VIEW_MAX_DAYS][12 * 24];

	/* Sizes of the various time strings. */
	gint large_hour_widths[24];
	gint small_hour_widths[24];
	gint minute_widths[12];		/* intervals of 5 minutes. */
	gint max_small_hour_width;
	gint max_large_hour_width;
	gint max_minute_width;
	gint colon_width;

	/* This specifies how we are displaying the dates at the top. */
	EDayViewDateFormat date_format;

	/* These are the maximum widths of the different types of dates. */
	gint long_format_width;
	gint abbreviated_format_width;

	/* The large font use to display the hours. I don't think we need a
	   fontset since we only display numbers. */
	GdkFont *large_font;

	/* The GC used for painting in different colors. */
	GdkGC *main_gc;

	/* The icons. */
	GdkPixmap *reminder_icon;
	GdkBitmap *reminder_mask;
	GdkPixmap *recurrence_icon;
	GdkBitmap *recurrence_mask;

	/* Colors for drawing. */
	GdkColor colors[E_DAY_VIEW_COLOR_LAST];

	/* The normal & resizing cursors. */
	GdkCursor *normal_cursor;
	GdkCursor *move_cursor;
	GdkCursor *resize_width_cursor;
	GdkCursor *resize_height_cursor;

	/* This remembers the last cursor set on the window. */
	GdkCursor *last_cursor_set_in_top_canvas;
	GdkCursor *last_cursor_set_in_main_canvas;

	/*
	 * Editing, Selection & Dragging data
	 */

	/* The horizontal bars to resize events in the main canvas. */
	GnomeCanvasItem *main_canvas_top_resize_bar_item;
	GnomeCanvasItem *main_canvas_bottom_resize_bar_item;

	/* The event currently being edited. The day is -1 if no event is
	 being edited, or E_DAY_VIEW_LONG_EVENT if a long event is edited. */
	gint editing_event_day;
	gint editing_event_num;

	/* This is TRUE if we are editing an event which we have just created.
	   We ignore the "update_event" callback which we will get from the
	   server when the event is added. */
	gboolean editing_new_event;

	/* This is a GnomeCanvasRect which is placed around an item while it
	   is being resized, so we can raise it above all other EText items. */
	GnomeCanvasItem *resize_long_event_rect_item;
	GnomeCanvasItem *resize_rect_item;
	GnomeCanvasItem *resize_bar_item;

	/* The event for which a popup menu is being displayed, as above. */
	gint popup_event_day;
	gint popup_event_num;

	/* The currently selected region. If selection_start_col is -1 there is
	   no current selection. If start_row or end_row is -1 then the
	   selection is in the top canvas. */
	gint selection_start_col;
	gint selection_end_col;
	gint selection_start_row;
	gint selection_end_row;

	/* This specifies which end of the selection is being dragged, or is
	   E_DAY_VIEW_DRAG_NONE if the selection isn't being dragged. */
	EDayViewDragPosition selection_drag_pos;

	/* This is TRUE if the selection is in the top canvas only (i.e. if the
	   last motion event was in the top canvas). */
	gboolean selection_in_top_canvas;

	/* The last mouse position, relative to the main canvas window.
	   Used when auto-scrolling to update the selection. */
	gint last_mouse_x;
	gint last_mouse_y;

	/* Auto-scroll info for when selecting an area or dragging an item. */
	gint auto_scroll_timeout_id;
	gint auto_scroll_delay;
	gboolean auto_scroll_up;

	/* These are used for the resize bars. */
	gint resize_bars_event_day;
	gint resize_bars_event_num;

	/* These are used when resizing events. */
	gint resize_event_day;
	gint resize_event_num;
	EDayViewPosition resize_drag_pos;
	gint resize_start_row;
	gint resize_end_row;

	/* This is the event the mouse button was pressed on. If the button
	   is released we start editing it, but if the mouse is dragged we set
	   this to -1. */
	gint pressed_event_day;
	gint pressed_event_num;

	/* These are used when dragging events. If drag_event_day is not -1 we
	   know that we are dragging one of the EDayView events around. */
	gint drag_event_day;
	gint drag_event_num;

	/* The last mouse position when dragging, in the entire canvas. */
	gint drag_event_x;
	gint drag_event_y;

	/* The offset of the mouse from the top of the event, in rows.
	   In the top canvas this is the offset from the left, in days. */
	gint drag_event_offset;

	/* The last day & row dragged to, so we know when we need to update
	   the dragged event's position. */
	gint drag_last_day;
	gint drag_last_row;

	/* This is a GnomeCanvasRect which is placed around an item while it
	   is being resized, so we can raise it above all other EText items. */
	GnomeCanvasItem *drag_long_event_rect_item;
	GnomeCanvasItem *drag_long_event_item;
	GnomeCanvasItem *drag_rect_item;
	GnomeCanvasItem *drag_bar_item;
	GnomeCanvasItem *drag_item;
};

struct _EDayViewClass
{
	GtkTableClass parent_class;
};


GtkType	   e_day_view_get_type			(void);
GtkWidget* e_day_view_new			(void);

void       e_day_view_set_calendar		(EDayView	*day_view,
						 GnomeCalendar	*calendar);

/* This sets the selected time range. The EDayView will show the day or week
   corresponding to the start time. If the start_time & end_time are not equal
   and are both visible in the view, then the selection is set to those times,
   otherwise it is set to 1 hour from the start of the working day. */
void       e_day_view_set_selected_time_range	(EDayView	*day_view,
						 time_t		 start_time,
						 time_t		 end_time);

/* This reloads all calendar events. */
void       e_day_view_update_all_events		(EDayView	*day_view);

/* This is called when one event has been added or updated. */
void       e_day_view_update_event		(EDayView	*day_view,
						 const gchar	*uid);

/* This removes all the events associated with the given uid. Note that for
   recurring events there may be more than one. If any events are found and
   removed we need to layout the events again. */
void	   e_day_view_remove_event		(EDayView	*day_view,
						 const gchar	*uid);

/* The number of days shown in the EDayView, from 1 to 7. This is normally
   either 1 or 5 (for the Work-Week view). */
gint	   e_day_view_get_days_shown		(EDayView	*day_view);
void	   e_day_view_set_days_shown		(EDayView	*day_view,
						 gint		 days_shown);

/* This specifies how many minutes are represented by one row in the display.
   It can be 60, 30, 15, 10 or 5. The default is 30. */
gint	   e_day_view_get_mins_per_row		(EDayView	*day_view);
void	   e_day_view_set_mins_per_row		(EDayView	*day_view,
						 gint		 mins_per_row);



/*
 * Internal functions called by the associated canvas items.
 */
void	   e_day_view_check_layout		(EDayView	*day_view);
gint	   e_day_view_convert_time_to_row	(EDayView	*day_view,
						 gint		 hour,
						 gint		 minute);
gint	   e_day_view_convert_time_to_position	(EDayView	*day_view,
						 gint		 hour,
						 gint		 minute);
gboolean   e_day_view_get_event_position	(EDayView	*day_view,
						 gint		 day,
						 gint		 event_num,
						 gint		*item_x,
						 gint		*item_y,
						 gint		*item_w,
						 gint		*item_h);
gboolean   e_day_view_get_long_event_position	(EDayView	*day_view,
						 gint		 event_num,
						 gint		*start_day,
						 gint		*end_day,
						 gint		*item_x,
						 gint		*item_y,
						 gint		*item_w,
						 gint		*item_h);
gboolean   e_day_view_find_long_event_days	(EDayView	*day_view,
						 EDayViewEvent	*event,
						 gint		*start_day,
						 gint		*end_day);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_DAY_VIEW_H_ */

/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_DAY_VIEW_H
#define E_DAY_VIEW_H

#include <time.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "e-calendar-view.h"

/*
 * EDayView - displays the Day & Work-Week views of the calendar.
 */

/* Standard GObject macros */
#define E_TYPE_DAY_VIEW \
	(e_day_view_get_type ())
#define E_DAY_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DAY_VIEW, EDayView))
#define E_DAY_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DAY_VIEW, EDayViewClass))
#define E_IS_DAY_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DAY_VIEW))
#define E_IS_DAY_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DAY_VIEW))
#define E_DAY_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DAY_VIEW, EDayViewClass))

/* The maximum number of days shown. We use the week view for anything more
 * than about 9 days. */
#define E_DAY_VIEW_MAX_DAYS		10

/* This is used as a special code to signify a long event instead of the day
 * of a normal event. */
#define E_DAY_VIEW_LONG_EVENT		E_DAY_VIEW_MAX_DAYS

/* The maximum number of columns of appointments within a day in multi-day
 * view. */
#define E_DAY_VIEW_MULTI_DAY_MAX_COLUMNS 6

/* minimum width of the event in one-day view in pixels */
#define E_DAY_VIEW_MIN_DAY_COL_WIDTH	60

/* The width of the gap between appointments. This should be at least
 * E_DAY_VIEW_BAR_WIDTH, since in the top canvas we use this space to draw
 * the triangle to represent continuing events. */
#define E_DAY_VIEW_GAP_WIDTH		7

/* The width of the bars down the left of each column and appointment.
 * This includes the borders on each side of it. */
#define E_DAY_VIEW_BAR_WIDTH		7

/* The height of the horizontal bar above & beneath the selected event.
 * This includes the borders on the top and bottom. */
#define E_DAY_VIEW_BAR_HEIGHT		6

/* The size of the reminder & recurrence icons, and padding around them. */
#define E_DAY_VIEW_ICON_WIDTH		16
#define E_DAY_VIEW_ICON_HEIGHT		16
#define E_DAY_VIEW_ICON_X_PAD		1
#define E_DAY_VIEW_ICON_Y_PAD		1

/* The space between the icons and the long event text. */
#define E_DAY_VIEW_LONG_EVENT_ICON_R_PAD	1

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

G_BEGIN_DECLS

/* These are used to specify the type of an appointment. They match those
 * used in EMeetingTimeSelector. */
typedef enum {
	E_DAY_VIEW_BUSY_TENTATIVE = 0,
	E_DAY_VIEW_BUSY_OUT_OF_OFFICE = 1,
	E_DAY_VIEW_BUSY_BUSY = 2,

	E_DAY_VIEW_BUSY_LAST = 3
} EDayViewBusyType;

/* This is used to specify the format used when displaying the dates.
 * The full format is like 'Thursday 12 September'. The abbreviated format is
 * like 'Thu 12 Sep'. The no weekday format is like '12 Sep'. The short format
 * is like '12'. The actual format used is determined in
 * e_day_view_recalc_cell_sizes (), once we know the font being used. */
typedef enum {
	E_DAY_VIEW_DATE_FULL,
	E_DAY_VIEW_DATE_ABBREVIATED,
	E_DAY_VIEW_DATE_NO_WEEKDAY,
	E_DAY_VIEW_DATE_SHORT
} EDayViewDateFormat;

/* These index our colors array. */
typedef enum {
	E_DAY_VIEW_COLOR_BG_WORKING,
	E_DAY_VIEW_COLOR_BG_NOT_WORKING,
	E_DAY_VIEW_COLOR_BG_SELECTED,
	E_DAY_VIEW_COLOR_BG_SELECTED_UNFOCUSSED,
	E_DAY_VIEW_COLOR_BG_GRID,
	E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY,

	E_DAY_VIEW_COLOR_BG_TOP_CANVAS,
	E_DAY_VIEW_COLOR_BG_TOP_CANVAS_SELECTED,
	E_DAY_VIEW_COLOR_BG_TOP_CANVAS_GRID,

	E_DAY_VIEW_COLOR_EVENT_VBAR,
	E_DAY_VIEW_COLOR_EVENT_BACKGROUND,
	E_DAY_VIEW_COLOR_EVENT_BORDER,

	E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND,
	E_DAY_VIEW_COLOR_LONG_EVENT_BORDER,

	E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE,

	E_DAY_VIEW_COLOR_LAST
} EDayViewColors;

/* These specify which part of the selection we are dragging, if any. */
typedef enum {
	E_DAY_VIEW_DRAG_START,
	E_DAY_VIEW_DRAG_END
} EDayViewDragPosition;

typedef struct _EDayViewEvent EDayViewEvent;
struct _EDayViewEvent {
	E_CALENDAR_VIEW_EVENT_FIELDS

	/* For events in the main canvas, this contains the start column.
	 * For long events in the top canvas, this is its row. */
	guint8 start_row_or_col;

	/* For events in the main canvas, this is the number of columns that
	 * it covers. For long events this is set to 1 if the event is shown.
	 * For both types of events this is set to 0 if the event is not shown,
	 * i.e. it couldn't fit into the display. Currently long events are
	 * always shown as we just increase the height of the top canvas. */
	guint8 num_columns;
};

typedef struct _EDayView EDayView;
typedef struct _EDayViewClass EDayViewClass;
typedef struct _EDayViewPrivate EDayViewPrivate;

struct _EDayView {
	ECalendarView parent;
	EDayViewPrivate *priv;

	/* The top canvas where the dates are shown. */
	GtkWidget *top_dates_canvas;
	GnomeCanvasItem *top_dates_canvas_item;

	/* The top canvas where the dates and long appointments are shown. */
	GtkWidget *top_canvas;
	GnomeCanvasItem *top_canvas_item;

	/* scrollbar for top_canvas */
	GtkWidget *tc_vscrollbar;

	/* horizontal scrollbar for main_canvas */
	GtkWidget *mc_hscrollbar;

	/* The main canvas where the rest of the appointments are shown. */
	GtkWidget *main_canvas;
	GnomeCanvasItem *main_canvas_item;

	/* The canvas displaying the times of the day. */
	GtkWidget *time_canvas;
	GnomeCanvasItem *time_canvas_item;

	GtkWidget *vscrollbar;

	/* label showing week number in upper-left corner */
	GtkWidget *week_number_label;

	/* The start and end of the days shown. */
	time_t lower;
	time_t upper;

	/* The start of each day & an extra element to hold the last time. */
	time_t day_starts[E_DAY_VIEW_MAX_DAYS + 1];

	/* An array of EDayViewEvent elements for the top view and each day. */
	GArray *long_events;
	GArray *events[E_DAY_VIEW_MAX_DAYS];

	/* These are set to FALSE whenever an event in the corresponding array
	 * is changed. Any function that needs the events sorted calls
	 * e_day_view_ensure_events_sorted (). */
	gboolean long_events_sorted;
	gboolean events_sorted[E_DAY_VIEW_MAX_DAYS];

	/* This is TRUE if we need to relayout the events before drawing. */
	gboolean long_events_need_layout;
	gboolean need_layout[E_DAY_VIEW_MAX_DAYS];

	/* This is TRUE if we need to reshape the canvas items, but a full
	 * layout is not necessary. */
	gboolean long_events_need_reshape;
	gboolean need_reshape[E_DAY_VIEW_MAX_DAYS];

	/* The ID of the timeout function for doing a new layout. */
	gint layout_timeout_id;

	/* The number of rows needed, depending on the times shown and the
	 * minutes per row. */
	gint rows;

	/* The height of each row. */
	gint row_height;

	/* The number of rows in the top display. */
	gint rows_in_top_display;

	/* The height of each row in the top canvas. */
	gint top_row_height;

	/* The first and last times shown in the display. The last time isn't
	 * included in the range. Default is 0:00-24:00 */
	gint first_hour_shown;
	gint first_minute_shown;
	gint last_hour_shown;
	gint last_minute_shown;

	/* Whether we use show event end times in the main canvas. */
	gboolean show_event_end_times;

	/* This is set to TRUE when the widget is created, so it scrolls to
	 * the start of the working day when first shown. */
	gboolean scroll_to_work_day;

	/* This is the width & offset of each of the day columns in the
	 * display. */
	gint day_widths[E_DAY_VIEW_MAX_DAYS + 1];
	gint day_offsets[E_DAY_VIEW_MAX_DAYS + 1];

	/* An array holding the number of columns in each row, in each day.
	 * Note that there are a maximum of 12 * 24 rows (when a row is 5 mins)
	 * but we don't always have that many rows. */
	guint8 cols_per_row[E_DAY_VIEW_MAX_DAYS][12 * 24];
	/* The maximum number of columns from all rows in cols_per_row */
	gint max_cols;

	/* Sizes of the various time strings. */
	gint small_hour_widths[24];
	gint max_small_hour_width;
	gint max_minute_width;
	gint colon_width;
	gint digit_width;	/* Size of '0' character. */

	/* This specifies how we are displaying the dates at the top. */
	EDayViewDateFormat date_format;

	/* These are the longest month & weekday names in the current font.
	 * Months are 0 to 11. Weekdays are 0 (Sun) to 6 (Sat). */
	gint longest_month_name;
	gint longest_abbreviated_month_name;
	gint longest_weekday_name;
	gint longest_abbreviated_weekday_name;

	/* The large font used to display the hours. I don't think we need a
	 * fontset since we only display numbers. */
	PangoFontDescription *large_font_desc;
	PangoFontDescription *small_font_desc;

	/* The icons. */
	GdkPixbuf *reminder_icon;
	GdkPixbuf *recurrence_icon;
	GdkPixbuf *detached_recurrence_icon;
	GdkPixbuf *timezone_icon;
	GdkPixbuf *meeting_icon;
	GdkPixbuf *attach_icon;

	/* Colors for drawing. */
	GdkRGBA colors[E_DAY_VIEW_COLOR_LAST];

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

	/* This is a GnomeCanvasRect which is placed around an item while it
	 * is being resized, so we can raise it above all other EText items. */
	GnomeCanvasItem *resize_long_event_rect_item;
	GnomeCanvasItem *resize_rect_item;
	GnomeCanvasItem *resize_bar_item;

	/* The event for which a popup menu is being displayed, as above. */
	gint popup_event_day;
	gint popup_event_num;

	/* The currently selected region. If selection_start_day is -1 there is
	 * no current selection. If start_row or end_row is -1 then the
	 * selection is in the top canvas. */
	gint selection_start_day;
	gint selection_end_day;
	gint selection_start_row;
	gint selection_end_row;

	/* This is TRUE if the selection is currently being dragged using the
	 * mouse. */
	gboolean selection_is_being_dragged;

	/* This specifies which end of the selection is being dragged. */
	EDayViewDragPosition selection_drag_pos;

	/* This is TRUE if the selection is in the top canvas only (i.e. if the
	 * last motion event was in the top canvas). */
	gboolean selection_in_top_canvas;

	/* The last mouse position, relative to the main canvas window.
	 * Used when auto-scrolling to update the selection. */
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
	ECalendarViewPosition resize_drag_pos;
	gint resize_start_row;
	gint resize_end_row;

	/* This is used to remember the last edited event. */
	gchar *last_edited_comp_string;

	/* This is the event the mouse button was pressed on. If the button
	 * is released we start editing it, but if the mouse is dragged we set
	 * this to -1. */
	gint pressed_event_day;
	gint pressed_event_num;

	/* These are used when dragging events. If drag_event_day is not -1 we
	 * know that we are dragging one of the EDayView events around. */
	gint drag_event_day;
	gint drag_event_num;

	/* The last mouse position when dragging, in the entire canvas. */
	gint drag_event_x;
	gint drag_event_y;

	/* The offset of the mouse from the top of the event, in rows.
	 * In the top canvas this is the offset from the left, in days. */
	gint drag_event_offset;

	/* The last day & row dragged to, so we know when we need to update
	 * the dragged event's position. */
	gint drag_last_day;
	gint drag_last_row;

	/* This is a GnomeCanvasRect which is placed around an item while it
	 * is being resized, so we can raise it above all other EText items. */
	GnomeCanvasItem *drag_long_event_rect_item;
	GnomeCanvasItem *drag_long_event_item;
	GnomeCanvasItem *drag_rect_item;
	GnomeCanvasItem *drag_bar_item;
	GnomeCanvasItem *drag_item;

	/* Grabbed pointer device while dragging. */
	GdkDevice *grabbed_pointer;

	/* "am" and "pm" in the current locale, and their widths. */
	gchar *am_string;
	gchar *pm_string;
	gint am_string_width;
	gint pm_string_width;

	/* remember last selected interval when click and restore on double click,
	 * if we double clicked inside that interval. */
	guint32 bc_event_time;
	time_t before_click_dtstart;
	time_t before_click_dtend;

	gboolean requires_update;
};

struct _EDayViewClass {
	ECalendarViewClass parent_class;
};

GType		e_day_view_get_type		(void) G_GNUC_CONST;
ECalendarView *	e_day_view_new			(ECalModel *model);

gboolean	e_day_view_get_draw_flat_events	(EDayView *day_view);
void		e_day_view_set_draw_flat_events	(EDayView *day_view,
						 gboolean draw_flat_events);

/* Whether we are displaying a work-week, in which case the display always
 * starts on the first day of the working week. */
gboolean	e_day_view_get_work_week_view	(EDayView *day_view);
void		e_day_view_set_work_week_view	(EDayView *day_view,
						 gboolean  work_week_view);

/* The number of days shown in the EDayView, from 1 to 7. This is normally
 * either 1 or 5 (for the Work-Week view). */
gint		e_day_view_get_days_shown	(EDayView *day_view);
void		e_day_view_set_days_shown	(EDayView *day_view,
						 gint days_shown);

/* Whether we display the Marcus Bains Line in the main canvas and time
 * canvas. */
void		e_day_view_marcus_bains_update	(EDayView *day_view);
gboolean	e_day_view_marcus_bains_get_show_line
						(EDayView *day_view);
void		e_day_view_marcus_bains_set_show_line
						(EDayView *day_view,
						 gboolean show_line);
const gchar *	e_day_view_marcus_bains_get_day_view_color
						(EDayView *day_view);
void		e_day_view_marcus_bains_set_day_view_color
						(EDayView *day_view,
						 const gchar *day_view_color);
const gchar *	e_day_view_marcus_bains_get_time_bar_color
						(EDayView *day_view);
void		e_day_view_marcus_bains_set_time_bar_color
						(EDayView *day_view,
						 const gchar *time_bar_color);
const gchar *	e_day_view_get_today_background_color
						(EDayView *day_view);
void		e_day_view_set_today_background_color
						(EDayView *day_view,
						 const gchar *color);

/* Whether we display event end times in the main canvas. */
gboolean	e_day_view_get_show_event_end_times
						(EDayView *day_view);
void		e_day_view_set_show_event_end_times
						(EDayView *day_view,
						 gboolean show);

void		e_day_view_delete_occurrence	(EDayView *day_view);

/* Returns the number of selected events (0 or 1 at present). */
gint		e_day_view_get_num_events_selected
						(EDayView *day_view);

/*
 * Internal functions called by the associated canvas items.
 */
void		e_day_view_check_layout		(EDayView *day_view);
gint		e_day_view_convert_time_to_row	(EDayView *day_view,
						 gint hour,
						 gint minute);
gint		e_day_view_convert_time_to_position
						(EDayView *day_view,
						 gint hour,
						 gint minute);
gboolean	e_day_view_get_event_rows	(EDayView *day_view,
						 gint day,
						 gint event_num,
						 gint *start_row_out,
						 gint *end_row_out);
gboolean	e_day_view_get_event_position	(EDayView *day_view,
						 gint day,
						 gint event_num,
						 gint *item_x,
						 gint *item_y,
						 gint *item_w,
						 gint *item_h);
gboolean	e_day_view_get_long_event_position
						(EDayView *day_view,
						 gint event_num,
						 gint *start_day,
						 gint *end_day,
						 gint *item_x,
						 gint *item_y,
						 gint *item_w,
						 gint *item_h);

void		e_day_view_start_selection	(EDayView *day_view,
						 gint day,
						 gint row);
void		e_day_view_update_selection	(EDayView *day_view,
						 gint day,
						 gint row);
void		e_day_view_finish_selection	(EDayView *day_view);

void		e_day_view_check_auto_scroll	(EDayView *day_view,
						 gint event_x,
						 gint event_y);
void		e_day_view_stop_auto_scroll	(EDayView *day_view);

void		e_day_view_convert_time_to_display
						(EDayView *day_view,
						 gint hour,
						 gint *display_hour,
						 const gchar **suffix,
						 gint *suffix_width);
gint		e_day_view_get_time_string_width
						(EDayView *day_view);

gint		e_day_view_event_sort_func	(gconstpointer arg1,
						 gconstpointer arg2);

gboolean	e_day_view_find_event_from_item	(EDayView *day_view,
						 GnomeCanvasItem *item,
						 gint *day_return,
						 gint *event_num_return);
void		e_day_view_update_calendar_selection_time
						(EDayView *day_view);
void		e_day_view_ensure_rows_visible	(EDayView *day_view,
						 gint start_row,
						 gint end_row);

gboolean	e_day_view_is_editing		(EDayView *day_view);

void		e_day_view_update_timezone_name_labels
						(EDayView *day_view);

G_END_DECLS

#endif /* E_DAY_VIEW_H */

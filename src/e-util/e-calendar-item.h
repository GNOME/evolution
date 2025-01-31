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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CALENDAR_ITEM_H_
#define _E_CALENDAR_ITEM_H_

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/*
 * ECalendarItem - canvas item displaying a calendar.
 */

#define	E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME	1
#define	E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME	1

/* The number of rows & columns of days in each month. */
#define E_CALENDAR_ROWS_PER_MONTH	6
#define E_CALENDAR_COLS_PER_MONTH	7

/* Used to mark days as bold in e_calendar_item_mark_day(). */
#define E_CALENDAR_ITEM_MARK_BOLD	(1 << 0)
#define E_CALENDAR_ITEM_MARK_ITALIC     (1 << 1)

/*
 * These are the padding sizes between various pieces of the calendar.
 */

/* The minimum padding around the numbers in each cell/day. */
#define	E_CALENDAR_ITEM_MIN_CELL_XPAD	4
#define	E_CALENDAR_ITEM_MIN_CELL_YPAD	0

/* Vertical padding. */
#define	E_CALENDAR_ITEM_YPAD_ABOVE_DAY_LETTERS		1
#define	E_CALENDAR_ITEM_YPAD_BELOW_DAY_LETTERS		0
#define	E_CALENDAR_ITEM_YPAD_ABOVE_CELLS		1
#define	E_CALENDAR_ITEM_YPAD_BELOW_CELLS		2

/* Horizontal padding in the heading bars. */
#define	E_CALENDAR_ITEM_XPAD_BEFORE_MONTH_NAME_WITH_BUTTON	10
#define	E_CALENDAR_ITEM_XPAD_BEFORE_MONTH_NAME			3
#define	E_CALENDAR_ITEM_XPAD_AFTER_MONTH_NAME			3
#define	E_CALENDAR_ITEM_XPAD_AFTER_MONTH_NAME_WITH_BUTTON	10

/* Horizontal padding in the month displays. */
#define	E_CALENDAR_ITEM_XPAD_BEFORE_WEEK_NUMBERS	4
#define	E_CALENDAR_ITEM_XPAD_AFTER_WEEK_NUMBERS		2
#define	E_CALENDAR_ITEM_XPAD_BEFORE_CELLS		1
#define	E_CALENDAR_ITEM_XPAD_AFTER_CELLS		4

/* These index our colors array. */
typedef enum
{
	E_CALENDAR_ITEM_COLOR_TODAY_BOX,
	E_CALENDAR_ITEM_COLOR_SELECTION_FG,
	E_CALENDAR_ITEM_COLOR_SELECTION_BG_FOCUSED,
	E_CALENDAR_ITEM_COLOR_SELECTION_BG,
	E_CALENDAR_ITEM_COLOR_PREV_OR_NEXT_MONTH_FG,

	E_CALENDAR_ITEM_COLOR_LAST
} ECalendarItemColors;

typedef struct _ECalendarItem       ECalendarItem;
typedef struct _ECalendarItemClass  ECalendarItemClass;

/* The type of the callback function optionally used to get the colors to
 * use for each day. */
typedef void (*ECalendarItemStyleCallback)   (ECalendarItem	*calitem,
					      gint		 year,
					      gint		 month,
					      gint		 day,
					      gint		 day_style,
					      gboolean		 today,
					      gboolean		 prev_or_next_month,
					      gboolean		 selected,
					      gboolean		 has_focus,
					      gboolean		 drop_target,
					      GdkRGBA	       **bg_color,
					      GdkRGBA	       **fg_color,
					      GdkRGBA	       **box_color,
					      gboolean		*bold,
					      gboolean		*italic,
					      gpointer		 data);

/* The type of the callback function optionally used to get the current time.
 */
typedef struct tm (*ECalendarItemGetTimeCallback) (ECalendarItem *calitem,
						   gpointer	  data);

/* Standard GObject macros */
#define E_TYPE_CALENDAR_ITEM \
	(e_calendar_item_get_type ())
#define E_CALENDAR_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALENDAR_ITEM, ECalendarItem))
#define E_CALENDAR_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALENDAR_ITEM, ECalendarItemClass))
#define E_IS_CALENDAR_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALENDAR_ITEM))
#define E_IS_CALENDAR_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALENDAR_ITEM))
#define E_CALENDAR_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALENDAR_ITEM, ECalendarItemClass))

struct _ECalendarItem {
	GnomeCanvasItem canvas_item;

	/* The year & month of the first calendar being displayed. */
	gint year;
	gint month;	/* 0 to 11 */

	/* Points to an array of styles, one gchar for each day. We use 32
	 * chars for each month, with n + 2 months, where n is the number of
	 * complete months shown (since we show some days before the first
	 * month and after the last month grayes out).
	 * A value of 0 is the default, and 1 is bold. */
	guint8 *styles;

	/*
	 * Options.
	 */

	/* The minimum & maximum number of rows & columns of months.
	 * If the maximum values are -1 then there is no maximum.
	 * The minimum valies default to 1. The maximum values to -1. */
	gint min_rows;
	gint min_cols;
	gint max_rows;
	gint max_cols;

	/* The actual number of rows & columns of months. */
	gint rows;
	gint cols;

	/* Whether we show week nubers. */
	gboolean show_week_numbers;
	/* whether to keep same week days selected on week number click */
	gboolean keep_wdays_on_weeknum_click;

	/* The first day of the week. */
	GDateWeekday week_start_day;

	/* Whether the cells expand to fill extra space. */
	gboolean expand;

	/* The maximum number of days that can be selected. Defaults to 1. */
	gint max_days_selected;

	/* The number of days selected before we switch to selecting whole
	 * weeks, or -1 if we never switch. Defaults to -1. */
	gint days_to_start_week_selection;

	/* Whether the selection is moved when we move back/forward one month.
	 * Used for things like the EDateEdit which only want the selection to
	 * be changed when the user explicitly selects a day. */
	gboolean move_selection_when_moving;

	/* Whether the selection day is preserved when we  move back/forward
	 * one month. Used for the work week and week view. */
	gboolean preserve_day_when_moving;

	/* Whether to display the pop-up, TRUE by default */
	gboolean display_popup;

	/*
	 * Internal stuff.
	 */

	/* Bounds of item. */
	gdouble x1, y1, x2, y2;

	/* The minimum size of each month, based on the fonts used. */
	gint min_month_width;
	gint min_month_height;

	/* The actual size of each month, after dividing extra space. */
	gint month_width;
	gint month_height;

	/* The offset to the left edge of the first calendar. */
	gint x_offset;

	/* The padding around each calendar month. */
	gint month_lpad, month_rpad;
	gint month_tpad, month_bpad;

	/* The size of each cell. */
	gint cell_width;
	gint cell_height;

	/* The current selection. The month offsets are from 0, which is the
	 * top-left calendar month view. Note that -1 is used for the last days
	 * from the previous month. The days are real month days. */
	gboolean selecting;
	GDate *selecting_axis;
	gboolean selection_dragging_end;
	gboolean selection_from_full_week;
	gboolean selection_set;
	gint selection_start_month_offset;
	gint selection_start_day;
	gint selection_end_month_offset;
	gint selection_end_day;
	gint selection_real_start_month_offset;
	gint selection_real_start_day;

	/* Widths of the day characters. */
	gint day_widths[8]; /* indexed by GDateWeekday */
	gint max_day_width;

	/* Widths of the digits, '0' .. '9'. */
	gint digit_widths[10];
	gint max_digit_width;

	gint week_number_digit_widths[10];
	gint max_week_number_digit_width;

	gint max_month_name_width;

	/* Fonts for drawing text. If font isn't set it uses the font from the
	 * canvas widget. If week_number_font isn't set it uses font. */
	PangoFontDescription *font_desc;
	PangoFontDescription *week_number_font_desc;

	ECalendarItemStyleCallback style_callback;
	gpointer style_callback_data;
	GDestroyNotify style_callback_destroy;

	ECalendarItemGetTimeCallback time_callback;
	gpointer time_callback_data;
	GDestroyNotify time_callback_destroy;

	/* Colors for drawing. */
	GdkRGBA colors[E_CALENDAR_ITEM_COLOR_LAST];

	/* Our idle handler for emitting signals. */
	gint signal_emission_idle_id;

	/* A flag to indicate that the selection or date range has changed.
	 * When set the idle function will emit the signal and reset it to
	 * FALSE. This is so we don't emit it several times when args are set
	 * etc. */
	gboolean selection_changed;
	gboolean date_range_changed;
};

struct _ECalendarItemClass {
	GnomeCanvasItemClass parent_class;

	void (* date_range_changed)	(ECalendarItem *calitem);
	void (* selection_changed)	(ECalendarItem *calitem);
	void (* selection_preview_changed)	(ECalendarItem *calitem);
};

GType	e_calendar_item_get_type		(void) G_GNUC_CONST;

/* FIXME: months are 0-11 throughout, but 1-12 may be better. */

void	e_calendar_item_get_first_month		(ECalendarItem *calitem,
						 gint *year,
						 gint *month);
void	e_calendar_item_set_first_month		(ECalendarItem *calitem,
						 gint year,
						 gint month);

/* Get the maximum number of days selectable */
gint	e_calendar_item_get_max_days_sel	(ECalendarItem *calitem);

/* Set the maximum number of days selectable */
void	e_calendar_item_set_max_days_sel	(ECalendarItem *calitem,
						 gint days);

/* Get the maximum number of days selectable */
gint	e_calendar_item_get_days_start_week_sel	(ECalendarItem *calitem);

/* Set the maximum number of days selectable */
void	e_calendar_item_set_days_start_week_sel	(ECalendarItem *calitem,
						 gint days);

/* Set the maximum number of days before whole weeks are selected */
gboolean
	e_calendar_item_get_display_popup	(ECalendarItem *calitem);

/* Get the maximum number of days before whole weeks are selected */
void	e_calendar_item_set_display_popup	(ECalendarItem *calitem,
						 gboolean display);

/* Gets the range of dates actually shown. Months are 0 to 11.
 * This also includes the last days of the previous month and the first days
 * of the following month, which are normally shown in gray.
 * It returns FALSE if no dates are currently shown. */
gboolean
	e_calendar_item_get_date_range		(ECalendarItem *calitem,
						 gint *start_year,
						 gint *start_month,
						 gint *start_day,
						 gint *end_year,
						 gint *end_month,
						 gint *end_day);

/* Returns the selected date range. It returns FALSE if no days are currently
 * selected. */
gboolean
	e_calendar_item_get_selection		(ECalendarItem *calitem,
						 GDate *start_date,
						 GDate *end_date);
/* Sets the selected date range, and changes the date range shown so at least
 * the start of the selection is shown. If start_date is NULL it clears the
 * selection. */
void	e_calendar_item_set_selection		(ECalendarItem *calitem,
						 const GDate *start_date,
						 const GDate *end_date);

/* Marks a particular day. Passing E_CALENDAR_ITEM_MARK_BOLD as the day style
 * will result in the day being shown as bold by default. The style callback
 * could support more day_styles, or the style callback could determine the
 * colors itself, without needing to mark days. */
void	e_calendar_item_clear_marks		(ECalendarItem *calitem);
void	e_calendar_item_mark_day		(ECalendarItem *calitem,
						 gint year,
						 gint month,
						 gint day,
						 guint8 day_style,
						 gboolean add_day_style);

/* Mark a range of days. Any days outside the currently shown range are
 * ignored. */
void	e_calendar_item_mark_days		(ECalendarItem *calitem,
						 gint start_year,
						 gint start_month,
						 gint start_day,
						 gint end_year,
						 gint end_month,
						 gint end_day,
						 guint8 day_style,
						 gboolean add_day_style);

/* Sets the function to call to get the colors to use for a particular day. */
void	e_calendar_item_set_style_callback	(ECalendarItem *calitem,
						 ECalendarItemStyleCallback cb,
						 gpointer data,
						 GDestroyNotify  destroy);

/* Sets a callback to use to get the current time. This is useful if the
 * application needs to use its own timezone data rather than rely on the
 * Unix timezone. */
void	e_calendar_item_set_get_time_callback	(ECalendarItem *calitem,
						 ECalendarItemGetTimeCallback cb,
						 gpointer data,
						 GDestroyNotify  destroy);
void	e_calendar_item_normalize_date		(ECalendarItem *calitem,
						 gint *year,
						 gint *month);
gint	e_calendar_item_get_week_number		(ECalendarItem *calitem,
						 gint day,
						 gint month,
						 gint year);
void	e_calendar_item_style_updated		(GtkWidget *widget,
						 ECalendarItem *calitem);
gboolean
	e_calendar_item_convert_position_to_date
						(ECalendarItem *calitem,
						 gint event_x,
						 gint event_y,
						 GDate *date);

G_END_DECLS

#endif /* _E_CALENDAR_ITEM_H_ */

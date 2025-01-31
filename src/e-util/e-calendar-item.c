/*
 * ECalendarItem - canvas item displaying a calendar.
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
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include <libebackend/libebackend.h>

#include "e-calendar-item.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "ea-widgets.h"
#include "e-misc-utils.h"
#include "e-util-enumtypes.h"

static const gint e_calendar_item_days_in_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define DAYS_IN_MONTH(year, month) \
  e_calendar_item_days_in_month[month] + (((month) == 1 \
  && ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))) ? 1 : 0)

static void	e_calendar_item_constructed	(GObject *object);
static void	e_calendar_item_dispose		(GObject *object);
static void	e_calendar_item_get_property	(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void	e_calendar_item_set_property	(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void	e_calendar_item_realize		(GnomeCanvasItem *item);
static void	e_calendar_item_unmap		(GnomeCanvasItem *item);
static void	e_calendar_item_update		(GnomeCanvasItem *item,
						 const cairo_matrix_t *i2c,
						 gint flags);
static void	e_calendar_item_draw		(GnomeCanvasItem *item,
						 cairo_t *cr,
						 gint x,
						 gint y,
						 gint width,
						 gint height);
static void	e_calendar_item_draw_month	(ECalendarItem *calitem,
						 cairo_t *cr,
						 gint x,
						 gint y,
						 gint width,
						 gint height,
						 gint row,
						 gint col);
static void	e_calendar_item_draw_day_numbers
						(ECalendarItem *calitem,
						 cairo_t *cr,
						 gint width,
						 gint height,
						 gint row,
						 gint col,
						 gint year,
						 gint month,
						 GDateWeekday start_weekday,
						 gint cells_x,
						 gint cells_y);
static GnomeCanvasItem *e_calendar_item_point	(GnomeCanvasItem *item,
						 gdouble x,
						 gdouble y,
						 gint cx,
						 gint cy);
static void	e_calendar_item_stop_selecting	(ECalendarItem *calitem,
						 guint32 time);
static void	e_calendar_item_selection_add_days
						(ECalendarItem *calitem,
						 gint n_days,
						 gboolean multi_selection);
static gint	e_calendar_item_key_press_event	(ECalendarItem *item,
						 GdkEvent *event);
static gint	e_calendar_item_event		(GnomeCanvasItem *item,
						 GdkEvent *event);
static void	e_calendar_item_bounds		(GnomeCanvasItem *item,
						 gdouble *x1,
						 gdouble *y1,
						 gdouble *x2,
						 gdouble *y2);

static gboolean	e_calendar_item_button_press	(ECalendarItem *calitem,
						 GdkEvent *event);
static gboolean	e_calendar_item_button_release	(ECalendarItem *calitem,
						 GdkEvent *event);
static gboolean	e_calendar_item_motion		(ECalendarItem *calitem,
						 GdkEvent *event);

static gboolean	e_calendar_item_convert_position_to_day
						(ECalendarItem *calitem,
						 gint x,
						 gint y,
						 gboolean round_empty_positions,
						 gint *month_offset,
						 gint *day,
						 gboolean *entire_week);
static void	e_calendar_item_get_month_info	(ECalendarItem *calitem,
						 gint row,
						 gint col,
						 gint *first_day_offset,
						 gint *days_in_month,
						 gint *days_in_prev_month);
static void	e_calendar_item_recalc_sizes	(ECalendarItem *calitem);

static void	e_calendar_item_get_day_style	(ECalendarItem *calitem,
						 gint year,
						 gint month,
						 gint day,
						 gint day_style,
						 gboolean today,
						 gboolean prev_or_next_month,
						 gboolean selected,
						 gboolean has_focus,
						 gboolean drop_target,
						 GdkRGBA **bg_color,
						 GdkRGBA **fg_color,
						 GdkRGBA **box_color,
						 gboolean *bold,
						 gboolean *italic,
						 GdkRGBA *local_bg_color,
						 GdkRGBA *local_fg_color);
static void	e_calendar_item_check_selection_end
						(ECalendarItem *calitem,
						 gint start_month,
						 gint start_day,
						 gint *end_month,
						 gint *end_day);
static void	e_calendar_item_check_selection_start
						(ECalendarItem *calitem,
						 gint *start_month,
						 gint *start_day,
						 gint end_month,
						 gint end_day);
static void	e_calendar_item_add_days_to_selection
						(ECalendarItem *calitem,
						 gint days);
static void	e_calendar_item_round_up_selection
						(ECalendarItem *calitem,
						 gint *month_offset,
						 gint *day);
static void	e_calendar_item_round_down_selection
						(ECalendarItem *calitem,
						 gint *month_offset,
						 gint *day);
static gint	e_calendar_item_get_inclusive_days
						(ECalendarItem *calitem,
						 gint start_month_offset,
						 gint start_day,
						 gint end_month_offset,
						 gint end_day);
static void	e_calendar_item_ensure_valid_day
						(ECalendarItem *calitem,
						 gint *month_offset,
						 gint *day);
static gboolean	e_calendar_item_ensure_days_visible
						(ECalendarItem *calitem,
						 gint start_year,
						 gint start_month,
						 gint start_day,
						 gint end_year,
						 gint end_month,
						 gint end_day,
						 gboolean emission);
static void	e_calendar_item_show_popup_menu	(ECalendarItem *calitem,
						 GdkEvent *button_event,
						 gint month_offset);
static void	e_calendar_item_on_menu_item_activate
						(GtkWidget *menuitem,
						 ECalendarItem *calitem);
static void	e_calendar_item_date_range_changed
						(ECalendarItem *calitem);
static void	e_calendar_item_queue_signal_emission
						(ECalendarItem *calitem);
static gboolean	e_calendar_item_signal_emission_idle_cb
						(gpointer data);
static void	e_calendar_item_set_selection_if_emission
						(ECalendarItem *calitem,
						 const GDate *start_date,
						 const GDate *end_date,
						 gboolean emission);
static void	e_calendar_item_set_first_month_with_emit
						(ECalendarItem *calitem,
						 gint year,
						 gint month,
						 gboolean emit_date_range_moved);

/* Our arguments. */
enum {
	PROP_0,
	PROP_YEAR,
	PROP_MONTH,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_FONT_DESC,
	PROP_WEEK_NUMBER_FONT,
	PROP_WEEK_NUMBER_FONT_DESC,
	PROP_ROW_HEIGHT,
	PROP_COLUMN_WIDTH,
	PROP_MINIMUM_ROWS,
	PROP_MINIMUM_COLUMNS,
	PROP_MAXIMUM_ROWS,
	PROP_MAXIMUM_COLUMNS,
	PROP_WEEK_START_DAY,
	PROP_SHOW_WEEK_NUMBERS,
	PROP_KEEP_WDAYS_ON_WEEKNUM_CLICK,
	PROP_MAXIMUM_DAYS_SELECTED,
	PROP_DAYS_TO_START_WEEK_SELECTION,
	PROP_MOVE_SELECTION_WHEN_MOVING,
	PROP_PRESERVE_DAY_WHEN_MOVING,
	PROP_DISPLAY_POPUP
};

enum {
	DATE_RANGE_CHANGED,
	DATE_RANGE_MOVED,
	SELECTION_CHANGED,
	SELECTION_PREVIEW_CHANGED,
	MONTH_WIDTH_CHANGED,
	CALC_MIN_COLUMN_WIDTH,
	LAST_SIGNAL
};

static guint e_calendar_item_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (
	ECalendarItem,
	e_calendar_item,
	GNOME_TYPE_CANVAS_ITEM,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
e_calendar_item_class_init (ECalendarItemClass *class)
{
	GObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_calendar_item_constructed;
	object_class->dispose = e_calendar_item_dispose;
	object_class->get_property = e_calendar_item_get_property;
	object_class->set_property = e_calendar_item_set_property;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->realize = e_calendar_item_realize;
	item_class->unmap = e_calendar_item_unmap;
	item_class->update = e_calendar_item_update;
	item_class->draw = e_calendar_item_draw;
	item_class->point = e_calendar_item_point;
	item_class->event = e_calendar_item_event;
	item_class->bounds = e_calendar_item_bounds;

	class->date_range_changed = NULL;
	class->selection_changed = NULL;
	class->selection_preview_changed = NULL;

	g_object_class_install_property (
		object_class,
		PROP_YEAR,
		g_param_spec_int (
			"year",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MONTH,
		g_param_spec_int (
			"month",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_X1,
		g_param_spec_double (
			"x1",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_Y1,
		g_param_spec_double (
			"y1",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_X2,
		g_param_spec_double (
			"x2",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_Y2,
		g_param_spec_double (
			"y2",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			0.,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_FONT_DESC,
		g_param_spec_boxed (
			"font_desc",
			NULL,
			NULL,
			PANGO_TYPE_FONT_DESCRIPTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_NUMBER_FONT_DESC,
		g_param_spec_boxed (
			"week_number_font_desc",
			NULL,
			NULL,
			PANGO_TYPE_FONT_DESCRIPTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ROW_HEIGHT,
		g_param_spec_int (
			"row_height",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_COLUMN_WIDTH,
		g_param_spec_int (
			"column_width",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_ROWS,
		g_param_spec_int (
			"minimum_rows",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_COLUMNS,
		g_param_spec_int (
			"minimum_columns",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAXIMUM_ROWS,
		g_param_spec_int (
			"maximum_rows",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAXIMUM_COLUMNS,
		g_param_spec_int (
			"maximum_columns",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_enum (
			"week-start-day",
			NULL,
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_WEEK_NUMBERS,
		g_param_spec_boolean (
			"show_week_numbers",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_KEEP_WDAYS_ON_WEEKNUM_CLICK,
		g_param_spec_boolean (
			"keep_wdays_on_weeknum_click",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MAXIMUM_DAYS_SELECTED,
		g_param_spec_int (
			"maximum_days_selected",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DAYS_TO_START_WEEK_SELECTION,
		g_param_spec_int (
			"days_to_start_week_selection",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MOVE_SELECTION_WHEN_MOVING,
		g_param_spec_boolean (
			"move_selection_when_moving",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PRESERVE_DAY_WHEN_MOVING,
		g_param_spec_boolean (
			"preserve_day_when_moving",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_DISPLAY_POPUP,
		g_param_spec_boolean (
			"display_popup",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_calendar_item_signals[DATE_RANGE_CHANGED] = g_signal_new (
		"date_range_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECalendarItemClass, date_range_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* Invoked when a user changes date range, by pressing month/year
	   arrows or any similar way, but not when selecting a day in the calendar. */
	e_calendar_item_signals[DATE_RANGE_MOVED] = g_signal_new (
		"date-range-moved",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	e_calendar_item_signals[SELECTION_CHANGED] = g_signal_new (
		"selection_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECalendarItemClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	e_calendar_item_signals[SELECTION_PREVIEW_CHANGED] = g_signal_new (
		"selection_preview_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarItemClass, selection_preview_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	e_calendar_item_signals[MONTH_WIDTH_CHANGED] = g_signal_new (
		"month-width-changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		0 /* G_STRUCT_OFFSET (ECalendarItemClass, month_width_changed) */,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	e_calendar_item_signals[CALC_MIN_COLUMN_WIDTH] = g_signal_new (
		"calc-min-column-width",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
		0 /* G_STRUCT_OFFSET (ECalendarItemClass, calc_min_column_width) */,
		NULL, NULL,
		NULL,
		G_TYPE_INT, 0);

	e_calendar_item_a11y_init ();
}

static void
e_calendar_item_init (ECalendarItem *calitem)
{
	struct tm *tmp_tm;
	time_t t;

	/* Set the default time to the current month. */
	t = time (NULL);
	tmp_tm = localtime (&t);
	calitem->year = tmp_tm->tm_year + 1900;
	calitem->month = tmp_tm->tm_mon;

	calitem->styles = NULL;

	calitem->min_cols = 1;
	calitem->min_rows = 1;
	calitem->max_cols = -1;
	calitem->max_rows = -1;

	calitem->rows = 0;
	calitem->cols = 0;

	calitem->show_week_numbers = FALSE;
	calitem->keep_wdays_on_weeknum_click = FALSE;
	calitem->week_start_day = G_DATE_MONDAY;
	calitem->expand = TRUE;
	calitem->max_days_selected = 1;
	calitem->days_to_start_week_selection = -1;
	calitem->move_selection_when_moving = TRUE;
	calitem->preserve_day_when_moving = FALSE;
	calitem->display_popup = TRUE;

	calitem->x1 = 0.0;
	calitem->y1 = 0.0;
	calitem->x2 = 0.0;
	calitem->y2 = 0.0;

	calitem->selecting = FALSE;
	calitem->selecting_axis = NULL;

	calitem->selection_set = FALSE;

	calitem->selection_changed = FALSE;
	calitem->date_range_changed = FALSE;

	calitem->style_callback = NULL;
	calitem->style_callback_data = NULL;
	calitem->style_callback_destroy = NULL;

	calitem->time_callback = NULL;
	calitem->time_callback_data = NULL;
	calitem->time_callback_destroy = NULL;

	calitem->signal_emission_idle_id = 0;
}

static void
e_calendar_item_constructed (GObject *object)
{
	ECalendarItem *calitem = E_CALENDAR_ITEM (object);

	G_OBJECT_CLASS (e_calendar_item_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (calitem));
}

static void
e_calendar_item_dispose (GObject *object)
{
	ECalendarItem *calitem;

	calitem = E_CALENDAR_ITEM (object);

	e_calendar_item_set_style_callback (calitem, NULL, NULL, NULL);
	e_calendar_item_set_get_time_callback (calitem, NULL, NULL, NULL);

	g_clear_pointer (&calitem->styles, g_free);

	if (calitem->signal_emission_idle_id > 0) {
		g_source_remove (calitem->signal_emission_idle_id);
		calitem->signal_emission_idle_id = -1;
	}

	g_clear_pointer (&calitem->font_desc, pango_font_description_free);
	g_clear_pointer (&calitem->week_number_font_desc, pango_font_description_free);

	g_free (calitem->selecting_axis);

	G_OBJECT_CLASS (e_calendar_item_parent_class)->dispose (object);
}

static void
e_calendar_item_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	ECalendarItem *calitem;
	gint min_column_width;

	calitem = E_CALENDAR_ITEM (object);

	switch (property_id) {
	case PROP_YEAR:
		g_value_set_int (value, calitem->year);
		return;
	case PROP_MONTH:
		g_value_set_int (value, calitem->month);
		return;
	case PROP_X1:
		g_value_set_double (value, calitem->x1);
		return;
	case PROP_Y1:
		g_value_set_double (value, calitem->y1);
		return;
	case PROP_X2:
		g_value_set_double (value, calitem->x2);
		return;
	case PROP_Y2:
		g_value_set_double (value, calitem->y2);
		return;
	case PROP_FONT_DESC:
		g_value_set_boxed (value, calitem->font_desc);
		return;
	case PROP_WEEK_NUMBER_FONT_DESC:
		g_value_set_boxed (value, calitem->week_number_font_desc);
		return;
	case PROP_ROW_HEIGHT:
		e_calendar_item_recalc_sizes (calitem);
		g_value_set_int (value, calitem->min_month_height);
		return;
	case PROP_COLUMN_WIDTH:
		e_calendar_item_recalc_sizes (calitem);

		min_column_width = 0;
		g_signal_emit (calitem, e_calendar_item_signals[CALC_MIN_COLUMN_WIDTH], 0, &min_column_width);

		if (min_column_width < calitem->min_month_width)
			min_column_width = calitem->min_month_width;

		g_value_set_int (value, min_column_width);
		return;
	case PROP_MINIMUM_ROWS:
		g_value_set_int (value, calitem->min_rows);
		return;
	case PROP_MINIMUM_COLUMNS:
		g_value_set_int (value, calitem->min_cols);
		return;
	case PROP_MAXIMUM_ROWS:
		g_value_set_int (value, calitem->max_rows);
		return;
	case PROP_MAXIMUM_COLUMNS:
		g_value_set_int (value, calitem->max_cols);
		return;
	case PROP_WEEK_START_DAY:
		g_value_set_enum (value, calitem->week_start_day);
		return;
	case PROP_SHOW_WEEK_NUMBERS:
		g_value_set_boolean (value, calitem->show_week_numbers);
		return;
	case PROP_KEEP_WDAYS_ON_WEEKNUM_CLICK:
		g_value_set_boolean (value, calitem->keep_wdays_on_weeknum_click);
		return;
	case PROP_MAXIMUM_DAYS_SELECTED:
		g_value_set_int (value, e_calendar_item_get_max_days_sel (calitem));
		return;
	case PROP_DAYS_TO_START_WEEK_SELECTION:
		g_value_set_int (value, e_calendar_item_get_days_start_week_sel (calitem));
		return;
	case PROP_MOVE_SELECTION_WHEN_MOVING:
		g_value_set_boolean (value, calitem->move_selection_when_moving);
		return;
	case PROP_PRESERVE_DAY_WHEN_MOVING:
		g_value_set_boolean (value, calitem->preserve_day_when_moving);
		return;
	case PROP_DISPLAY_POPUP:
		g_value_set_boolean (value, e_calendar_item_get_display_popup (calitem));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_calendar_item_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	ECalendarItem *calitem;
	PangoFontDescription *font_desc;
	gdouble dvalue;
	gint ivalue;
	gboolean bvalue;

	item = GNOME_CANVAS_ITEM (object);
	calitem = E_CALENDAR_ITEM (object);

	switch (property_id) {
	case PROP_YEAR:
		ivalue = g_value_get_int (value);
		e_calendar_item_set_first_month (
			calitem, ivalue, calitem->month);
		return;
	case PROP_MONTH:
		ivalue = g_value_get_int (value);
		e_calendar_item_set_first_month (
			calitem, calitem->year, ivalue);
		return;
	case PROP_X1:
		dvalue = g_value_get_double (value);
		if (calitem->x1 != dvalue) {
			calitem->x1 = dvalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_Y1:
		dvalue = g_value_get_double (value);
		if (calitem->y1 != dvalue) {
			calitem->y1 = dvalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_X2:
		dvalue = g_value_get_double (value);
		if (calitem->x2 != dvalue) {
			calitem->x2 = dvalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_Y2:
		dvalue = g_value_get_double (value);
		if (calitem->y2 != dvalue) {
			calitem->y2 = dvalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_FONT_DESC:
		font_desc = g_value_get_boxed (value);
		if (calitem->font_desc)
			pango_font_description_free (calitem->font_desc);
		calitem->font_desc = pango_font_description_copy (font_desc);
		if (item->canvas)
			gnome_canvas_item_request_update (item);
		return;
	case PROP_WEEK_NUMBER_FONT_DESC:
		font_desc = g_value_get_boxed (value);
		if (calitem->week_number_font_desc)
			pango_font_description_free (calitem->week_number_font_desc);
		calitem->week_number_font_desc = pango_font_description_copy (font_desc);
		if (item->canvas)
			gnome_canvas_item_request_update (item);
		return;
	case PROP_MINIMUM_ROWS:
		ivalue = g_value_get_int (value);
		ivalue = MAX (1, ivalue);
		if (calitem->min_rows != ivalue) {
			calitem->min_rows = ivalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_MINIMUM_COLUMNS:
		ivalue = g_value_get_int (value);
		ivalue = MAX (1, ivalue);
		if (calitem->min_cols != ivalue) {
			calitem->min_cols = ivalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_MAXIMUM_ROWS:
		ivalue = g_value_get_int (value);
		if (calitem->max_rows != ivalue) {
			calitem->max_rows = ivalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_MAXIMUM_COLUMNS:
		ivalue = g_value_get_int (value);
		if (calitem->max_cols != ivalue) {
			calitem->max_cols = ivalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_WEEK_START_DAY:
		ivalue = g_value_get_enum (value);
		if (calitem->week_start_day != ivalue) {
			calitem->week_start_day = ivalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_SHOW_WEEK_NUMBERS:
		bvalue = g_value_get_boolean (value);
		if (calitem->show_week_numbers != bvalue) {
			calitem->show_week_numbers = bvalue;
			if (item->canvas)
				gnome_canvas_item_request_update (item);
		}
		return;
	case PROP_KEEP_WDAYS_ON_WEEKNUM_CLICK:
		calitem->keep_wdays_on_weeknum_click = g_value_get_boolean (value);
		return;
	case PROP_MAXIMUM_DAYS_SELECTED:
		ivalue = g_value_get_int (value);
		e_calendar_item_set_max_days_sel (calitem, ivalue);
		return;
	case PROP_DAYS_TO_START_WEEK_SELECTION:
		ivalue = g_value_get_int (value);
		e_calendar_item_set_days_start_week_sel (calitem, ivalue);
		return;
	case PROP_MOVE_SELECTION_WHEN_MOVING:
		bvalue = g_value_get_boolean (value);
		calitem->move_selection_when_moving = bvalue;
		return;
	case PROP_PRESERVE_DAY_WHEN_MOVING:
		bvalue = g_value_get_boolean (value);
		calitem->preserve_day_when_moving = bvalue;
		return;
	case PROP_DISPLAY_POPUP:
		bvalue = g_value_get_boolean (value);
		e_calendar_item_set_display_popup (calitem, bvalue);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_calendar_item_realize (GnomeCanvasItem *item)
{
	ECalendarItem *calitem;

	if (GNOME_CANVAS_ITEM_CLASS (e_calendar_item_parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS (e_calendar_item_parent_class)->realize) (item);

	calitem = E_CALENDAR_ITEM (item);

	e_calendar_item_style_updated (GTK_WIDGET (item->canvas), calitem);
}

static void
e_calendar_item_unmap (GnomeCanvasItem *item)
{
	ECalendarItem *calitem;

	calitem = E_CALENDAR_ITEM (item);

	if (calitem->selecting) {
		gnome_canvas_item_ungrab (item, GDK_CURRENT_TIME);
		calitem->selecting = FALSE;
	}

	if (GNOME_CANVAS_ITEM_CLASS (e_calendar_item_parent_class)->unmap)
		(* GNOME_CANVAS_ITEM_CLASS (e_calendar_item_parent_class)->unmap) (item);
}

static void
e_calendar_item_update (GnomeCanvasItem *item,
                        const cairo_matrix_t *i2c,
                        gint flags)
{
	GnomeCanvasItemClass *item_class;
	ECalendarItem *calitem;
	gint char_height, width, height, space, space_per_cal, space_per_cell;
	gint rows, cols, xthickness, ythickness, old_month_width, min_column_width;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	GtkStyleContext *style_context;
	GtkBorder padding;

	item_class = GNOME_CANVAS_ITEM_CLASS (e_calendar_item_parent_class);
	if (item_class->update != NULL)
		item_class->update (item, i2c, flags);

	calitem = E_CALENDAR_ITEM (item);
	style_context = gtk_widget_get_style_context (GTK_WIDGET (item->canvas));
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);
	xthickness = padding.left;
	ythickness = padding.top;

	item->x1 = calitem->x1;
	item->y1 = calitem->y1;
	item->x2 = calitem->x2 >= calitem->x1 ? calitem->x2 : calitem->x1;
	item->y2 = calitem->y2 >= calitem->y1 ? calitem->y2 : calitem->y1;

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (item->canvas));
	font_metrics = pango_context_get_metrics (
		pango_context, NULL,
		pango_context_get_language (pango_context));

	/*
	 * Calculate the new layout of the calendar.
	 */

	/* Make sure the minimum row width & cell height and the widths of
	 * all the digits and characters are up to date. */
	e_calendar_item_recalc_sizes (calitem);

	/* Calculate how many rows & cols we can fit in. */
	width = item->x2 - item->x1;
	height = item->y2 - item->y1;

	width -= xthickness * 2;
	height -= ythickness * 2;

	if (calitem->min_month_height == 0)
		rows = 1;
	else
		rows = height / calitem->min_month_height;
	rows = MAX (rows, calitem->min_rows);
	if (calitem->max_rows > 0)
		rows = MIN (rows, calitem->max_rows);

	min_column_width = 0;
	g_signal_emit (calitem, e_calendar_item_signals[CALC_MIN_COLUMN_WIDTH], 0, &min_column_width);

	if (min_column_width < calitem->min_month_width)
		min_column_width = calitem->min_month_width;

	if (min_column_width == 0)
		cols = 1;
	else
		cols = width / min_column_width;
	cols = MAX (cols, calitem->min_cols);
	if (calitem->max_cols > 0)
		cols = MIN (cols, calitem->max_cols);

	if (rows != calitem->rows || cols != calitem->cols)
		e_calendar_item_date_range_changed (calitem);

	calitem->rows = rows;
	calitem->cols = cols;

	/* Split up the empty space according to the configuration.
	 * If the calendar is set to expand, we divide the space between the
	 * cells and the spaces around the calendar, otherwise we place the
	 * calendars in the center of the available area. */

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	old_month_width = calitem->month_width;
	calitem->month_width = calitem->min_month_width;
	calitem->month_height = calitem->min_month_height;
	calitem->cell_width = MAX (calitem->max_day_width, (calitem->max_digit_width * 2))
		+ E_CALENDAR_ITEM_MIN_CELL_XPAD;
	calitem->cell_height = char_height
		+ E_CALENDAR_ITEM_MIN_CELL_YPAD;
	calitem->month_tpad = 0;
	calitem->month_bpad = 0;
	calitem->month_lpad = 0;
	calitem->month_rpad = 0;

	space = height - calitem->rows * calitem->month_height;
	if (space > 0) {
		space_per_cal = space / calitem->rows;
		calitem->month_height += space_per_cal;

		if (calitem->expand) {
			space_per_cell = space_per_cal / E_CALENDAR_ROWS_PER_MONTH;
			calitem->cell_height += space_per_cell;
			space_per_cal -= space_per_cell * E_CALENDAR_ROWS_PER_MONTH;
		}

		calitem->month_tpad = space_per_cal / 2;
		calitem->month_bpad = space_per_cal - calitem->month_tpad;
	}

	space = width - calitem->cols * calitem->month_width;
	if (space > 0) {
		space_per_cal = space / calitem->cols;
		calitem->month_width += space_per_cal;
		space -= space_per_cal * calitem->cols;

		if (calitem->expand) {
			space_per_cell = space_per_cal / E_CALENDAR_COLS_PER_MONTH;
			calitem->cell_width += space_per_cell;
			space_per_cal -= space_per_cell * E_CALENDAR_COLS_PER_MONTH;
		}

		calitem->month_lpad = space_per_cal / 2;
		calitem->month_rpad = space_per_cal - calitem->month_lpad;
	}

	space = MAX (0, space);
	calitem->x_offset = space / 2;

	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1,
		item->x2, item->y2);

	pango_font_metrics_unref (font_metrics);

	if (old_month_width != calitem->month_width) {
		g_signal_emit (calitem, e_calendar_item_signals[MONTH_WIDTH_CHANGED], 0, NULL);
	}
}

/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */
static void
e_calendar_item_draw (GnomeCanvasItem *canvas_item,
                      cairo_t *cr,
                      gint x,
                      gint y,
                      gint width,
                      gint height)
{
	ECalendarItem *calitem;
	GtkWidget *widget;
	GtkStyleContext *style_context;
	gint char_height, row, col, row_y, bar_height;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	GdkRGBA bg_color;
	GtkBorder border;

#if 0
	g_print (
		"In e_calendar_item_draw %i,%i %ix%i\n",
		x, y, width, height);
#endif
	calitem = E_CALENDAR_ITEM (canvas_item);

	widget = GTK_WIDGET (canvas_item->canvas);
	style_context = gtk_widget_get_style_context (widget);

	/* Set up Pango prerequisites */
	pango_context = gtk_widget_get_pango_context (
		GTK_WIDGET (canvas_item->canvas));
	/* It's OK when the calitem->font_desc is NUL, then the currently set font is used */
	font_metrics = pango_context_get_metrics (
		pango_context, calitem->font_desc,
		pango_context_get_language (pango_context));

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	e_utils_get_theme_color (widget, "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg_color);

	gtk_style_context_get_border (style_context, gtk_style_context_get_state (style_context), &border);

	/* Clear the entire background. */
	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &bg_color);
	cairo_rectangle (
		cr, calitem->x1 - x, calitem->y1 - y,
		calitem->x2 - calitem->x1 + 1,
		calitem->y2 - calitem->y1 + 1);
	cairo_fill (cr);
	cairo_restore (cr);

	row_y = canvas_item->y1 + border.top;
	bar_height =
		border.top + border.bottom +
		E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME + char_height +
		E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME;

	for (row = 0; row < calitem->rows; row++) {
		/* Draw the background for the title bars and the shadow around
		 * it, and the vertical lines between columns. */

		cairo_save (cr);
		gdk_cairo_set_source_rgba (cr, &bg_color);
		cairo_rectangle (
			cr, calitem->x1 + border.left - x,
			row_y - y,
			calitem->x2 - calitem->x1 + 1 -
			(border.left + border.right),
			bar_height);
		cairo_fill (cr);
		cairo_restore (cr);

		gtk_style_context_save (style_context);
		gtk_style_context_add_class (
			style_context, GTK_STYLE_CLASS_HEADER);
		cairo_save (cr);
		gtk_render_frame (
			style_context, cr,
			(gdouble) calitem->x1 + border.left - x,
			(gdouble) row_y - y,
			(gdouble) calitem->x2 - calitem->x1 + 1 -
				(border.left + border.right),
			(gdouble) bar_height);
		cairo_restore (cr);
		gtk_style_context_restore (style_context);

		for (col = 0; col < calitem->cols; col++) {
			e_calendar_item_draw_month (
				calitem, cr, x, y,
				width, height, row, col);
		}

		row_y += calitem->month_height;
	}

	/* Draw the shadow around the entire item. */
	gtk_style_context_save (style_context);
	gtk_style_context_add_class (
		style_context, GTK_STYLE_CLASS_ENTRY);
	cairo_save (cr);
	gtk_render_frame (
		style_context, cr,
		(gdouble) calitem->x1 - x,
		(gdouble) calitem->y1 - y,
		(gdouble) calitem->x2 - calitem->x1 + 1,
		(gdouble) calitem->y2 - calitem->y1 + 1);
	cairo_restore (cr);
	gtk_style_context_restore (style_context);

	pango_font_metrics_unref (font_metrics);
}

static void
layout_set_day_text (ECalendarItem *calitem,
                     PangoLayout *layout,
                     GDateWeekday weekday)
{
	const gchar *abbr_name;

	abbr_name = e_get_weekday_name (weekday, TRUE);
	pango_layout_set_text (layout, abbr_name, -1);
}

static void
e_calendar_item_draw_month (ECalendarItem *calitem,
                            cairo_t *cr,
                            gint x,
                            gint y,
                            gint width,
                            gint height,
                            gint row,
                            gint col)
{
	GnomeCanvasItem *item;
	GtkWidget *widget;
	struct tm tmp_tm;
	GdkRectangle clip_rect;
	GDateWeekday start_weekday;
	gint char_height, xthickness, ythickness;
	gint year, month;
	gint month_x, month_y, month_w, month_h;
	gint min_x, max_x, text_x, text_y;
	gint day, cells_x, cells_y, min_cell_width, text_width, arrow_button_size;
	gint clip_width, clip_height;
	gchar buffer[64];
	GDateWeekday weekday;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	GtkStyleContext *style_context;
	GtkBorder padding;
	PangoFontDescription *font_desc;
	GdkRGBA rgba;

#if 0
	g_print (
		"In e_calendar_item_draw_month: %i,%i %ix%i row:%i col:%i\n",
		x, y, width, height, row, col);
#endif
	item = GNOME_CANVAS_ITEM (calitem);
	widget = GTK_WIDGET (item->canvas);

	/* Set up Pango prerequisites */
	font_desc = calitem->font_desc;
	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	if (!font_desc)
		font_desc = pango_context_get_font_description (pango_context);
	font_desc = pango_font_description_copy (font_desc);

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);
	xthickness = padding.left;
	ythickness = padding.top;
	arrow_button_size =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics))
		+ PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics))
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		+ 2 * xthickness;

	pango_font_metrics_unref (font_metrics);

	/* Calculate the top-left position of the entire month display. */
	month_x = item->x1 + xthickness + calitem->x_offset
		+ ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		? (calitem->cols - 1 - col) : col) * calitem->month_width - x;
	month_w = item->x2 - item->x1 - xthickness * 2;
	month_w = MAX (MIN (month_w, calitem->month_width), calitem->min_month_width);
	month_y = item->y1 + ythickness + row * calitem->month_height - y;
	month_h = item->y2 - item->y1 - ythickness * 2;
	month_h = MAX (MIN (month_h, calitem->month_height), calitem->min_month_height);

	/* Just return if the month is outside the given area. */
	if (month_x >= width || month_x + calitem->month_width <= 0
	    || month_y >= height || month_y + calitem->month_height <= 0) {
		pango_font_description_free (font_desc);
		return;
	}

	month = calitem->month + row * calitem->cols + col;
	year = calitem->year + month / 12;
	month %= 12;

	/* Draw the month name & year, with clipping. Note that the top row
	 * needs extra space around it for the buttons. */

	layout = gtk_widget_create_pango_layout (widget, NULL);

	if (row == 0 && col == 0)
		min_x = E_CALENDAR_ITEM_XPAD_BEFORE_MONTH_NAME_WITH_BUTTON;
	else
		min_x = E_CALENDAR_ITEM_XPAD_BEFORE_MONTH_NAME;

	max_x = month_w;
	if (row == 0 && col == 0)
		max_x -= E_CALENDAR_ITEM_XPAD_AFTER_MONTH_NAME_WITH_BUTTON;
	else
		max_x -= E_CALENDAR_ITEM_XPAD_AFTER_MONTH_NAME;

	text_y = month_y + padding.top
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME;
	clip_rect.x = month_x + min_x;
	clip_rect.x = MAX (0, clip_rect.x);
	clip_rect.y = MAX (0, text_y);

	memset (&tmp_tm, 0, sizeof (tmp_tm));
	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);

	start_weekday = e_weekday_from_tm_wday (tmp_tm.tm_wday);

	if (month_x + max_x - clip_rect.x > 0) {
		cairo_save (cr);

		clip_rect.width = month_x + max_x - clip_rect.x;
		clip_rect.height = text_y + char_height - clip_rect.y;
		gdk_cairo_rectangle (cr, &clip_rect);
		cairo_clip (cr);

		e_utils_get_theme_color (widget, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &rgba);
		gdk_cairo_set_source_rgba (cr, &rgba);

		if (row == 0 && col == 0) {
			PangoLayout *layout_yr;
			gchar buffer_yr[64];
			gdouble max_width;

			layout_yr = gtk_widget_create_pango_layout (widget, NULL);

			/* This is a strftime() format. %B = Month name. */
			e_utf8_strftime (buffer, sizeof (buffer), C_("CalItem", "%B"), &tmp_tm);
			/* This is a strftime() format. %Y = Year. */
			e_utf8_strftime (buffer_yr, sizeof (buffer_yr), C_("CalItem", "%Y"), &tmp_tm);

			pango_layout_set_font_description (layout, font_desc);
			pango_layout_set_text (layout, buffer, -1);

			pango_layout_set_font_description (layout_yr, font_desc);
			pango_layout_set_text (layout_yr, buffer_yr, -1);

			if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL) {
				max_width = calitem->max_month_name_width;
				pango_layout_get_pixel_size (layout, &text_width, NULL);

				cairo_move_to (cr, month_x + min_x + arrow_button_size + (max_width - text_width) / 2, text_y);
				pango_cairo_show_layout (cr, layout);

				max_width = calitem->max_digit_width * 5;
				pango_layout_get_pixel_size (layout_yr, &text_width, NULL);

				cairo_move_to (cr, month_x + month_w - arrow_button_size - (max_width - text_width) / 2 - text_width - min_x, text_y);
				pango_cairo_show_layout (cr, layout_yr);
			} else {
				max_width = calitem->max_digit_width * 5;
				pango_layout_get_pixel_size (layout_yr, &text_width, NULL);

				cairo_move_to (cr, month_x + min_x + arrow_button_size + (max_width - text_width) / 2, text_y);
				pango_cairo_show_layout (cr, layout_yr);

				max_width = calitem->max_month_name_width;
				pango_layout_get_pixel_size (layout, &text_width, NULL);

				cairo_move_to (cr, month_x + month_w - arrow_button_size - (max_width - text_width) / 2 - text_width - min_x, text_y);
				pango_cairo_show_layout (cr, layout);
			}

			g_object_unref (layout_yr);
		} else {
			/* This is a strftime() format. %B = Month name, %Y = Year. */
			e_utf8_strftime (buffer, sizeof (buffer), C_("CalItem", "%B %Y"), &tmp_tm);

			pango_layout_set_font_description (layout, font_desc);
			pango_layout_set_text (layout, buffer, -1);

			/* Ideally we place the text centered in the month, but we
			 * won't go to the left of the minimum x position. */
			pango_layout_get_pixel_size (layout, &text_width, NULL);
			text_x = (calitem->month_width - text_width) / 2;
			text_x = MAX (min_x, text_x);

			cairo_move_to (cr, month_x + text_x, text_y);
			pango_cairo_show_layout (cr, layout);
		}

		cairo_restore (cr);
	}

	/* Set the clip rectangle for the main month display. */
	clip_rect.x = MAX (0, month_x);
	clip_rect.y = MAX (0, month_y);
	clip_width = month_x + month_w - clip_rect.x;
	clip_height = month_y + month_h - clip_rect.y;

	if (clip_width <= 0 || clip_height <= 0) {
		g_object_unref (layout);
		pango_font_description_free (font_desc);
		return;
	}

	clip_rect.width = clip_width;
	clip_rect.height = clip_height;

	cairo_save (cr);

	gdk_cairo_rectangle (cr, &clip_rect);
	cairo_clip (cr);

	/* Draw the day initials across the top of the month. */
	min_cell_width = MAX (calitem->max_day_width, (calitem->max_digit_width * 2))
		+ E_CALENDAR_ITEM_MIN_CELL_XPAD;

	cells_x = month_x + E_CALENDAR_ITEM_XPAD_BEFORE_WEEK_NUMBERS + calitem->month_lpad
		+ E_CALENDAR_ITEM_XPAD_BEFORE_CELLS;
	if (calitem->show_week_numbers)
		cells_x += calitem->max_week_number_digit_width * 2
			+ E_CALENDAR_ITEM_XPAD_AFTER_WEEK_NUMBERS + 1;
	text_x = cells_x + calitem->cell_width
		- (calitem->cell_width - min_cell_width) / 2;
	text_x -= E_CALENDAR_ITEM_MIN_CELL_XPAD / 2;
	text_y = month_y + ythickness * 2
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ char_height + E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_ABOVE_DAY_LETTERS + calitem->month_tpad;

	cells_y = text_y + char_height
		+ E_CALENDAR_ITEM_YPAD_BELOW_DAY_LETTERS + 1
		+ E_CALENDAR_ITEM_YPAD_ABOVE_CELLS;

	cairo_save (cr);
	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &rgba);
	gdk_cairo_set_source_rgba (cr, &rgba);
	cairo_rectangle (
		cr, cells_x ,
		text_y - E_CALENDAR_ITEM_YPAD_ABOVE_CELLS - 1,
			calitem->cell_width * 7  , cells_y - text_y);
	cairo_fill (cr);
	cairo_restore (cr);

	weekday = calitem->week_start_day;
	pango_layout_set_font_description (layout, font_desc);
	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		text_x += (7 - 1) * calitem->cell_width;
	e_utils_get_theme_color (widget, "theme_selected_fg_color,theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &rgba);
	gdk_cairo_set_source_rgba (cr, &rgba);
	for (day = 0; day < 7; day++) {
		cairo_save (cr);
		layout_set_day_text (calitem, layout, weekday);
		cairo_move_to (
			cr,
			text_x - calitem->day_widths[weekday],
			text_y);
		pango_cairo_show_layout (cr, layout);
		text_x += (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
				? -calitem->cell_width : calitem->cell_width;
		cairo_restore (cr);

		weekday = e_weekday_get_next (weekday);
	}

	/* Draw the rectangle around the week number. */
	if (calitem->show_week_numbers) {
		cairo_save (cr);
		e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &rgba);
		gdk_cairo_set_source_rgba (cr, &rgba);
		cairo_rectangle (
			cr, cells_x, cells_y - (cells_y - text_y + 2) ,
				-20, E_CALENDAR_ROWS_PER_MONTH * calitem->cell_height + 18);
		cairo_fill (cr);
		cairo_restore (cr);
	}

	e_calendar_item_draw_day_numbers (
		calitem, cr, width, height, row, col,
		year, month, start_weekday, cells_x, cells_y);

	g_object_unref (layout);
	cairo_restore (cr);
	pango_font_description_free (font_desc);
}

static const gchar *
get_digit_fomat (void)
{

#ifdef HAVE_GNU_GET_LIBC_VERSION
#include <gnu/libc-version.h>

	const gchar *libc_version = gnu_get_libc_version ();
	gchar **split = g_strsplit (libc_version, ".", -1);
	gint major = 0;
	gint minor = 0;
	gint revision = 0;

	major = atoi (split[0]);
	minor = atoi (split[1]);

	if (g_strv_length (split) > 2)
		revision = atoi (split[2]);
	g_strfreev (split);

	if (major > 2 || minor > 2 || (minor == 2 && revision > 2)) {
		return "%Id";
	}
#endif

	return "%d";
}

static void
e_calendar_item_draw_day_numbers (ECalendarItem *calitem,
                                  cairo_t *cr,
                                  gint width,
                                  gint height,
                                  gint row,
                                  gint col,
                                  gint year,
                                  gint month,
                                  GDateWeekday start_weekday,
                                  gint cells_x,
                                  gint cells_y)
{
	GnomeCanvasItem *item;
	GtkWidget *widget;
	PangoFontDescription *font_desc;
	GdkRGBA *bg_color, *fg_color, *box_color;
	GdkRGBA rgba;
	struct tm today_tm;
	time_t t;
	gint char_height, min_cell_width, min_cell_height;
	gint day_num, drow, dcol, day_x, day_y;
	gint text_x, text_y;
	gint num_chars, digit;
	gint week_num, mon, days_from_week_start;
	gint years[3], months[3], days_in_month[3];
	gboolean today, selected, has_focus, drop_target = FALSE;
	gboolean bold, italic, draw_day, finished = FALSE;
	gint today_year, today_month, today_mday, month_offset;
	gchar buffer[64];
	gint day_style = 0;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	PangoAttrList *tnum;
	PangoAttribute *attr;

	item = GNOME_CANVAS_ITEM (calitem);
	widget = GTK_WIDGET (item->canvas);

	/* Set up Pango prerequisites */
	font_desc = calitem->font_desc;

	pango_context = gtk_widget_get_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	if (!font_desc)
		font_desc = pango_context_get_font_description (pango_context);
	font_desc = pango_font_description_copy (font_desc);

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	min_cell_width = MAX (calitem->max_day_width, (calitem->max_digit_width * 2))
		+ E_CALENDAR_ITEM_MIN_CELL_XPAD;
	min_cell_height = char_height + E_CALENDAR_ITEM_MIN_CELL_YPAD;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (widget), NULL);

	/* Calculate the number of days in the previous, current, and next
	 * months. */
	years[0] = years[1] = years[2] = year;
	months[0] = month - 1;
	months[1] = month;
	months[2] = month + 1;
	if (months[0] == -1) {
		months[0] = 11;
		years[0]--;
	}
	if (months[2] == 12) {
		months[2] = 0;
		years[2]++;
	}

	days_in_month[0] = DAYS_IN_MONTH (years[0], months[0]);
	days_in_month[1] = DAYS_IN_MONTH (years[1], months[1]);
	days_in_month[2] = DAYS_IN_MONTH (years[2], months[2]);

	/* Mon 0 is the previous month, which we may show the end of. Mon 1 is
	 * the current month, and mon 2 is the next month. */
	mon = 0;

	month_offset = row * calitem->cols + col - 1;
	day_num = days_in_month[0];
	days_from_week_start = e_weekday_get_days_between (
		calitem->week_start_day, start_weekday);
	/* For the top-left month we show the end of the previous month, and
	 * if the new month starts on the first day of the week we show a
	 * complete week from the previous month. */
	if (days_from_week_start == 0) {
		if (row == 0 && col == 0) {
			day_num -= 6;
		} else {
			mon++;
			month_offset++;
			day_num = 1;
		}
	} else {
		day_num -= days_from_week_start - 1;
	}

	/* Get today's date, so we can highlight it. */
	if (calitem->time_callback) {
		today_tm = calitem->time_callback (
			calitem, calitem->time_callback_data);
	} else {
		t = time (NULL);
		today_tm = *localtime (&t);
	}
	today_year = today_tm.tm_year + 1900;
	today_month = today_tm.tm_mon;
	today_mday = today_tm.tm_mday;

	/* We usually skip the last days of the previous month (mon = 0),
	 * except for the top-left month displayed. */
	draw_day = (mon == 1 || (row == 0 && col == 0));

	tnum = pango_attr_list_new ();
	attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);

	for (drow = 0; drow < 6; drow++) {
		/* Draw the week number. */
		if (calitem->show_week_numbers) {
			week_num = e_calendar_item_get_week_number (
				calitem, day_num, months[mon], years[mon]);

			text_x = cells_x - E_CALENDAR_ITEM_XPAD_BEFORE_CELLS - 1
				- E_CALENDAR_ITEM_XPAD_AFTER_WEEK_NUMBERS;
			text_y = cells_y + drow * calitem->cell_height +
				+ (calitem->cell_height - min_cell_height + 1) / 2;

			num_chars = 0;
			if (week_num >= 10) {
				digit = week_num / 10;
				text_x -= calitem->week_number_digit_widths[digit];
				num_chars += sprintf (
					&buffer[num_chars],
					get_digit_fomat (), digit);
			}

			digit = week_num % 10;
			text_x -= calitem->week_number_digit_widths[digit] + 6;
			num_chars += sprintf (
				&buffer[num_chars],
				get_digit_fomat (), digit);

			cairo_save (cr);
			e_utils_get_theme_color (widget, "theme_selected_fg_color,theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &rgba);
			gdk_cairo_set_source_rgba (cr, &rgba);
			pango_layout_set_font_description (layout, font_desc);
			pango_layout_set_text (layout, buffer, num_chars);
			cairo_move_to (cr, text_x, text_y);
			pango_cairo_update_layout (cr, layout);
			pango_cairo_show_layout (cr, layout);
			cairo_restore (cr);
		}

		for (dcol = 0; dcol < 7; dcol++) {
			if (draw_day) {
				GdkRGBA local_bg_color, local_fg_color;

				day_x = cells_x +
					((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
					? 7 - 1 - dcol : dcol) * calitem->cell_width;

				day_y = cells_y + drow * calitem->cell_height;

				today = years[mon] == today_year
					&& months[mon] == today_month
					&& day_num == today_mday;

				selected = calitem->selection_set
					&& (calitem->selection_start_month_offset < month_offset
					    || (calitem->selection_start_month_offset == month_offset
						&& calitem->selection_start_day <= day_num))
					&& (calitem->selection_end_month_offset > month_offset
					    || (calitem->selection_end_month_offset == month_offset
						&& calitem->selection_end_day >= day_num));

				if (calitem->styles)
					day_style = calitem->styles[(month_offset + 1) * 32 + day_num];

				/* Get the colors & style to use for the day.*/
				if ((gtk_widget_has_focus (GTK_WIDGET (item->canvas))) &&
				    item->canvas->focused_item == item)
					has_focus = TRUE;
				else
					has_focus = FALSE;

				bold = FALSE;
				italic = FALSE;

				if (calitem->style_callback)
					calitem->style_callback (
						calitem,
						 years[mon],
						 months[mon],
						 day_num,
						 day_style,
						 today,
						 mon != 1,
						 selected,
						 has_focus,
						 drop_target,
						 &bg_color,
						 &fg_color,
						 &box_color,
						 &bold,
						 &italic,
						 calitem->style_callback_data);
				else
					e_calendar_item_get_day_style (
						calitem,
						years[mon],
						months[mon],
						day_num,
						day_style,
						today,
						mon != 1,
						selected,
						has_focus,
						drop_target,
						&bg_color,
						&fg_color,
						&box_color,
						&bold,
						&italic,
						&local_bg_color,
						&local_fg_color);

				/* Draw the background, if set. */
				if (bg_color) {
					cairo_save (cr);
					gdk_cairo_set_source_rgba (cr, bg_color);
					cairo_rectangle (
						cr, day_x , day_y,
						calitem->cell_width,
						calitem->cell_height);
					cairo_fill (cr);
					cairo_restore (cr);
				}

				/* Draw the box, if set. */
				if (box_color) {
					cairo_save (cr);
					gdk_cairo_set_source_rgba (cr, box_color);
					cairo_rectangle (
						cr, day_x , day_y,
						calitem->cell_width - 1,
						calitem->cell_height - 1);
					cairo_stroke (cr);
					cairo_restore (cr);
				}

				/* Draw the 1- or 2-digit day number. */
				day_x += calitem->cell_width -
					(calitem->cell_width -
					min_cell_width) / 2;
				day_x -= E_CALENDAR_ITEM_MIN_CELL_XPAD / 2;
				day_y += (calitem->cell_height - min_cell_height + 1) / 2;
				day_y += E_CALENDAR_ITEM_MIN_CELL_YPAD / 2;

				num_chars = 0;
				if (day_num >= 10) {
					digit = day_num / 10;
					day_x -= calitem->digit_widths[digit];
					num_chars += sprintf (
						&buffer[num_chars],
						get_digit_fomat (), digit);
				}

				digit = day_num % 10;
				day_x -= calitem->digit_widths[digit];
				num_chars += sprintf (
					&buffer[num_chars],
					get_digit_fomat (), digit);

				cairo_save (cr);
				if (fg_color) {
					gdk_cairo_set_source_rgba (
						cr, fg_color);
				} else {
					e_utils_get_theme_color (widget, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &rgba);
					gdk_cairo_set_source_rgba (cr, &rgba);
				}

				if (bold) {
					pango_font_description_set_weight (
						font_desc, PANGO_WEIGHT_BOLD);
				} else {
					pango_font_description_set_weight (
						font_desc, PANGO_WEIGHT_NORMAL);
				}

				if (italic) {
					pango_font_description_set_style (
						font_desc, PANGO_STYLE_ITALIC);
				} else {
					pango_font_description_set_style (
						font_desc, PANGO_STYLE_NORMAL);
				}

				pango_layout_set_font_description (layout, font_desc);
				pango_layout_set_attributes (layout, tnum);
				pango_layout_set_text (layout, buffer, num_chars);
				cairo_move_to (cr, day_x, day_y);
				pango_cairo_update_layout (cr, layout);
				pango_cairo_show_layout (cr, layout);
				cairo_restore (cr);
			}

			/* See if we've reached the end of a month. */
			if (day_num == days_in_month[mon]) {
				month_offset++;
				mon++;
				/* We only draw the start of the next month
				 * for the bottom-right month displayed. */
				if (mon == 2 && (row != calitem->rows - 1
						 || col != calitem->cols - 1)) {
					/* Set a flag so we exit the loop. */
					finished = TRUE;
					break;
				}
				day_num = 1;
				draw_day = TRUE;
			} else {
				day_num++;
			}
		}

		/* Exit the loop if the flag is set. */
		if (finished)
			break;
	}

	/* Reset pango weight and style */
	pango_font_description_set_weight (font_desc, PANGO_WEIGHT_NORMAL);
	pango_font_description_set_style (font_desc, PANGO_STYLE_NORMAL);

	g_object_unref (layout);

	pango_attr_list_unref (tnum);
	pango_font_metrics_unref (font_metrics);
	pango_font_description_free (font_desc);
}

gint
e_calendar_item_get_week_number (ECalendarItem *calitem,
                                 gint day,
                                 gint month,
                                 gint year)
{
	GDate date;
	GDateWeekday weekday;
	guint yearday;
	gint week_num;

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, day, month + 1, year);

	weekday = g_date_get_weekday (&date);

	if (g_date_valid_weekday (weekday)) {
		guint days_between;

		/* We want always point to nearest Monday as the first day
		 * of the week regardless of the calendar's week_start_day. */
		if (weekday >= G_DATE_THURSDAY) {
			days_between = e_weekday_get_days_between (
				weekday, G_DATE_MONDAY);
			g_date_add_days (&date, days_between);
		} else {
			days_between = e_weekday_get_days_between (
				G_DATE_MONDAY, weekday);
			g_date_subtract_days (&date, days_between);
		}
	}

	/* Calculate the day of the year, from 0 to 365. */
	yearday = g_date_get_day_of_year (&date) - 1;

	/* If the week starts on or after 29th December, it is week 1 of the
	 * next year, since there are 4 days in the next year. */
	if (g_date_get_month (&date) == 12 && g_date_get_day (&date) >= 29)
		return 1;

	/* Calculate the week number, from 0. */
	week_num = yearday / 7;

	/* If the first week starts on or after Jan 5th, then we need to add
	 * 1 since the previous week will really be the first week. */
	if (yearday % 7 >= 4)
		week_num++;

	/* Add 1 so week numbers are from 1 to 53. */
	return week_num + 1;
}

/* This is supposed to return the nearest item to the point and the distance.
 * Since we are the only item we just return ourself and 0 for the distance.
 * This is needed so that we get button/motion events. */
static GnomeCanvasItem *
e_calendar_item_point (GnomeCanvasItem *item,
                       gdouble x,
                       gdouble y,
                       gint cx,
                       gint cy)
{
	return item;
}

static void
e_calendar_item_stop_selecting (ECalendarItem *calitem,
                                guint32 time)
{
	if (!calitem->selecting)
		return;

	gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (calitem), time);

	calitem->selecting = FALSE;

	/* If the user selects the grayed dates before the first month or
	 * after the last month, we move backwards or forwards one month.
	 * The set_month () call should take care of updating the selection. */
	if (calitem->selection_end_month_offset == -1)
		e_calendar_item_set_first_month_with_emit (
			calitem, calitem->year,
			calitem->month - 1, FALSE);
	else if (calitem->selection_start_month_offset == calitem->rows * calitem->cols)
		e_calendar_item_set_first_month_with_emit (
			calitem, calitem->year,
			calitem->month + 1, FALSE);

	calitem->selection_changed = TRUE;
	g_clear_pointer (&calitem->selecting_axis, g_free);

	e_calendar_item_queue_signal_emission (calitem);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
}

static void
e_calendar_item_selection_add_days (ECalendarItem *calitem,
                                    gint n_days,
                                    gboolean multi_selection)
{
	GDate gdate_start, gdate_end;

	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	if (!e_calendar_item_get_selection (calitem, &gdate_start, &gdate_end)) {
		/* We set the date to the first day of the month */
		g_date_set_dmy (&gdate_start, 1, calitem->month + 1, calitem->year);
		gdate_end = gdate_start;
	}

	if (multi_selection && calitem->max_days_selected > 1) {
		gint days_between;

		days_between = g_date_days_between (&gdate_start, &gdate_end);
		if (!calitem->selecting_axis) {
			calitem->selecting_axis = g_new (GDate, 1);
			*(calitem->selecting_axis) = gdate_start;
		}
		if ((days_between != 0 &&
		     g_date_compare (calitem->selecting_axis, &gdate_end) == 0) ||
		    (days_between == 0 && n_days < 0)) {
			if (days_between - n_days > calitem->max_days_selected - 1)
				n_days = days_between + 1 - calitem->max_days_selected;
			g_date_add_days (&gdate_start, n_days);
		}
		else {
			if (days_between + n_days > calitem->max_days_selected - 1)
				n_days = calitem->max_days_selected - 1 - days_between;
			g_date_add_days (&gdate_end, n_days);
		}

		if (g_date_compare (&gdate_end, &gdate_start) < 0) {
			GDate tmp_date;
			tmp_date = gdate_start;
			gdate_start = gdate_end;
			gdate_end = tmp_date;
		}
	}
	else {
		/* clear "selecting_axis", it is only for mulit-selecting */
		g_clear_pointer (&calitem->selecting_axis, g_free);
		g_date_add_days (&gdate_start, n_days);
		gdate_end = gdate_start;
	}

	calitem->selecting = TRUE;

	e_calendar_item_set_selection_if_emission (
		calitem, &gdate_start, &gdate_end, FALSE);

	g_signal_emit_by_name (calitem, "selection_preview_changed");
}

static gint
e_calendar_item_key_press_event (ECalendarItem *calitem,
                                 GdkEvent *event)
{
	guint keyval = event->key.keyval;
	gboolean is_rtl;
	gboolean multi_selection;

	if (event->key.state & GDK_CONTROL_MASK ||
	    event->key.state & GDK_MOD1_MASK)
		return FALSE;

	is_rtl = gtk_widget_get_direction (GTK_WIDGET (GNOME_CANVAS_ITEM (calitem)->canvas)) == GTK_TEXT_DIR_RTL;
	multi_selection = event->key.state & GDK_SHIFT_MASK;

	switch (keyval) {
	case GDK_KEY_Up:
		e_calendar_item_selection_add_days (
			calitem, -7,
			multi_selection);
		break;
	case GDK_KEY_Down:
		e_calendar_item_selection_add_days (
			calitem, 7,
			multi_selection);
		break;
	case GDK_KEY_Left:
		e_calendar_item_selection_add_days (
			calitem, is_rtl ? 1 : -1,
			multi_selection);
		break;
	case GDK_KEY_Right:
		e_calendar_item_selection_add_days (
			calitem, is_rtl ? -1 : 1,
			multi_selection);
		break;
	case GDK_KEY_space:
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		e_calendar_item_stop_selecting (calitem, event->key.time);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

static gint
e_calendar_item_event (GnomeCanvasItem *item,
                       GdkEvent *event)
{
	ECalendarItem *calitem;

	calitem = E_CALENDAR_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return e_calendar_item_button_press (calitem, event);
	case GDK_BUTTON_RELEASE:
		return e_calendar_item_button_release (calitem, event);
	case GDK_MOTION_NOTIFY:
		return e_calendar_item_motion (calitem, event);
	case GDK_FOCUS_CHANGE:
		gnome_canvas_item_request_update (item);
		return FALSE;
	case GDK_KEY_PRESS:
		return e_calendar_item_key_press_event (calitem, event);
	default:
		break;
	}

	return FALSE;
}

static void
e_calendar_item_bounds (GnomeCanvasItem *item,
                        gdouble *x1,
                        gdouble *y1,
                        gdouble *x2,
                        gdouble *y2)
{
	ECalendarItem *calitem;

	g_return_if_fail (E_IS_CALENDAR_ITEM (item));

	calitem = E_CALENDAR_ITEM (item);
	*x1 = calitem->x1;
	*y1 = calitem->y1;
	*x2 = calitem->x2;
	*y2 = calitem->y2;
}

/* This checks if any fonts have changed, and if so it recalculates the
 * text sizes and the minimum month size. */
static void
e_calendar_item_recalc_sizes (ECalendarItem *calitem)
{
	GnomeCanvasItem *canvas_item;
	gint max_day_width, digit, max_digit_width, max_week_number_digit_width;
	gint char_height, width, min_cell_width, min_cell_height;
	gchar buffer[64];
	struct tm tmp_tm;
	PangoFontDescription *font_desc, *wkfont_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	PangoAttrList *tnum;
	PangoAttribute *attr;
	GDateWeekday weekday;
	GtkWidget *widget;
	GtkStyleContext *style_context;
	GtkBorder padding;

	canvas_item = GNOME_CANVAS_ITEM (calitem);
	widget = GTK_WIDGET (canvas_item->canvas);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);

	/* Set up Pango prerequisites */
	font_desc = calitem->font_desc;
	wkfont_desc = calitem->week_number_font_desc;

	pango_context = gtk_widget_create_pango_context (
		GTK_WIDGET (canvas_item->canvas));
	font_metrics = pango_context_get_metrics (
		pango_context, font_desc,
		pango_context_get_language (pango_context));
	if (!font_desc)
		font_desc = pango_context_get_font_description (pango_context);
	font_desc = pango_font_description_copy (font_desc);
	layout = pango_layout_new (pango_context);

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));

	max_day_width = 0;
	for (weekday = G_DATE_MONDAY; weekday <= G_DATE_SUNDAY; weekday++) {
		layout_set_day_text (calitem, layout, weekday);
		pango_layout_get_pixel_size (layout, &width, NULL);

		calitem->day_widths[weekday] = width;
		max_day_width = MAX (max_day_width, width);
	}
	calitem->max_day_width = max_day_width;

	tnum = pango_attr_list_new ();
	attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);
	pango_layout_set_attributes (layout, tnum);
	pango_attr_list_unref (tnum);

	max_digit_width = 0;
	max_week_number_digit_width = 0;
	for (digit = 0; digit < 10; digit++) {
		gchar locale_digit[5];
		gint locale_digit_len;

		locale_digit_len = sprintf (locale_digit, get_digit_fomat (), digit);

		pango_layout_set_text (layout, locale_digit, locale_digit_len);
		pango_layout_get_pixel_size (layout, &width, NULL);

		calitem->digit_widths[digit] = width;
		max_digit_width = MAX (max_digit_width, width);

		if (wkfont_desc) {
			pango_context_set_font_description (pango_context, wkfont_desc);
			pango_layout_context_changed (layout);

			pango_layout_set_text (layout, locale_digit, locale_digit_len);
			pango_layout_get_pixel_size (layout, &width, NULL);

			calitem->week_number_digit_widths[digit] = width;
			max_week_number_digit_width = MAX (max_week_number_digit_width, width);

			pango_context_set_font_description (pango_context, font_desc);
			pango_layout_context_changed (layout);
		} else {
			calitem->week_number_digit_widths[digit] = width;
			max_week_number_digit_width = max_digit_width;
		}
	}
	calitem->max_digit_width = max_digit_width;
	calitem->max_week_number_digit_width = max_week_number_digit_width;

	min_cell_width = MAX (calitem->max_day_width, (calitem->max_digit_width * 2))
		+ E_CALENDAR_ITEM_MIN_CELL_XPAD;
	min_cell_height = char_height + E_CALENDAR_ITEM_MIN_CELL_YPAD;

	calitem->min_month_width = E_CALENDAR_ITEM_XPAD_BEFORE_WEEK_NUMBERS
		+ E_CALENDAR_ITEM_XPAD_BEFORE_CELLS + min_cell_width * 7
		+ E_CALENDAR_ITEM_XPAD_AFTER_CELLS;
	if (calitem->show_week_numbers) {
		calitem->min_month_width += calitem->max_week_number_digit_width * 2
			+ E_CALENDAR_ITEM_XPAD_AFTER_WEEK_NUMBERS + 1;
	}

	calitem->min_month_height = padding.top * 2
		+ E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME + char_height
		+ E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME + 1
		+ E_CALENDAR_ITEM_YPAD_ABOVE_DAY_LETTERS
		+ char_height + E_CALENDAR_ITEM_YPAD_BELOW_DAY_LETTERS + 1
		+ E_CALENDAR_ITEM_YPAD_ABOVE_CELLS + min_cell_height * 6
		+ E_CALENDAR_ITEM_YPAD_BELOW_CELLS;

	calitem->max_month_name_width = 50;
	memset (&tmp_tm, 0, sizeof (tmp_tm));
	tmp_tm.tm_year = 2000 - 100;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_isdst = -1;
	for (tmp_tm.tm_mon = 0; tmp_tm.tm_mon < 12; tmp_tm.tm_mon++) {
		mktime (&tmp_tm);

		e_utf8_strftime (buffer, sizeof (buffer), C_("CalItem", "%B"), &tmp_tm);

		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		if (width > calitem->max_month_name_width)
			calitem->max_month_name_width = width;
	}

	g_object_unref (layout);
	g_object_unref (pango_context);
	pango_font_metrics_unref (font_metrics);
	pango_font_description_free (font_desc);
}

static void
e_calendar_item_get_day_style (ECalendarItem *calitem,
                               gint year,
                               gint month,
                               gint day,
                               gint day_style,
                               gboolean today,
                               gboolean prev_or_next_month,
                               gboolean selected,
                               gboolean has_focus,
                               gboolean drop_target,
                               GdkRGBA **bg_color,
                               GdkRGBA **fg_color,
                               GdkRGBA **box_color,
                               gboolean *bold,
                               gboolean *italic,
			       GdkRGBA *local_bg_color,
			       GdkRGBA *local_fg_color)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (GNOME_CANVAS_ITEM (calitem)->canvas);

	*bg_color = NULL;
	*fg_color = NULL;
	*box_color = NULL;

	*bold = (day_style & E_CALENDAR_ITEM_MARK_BOLD) ==
		E_CALENDAR_ITEM_MARK_BOLD;
	*italic = (day_style & E_CALENDAR_ITEM_MARK_ITALIC) ==
		E_CALENDAR_ITEM_MARK_ITALIC;

	if (today)
		*box_color = &calitem->colors[E_CALENDAR_ITEM_COLOR_TODAY_BOX];

	if (prev_or_next_month) {
		*fg_color = local_fg_color;
		e_utils_get_theme_color (widget, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, local_fg_color);
	}

	if (selected) {
		*bg_color = local_bg_color;
		*fg_color = local_fg_color;

		if (has_focus) {
			e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, local_bg_color);
			e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, local_fg_color);
		} else {
			GdkRGBA base_bg;

			e_utils_get_theme_color (widget, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, local_bg_color);
			e_utils_get_theme_color (widget, "theme_unfocused_selected_fg_color,theme_selected_fg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_FG_COLOR, local_fg_color);

			e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &base_bg);

			if (local_bg_color->red == base_bg.red &&
			    local_bg_color->green == base_bg.green &&
			    local_bg_color->blue == base_bg.blue) {
				e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, local_bg_color);
				e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, local_fg_color);
			}
		}
	}
}

static gboolean
e_calendar_item_button_press (ECalendarItem *calitem,
                              GdkEvent *button_event)
{
	GdkGrabStatus grab_status;
	GdkDevice *event_device;
	guint event_button = 0;
	guint32 event_time;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;
	gint month_offset, day, add_days = 0;
	gboolean all_week, round_up_end = FALSE, round_down_start = FALSE;

	gdk_event_get_button (button_event, &event_button);
	gdk_event_get_coords (button_event, &event_x_win, &event_y_win);
	event_device = gdk_event_get_device (button_event);
	event_time = gdk_event_get_time (button_event);

	if (event_button == 4)
		e_calendar_item_set_first_month_with_emit (
			calitem, calitem->year,
			calitem->month - 1, TRUE);
	else if (event_button == 5)
		e_calendar_item_set_first_month_with_emit (
			calitem, calitem->year,
			calitem->month + 1, TRUE);

	if (!e_calendar_item_convert_position_to_day (calitem,
						      event_x_win,
						      event_y_win,
						      TRUE,
						      &month_offset, &day,
						      &all_week))
		return FALSE;

	if (event_button == 3 && day == -1
	    && e_calendar_item_get_display_popup (calitem)) {
		e_calendar_item_show_popup_menu (
			calitem, button_event, month_offset);
		return TRUE;
	}

	if (event_button != 1 || day == -1)
		return FALSE;

	if (calitem->max_days_selected < 1)
		return TRUE;

	grab_status = gnome_canvas_item_grab (
		GNOME_CANVAS_ITEM (calitem),
		GDK_POINTER_MOTION_MASK |
		GDK_BUTTON_RELEASE_MASK,
		NULL,
		event_device,
		event_time);

	if (grab_status != GDK_GRAB_SUCCESS)
		return FALSE;

	if (all_week && calitem->keep_wdays_on_weeknum_click) {
		gint tmp_start_moff, tmp_start_day;

		tmp_start_moff = calitem->selection_start_month_offset;
		tmp_start_day = calitem->selection_start_day;
		e_calendar_item_round_down_selection (
			calitem, &tmp_start_moff, &tmp_start_day);

		e_calendar_item_round_down_selection (calitem, &month_offset, &day);
		month_offset += calitem->selection_start_month_offset - tmp_start_moff;
		day += calitem->selection_start_day - tmp_start_day;

		/* keep same count of days selected */
		add_days = e_calendar_item_get_inclusive_days (
			calitem,
			calitem->selection_start_month_offset,
			calitem->selection_start_day,
			calitem->selection_end_month_offset,
			calitem->selection_end_day) - 1;
	}

	calitem->selection_set = TRUE;
	calitem->selection_start_month_offset = month_offset;
	calitem->selection_start_day = day;
	calitem->selection_end_month_offset = month_offset;
	calitem->selection_end_day = day;

	if (add_days > 0)
		e_calendar_item_add_days_to_selection (calitem, add_days);

	calitem->selection_real_start_month_offset = month_offset;
	calitem->selection_real_start_day = day;

	calitem->selection_from_full_week = FALSE;
	calitem->selecting = TRUE;
	calitem->selection_dragging_end = TRUE;

	if (all_week && !calitem->keep_wdays_on_weeknum_click) {
		calitem->selection_from_full_week = TRUE;
		round_up_end = TRUE;
	}

	if (calitem->days_to_start_week_selection == 1) {
		round_down_start = TRUE;
		round_up_end = TRUE;
	}

	/* Don't round up or down if we can't select a week or more,
	 * or when keeping week days. */
	if (calitem->max_days_selected < 7 ||
		(all_week && calitem->keep_wdays_on_weeknum_click)) {
		round_down_start = FALSE;
		round_up_end = FALSE;
	}

	if (round_up_end)
		e_calendar_item_round_up_selection (
			calitem, &calitem->selection_end_month_offset,
			&calitem->selection_end_day);

	if (round_down_start)
		e_calendar_item_round_down_selection (
			calitem, &calitem->selection_start_month_offset,
			&calitem->selection_start_day);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));

	return TRUE;
}

static gboolean
e_calendar_item_button_release (ECalendarItem *calitem,
                                GdkEvent *button_event)
{
	guint32 event_time;

	event_time = gdk_event_get_time (button_event);
	e_calendar_item_stop_selecting (calitem, event_time);

	return FALSE;
}

static gboolean
e_calendar_item_motion (ECalendarItem *calitem,
                        GdkEvent *event)
{
	gint start_month, start_day, end_month, end_day, month_offset, day;
	gint tmp_month, tmp_day, days_in_selection;
	gboolean all_week, round_up_end = FALSE, round_down_start = FALSE;

	if (!calitem->selecting)
		return FALSE;

	if (!e_calendar_item_convert_position_to_day (calitem,
						      event->button.x,
						      event->button.y,
						      TRUE,
						      &month_offset, &day,
						      &all_week))
		return FALSE;

	if (day == -1)
		return FALSE;

	if (calitem->selection_dragging_end) {
		start_month = calitem->selection_real_start_month_offset;
		start_day = calitem->selection_real_start_day;
		end_month = month_offset;
		end_day = day;
	} else {
		start_month = month_offset;
		start_day = day;
		end_month = calitem->selection_real_start_month_offset;
		end_day = calitem->selection_real_start_day;
	}

	if (start_month > end_month || (start_month == end_month
					&& start_day > end_day)) {
		tmp_month = start_month;
		tmp_day = start_day;
		start_month = end_month;
		start_day = end_day;
		end_month = tmp_month;
		end_day = tmp_day;

		calitem->selection_dragging_end =
			!calitem->selection_dragging_end;
	}

	if (calitem->days_to_start_week_selection > 0) {
		days_in_selection = e_calendar_item_get_inclusive_days (
			calitem, start_month, start_day, end_month, end_day);
		if (days_in_selection >= calitem->days_to_start_week_selection) {
			round_down_start = TRUE;
			round_up_end = TRUE;
		}
	}

	/* If we are over a week number and we are dragging the end of the
	 * selection, we round up to the end of this week. */
	if (all_week && calitem->selection_dragging_end)
		round_up_end = TRUE;

	/* If the selection was started from a week number and we are dragging
	 * the start of the selection, we need to round up the end to include
	 * all of the original week selected. */
	if (calitem->selection_from_full_week
	    && !calitem->selection_dragging_end)
			round_up_end = TRUE;

	/* Don't round up or down if we can't select a week or more. */
	if (calitem->max_days_selected < 7) {
		round_down_start = FALSE;
		round_up_end = FALSE;
	}

	if (round_up_end)
		e_calendar_item_round_up_selection (
			calitem, &end_month,
			&end_day);
	if (round_down_start)
		e_calendar_item_round_down_selection (
			calitem, &start_month,
			&start_day);

	/* Check we don't go over the maximum number of days to select. */
	if (calitem->selection_dragging_end) {
		e_calendar_item_check_selection_end (
			calitem,
			start_month,
			start_day,
			&end_month,
			&end_day);
	} else {
		e_calendar_item_check_selection_start (
			calitem,
			&start_month,
			&start_day,
			end_month,
			end_day);
	}

	if (start_month == calitem->selection_start_month_offset
	    && start_day == calitem->selection_start_day
	    && end_month == calitem->selection_end_month_offset
	    && end_day == calitem->selection_end_day)
		return FALSE;

	calitem->selection_start_month_offset = start_month;
	calitem->selection_start_day = start_day;
	calitem->selection_end_month_offset = end_month;
	calitem->selection_end_day = end_day;

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));

	return TRUE;
}

static void
e_calendar_item_check_selection_end (ECalendarItem *calitem,
                                     gint start_month,
                                     gint start_day,
                                     gint *end_month,
                                     gint *end_day)
{
	gint year, month, max_month, max_day, days_in_month;

	if (calitem->max_days_selected <= 0)
		return;

	year = calitem->year;
	month = calitem->month + start_month;
	e_calendar_item_normalize_date (calitem, &year, &month);

	max_month = start_month;
	max_day = start_day + calitem->max_days_selected - 1;

	for (;;) {
		days_in_month = DAYS_IN_MONTH (year, month);
		if (max_day <= days_in_month)
			break;
		max_month++;
		month++;
		if (month == 12) {
			year++;
			month = 0;
		}
		max_day -= days_in_month;
	}

	if (*end_month > max_month) {
		*end_month = max_month;
		*end_day = max_day;
	} else if (*end_month == max_month && *end_day > max_day) {
		*end_day = max_day;
	}
}

static void
e_calendar_item_check_selection_start (ECalendarItem *calitem,
                                       gint *start_month,
                                       gint *start_day,
                                       gint end_month,
                                       gint end_day)
{
	gint year, month, min_month, min_day, days_in_month;

	if (calitem->max_days_selected <= 0)
		return;

	year = calitem->year;
	month = calitem->month + end_month;
	e_calendar_item_normalize_date (calitem, &year, &month);

	min_month = end_month;
	min_day = end_day - calitem->max_days_selected + 1;

	while (min_day <= 0) {
		min_month--;
		month--;
		if (month == -1) {
			year--;
			month = 11;
		}
		days_in_month = DAYS_IN_MONTH (year, month);
		min_day += days_in_month;
	}

	if (*start_month < min_month) {
		*start_month = min_month;
		*start_day = min_day;
	} else if (*start_month == min_month && *start_day < min_day) {
		*start_day = min_day;
	}
}

/* Converts a position within the item to a month & day.
 * The month returned is 0 for the top-left month displayed.
 * If the position is over the month heading -1 is returned for the day.
 * If the position is over a week number the first day of the week is returned
 * and entire_week is set to TRUE.
 * It returns FALSE if the position is completely outside all months. */
static gboolean
e_calendar_item_convert_position_to_day (ECalendarItem *calitem,
                                         gint event_x,
                                         gint event_y,
                                         gboolean round_empty_positions,
                                         gint *month_offset,
                                         gint *day,
                                         gboolean *entire_week)
{
	GnomeCanvasItem *item;
	GtkWidget *widget;
	GtkStyleContext *style_context;
	GtkBorder padding;
	gint xthickness, ythickness, char_height;
	gint x, y, row, col, cells_x, cells_y, day_row, day_col;
	gint first_day_offset, days_in_month, days_in_prev_month;
	gint week_num_x1, week_num_x2;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;

	item = GNOME_CANVAS_ITEM (calitem);
	widget = GTK_WIDGET (item->canvas);
	style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_padding (style_context, gtk_style_context_get_state (style_context), &padding);

	pango_context = gtk_widget_create_pango_context (widget);
	font_metrics = pango_context_get_metrics (
		pango_context, calitem->font_desc,
		pango_context_get_language (pango_context));

	char_height =
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics));
	xthickness = padding.left;
	ythickness = padding.top;

	pango_font_metrics_unref (font_metrics);
	g_object_unref (pango_context);

	*entire_week = FALSE;

	x = event_x - xthickness - calitem->x_offset;
	y = event_y - ythickness;

	if (x < 0 || y < 0)
		return FALSE;

	row = y / calitem->month_height;
	col = x / calitem->month_width;

	if (row >= calitem->rows || col >= calitem->cols)
		return FALSE;
	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		col = calitem->cols - 1 - col;

	*month_offset = row * calitem->cols + col;

	x = x % calitem->month_width;
	y = y % calitem->month_height;

	if (y < ythickness * 2 + E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
	    + char_height + E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME) {
		*day = -1;
		return TRUE;
	}

	cells_y = ythickness * 2 + E_CALENDAR_ITEM_YPAD_ABOVE_MONTH_NAME
		+ char_height + E_CALENDAR_ITEM_YPAD_BELOW_MONTH_NAME
		+ E_CALENDAR_ITEM_YPAD_ABOVE_DAY_LETTERS + calitem->month_tpad
		+ char_height + E_CALENDAR_ITEM_YPAD_BELOW_DAY_LETTERS + 1
		+ E_CALENDAR_ITEM_YPAD_ABOVE_CELLS;
	y -= cells_y;
	if (y < 0)
		return FALSE;
	day_row = y / calitem->cell_height;
	if (day_row >= E_CALENDAR_ROWS_PER_MONTH)
		return FALSE;

	week_num_x1 = E_CALENDAR_ITEM_XPAD_BEFORE_WEEK_NUMBERS + calitem->month_lpad;

	if (calitem->show_week_numbers) {
		week_num_x2 = week_num_x1
			+ calitem->max_week_number_digit_width * 2;
		if (x >= week_num_x1 && x < week_num_x2)
			*entire_week = TRUE;
		cells_x = week_num_x2 + E_CALENDAR_ITEM_XPAD_AFTER_WEEK_NUMBERS + 1;
	} else {
		cells_x = week_num_x1;
	}

	if (*entire_week) {
		day_col = 0;
	} else {
		cells_x += E_CALENDAR_ITEM_XPAD_BEFORE_CELLS;
		x -= cells_x;
		if (x < 0)
			return FALSE;
		day_col = x / calitem->cell_width;
		if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
			day_col = E_CALENDAR_COLS_PER_MONTH - 1 - day_col;
		if (day_col >= E_CALENDAR_COLS_PER_MONTH)
			return FALSE;
	}

	*day = day_row * E_CALENDAR_COLS_PER_MONTH + day_col;

	e_calendar_item_get_month_info (
		calitem, row, col, &first_day_offset,
		&days_in_month, &days_in_prev_month);
	if (*day < first_day_offset) {
		if (*entire_week || (row == 0 && col == 0)) {
			(*month_offset)--;
			*day = days_in_prev_month + 1 - first_day_offset
				+ *day;
			return TRUE;
		} else if (round_empty_positions) {
			*day = first_day_offset;
		} else {
			return FALSE;
		}
	}

	*day -= first_day_offset - 1;

	if (*day > days_in_month) {
		if (row == calitem->rows - 1 && col == calitem->cols - 1) {
			(*month_offset)++;
			*day -= days_in_month;
			return TRUE;
		} else if (round_empty_positions) {
			*day = days_in_month;
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

static void
e_calendar_item_get_month_info (ECalendarItem *calitem,
                                gint row,
                                gint col,
                                gint *first_day_offset,
                                gint *days_in_month,
                                gint *days_in_prev_month)
{
	GDateWeekday start_weekday;
	gint year, month, first_day_of_month;
	struct tm tmp_tm = { 0 };

	month = calitem->month + row * calitem->cols + col;
	year = calitem->year + month / 12;
	month = month % 12;

	*days_in_month = DAYS_IN_MONTH (year, month);
	if (month == 0)
		*days_in_prev_month = DAYS_IN_MONTH (year - 1, 11);
	else
		*days_in_prev_month = DAYS_IN_MONTH (year, month - 1);

	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = 1;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);

	start_weekday = e_weekday_from_tm_wday (tmp_tm.tm_wday);

	first_day_of_month = e_weekday_get_days_between (
		calitem->week_start_day, start_weekday);

	if (row == 0 && col == 0 && first_day_of_month == 0)
		*first_day_offset = 7;
	else
		*first_day_offset = first_day_of_month;
}

void
e_calendar_item_get_first_month (ECalendarItem *calitem,
                                 gint *year,
                                 gint *month)
{
	*year = calitem->year;
	*month = calitem->month;
}

gboolean
e_calendar_item_convert_position_to_date (ECalendarItem *calitem,
					  gint event_x,
					  gint event_y,
					  GDate *date)
{
	gint month_offset = -1;
	gint day = -1, dday, dmonth, dyear;
	gboolean entire_week = FALSE;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (calitem), FALSE);
	g_return_val_if_fail (date != NULL, FALSE);

	if (calitem->rows == 0 || calitem->cols == 0)
		return FALSE;

	if (!e_calendar_item_convert_position_to_day (calitem, event_x, event_y, FALSE, &month_offset, &day, &entire_week) ||
	    day < 0 || entire_week)
		return FALSE;

	dyear = calitem->year;
	dmonth = calitem->month + month_offset;
	e_calendar_item_normalize_date (calitem, &dyear, &dmonth);
	dday = day;

	g_date_set_dmy (date, dday, dmonth + 1, dyear);

	return g_date_valid (date);
}

static void
e_calendar_item_preserve_day_selection (ECalendarItem *calitem,
                                        gint selected_day,
                                        gint *month_offset,
                                        gint *day)
{
	gint year, month, weekday, days, days_in_month;
	struct tm tmp_tm = { 0 };

	year = calitem->year;
	month = calitem->month + *month_offset;
	e_calendar_item_normalize_date (calitem, &year, &month);

	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = *day;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);

	/* Convert to 0 (Monday) to 6 (Sunday). */
	weekday = (tmp_tm.tm_wday + 6) % 7;

	/* Calculate how many days to the start of the row. */
	days = (weekday + 7 - selected_day) % 7;

	*day -= days;
	if (*day <= 0) {
		month--;
		if (month == -1) {
			year--;
			month = 11;
		}
		days_in_month = DAYS_IN_MONTH (year, month);
		(*month_offset)--;
		*day += days_in_month;
	}
}

/* This also handles values of month < 0 or > 11 by updating the year. */
static void
e_calendar_item_set_first_month_with_emit (ECalendarItem *calitem,
					   gint year,
					   gint month,
					   gboolean emit_date_range_moved)
{
	gint new_year, new_month, months_diff, num_months;
	gint old_days_in_selection, new_days_in_selection;

	new_year = year;
	new_month = month;
	e_calendar_item_normalize_date (calitem, &new_year, &new_month);

	if (calitem->year == new_year && calitem->month == new_month)
		return;

	/* Update the selection. */
	num_months = calitem->rows * calitem->cols;
	months_diff = (new_year - calitem->year) * 12
		+ new_month - calitem->month;

	if (calitem->selection_set) {
		if (!calitem->move_selection_when_moving
		    || (calitem->selection_start_month_offset - months_diff >= 0
			&& calitem->selection_end_month_offset - months_diff < num_months)) {
			calitem->selection_start_month_offset -= months_diff;
			calitem->selection_end_month_offset -= months_diff;
			calitem->selection_real_start_month_offset -= months_diff;

			calitem->year = new_year;
			calitem->month = new_month;
		} else {
			gint selected_day;
			struct tm tmp_tm = { 0 };

			old_days_in_selection = e_calendar_item_get_inclusive_days (
				calitem,
				calitem->selection_start_month_offset,
				calitem->selection_start_day,
				calitem->selection_end_month_offset,
				calitem->selection_end_day);

			/* Calculate the currently selected day */
			tmp_tm.tm_year = calitem->year - 1900;
			tmp_tm.tm_mon = calitem->month + calitem->selection_start_month_offset;
			tmp_tm.tm_mday = calitem->selection_start_day;
			tmp_tm.tm_isdst = -1;
			mktime (&tmp_tm);

			selected_day = (tmp_tm.tm_wday + 6) % 7;

			/* Make sure the selection will be displayed. */
			if (calitem->selection_start_month_offset < 0
			    || calitem->selection_start_month_offset >= num_months) {
				calitem->selection_end_month_offset -=
					calitem->selection_start_month_offset;
				calitem->selection_start_month_offset = 0;
			}

			/* We want to ensure that the same number of days are
			 * selected after we have moved the selection. */
			calitem->year = new_year;
			calitem->month = new_month;

			e_calendar_item_ensure_valid_day (
				calitem, &calitem->selection_start_month_offset,
				&calitem->selection_start_day);
			e_calendar_item_ensure_valid_day (
				calitem, &calitem->selection_end_month_offset,
				&calitem->selection_end_day);

			if (calitem->preserve_day_when_moving) {
				e_calendar_item_preserve_day_selection (
					calitem, selected_day,
					&calitem->selection_start_month_offset,
					&calitem->selection_start_day);
			}

			new_days_in_selection = e_calendar_item_get_inclusive_days (
				calitem,
				calitem->selection_start_month_offset,
				calitem->selection_start_day,
				calitem->selection_end_month_offset,
				calitem->selection_end_day);

			if (old_days_in_selection != new_days_in_selection)
				e_calendar_item_add_days_to_selection (
					calitem, old_days_in_selection -
					new_days_in_selection);

			/* Flag that we need to emit the "selection_changed"
			 * signal. We don't want to emit it here since setting
			 * the "year" and "month" args would result in 2
			 * signals emitted. */
			calitem->selection_changed = TRUE;
		}
	} else {
		calitem->year = new_year;
		calitem->month = new_month;
	}

	e_calendar_item_date_range_changed (calitem);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));

	if (emit_date_range_moved)
		g_signal_emit (calitem, e_calendar_item_signals[DATE_RANGE_MOVED], 0);
}

/* This also handles values of month < 0 or > 11 by updating the year. */
void
e_calendar_item_set_first_month (ECalendarItem *calitem,
				 gint year,
				 gint month)
{
	e_calendar_item_set_first_month_with_emit (calitem, year, month, TRUE);
}

/* Get the maximum number of days selectable */
gint
e_calendar_item_get_max_days_sel (ECalendarItem *calitem)
{
	return calitem->max_days_selected;
}

/* Set the maximum number of days selectable */
void
e_calendar_item_set_max_days_sel (ECalendarItem *calitem,
                                  gint days)
{
	calitem->max_days_selected = MAX (0, days);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
}

/* Get the maximum number of days before whole weeks are selected */
gint
e_calendar_item_get_days_start_week_sel (ECalendarItem *calitem)
{
	return calitem->days_to_start_week_selection;
}

/* Set the maximum number of days before whole weeks are selected */
void
e_calendar_item_set_days_start_week_sel (ECalendarItem *calitem,
                                         gint days)
{
	calitem->days_to_start_week_selection = days;
}

gboolean
e_calendar_item_get_display_popup (ECalendarItem *calitem)
{
	return calitem->display_popup;
}

void
e_calendar_item_set_display_popup (ECalendarItem *calitem,
                                   gboolean display)
{
	calitem->display_popup = display;
}

/* This will make sure that the given year & month are valid, i.e. if month
 * is < 0 or > 11 the year and month will be updated accordingly. */
void
e_calendar_item_normalize_date (ECalendarItem *calitem,
                                gint *year,
                                gint *month)
{
	if (*month >= 0) {
		*year += *month / 12;
		*month = *month % 12;
	} else {
		*year += *month / 12 - 1;
		*month = *month % 12;
		if (*month != 0)
			*month += 12;
	}
}

/* Adds or subtracts days from the selection. It is used when we switch months
 * and the selection extends past the end of a month but we want to keep the
 * number of days selected the same. days should not be more than 30. */
static void
e_calendar_item_add_days_to_selection (ECalendarItem *calitem,
                                       gint days)
{
	gint year, month, days_in_month;

	year = calitem->year;
	month = calitem->month + calitem->selection_end_month_offset;
	e_calendar_item_normalize_date (calitem, &year,	&month);

	calitem->selection_end_day += days;
	if (calitem->selection_end_day <= 0) {
		month--;
		e_calendar_item_normalize_date (calitem, &year,	&month);
		calitem->selection_end_month_offset--;
		calitem->selection_end_day += DAYS_IN_MONTH (year, month);
	} else {
		days_in_month = DAYS_IN_MONTH (year, month);
		if (calitem->selection_end_day > days_in_month) {
			calitem->selection_end_month_offset++;
			calitem->selection_end_day -= days_in_month;
		}
	}
}

/* Gets the range of dates actually shown. Months are 0 to 11.
 * This also includes the last days of the previous month and the first days
 * of the following month, which are normally shown in gray.
 * It returns FALSE if no dates are currently shown. */
gboolean
e_calendar_item_get_date_range (ECalendarItem *calitem,
                                gint *start_year,
                                gint *start_month,
                                gint *start_day,
                                gint *end_year,
                                gint *end_month,
                                gint *end_day)
{
	gint first_day_offset, days_in_month, days_in_prev_month;

	if (calitem->rows == 0 || calitem->cols == 0)
		return FALSE;

	/* Calculate the first day shown. This will be one of the greyed-out
	 * days before the first full month begins. */
	e_calendar_item_get_month_info (
		calitem, 0, 0, &first_day_offset,
		&days_in_month, &days_in_prev_month);
	*start_year = calitem->year;
	*start_month = calitem->month - 1;
	if (*start_month == -1) {
		(*start_year)--;
		*start_month = 11;
	}
	*start_day = days_in_prev_month + 1 - first_day_offset;

	/* Calculate the last day shown. This will be one of the greyed-out
	 * days after the last full month ends. */
	e_calendar_item_get_month_info (
		calitem, calitem->rows - 1,
		calitem->cols - 1, &first_day_offset,
		&days_in_month, &days_in_prev_month);
	*end_month = calitem->month + calitem->rows * calitem->cols;
	*end_year = calitem->year + *end_month / 12;
	*end_month %= 12;
	*end_day = E_CALENDAR_ROWS_PER_MONTH * E_CALENDAR_COLS_PER_MONTH
		- first_day_offset - days_in_month;

	return TRUE;
}

/* Simple way to mark days so they appear bold.
 * A more flexible interface may be added later. */
void
e_calendar_item_clear_marks (ECalendarItem *calitem)
{
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (calitem);

	g_free (calitem->styles);
	calitem->styles = NULL;

	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1,
		item->x2, item->y2);
}

/* add_day_style - whether bit-or with the actual style or change the style fully */
void
e_calendar_item_mark_day (ECalendarItem *calitem,
                          gint year,
                          gint month,
                          gint day,
                          guint8 day_style,
                          gboolean add_day_style)
{
	gint month_offset;
	gint index;

	month_offset = (year - calitem->year) * 12 + month - calitem->month;
	if (month_offset < -1 || month_offset > calitem->rows * calitem->cols)
		return;

	if (!calitem->styles)
		calitem->styles = g_new0 (guint8, (calitem->rows * calitem->cols + 2) * 32);

	index = (month_offset + 1) * 32 + day;
	calitem->styles[index] = day_style |
		(add_day_style ? calitem->styles[index] : 0);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
}

void
e_calendar_item_mark_days (ECalendarItem *calitem,
                           gint start_year,
                           gint start_month,
                           gint start_day,
                           gint end_year,
                           gint end_month,
                           gint end_day,
                           guint8 day_style,
                           gboolean add_day_style)
{
	gint month_offset, end_month_offset, day;

	month_offset = (start_year - calitem->year) * 12 + start_month
		- calitem->month;
	day = start_day;
	if (month_offset > calitem->rows * calitem->cols)
		return;
	if (month_offset < -1) {
		month_offset = -1;
		day = 1;
	}

	end_month_offset = (end_year - calitem->year) * 12 + end_month
		- calitem->month;
	if (end_month_offset < -1)
		return;
	if (end_month_offset > calitem->rows * calitem->cols) {
		end_month_offset = calitem->rows * calitem->cols;
		end_day = 31;
	}

	if (month_offset > end_month_offset)
		return;

	if (!calitem->styles)
		calitem->styles = g_new0 (guint8, (calitem->rows * calitem->cols + 2) * 32);

	for (;;) {
		gint index;

		if (month_offset == end_month_offset && day > end_day)
			break;

		if (month_offset < -1 || month_offset > calitem->rows * calitem->cols)
			g_warning ("Bad month offset: %i\n", month_offset);
		if (day < 1 || day > 31)
			g_warning ("Bad day: %i\n", day);

#if 0
		g_print ("Marking Month:%i Day:%i\n", month_offset, day);
#endif
		index = (month_offset + 1) * 32 + day;
		calitem->styles[index] = day_style |
			(add_day_style ? calitem->styles[index] : 0);

		day++;
		if (day == 32) {
			month_offset++;
			day = 1;
			if (month_offset > end_month_offset)
				break;
		}
	}

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
}

/* Rounds up the given day to the end of the week. */
static void
e_calendar_item_round_up_selection (ECalendarItem *calitem,
                                    gint *month_offset,
                                    gint *day)
{
	GDateWeekday weekday;
	gint year, month, days, days_in_month;
	struct tm tmp_tm = { 0 };

	year = calitem->year;
	month = calitem->month + *month_offset;
	e_calendar_item_normalize_date (calitem, &year, &month);

	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = *day;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);

	/* Calculate how many days to the end of the row. */
	weekday = e_weekday_from_tm_wday (tmp_tm.tm_wday);
	days = e_weekday_get_days_between (weekday, calitem->week_start_day);

	*day += days;
	days_in_month = DAYS_IN_MONTH (year, month);
	if (*day > days_in_month) {
		(*month_offset)++;
		*day -= days_in_month;
	}
}

/* Rounds down the given day to the start of the week. */
static void
e_calendar_item_round_down_selection (ECalendarItem *calitem,
                                      gint *month_offset,
                                      gint *day)
{
	GDateWeekday weekday;
	gint year, month, days, days_in_month;
	struct tm tmp_tm = { 0 };

	year = calitem->year;
	month = calitem->month + *month_offset;
	e_calendar_item_normalize_date (calitem, &year, &month);

	tmp_tm.tm_year = year - 1900;
	tmp_tm.tm_mon = month;
	tmp_tm.tm_mday = *day;
	tmp_tm.tm_isdst = -1;
	mktime (&tmp_tm);

	/* Calculate how many days to the start of the row. */
	weekday = e_weekday_from_tm_wday (tmp_tm.tm_wday);
	days = e_weekday_get_days_between (weekday, calitem->week_start_day);

	*day -= days;
	if (*day <= 0) {
		month--;
		if (month == -1) {
			year--;
			month = 11;
		}
		days_in_month = DAYS_IN_MONTH (year, month);
		(*month_offset)--;
		*day += days_in_month;
	}
}

static gint
e_calendar_item_get_inclusive_days (ECalendarItem *calitem,
                                    gint start_month_offset,
                                    gint start_day,
                                    gint end_month_offset,
                                    gint end_day)
{
	gint start_year, start_month, end_year, end_month, days = 0;

	start_year = calitem->year;
	start_month = calitem->month + start_month_offset;
	e_calendar_item_normalize_date (calitem, &start_year, &start_month);

	end_year = calitem->year;
	end_month = calitem->month + end_month_offset;
	e_calendar_item_normalize_date (calitem, &end_year, &end_month);

	while (start_year < end_year || start_month < end_month) {
		days += DAYS_IN_MONTH (start_year, start_month);
		start_month++;
		if (start_month == 12) {
			start_year++;
			start_month = 0;
		}
	}

	days += end_day - start_day + 1;

	return days;
}

/* If the day is off the end of the month it is set to the last day of the
 * month. */
static void
e_calendar_item_ensure_valid_day (ECalendarItem *calitem,
                                  gint *month_offset,
                                  gint *day)
{
	gint year, month, days_in_month;

	year = calitem->year;
	month = calitem->month + *month_offset;
	e_calendar_item_normalize_date (calitem, &year, &month);

	days_in_month = DAYS_IN_MONTH (year, month);
	if (*day > days_in_month)
		*day = days_in_month;
}

gboolean
e_calendar_item_get_selection (ECalendarItem *calitem,
                               GDate *start_date,
                               GDate *end_date)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;

	g_date_clear (start_date, 1);
	g_date_clear (end_date, 1);

	if (!calitem->selection_set)
		return FALSE;

	start_year = calitem->year;
	start_month = calitem->month + calitem->selection_start_month_offset;
	e_calendar_item_normalize_date (calitem, &start_year, &start_month);
	start_day = calitem->selection_start_day;

	end_year = calitem->year;
	end_month = calitem->month + calitem->selection_end_month_offset;
	e_calendar_item_normalize_date (calitem, &end_year, &end_month);
	end_day = calitem->selection_end_day;

	g_date_set_dmy (start_date, start_day, start_month + 1, start_year);
	g_date_set_dmy (end_date, end_day, end_month + 1, end_year);

	return TRUE;
}

static void
e_calendar_item_set_selection_if_emission (ECalendarItem *calitem,
                                           const GDate *start_date,
                                           const GDate *end_date,
                                           gboolean emission)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	gint new_start_month_offset, new_start_day;
	gint new_end_month_offset, new_end_day;
	gboolean need_update;

	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	/* If start_date is NULL, we clear the selection without changing the
	 * month shown. */
	if (start_date == NULL) {
		calitem->selection_set = FALSE;
		calitem->selection_changed = TRUE;
		e_calendar_item_queue_signal_emission (calitem);
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
		return;
	}

	if (end_date == NULL)
		end_date = start_date;

	g_return_if_fail (g_date_compare (start_date, end_date) <= 0);

	start_year = g_date_get_year (start_date);
	start_month = g_date_get_month (start_date) - 1;
	start_day = g_date_get_day (start_date);
	end_year = g_date_get_year (end_date);
	end_month = g_date_get_month (end_date) - 1;
	end_day = g_date_get_day (end_date);

	need_update = e_calendar_item_ensure_days_visible (
		calitem,
		start_year,
		start_month,
		start_day,
		end_year,
		end_month,
		end_day,
		emission);

	new_start_month_offset = (start_year - calitem->year) * 12
		+ start_month - calitem->month;
	new_start_day = start_day;

	/* This may go outside the visible months, but we don't care. */
	new_end_month_offset = (end_year - calitem->year) * 12
		+ end_month - calitem->month;
	new_end_day = end_day;

	if (!calitem->selection_set
	    || calitem->selection_start_month_offset != new_start_month_offset
	    || calitem->selection_start_day != new_start_day
	    || calitem->selection_end_month_offset != new_end_month_offset
	    || calitem->selection_end_day != new_end_day) {
		need_update = TRUE;
		if (emission) {
			calitem->selection_changed = TRUE;
			e_calendar_item_queue_signal_emission (calitem);
		}
		calitem->selection_set = TRUE;
		calitem->selection_start_month_offset = new_start_month_offset;
		calitem->selection_start_day = new_start_day;
		calitem->selection_end_month_offset = new_end_month_offset;
		calitem->selection_end_day = new_end_day;

		calitem->selection_real_start_month_offset = new_start_month_offset;
		calitem->selection_real_start_day = new_start_day;
		calitem->selection_from_full_week = FALSE;
	}

	if (need_update) {
		g_signal_emit (
			calitem,
			e_calendar_item_signals[DATE_RANGE_CHANGED], 0);
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (calitem));
	}
}

void
e_calendar_item_style_updated (GtkWidget *widget,
			       ECalendarItem *calitem)
{
	GdkRGBA unfocused_selected_bg, selected_bg, fg, base_bg;

	e_utils_get_theme_color (widget, "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &selected_bg);
	e_utils_get_theme_color (widget, "theme_unfocused_selected_bg_color,theme_selected_bg_color", E_UTILS_DEFAULT_THEME_UNFOCUSED_SELECTED_BG_COLOR, &unfocused_selected_bg);
	e_utils_get_theme_color (widget, "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &fg);
	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &base_bg);

	if (gdk_rgba_equal (&selected_bg, &unfocused_selected_bg))
		e_utils_get_theme_color (widget, "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &selected_bg);

	calitem->colors[E_CALENDAR_ITEM_COLOR_TODAY_BOX] = selected_bg;
	calitem->colors[E_CALENDAR_ITEM_COLOR_SELECTION_FG] = base_bg;
	calitem->colors[E_CALENDAR_ITEM_COLOR_SELECTION_BG_FOCUSED] = unfocused_selected_bg;
	calitem->colors[E_CALENDAR_ITEM_COLOR_SELECTION_BG] = fg;
	calitem->colors[E_CALENDAR_ITEM_COLOR_PREV_OR_NEXT_MONTH_FG] = fg;

	e_calendar_item_recalc_sizes (calitem);
}

void
e_calendar_item_set_selection (ECalendarItem *calitem,
                               const GDate *start_date,
                               const GDate *end_date)
{
	GDate current_start_date, current_end_date;

	/* If the user is in the middle of a selection, we must abort it. */
	if (calitem->selecting) {
		gnome_canvas_item_ungrab (
			GNOME_CANVAS_ITEM (calitem),
			GDK_CURRENT_TIME);
		calitem->selecting = FALSE;
	}

	if (e_calendar_item_get_selection (calitem, &current_start_date, &current_end_date)) {
		/* No change, no need to recalculate anything */
		if (start_date && end_date && g_date_valid (start_date) && g_date_valid (end_date) &&
		    g_date_compare (start_date, &current_start_date) == 0 &&
		    g_date_compare (end_date, &current_end_date) == 0)
			return;
	}

	e_calendar_item_set_selection_if_emission (calitem,
						   start_date, end_date,
						   TRUE);
}

/* This tries to ensure that the given time range is visible. If the range
 * given is longer than we can show, only the start of it will be visible.
 * Note that this will not update the selection. That should be done somewhere
 * else. It returns TRUE if the visible range has been changed. */
static gboolean
e_calendar_item_ensure_days_visible (ECalendarItem *calitem,
                                     gint start_year,
                                     gint start_month,
                                     gint start_day,
                                     gint end_year,
                                     gint end_month,
                                     gint end_day,
                                     gboolean emission)
{
	gint current_end_year, current_end_month;
	gint months_shown;
	gint first_day_offset, days_in_month, days_in_prev_month;
	gboolean need_update = FALSE;

	months_shown = calitem->rows * calitem->cols;

	/* Calculate the range of months currently displayed. */
	current_end_year = calitem->year;
	current_end_month = calitem->month + months_shown - 1;
	e_calendar_item_normalize_date (
		calitem, &current_end_year,
		&current_end_month);

	/* Try to ensure that the end month is shown. */
	if ((end_year == current_end_year + 1 &&
		current_end_month == 11 && end_month == 0) ||
	    (end_year == current_end_year && end_month == current_end_month + 1)) {
		/* See if the end of the selection will fit in the
		 * leftover days of the month after the last one shown. */
		calitem->month += (months_shown - 1);
		e_calendar_item_normalize_date (
			calitem, &calitem->year,
			&calitem->month);

		e_calendar_item_get_month_info (
			calitem, 0, 0,
			&first_day_offset,
			&days_in_month,
			&days_in_prev_month);

		if (end_day >= E_CALENDAR_ROWS_PER_MONTH * E_CALENDAR_COLS_PER_MONTH -
		    first_day_offset - days_in_month) {
			need_update = TRUE;

			calitem->year = end_year;
			calitem->month = end_month - months_shown + 1;
		} else {
			calitem->month -= (months_shown - 1);
		}

		e_calendar_item_normalize_date (
			calitem, &calitem->year,
			&calitem->month);
	}
	else if (end_year > current_end_year ||
		 (end_year == current_end_year && end_month > current_end_month)) {
		/* The selection will definitely not fit in the leftover days
		 * of the month after the last one shown. */
		need_update = TRUE;

		calitem->year = end_year;
		calitem->month = end_month - months_shown + 1;

		e_calendar_item_normalize_date (
			calitem, &calitem->year,
			&calitem->month);
	}

	/* Now try to ensure that the start month is shown. We do this after
	 * the end month so that the start month will always be shown. */
	if (start_year < calitem->year
	    || (start_year == calitem->year
		&& start_month < calitem->month)) {
		need_update = TRUE;

		/* First we see if the start of the selection will fit in the
		 * leftover days of the month before the first one shown. */
		calitem->year = start_year;
		calitem->month = start_month + 1;
		e_calendar_item_normalize_date (
			calitem, &calitem->year,
			&calitem->month);

		e_calendar_item_get_month_info (
			calitem, 0, 0,
			&first_day_offset,
			&days_in_month,
			&days_in_prev_month);

		if (start_day <= days_in_prev_month - first_day_offset) {
			calitem->year = start_year;
			calitem->month = start_month;
		}
	}

	if (need_update && emission)
		e_calendar_item_date_range_changed (calitem);

	return need_update;
}

static void
e_calendar_item_show_popup_menu (ECalendarItem *calitem,
                                 GdkEvent *button_event,
                                 gint month_offset)
{
	GtkWidget *menu, *submenu, *menuitem, *label;
	GtkWidget *canvas_widget;
	gint year, month;
	const gchar *name;
	gchar buffer[64];

	menu = gtk_menu_new ();

	for (year = calitem->year - 2; year <= calitem->year + 2; year++) {
		g_snprintf (buffer, 64, "%i", year);
		menuitem = gtk_menu_item_new_with_label (buffer);
		gtk_widget_show (menuitem);
		gtk_container_add (GTK_CONTAINER (menu), menuitem);

		submenu = gtk_menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

		g_object_set_data (
			G_OBJECT (submenu), "year",
			GINT_TO_POINTER (year));
		g_object_set_data (
			G_OBJECT (submenu), "month_offset",
			GINT_TO_POINTER (month_offset));

		for (month = 0; month < 12; month++) {
			name = e_get_month_name (month + 1, FALSE);

			menuitem = gtk_menu_item_new ();
			gtk_widget_show (menuitem);
			gtk_container_add (GTK_CONTAINER (submenu), menuitem);

			label = gtk_label_new (name);
			gtk_label_set_xalign (GTK_LABEL (label), 0);
			gtk_widget_show (label);
			gtk_container_add (GTK_CONTAINER (menuitem), label);

			g_object_set_data (
				G_OBJECT (menuitem), "month",
				GINT_TO_POINTER (month));

			g_signal_connect (
				menuitem, "activate",
				G_CALLBACK (e_calendar_item_on_menu_item_activate),
				calitem);
		}
	}

	g_signal_connect (
		menu, "deactivate",
		G_CALLBACK (gtk_menu_detach), NULL);

	canvas_widget = GTK_WIDGET (calitem->canvas_item.canvas);
	gtk_menu_attach_to_widget (GTK_MENU (menu), canvas_widget, NULL);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), button_event);
}

static void
e_calendar_item_on_menu_item_activate (GtkWidget *menuitem,
                                       ECalendarItem *calitem)
{
	GtkWidget *parent;
	gint year, month_offset, month;
	gpointer data;

	parent = gtk_widget_get_parent (menuitem);
	data = g_object_get_data (G_OBJECT (parent), "year");
	year = GPOINTER_TO_INT (data);

	parent = gtk_widget_get_parent (menuitem);
	data = g_object_get_data (G_OBJECT (parent), "month_offset");
	month_offset = GPOINTER_TO_INT (data);

	data = g_object_get_data (G_OBJECT (menuitem), "month");
	month = GPOINTER_TO_INT (data);

	month -= month_offset;
	e_calendar_item_normalize_date (calitem, &year, &month);
	e_calendar_item_set_first_month_with_emit (calitem, year, month, TRUE);
}

/* Sets the function to call to get the colors to use for a particular day. */
void
e_calendar_item_set_style_callback (ECalendarItem *calitem,
                                    ECalendarItemStyleCallback cb,
                                    gpointer data,
                                    GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	if (calitem->style_callback_data && calitem->style_callback_destroy)
		(*calitem->style_callback_destroy) (calitem->style_callback_data);

	calitem->style_callback = cb;
	calitem->style_callback_data = data;
	calitem->style_callback_destroy = destroy;
}

static void
e_calendar_item_date_range_changed (ECalendarItem *calitem)
{
	g_free (calitem->styles);
	calitem->styles = NULL;
	calitem->date_range_changed = TRUE;
	e_calendar_item_queue_signal_emission (calitem);
}

static void
e_calendar_item_queue_signal_emission (ECalendarItem *calitem)
{
	if (calitem->signal_emission_idle_id == 0) {
		calitem->signal_emission_idle_id = g_idle_add_full (
			G_PRIORITY_HIGH, (GSourceFunc)
			e_calendar_item_signal_emission_idle_cb,
			calitem, NULL);
	}
}

static gboolean
e_calendar_item_signal_emission_idle_cb (gpointer data)
{
	ECalendarItem *calitem;

	g_return_val_if_fail (E_IS_CALENDAR_ITEM (data), FALSE);

	calitem = E_CALENDAR_ITEM (data);

	calitem->signal_emission_idle_id = 0;

	/* We ref the calitem & check in case it gets destroyed, since we
	 * were getting a free memory write here. */
	g_object_ref ((calitem));

	if (calitem->date_range_changed) {
		calitem->date_range_changed = FALSE;
		g_signal_emit (calitem, e_calendar_item_signals[DATE_RANGE_CHANGED], 0);
	}

	if (calitem->selection_changed) {
		calitem->selection_changed = FALSE;
		g_signal_emit (calitem, e_calendar_item_signals[SELECTION_CHANGED], 0);
	}

	g_object_unref ((calitem));

	return FALSE;
}

/* Sets a callback to use to get the current time. This is useful if the
 * application needs to use its own timezone data rather than rely on the
 * Unix timezone. */
void
e_calendar_item_set_get_time_callback (ECalendarItem *calitem,
                                       ECalendarItemGetTimeCallback cb,
                                       gpointer data,
                                       GDestroyNotify destroy)
{
	g_return_if_fail (E_IS_CALENDAR_ITEM (calitem));

	if (calitem->time_callback_data && calitem->time_callback_destroy)
		(*calitem->time_callback_destroy) (calitem->time_callback_data);

	calitem->time_callback = cb;
	calitem->time_callback_data = data;
	calitem->time_callback_destroy = destroy;
}

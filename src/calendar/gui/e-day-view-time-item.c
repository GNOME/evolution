/*
 * EDayViewTimeItem - canvas item which displays the times down the left of
 * the EDayView.
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-day-view-time-item.h"
#include "calendar-config.h"

/* The spacing between items in the time column. GRID_X_PAD is the space down
 * either side of the column, i.e. outside the main horizontal grid lines.
 * HOUR_L_PAD & HOUR_R_PAD are the spaces on the left & right side of the
 * big hour number (this is inside the horizontal grid lines).
 * MIN_X_PAD is the spacing either side of the minute number. The smaller
 * horizontal grid lines match with this.
 * 60_MIN_X_PAD is the space either side of the HH:MM display used when
 * we are displaying 60 mins per row (inside the main grid lines).
 * LARGE_HOUR_Y_PAD is the offset of the large hour string from the top of the
 * row.
 * SMALL_FONT_Y_PAD is the offset of the small time/minute string from the top
 * of the row. */
#define E_DVTMI_TIME_GRID_X_PAD		4
#define E_DVTMI_HOUR_L_PAD		4
#define E_DVTMI_HOUR_R_PAD		2
#define E_DVTMI_MIN_X_PAD		2
#define E_DVTMI_60_MIN_X_PAD		4
#define E_DVTMI_LARGE_HOUR_Y_PAD	1
#define E_DVTMI_SMALL_FONT_Y_PAD	1

struct _EDayViewTimeItemPrivate {
	/* The parent EDayView widget. */
	EDayView *day_view;

	/* The width of the time column. */
	gint column_width;

	/* TRUE if we are currently dragging the selection times. */
	gboolean dragging_selection;

	/* The second timezone if shown, or else NULL. */
	ICalTimezone *second_zone;
};

static void	e_day_view_time_item_update	(GnomeCanvasItem *item,
						 const cairo_matrix_t *i2c,
						 gint flags);
static void	e_day_view_time_item_draw	(GnomeCanvasItem *item,
						 cairo_t *cr,
						 gint x,
						 gint y,
						 gint width,
						 gint height);
static GnomeCanvasItem *
		e_day_view_time_item_point	(GnomeCanvasItem *item,
						 gdouble x,
						 gdouble y,
						 gint cx,
						 gint cy);
static gint	e_day_view_time_item_event	(GnomeCanvasItem *item,
						 GdkEvent *event);
static void	e_day_view_time_item_increment_time
						(gint *hour,
						 gint *minute,
						 gint time_divisions);
static void	e_day_view_time_item_show_popup_menu
						(EDayViewTimeItem *time_item,
						 GdkEvent *event);
static void	e_day_view_time_item_on_set_divisions
						(GtkWidget *item,
						 EDayViewTimeItem *time_item);
static void	e_day_view_time_item_on_button_press
						(EDayViewTimeItem *time_item,
						 GdkEvent *event);
static void	e_day_view_time_item_on_button_release
						(EDayViewTimeItem *time_item,
						 GdkEvent *event);
static void	e_day_view_time_item_on_motion_notify
						(EDayViewTimeItem *time_item,
						 GdkEvent *event);
static gint	e_day_view_time_item_convert_position_to_row
						(EDayViewTimeItem *time_item,
						 gint y);

static void	edvti_second_zone_changed_cb	(GSettings *settings,
						 const gchar *key,
						 gpointer user_data);

enum {
	PROP_0,
	PROP_DAY_VIEW
};

G_DEFINE_TYPE_WITH_PRIVATE (EDayViewTimeItem, e_day_view_time_item, GNOME_TYPE_CANVAS_ITEM)

static void
day_view_time_item_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			e_day_view_time_item_set_day_view (
				E_DAY_VIEW_TIME_ITEM (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_time_item_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			g_value_set_object (
				value, e_day_view_time_item_get_day_view (
				E_DAY_VIEW_TIME_ITEM (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_time_item_dispose (GObject *object)
{
	EDayViewTimeItem *self = E_DAY_VIEW_TIME_ITEM (object);

	g_clear_object (&self->priv->day_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_day_view_time_item_parent_class)->dispose (object);
}

static void
day_view_time_item_finalize (GObject *object)
{
	EDayViewTimeItem *time_item;

	time_item = E_DAY_VIEW_TIME_ITEM (object);

	calendar_config_remove_notification (
		(CalendarConfigChangedFunc)
		edvti_second_zone_changed_cb, time_item);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_day_view_time_item_parent_class)->finalize (object);
}

static void
e_day_view_time_item_class_init (EDayViewTimeItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = day_view_time_item_set_property;
	object_class->get_property = day_view_time_item_get_property;
	object_class->dispose = day_view_time_item_dispose;
	object_class->finalize = day_view_time_item_finalize;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = e_day_view_time_item_update;
	item_class->draw = e_day_view_time_item_draw;
	item_class->point = e_day_view_time_item_point;
	item_class->event = e_day_view_time_item_event;

	g_object_class_install_property (
		object_class,
		PROP_DAY_VIEW,
		g_param_spec_object (
			"day-view",
			"Day View",
			NULL,
			E_TYPE_DAY_VIEW,
			G_PARAM_READWRITE));
}

static void
e_day_view_time_item_init (EDayViewTimeItem *time_item)
{
	gchar *last;

	time_item->priv = e_day_view_time_item_get_instance_private (time_item);

	last = calendar_config_get_day_second_zone ();

	if (last) {
		if (*last)
			time_item->priv->second_zone =
				i_cal_timezone_get_builtin_timezone (last);
		g_free (last);
	}

	calendar_config_add_notification_day_second_zone (
		(CalendarConfigChangedFunc) edvti_second_zone_changed_cb,
		time_item);
}

static void
e_day_view_time_item_update (GnomeCanvasItem *item,
                             const cairo_matrix_t *i2c,
                             gint flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_day_view_time_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_day_view_time_item_parent_class)->update) (item, i2c, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}

/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */
static void
edvti_draw_zone (GnomeCanvasItem *canvas_item,
		 cairo_t *cr,
		 gint x,
		 gint y,
		 gint width,
		 gint height,
		 gint x_offset,
		 ICalTimezone *use_zone)
{
	EDayView *day_view;
	EDayViewTimeItem *time_item;
	ECalendarView *cal_view;
	ECalModel *model;
	const gchar *suffix;
	gchar buffer[64], *midnight_day = NULL, *midnight_month = NULL;
	gint time_divisions;
	gint hour, display_hour, minute, row;
	gint row_y, start_y, large_hour_y_offset, small_font_y_offset;
	gint long_line_x1, long_line_x2, short_line_x1;
	gint large_hour_x2, minute_x2;
	gint hour_width, minute_width, suffix_width;
	gint max_suffix_width, max_minute_or_suffix_width;
	PangoLayout *layout;
	PangoAttrList *tnum;
	PangoAttribute *attr;
	PangoContext *context;
	PangoFontMetrics *large_font_metrics, *small_font_metrics;
	GtkWidget *widget;
	GdkRGBA fg, dark, mb_color;

	time_item = E_DAY_VIEW_TIME_ITEM (canvas_item);
	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	widget = GTK_WIDGET (day_view);
	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	context = gtk_widget_get_pango_context (GTK_WIDGET (day_view));
	small_font_metrics = pango_context_get_metrics (
		context, NULL,
		pango_context_get_language (context));
	large_font_metrics = pango_context_get_metrics (
		context, day_view->large_font_desc,
		pango_context_get_language (context));

	e_utils_get_theme_color (widget, "theme_fg_color,theme_text_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &fg);
	e_utils_get_theme_color (widget, "theme_base_color", E_UTILS_DEFAULT_THEME_BASE_COLOR, &dark);

	tnum = pango_attr_list_new ();
	attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);

	/* The start and end of the long horizontal line between hours. */
	long_line_x1 =
		(use_zone ? 0 : E_DVTMI_TIME_GRID_X_PAD) - x + x_offset;
	long_line_x2 =
		time_item->priv->column_width -
		E_DVTMI_TIME_GRID_X_PAD - x -
		(use_zone ? E_DVTMI_TIME_GRID_X_PAD : 0) + x_offset;

	if (time_divisions == 60) {
		/* The right edge of the complete time string in 60-min
		 * divisions, e.g. "14:00" or "2 pm". */
		minute_x2 = long_line_x2 - E_DVTMI_60_MIN_X_PAD;

		/* These aren't used for 60-minute divisions, but we initialize
		 * them to keep gcc happy. */
		short_line_x1 = 0;
		large_hour_x2 = 0;
	} else {
		max_suffix_width = MAX (
			day_view->am_string_width,
			day_view->pm_string_width);

		max_minute_or_suffix_width = MAX (
			max_suffix_width,
			day_view->max_minute_width);

		/* The start of the short horizontal line between the periods
		 * within each hour. */
		short_line_x1 = long_line_x2 - E_DVTMI_MIN_X_PAD * 2
			- max_minute_or_suffix_width;

		/* The right edge of the large hour string. */
		large_hour_x2 = short_line_x1 - E_DVTMI_HOUR_R_PAD;

		/* The right edge of the minute part of the time. */
		minute_x2 = long_line_x2 - E_DVTMI_MIN_X_PAD;
	}

	/* Start with the first hour & minute shown in the EDayView. */
	hour = day_view->first_hour_shown;
	minute = day_view->first_minute_shown;

	if (use_zone) {
		/* shift time with a difference between
		 * local time and the other timezone */
		ICalTimezone *cal_zone;
		ICalTime *tt;
		gint is_daylight = 0; /* Its value is ignored, but libical-glib 3.0.5 API requires it */
		gint diff;
		struct tm mn;

		cal_zone = e_calendar_view_get_timezone (
			E_CALENDAR_VIEW (day_view));
		tt = i_cal_time_new_from_timet_with_zone (
			day_view->day_starts[0], 0, cal_zone);

		/* diff is number of minutes */
		diff = (i_cal_timezone_get_utc_offset (use_zone, tt, &is_daylight) -
			i_cal_timezone_get_utc_offset (cal_zone, tt, &is_daylight)) / 60;

		g_clear_object (&tt);

		tt = i_cal_time_new_from_timet_with_zone (day_view->day_starts[0], 0, cal_zone);
		i_cal_time_set_is_date (tt, FALSE);
		i_cal_time_set_timezone (tt, cal_zone);
		i_cal_time_convert_to_zone_inplace (tt, use_zone);

		if (diff != 0) {
			/* shows the next midnight */
			i_cal_time_adjust (tt, 1, 0, 0, 0);
		}

		mn = e_cal_util_icaltime_to_tm (tt);

		g_clear_object (&tt);

		/* up to two characters/numbers */
		e_utf8_strftime (buffer, sizeof (buffer), "%d", &mn);
		midnight_day = g_strdup (buffer);
		/* up to three characters, abbreviated month name */
		e_utf8_strftime (buffer, sizeof (buffer), "%b", &mn);
		midnight_month = g_strdup (buffer);

		minute += (diff % 60);
		hour += (diff / 60) + (minute / 60);

		minute = minute % 60;
		if (minute < 0) {
			hour--;
			minute += 60;
		}

		hour = (hour + 48) % 24;
	}

	/* The offset of the large hour string from the top of the row. */
	large_hour_y_offset = E_DVTMI_LARGE_HOUR_Y_PAD;

	/* The offset of the small time/minute string from top of row. */
	small_font_y_offset = E_DVTMI_SMALL_FONT_Y_PAD;

	/* Calculate the minimum y position of the first row we need to draw.
	 * This is normally one row height above the 0 position, but if we
	 * are using the large font we may have to go back a bit further. */
	start_y = 0 - MAX (day_view->row_height,
			   (pango_font_metrics_get_ascent (large_font_metrics) +
			    pango_font_metrics_get_descent (large_font_metrics)) / PANGO_SCALE +
			   E_DVTMI_LARGE_HOUR_Y_PAD);

	/* Draw the Marcus Bains Line first, so it appears under other elements. */
	if (e_day_view_marcus_bains_get_show_line (day_view)) {
		ICalTime *time_now;
		const gchar *marcus_bains_time_bar_color;
		gint marcus_bains_y;

		cairo_save (cr);
		gdk_cairo_set_source_rgba (
			cr, &day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE]);

		marcus_bains_time_bar_color =
			e_day_view_marcus_bains_get_time_bar_color (day_view);
		if (marcus_bains_time_bar_color == NULL)
			marcus_bains_time_bar_color = "";

		if (gdk_rgba_parse (&mb_color, marcus_bains_time_bar_color)) {
			gdk_cairo_set_source_rgba (cr, &mb_color);
		} else {
			mb_color = day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE];
		}

		time_now = i_cal_time_new_current_with_zone (
			e_calendar_view_get_timezone (
			E_CALENDAR_VIEW (day_view)));
		marcus_bains_y =
			(i_cal_time_get_hour (time_now) * 60 + i_cal_time_get_minute (time_now)) *
			day_view->row_height / time_divisions - y;
		cairo_set_line_width (cr, 1.5);
		cairo_move_to (
			cr, long_line_x1 -
			(use_zone ? E_DVTMI_TIME_GRID_X_PAD : 0),
			marcus_bains_y);
		cairo_line_to (cr, long_line_x2, marcus_bains_y);
		cairo_stroke (cr);
		cairo_restore (cr);

		g_clear_object (&time_now);
	} else {
		const gchar *marcus_bains_time_bar_color;

		marcus_bains_time_bar_color =
			e_day_view_marcus_bains_get_time_bar_color (day_view);
		if (marcus_bains_time_bar_color == NULL)
			marcus_bains_time_bar_color = "";

		mb_color = day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE];

		if (gdk_rgba_parse (&mb_color, marcus_bains_time_bar_color))
			gdk_cairo_set_source_rgba (cr, &mb_color);
	}

	/* Step through each row, drawing the times and the horizontal lines
	 * between them. */
	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		gboolean show_midnight_date;

		show_midnight_date =
			use_zone && hour == 0 &&
			(minute == 0 || time_divisions == 60) &&
			midnight_day && midnight_month;

		/* If the row is above the first row we want to draw just
		 * increment the time and skip to the next row. */
		if (row_y < start_y) {
			e_day_view_time_item_increment_time (
				&hour, &minute, time_divisions);
			continue;
		}

		/* Calculate the actual hour number to display. For 12-hour
		 * format we convert 0-23 to 12-11am / 12 - 11pm. */
		e_day_view_convert_time_to_display (
			day_view, hour,
			&display_hour,
			&suffix, &suffix_width);

		if (time_divisions == 60) {
			/* 60 minute intervals - draw a long horizontal line
			 * between hours and display as one long string,
			 * e.g. "14:00" or "2 pm". */
			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, &dark);
			cairo_save (cr);
			cairo_set_line_width (cr, 0.7);
			cairo_move_to (cr, long_line_x1, row_y);
			cairo_line_to (cr, long_line_x2, row_y);
			cairo_stroke (cr);
			cairo_restore (cr);

			if (show_midnight_date) {
				g_snprintf (buffer, sizeof (buffer), "%s %s", midnight_day, midnight_month);
			} else if (e_cal_model_get_use_24_hour_format (model)) {
				g_snprintf (
					buffer, sizeof (buffer), "%i:%02i",
					display_hour, minute);
			} else {
				g_snprintf (
					buffer, sizeof (buffer), "%i %s",
					display_hour, suffix);
			}

			cairo_save (cr);
			if (show_midnight_date)
				gdk_cairo_set_source_rgba (cr, &mb_color);
			else
				gdk_cairo_set_source_rgba (cr, &fg);
			layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), NULL);
			pango_layout_set_attributes (layout, tnum);
			pango_layout_set_text (layout, buffer, -1);
			pango_layout_get_pixel_size (layout, &minute_width, NULL);
			cairo_translate (
				cr, minute_x2 - minute_width,
				row_y + small_font_y_offset);
			pango_cairo_update_layout (cr, layout);
			pango_cairo_show_layout (cr, layout);
			cairo_restore (cr);

			g_object_unref (layout);

			cairo_restore (cr);
		} else {
			/* 5/10/15/30 minute intervals. */

			if (minute == 0) {
				/* On the hour - draw a long horizontal line
				 * before the hour and display the hour in the
				 * large font. */

				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &dark);
				if (show_midnight_date)
					g_snprintf (buffer, sizeof (buffer), "%s", midnight_day);
				else
					g_snprintf (
						buffer, sizeof (buffer), "%i",
						display_hour);

				cairo_set_line_width (cr, 0.7);
				cairo_move_to (cr, long_line_x1, row_y);
				cairo_line_to (cr, long_line_x2, row_y);
				cairo_stroke (cr);
				cairo_restore (cr);

				cairo_save (cr);
				if (show_midnight_date)
					gdk_cairo_set_source_rgba (cr, &mb_color);
				else
					gdk_cairo_set_source_rgba (cr, &fg);
				layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), NULL);
				pango_layout_set_attributes (layout, tnum);
				pango_layout_set_text (layout, buffer, -1);
				pango_layout_set_font_description (
					layout, day_view->large_font_desc);
				pango_layout_get_pixel_size (
					layout, &hour_width, NULL);
				cairo_translate (
					cr, large_hour_x2 - hour_width,
					row_y + large_hour_y_offset);
				pango_cairo_update_layout (cr, layout);
				pango_cairo_show_layout (cr, layout);
				cairo_restore (cr);

				g_object_unref (layout);
			} else {
				/* Within the hour - draw a short line before
				 * the time. */
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &dark);
				cairo_set_line_width (cr, 0.7);
				cairo_move_to (cr, short_line_x1, row_y);
				cairo_line_to (cr, long_line_x2, row_y);
				cairo_stroke (cr);
				cairo_restore (cr);
			}

			/* Normally we display the minute in each
			 * interval, but when using 30-minute intervals
			 * we don't display the '30'. */
			if (time_divisions != 30 || minute != 30) {
				/* In 12-hour format we display 'am' or 'pm'
				 * instead of '00'. */
				if (show_midnight_date)
					g_snprintf (buffer, sizeof (buffer), "%s", midnight_month);
				else if (minute == 0
				    && !e_cal_model_get_use_24_hour_format (model)) {
					g_snprintf (buffer, sizeof (buffer), "%s", suffix);
				} else {
					g_snprintf (
						buffer, sizeof (buffer),
						"%02i", minute);
				}

				cairo_save (cr);
				if (show_midnight_date)
					gdk_cairo_set_source_rgba (cr, &mb_color);
				else
					gdk_cairo_set_source_rgba (cr, &fg);
				layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), NULL);
				pango_layout_set_attributes (layout, tnum);
				pango_layout_set_text (layout, buffer, -1);
				pango_layout_set_font_description (
					layout, day_view->small_font_desc);
				pango_layout_get_pixel_size (
					layout, &minute_width, NULL);
				cairo_translate (
					cr, minute_x2 - minute_width,
					row_y + small_font_y_offset);
				pango_cairo_update_layout (cr, layout);
				pango_cairo_show_layout (cr, layout);
				cairo_restore (cr);

				g_object_unref (layout);
			}
		}

		e_day_view_time_item_increment_time (
			&hour, &minute,
			time_divisions);
	}

	pango_attr_list_unref (tnum);
	pango_font_metrics_unref (large_font_metrics);
	pango_font_metrics_unref (small_font_metrics);

	g_free (midnight_day);
	g_free (midnight_month);
}

static void
e_day_view_time_item_draw (GnomeCanvasItem *canvas_item,
                           cairo_t *cr,
                           gint x,
                           gint y,
                           gint width,
                           gint height)
{
	EDayViewTimeItem *time_item;

	time_item = E_DAY_VIEW_TIME_ITEM (canvas_item);
	g_return_if_fail (time_item != NULL);

	edvti_draw_zone (canvas_item, cr, x, y, width, height, 0, NULL);

	if (time_item->priv->second_zone)
		edvti_draw_zone (
			canvas_item, cr, x, y, width, height,
			time_item->priv->column_width,
			time_item->priv->second_zone);
}

/* Increment the time by the 5/10/15/30/60 minute interval.
 * Note that time_divisions is never > 60, so we never have to
 * worry about adding more than 60 minutes. */
static void
e_day_view_time_item_increment_time (gint *hour,
                                     gint *minute,
                                     gint time_divisions)
{
	*minute += time_divisions;
	if (*minute >= 60) {
		*minute -= 60;
		/* Currently we never wrap around to the next day, but
		 * we may do if we display extra timezones. */
		*hour = (*hour + 1) % 24;
	}
}

static GnomeCanvasItem *
e_day_view_time_item_point (GnomeCanvasItem *item,
                            gdouble x,
                            gdouble y,
                            gint cx,
                            gint cy)
{
	return item;
}

static gint
e_day_view_time_item_event (GnomeCanvasItem *item,
                            GdkEvent *event)
{
	EDayViewTimeItem *time_item;

	time_item = E_DAY_VIEW_TIME_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			e_day_view_time_item_on_button_press (time_item, event);
		} else if (event->button.button == 3) {
			e_day_view_time_item_show_popup_menu (time_item, event);
			return TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1)
			e_day_view_time_item_on_button_release (
				time_item, event);
		break;

	case GDK_MOTION_NOTIFY:
		e_day_view_time_item_on_motion_notify (time_item, event);
		break;

	default:
		break;
	}

	return FALSE;
}

static void
edvti_second_zone_changed_cb (GSettings *settings,
                              const gchar *key,
                              gpointer user_data)
{
	EDayViewTimeItem *time_item = user_data;
	EDayView *day_view;
	ICalTimezone *second_zone;
	gchar *location;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (E_IS_DAY_VIEW_TIME_ITEM (time_item));

	location = calendar_config_get_day_second_zone ();
	second_zone = location ? i_cal_timezone_get_builtin_timezone (location) : NULL;
	g_free (location);

	if (second_zone == time_item->priv->second_zone)
		return;

	time_item->priv->second_zone = second_zone;

	day_view = e_day_view_time_item_get_day_view (time_item);
	gtk_widget_set_size_request (
		day_view->time_canvas,
		e_day_view_time_item_get_column_width (time_item), -1);
	gtk_widget_queue_draw (day_view->time_canvas);

	e_day_view_update_timezone_name_labels (day_view);
}

static void
edvti_on_select_zone (GtkWidget *item,
                      EDayViewTimeItem *time_item)
{
	calendar_config_select_day_second_zone (gtk_widget_get_toplevel (item));
}

static void
edvti_on_set_zone (GtkWidget *item,
                   EDayViewTimeItem *time_item)
{
	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;

	calendar_config_set_day_second_zone (
		g_object_get_data (G_OBJECT (item), "timezone"));
}

static void
e_day_view_time_item_show_popup_menu (EDayViewTimeItem *time_item,
                                      GdkEvent *event)
{
	static gint divisions[] = { 60, 30, 15, 10, 5 };
	EDayView *day_view;
	ECalendarView *cal_view;
	GtkWidget *menu, *item, *submenu;
	gchar buffer[256];
	GSList *group = NULL, *recent_zones, *s;
	gint current_divisions, i;
	ICalTimezone *zone;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	cal_view = E_CALENDAR_VIEW (day_view);
	current_divisions = e_calendar_view_get_time_divisions (cal_view);

	menu = gtk_menu_new ();

	/* Make sure the menu is destroyed when it disappears. */
	g_signal_connect (
		menu, "selection-done",
		G_CALLBACK (gtk_widget_destroy), NULL);

	for (i = 0; i < G_N_ELEMENTS (divisions); i++) {
		g_snprintf (
			buffer, sizeof (buffer),
			/* Translators: %02i is the number of minutes;
			 * this is a context menu entry to change the
			 * length of the time division in the calendar
			 * day view, e.g. a day is displayed in
			 * 24 "60 minute divisions" or
			 * 48 "30 minute divisions". */
			_("%02i minute divisions"), divisions[i]);
		item = gtk_radio_menu_item_new_with_label (group, buffer);
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		if (current_divisions == divisions[i])
			gtk_check_menu_item_set_active (
				GTK_CHECK_MENU_ITEM (item), TRUE);

		g_object_set_data (
			G_OBJECT (item), "divisions",
			GINT_TO_POINTER (divisions[i]));

		g_signal_connect (
			item, "toggled",
			G_CALLBACK (e_day_view_time_item_on_set_divisions),
			time_item);
	}

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	submenu = gtk_menu_new ();
	item = gtk_menu_item_new_with_label (_("Show the second time zone"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
	if (zone)
		item = gtk_menu_item_new_with_label (i_cal_timezone_get_display_name (zone));
	else
		item = gtk_menu_item_new_with_label ("---");
	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

	group = NULL;
	item = gtk_radio_menu_item_new_with_label (group, C_("cal-second-zone", "None"));
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
	if (!time_item->priv->second_zone)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
	g_signal_connect (
		item, "toggled",
		G_CALLBACK (edvti_on_set_zone), time_item);

	recent_zones = calendar_config_get_day_second_zones ();
	for (s = recent_zones; s != NULL; s = s->next) {
		zone = i_cal_timezone_get_builtin_timezone (s->data);
		if (!zone)
			continue;

		item = gtk_radio_menu_item_new_with_label (
			group, i_cal_timezone_get_display_name (zone));
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		/* both comes from builtin, thus no problem to compare pointers */
		if (zone == time_item->priv->second_zone)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
		g_object_set_data_full (
			G_OBJECT (item), "timezone",
			g_strdup (s->data), g_free);
		g_signal_connect (
			item, "toggled",
			G_CALLBACK (edvti_on_set_zone), time_item);
	}
	calendar_config_free_day_second_zones (recent_zones);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

	item = gtk_menu_item_new_with_label (_("Select…"));
	g_signal_connect (
		item, "activate",
		G_CALLBACK (edvti_on_select_zone), time_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

	gtk_widget_show_all (submenu);

	gtk_menu_attach_to_widget (GTK_MENU (menu),
				   GTK_WIDGET (day_view),
				   NULL);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
}

static void
e_day_view_time_item_on_set_divisions (GtkWidget *item,
                                       EDayViewTimeItem *time_item)
{
	EDayView *day_view;
	ECalendarView *cal_view;
	gint divisions;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;

	divisions = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "divisions"));

	cal_view = E_CALENDAR_VIEW (day_view);
	e_calendar_view_set_time_divisions (cal_view, divisions);
}

static void
e_day_view_time_item_on_button_press (EDayViewTimeItem *time_item,
                                      GdkEvent *event)
{
	GdkWindow *window;
	EDayView *day_view;
	GnomeCanvas *canvas;
	GdkGrabStatus grab_status;
	GdkDevice *event_device;
	guint32 event_time;
	gint row;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (time_item)->canvas;

	row = e_day_view_time_item_convert_position_to_row (
		time_item,
		event->button.y);

	if (row == -1)
		return;

	if (!gtk_widget_has_focus (GTK_WIDGET (day_view)))
		gtk_widget_grab_focus (GTK_WIDGET (day_view));

	window = gtk_layout_get_bin_window (GTK_LAYOUT (canvas));

	event_device = gdk_event_get_device (event);
	event_time = gdk_event_get_time (event);

	grab_status = gdk_device_grab (
		event_device,
		window,
		GDK_OWNERSHIP_NONE,
		FALSE,
		GDK_POINTER_MOTION_MASK |
		GDK_BUTTON_RELEASE_MASK,
		NULL,
		event_time);

	if (grab_status == GDK_GRAB_SUCCESS) {
		e_day_view_start_selection (day_view, -1, row);
		time_item->priv->dragging_selection = TRUE;
	}
}

static void
e_day_view_time_item_on_button_release (EDayViewTimeItem *time_item,
                                        GdkEvent *event)
{
	EDayView *day_view;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	if (time_item->priv->dragging_selection) {
		GdkDevice *event_device;
		guint32 event_time;

		event_device = gdk_event_get_device (event);
		event_time = gdk_event_get_time (event);
		gdk_device_ungrab (event_device, event_time);

		e_day_view_finish_selection (day_view);
		e_day_view_stop_auto_scroll (day_view);
	}

	time_item->priv->dragging_selection = FALSE;
}

static void
e_day_view_time_item_on_motion_notify (EDayViewTimeItem *time_item,
                                       GdkEvent *event)
{
	EDayView *day_view;
	GnomeCanvas *canvas;
	gdouble window_y;
	gint y, row;

	if (!time_item->priv->dragging_selection)
		return;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (time_item)->canvas;

	y = event->motion.y;
	row = e_day_view_time_item_convert_position_to_row (time_item, y);

	if (row != -1) {
		gnome_canvas_world_to_window (
			canvas, 0, event->motion.y,
			NULL, &window_y);
		e_day_view_update_selection (day_view, -1, row);
		e_day_view_check_auto_scroll (day_view, -1, (gint) window_y);
	}
}

/* Returns the row corresponding to the y position, or -1. */
static gint
e_day_view_time_item_convert_position_to_row (EDayViewTimeItem *time_item,
                                              gint y)
{
	EDayView *day_view;
	gint row;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_val_if_fail (day_view != NULL, -1);

	if (y < 0)
		return -1;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return -1;

	return row;
}

EDayView *
e_day_view_time_item_get_day_view (EDayViewTimeItem *time_item)
{
	g_return_val_if_fail (E_IS_DAY_VIEW_TIME_ITEM (time_item), NULL);

	return time_item->priv->day_view;
}

void
e_day_view_time_item_set_day_view (EDayViewTimeItem *time_item,
                                   EDayView *day_view)
{
	g_return_if_fail (E_IS_DAY_VIEW_TIME_ITEM (time_item));
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (time_item->priv->day_view == day_view)
		return;

	if (time_item->priv->day_view != NULL)
		g_object_unref (time_item->priv->day_view);

	time_item->priv->day_view = g_object_ref (day_view);

	g_object_notify (G_OBJECT (time_item), "day-view");
}

/* Returns the minimum width needed for the column, by adding up all the
 * maximum widths of the strings. The string widths are all calculated in
 * the style_updated handlers of EDayView and EDayViewTimeCanvas. */
gint
e_day_view_time_item_get_column_width (EDayViewTimeItem *time_item)
{
	EDayView *day_view;
	PangoAttrList *tnum;
	PangoAttribute *attr;
	gint digit, large_digit_width, max_large_digit_width = 0;
	gint max_suffix_width, max_minute_or_suffix_width;
	gint column_width_default, column_width_60_min_rows;

	day_view = e_day_view_time_item_get_day_view (time_item);
	g_return_val_if_fail (day_view != NULL, 0);

	tnum = pango_attr_list_new ();
	attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);

	/* Find the maximum width a digit can have. FIXME: We could use pango's
	 * approximation function, but I worry it won't be precise enough. Also
	 * it needs a language tag that I don't know where to get. */
	for (digit = '0'; digit <= '9'; digit++) {
		PangoLayout *layout;
		gchar digit_str[2];

		digit_str[0] = digit;
		digit_str[1] = '\0';

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), digit_str);
		pango_layout_set_attributes (layout, tnum);
		pango_layout_set_font_description (layout, day_view->large_font_desc);
		pango_layout_get_pixel_size (layout, &large_digit_width, NULL);

		g_object_unref (layout);

		max_large_digit_width = MAX (
			max_large_digit_width,
			large_digit_width);
	}

	pango_attr_list_unref (tnum);

	/* Calculate the width of each time column, using the maximum of the
	 * default format with large hour numbers, and the 60-min divisions
	 * format which uses small text. */
	max_suffix_width = MAX (
		day_view->am_string_width,
		day_view->pm_string_width);

	max_minute_or_suffix_width = MAX (
		max_suffix_width,
		day_view->max_minute_width);

	column_width_default = max_large_digit_width * 2
		+ max_minute_or_suffix_width
		+ E_DVTMI_MIN_X_PAD * 2
		+ E_DVTMI_HOUR_L_PAD
		+ E_DVTMI_HOUR_R_PAD
		+ E_DVTMI_TIME_GRID_X_PAD * 2;

	column_width_60_min_rows = day_view->max_small_hour_width
		+ day_view->colon_width
		+ max_minute_or_suffix_width
		+ E_DVTMI_60_MIN_X_PAD * 2
		+ E_DVTMI_TIME_GRID_X_PAD * 2;

	time_item->priv->column_width =
		MAX (column_width_default, column_width_60_min_rows);

	if (time_item->priv->second_zone)
		return (2 * time_item->priv->column_width) -
			E_DVTMI_TIME_GRID_X_PAD;

	return time_item->priv->column_width;
}

ICalTimezone *
e_day_view_time_item_get_second_zone (EDayViewTimeItem *time_item)
{
	g_return_val_if_fail (E_IS_DAY_VIEW_TIME_ITEM (time_item), NULL);

	return time_item->priv->second_zone;
}

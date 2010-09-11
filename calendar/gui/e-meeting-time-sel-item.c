/*
 * EMeetingTimeSelectorItem - A GnomeCanvasItem which is used for both the main
 * display canvas and the top display (with the dates, times & All Attendees).
 * I didn't make these separate GnomeCanvasItems since they share a lot of
 * code.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *		Damon Chaplin <damon@gtk.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "calendar-config.h"
#include "e-meeting-time-sel-item.h"
#include "e-meeting-time-sel.h"

/* Initially the grid lines were drawn at the bottom of cells, but this didn't
   line up well with the GtkEntry widgets, which in the default theme draw a
   black shadow line across the top. So I've switched our code to draw the
   lines across the top of cells. */
#define E_MEETING_TIME_SELECTOR_DRAW_GRID_LINES_AT_BOTTOM 0

static void e_meeting_time_selector_item_dispose (GObject *object);

static void e_meeting_time_selector_item_set_property (GObject *object,
                                                       guint property_id,
                                                       const GValue *value,
                                                       GParamSpec *pspec);
static void e_meeting_time_selector_item_realize (GnomeCanvasItem *item);
static void e_meeting_time_selector_item_unrealize (GnomeCanvasItem *item);
static void e_meeting_time_selector_item_update (GnomeCanvasItem *item,
						 double *affine,
						 ArtSVP *clip_path, gint flags);
static void e_meeting_time_selector_item_draw (GnomeCanvasItem *item,
					       GdkDrawable *drawable,
					       gint x, gint y,
					       gint width, gint height);
static double e_meeting_time_selector_item_point (GnomeCanvasItem *item,
						  double x, double y,
						  gint cx, gint cy,
						  GnomeCanvasItem **actual_item);
static gint e_meeting_time_selector_item_event (GnomeCanvasItem *item,
						GdkEvent *event);
static gint e_meeting_time_selector_item_button_press (EMeetingTimeSelectorItem *mts_item,
						       GdkEvent *event);
static gint e_meeting_time_selector_item_button_release (EMeetingTimeSelectorItem *mts_item,
							 GdkEvent *event);
static gint e_meeting_time_selector_item_motion_notify (EMeetingTimeSelectorItem *mts_item,
							GdkEvent *event);

static void e_meeting_time_selector_item_paint_day_top (EMeetingTimeSelectorItem *mts_item,
							GdkDrawable *drawable,
							GDate *date,
							gint x, gint scroll_y,
							gint width, gint height);
static void e_meeting_time_selector_item_paint_all_attendees_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, GDate *date, gint x, gint y, gint width, gint height);
static void e_meeting_time_selector_item_paint_day (EMeetingTimeSelectorItem *mts_item,
						    GdkDrawable *drawable,
						    GDate *date,
						    gint x, gint scroll_y,
						    gint width, gint height);
static void e_meeting_time_selector_item_paint_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, GDate *date, gint x, gint scroll_y, gint width, gint height);
static gint e_meeting_time_selector_item_find_first_busy_period (EMeetingTimeSelectorItem *mts_item, GDate *date, gint row);
static void e_meeting_time_selector_item_paint_attendee_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, gint row, gint x, gint y, gint width, gint first_period, EMeetingFreeBusyType busy_type, cairo_t *cr);

static EMeetingTimeSelectorPosition e_meeting_time_selector_item_get_drag_position (EMeetingTimeSelectorItem *mts_item, gint x, gint y);
static gboolean e_meeting_time_selector_item_calculate_busy_range (EMeetingTimeSelector *mts,
								   gint row,
								   gint x,
								   gint width,
								   gint *start_x,
								   gint *end_x);

/* The arguments we take */
enum {
	PROP_0,
	PROP_MEETING_TIME_SELECTOR
};

G_DEFINE_TYPE (EMeetingTimeSelectorItem, e_meeting_time_selector_item, GNOME_TYPE_CANVAS_ITEM)

static void
e_meeting_time_selector_item_class_init (EMeetingTimeSelectorItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_meeting_time_selector_item_dispose;
	object_class->set_property = e_meeting_time_selector_item_set_property;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->realize = e_meeting_time_selector_item_realize;
	item_class->unrealize = e_meeting_time_selector_item_unrealize;
	item_class->update = e_meeting_time_selector_item_update;
	item_class->draw = e_meeting_time_selector_item_draw;
	item_class->point = e_meeting_time_selector_item_point;
	item_class->event = e_meeting_time_selector_item_event;

	g_object_class_install_property (
		object_class,
		PROP_MEETING_TIME_SELECTOR,
		g_param_spec_pointer (
			"meeting_time_selector",
			NULL,
			NULL,
			G_PARAM_WRITABLE));
}

static void
e_meeting_time_selector_item_init (EMeetingTimeSelectorItem *mts_item)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (mts_item);

	mts_item->mts = NULL;

	mts_item->main_gc = NULL;
	mts_item->stipple_gc = NULL;

	/* Create the cursors. */
	mts_item->normal_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	mts_item->resize_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	mts_item->busy_cursor = gdk_cursor_new (GDK_WATCH);
	mts_item->last_cursor_set = NULL;

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;
}

static void
e_meeting_time_selector_item_dispose (GObject *object)
{
	EMeetingTimeSelectorItem *mts_item;

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (object);

	if (mts_item->normal_cursor) {
		gdk_cursor_unref (mts_item->normal_cursor);
		mts_item->normal_cursor = NULL;
	}
	if (mts_item->resize_cursor) {
		gdk_cursor_unref (mts_item->resize_cursor);
		mts_item->resize_cursor = NULL;
	}
	if (mts_item->busy_cursor) {
		gdk_cursor_unref (mts_item->busy_cursor);
		mts_item->busy_cursor = NULL;
	}

	G_OBJECT_CLASS (e_meeting_time_selector_item_parent_class)->dispose (object);
}

static void
e_meeting_time_selector_item_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
	EMeetingTimeSelectorItem *mts_item;

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (object);

	switch (property_id) {
	case PROP_MEETING_TIME_SELECTOR:
		mts_item->mts = g_value_get_pointer (value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_meeting_time_selector_item_realize (GnomeCanvasItem *item)
{
	GnomeCanvas *canvas;
	GdkWindow *window;
	EMeetingTimeSelectorItem *mts_item;

	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->realize)
		(*GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->realize)(item);

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (item);

	canvas = item->canvas;
	window = gtk_widget_get_window (GTK_WIDGET (canvas));

	mts_item->main_gc = gdk_gc_new (window);
	mts_item->stipple_gc = gdk_gc_new (window);
}

static void
e_meeting_time_selector_item_unrealize (GnomeCanvasItem *item)
{
	EMeetingTimeSelectorItem *mts_item;

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (item);

	g_object_unref (mts_item->main_gc);
	mts_item->main_gc = NULL;
	g_object_unref (mts_item->stipple_gc);
	mts_item->stipple_gc = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->unrealize)(item);
}

static void
e_meeting_time_selector_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, gint flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_item_parent_class)->update) (item, affine, clip_path, flags);

	/* The grid covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}

/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */

static void
e_meeting_time_selector_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable, gint x, gint y, gint width, gint height)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorItem *mts_item;
	EMeetingAttendee *ia;
	gint day_x, meeting_start_x, meeting_end_x, bar_y, bar_height;
	gint row, row_y, start_x, end_x;
	GDate date, last_date, current_date;
	gboolean is_display_top, show_meeting_time;
	GdkGC *gc, *stipple_gc;
	cairo_t *cr;

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (item);
	mts = mts_item->mts;
	g_return_if_fail (mts != NULL);
	gc = mts_item->main_gc;
	stipple_gc = mts_item->stipple_gc;
	cr = gdk_cairo_create (drawable);

	is_display_top = (GTK_WIDGET (item->canvas) == mts->display_top)
		? TRUE : FALSE;

	/* Calculate the first and last visible days and positions. */
	e_meeting_time_selector_calculate_day_and_position (mts, x,
							    &date, &day_x);
	e_meeting_time_selector_calculate_day_and_position (mts, x + width,
							    &last_date, NULL);

	/* For the top display draw the 'All Attendees' row background. */
	cairo_save (cr);
	if (is_display_top) {
		gdk_cairo_set_source_color (cr, &mts->all_attendees_bg_color);
		cairo_rectangle (cr, 0, mts->row_height * 2 - y,
				    width, mts->row_height);
		cairo_fill (cr);
	} else {
		gdk_cairo_set_source_color (cr, &mts->bg_color);
		cairo_rectangle (cr,  0, 0, width, height);
		cairo_fill (cr);
	}
	cairo_restore (cr);

	/* Calculate the x coordinates of the meeting time. */
	show_meeting_time = e_meeting_time_selector_get_meeting_time_positions (mts, &meeting_start_x, &meeting_end_x);

	/* Draw the meeting time background. */
	if (show_meeting_time
	    && (meeting_end_x - 1 >= x) && (meeting_start_x + 1 < x + width)
	    && (meeting_end_x - meeting_start_x > 2)) {
		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &mts->meeting_time_bg_color);
		if (is_display_top) {
			cairo_rectangle (cr, meeting_start_x + 1 - x, mts->row_height * 2 - y,
					    meeting_end_x - meeting_start_x - 2, mts->row_height);
			cairo_fill (cr);
		} else {
			cairo_rectangle (cr, meeting_start_x + 1 - x, 0,
					    meeting_end_x - meeting_start_x - 2, height);
			cairo_fill (cr);
		}
		cairo_restore (cr);
	}

	/* For the main display draw the stipple background for attendee's
	   that have no calendar information. */
	if (!is_display_top) {
		gdk_cairo_set_source_color (cr, &mts->grid_color);
		gdk_gc_set_foreground (gc, &mts->grid_color);
		gdk_gc_set_foreground (stipple_gc, &mts->grid_color);
		gdk_gc_set_background (stipple_gc, &mts->stipple_bg_color);
		gdk_gc_set_stipple (stipple_gc, mts->stipple);
		gnome_canvas_set_stipple_origin (item->canvas, stipple_gc);
		gdk_gc_set_fill (stipple_gc, GDK_OPAQUE_STIPPLED);
		row = y / mts->row_height;
		row_y = row * mts->row_height - y;
		while (row < e_meeting_store_count_actual_attendees (mts->model) && row_y < height) {
			ia = e_meeting_store_find_attendee_at_row (mts->model, row);

			if (e_meeting_attendee_get_has_calendar_info (ia)) {
				if (e_meeting_time_selector_item_calculate_busy_range (mts, row, x, width, &start_x, &end_x)) {
					if (start_x >= width || end_x <= 0) {
						gdk_draw_rectangle (drawable, stipple_gc, TRUE, 0, row_y, width, mts->row_height);
					} else {
						if (start_x >= 0) {
							gdk_draw_rectangle (drawable, stipple_gc, TRUE, 0, row_y, start_x, mts->row_height);
							cairo_move_to (cr, start_x, row_y);
							cairo_line_to (cr, start_x, row_y + mts->row_height);
							cairo_stroke (cr);
						}
						if (end_x <= width) {
							gdk_draw_rectangle (drawable, stipple_gc, TRUE, end_x, row_y, width - end_x, mts->row_height);
							cairo_move_to (cr, end_x, row_y);
							cairo_line_to (cr, end_x, row_y + mts->row_height);
							cairo_stroke (cr);
						}
					}
				}
			} else {
				gdk_draw_rectangle (drawable, stipple_gc, TRUE,
						    0, row_y,
						    width, mts->row_height);
			}
			row++;
			row_y += mts->row_height;
		}
		gdk_gc_set_fill (gc, GDK_SOLID);
	}

	/* Now paint the visible days one by one. */
	current_date = date;
	for (;;) {
		/* Currently we use the same GnomeCanvasItem class for the
		   top display and the main display. We may use separate
		   classes in future if necessary. */
		if (is_display_top)
			e_meeting_time_selector_item_paint_day_top (mts_item, drawable, &current_date, day_x, y, width, height);
		else
			e_meeting_time_selector_item_paint_day (mts_item, drawable, &current_date, day_x, y, width, height);

		day_x += mts_item->mts->day_width;
		if (g_date_compare (&current_date, &last_date) == 0)
			break;
		g_date_add_days (&current_date, 1);
	}

	/* Draw the busy periods. */
	if (is_display_top)
		e_meeting_time_selector_item_paint_all_attendees_busy_periods (mts_item, drawable, &date, x, y, width, height);
	else
		e_meeting_time_selector_item_paint_busy_periods (mts_item, drawable, &date, x, y, width, height);

	/* Draw the currently-selected meeting time vertical bars. */
	if (show_meeting_time) {
		if (is_display_top) {
			bar_y = mts->row_height * 2 - y;
			bar_height = mts->row_height;
		} else {
			bar_y = 0;
			bar_height = height;
		}

		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &mts->grid_color);

		if ((meeting_start_x + 2 >= x)
		    && (meeting_start_x - 2 < x + width)) {
			cairo_rectangle (cr, meeting_start_x - 2 - x, bar_y,
					    5, bar_height);
			cairo_fill (cr);
		}

		if ((meeting_end_x + 2 >= x)
		    && (meeting_end_x - 2 < x + width)) {
			cairo_rectangle (cr, meeting_end_x - 2 - x, bar_y,
					    5, bar_height);
			cairo_fill (cr);

		}
		cairo_restore (cr);
	}
	cairo_destroy (cr);
}

static void
e_meeting_time_selector_item_paint_day_top (EMeetingTimeSelectorItem *mts_item,
					    GdkDrawable *drawable, GDate *date,
					    gint x, gint scroll_y,
					    gint width, gint height)
{
	EMeetingTimeSelector *mts;
	GdkGC *gc;
	gint y, grid_x;
	gchar buffer[128], *format;
	gint hour, hour_x, hour_y;
	GdkRectangle clip_rect;
	PangoLayout *layout;

	mts = mts_item->mts;
	gc = mts_item->main_gc;

	gdk_gc_set_foreground (gc, &mts->grid_color);
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (mts), NULL);

	/* Draw the horizontal lines. */
	y = mts->row_height - 1 - scroll_y;
	gdk_draw_line (drawable, gc, x, y, x + mts->day_width - 1, y);
	gdk_gc_set_foreground (gc, &mts->grid_shadow_color);
	gdk_draw_line (drawable, gc, x, y + 1, x + mts->day_width - 1, y + 1);
	gdk_gc_set_foreground (gc, &mts->grid_color);
	y += mts->row_height;
	gdk_draw_line (drawable, gc, x, y, x + mts->day_width - 1, y);
	y += mts->row_height;
	gdk_draw_line (drawable, gc, x, y, x + mts->day_width - 1, y);

	/* Draw the vertical grid lines. */
	for (grid_x = mts->col_width - 1;
	     grid_x < mts->day_width - mts->col_width;
	     grid_x += mts->col_width) {
		gdk_draw_line (drawable, gc,
			       x + grid_x, mts->row_height * 2 - 1 - scroll_y,
			       x + grid_x, height);
	}
	grid_x = mts->day_width - 2;
	gdk_draw_line (drawable, gc, x + grid_x, 0, x + grid_x, height);
	grid_x++;
	gdk_draw_line (drawable, gc, x + grid_x, 0, x + grid_x, height);

	/* Draw the date. Set a clipping rectangle so we don't draw over the
	   next day. */
	if (mts->date_format == E_MEETING_TIME_SELECTOR_DATE_FULL)
		/* This is a strftime() format string %A = full weekday name,
		   %B = full month name, %d = month day, %Y = full year. */
		format = _("%A, %B %d, %Y");
	else if (mts->date_format == E_MEETING_TIME_SELECTOR_DATE_ABBREVIATED_DAY)
		/* This is a strftime() format string %a = abbreviated weekday
		   name, %m = month number, %d = month day, %Y = full year. */
		format = _("%a %m/%d/%Y");
	else
		/* This is a strftime() format string %m = month number,
		   %d = month day, %Y = full year. */
		format = _("%m/%d/%Y");

	g_date_strftime (buffer, sizeof (buffer), format, date);

	clip_rect.x = x;
	clip_rect.y = -scroll_y;
	clip_rect.width = mts->day_width - 2;
	clip_rect.height = mts->row_height - 2;
	gdk_gc_set_clip_rectangle (gc, &clip_rect);
	pango_layout_set_text (layout, buffer, -1);
	gdk_draw_layout (drawable, gc,
			 x + 2,
			 4 - scroll_y,
			 layout);
	gdk_gc_set_clip_rectangle (gc, NULL);

	/* Draw the hours. */
	hour = mts->first_hour_shown;
	hour_x = x + 2;
	hour_y = mts->row_height + 4 - scroll_y;
	while (hour < mts->last_hour_shown) {
		if (calendar_config_get_24_hour_format ())
			pango_layout_set_text (layout, EMeetingTimeSelectorHours[hour], -1);
		else
			pango_layout_set_text (layout, EMeetingTimeSelectorHours12[hour], -1);

		gdk_draw_layout (drawable, gc,
				 hour_x,
				 hour_y,
				 layout);

		hour += mts->zoomed_out ? 3 : 1;
		hour_x += mts->col_width;
	}

	g_object_unref (layout);
}

/* This paints the colored bars representing busy periods for the combined
   list of attendees. For now we just paint the bars for each attendee of
   each other. If we want to speed it up we could optimise it later. */
static void
e_meeting_time_selector_item_paint_all_attendees_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, GDate *date, gint x, gint scroll_y, gint width, gint height)
{
	EMeetingTimeSelector *mts;
	EMeetingFreeBusyType busy_type;
	gint row, y;
	GdkGC *gc;
	gint *first_periods;
	cairo_t *cr;
	guint16 red, green, blue;
	gdouble cc = 65535.0;

	mts = mts_item->mts;
	gc = mts_item->main_gc;
	cr = gdk_cairo_create (drawable);

	/* Calculate the y coordinate to paint the row at in the drawable. */
	y = 2 * mts->row_height - scroll_y - 1;

	/* Get the first visible busy periods for all the attendees. */
	first_periods = g_new (gint, e_meeting_store_count_actual_attendees (mts->model));
	for (row = 0; row < e_meeting_store_count_actual_attendees (mts->model); row++)
		first_periods[row] = e_meeting_time_selector_item_find_first_busy_period (mts_item, date, row);

	for (busy_type = 0;
	     busy_type < E_MEETING_FREE_BUSY_LAST;
	     busy_type++) {
		gdk_gc_set_foreground (gc, &mts->busy_colors[busy_type]);
		red = mts->busy_colors[busy_type].red;
		green = mts->busy_colors[busy_type].green;
		blue = mts->busy_colors[busy_type].blue;
		cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0.8);
		for (row = 0; row < e_meeting_store_count_actual_attendees (mts->model); row++) {
			if (first_periods[row] == -1)
				continue;
			e_meeting_time_selector_item_paint_attendee_busy_periods (mts_item, drawable, x, y, width, row, first_periods[row], busy_type, cr);
		}
	}

	g_free (first_periods);
	cairo_destroy (cr);
}

static void
e_meeting_time_selector_item_paint_day (EMeetingTimeSelectorItem *mts_item,
					GdkDrawable *drawable, GDate *date,
					gint x, gint scroll_y,
					gint width, gint height)
{
	EMeetingTimeSelector *mts;
	GdkGC *gc;
	gint grid_x, grid_y, attendee_index, unused_y;

	mts = mts_item->mts;
	gc = mts_item->main_gc;

	/* Draw the grid lines. The grid lines around unused rows are drawn in
	   a different color. */

	/* Draw the horizontal grid lines. */
	attendee_index = scroll_y / mts->row_height;
#if E_MEETING_TIME_SELECTOR_DRAW_GRID_LINES_AT_BOTTOM
	for (grid_y = mts->row_height - 1 - (scroll_y % mts->row_height);
#else
	for (grid_y = - (scroll_y % mts->row_height);
#endif
	     grid_y < height;
	     grid_y += mts->row_height)
	  {
		  if (attendee_index <= e_meeting_store_count_actual_attendees (mts->model)) {
			  gdk_gc_set_foreground (gc, &mts->grid_color);
			  gdk_draw_line (drawable, gc, 0, grid_y,
					 width, grid_y);
		  } else {
			  gdk_gc_set_foreground (gc, &mts->grid_unused_color);
			  gdk_draw_line (drawable, gc, 0, grid_y,
					 width, grid_y);
		  }
		  attendee_index++;
	  }

	/* Draw the vertical grid lines. */
	unused_y = (e_meeting_store_count_actual_attendees (mts->model) * mts->row_height) - scroll_y;
	if (unused_y >= 0) {
		gdk_gc_set_foreground (gc, &mts->grid_color);
		for (grid_x = mts->col_width - 1;
		     grid_x < mts->day_width - mts->col_width;
		     grid_x += mts->col_width)
			{
				gdk_draw_line (drawable, gc,
					       x + grid_x, 0,
					       x + grid_x, unused_y - 1);
			}
		gdk_draw_rectangle (drawable, gc, TRUE,
				    x + mts->day_width - 2, 0,
				    2, unused_y);
	}

	if (unused_y < height) {
		gdk_gc_set_foreground (gc, &mts->grid_unused_color);
		for (grid_x = mts->col_width - 1;
		     grid_x < mts->day_width - mts->col_width;
		     grid_x += mts->col_width)
			{
				gdk_draw_line (drawable, gc,
					       x + grid_x, unused_y,
					       x + grid_x, height);
			}
		gdk_draw_rectangle (drawable, gc, TRUE,
				    x + mts->day_width - 2, unused_y,
				    2, height - unused_y);
	}

}

/* This paints the colored bars representing busy periods for the individual
   attendees. */
static void
e_meeting_time_selector_item_paint_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, GDate *date, gint x, gint scroll_y, gint width, gint height)
{
	EMeetingTimeSelector *mts;
	EMeetingFreeBusyType busy_type;
	gint row, y, first_period;
	GdkGC *gc;
	cairo_t *cr;
	guint16 red, green, blue;
	gdouble cc = 65535.0;

	mts = mts_item->mts;
	gc = mts_item->main_gc;
	cr = gdk_cairo_create (drawable);

	/* Calculate the first visible attendee row. */
	row = scroll_y / mts->row_height;

	/* Calculate the y coordinate to paint the row at in the drawable. */
	y = row * mts->row_height - scroll_y;

	/* Step through the attendees painting the busy periods. */
	while (y < height && row < e_meeting_store_count_actual_attendees (mts->model)) {

		/* Find the first visible busy period. */
		first_period = e_meeting_time_selector_item_find_first_busy_period (mts_item, date, row);
		if (first_period != -1) {
			/* Paint the different types of busy periods, in
			   reverse order of precedence, so the highest
			   precedences are displayed. */
			for (busy_type = 0;
			     busy_type < E_MEETING_FREE_BUSY_LAST;
			     busy_type++) {
				gdk_gc_set_foreground (gc, &mts->busy_colors[busy_type]);
				red = mts->busy_colors[busy_type].red;
				green = mts->busy_colors[busy_type].green;
				blue = mts->busy_colors[busy_type].blue;
				cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0.8);
				e_meeting_time_selector_item_paint_attendee_busy_periods (mts_item, drawable, x, y, width, row, first_period, busy_type, cr);
			}
		}
		y += mts->row_height;
		row++;
	}
	cairo_destroy (cr);
}

/* This subtracts the attendees longest_period_in_days from the given date,
   and does a binary search of the attendee's busy periods array to find the
   first one which could possible end on the given day or later.
   If none are found it returns -1. */
static gint
e_meeting_time_selector_item_find_first_busy_period (EMeetingTimeSelectorItem *mts_item, GDate *date, gint row)
{
	EMeetingTimeSelector *mts;
	EMeetingAttendee *ia;
	const GArray *busy_periods;
	EMeetingFreeBusyPeriod *period;
	gint period_num;

	mts = mts_item->mts;

	ia = e_meeting_store_find_attendee_at_row (mts->model, row);

	period_num = e_meeting_attendee_find_first_busy_period (ia, date);
	if (period_num == -1)
		return -1;

	/* Check if the period starts after the end of the current canvas
	   scroll area. */
	busy_periods = e_meeting_attendee_get_busy_periods (ia);
	period = &g_array_index (busy_periods, EMeetingFreeBusyPeriod, period_num);
	if (g_date_compare (&mts->last_date_shown, &period->start.date) < 0)
		return -1;

	return period_num;
}

/* This paints the visible busy periods for one attendee which are of a certain
   busy type, e.g out of office. It is passed the index of the first visible
   busy period of the attendee and continues until it runs off the screen. */
static void
e_meeting_time_selector_item_paint_attendee_busy_periods (EMeetingTimeSelectorItem *mts_item, GdkDrawable *drawable, gint x, gint y, gint width, gint row, gint first_period, EMeetingFreeBusyType busy_type, cairo_t *cr)
{
	EMeetingTimeSelector *mts;
	EMeetingAttendee *ia;
	const GArray *busy_periods;
	EMeetingFreeBusyPeriod *period;
	gint period_num, x1, x2, x2_within_day, x2_within_col;

	mts = mts_item->mts;

	ia = e_meeting_store_find_attendee_at_row (mts->model, row);

	busy_periods = e_meeting_attendee_get_busy_periods (ia);
	for (period_num = first_period;
	     period_num < busy_periods->len;
	     period_num++) {
		period = &g_array_index (busy_periods, EMeetingFreeBusyPeriod, period_num);

		if (period->busy_type != busy_type)
			continue;

		/* Convert the period start and end times to x coordinates. */
		x1 = e_meeting_time_selector_calculate_time_position (mts, &period->start);
		/* If the period is off the right of the area being drawn, we
		   are finished. */
		if (x1 >= x + width)
			return;

		x2 = e_meeting_time_selector_calculate_time_position (mts, &period->end);
		/* If the period is off the left edge of the area skip it. */
		if (x2 <= x)
			continue;

		/* We paint from x1 to x2 - 1, so that for example a time
		   from 5:00-6:00 is distinct from 6:00-7:00.
		   We never finish on a grid line separating days, and we only
		   ever paint on a normal vertical grid line if the period is
		   only 1 pixel wide. */
		x2_within_day = x2 % mts->day_width;
		if (x2_within_day == 0) {
			x2 -= 2;
		} else if (x2_within_day == mts->day_width - 1) {
			x2 -= 1;
		} else {
			x2_within_col = x2_within_day % mts->col_width;
			if (x2_within_col == 0 && x2 > x1 + 1)
				x2 -= 1;
		}

		/* Paint the rectangle. We leave a gap of 2 pixels at the
		 top and bottom, remembering that the grid is painted along
		 the top/bottom line of each row. */
		if (x2 - x1 > 0) {
#if E_MEETING_TIME_SELECTOR_DRAW_GRID_LINES_AT_BOTTOM
			cairo_rectangle (cr, x1 - x, y + 2,
					    x2 - x1, mts->row_height - 5);
#else
			cairo_rectangle (cr, x1 - x, y + 3,
					    x2 - x1, mts->row_height - 5);
#endif
			cairo_fill (cr);
		}
	}
}

/*
 * CANVAS ITEM ROUTINES - functions to be a GnomeCanvasItem.
 */

/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_meeting_time_selector_item_point (GnomeCanvasItem *item, double x, double y,
				    gint cx, gint cy,
				    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

static gint
e_meeting_time_selector_item_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMeetingTimeSelectorItem *mts_item;

	mts_item = E_MEETING_TIME_SELECTOR_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return e_meeting_time_selector_item_button_press (mts_item,
								  event);
	case GDK_BUTTON_RELEASE:
		return e_meeting_time_selector_item_button_release (mts_item,
								    event);
	case GDK_MOTION_NOTIFY:
		return e_meeting_time_selector_item_motion_notify (mts_item,
								   event);
	default:
		break;
	}

	return FALSE;
}

/* This handles all button press events for the item. If the cursor is over
   one of the meeting time vertical bars we start a drag. If not we set the
   meeting time to the nearest half-hour interval.
   Note that GnomeCanvas converts the event coords to world coords,
   i.e. relative to the entire canvas scroll area. */
static gint
e_meeting_time_selector_item_button_press (EMeetingTimeSelectorItem *mts_item,
					   GdkEvent *event)
{
	EMeetingTimeSelector *mts;
	EMeetingTime start_time, end_time;
	EMeetingTimeSelectorPosition position;
	GDate *start_date, *end_date;
	gint x, y;

	mts = mts_item->mts;
	x = (gint) event->button.x;
	y = (gint) event->button.y;

	/* Check if we are starting a drag of the vertical meeting time bars.*/
	position = e_meeting_time_selector_item_get_drag_position (mts_item,
								   x, y);
	if (position != E_MEETING_TIME_SELECTOR_POS_NONE) {
		if (gnome_canvas_item_grab (GNOME_CANVAS_ITEM (mts_item),
					    GDK_POINTER_MOTION_MASK
					    | GDK_BUTTON_RELEASE_MASK,
					    mts_item->resize_cursor,
					    event->button.time) == 0 /*Success*/) {
			mts->dragging_position = position;
			return TRUE;
		}
	}

	/* Convert the x coordinate into a EMeetingTimeSelectorTime. */
	e_meeting_time_selector_calculate_time (mts, x, &start_time);
	start_date = &start_time.date;
	end_date = &end_time.date;

	/* Find the nearest half-hour or hour interval, depending on whether
	   zoomed_out is set. */
	if (!mts->all_day) {
		gint astart_year, astart_month, astart_day, astart_hour, astart_minute;
		gint aend_year, aend_month, aend_day, aend_hour, aend_minute;
		gint hdiff, mdiff;
		GDate asdate, aedate;

		e_meeting_time_selector_get_meeting_time (mts_item->mts,
					       &astart_year,
					       &astart_month,
					       &astart_day,
					       &astart_hour,
					       &astart_minute,
					       &aend_year,
					       &aend_month,
					       &aend_day,
					       &aend_hour,
					       &aend_minute);
		if (mts->zoomed_out)
			start_time.minute = 0;
		else
			start_time.minute -= start_time.minute % 30;

		g_date_set_dmy (&asdate, astart_day, astart_month, astart_year);
		g_date_set_dmy (&aedate, aend_day, aend_month, aend_year);
		end_time = start_time;
		mdiff = end_time.minute + aend_minute - astart_minute;
		hdiff = end_time.hour + aend_hour - astart_hour + (24 * g_date_days_between (&asdate, &aedate));
		while (mdiff < 0) {
			mdiff += 60;
			hdiff -= 1;
		}
		while (mdiff > 60) {
			mdiff -= 60;
			hdiff += 1;
		}
		while (hdiff < 0) {
			hdiff += 24;
			g_date_subtract_days (end_date, 1);
		}
		while (hdiff >= 24) {
			hdiff -= 24;
			g_date_add_days (end_date, 1);
		}
		end_time.minute = mdiff;
		end_time.hour = hdiff;
	} else {
		start_time.hour = 0;
		start_time.minute = 0;
		end_time = start_time;
		g_date_add_days (&end_time.date, 1);
	}

	/* Fix any overflows. */
	e_meeting_time_selector_fix_time_overflows (&end_time);

	/* Set the new meeting time. */
	e_meeting_time_selector_set_meeting_time (mts_item->mts,
						  g_date_get_year (start_date),
						  g_date_get_month (start_date),
						  g_date_get_day (start_date),
						  start_time.hour,
						  start_time.minute,
						  g_date_get_year (end_date),
						  g_date_get_month (end_date),
						  g_date_get_day (end_date),
						  end_time.hour,
						  end_time.minute);

	return FALSE;
}

/* This handles all button release events for the item. If we were dragging,
   we finish the drag. */
static gint
e_meeting_time_selector_item_button_release (EMeetingTimeSelectorItem *mts_item,
					     GdkEvent *event)
{
	EMeetingTimeSelector *mts;

	mts = mts_item->mts;

	/* Reset any drag. */
	if (mts->dragging_position != E_MEETING_TIME_SELECTOR_POS_NONE) {
		mts->dragging_position = E_MEETING_TIME_SELECTOR_POS_NONE;
		e_meeting_time_selector_remove_timeout (mts);
		gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (mts_item),
					  event->button.time);
	}

	return FALSE;
}

/* This handles all motion notify events for the item. If button1 is pressed
   we check if a drag is in progress. If not, we set the cursor if we are over
   the meeting time vertical bars. Note that GnomeCanvas doesn't use motion
   hints, which may affect performance. */
static gint
e_meeting_time_selector_item_motion_notify (EMeetingTimeSelectorItem *mts_item,
					    GdkEvent *event)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorPosition position;
	GdkCursor *cursor;
	gint x, y;

	mts = mts_item->mts;
	x = (gint) event->motion.x;
	y = (gint) event->motion.y;

	if (mts->dragging_position != E_MEETING_TIME_SELECTOR_POS_NONE) {
		e_meeting_time_selector_drag_meeting_time (mts, x);
		return TRUE;
	}

	position = e_meeting_time_selector_item_get_drag_position (mts_item,
								   x, y);

	/* Determine which cursor should be used. */
	if (position != E_MEETING_TIME_SELECTOR_POS_NONE)
		cursor = mts_item->resize_cursor;
	/* If the Main window shows busy cursor show the same */
	else if (mts_item->mts->last_cursor_set == GDK_WATCH)
		cursor = mts_item->busy_cursor;
	else
		cursor = mts_item->normal_cursor;

	/* Only set the cursor if it is different to the last one we set. */
	if (mts_item->last_cursor_set != cursor) {
		GdkWindow *window;
		GnomeCanvas *canvas;

		mts_item->last_cursor_set = cursor;

		canvas = GNOME_CANVAS_ITEM (mts_item)->canvas;
		window = gtk_widget_get_window (GTK_WIDGET (canvas));
		gdk_window_set_cursor (window, cursor);
	}

	return FALSE;
}

static EMeetingTimeSelectorPosition
e_meeting_time_selector_item_get_drag_position (EMeetingTimeSelectorItem *mts_item,
						gint x, gint y)
{
	EMeetingTimeSelector *mts;
	gboolean is_display_top;
	gint meeting_start_x, meeting_end_x;

	mts = mts_item->mts;

	is_display_top = (GTK_WIDGET (GNOME_CANVAS_ITEM (mts_item)->canvas) == mts->display_top) ? TRUE : FALSE;

	if (is_display_top && y < mts->row_height * 2)
		return E_MEETING_TIME_SELECTOR_POS_NONE;

	if (!e_meeting_time_selector_get_meeting_time_positions (mts, &meeting_start_x, &meeting_end_x))
		return E_MEETING_TIME_SELECTOR_POS_NONE;

	if (x >= meeting_end_x - 2 && x <= meeting_end_x + 2)
		return E_MEETING_TIME_SELECTOR_POS_END;

	if (x >= meeting_start_x - 2 && x <= meeting_start_x + 2)
		return E_MEETING_TIME_SELECTOR_POS_START;

	return E_MEETING_TIME_SELECTOR_POS_NONE;
}

static gboolean
e_meeting_time_selector_item_calculate_busy_range (EMeetingTimeSelector *mts,
						   gint row,
						   gint x,
						   gint width,
						   gint *start_x,
						   gint *end_x)
{
	EMeetingAttendee *ia;
	EMeetingTime busy_periods_start;
	EMeetingTime busy_periods_end;

	ia = e_meeting_store_find_attendee_at_row (mts->model, row);
	busy_periods_start = e_meeting_attendee_get_start_busy_range (ia);
	busy_periods_end = e_meeting_attendee_get_end_busy_range (ia);

	*start_x = -1;
	*end_x = -1;

	if (!g_date_valid (&busy_periods_start.date)
	    || !g_date_valid (&busy_periods_end.date))
		return FALSE;

	*start_x = e_meeting_time_selector_calculate_time_position (mts, &busy_periods_start) - x - 1;

	*end_x = e_meeting_time_selector_calculate_time_position (mts, &busy_periods_end) - x;

	return TRUE;
}

void
e_meeting_time_selector_item_set_normal_cursor (EMeetingTimeSelectorItem *mts_item)
{
	GnomeCanvas *canvas;
	GdkWindow *window;

	g_return_if_fail (IS_E_MEETING_TIME_SELECTOR_ITEM (mts_item));

	canvas = GNOME_CANVAS_ITEM (mts_item)->canvas;
	window = gtk_widget_get_window (GTK_WIDGET (canvas));
	gdk_window_set_cursor (window, mts_item->normal_cursor);
}

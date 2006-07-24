/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 1999, Ximian, Inc.
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

/*
 * EDayViewMainItem - canvas item which displays most of the appointment
 * data in the main Day/Work Week display.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-util/e-categories-config.h"
#include "e-day-view-layout.h"
#include "e-day-view-main-item.h"
#include "ea-calendar.h"
#include "e-calendar-view.h"
#include <libecal/e-cal-time-util.h>
#include <e-calendar-view.h>

static void e_day_view_main_item_set_arg (GtkObject *o, GtkArg *arg ,
					  guint arg_id);
static void e_day_view_main_item_update (GnomeCanvasItem *item,
					 double *affine,
					 ArtSVP *clip_path, int flags);
static void e_day_view_main_item_draw (GnomeCanvasItem *item,
				       GdkDrawable *drawable,
				       int x, int y,
				       int width, int height);
static double e_day_view_main_item_point (GnomeCanvasItem *item,
					  double x, double y,
					  int cx, int cy,
					  GnomeCanvasItem **actual_item);
static gint e_day_view_main_item_event (GnomeCanvasItem *item,
					GdkEvent *event);

static void e_day_view_main_item_draw_long_events_in_vbars (EDayViewMainItem *dvmitem,
							    GdkDrawable *drawable,
							    int x,
							    int y,
							    int width,
							    int height);
static void e_day_view_main_item_draw_events_in_vbars (EDayViewMainItem *dvmitem,
						       GdkDrawable *drawable,
						       int x, int y,
						       int width, int height,
						       gint day);
static void e_day_view_main_item_draw_day_events (EDayViewMainItem *dvmitem,
						  GdkDrawable *drawable,
						  int x, int y,
						  int width, int height,
						  gint day);
static void e_day_view_main_item_draw_day_event (EDayViewMainItem *dvmitem,
						 GdkDrawable *drawable,
						 int x, int y,
						 int width, int height,
						 gint day, gint event_num);

/* The arguments we take */
enum {
	ARG_0,
	ARG_DAY_VIEW
};

G_DEFINE_TYPE (EDayViewMainItem, e_day_view_main_item, GNOME_TYPE_CANVAS_ITEM)

static void
e_day_view_main_item_class_init (EDayViewMainItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EDayViewMainItem::day_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_DAY_VIEW);

	object_class->set_arg = e_day_view_main_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_day_view_main_item_update;
	item_class->draw        = e_day_view_main_item_draw;
	item_class->point       = e_day_view_main_item_point;
	item_class->event       = e_day_view_main_item_event;

	/* init the accessibility support for e_day_view */
 	e_day_view_main_item_a11y_init ();
}


static void
e_day_view_main_item_init (EDayViewMainItem *dvtitem)
{
	dvtitem->day_view = NULL;
}


static void
e_day_view_main_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EDayViewMainItem *dvmitem;

	dvmitem = E_DAY_VIEW_MAIN_ITEM (o);
	
	switch (arg_id){
	case ARG_DAY_VIEW:
		dvmitem->day_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_day_view_main_item_update (GnomeCanvasItem *item,
			    double *affine,
			    ArtSVP *clip_path,
			    int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_day_view_main_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_day_view_main_item_parent_class)->update) (item, affine, clip_path, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}


/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */
#ifndef ENABLE_CAIRO
static void
e_day_view_main_item_draw (GnomeCanvasItem *canvas_item, GdkDrawable *drawable,
			   int x, int y, int width, int height)
{
	EDayViewMainItem *dvmitem;
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *gc;
	gint row, row_y, grid_x1, grid_x2;
	gint day, grid_y1, grid_y2;
	gint work_day_start_y, work_day_end_y;
	gint day_x, day_w, work_day;
	gint start_row, end_row, rect_x, rect_y, rect_width, rect_height;
	struct icaltimetype day_start_tt;
	gint weekday;

#if 0
	g_print ("In e_day_view_main_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif
	dvmitem = E_DAY_VIEW_MAIN_ITEM (canvas_item);
	day_view = dvmitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));

	/* Paint the background colors. */
	gc = day_view->main_gc;
	work_day_start_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute) - y;
	work_day_end_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_end_hour, day_view->work_day_end_minute) - y;

	for (day = 0; day < day_view->days_shown; day++) {
		day_start_tt = icaltime_from_timet_with_zone (day_view->day_starts[day], FALSE,
							      e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
		weekday = icaltime_day_of_week (day_start_tt) - 1;
		
		work_day = day_view->working_days & (1 << weekday);

		day_x = day_view->day_offsets[day] - x;
		day_w = day_view->day_widths[day];

		if (work_day) {
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    day_x, 0 - y,
					    day_w, work_day_start_y - (0 - y));
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    day_x, work_day_start_y,
					    day_w, work_day_end_y - work_day_start_y);
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    day_x, work_day_end_y,
					    day_w, height - work_day_end_y);
		} else {
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    day_x, 0,
					    day_w, height);
		}
	}

	/* Paint the selection background. */
	if (day_view->selection_start_day != -1
	    && !day_view->selection_in_top_canvas) {
		for (day = day_view->selection_start_day;
		     day <= day_view->selection_end_day;
		     day++) {
			if (day == day_view->selection_start_day
			    && day_view->selection_start_row != -1)
				start_row = day_view->selection_start_row;
			else
				start_row = 0;
			if (day == day_view->selection_end_day
			    && day_view->selection_end_row != -1)
				end_row = day_view->selection_end_row;
			else
				end_row = day_view->rows - 1;

			rect_x = day_view->day_offsets[day] - x;
			rect_width = day_view->day_widths[day];
			rect_y = start_row * day_view->row_height - y;
			rect_height = (end_row - start_row + 1) * day_view->row_height;

			if (GTK_WIDGET_HAS_FOCUS(day_view))
				gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED]);
			else
				gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED_UNFOCUSSED]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    rect_x, rect_y,
					    rect_width, rect_height);
		}
	}

	/* Drawing the horizontal grid lines. */
	grid_x1 = day_view->day_offsets[0] - x;
	grid_x2 = day_view->day_offsets[day_view->days_shown] - x;

	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		if (row_y >= 0 && row_y < height)
			gdk_draw_line (drawable, gc,
				       grid_x1, row_y, grid_x2, row_y);
	}

	/* Draw the vertical bars down the left of each column. */
	grid_y1 = 0;
	grid_y2 = height;
	for (day = 0; day < day_view->days_shown; day++) {
		grid_x1 = day_view->day_offsets[day] - x;

		/* Skip if it isn't visible. */
		if (grid_x1 >= width || grid_x1 + E_DAY_VIEW_BAR_WIDTH <= 0)
			continue;

		gdk_draw_line (drawable, style->black_gc,
			       grid_x1, grid_y1,
			       grid_x1, grid_y2);
		gdk_draw_line (drawable, style->black_gc,
			       grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y1,
			       grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y2);
		gdk_draw_rectangle (drawable, style->white_gc, TRUE,
			       grid_x1 + 1, grid_y1,
			       E_DAY_VIEW_BAR_WIDTH - 2, grid_y2 - grid_y1);

		/* Fill in the bars when the user is busy. */
		e_day_view_main_item_draw_events_in_vbars (dvmitem, drawable,
							   x, y,
							   width, height,
							   day);
	}

	/* Fill in the vertical bars corresponding to the busy times from the
	   long events. */
	e_day_view_main_item_draw_long_events_in_vbars (dvmitem, drawable,
							x, y, width, height);

	/* Draw the event borders and backgrounds, and the vertical bars
	   down the left edges. */
	for (day = 0; day < day_view->days_shown; day++) {
		e_day_view_main_item_draw_day_events (dvmitem, drawable,
						      x, y, width, height,
						      day);
	}


	if (e_day_view_get_show_marcus_bains (day_view)) {
		icaltimezone *zone;
		struct icaltimetype time_now, day_start;
		int marcus_bains_y;
		GdkColor mb_color;

		gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE]);

		if (day_view->marcus_bains_day_view_color && gdk_color_parse (day_view->marcus_bains_day_view_color, &mb_color)) {
			GdkColormap *colormap;
			
			colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
			if (gdk_colormap_alloc_color (colormap, &mb_color, TRUE, TRUE)) {
				gdk_gc_set_foreground (gc, &mb_color);
			}
		}

		zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
		time_now = icaltime_current_time_with_zone (zone);
		
		for (day = 0; day < day_view->days_shown; day++) {
			day_start = icaltime_from_timet_with_zone (day_view->day_starts[day], FALSE, zone);

			if ((day_start.year  == time_now.year) &&
			    (day_start.month == time_now.month) &&
			    (day_start.day   == time_now.day)) {

				grid_x1 = day_view->day_offsets[day] - x + E_DAY_VIEW_BAR_WIDTH;
				grid_x2 = day_view->day_offsets[day + 1] - x - 1;
				marcus_bains_y = (time_now.hour * 60 + time_now.minute) * day_view->row_height / day_view->mins_per_row - y;
				gdk_draw_line (drawable, gc, grid_x1, marcus_bains_y, grid_x2, marcus_bains_y);
			}
		}
	}


}
static void
e_day_view_main_item_draw_events_in_vbars (EDayViewMainItem *dvmitem,
					   GdkDrawable *drawable,
					   int x, int y,
					   int width, int height,
					   gint day)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GdkGC *gc;
	gint grid_x, event_num, bar_y, bar_h;
	ECalComponentTransparency transparency;

	day_view = dvmitem->day_view;

	gc = day_view->main_gc;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	grid_x = day_view->day_offsets[day] + 1 - x;

	/* Draw the busy times corresponding to the events in the day. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		ECalComponent *comp;

		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		/* If the event is TRANSPARENT, skip it. */
		e_cal_component_get_transparency (comp, &transparency);
		if (transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
			continue;

		/* We can skip the events in the first column since they will
		   draw over this anyway. */
		if (event->num_columns > 0 && event->start_row_or_col == 0)
			continue;

		bar_y = event->start_minute * day_view->row_height / day_view->mins_per_row;
		bar_h = event->end_minute * day_view->row_height / day_view->mins_per_row - bar_y;
		bar_y -= y;

		/* Skip it if it isn't visible. */
		if (bar_y >= height || bar_y + bar_h <= 0)
			continue;

		gdk_draw_rectangle (drawable, gc, TRUE,
				    grid_x, bar_y,
				    E_DAY_VIEW_BAR_WIDTH - 2, bar_h);

		g_object_unref (comp);
	}
}


static void
e_day_view_main_item_draw_long_events_in_vbars (EDayViewMainItem *dvmitem,
						GdkDrawable *drawable,
						int x, int y,
						int width, int height)
{
	EDayView *day_view;
	EDayViewEvent *event;
	gint event_num, start_day, end_day, day, bar_y1, bar_y2, grid_x;
	GdkGC *gc;
	ECalComponentTransparency transparency;

	day_view = dvmitem->day_view;

	gc = day_view->main_gc;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		ECalComponent *comp;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		/* If the event is TRANSPARENT, skip it. */
		e_cal_component_get_transparency (comp, &transparency);
		if (transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
			continue;

		if (!e_day_view_find_long_event_days (event,
						      day_view->days_shown,
						      day_view->day_starts,
						      &start_day, &end_day))
			continue;

		for (day = start_day; day <= end_day; day++) {
			grid_x = day_view->day_offsets[day] + 1 - x;

			/* Skip if it isn't visible. */
			if (grid_x >= width
			    || grid_x + E_DAY_VIEW_BAR_WIDTH <= 0)
				continue;

			if (event->start <= day_view->day_starts[day]) {
				bar_y1 = 0;
			} else {
				bar_y1 = event->start_minute * day_view->row_height / day_view->mins_per_row - y;
			}

			if (event->end >= day_view->day_starts[day + 1]) {
				bar_y2 = height;
			} else {
				bar_y2 = event->end_minute * day_view->row_height / day_view->mins_per_row - y;
			}

			if (bar_y1 < height && bar_y2 > 0 && bar_y2 > bar_y1) {
				gdk_draw_rectangle (drawable, gc, TRUE,
						    grid_x, bar_y1,
						    E_DAY_VIEW_BAR_WIDTH - 2,
						    bar_y2 - bar_y1);
			}
		}


		g_object_unref (comp);
	}
}


static void
e_day_view_main_item_draw_day_events (EDayViewMainItem *dvmitem,
				      GdkDrawable *drawable,
				      int x, int y, int width, int height,
				      gint day)
{
	EDayView *day_view;
	gint event_num;

	day_view = dvmitem->day_view;

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		e_day_view_main_item_draw_day_event (dvmitem, drawable,
						     x, y, width, height,
						     day, event_num);
	}
}


static void
e_day_view_main_item_draw_day_event (EDayViewMainItem *dvmitem,
				     GdkDrawable *drawable,
				     int x, int y, int width, int height,
				     gint day, gint event_num)
{
	EDayView *day_view;
	EDayViewEvent *event;
	gint item_x, item_y, item_w, item_h, bar_y1, bar_y2;
	GdkGC *gc;
	GdkColor bg_color;
	ECalComponent *comp;
	gint num_icons, icon_x, icon_y, icon_x_inc, icon_y_inc;
	gint max_icon_w, max_icon_h;
	gboolean draw_reminder_icon, draw_recurrence_icon, draw_timezone_icon, draw_meeting_icon;
	gboolean draw_attach_icon;
	GSList *categories_list, *elem;
	ECalComponentTransparency transparency;

	day_view = dvmitem->day_view;

	/* If the event is currently being dragged, don't draw it. It will
	   be drawn in the special drag items. */
	if (day_view->drag_event_day == day
	    && day_view->drag_event_num == event_num)
		return;

	gc = day_view->main_gc;

	/* Get the position of the event. If it is not shown skip it.*/
	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h))
		return;

	item_x -= x;
	item_y -= y;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* Fill in the event background. Note that for events in the first
	   column of the day, we might not want to paint over the vertical bar,
	   since that is used for multiple events. But then you can't see
	   where the event in the first column finishes. */

	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND]);

	if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data),
			     &bg_color)) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
		if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE))
			gdk_gc_set_foreground (gc, &bg_color);
	}

#if 1
	if (event->start_row_or_col == 0)
		gdk_draw_rectangle (drawable, gc, TRUE,
				    item_x + E_DAY_VIEW_BAR_WIDTH, item_y + 1,
				    MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 1, 0),
				    item_h - 2);
	else
#endif
		gdk_draw_rectangle (drawable, gc, TRUE,
				    item_x + 1, item_y + 1,
				    MAX (item_w - 2, 0), item_h - 2);

	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Draw the right edge of the vertical bar. */
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER]);
	gdk_draw_line (drawable, gc,
		       item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		       item_y + 1,
		       item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		       item_y + item_h - 2);

	/* Draw the vertical colored bar showing when the appointment
	   begins & ends. */
	bar_y1 = event->start_minute * day_view->row_height / day_view->mins_per_row - y;
	bar_y2 = event->end_minute * day_view->row_height / day_view->mins_per_row - y;

	/* When an item is being resized, we fill the bar up to the new row. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE)
			bar_y1 = item_y + 1;
		else if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_BOTTOM_EDGE)
			bar_y2 = item_y + item_h - 1;
	}

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	/* Only fill it in if the event isn't TRANSPARENT. */
	e_cal_component_get_transparency (comp, &transparency);
	if (transparency != E_CAL_COMPONENT_TRANSP_TRANSPARENT) {
		gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);
		gdk_draw_rectangle (drawable, gc, TRUE,
				    item_x + 1, bar_y1,
				    E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);
	}

	/* Draw the box around the entire event. Do this after drawing
	   the colored bar so we don't have to worry about being 1
	   pixel out. */
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BORDER]);
	gdk_draw_rectangle (drawable, gc, FALSE,
			    item_x, item_y, MAX (item_w - 1, 0), item_h - 1);
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

#if 0
	/* Draw the horizontal bars above and beneath the event if it
	   is currently being edited. */
	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num) {
		gdk_draw_rectangle (drawable, gc, TRUE,
				    item_x,
				    item_y - E_DAY_VIEW_BAR_HEIGHT,
				    item_w,
				    E_DAY_VIEW_BAR_HEIGHT);
		gdk_draw_rectangle (drawable, gc, TRUE,
				    item_x, item_y + item_h,
				    item_w, E_DAY_VIEW_BAR_HEIGHT);
	}
#endif

	/* Draw the reminder & recurrence icons, if needed. */
	num_icons = 0;
	draw_reminder_icon = FALSE;
	draw_recurrence_icon = FALSE;
	draw_timezone_icon = FALSE;
	draw_meeting_icon = FALSE;
	draw_attach_icon = FALSE;
	icon_x = item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_ICON_X_PAD;
	icon_y = item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
		+ E_DAY_VIEW_ICON_Y_PAD;

	if (e_cal_component_has_alarms (comp)) {
		draw_reminder_icon = TRUE;
		num_icons++;
	}

	if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp)) {
		draw_recurrence_icon = TRUE;
		num_icons++;
	}
	if (e_cal_component_has_attachments (comp)) {
		draw_attach_icon = TRUE;
		num_icons++;
	}
	/* If the DTSTART or DTEND are in a different timezone to our current
	   timezone, we display the timezone icon. */
	if (event->different_timezone) {
		draw_timezone_icon = TRUE;
		num_icons++;
	}

	if (e_cal_component_has_organizer (comp)) {
		draw_meeting_icon = TRUE;
		num_icons++;
	}

	e_cal_component_get_categories_list (comp, &categories_list);
	for (elem = categories_list; elem; elem = elem->next) {
		char *category;
		GdkPixmap *pixmap = NULL;
		GdkBitmap *mask = NULL;

		category = (char *) elem->data;
		if (e_categories_config_get_icon_for (category, &pixmap, &mask))
			num_icons++;
	}

	if (num_icons != 0) {
		if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD)
		    * num_icons) {
			icon_x_inc = 0;
			icon_y_inc = E_DAY_VIEW_ICON_HEIGHT
				+ E_DAY_VIEW_ICON_Y_PAD;
		} else {
			icon_x_inc = E_DAY_VIEW_ICON_WIDTH
				+ E_DAY_VIEW_ICON_X_PAD;
			icon_y_inc = 0;
		}

		if (draw_reminder_icon) {
			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_mask (gc, NULL);
			gdk_draw_pixbuf (drawable, gc,
					 day_view->reminder_icon,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h),
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}

		if (draw_recurrence_icon) {
			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_mask (gc, NULL);
			gdk_draw_pixbuf (drawable, gc,
					 day_view->recurrence_icon,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h),
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}
		if (draw_attach_icon) {
			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_mask (gc, NULL);
			gdk_draw_pixbuf (drawable, gc,
					 day_view->attach_icon,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h),
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}
		if (draw_timezone_icon) {
			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_mask (gc, NULL);
			gdk_draw_pixbuf (drawable, gc,
					 day_view->timezone_icon,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h),
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}


		if (draw_meeting_icon) {
			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_mask (gc, NULL);
			gdk_draw_pixbuf (drawable, gc,
					 day_view->meeting_icon,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h),
					 GDK_RGB_DITHER_NORMAL,
					 0, 0);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}

		/* draw categories icons */
		for (elem = categories_list; elem; elem = elem->next) {
			char *category;
			GdkPixmap *pixmap = NULL;
			GdkBitmap *mask = NULL;

			category = (char *) elem->data;
			if (!e_categories_config_get_icon_for (category, &pixmap, &mask))
				continue;

			max_icon_w = item_x + item_w - icon_x
				- E_DAY_VIEW_EVENT_BORDER_WIDTH;
			max_icon_h = item_y + item_h - icon_y
				- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			if (mask != NULL)
				gdk_gc_set_clip_mask (gc, mask);
			gdk_draw_pixmap (drawable, gc,
					 pixmap,
					 0, 0, icon_x, icon_y,
					 MIN (E_DAY_VIEW_ICON_WIDTH,
					      max_icon_w),
					 MIN (E_DAY_VIEW_ICON_HEIGHT,
					      max_icon_h));

			gdk_pixmap_unref (pixmap);
			if (mask != NULL)
				gdk_bitmap_unref (mask);

			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}
		
		gdk_gc_set_clip_mask (gc, NULL);
	}

	/* free memory */
	e_cal_component_free_categories_list (categories_list);
	g_object_unref (comp);
}
#endif

#ifdef ENABLE_CAIRO
static void
e_day_view_main_item_draw (GnomeCanvasItem *canvas_item, GdkDrawable *drawable,
			   int x, int y, int width, int height)
{
	EDayViewMainItem *dvmitem;
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *gc;
	gint row, row_y, grid_x1, grid_x2;
	gint day, grid_y1, grid_y2;
	gint work_day_start_y, work_day_end_y;
	gint day_x, day_w, work_day;
	gint start_row, end_row, rect_x, rect_y, rect_width, rect_height;
	struct icaltimetype day_start_tt;
	gint weekday;
	cairo_t *cr;

	cr = gdk_cairo_create (drawable);

#if 0
	g_print ("In e_day_view_main_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif

	dvmitem = E_DAY_VIEW_MAIN_ITEM (canvas_item);
	day_view = dvmitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));

	/* Paint the background colors. */
	work_day_start_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute) - y;
	gc = day_view->main_gc;
	work_day_end_y = e_day_view_convert_time_to_position (day_view, day_view->work_day_end_hour, day_view->work_day_end_minute) - y;

	for (day = 0; day < day_view->days_shown; day++) {
		day_start_tt = icaltime_from_timet_with_zone (day_view->day_starts[day], FALSE,
							      e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
		weekday = icaltime_day_of_week (day_start_tt) - 1;
		
		work_day = day_view->working_days & (1 << weekday);

		day_x = day_view->day_offsets[day] - x;
		day_w = day_view->day_widths[day];

		if (work_day) {
			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			
			cairo_rectangle (cr, day_x, 0 - y, day_w, 
					work_day_start_y - (0 - y));
			cairo_fill (cr);
			cairo_restore (cr);

			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);
			
			cairo_rectangle (cr, day_x, work_day_start_y, day_w, 
					work_day_end_y - work_day_start_y);
			cairo_fill (cr);
			cairo_restore (cr);

			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			
			cairo_rectangle (cr, day_x, work_day_end_y, day_w, 
					height - work_day_end_y);
			cairo_fill (cr);
			cairo_restore (cr);
		} else {
			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);

			cairo_rectangle (cr, day_x, 0, day_w, height);
			cairo_fill (cr);
			cairo_restore (cr);
		}
	}

	/* Paint the selection background. */
	if (day_view->selection_start_day != -1
	    && !day_view->selection_in_top_canvas) {
		for (day = day_view->selection_start_day;
		     day <= day_view->selection_end_day;
		     day++) {
			if (day == day_view->selection_start_day
			    && day_view->selection_start_row != -1)
				start_row = day_view->selection_start_row;
			else
				start_row = 0;
			if (day == day_view->selection_end_day
			    && day_view->selection_end_row != -1)
				end_row = day_view->selection_end_row;
			else
				end_row = day_view->rows - 1;

			rect_x = day_view->day_offsets[day] - x;
			rect_width = day_view->day_widths[day];
			rect_y = start_row * day_view->row_height - y;
			rect_height = (end_row - start_row + 1) * day_view->row_height;

			if (GTK_WIDGET_HAS_FOCUS(day_view)) {
				cairo_save (cr);
				gdk_cairo_set_source_color (cr, 
					&day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED]);
				cairo_rectangle (cr, rect_x, rect_y, rect_width, 
						rect_height);
				cairo_fill (cr);
				cairo_restore (cr);
			} else {
				cairo_save (cr);
				gdk_cairo_set_source_color (cr, 
					&day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED_UNFOCUSSED]);
				cairo_rectangle (cr, rect_x, rect_y, rect_width, 
					rect_height);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		}
	}

	/* Drawing the horizontal grid lines. */
	grid_x1 = day_view->day_offsets[0] - x;
	grid_x2 = day_view->day_offsets[day_view->days_shown] - x;

	cairo_save(cr);
	gdk_cairo_set_source_color (cr, 
		&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
	
	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		if (row_y >= 0 && row_y < height) {
			cairo_set_line_width (cr, 0.7);
			cairo_move_to (cr, grid_x1, row_y);
			cairo_line_to (cr, grid_x2, row_y);
			cairo_stroke (cr);
		}
	}
	cairo_restore (cr);

	/* Draw the vertical bars down the left of each column. */
	grid_y1 = 0;
	grid_y2 = height;
	for (day = 0; day < day_view->days_shown; day++) {
		grid_x1 = day_view->day_offsets[day] - x;

		/* Skip if it isn't visible. */
		if (grid_x1 >= width || grid_x1 + E_DAY_VIEW_BAR_WIDTH <= 0)
			continue;
		cairo_save (cr);

		gdk_cairo_set_source_color (cr, 
		&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
		cairo_move_to (cr, grid_x1, grid_y1);
		cairo_line_to (cr, grid_x1, grid_y2);
		cairo_stroke (cr);
		
		gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);

		cairo_move_to (cr, grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y1);
		cairo_line_to (cr, grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y2);
		cairo_stroke (cr);
			
		cairo_set_source_rgb (cr, 1, 1, 1);

		cairo_rectangle (cr, grid_x1 + 1, grid_y1,
			       E_DAY_VIEW_BAR_WIDTH - 2, grid_y2 - grid_y1);
	
		cairo_fill (cr);
		
		cairo_restore (cr);

		/* Fill in the bars when the user is busy. */
		e_day_view_main_item_draw_events_in_vbars (dvmitem, drawable,
							   x, y,
							   width, height,
							   day);
	}

	/* Fill in the vertical bars corresponding to the busy times from the
	   long events. */
	e_day_view_main_item_draw_long_events_in_vbars (dvmitem, drawable,
							x, y, width, height);

	/* Draw the event borders and backgrounds, and the vertical bars
	   down the left edges. */
	for (day = 0; day < day_view->days_shown; day++) {
		e_day_view_main_item_draw_day_events (dvmitem, drawable,
						      x, y, width, height,
						      day);
	}


	if (e_day_view_get_show_marcus_bains (day_view)) {
		icaltimezone *zone;
		struct icaltimetype time_now, day_start;
		int marcus_bains_y;
		GdkColor mb_color;

		cairo_save (cr);
		gdk_cairo_set_source_color (cr, 
				&day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE]);

		if (day_view->marcus_bains_day_view_color && gdk_color_parse (day_view->marcus_bains_day_view_color, &mb_color)) {
			GdkColormap *colormap;
			
			colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
			if (gdk_colormap_alloc_color (colormap, &mb_color, TRUE, TRUE))
				gdk_cairo_set_source_color (cr, &mb_color);
		}
		zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
		time_now = icaltime_current_time_with_zone (zone);
		
		for (day = 0; day < day_view->days_shown; day++) {
			day_start = icaltime_from_timet_with_zone (day_view->day_starts[day], FALSE, zone);

			if ((day_start.year  == time_now.year) &&
			    (day_start.month == time_now.month) &&
			    (day_start.day   == time_now.day)) {

				grid_x1 = day_view->day_offsets[day] - x + E_DAY_VIEW_BAR_WIDTH;
				grid_x2 = day_view->day_offsets[day + 1] - x - 1;
				marcus_bains_y = (time_now.hour * 60 + time_now.minute) * day_view->row_height / day_view->mins_per_row - y;
				cairo_set_line_width (cr, 1.5);
				cairo_move_to (cr, grid_x1, marcus_bains_y);
				cairo_line_to (cr, grid_x2, marcus_bains_y);
				cairo_stroke (cr);
			}
		}
		cairo_restore (cr);
	}
	cairo_destroy (cr);	
}


static void
e_day_view_main_item_draw_events_in_vbars (EDayViewMainItem *dvmitem,
					   GdkDrawable *drawable,
					   int x, int y,
					   int width, int height,
					   gint day)
{
	EDayView *day_view;
	EDayViewEvent *event;
	gint grid_x, event_num, bar_y, bar_h;
	ECalComponentTransparency transparency;
	cairo_t *cr;
	GdkColor bg_color;
	day_view = dvmitem->day_view;

	cr = gdk_cairo_create (drawable);
	cairo_save (cr);
			
	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND]);

	grid_x = day_view->day_offsets[day] + 1 - x;

	/* Draw the busy times corresponding to the events in the day. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		ECalComponent *comp;

		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);
		if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data),
				     &bg_color)) {
			GdkColormap *colormap;

			colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
			if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE)) {
				gdk_cairo_set_source_color (cr, 
					&bg_color);
				}
		}


		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		/* If the event is TRANSPARENT, skip it. */
		e_cal_component_get_transparency (comp, &transparency);
		if (transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
			continue;

		/* We can skip the events in the first column since they will
		   draw over this anyway. */
		if (event->num_columns > 0 && event->start_row_or_col == 0)
			continue;

		bar_y = event->start_minute * day_view->row_height / day_view->mins_per_row;
		bar_h = event->end_minute * day_view->row_height / day_view->mins_per_row - bar_y;
		bar_y -= y;

		/* Skip it if it isn't visible. */
		if (bar_y >= height || bar_y + bar_h <= 0)
			continue;

		cairo_rectangle (cr, grid_x, bar_y,
			       E_DAY_VIEW_BAR_WIDTH - 2, bar_h);
	
		cairo_fill (cr);

		g_object_unref (comp);
	}
	cairo_restore (cr);
	cairo_destroy (cr);
}


static void
e_day_view_main_item_draw_long_events_in_vbars (EDayViewMainItem *dvmitem,
						GdkDrawable *drawable,
						int x, int y,
						int width, int height)
{
	EDayView *day_view;
	EDayViewEvent *event;
	gint event_num, start_day, end_day, day, bar_y1, bar_y2, grid_x;
	ECalComponentTransparency transparency;
	cairo_t *cr;
	GdkColor bg_color;

	day_view = dvmitem->day_view;

	cr = gdk_cairo_create (drawable);
	cairo_save (cr);

	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND]);

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		ECalComponent *comp;

		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);
		if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data),
				     &bg_color)) {
			GdkColormap *colormap;

			colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
			if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE)) {
				gdk_cairo_set_source_color (cr, &bg_color);
			}
		}

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		/* If the event is TRANSPARENT, skip it. */
		e_cal_component_get_transparency (comp, &transparency);
		if (transparency == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
			continue;

		if (!e_day_view_find_long_event_days (event,
						      day_view->days_shown,
						      day_view->day_starts,
						      &start_day, &end_day))
			continue;

		for (day = start_day; day <= end_day; day++) {
			grid_x = day_view->day_offsets[day] + 1 - x;

			/* Skip if it isn't visible. */
			if (grid_x >= width
			    || grid_x + E_DAY_VIEW_BAR_WIDTH <= 0)
				continue;

			if (event->start <= day_view->day_starts[day]) {
				bar_y1 = 0;
			} else {
				bar_y1 = event->start_minute * day_view->row_height / day_view->mins_per_row - y;
			}

			if (event->end >= day_view->day_starts[day + 1]) {
				bar_y2 = height;
			} else {
				bar_y2 = event->end_minute * day_view->row_height / day_view->mins_per_row - y;
			}

			if (bar_y1 < height && bar_y2 > 0 && bar_y2 > bar_y1) {
				cairo_rectangle (cr, grid_x, bar_y1,
					       E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);
	
				cairo_fill (cr);
			}
		}
		g_object_unref (comp);
	}
	cairo_restore (cr);
	cairo_destroy (cr);
}


static void
e_day_view_main_item_draw_day_events (EDayViewMainItem *dvmitem,
				      GdkDrawable *drawable,
				      int x, int y, int width, int height,
				      gint day)
{
	EDayView *day_view;
	gint event_num;

	day_view = dvmitem->day_view;

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		e_day_view_main_item_draw_day_event (dvmitem, drawable,
						     x, y, width, height,
						     day, event_num);
	}
}

static void
e_day_view_main_item_draw_day_event (EDayViewMainItem *dvmitem,
				     GdkDrawable *drawable,
				     int x, int y, int width, int height,
				     gint day, gint event_num)
{
	EDayView *day_view;
	EDayViewEvent *event;
	gint item_x, item_y, item_w, item_h, bar_y1, bar_y2;
	GdkGC *gc;
	GdkColor bg_color;
	ECalComponent *comp;
	gint num_icons, icon_x, icon_y, icon_x_inc, icon_y_inc;
	gint max_icon_w, max_icon_h;
	gboolean draw_reminder_icon, draw_recurrence_icon, draw_timezone_icon, draw_meeting_icon;
	gboolean draw_attach_icon;
	GSList *categories_list, *elem;
	ECalComponentTransparency transparency;
	cairo_t *cr;
	cairo_pattern_t *pat;
	guint16 red, green, blue;
	gint i;
	gdouble radius, x0, y0, rect_height, rect_width;
	gfloat alpha;
	gboolean gradient;
	gdouble cc = 65535.0;
	gdouble date_fraction;
	gboolean short_event, resize_flag = FALSE;
	gchar *end_time, *end_suffix;
	gint end_hour, end_display_hour, end_minute, end_suffix_width;
	int scroll_flag = 0;
	gint row_y;
	GConfClient *gconf;

	day_view = dvmitem->day_view;

	cr = gdk_cairo_create (drawable);
	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	gc = day_view->main_gc;

	gconf = gconf_client_get_default ();

	alpha = gconf_client_get_float (gconf,
				         "/apps/evolution/calendar/display/events_transparency",
					 NULL);

	gradient = gconf_client_get_bool (gconf,
					"/apps/evolution/calendar/display/events_gradient",
					NULL);

	g_object_unref (gconf);

	/* If the event is currently being dragged, don't draw it. It will
	   be drawn in the special drag items. */
	if (day_view->drag_event_day == day
	    && day_view->drag_event_num == event_num)
		return;

	/* Get the position of the event. If it is not shown skip it.*/
	if (!e_day_view_get_event_position (day_view, day, event_num,	
					    &item_x, &item_y,
					    &item_w, &item_h))
		return;

	item_x -= x;
	item_y -= y;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* Fill in the event background. Note that for events in the first
	   column of the day, we might not want to paint over the vertical bar,
	   since that is used for multiple events. But then you can't see
	   where the event in the first column finishes. The border is drawn
           along with the event using cairo*/

	red = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].red;
	green = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].green;
	blue = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].blue;

	if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data),
			     &bg_color)) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
		if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE)) {
			red = bg_color.red;
			green = bg_color.green;
			blue = bg_color.blue;
			}
	}

	/* Draw shadow around the event when selected */
	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num  && (GTK_WIDGET_HAS_FOCUS (day_view->main_canvas)))
	{
		/* For embossing Item selection */
		item_x -= 1;
		item_y -= 2;

		/* Vertical Line */
		cairo_save (cr);
		pat = cairo_pattern_create_linear (item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 6.5, item_y + 13.75,
							item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 13.75, item_y + 13.75);
		cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
		cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
		cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
		cairo_set_source (cr, pat);
		cairo_rectangle (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 6.5, item_y + 14.75, 7.0, item_h - 22.0);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

		/* Arc at the right */		
		pat = cairo_pattern_create_radial (item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 3, item_y + 13.5, 5.0,
					item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 13.5, 12.0);
		cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
		cairo_pattern_add_color_stop_rgba (pat, 0.25, 0, 0, 0, 0.2);
		cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
		cairo_set_source (cr, pat);
		cairo_arc (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 13.5, 8.0, 11 * M_PI / 8, M_PI / 8);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_set_line_width (cr, 1.25);
		cairo_move_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 9.5);
		cairo_line_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 9.5, item_y + 15);
		cairo_stroke (cr);

		/* Horizontal line */
		pat = cairo_pattern_create_linear (item_x + E_DAY_VIEW_BAR_WIDTH + 15, item_y + item_h,
							item_x + E_DAY_VIEW_BAR_WIDTH + 15, item_y + item_h + 7);
		cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
		cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
		cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
		cairo_set_source (cr, pat);
		cairo_rectangle (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 16.5, item_y + item_h, item_w - 31.5, 7.0);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

		/* Bottom arc */
		pat = cairo_pattern_create_radial (item_x + E_DAY_VIEW_BAR_WIDTH + 12.5, item_y + item_h - 5, 5.0,
					item_x + E_DAY_VIEW_BAR_WIDTH + 12.5, item_y + item_h - 5, 12.0);
		cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
		cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
		cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
		cairo_set_source (cr, pat);
		cairo_arc (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 13, item_y + item_h - 5, 12.0, 3 * M_PI / 8, 9 * M_PI / 8);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

		cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
		cairo_set_line_width (cr, 2);
		cairo_move_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 14, item_y + item_h + 2);
		cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 15.5, item_y + item_h + 3);
		cairo_stroke (cr);
		cairo_set_source_rgba (cr, 0, 0, 0, 0.27);
		cairo_move_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 15, item_y + item_h + 3.5);
		cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 17, item_y + item_h + 3.5);
		cairo_stroke (cr);

		/* Arc in middle */
		pat = cairo_pattern_create_radial (item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 1.0,
					item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 12.0);
		cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
		cairo_pattern_add_color_stop_rgba (pat, 0.8, 0, 0, 0, 0.2);
		cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
		cairo_set_source (cr, pat);
		cairo_arc (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 12.0, 15 * M_PI / 8,  5 * M_PI / 8);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

		cairo_set_source_rgba (cr, 0, 0, 0, 0.27);
		cairo_move_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH - 1, item_y + item_h + 3);
		cairo_line_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH , item_y + item_h + 3);
		cairo_stroke (cr);

		cairo_set_source_rgba (cr, 0, 0, 0, 0.27);
		cairo_move_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 9, item_y + item_h - 6);
		cairo_line_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 10, item_y + item_h - 6);
		cairo_stroke (cr);


		cairo_restore (cr);
	
		/* Black border */
		cairo_save (cr);
		x0	   = item_x + E_DAY_VIEW_BAR_WIDTH + 9; 
		y0	   = item_y + 10;
		rect_width = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 7, 0);
		rect_height = item_h - 7;

		radius = 20; 	

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_fill (cr);
		cairo_restore (cr);

		/* Extra Grid lines when clicked */
		cairo_save (cr);

		x0	   = item_x + E_DAY_VIEW_BAR_WIDTH + 1; 
		y0	   = item_y + 2;
		rect_width  = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 3, 0);
		rect_height = item_h - 4.;

		radius = 16; 	

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_fill (cr);

		gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);

		for (row_y = y0;
		     row_y < rect_height + y0;
		     row_y += day_view->row_height) {
			if (row_y >= 0 && row_y < rect_height + y0) {
				cairo_set_line_width (cr, 0.7);
				cairo_move_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 1 , row_y);
				cairo_line_to (cr, item_x + item_w -2, row_y);
				cairo_stroke (cr);
			}
		}
		cairo_restore (cr);
	}
	
	/* Draw the background of the event with white to play with transparency */
	cairo_save (cr);

	x0	   = item_x + E_DAY_VIEW_BAR_WIDTH + 1; 
	y0	   = item_y + 2;
	rect_width  = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 3, 0);
	rect_height = item_h - 4.;

	radius = 16; 	

	draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

	cairo_set_source_rgba (cr, 1, 1, 1, alpha);
	cairo_fill (cr);

	cairo_restore (cr);

	/* Here we draw the border in event color */
	cairo_save (cr);

	x0	   = item_x + E_DAY_VIEW_BAR_WIDTH; 
	y0	   = item_y + 1.;
	rect_width  = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 1., 0);
	rect_height = item_h - 2.;

	radius = 16; 	

	draw_curved_rectangle (cr, x0, y0, rect_width,rect_height, radius);
	cairo_set_line_width (cr, 2.);
	cairo_set_source_rgb (cr, red/cc, green/cc, blue/cc);
	cairo_stroke (cr);
	cairo_restore (cr);

	/* Fill in the Event */

	cairo_save (cr);

	x0	   = item_x + E_DAY_VIEW_BAR_WIDTH + 1.75;
	y0	   = item_y + 2.75;
	rect_width  = item_w - E_DAY_VIEW_BAR_WIDTH - 4.5;
	rect_height = item_h - 5.5;

	radius = 14; 	
	
	draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

	date_fraction = rect_height / day_view->row_height;
	short_event = ((((event->end_minute - event->start_minute)/day_view->mins_per_row) >= 2) 
				|| (((event->end_minute - event->start_minute)%day_view->mins_per_row) <= day_view->mins_per_row))? FALSE : TRUE ;	

	if (day_view->editing_event_day == day
	    && day_view->editing_event_num == event_num) 
		short_event = TRUE;

	if (gradient) {
		pat = cairo_pattern_create_linear (item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + 7.75,
							item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + item_h - 7.75);
		if (!short_event) {
			cairo_pattern_add_color_stop_rgba (pat, 1, red/cc, green/cc, blue/cc, 0.8);
			cairo_pattern_add_color_stop_rgba (pat, 1/(date_fraction + (rect_height/18)), red/cc, green/cc, blue/cc, 0.8);
			cairo_pattern_add_color_stop_rgba (pat, 1/(date_fraction + (rect_height/18)), red/cc, green/cc, blue/cc, 0.4);
			cairo_pattern_add_color_stop_rgba (pat, 1, red/cc, green/cc, blue/cc, 0.8);
		} else {
			cairo_pattern_add_color_stop_rgba (pat, 1, red/cc, green/cc, blue/cc, 0.8);
			cairo_pattern_add_color_stop_rgba (pat, 0, red/cc, green/cc, blue/cc, 0.4);
		}
		cairo_set_source (cr, pat);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pat);
	} else {
		cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0.8);
		cairo_fill_preserve (cr);
	}

	cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0.2);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke (cr);
	cairo_restore (cr);
	
	/* Draw the right edge of the vertical bar. */
	cairo_save (cr);
	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
	cairo_set_line_width (cr, 0.7);
	cairo_move_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, item_y + 1);
	cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, item_y + item_h - 2);
	cairo_stroke (cr);
	cairo_restore (cr);

	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Draw the vertical colored bar showing when the appointment
	   begins & ends. */
	bar_y1 = event->start_minute * day_view->row_height / day_view->mins_per_row - y;
	bar_y2 = event->end_minute * day_view->row_height / day_view->mins_per_row - y;

	scroll_flag = bar_y2;

	/* When an item is being resized, we fill the bar up to the new row. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		resize_flag = TRUE;		

		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE)
			bar_y1 = item_y + 1;

		else if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_BOTTOM_EDGE) {
			bar_y2 = item_y + item_h - 1;

			end_minute = event->end_minute;
				
			end_hour   = end_minute / 60;
			end_minute = end_minute % 60;

			e_day_view_convert_time_to_display (day_view, end_hour,
							    &end_display_hour,
							    &end_suffix,
							    &end_suffix_width);
			
			cairo_save (cr);
			if (e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (day_view))) {
				cairo_translate (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH - 32, item_y + item_h - 8);
				end_time = g_strdup_printf ("%2i:%02i",
					 end_display_hour, end_minute);
					 
			} else {
				cairo_translate (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH - 48, item_y + item_h - 8);
				end_time = g_strdup_printf ("%2i:%02i%s",
						 end_display_hour, end_minute,
						 end_suffix);
			}

			cairo_set_font_size (cr, 14);
			if ((red/cc > 0.7) || (green/cc > 0.7) || (blue/cc > 0.7 ))
				cairo_set_source_rgb (cr, 0, 0, 0);
			else
				cairo_set_source_rgb (cr, 1, 1, 1);
			cairo_show_text (cr, end_time);
			cairo_restore (cr);
		}
	}

	if (bar_y2 > scroll_flag){
		event->end_minute += day_view->mins_per_row;
	} else if (bar_y2 < scroll_flag) {
		event->end_minute -= day_view->mins_per_row;
	}

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	/* Only fill it in if the event isn't TRANSPARENT. */
	e_cal_component_get_transparency (comp, &transparency);
	if (transparency != E_CAL_COMPONENT_TRANSP_TRANSPARENT) {
		cairo_save (cr);
		pat = cairo_pattern_create_linear (item_x + E_DAY_VIEW_BAR_WIDTH, item_y + 1,
						item_x + E_DAY_VIEW_BAR_WIDTH, item_y + item_h - 1);
		cairo_pattern_add_color_stop_rgba (pat, 1, red/cc, green/cc, blue/cc, 0.7);
		cairo_pattern_add_color_stop_rgba (pat, 0.5, red/cc, green/cc, blue/cc, 0.7);
		cairo_pattern_add_color_stop_rgba (pat, 0, red/cc, green/cc, blue/cc, 0.2);

		cairo_rectangle (cr, item_x + 1, bar_y1,
			       E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);

		cairo_set_source (cr, pat);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);
		cairo_restore (cr);

		/* This is for achieving the white stripes in vbar across event color */
		for (i = 0; i <= (bar_y2 - bar_y1) ; i+=4) {
			cairo_set_source_rgb (cr, 1, 1, 1);
			cairo_set_line_width (cr, 0.3);
			cairo_move_to (cr, item_x + 1, bar_y1 + i);
			cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, bar_y1 + i);
			cairo_stroke (cr);
		}
	}

	gdk_cairo_set_source_color (cr, 
			&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Draw the reminder & recurrence icons, if needed. */
	if (!resize_flag) {
		num_icons = 0;
		draw_reminder_icon = FALSE;
		draw_recurrence_icon = FALSE;
		draw_timezone_icon = FALSE;
		draw_meeting_icon = FALSE;
		draw_attach_icon = FALSE;
		icon_x = item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_ICON_X_PAD;
		icon_y = item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
			+ E_DAY_VIEW_ICON_Y_PAD;

		if (e_cal_component_has_alarms (comp)) {
			draw_reminder_icon = TRUE;
			num_icons++;
		}

		if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp)) {
			draw_recurrence_icon = TRUE;
			num_icons++;
		}
		if (e_cal_component_has_attachments (comp)) {
			draw_attach_icon = TRUE;
			num_icons++;
		}
		/* If the DTSTART or DTEND are in a different timezone to our current
		   timezone, we display the timezone icon. */
		if (event->different_timezone) {
			draw_timezone_icon = TRUE;
			num_icons++;
		}

		if (e_cal_component_has_organizer (comp)) {
			draw_meeting_icon = TRUE;
			num_icons++;
		}

		e_cal_component_get_categories_list (comp, &categories_list);
		for (elem = categories_list; elem; elem = elem->next) {
			char *category;
			GdkPixmap *pixmap = NULL;
			GdkBitmap *mask = NULL;

			category = (char *) elem->data;
			if (e_categories_config_get_icon_for (category, &pixmap, &mask))
				num_icons++;
		}

		if (num_icons != 0) {
			if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD)
			    * num_icons) {
				icon_x_inc = 0;
				icon_y_inc = E_DAY_VIEW_ICON_HEIGHT
					+ E_DAY_VIEW_ICON_Y_PAD;
			} else {
				icon_x_inc = E_DAY_VIEW_ICON_WIDTH
					+ E_DAY_VIEW_ICON_X_PAD;
				icon_y_inc = 0;
			}

			if (draw_reminder_icon) {
				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

				cairo_save (cr);
				gdk_cairo_set_source_pixbuf (cr, day_view->reminder_icon, icon_x, icon_y);
				cairo_paint (cr);
				cairo_restore (cr);
	
				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}	

			if (draw_recurrence_icon) {
				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;
			
				cairo_save (cr);
				gdk_cairo_set_source_pixbuf (cr, day_view->recurrence_icon, icon_x, icon_y);
				cairo_paint (cr);
				cairo_restore (cr);
	
				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}
			if (draw_attach_icon) {
				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

				cairo_save (cr);
				gdk_cairo_set_source_pixbuf (cr, day_view->attach_icon, icon_x, icon_y);
				cairo_paint (cr);
				cairo_restore (cr);
				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}
			if (draw_timezone_icon) {
				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

				cairo_save (cr);
				gdk_cairo_set_source_pixbuf (cr, day_view->timezone_icon, icon_x, icon_y);
				cairo_paint (cr);
				cairo_restore (cr);
			
				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}


			if (draw_meeting_icon) {
				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

				cairo_save (cr);
				gdk_cairo_set_source_pixbuf (cr, day_view->meeting_icon, icon_x, icon_y);
				cairo_paint (cr);
				cairo_restore (cr);
	
				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}

			/* draw categories icons */
			for (elem = categories_list; elem; elem = elem->next) {
				char *category;
				GdkPixmap *pixmap = NULL;
				GdkBitmap *mask = NULL;

				category = (char *) elem->data;
				if (!e_categories_config_get_icon_for (category, &pixmap, &mask))
					continue;

				max_icon_w = item_x + item_w - icon_x
					- E_DAY_VIEW_EVENT_BORDER_WIDTH;
				max_icon_h = item_y + item_h - icon_y
					- E_DAY_VIEW_EVENT_BORDER_HEIGHT;

				gdk_gc_set_clip_origin (gc, icon_x, icon_y);
				if (mask != NULL)
					gdk_gc_set_clip_mask (gc, mask);
				gdk_draw_pixmap (drawable, gc,
						 pixmap,
						 0, 0, icon_x, icon_y,
						 MIN (E_DAY_VIEW_ICON_WIDTH,
						      max_icon_w),
						 MIN (E_DAY_VIEW_ICON_HEIGHT,
						      max_icon_h));

				gdk_pixmap_unref (pixmap);
				if (mask != NULL)
					gdk_bitmap_unref (mask);

				icon_x += icon_x_inc;
				icon_y += icon_y_inc;
			}
		
			gdk_gc_set_clip_mask (gc, NULL);
		}

	/* free memory */
	e_cal_component_free_categories_list (categories_list);
	}

	g_object_unref (comp);
	cairo_destroy (cr);
}
#endif

/* This is supposed to return the nearest item to the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_day_view_main_item_point (GnomeCanvasItem *item, double x, double y,
			    int cx, int cy,
			    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


static gint
e_day_view_main_item_event (GnomeCanvasItem *item, GdkEvent *event)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:

	case GDK_BUTTON_RELEASE:

	case GDK_MOTION_NOTIFY:

	default:
		break;
	}

	return FALSE;
}



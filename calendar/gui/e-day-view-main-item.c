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

/*
 * EDayViewMainItem - canvas item which displays most of the appointment
 * data in the main Day/Work Week display.
 */

#include "e-day-view-main-item.h"

static void e_day_view_main_item_class_init (EDayViewMainItemClass *class);
static void e_day_view_main_item_init (EDayViewMainItem *dvtitem);

static void e_day_view_main_item_set_arg (GtkObject *o, GtkArg *arg,
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

static GnomeCanvasItemClass *parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_DAY_VIEW
};


GtkType
e_day_view_main_item_get_type (void)
{
	static GtkType e_day_view_main_item_type = 0;

	if (!e_day_view_main_item_type) {
		GtkTypeInfo e_day_view_main_item_info = {
			"EDayViewMainItem",
			sizeof (EDayViewMainItem),
			sizeof (EDayViewMainItemClass),
			(GtkClassInitFunc) e_day_view_main_item_class_init,
			(GtkObjectInitFunc) e_day_view_main_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_day_view_main_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &e_day_view_main_item_info);
	}

	return e_day_view_main_item_type;
}


static void
e_day_view_main_item_class_init (EDayViewMainItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type());

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
}


static void
e_day_view_main_item_init (EDayViewMainItem *dvtitem)
{
	dvtitem->day_view = NULL;
}


static void
e_day_view_main_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EDayViewMainItem *dvmitem;

	item = GNOME_CANVAS_ITEM (o);
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
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, clip_path, flags);

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
e_day_view_main_item_draw (GnomeCanvasItem *canvas_item, GdkDrawable *drawable,
			   int x, int y, int width, int height)
{
	EDayViewMainItem *dvmitem;
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *fg_gc, *bg_gc, *light_gc, *dark_gc, *gc;
	GdkFont *font;
	gint row, row_y, grid_x1, grid_x2;
	gint day, grid_y1, grid_y2;
	gint work_day_start_row, work_day_end_row;
	gint work_day_start_y, work_day_end_y;
	gint work_day_x, work_day_w;
	gint start_row, end_row, rect_x, rect_y, rect_width, rect_height;

#if 0
	g_print ("In e_day_view_main_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif
	dvmitem = E_DAY_VIEW_MAIN_ITEM (canvas_item);
	day_view = dvmitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = GTK_WIDGET (day_view)->style;
	font = style->font;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	bg_gc = style->bg_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];

	/* Paint the background colors. */
	gc = day_view->main_gc;
	work_day_start_row = e_day_view_convert_time_to_row (day_view, day_view->work_day_start_hour, day_view->work_day_start_minute);
	work_day_start_y = work_day_start_row * day_view->row_height - y;
	work_day_end_row = e_day_view_convert_time_to_row (day_view, day_view->work_day_end_hour, day_view->work_day_end_minute);
	work_day_end_y = work_day_end_row * day_view->row_height - y;

	work_day_x = day_view->day_offsets[0] - x;
	work_day_w = width - work_day_x;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    work_day_x, 0 - y,
			    work_day_w, work_day_start_y - (0 - y));
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    work_day_x, work_day_start_y,
			    work_day_w, work_day_end_y - work_day_start_y);
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    work_day_x, work_day_end_y,
			    work_day_w, height - work_day_end_y);

	/* Paint the selection background. */
	if (day_view->selection_start_col != -1
	    && !day_view->selection_in_top_canvas) {
		for (day = day_view->selection_start_col;
		     day <= day_view->selection_end_col;
		     day++) {
			if (day == day_view->selection_start_col
			    && day_view->selection_start_row != -1)
				start_row = day_view->selection_start_row;
			else
				start_row = 0;
			if (day == day_view->selection_end_col
			    && day_view->selection_end_row != -1)
				end_row = day_view->selection_end_row;
			else
				end_row = day_view->rows - 1;

			rect_x = day_view->day_offsets[day] - x;
			rect_width = day_view->day_widths[day];
			rect_y = start_row * day_view->row_height - y;
			rect_height = (end_row - start_row + 1) * day_view->row_height;

			gc = style->bg_gc[GTK_STATE_SELECTED];
			gdk_draw_rectangle (drawable, gc, TRUE,
					    rect_x, rect_y,
					    rect_width, rect_height);
		}
	}

	/* Drawing the horizontal grid lines. */
	grid_x1 = day_view->day_offsets[0] - x;
	grid_x2 = day_view->day_offsets[day_view->days_shown] - x;

	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		if (row_y >= 0 && row_y < height)
			gdk_draw_line (drawable, dark_gc,
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

		gdk_draw_line (drawable, fg_gc,
			       grid_x1, grid_y1,
			       grid_x1, grid_y2);
		gdk_draw_line (drawable, fg_gc,
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

	day_view = dvmitem->day_view;

	gc = day_view->main_gc;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	grid_x = day_view->day_offsets[day] + 1 - x;

	/* Draw the busy times corresponding to the events in the day. */
	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent,
					event_num);

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

	day_view = dvmitem->day_view;

	gc = day_view->main_gc;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		event = &g_array_index (day_view->long_events, EDayViewEvent,
					event_num);

		if (!e_day_view_find_long_event_days (day_view, event,
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
	GtkStyle *style;
	GdkGC *gc;
	iCalObject *ico;
	gint num_icons, icon_x, icon_y, icon_x_inc, icon_y_inc;
	gboolean draw_reminder_icon, draw_recurrence_icon;

	day_view = dvmitem->day_view;

	/* If the event is currently being dragged, don't draw it. It will
	   be drawn in the special drag items. */
	if (day_view->drag_event_day == day
	    && day_view->drag_event_num == event_num)
		return;

	style = GTK_WIDGET (day_view)->style;

	gc = day_view->main_gc;
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Get the position of the event. If it is not shown skip it.*/
	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h))
		return;

	item_x -= x;
	item_y -= y;

	event = &g_array_index (day_view->events[day], EDayViewEvent,
				event_num);

	/* Fill in the white background. Note that for events in the first
	   column of the day, we might not want topaint over the vertical bar,
	   since that is used for multiple events. But then you can't see
	   where the event in the first column finishes. */
#if 0
	if (event->start_row_or_col == 0)
		gdk_draw_rectangle (drawable, style->white_gc, TRUE,
				    item_x + E_DAY_VIEW_BAR_WIDTH, item_y + 1,
				    item_w - E_DAY_VIEW_BAR_WIDTH - 1,
				    item_h - 2);
	else
#endif
		gdk_draw_rectangle (drawable, style->white_gc, TRUE,
				    item_x + 1, item_y + 1,
				    item_w - 2, item_h - 2);

	/* Draw the right edge of the vertical bar. */
	gdk_draw_line (drawable, style->black_gc,
		       item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		       item_y + 1,
		       item_x + E_DAY_VIEW_BAR_WIDTH - 1,
		       item_y + item_h - 2);

	/* Draw the vertical colored bar showing when the appointment
	   begins & ends. */
	bar_y1 = event->start_minute * day_view->row_height / day_view->mins_per_row - y;
	bar_y2 = event->end_minute * day_view->row_height / day_view->mins_per_row - y;

	/* When an item is being resized, we fill the bar up to the new row. */
	if (day_view->resize_drag_pos != E_DAY_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_DAY_VIEW_POS_TOP_EDGE)
			bar_y1 = item_y + 1;
		else if (day_view->resize_drag_pos == E_DAY_VIEW_POS_BOTTOM_EDGE)
			bar_y2 = item_y + item_h - 1;
	}

	gdk_draw_rectangle (drawable, gc, TRUE,
			    item_x + 1, bar_y1,
			    E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);

	/* Draw the box around the entire event. Do this after drawing
	   the colored bar so we don't have to worry about being 1
	   pixel out. */
	gdk_draw_rectangle (drawable, style->black_gc, FALSE,
			    item_x, item_y, item_w - 1, item_h - 1);

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
	icon_x = item_x + E_DAY_VIEW_BAR_WIDTH + E_DAY_VIEW_ICON_X_PAD;
	icon_y = item_y + E_DAY_VIEW_EVENT_BORDER_HEIGHT
		+ E_DAY_VIEW_ICON_Y_PAD;
	ico = event->ico;

	if (ico->dalarm.enabled || ico->malarm.enabled
	    || ico->palarm.enabled || ico->aalarm.enabled) {
		draw_reminder_icon = TRUE;
		num_icons++;
	}

	if (ico->recur) {
		draw_recurrence_icon = TRUE;
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
			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			gdk_gc_set_clip_mask (gc, day_view->reminder_mask);
			gdk_draw_pixmap (drawable, gc,
					 day_view->reminder_icon,
					 0, 0, icon_x, icon_y,
					 E_DAY_VIEW_ICON_WIDTH,
					 E_DAY_VIEW_ICON_HEIGHT);
			icon_x += icon_x_inc;
			icon_y += icon_y_inc;
		}

		if (draw_recurrence_icon) {
			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			gdk_gc_set_clip_mask (gc, day_view->recurrence_mask);
			gdk_draw_pixmap (drawable, gc,
					 day_view->recurrence_icon,
					 0, 0, icon_x, icon_y,
					 E_DAY_VIEW_ICON_WIDTH,
					 E_DAY_VIEW_ICON_HEIGHT);
		}
		gdk_gc_set_clip_mask (gc, NULL);
	}
}


/* This is supposed to return the nearest item the the point and the distance.
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
	EDayViewMainItem *dvtitem;

	dvtitem = E_DAY_VIEW_MAIN_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:

	case GDK_BUTTON_RELEASE:

	case GDK_MOTION_NOTIFY:

	default:
		break;
	}

	return FALSE;
}



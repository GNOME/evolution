/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
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
 * EDayViewTopItem - displays the top part of the Day/Work Week calendar view.
 */

#include <config.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "e-util/e-categories-config.h"
#include <libecal/e-cal-time-util.h>
#include "e-day-view-top-item.h"

static void e_day_view_top_item_class_init	(EDayViewTopItemClass *class);
static void e_day_view_top_item_init		(EDayViewTopItem *dvtitem);

static void e_day_view_top_item_set_arg		(GtkObject	 *o,
						 GtkArg		 *arg,
						 guint		  arg_id);
static void e_day_view_top_item_update		(GnomeCanvasItem *item,
						 double		 *affine,
						 ArtSVP		 *clip_path,
						 int		  flags);
static void e_day_view_top_item_draw		(GnomeCanvasItem *item,
						 GdkDrawable	 *drawable,
						 int		  x,
						 int		  y,
						 int		  width,
						 int		  height);
static void e_day_view_top_item_draw_long_event	(EDayViewTopItem *dvtitem,
						 gint		  event_num,
						 GdkDrawable	 *drawable,
						 int		  x,
						 int		  y,
						 int		  width,
						 int		  height);
static void e_day_view_top_item_draw_triangle	(EDayViewTopItem *dvtitem,
						 GdkDrawable	 *drawable,
						 gint		  x,
						 gint		  y,
						 gint		  w,
						 gint		  h);
static double e_day_view_top_item_point		(GnomeCanvasItem *item,
						 double		  x,
						 double		  y,
						 int		  cx,
						 int		  cy,
						 GnomeCanvasItem **actual_item);
static gint e_day_view_top_item_event		(GnomeCanvasItem *item,
						 GdkEvent	 *event);


static GnomeCanvasItemClass *parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_DAY_VIEW
};

E_MAKE_TYPE (e_day_view_top_item, "EDayViewTopItem", EDayViewTopItem, e_day_view_top_item_class_init,
	     e_day_view_top_item_init, GNOME_TYPE_CANVAS_ITEM);

static void
e_day_view_top_item_class_init (EDayViewTopItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EDayViewTopItem::day_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_DAY_VIEW);

	object_class->set_arg = e_day_view_top_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_day_view_top_item_update;
	item_class->draw        = e_day_view_top_item_draw;
	item_class->point       = e_day_view_top_item_point;
	item_class->event       = e_day_view_top_item_event;
}


static void
e_day_view_top_item_init (EDayViewTopItem *dvtitem)
{
	dvtitem->day_view = NULL;
}


static void
e_day_view_top_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EDayViewTopItem *dvtitem;

	item = GNOME_CANVAS_ITEM (o);
	dvtitem = E_DAY_VIEW_TOP_ITEM (o);
	
	switch (arg_id){
	case ARG_DAY_VIEW:
		dvtitem->day_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_day_view_top_item_update (GnomeCanvasItem *item,
			    double	    *affine,
			    ArtSVP	    *clip_path,
			    int		     flags)
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
e_day_view_top_item_draw (GnomeCanvasItem *canvas_item,
			  GdkDrawable	  *drawable,
			  int		   x,
			  int		   y,
			  int		   width,
			  int		   height)
{
	EDayViewTopItem *dvtitem;
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *gc, *fg_gc, *bg_gc, *light_gc, *dark_gc;
	gchar buffer[128];
	GdkRectangle clip_rect;
	gint canvas_width, canvas_height, left_edge, day, date_width, date_x;
	gint item_height, event_num;
	PangoLayout *layout;

#if 0
	g_print ("In e_day_view_top_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif
	dvtitem = E_DAY_VIEW_TOP_ITEM (canvas_item);
	day_view = dvtitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	gc = day_view->main_gc;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	bg_gc = style->bg_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	canvas_width = GTK_WIDGET (canvas_item->canvas)->allocation.width;
	canvas_height = GTK_WIDGET (canvas_item->canvas)->allocation.height;
	left_edge = 0;
	item_height = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;

	/* Draw the shadow around the dates. */
	gdk_draw_line (drawable, light_gc,
		       left_edge - x, 1 - y,
		       canvas_width - 2 - x, 1 - y);
	gdk_draw_line (drawable, light_gc,
		       left_edge - x, 2 - y,
		       left_edge - x, item_height - 2 - y);
	gdk_draw_line (drawable, dark_gc,
		       left_edge - x, item_height - 1 - y,
		       canvas_width - 1 - x, item_height - 1 - y);
	gdk_draw_line (drawable, dark_gc,
		       canvas_width - 1 - x, 1 - y,
		       canvas_width - 1 - x, item_height - 1 - y);

	/* Draw the background for the dates. */
	gdk_draw_rectangle (drawable, bg_gc, TRUE,
			    left_edge + 2 - x, 2 - y,
			    canvas_width - left_edge - 3,
			    item_height - 3);

	/* Clear the main area background. */
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS]);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    left_edge - x, item_height - y,
			    canvas_width - left_edge,
			    canvas_height - item_height);

	/* Draw the selection background. */
	if (GTK_WIDGET_HAS_FOCUS (day_view)
	    && day_view->selection_start_day != -1) {
		gint start_col, end_col, rect_x, rect_y, rect_w, rect_h;

		start_col = day_view->selection_start_day;
		end_col = day_view->selection_end_day;

		if (end_col > start_col
		    || day_view->selection_start_row == -1
		    || day_view->selection_end_row == -1) {
			rect_x = day_view->day_offsets[start_col];
			rect_y = item_height;
			rect_w = day_view->day_offsets[end_col + 1] - rect_x;
			rect_h = canvas_height - 1 - rect_y;

			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_SELECTED]);
			gdk_draw_rectangle (drawable, gc, TRUE,
					    rect_x - x, rect_y - y,
					    rect_w, rect_h);
		}
	}

	/* Draw the date. Set a clipping rectangle so we don't draw over the
	   next day. */
	for (day = 0; day < day_view->days_shown; day++) {
		e_day_view_top_item_get_day_label (day_view, day,
						   buffer, sizeof (buffer));
		clip_rect.x = day_view->day_offsets[day] - x;
		clip_rect.y = 2 - y;
		clip_rect.width = day_view->day_widths[day];
		clip_rect.height = item_height - 2;
		gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
		pango_layout_get_pixel_size (layout, &date_width, NULL);
		date_x = day_view->day_offsets[day] + (day_view->day_widths[day] - date_width) / 2;

		gdk_draw_layout (drawable, fg_gc,
				 date_x - x,
				 3 - y,
				 layout);
		g_object_unref (layout);

		gdk_gc_set_clip_rectangle (fg_gc, NULL);

		/* Draw the lines down the left and right of the date cols. */
		if (day != 0) {
			gdk_draw_line (drawable, light_gc,
				       day_view->day_offsets[day] - x,
				       4 - y,
				       day_view->day_offsets[day] - x,
				       item_height - 4 - y);

			gdk_draw_line (drawable, dark_gc,
				       day_view->day_offsets[day] - 1 - x,
				       4 - y,
				       day_view->day_offsets[day] - 1 - x,
				       item_height - 4 - y);
		}

		/* Draw the lines between each column. */
		if (day != 0) {
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_GRID]);
			gdk_draw_line (drawable, gc,
				       day_view->day_offsets[day] - x,
				       item_height - y,
				       day_view->day_offsets[day] - x,
				       canvas_height - y);
		}
	}

	/* Draw the long events. */
	for (event_num = 0; event_num < day_view->long_events->len;
	     event_num++) {
		e_day_view_top_item_draw_long_event (dvtitem, event_num,
						     drawable,
						     x, y, width, height);
	}
}


/* This draws one event in the top canvas. */
static void
e_day_view_top_item_draw_long_event (EDayViewTopItem *dvtitem,
				     gint	      event_num,
				     GdkDrawable     *drawable,
				     int	      x,
				     int	      y,
				     int	      width,
				     int	      height)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GtkStyle *style;
	GdkGC *gc, *fg_gc, *bg_gc;
	gint start_day, end_day;
	gint item_x, item_y, item_w, item_h;
	gint text_x, icon_x, icon_y, icon_x_inc;
	ECalComponent *comp;
	gchar buffer[16];
	gint hour, display_hour, minute, offset, time_width, time_x;
	gint min_end_time_x, suffix_width, max_icon_x;
	gchar *suffix;
	gboolean draw_start_triangle, draw_end_triangle;
	GdkRectangle clip_rect;
	GSList *categories_list, *elem;
	PangoLayout *layout;
	GdkColor bg_color;

	day_view = dvtitem->day_view;

	/* If the event is currently being dragged, don't draw it. It will
	   be drawn in the special drag items. */
	if (day_view->drag_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->drag_event_num == event_num)
		return;

	if (!e_day_view_get_long_event_position (day_view, event_num,
						 &start_day, &end_day,
						 &item_x, &item_y,
						 &item_w, &item_h))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	gc = day_view->main_gc;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	bg_gc = style->bg_gc[GTK_STATE_NORMAL];
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	/* Draw the lines across the top & bottom of the entire event. */
	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
	gdk_draw_line (drawable, gc,
		       item_x - x, item_y - y,
		       item_x + item_w - 1 - x, item_y - y);
	gdk_draw_line (drawable, gc,
		       item_x - x, item_y + item_h - 1 - y,
		       item_x + item_w - 1 - x, item_y + item_h - 1 - y);

	/* Fill it in. */
	if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)),
								  event->comp_data),
			     &bg_color)) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
		if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE))
			gdk_gc_set_foreground (gc, &bg_color);
		else
			gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND]);
	} else
		gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND]);
	gdk_draw_rectangle (drawable, gc, TRUE,
			    item_x - x, item_y + 1 - y,
			    item_w, item_h - 2);

	/* When resizing we don't draw the triangles.*/
	draw_start_triangle = TRUE;
	draw_end_triangle = TRUE;
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->resize_event_num == event_num) {
		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_LEFT_EDGE)
			draw_start_triangle = FALSE;

		if  (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_RIGHT_EDGE)
			draw_end_triangle = FALSE;
	}

	/* If the event starts before the first day shown, draw a triangle,
	   else just draw a vertical line down the left. */
	if (draw_start_triangle
	    && event->start < day_view->day_starts[start_day]) {
		e_day_view_top_item_draw_triangle (dvtitem, drawable,
						   item_x - x, item_y - y,
						   -E_DAY_VIEW_BAR_WIDTH,
						   item_h);
	} else {
		gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
		gdk_draw_line (drawable, gc,
			       item_x - x, item_y - y,
			       item_x - x, item_y + item_h - 1 - y);
	}

	/* Similar for the event end. */
	if (draw_end_triangle
	    && event->end > day_view->day_starts[end_day + 1]) {
		e_day_view_top_item_draw_triangle (dvtitem, drawable,
						   item_x + item_w - 1 - x,
						   item_y - y,
						   E_DAY_VIEW_BAR_WIDTH,
						   item_h);
	} else {
		gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
		gdk_draw_line (drawable, gc,
			       item_x + item_w - 1 - x,
			       item_y - y,
			       item_x + item_w - 1 - x,
			       item_y + item_h - 1 - y);
	}

	/* If we are editing the event we don't show the icons or the start
	   & end times. */
	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num)
		return;

	/* Determine the position of the label, so we know where to place the
	   icons. Note that since the top canvas never scrolls we don't need
	   to take the scroll offset into account. It will always be 0. */
	text_x = event->canvas_item->x1;

	/* Draw the start & end times, if necessary. */
	min_end_time_x = item_x + E_DAY_VIEW_LONG_EVENT_X_PAD - x;

	time_width = e_day_view_get_time_string_width (day_view);

	if (event->start > day_view->day_starts[start_day]) {
		offset = day_view->first_hour_shown * 60
			+ day_view->first_minute_shown + event->start_minute;
		hour = offset / 60;
		minute = offset % 60;
		/* Calculate the actual hour number to display. For 12-hour
		   format we convert 0-23 to 12-11am/12-11pm. */
		e_day_view_convert_time_to_display (day_view, hour,
						    &display_hour,
						    &suffix, &suffix_width);
		if (e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (day_view))) {
			g_snprintf (buffer, sizeof (buffer), "%i:%02i",
				    display_hour, minute);
		} else {
			g_snprintf (buffer, sizeof (buffer), "%i:%02i%s",
				    display_hour, minute, suffix);
		}

		clip_rect.x = item_x - x;
		clip_rect.y = item_y - y;
		clip_rect.width = item_w - E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH;
		clip_rect.height = item_h;
		gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

		time_x = item_x + E_DAY_VIEW_LONG_EVENT_X_PAD - x;
		if (display_hour < 10)
			time_x += day_view->digit_width;

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
		gdk_draw_layout (drawable, fg_gc,
				 time_x,
				 item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT +
				 E_DAY_VIEW_LONG_EVENT_Y_PAD - y,
				 layout);
		g_object_unref (layout);

		gdk_gc_set_clip_rectangle (fg_gc, NULL);

		min_end_time_x += time_width
			+ E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;
	}

	max_icon_x = item_x + item_w - E_DAY_VIEW_LONG_EVENT_X_PAD
		- E_DAY_VIEW_ICON_WIDTH;

	if (event->end < day_view->day_starts[end_day + 1]) {
		offset = day_view->first_hour_shown * 60
			+ day_view->first_minute_shown
			+ event->end_minute;
		hour = offset / 60;
		minute = offset % 60;
		time_x = item_x + item_w - E_DAY_VIEW_LONG_EVENT_X_PAD - time_width - E_DAY_VIEW_LONG_EVENT_TIME_X_PAD - x;

		if (time_x >= min_end_time_x) {
			/* Calculate the actual hour number to display. */
			e_day_view_convert_time_to_display (day_view, hour,
							    &display_hour,
							    &suffix,
							    &suffix_width);
			if (e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (day_view))) {
				g_snprintf (buffer, sizeof (buffer),
					    "%i:%02i", display_hour, minute);
			} else {
				g_snprintf (buffer, sizeof (buffer),
					    "%i:%02i%s", display_hour, minute,
					    suffix);
			}

			if (display_hour < 10)
				time_x += day_view->digit_width;

			layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
			gdk_draw_layout (drawable, fg_gc,
					 time_x,
					 item_y + E_DAY_VIEW_LONG_EVENT_Y_PAD + 1 - y,
					 layout);
			g_object_unref (layout);

			max_icon_x -= time_width + E_DAY_VIEW_LONG_EVENT_TIME_X_PAD;
		}
	}

	/* Draw the icons. */
	icon_x_inc = E_DAY_VIEW_ICON_WIDTH + E_DAY_VIEW_ICON_X_PAD;
	icon_x = text_x - E_DAY_VIEW_LONG_EVENT_ICON_R_PAD
		- icon_x_inc - x;
	icon_y = item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT
		+ E_DAY_VIEW_ICON_Y_PAD - y;

	if (icon_x <= max_icon_x && e_cal_component_has_recurrences (comp)) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_draw_pixbuf (drawable, gc,
				 day_view->recurrence_icon,
				 0, 0, icon_x, icon_y,
				 E_DAY_VIEW_ICON_WIDTH,
				 E_DAY_VIEW_ICON_HEIGHT,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
		icon_x -= icon_x_inc;
	}

	if (icon_x <= max_icon_x && e_cal_component_has_alarms (comp)) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_draw_pixbuf (drawable, gc,
				 day_view->reminder_icon,
				 0, 0, icon_x, icon_y,
				 E_DAY_VIEW_ICON_WIDTH,
				 E_DAY_VIEW_ICON_HEIGHT,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
		icon_x -= icon_x_inc;
	}

	/* draw categories icons */
	e_cal_component_get_categories_list (comp, &categories_list);
	for (elem = categories_list; elem; elem = elem->next) {
		char *category;
		GdkPixmap *pixmap = NULL;
		GdkBitmap *mask = NULL;

		category = (char *) elem->data;
		e_categories_config_get_icon_for (category, &pixmap, &mask);
		if (pixmap == NULL)
			continue;

		if (icon_x <= max_icon_x) {
			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			if (mask != NULL)
				gdk_gc_set_clip_mask (gc, mask);
			gdk_draw_pixmap (drawable, gc,
					 pixmap,
					 0, 0, icon_x, icon_y,
					 E_DAY_VIEW_ICON_WIDTH,
					 E_DAY_VIEW_ICON_HEIGHT);
			icon_x -= icon_x_inc;
		}

		gdk_pixmap_unref (pixmap);
		if (mask != NULL)
			gdk_bitmap_unref (mask);
	}

	e_cal_component_free_categories_list (categories_list);
	g_object_unref (comp);

	gdk_gc_set_clip_mask (gc, NULL);
}


/* This draws a little triangle to indicate that an event extends past
   the days visible on screen. */
static void
e_day_view_top_item_draw_triangle (EDayViewTopItem *dvtitem,
				   GdkDrawable	   *drawable,
				   gint		    x,
				   gint		    y,
				   gint		    w,
				   gint		    h)
{
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *gc;
	GdkPoint points[3];
	gint c1, c2;

	day_view = dvtitem->day_view;

	style = GTK_WIDGET (day_view)->style;
	gc = day_view->main_gc;

	points[0].x = x;
	points[0].y = y;
	points[1].x = x + w;
	points[1].y = y + (h / 2) - 1;
	points[2].x = x;
	points[2].y = y + h - 1;

	/* If the height is odd we can use the same central point for both
	   lines. If it is even we use different end-points. */
	c1 = c2 = y + (h / 2);
	if (h % 2 == 0)
		c1--;

	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND]);
	gdk_draw_polygon (drawable, gc, TRUE, points, 3);

	gdk_gc_set_foreground (gc, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
	gdk_draw_line (drawable, gc, x, y, x + w, c1);
	gdk_draw_line (drawable, gc, x, y + h - 1, x + w, c2);
}


/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_day_view_top_item_point (GnomeCanvasItem *item, double x, double y,
			   int cx, int cy,
			   GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


static gint
e_day_view_top_item_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EDayViewTopItem *dvtitem;

	dvtitem = E_DAY_VIEW_TOP_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:

	case GDK_BUTTON_RELEASE:

	case GDK_MOTION_NOTIFY:

	default:
		break;
	}

	return FALSE;
}

void
e_day_view_top_item_get_day_label (EDayView *day_view, gint day,
				   gchar *buffer, gint buffer_len)
{
	struct icaltimetype day_start_tt;
	struct tm day_start = { 0 };
	gchar *format;

	day_start_tt = icaltime_from_timet_with_zone (day_view->day_starts[day],
						      FALSE,
						      e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
	day_start.tm_year = day_start_tt.year - 1900;
	day_start.tm_mon = day_start_tt.month - 1;
	day_start.tm_mday = day_start_tt.day;
	day_start.tm_isdst = -1;

	day_start.tm_wday = time_day_of_week (day_start_tt.day,
					      day_start_tt.month - 1,
					      day_start_tt.year);

	if (day_view->date_format == E_DAY_VIEW_DATE_FULL)
		/* strftime format %A = full weekday name, %d = day of month,
		   %B = full month name. Don't use any other specifiers. */
		format = _("%A %d %B");
	else if (day_view->date_format == E_DAY_VIEW_DATE_ABBREVIATED)
		/* strftime format %a = abbreviated weekday name, %d = day of month,
		   %b = abbreviated month name. Don't use any other specifiers. */
		format = _("%a %d %b");
	else if (day_view->date_format == E_DAY_VIEW_DATE_NO_WEEKDAY)
		/* strftime format %d = day of month, %b = abbreviated month name.
		   Don't use any other specifiers. */
		format = _("%d %b");
	else
		format = "%d";

	e_utf8_strftime (buffer, buffer_len, format, &day_start);
}

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
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#include <config.h>
#include "../widgets/e-text/e-text.h"
#include "e-week-view-event-item.h"

static void e_week_view_event_item_class_init	(EWeekViewEventItemClass *class);
static void e_week_view_event_item_init		(EWeekViewEventItem *wveitem);

static void e_week_view_event_item_set_arg	(GtkObject	 *o,
						 GtkArg		 *arg,
						 guint		  arg_id);
static void e_week_view_event_item_update	(GnomeCanvasItem *item,
						 double		 *affine,
						 ArtSVP		 *clip_path,
						 int		  flags);
static void e_week_view_event_item_draw		(GnomeCanvasItem *item,
						 GdkDrawable	 *drawable,
						 int		  x,
						 int		  y,
						 int		  width,
						 int		  height);
static void e_week_view_event_item_draw_icons	(EWeekViewEventItem *wveitem,
						 GdkDrawable        *drawable,
						 gint		     icon_x,
						 gint		     icon_y,
						 gint		     x2,
						 gboolean	     right_align);
static void e_week_view_event_item_draw_triangle (EWeekViewEventItem *wveitem,
						  GdkDrawable	     *drawable,
						  gint		      x,
						  gint		      y,
						  gint		      w,
						  gint		      h);
static double e_week_view_event_item_point	(GnomeCanvasItem *item,
						 double		  x,
						 double		  y,
						 int		  cx,
						 int		  cy,
						 GnomeCanvasItem **actual_item);
static gint e_week_view_event_item_event	(GnomeCanvasItem *item,
						 GdkEvent	 *event);
static gboolean e_week_view_event_item_button_press (EWeekViewEventItem *wveitem,
						     GdkEvent		*event);
static gboolean e_week_view_event_item_button_release (EWeekViewEventItem *wveitem,
						       GdkEvent		  *event);
static EWeekViewPosition e_week_view_event_item_get_position (EWeekViewEventItem *wveitem,
							      gdouble x,
							      gdouble y);


static GnomeCanvasItemClass *parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_EVENT_NUM,
	ARG_SPAN_NUM
};


GtkType
e_week_view_event_item_get_type (void)
{
	static GtkType e_week_view_event_item_type = 0;

	if (!e_week_view_event_item_type) {
		GtkTypeInfo e_week_view_event_item_info = {
			"EWeekViewEventItem",
			sizeof (EWeekViewEventItem),
			sizeof (EWeekViewEventItemClass),
			(GtkClassInitFunc) e_week_view_event_item_class_init,
			(GtkObjectInitFunc) e_week_view_event_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_week_view_event_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &e_week_view_event_item_info);
	}

	return e_week_view_event_item_type;
}


static void
e_week_view_event_item_class_init (EWeekViewEventItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type());

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EWeekViewEventItem::event_num",
				 GTK_TYPE_INT, GTK_ARG_WRITABLE,
				 ARG_EVENT_NUM);
	gtk_object_add_arg_type ("EWeekViewEventItem::span_num",
				 GTK_TYPE_INT, GTK_ARG_WRITABLE,
				 ARG_SPAN_NUM);

	object_class->set_arg = e_week_view_event_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_week_view_event_item_update;
	item_class->draw        = e_week_view_event_item_draw;
	item_class->point       = e_week_view_event_item_point;
	item_class->event       = e_week_view_event_item_event;
}


static void
e_week_view_event_item_init (EWeekViewEventItem *wveitem)
{
	wveitem->event_num = -1;
	wveitem->span_num = -1;
}


static void
e_week_view_event_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EWeekViewEventItem *wveitem;
	gboolean needs_update = FALSE;

	item = GNOME_CANVAS_ITEM (o);
	wveitem = E_WEEK_VIEW_EVENT_ITEM (o);
	
	switch (arg_id){
	case ARG_EVENT_NUM:
		wveitem->event_num = GTK_VALUE_INT (*arg);
		needs_update = TRUE;
		break;
	case ARG_SPAN_NUM:
		wveitem->span_num = GTK_VALUE_INT (*arg);
		needs_update = TRUE;
		break;
	}

	if (needs_update)
		gnome_canvas_item_request_update (item);
}


static void
e_week_view_event_item_update (GnomeCanvasItem *item,
			       double	       *affine,
			       ArtSVP	       *clip_path,
			       int	        flags)
{
	EWeekViewEventItem *wveitem;
	EWeekView *week_view;
	gint span_x, span_y, span_w;

#if 0
	g_print ("In e_week_view_event_item_update\n");
#endif

	wveitem = E_WEEK_VIEW_EVENT_ITEM (item);
	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, clip_path, flags);

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;

	if (wveitem->event_num != -1 && wveitem->span_num != -1) {
		if (e_week_view_get_span_position (week_view,
						   wveitem->event_num,
						   wveitem->span_num,
						   &span_x, &span_y,
						   &span_w)) {
#if 0
			g_print ("  Event:%i Span:%i %i,%i W:%i\n",
				 wveitem->event_num, wveitem->span_num,
				 span_x, span_y, span_w);
#endif
			item->x1 = span_x;
			item->y1 = span_y;
			item->x2 = span_x + span_w - 1;
			item->y2 = span_y + week_view->row_height - 1;
		}
	}
}


/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */

static void
e_week_view_event_item_draw (GnomeCanvasItem  *canvas_item,
			     GdkDrawable      *drawable,
			     int	       x,
			     int	       y,
			     int	       width,
			     int	       height)
{
	EWeekViewEventItem *wveitem;
	EWeekView *week_view;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	GtkStyle *style;
	GdkGC *fg_gc, *gc;
	GdkFont *font;
	gint x1, y1, x2, y2, time_x, time_y, time_y_small_min;
	gint icon_x, icon_y, time_width, min_end_time_x;
	gint rect_x, rect_w, rect_x2;
	gboolean one_day_event, editing_span = FALSE;
	gint start_minute, end_minute;
	gchar buffer[128];
	gboolean draw_start_triangle = FALSE, draw_end_triangle = FALSE;
	GdkRectangle clip_rect;

#if 0
	g_print ("In e_week_view_event_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif

	wveitem = E_WEEK_VIEW_EVENT_ITEM (canvas_item);
	week_view = E_WEEK_VIEW (GTK_WIDGET (canvas_item->canvas)->parent);
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (wveitem->event_num == -1 || wveitem->span_num == -1)
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				wveitem->event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + wveitem->span_num);

	style = GTK_WIDGET (week_view)->style;
	font = style->font;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	gc = week_view->main_gc;

	x1 = canvas_item->x1 - x;
	y1 = canvas_item->y1 - y;
	x2 = canvas_item->x2 - x;
	y2 = canvas_item->y2 - y;

	if (x1 == x2 || y1 == y2)
		return;

	icon_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD;
	start_minute = event->start_minute;
	end_minute = event->end_minute;
	time_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD + font->ascent;
	if (week_view->small_font)
		time_y_small_min = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
			+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD
			+ week_view->small_font->ascent;
	if (week_view->use_small_font && week_view->small_font)
		time_width = week_view->digit_width * 2
			+ week_view->small_digit_width * 2;
	else
		time_width = week_view->digit_width * 4
			+ week_view->colon_width;

	one_day_event = e_week_view_is_one_day_event (week_view,
						      wveitem->event_num);
	if (one_day_event) {
		time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;

		/* Convert the time into a string. We use different parts of
		   the string for the different time formats. Notice that the
		   string is always 11 characters long. */
		sprintf (buffer, "%02i:%02i %02i:%02i",
			 start_minute / 60, start_minute % 60,
			 end_minute / 60, end_minute % 60);

		/* Draw the start and end times, as required. */
		switch (week_view->time_format) {
		case E_WEEK_VIEW_TIME_BOTH_SMALL_MIN:
			gdk_draw_text (drawable, font, fg_gc,
				       time_x, time_y, buffer, 2);
			gdk_draw_text (drawable, week_view->small_font, fg_gc,
				       time_x + week_view->digit_width * 2,
				       time_y_small_min, buffer + 3, 2);
			gdk_draw_text (drawable, font, fg_gc,
				       time_x + week_view->digit_width * 4 - 2,
				       time_y, buffer + 6, 2);
			gdk_draw_text (drawable, week_view->small_font, fg_gc,
				       time_x + week_view->digit_width * 6 - 2,
				       time_y_small_min, buffer + 9, 2);

			icon_x = x1 + time_width * 2 + week_view->space_width
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_START_SMALL_MIN:
			gdk_draw_text (drawable, font, fg_gc,
				       time_x, time_y, buffer, 2);
			gdk_draw_text (drawable, week_view->small_font, fg_gc,
				       time_x + week_view->digit_width * 2,
				       time_y_small_min, buffer + 3, 2);

			icon_x = x1 + time_width
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_BOTH:
			gdk_draw_text (drawable, font, fg_gc,
				       time_x, time_y, buffer, 11);
			icon_x = x1 + time_width * 2 + week_view->space_width
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_START:
			gdk_draw_text (drawable, font, fg_gc,
				       time_x, time_y, buffer, 5);
			icon_x = x1 + time_width
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
			break;
		case E_WEEK_VIEW_TIME_NONE:
			icon_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
			break;
		}

		/* Draw the icons. */
		e_week_view_event_item_draw_icons (wveitem, drawable,
						   icon_x, icon_y,
						   x2, FALSE);

	} else {
		rect_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
		rect_w = x2 - x1 - E_WEEK_VIEW_EVENT_L_PAD
			- E_WEEK_VIEW_EVENT_R_PAD + 1;

		/* Draw the triangles at the start & end, if needed. */
		if (event->start < week_view->day_starts[span->start_day]) {
			draw_start_triangle = TRUE;
			rect_x += 2;
			rect_w -= 2;
		}

		if (event->end > week_view->day_starts[span->start_day
						      + span->num_days]) {
			draw_end_triangle = TRUE;
			rect_w -= 2;
		}

		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
		gdk_draw_rectangle (drawable, gc, TRUE,
				    rect_x, y1 + 1, rect_w, y2 - y1 - 1);

		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER]);
		rect_x2 = rect_x + rect_w - 1;
		gdk_draw_line (drawable, gc, rect_x,  y1, rect_x2, y1);
		gdk_draw_line (drawable, gc, rect_x,  y2, rect_x2, y2);

		if (draw_start_triangle) {
			e_week_view_event_item_draw_triangle (wveitem, drawable, x1 + E_WEEK_VIEW_EVENT_L_PAD + 2, y1, -3, y2 - y1 + 1);
		} else {
			gdk_draw_line (drawable, gc, rect_x,  y1, rect_x, y2);
		}

		if (draw_end_triangle) {
			e_week_view_event_item_draw_triangle (wveitem, drawable, x2 - E_WEEK_VIEW_EVENT_R_PAD - 2, y1, 3, y2 - y1 + 1);
		} else {
			gdk_draw_line (drawable, gc, rect_x2, y1, rect_x2, y2);
		}

		if (span->text_item && E_TEXT (span->text_item)->editing)
			editing_span = TRUE;

		/* Draw the start & end times, if necessary. */
		min_end_time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
			+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
			+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;
		if (!editing_span
		    && event->start > week_view->day_starts[span->start_day]) {
			sprintf (buffer, "%02i:%02i",
				 start_minute / 60, start_minute % 60);
			time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_TEXT_X_PAD;

			clip_rect.x = x1;
			clip_rect.y = y1;
			clip_rect.width = x2 - x1 - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH + 1;
			clip_rect.height = y2 - y1 + 1;
			gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

			if (week_view->use_small_font
			    && week_view->small_font) {
				gdk_draw_text (drawable, font, fg_gc,
					       time_x, time_y, buffer, 2);
				gdk_draw_text (drawable, week_view->small_font,
					       fg_gc,
					       time_x + week_view->digit_width * 2,
					       time_y_small_min,
					       buffer + 3, 2);
			} else {
				gdk_draw_text (drawable, font, fg_gc,
					       time_x, time_y, buffer, 5);
			}

			gdk_gc_set_clip_rectangle (fg_gc, NULL);

			min_end_time_x += time_width + 2;
		}

		if (!editing_span
		    && event->end < week_view->day_starts[span->start_day
							 + span->num_days]) {
			sprintf (buffer, "%02i:%02i",
				 end_minute / 60, end_minute % 60);
			time_x = x2 - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_TEXT_X_PAD - 1
				- time_width;

			if (time_x >= min_end_time_x) {
				if (week_view->use_small_font
				    && week_view->small_font) {
					gdk_draw_text (drawable, font, fg_gc,
						       time_x, time_y,
						       buffer, 2);
					gdk_draw_text (drawable,
						       week_view->small_font,
						       fg_gc,
						       time_x + week_view->digit_width * 2,
						       time_y_small_min,
						       buffer + 3, 2);
				} else {
					gdk_draw_text (drawable, font, fg_gc,
						       time_x, time_y,
						       buffer, 5);
				}
			}
		}

		/* Draw the icons. */
		if (span->text_item) {
			icon_x = span->text_item->x1 - x;
			e_week_view_event_item_draw_icons (wveitem, drawable,
							   icon_x, icon_y,
							   x2, TRUE);
		}
	}
}


static void
e_week_view_event_item_draw_icons (EWeekViewEventItem *wveitem,
				   GdkDrawable        *drawable,
				   gint		       icon_x,
				   gint		       icon_y,
				   gint		       x2,
				   gboolean	       right_align)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	iCalObject *ico;
	GdkGC *gc;
	gint num_icons = 0, icon_x_inc;
	gboolean draw_reminder_icon = FALSE, draw_recurrence_icon = FALSE;

	week_view = E_WEEK_VIEW (GTK_WIDGET (GNOME_CANVAS_ITEM (wveitem)->canvas)->parent);

	event = &g_array_index (week_view->events, EWeekViewEvent,
				wveitem->event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + wveitem->span_num);
	ico = event->ico;

	gc = week_view->main_gc;

	if (ico->dalarm.enabled || ico->malarm.enabled
	    || ico->palarm.enabled || ico->aalarm.enabled) {
		draw_reminder_icon = TRUE;
		num_icons++;
	}

	if (ico->recur) {
		draw_recurrence_icon = TRUE;
		num_icons++;
	}

	icon_x_inc = E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD;

	if (right_align)
		icon_x -= icon_x_inc * num_icons;

	if (draw_reminder_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		gdk_gc_set_clip_origin (gc, icon_x, icon_y);
		gdk_gc_set_clip_mask (gc, week_view->reminder_mask);
		gdk_draw_pixmap (drawable, gc,
				 week_view->reminder_icon,
				 0, 0, icon_x, icon_y,
				 E_WEEK_VIEW_ICON_WIDTH,
				 E_WEEK_VIEW_ICON_HEIGHT);
		icon_x += icon_x_inc;
	}

	if (draw_recurrence_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		gdk_gc_set_clip_origin (gc, icon_x, icon_y);
		gdk_gc_set_clip_mask (gc, week_view->recurrence_mask);
		gdk_draw_pixmap (drawable, gc,
				 week_view->recurrence_icon,
				 0, 0, icon_x, icon_y,
				 E_WEEK_VIEW_ICON_WIDTH,
				 E_WEEK_VIEW_ICON_HEIGHT);
		icon_x += icon_x_inc;
	}

	gdk_gc_set_clip_mask (gc, NULL);
}


/* This draws a little triangle to indicate that an event extends past
   the days visible on screen. */
static void
e_week_view_event_item_draw_triangle (EWeekViewEventItem *wveitem,
				      GdkDrawable	 *drawable,
				      gint		  x,
				      gint		  y,
				      gint		  w,
				      gint		  h)
{
	EWeekView *week_view;
	GdkGC *gc;
	GdkPoint points[3];
	gint c1, c2;

	week_view = E_WEEK_VIEW (GTK_WIDGET (GNOME_CANVAS_ITEM (wveitem)->canvas)->parent);

	gc = week_view->main_gc;

	points[0].x = x;
	points[0].y = y;
	points[1].x = x + w;
	points[1].y = y + (h / 2) - 1;
	points[2].x = x;
	points[2].y = y + h - 1;

	gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
	gdk_draw_polygon (drawable, gc, TRUE, points, 3);

	gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER]);

	/* If the height is odd we can use the same central point for both
	   lines. If it is even we use different end-points. */
	c1 = c2 = y + (h / 2);
	if (h % 2 == 0)
		c1--;

	gdk_draw_line (drawable, gc, x, y, x + w, c1);
	gdk_draw_line (drawable, gc, x, y + h - 1, x + w, c2);
}


/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_week_view_event_item_point (GnomeCanvasItem *item, double x, double y,
			      int cx, int cy,
			      GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


static gint
e_week_view_event_item_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EWeekViewEventItem *wveitem;

	wveitem = E_WEEK_VIEW_EVENT_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return e_week_view_event_item_button_press (wveitem, event);
	case GDK_BUTTON_RELEASE:
		return e_week_view_event_item_button_release (wveitem, event);
	case GDK_MOTION_NOTIFY:
		break;
	default:
		break;
	}

	return FALSE;
}


static gboolean
e_week_view_event_item_button_press (EWeekViewEventItem *wveitem,
				     GdkEvent		*bevent)
{
	EWeekView *week_view;
	EWeekViewPosition pos;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (wveitem);

	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	event = &g_array_index (week_view->events, EWeekViewEvent,
				wveitem->event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + wveitem->span_num);

#if 0
	g_print ("In e_week_view_event_item_button_press\n");
#endif

	pos = e_week_view_event_item_get_position (wveitem, bevent->button.x,
						   bevent->button.y);
	if (pos == E_WEEK_VIEW_POS_NONE)
		return FALSE;

	week_view->pressed_event_num = wveitem->event_num;
	week_view->pressed_span_num = wveitem->span_num;

	if (bevent->button.button == 1) {
		/* Ignore clicks on the event while editing. */
		if (E_TEXT (span->text_item)->editing)
			return FALSE;

		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		week_view->drag_event_x = bevent->button.x;
		week_view->drag_event_y = bevent->button.y;

		/* FIXME: Remember the day offset from the start of the event.
		 */
	} else if (bevent->button.button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (week_view))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));
		e_week_view_show_popup_menu (week_view,
					     (GdkEventButton*) bevent,
					     wveitem->event_num);
		gtk_signal_emit_stop_by_name (GTK_OBJECT (item->canvas),
					      "button_press_event");
	}

	return TRUE;
}


static gboolean
e_week_view_event_item_button_release (EWeekViewEventItem *wveitem,
				       GdkEvent		  *event)
{
	EWeekView *week_view;
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (wveitem);

	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

#if 0
	g_print ("In e_week_view_event_item_button_release\n");
#endif

	if (week_view->pressed_event_num != -1
	    && week_view->pressed_event_num == wveitem->event_num
	    && week_view->pressed_span_num == wveitem->span_num) {
		e_week_view_start_editing_event (week_view,
						 wveitem->event_num,
						 wveitem->span_num,
						 NULL);
		week_view->pressed_event_num = -1;
		return TRUE;
	}

	week_view->pressed_event_num = -1;

	return FALSE;
}


static EWeekViewPosition
e_week_view_event_item_get_position (EWeekViewEventItem *wveitem,
				     gdouble x,
				     gdouble y)
{
	EWeekView *week_view;
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (wveitem);

	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), E_WEEK_VIEW_POS_NONE);

#if 0
	g_print ("In e_week_view_event_item_get_position item: %g,%g %g,%g point: %g,%g\n", item->x1, item->y1, item->x2, item->y2, x, y);
#endif

	if (x < item->x1 + E_WEEK_VIEW_EVENT_L_PAD
	    || x >= item->x2 - E_WEEK_VIEW_EVENT_R_PAD)
		return E_WEEK_VIEW_POS_NONE;

	/* Support left/right edge for long events only. */
	if (!e_week_view_is_one_day_event (week_view, wveitem->event_num)) {
		if (x < item->x1 + E_WEEK_VIEW_EVENT_L_PAD
		    + E_WEEK_VIEW_EVENT_BORDER_WIDTH
		    + E_WEEK_VIEW_EVENT_TEXT_X_PAD)
			return E_WEEK_VIEW_POS_LEFT_EDGE;

		if (x >= item->x2 - E_WEEK_VIEW_EVENT_R_PAD
		    - E_WEEK_VIEW_EVENT_BORDER_WIDTH
		    - E_WEEK_VIEW_EVENT_TEXT_X_PAD)
			return E_WEEK_VIEW_POS_RIGHT_EDGE;
	}

	return E_WEEK_VIEW_POS_EVENT;
}

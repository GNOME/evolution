/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 2001, Ximian, Inc.
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
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#include <config.h>

#include "e-util/e-categories-config.h"
#include "e-week-view-event-item.h"

#include <gtk/gtksignal.h>
#include <gal/e-text/e-text.h>

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
static void e_week_view_draw_time		(EWeekView	*week_view,
						 GdkDrawable	*drawable,
						 gint		 time_x,
						 gint		 time_y,
						 gint		 hour,
						 gint		 minute);
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
static gboolean e_week_view_event_item_double_click (EWeekViewEventItem *wveitem,
						     GdkEvent		*bevent);
static ECalendarViewPosition e_week_view_event_item_get_position (EWeekViewEventItem *wveitem,
							      gdouble x,
							      gdouble y);


static GnomeCanvasItemClass *parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_EVENT_NUM,
	ARG_SPAN_NUM
};

E_MAKE_TYPE (e_week_view_event_item, "EWeekViewEventItem", EWeekViewEventItem,
	     e_week_view_event_item_class_init, e_week_view_event_item_init,
	     GNOME_TYPE_CANVAS_ITEM);

static void
e_week_view_event_item_class_init (EWeekViewEventItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = g_type_class_peek_parent (class);

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
	GdkGC *gc;
	gint x1, y1, x2, y2, time_x, time_y;
	gint icon_x, icon_y, time_width, min_end_time_x, max_icon_x;
	gint rect_x, rect_w, rect_x2;
	gboolean one_day_event, editing_span = FALSE;
	gint start_hour, start_minute, end_hour, end_minute;
	gboolean draw_start, draw_end;
	gboolean draw_start_triangle = FALSE, draw_end_triangle = FALSE;
	GdkRectangle clip_rect;
	GdkColor bg_color;

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
	gc = week_view->main_gc;

	x1 = canvas_item->x1 - x;
	y1 = canvas_item->y1 - y;
	x2 = canvas_item->x2 - x;
	y2 = canvas_item->y2 - y;

	if (x1 == x2 || y1 == y2)
		return;

	icon_x = 0;
	icon_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD;

	/* Get the start & end times in 24-hour format. */
	start_hour = event->start_minute / 60;
	start_minute = event->start_minute % 60;
	end_hour = event->end_minute / 60;
	end_minute = event->end_minute % 60;

	time_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD;

	time_width = e_week_view_get_time_string_width (week_view);

	one_day_event = e_week_view_is_one_day_event (week_view,
						      wveitem->event_num);
	if (one_day_event) {
		time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD + 1;
		rect_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
		rect_w = x2 - x1 - E_WEEK_VIEW_EVENT_L_PAD - E_WEEK_VIEW_EVENT_R_PAD + 1;

		if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)),
									  event->comp_data),
				     &bg_color)) {
			GdkColormap *colormap;

			colormap = gtk_widget_get_colormap (GTK_WIDGET (week_view));
			if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE))
				gdk_gc_set_foreground (gc, &bg_color);
			else
				gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
		} else
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
		gdk_draw_rectangle (drawable, gc, TRUE, rect_x, y1 + 1, rect_w, y2 - y1 - 1);

		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER]);
		gdk_draw_line (drawable, gc, rect_x,  y1 + 1, rect_x + rect_w, y1 + 1);
		gdk_draw_line (drawable, gc, rect_x,  y2, rect_x + rect_w, y2);
		gdk_draw_line (drawable, gc, rect_x, y1 + 1, rect_x, y1 + (y2 - (y1 + 1)));
		gdk_draw_line (drawable, gc, rect_x + rect_w, y1 + 1, rect_x + rect_w, y1 + (y2 - (y1 + 1)));

		/* Draw the start and end times, as required. */
		switch (week_view->time_format) {
		case E_WEEK_VIEW_TIME_BOTH_SMALL_MIN:
		case E_WEEK_VIEW_TIME_BOTH:
			draw_start = TRUE;
			draw_end = TRUE;
			break;

		case E_WEEK_VIEW_TIME_START_SMALL_MIN:
		case E_WEEK_VIEW_TIME_START:
			draw_start = TRUE;
			draw_end = FALSE;
			break;

		case E_WEEK_VIEW_TIME_NONE:
			draw_start = FALSE;
			draw_end = FALSE;
			break;
		default:
			g_assert_not_reached();
			draw_start = FALSE;
			draw_end = FALSE;
			break;
		}

		if (draw_start) {
			e_week_view_draw_time (week_view, drawable,
					       time_x, time_y,
					       start_hour, start_minute);
			time_x += time_width;
		}

		if (draw_end) {
			time_x += E_WEEK_VIEW_EVENT_TIME_SPACING;
			e_week_view_draw_time (week_view, drawable,
					       time_x, time_y,
					       end_hour, end_minute);
			time_x += time_width;
		}

		icon_x = time_x;
		if (draw_start)
			icon_x += E_WEEK_VIEW_EVENT_TIME_X_PAD;

		/* Draw the icons. */
		e_week_view_event_item_draw_icons (wveitem, drawable,
						   icon_x, icon_y,
						   x2, FALSE);

	} else {
		rect_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
		rect_w = x2 - x1 - E_WEEK_VIEW_EVENT_L_PAD
			- E_WEEK_VIEW_EVENT_R_PAD + 1;

		/* Draw the triangles at the start & end, if needed.
		   They also use the first few pixels at the edge of the
		   event so we update rect_x & rect_w so we don't draw over
		   them. */
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

		if (gdk_color_parse (e_cal_model_get_color_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (week_view)),
									  event->comp_data),
				     &bg_color)) {
			GdkColormap *colormap;

			colormap = gtk_widget_get_colormap (GTK_WIDGET (week_view));
			if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE))
				gdk_gc_set_foreground (gc, &bg_color);
			else
				gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
		} else
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND]);
		gdk_draw_rectangle (drawable, gc, TRUE, rect_x, y1 + 1, rect_w, y2 - y1 - 1);

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

		/* Draw the start & end times, if they are not on day
		   boundaries. The start time would always be shown if it was
		   needed, though it may be clipped as the window shrinks.
		   The end time is only displayed if there is enough room.
		   We calculate the minimum position for the end time, which
		   depends on whether the start time is displayed. If the end
		   time doesn't fit, then we don't draw it. */
		min_end_time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
			+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
			+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
		if (!editing_span
		    && event->start > week_view->day_starts[span->start_day]) {
			time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;

			clip_rect.x = x1;
			clip_rect.y = y1;
			clip_rect.width = x2 - x1 - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH + 1;
			clip_rect.height = y2 - y1 + 1;
			gdk_gc_set_clip_rectangle (gc, &clip_rect);

			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT]);

			e_week_view_draw_time (week_view, drawable,
					       time_x, time_y,
					       start_hour, start_minute);

			gdk_gc_set_clip_rectangle (gc, NULL);

			/* We don't want the end time to be drawn over the
			   start time, so we increase the minimum position. */
			min_end_time_x += time_width
				+ E_WEEK_VIEW_EVENT_TIME_X_PAD;
		}

		max_icon_x = x2 + 1 - E_WEEK_VIEW_EVENT_R_PAD
			- E_WEEK_VIEW_EVENT_BORDER_WIDTH
			- E_WEEK_VIEW_EVENT_EDGE_X_PAD;

		if (!editing_span
		    && event->end < week_view->day_starts[span->start_day
							 + span->num_days]) {
			/* Calculate where the end time should be displayed. */
			time_x = x2 + 1 - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH
				- E_WEEK_VIEW_EVENT_EDGE_X_PAD
				- time_width;

			/* Draw the end time, if the position is greater than
			   the minimum calculated above. */
			if (time_x >= min_end_time_x) {
				e_week_view_draw_time (week_view, drawable,
						       time_x, time_y,
						       end_hour, end_minute);
				max_icon_x -= time_width
					+ E_WEEK_VIEW_EVENT_TIME_X_PAD;
			}
		}

		/* Draw the icons. */
		if (span->text_item
		    && (week_view->editing_event_num != wveitem->event_num
			|| week_view->editing_span_num != wveitem->span_num)) {
			icon_x = span->text_item->x1 - E_WEEK_VIEW_ICON_R_PAD - x;
			e_week_view_event_item_draw_icons (wveitem, drawable,
							   icon_x, icon_y,
							   max_icon_x, TRUE);
		}
	}
}


static void
e_week_view_draw_time	(EWeekView	*week_view,
			 GdkDrawable	*drawable,
			 gint		 time_x,
			 gint		 time_y,
			 gint		 hour,
			 gint		 minute)
{
	GtkStyle *style;
	GdkGC *gc;
	gint hour_to_display, suffix_width;
	gint time_y_normal_font, time_y_small_font;
	gchar buffer[128], *suffix;
	PangoLayout *layout;
	PangoFontDescription *small_font_desc;

	style = gtk_widget_get_style (GTK_WIDGET (week_view));
	small_font_desc = week_view->small_font_desc;
	gc = week_view->main_gc;

	gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_TEXT]);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), NULL);

	time_y_normal_font = time_y_small_font = time_y;
	if (small_font_desc)
		time_y_small_font = time_y;

	e_week_view_convert_time_to_display (week_view, hour, &hour_to_display,
					     &suffix, &suffix_width);

	if (week_view->use_small_font && week_view->small_font_desc) {
		g_snprintf (buffer, sizeof (buffer), "%2i:%02i",
			    hour_to_display, minute);

		/* Draw the hour. */
		if (hour_to_display < 10) {
			pango_layout_set_text (layout, buffer + 1, 1);
			gdk_draw_layout (drawable, gc,
					 time_x + week_view->digit_width,
					 time_y_normal_font,
					 layout);
		} else {
			pango_layout_set_text (layout, buffer, 2);
			gdk_draw_layout (drawable, gc,
					 time_x,
					 time_y_normal_font,
					 layout);
		}

		time_x += week_view->digit_width * 2;

		/* Draw the start minute, in the small font. */
		pango_layout_set_font_description (layout, week_view->small_font_desc);
		pango_layout_set_text (layout, buffer + 3, 2);
		gdk_draw_layout (drawable, gc,
				 time_x,
				 time_y_small_font,
				 layout);

		pango_layout_set_font_description (layout, style->font_desc);

		time_x += week_view->small_digit_width * 2;

		/* Draw the 'am'/'pm' suffix, if 12-hour format. */
		if (!e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (week_view))) {
			pango_layout_set_text (layout, suffix, -1);
			gdk_draw_layout (drawable, gc,
					 time_x,
					 time_y_normal_font,
					 layout);
		}
	} else {
		/* Draw the start time in one go. */
		g_snprintf (buffer, sizeof (buffer), "%2i:%02i%s",
			    hour_to_display, minute, suffix);
		if (hour_to_display < 10) {
			pango_layout_set_text (layout, buffer + 1, -1);
			gdk_draw_layout (drawable, gc,
					 time_x + week_view->digit_width,
					 time_y_normal_font,
					 layout);
		} else {
			pango_layout_set_text (layout, buffer, -1);
			gdk_draw_layout (drawable, gc,
					 time_x,
					 time_y_normal_font,
					 layout);
		}

	}

	g_object_unref (layout);
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
	ECalComponent *comp;
	GdkGC *gc;
	gint num_icons = 0, icon_x_inc;
	gboolean draw_reminder_icon = FALSE, draw_recurrence_icon = FALSE;
	gboolean draw_timezone_icon = FALSE;
	GSList *categories_list, *elem;

	week_view = E_WEEK_VIEW (GTK_WIDGET (GNOME_CANVAS_ITEM (wveitem)->canvas)->parent);

	event = &g_array_index (week_view->events, EWeekViewEvent,
				wveitem->event_num);
	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + wveitem->span_num);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	gc = week_view->main_gc;

	if (e_cal_component_has_alarms (comp)) {
		draw_reminder_icon = TRUE;
		num_icons++;
	}

	if (e_cal_component_has_recurrences (comp)) {
		draw_recurrence_icon = TRUE;
		num_icons++;
	}

	if (event->different_timezone) {
		draw_timezone_icon = TRUE;
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

	icon_x_inc = E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD;

	if (right_align)
		icon_x -= icon_x_inc * num_icons;

	if (draw_reminder_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_draw_pixbuf (drawable, gc,
				 week_view->reminder_icon,
				 0, 0, icon_x, icon_y,
				 E_WEEK_VIEW_ICON_WIDTH,
				 E_WEEK_VIEW_ICON_HEIGHT,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
		icon_x += icon_x_inc;
	}

	if (draw_recurrence_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_draw_pixbuf (drawable, gc,
				 week_view->recurrence_icon,
				 0, 0, icon_x, icon_y,
				 E_WEEK_VIEW_ICON_WIDTH,
				 E_WEEK_VIEW_ICON_HEIGHT,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
		icon_x += icon_x_inc;
	}

	if (draw_timezone_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		gdk_gc_set_clip_mask (gc, NULL);
		gdk_draw_pixbuf (drawable, gc,
				 week_view->timezone_icon,
				 0, 0, icon_x, icon_y,
				 E_WEEK_VIEW_ICON_WIDTH,
				 E_WEEK_VIEW_ICON_HEIGHT,
				 GDK_RGB_DITHER_NORMAL,
				 0, 0);
		icon_x += icon_x_inc;
	}

	/* draw categories icons */
	for (elem = categories_list; elem; elem = elem->next) {
		char *category;
		GdkPixmap *pixmap = NULL;
		GdkBitmap *mask = NULL;

		category = (char *) elem->data;
		if (!e_categories_config_get_icon_for (category, &pixmap, &mask))
			continue;

		if (icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			if (mask != NULL)
				gdk_gc_set_clip_mask (gc, mask);
			gdk_draw_pixmap (drawable, gc,
					 pixmap,
					 0, 0, icon_x, icon_y,
					 E_WEEK_VIEW_ICON_WIDTH,
					 E_WEEK_VIEW_ICON_HEIGHT);
			icon_x += icon_x_inc;
		}
		gdk_pixmap_unref (pixmap);
		if (mask != NULL)
			gdk_bitmap_unref (mask);
	}

	e_cal_component_free_categories_list (categories_list);

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
	case GDK_2BUTTON_PRESS:
		return e_week_view_event_item_double_click (wveitem, event);
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
	ECalendarViewPosition pos;
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
	if (pos == E_CALENDAR_VIEW_POS_NONE)
		return FALSE;

	if (bevent->button.button == 1) {
		week_view->pressed_event_num = wveitem->event_num;
		week_view->pressed_span_num = wveitem->span_num;

		/* Ignore clicks on the event while editing. */
		if (E_TEXT (span->text_item)->editing)
			return FALSE;

		/* Remember the item clicked and the mouse position,
		   so we can start a drag if the mouse moves. */
		week_view->drag_event_x = bevent->button.x;
		week_view->drag_event_y = bevent->button.y;

		/* FIXME: Remember the day offset from the start of the event.
		 */

		return TRUE;
	} else if (bevent->button.button == 3) {
		if (!GTK_WIDGET_HAS_FOCUS (week_view))
			gtk_widget_grab_focus (GTK_WIDGET (week_view));

		e_week_view_set_selected_time_range_visible (week_view, event->start, event->end);

		e_week_view_show_popup_menu (week_view,
					     (GdkEventButton*) bevent,
					     wveitem->event_num);
		gtk_signal_emit_stop_by_name (GTK_OBJECT (item->canvas),
					      "button_press_event");

		return TRUE;
	}

	return FALSE;
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


static gboolean
e_week_view_event_item_double_click (EWeekViewEventItem *wveitem,
				     GdkEvent		*bevent)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (wveitem);

	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), FALSE);

	event = &g_array_index (week_view->events, EWeekViewEvent,
				wveitem->event_num);

	e_week_view_stop_editing_event (week_view);

	e_calendar_view_edit_appointment (E_CALENDAR_VIEW (week_view), event->comp_data->client, event->comp_data->icalcomp, FALSE);

	return TRUE;
}


static ECalendarViewPosition
e_week_view_event_item_get_position (EWeekViewEventItem *wveitem,
				     gdouble x,
				     gdouble y)
{
	EWeekView *week_view;
	GnomeCanvasItem *item;

	item = GNOME_CANVAS_ITEM (wveitem);

	week_view = E_WEEK_VIEW (GTK_WIDGET (item->canvas)->parent);
	g_return_val_if_fail (E_IS_WEEK_VIEW (week_view), E_CALENDAR_VIEW_POS_NONE);

#if 0
	g_print ("In e_week_view_event_item_get_position item: %g,%g %g,%g point: %g,%g\n", item->x1, item->y1, item->x2, item->y2, x, y);
#endif

	if (x < item->x1 + E_WEEK_VIEW_EVENT_L_PAD
	    || x >= item->x2 - E_WEEK_VIEW_EVENT_R_PAD)
		return E_CALENDAR_VIEW_POS_NONE;

	/* Support left/right edge for long events only. */
	if (!e_week_view_is_one_day_event (week_view, wveitem->event_num)) {
		if (x < item->x1 + E_WEEK_VIEW_EVENT_L_PAD
		    + E_WEEK_VIEW_EVENT_BORDER_WIDTH
		    + E_WEEK_VIEW_EVENT_EDGE_X_PAD)
			return E_CALENDAR_VIEW_POS_LEFT_EDGE;

		if (x >= item->x2 + 1 - E_WEEK_VIEW_EVENT_R_PAD
		    - E_WEEK_VIEW_EVENT_BORDER_WIDTH
		    - E_WEEK_VIEW_EVENT_EDGE_X_PAD)
			return E_CALENDAR_VIEW_POS_RIGHT_EDGE;
	}

	return E_CALENDAR_VIEW_POS_EVENT;
}

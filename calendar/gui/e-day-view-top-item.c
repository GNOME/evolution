/*
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
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * EDayViewTopItem - displays the top part of the Day/Work Week calendar view.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include "e-util/e-categories-config.h"
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-categories.h>
#include "calendar-config.h"
#include "e-calendar-view.h"
#include "e-day-view-top-item.h"

#define E_DAY_VIEW_TOP_ITEM_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_DAY_VIEW_TOP_ITEM, EDayViewTopItemPrivate))

struct _EDayViewTopItemPrivate {
	/* The parent EDayView widget. */
	EDayView *day_view;

	/* Show dates or events. */
	gboolean show_dates;
};

enum {
	PROP_0,
	PROP_DAY_VIEW,
	PROP_SHOW_DATES
};

static gpointer parent_class;

/* This draws a little triangle to indicate that an event extends past
   the days visible on screen. */
static void
day_view_top_item_draw_triangle (EDayViewTopItem *top_item,
                                 GdkDrawable *drawable,
                                 gint x,
                                 gint y,
                                 gint w,
                                 gint h,
                                 gint event_num)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GdkColor bg_color;
	GdkPoint points[3];
	gint c1, c2;
	cairo_t *cr;

	cr = gdk_cairo_create (drawable);

	day_view = e_day_view_top_item_get_day_view (top_item);

	points[0].x = x;
	points[0].y = y;
	points[1].x = x + w;
	points[1].y = y + (h / 2);
	points[2].x = x;
	points[2].y = y + h - 1;

	/* If the height is odd we can use the same central point for both
	   lines. If it is even we use different end-points. */
	c1 = c2 = y + (h / 2);
	if (h % 2 == 0)
		c1--;

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	cairo_save (cr);
	/* Fill it in. */
	if (gdk_color_parse (
		e_cal_model_get_color_for_component (
		e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)),
		event->comp_data), &bg_color)) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
		if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE)) {
			gdk_cairo_set_source_color (cr, &bg_color);
		} else {
			gdk_cairo_set_source_color (
				cr, &day_view->colors
				[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND]);
		}
	} else {
		gdk_cairo_set_source_color (
			cr, &day_view->colors
			[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND]);
	}

	cairo_move_to (cr, points[0].x, points[0].y);
	cairo_line_to (cr, points[1].x, points[1].y);
	cairo_line_to (cr, points[2].x, points[2].y);
	cairo_line_to (cr, points[0].x, points[0].y);
	cairo_fill (cr);
	cairo_restore (cr);

	cairo_save (cr);
	gdk_cairo_set_source_color (
		cr, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, x + w, c1);
	cairo_move_to (cr, x, y + h - 1);
	cairo_line_to (cr, x + w, c2);
	cairo_stroke (cr);
	cairo_restore (cr);

	cairo_destroy (cr);
}

/* This draws one event in the top canvas. */
static void
day_view_top_item_draw_long_event (EDayViewTopItem *top_item,
                                   gint event_num,
                                   GdkDrawable *drawable,
                                   gint x,
                                   gint y,
                                   gint width,
                                   gint height)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GtkStyle *style;
	GdkGC *gc, *fg_gc;
	gint start_day, end_day;
	gint item_x, item_y, item_w, item_h;
	gint text_x, icon_x, icon_y, icon_x_inc;
	ECalModel *model;
	ECalComponent *comp;
	gchar buffer[16];
	gint hour, display_hour, minute, offset, time_width, time_x;
	gint min_end_time_x, suffix_width, max_icon_x;
	const gchar *suffix;
	gboolean draw_start_triangle, draw_end_triangle;
	GdkRectangle clip_rect;
	GSList *categories_list, *elem;
	PangoLayout *layout;
	GdkColor bg_color;
	cairo_t *cr;
	cairo_pattern_t *pat;
	guint16 red, green, blue;
	gdouble cc = 65535.0;
	gboolean gradient;
	gfloat alpha;
	gdouble x0, y0, rect_height, rect_width, radius;

	day_view = e_day_view_top_item_get_day_view (top_item);
	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	cr = gdk_cairo_create (drawable);

	gradient = calendar_config_get_display_events_gradient ();
	alpha = calendar_config_get_display_events_alpha ();

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

	if (!is_array_index_in_bounds (day_view->long_events, event_num))
		return;

	event = &g_array_index (day_view->long_events, EDayViewEvent,
				event_num);

	if (!is_comp_data_valid (event))
		return;

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	gc = day_view->main_gc;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	if (gdk_color_parse (
		e_cal_model_get_color_for_component (
		e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)),
		event->comp_data), &bg_color)) {
		GdkColormap *colormap;

		colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
		if (gdk_colormap_alloc_color (colormap, &bg_color, TRUE, TRUE)) {
			red = bg_color.red;
			green = bg_color.green;
			blue = bg_color.blue;
		} else {
			red = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].red;
			green = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].green;
			blue = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].blue;
		}
	} else {
		red = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].red;
		green = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].green;
		blue = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND].blue;
	}

	/* Fill the background with white to play with transparency */
	cairo_save (cr);
	x0	   = item_x - x + 4;
	y0	   = item_y + 1 - y;
	rect_width  = item_w - 8;
	rect_height = item_h - 2;

	radius = 12;

	draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

	cairo_set_source_rgba (cr, 1, 1, 1, alpha);
	cairo_fill_preserve (cr);

	cairo_restore (cr);

	/* Draw the border around the event */

	cairo_save (cr);
	x0	   = item_x - x + 4;
	y0	   = item_y + 1 - y;
	rect_width  = item_w - 8;
	rect_height = item_h - 2;

	radius = 12;

	draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

	cairo_set_source_rgb (cr, red/cc, green/cc, blue/cc);
	cairo_set_line_width (cr, 1.5);
	cairo_stroke (cr);
	cairo_restore (cr);

	/* Fill in with gradient */

	cairo_save (cr);

	x0	   = item_x - x + 5.5;
	y0	   = item_y + 2.5 - y;
	rect_width  = item_w - 11;
	rect_height = item_h - 5;

	radius = 10;

	draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

	if (gradient) {
		pat = cairo_pattern_create_linear (item_x - x + 5.5, item_y + 2.5 - y,
						item_x - x + 5, item_y - y + item_h + 7.5);
		cairo_pattern_add_color_stop_rgba (pat, 1, red/cc, green/cc, blue/cc, 0.8);
		cairo_pattern_add_color_stop_rgba (pat, 0, red/cc, green/cc, blue/cc, 0.4);
		cairo_set_source (cr, pat);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pat);
	} else {
		cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0.8);
		cairo_fill_preserve (cr);
	}
	cairo_set_source_rgba (cr, red/cc, green/cc, blue/cc, 0);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke (cr);
	cairo_restore (cr);

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

	/* If the event starts before the first day shown, draw a triangle */
	if (draw_start_triangle
	    && event->start < day_view->day_starts[start_day]) {
		day_view_top_item_draw_triangle (
			top_item, drawable, item_x - x + 4, item_y - y,
			-E_DAY_VIEW_BAR_WIDTH, item_h, event_num);
	}

	/* Similar for the event end. */
	if (draw_end_triangle
	    && event->end > day_view->day_starts[end_day + 1]) {
		day_view_top_item_draw_triangle (
			top_item, drawable, item_x + item_w - 4 - x,
			item_y - y, E_DAY_VIEW_BAR_WIDTH, item_h,
			event_num);
	}

	/* If we are editing the event we don't show the icons or the start
	   & end times. */
	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num) {
		g_object_unref (comp);
		cairo_destroy (cr);
		return;
	}

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
		if (e_cal_model_get_use_24_hour_format (model)) {
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
		time_x =
			item_x + item_w - E_DAY_VIEW_LONG_EVENT_X_PAD -
			time_width - E_DAY_VIEW_LONG_EVENT_TIME_X_PAD - x;

		if (time_x >= min_end_time_x) {
			/* Calculate the actual hour number to display. */
			e_day_view_convert_time_to_display (day_view, hour,
							    &display_hour,
							    &suffix,
							    &suffix_width);
			if (e_cal_model_get_use_24_hour_format (model)) {
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

	if (icon_x <= max_icon_x && (
		e_cal_component_has_recurrences (comp) ||
		e_cal_component_is_instance (comp))) {
		cairo_save (cr);
		gdk_cairo_set_source_pixbuf (cr, day_view->recurrence_icon, icon_x, icon_y);
		cairo_paint (cr);
		cairo_restore (cr);

		icon_x -= icon_x_inc;
	}

	if (icon_x <= max_icon_x && e_cal_component_has_attachments (comp)) {
		cairo_save (cr);
		gdk_cairo_set_source_pixbuf (cr, day_view->attach_icon, icon_x, icon_y);
		cairo_paint (cr);
		cairo_restore (cr);

		icon_x -= icon_x_inc;
	}
	if (icon_x <= max_icon_x && e_cal_component_has_alarms (comp)) {
		cairo_save (cr);
		gdk_cairo_set_source_pixbuf (cr, day_view->reminder_icon, icon_x, icon_y);
		cairo_paint (cr);
		cairo_restore (cr);

		icon_x -= icon_x_inc;
	}

	if (icon_x <= max_icon_x && e_cal_component_has_attendees (comp)) {
		cairo_save (cr);
		gdk_cairo_set_source_pixbuf (cr, day_view->meeting_icon, icon_x, icon_y);
		cairo_paint (cr);
		cairo_restore (cr);

		icon_x -= icon_x_inc;
	}

	/* draw categories icons */
	e_cal_component_get_categories_list (comp, &categories_list);
	for (elem = categories_list; elem; elem = elem->next) {
		gchar *category;
		const gchar *file;
		GdkPixbuf *pixbuf;

		category = (gchar *) elem->data;
		file = e_categories_get_icon_file_for (category);
		if (!file)
			continue;

		pixbuf = gdk_pixbuf_new_from_file (file, NULL);
		if (pixbuf == NULL)
			continue;

		if (icon_x <= max_icon_x) {
			gdk_gc_set_clip_origin (gc, icon_x, icon_y);
			gdk_draw_pixbuf (drawable, gc,
					 pixbuf,
					 0, 0, icon_x, icon_y,
					 E_DAY_VIEW_ICON_WIDTH,
					 E_DAY_VIEW_ICON_HEIGHT,
					 GDK_RGB_DITHER_NORMAL, 0, 0);
			icon_x -= icon_x_inc;
		}
	}

	e_cal_component_free_categories_list (categories_list);
	g_object_unref (comp);
	cairo_destroy (cr);
	gdk_gc_set_clip_mask (gc, NULL);
}

static void
day_view_top_item_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			e_day_view_top_item_set_day_view (
				E_DAY_VIEW_TOP_ITEM (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_DATES:
			e_day_view_top_item_set_show_dates (
				E_DAY_VIEW_TOP_ITEM (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_top_item_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			g_value_set_object (
				value, e_day_view_top_item_get_day_view (
				E_DAY_VIEW_TOP_ITEM (object)));
			return;

		case PROP_SHOW_DATES:
			g_value_set_boolean (
				value, e_day_view_top_item_get_show_dates (
				E_DAY_VIEW_TOP_ITEM (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_top_item_dispose (GObject *object)
{
	EDayViewTopItemPrivate *priv;

	priv = E_DAY_VIEW_TOP_ITEM_GET_PRIVATE (object);

	if (priv->day_view != NULL) {
		g_object_unref (priv->day_view);
		priv->day_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
day_view_top_item_update (GnomeCanvasItem *item,
                          gdouble *affine,
                          ArtSVP *clip_path,
                          gint flags)
{
	GnomeCanvasItemClass *canvas_item_class;

	/* Chain up to parent's update() method. */
	canvas_item_class = GNOME_CANVAS_ITEM_CLASS (parent_class);
	canvas_item_class->update (item, affine, clip_path, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}

static void
day_view_top_item_draw (GnomeCanvasItem *canvas_item,
                        GdkDrawable *drawable,
                        gint x,
                        gint y,
                        gint width,
                        gint height)
{
	EDayViewTopItem *top_item;
	EDayView *day_view;
	GtkStyle *style;
	GdkGC *fg_gc;
	gchar buffer[128];
	GtkAllocation allocation;
	GdkRectangle clip_rect;
	gint canvas_width, canvas_height, left_edge, day, date_width, date_x;
	gint item_height, event_num;
	PangoLayout *layout;
	cairo_t *cr;
	GdkColor bg, light, dark;
	gboolean show_dates;

	top_item = E_DAY_VIEW_TOP_ITEM (canvas_item);
	day_view = e_day_view_top_item_get_day_view (top_item);
	g_return_if_fail (day_view != NULL);
	show_dates = top_item->priv->show_dates;

	cr = gdk_cairo_create (drawable);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	gtk_widget_get_allocation (
		GTK_WIDGET (canvas_item->canvas), &allocation);
	canvas_width = allocation.width;
	canvas_height =
		(show_dates ? 1 :
		(MAX (1, day_view->rows_in_top_display) + 1)) *
		day_view->top_row_height;
	left_edge = 0;
	item_height = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;

	bg = style->bg[GTK_STATE_NORMAL];
	light = style->light[GTK_STATE_NORMAL];
	dark = style->dark[GTK_STATE_NORMAL];

	if (show_dates) {
		/* Draw the shadow around the dates. */
		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &light);
		cairo_move_to (cr, left_edge - x, 1 - y);
		cairo_line_to (cr, canvas_width - 2 - x, 1 - y);
		cairo_move_to (cr, left_edge - x, 2 - y);
		cairo_line_to (cr, left_edge - x, item_height - 2 - y);
		cairo_stroke (cr);
		cairo_restore (cr);

		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &dark);
		cairo_move_to (cr, left_edge - x, item_height - 1 - y);
		cairo_line_to (cr, canvas_width - 1 - x, item_height - 1 - y);
		cairo_move_to (cr, canvas_width - 1 - x, 1 - y);
		cairo_line_to (cr, canvas_width - 1 - x, item_height - 1 - y);
		cairo_stroke (cr);
		cairo_restore (cr);

		/* Draw the background for the dates. */
		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &bg);
		cairo_rectangle (cr, left_edge + 2 - x, 2 - y,
				 canvas_width - left_edge - 3,
				 item_height - 3);
		cairo_fill (cr);
		cairo_restore (cr);
	}

	if (!show_dates) {
		/* Clear the main area background. */
		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS]);
		cairo_rectangle (cr, left_edge - x, - y,
				 canvas_width - left_edge,
				 canvas_height);
		cairo_fill (cr);
		cairo_restore (cr);

		/* Draw the selection background. */
		if (gtk_widget_has_focus (GTK_WIDGET (day_view))
			&& day_view->selection_start_day != -1) {
			gint start_col, end_col, rect_x, rect_y, rect_w, rect_h;

			start_col = day_view->selection_start_day;
			end_col = day_view->selection_end_day;

			if (end_col > start_col
			    || day_view->selection_start_row == -1
			    || day_view->selection_end_row == -1) {
				rect_x = day_view->day_offsets[start_col];
				rect_y = 0;
				rect_w = day_view->day_offsets[end_col + 1] - rect_x;
				rect_h = canvas_height - 1 - rect_y;

				cairo_save (cr);
				gdk_cairo_set_source_color (
					cr, &day_view->colors
					[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_SELECTED]);
				cairo_rectangle (cr, rect_x - x, rect_y - y,
						 rect_w, rect_h);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		}
	}

	if (show_dates) {
		/* Draw the date. Set a clipping rectangle so we don't draw over the
		   next day. */
		for (day = 0; day < day_view->days_shown; day++) {
			e_day_view_top_item_get_day_label (day_view, day, buffer, sizeof (buffer));
			clip_rect.x = day_view->day_offsets[day] - x;
			clip_rect.y = 2 - y;
			if (day_view->days_shown == 1) {
				gtk_widget_get_allocation (
					day_view->top_canvas, &allocation);
				clip_rect.width = allocation.width - day_view->day_offsets[day];
			} else
				clip_rect.width = day_view->day_widths[day];
			clip_rect.height = item_height - 2;

			gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

			layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
			pango_layout_get_pixel_size (layout, &date_width, NULL);
			date_x = day_view->day_offsets[day] + (clip_rect.width - date_width) / 2;

			gdk_draw_layout (drawable, fg_gc,
					 date_x - x,
					 3 - y,
					 layout);
			g_object_unref (layout);

			gdk_gc_set_clip_rectangle (fg_gc, NULL);

			/* Draw the lines down the left and right of the date cols. */
			if (day != 0) {
				cairo_save (cr);
				gdk_cairo_set_source_color (cr, &light);
				cairo_move_to (cr, day_view->day_offsets[day] - x,
						4 - y);
				cairo_line_to (cr, day_view->day_offsets[day] - x,
						item_height - 4 - y);
				cairo_stroke (cr);
				gdk_cairo_set_source_color (cr, &dark);
				cairo_move_to (cr, day_view->day_offsets[day] - 1 - x,
						4 - y);
				cairo_line_to (cr, day_view->day_offsets[day] - 1 - x,
						item_height - 4 - y);
				cairo_stroke (cr);
				cairo_restore (cr);
			}

			/* Draw the lines between each column. */
			if (day != 0) {
				cairo_save (cr);
				gdk_cairo_set_source_color (
					cr, &day_view->colors
					[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_GRID]);
				cairo_move_to (cr, day_view->day_offsets[day] - x,
						item_height - y);
				cairo_line_to (cr, day_view->day_offsets[day] - x,
						canvas_height - y);
				cairo_stroke (cr);
				cairo_restore (cr);
			}
		}
	}

	if (!show_dates) {
		/* Draw the long events. */
		for (event_num = 0; event_num < day_view->long_events->len; event_num++) {
			day_view_top_item_draw_long_event (
				top_item, event_num, drawable,
				x, y, width, height);
		}
	}

	cairo_destroy (cr);
}

static double
day_view_top_item_point (GnomeCanvasItem *item,
                         gdouble x,
                         gdouble y,
                         gint cx,
                         gint cy,
                         GnomeCanvasItem **actual_item)
{
	/* This is supposed to return the nearest item the the point
	 * and the distance.  Since we are the only item we just return
	 * ourself and 0 for the distance.  This is needed so that we
	 * get button/motion events. */
	*actual_item = item;

	return 0.0;
}

static void
day_view_top_item_class_init (EDayViewTopItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EDayViewTopItemPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = day_view_top_item_set_property;
	object_class->get_property = day_view_top_item_get_property;
	object_class->dispose = day_view_top_item_dispose;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = day_view_top_item_update;
	item_class->draw = day_view_top_item_draw;
	item_class->point = day_view_top_item_point;

	g_object_class_install_property (
		object_class,
		PROP_DAY_VIEW,
		g_param_spec_object (
			"day_view",
			"Day View",
			NULL,
			E_TYPE_DAY_VIEW,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DATES,
		g_param_spec_boolean (
			"show_dates",
			"Show Dates",
			NULL,
			TRUE,
			G_PARAM_READWRITE));
}

static void
day_view_top_item_init (EDayViewTopItem *top_item)
{
	top_item->priv = E_DAY_VIEW_TOP_ITEM_GET_PRIVATE (top_item);
}

GType
e_day_view_top_item_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EDayViewTopItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) day_view_top_item_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EDayViewTopItem),
			0,     /* n_preallocs */
			(GInstanceInitFunc) day_view_top_item_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GNOME_TYPE_CANVAS_ITEM, "EDayViewTopItem",
			&type_info, 0);
	}

	return type;
}

void
e_day_view_top_item_get_day_label (EDayView *day_view, gint day,
				   gchar *buffer, gint buffer_len)
{
	struct icaltimetype day_start_tt;
	struct tm day_start = { 0 };
	const gchar *format;

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

EDayView *
e_day_view_top_item_get_day_view (EDayViewTopItem *top_item)
{
	g_return_val_if_fail (E_IS_DAY_VIEW_TOP_ITEM (top_item), NULL);

	return top_item->priv->day_view;
}

void
e_day_view_top_item_set_day_view (EDayViewTopItem *top_item,
                                  EDayView *day_view)
{
	g_return_if_fail (E_IS_DAY_VIEW_TOP_ITEM (top_item));
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (top_item->priv->day_view != NULL)
		g_object_unref (top_item->priv->day_view);

	top_item->priv->day_view = g_object_ref (day_view);

	g_object_notify (G_OBJECT (top_item), "day-view");
}

gboolean
e_day_view_top_item_get_show_dates (EDayViewTopItem *top_item)
{
	g_return_val_if_fail (E_IS_DAY_VIEW_TOP_ITEM (top_item), FALSE);

	return top_item->priv->show_dates;
}

void
e_day_view_top_item_set_show_dates (EDayViewTopItem *top_item,
                                    gboolean show_dates)
{
	g_return_if_fail (E_IS_DAY_VIEW_TOP_ITEM (top_item));

	top_item->priv->show_dates = show_dates;

	g_object_notify (G_OBJECT (top_item), "show-dates");
}

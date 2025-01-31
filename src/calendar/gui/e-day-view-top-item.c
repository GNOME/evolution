/*
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

/*
 * EDayViewTopItem - displays the top part of the Day/Work Week calendar view.
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include "e-calendar-view.h"
#include "e-day-view-top-item.h"

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

G_DEFINE_TYPE_WITH_PRIVATE (EDayViewTopItem, e_day_view_top_item, GNOME_TYPE_CANVAS_ITEM)

/* This draws a little triangle to indicate that an event extends past
 * the days visible on screen. */
static void
day_view_top_item_draw_triangle (EDayViewTopItem *top_item,
                                 cairo_t *cr,
                                 gint x,
                                 gint y,
                                 gint w,
                                 gint h,
                                 gint event_num)
{
	EDayView *day_view;
	EDayViewEvent *event;
	GdkRGBA bg_color;
	GdkPoint points[3];
	gint c1, c2;

	day_view = e_day_view_top_item_get_day_view (top_item);

	points[0].x = x;
	points[0].y = y;
	points[1].x = x + w;
	points[1].y = y + (h / 2);
	points[2].x = x;
	points[2].y = y + h - 1;

	/* If the height is odd we can use the same central point for both
	 * lines. If it is even we use different end-points. */
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
	if (e_cal_model_get_rgba_for_component (
		e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)),
		event->comp_data, &bg_color)) {
		gdk_cairo_set_source_rgba (cr, &bg_color);
	} else {
		gdk_cairo_set_source_rgba (
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
	gdk_cairo_set_source_rgba (
		cr, &day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BORDER]);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, x + w, c1);
	cairo_move_to (cr, x, y + h - 1);
	cairo_line_to (cr, x + w, c2);
	cairo_stroke (cr);
	cairo_restore (cr);
}

/* This draws one event in the top canvas. */
static void
day_view_top_item_draw_long_event (EDayViewTopItem *top_item,
                                   gint event_num,
                                   cairo_t *cr,
                                   gint x,
                                   gint y,
                                   gint width,
                                   gint height)
{
	EDayView *day_view;
	EDayViewEvent *event;
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
	GSList *categories_list, *elem;
	PangoLayout *layout;
	GdkRGBA bg_rgba, rgba;
	cairo_pattern_t *pat;
	gdouble x0, y0, rect_height, rect_width, radius, x_offset = 0.0;
	gboolean draw_flat_events;

	day_view = e_day_view_top_item_get_day_view (top_item);
	draw_flat_events = e_day_view_get_draw_flat_events (day_view);
	model = e_calendar_view_get_model (E_CALENDAR_VIEW (day_view));

	/* If the event is currently being dragged, don't draw it. It will
	 * be drawn in the special drag items. */
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

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	if (!e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data, &bg_rgba)) {
		bg_rgba = day_view->colors[E_DAY_VIEW_COLOR_LONG_EVENT_BACKGROUND];
	}

	if (draw_flat_events) {
		x0 = item_x - x;
		y0 = item_y - y + 2;
		rect_width = item_w;
		rect_height = item_h - 2;

		cairo_save (cr);
		cairo_rectangle (cr, x0, y0, rect_width, rect_height);
		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_fill (cr);
		cairo_restore (cr);
	} else {
		/* Fill the background with white to play with transparency */
		cairo_save (cr);

		x0 = item_x - x + 4;
		y0 = item_y + 1 - y;
		rect_width = item_w - 8;
		rect_height = item_h - 2;

		radius = 12;

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
		cairo_fill_preserve (cr);

		cairo_restore (cr);

		/* Draw the border around the event */

		cairo_save (cr);
		x0 = item_x - x + 4;
		y0 = item_y + 1 - y;
		rect_width = item_w - 8;
		rect_height = item_h - 2;

		radius = 12;

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_set_line_width (cr, 1.5);
		cairo_stroke (cr);
		cairo_restore (cr);

		/* Fill in with gradient */

		cairo_save (cr);

		x0 = item_x - x + 5.5;
		y0 = item_y + 2.5 - y;
		rect_width = item_w - 11;
		rect_height = item_h - 5;

		radius = 10;

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		pat = cairo_pattern_create_linear (
			item_x - x + 5.5, item_y + 2.5 - y,
			item_x - x + 5, item_y - y + item_h + 7.5);
		cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.4 * bg_rgba.alpha);
		cairo_set_source (cr, pat);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pat);

		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_set_line_width (cr, 0.5);
		cairo_stroke (cr);
		cairo_restore (cr);
	}

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
			top_item, cr, item_x - x, item_y - y + 2,
			-E_DAY_VIEW_BAR_WIDTH, item_h - 1, event_num);
	}

	/* Similar for the event end. */
	if (draw_end_triangle
	    && event->end > day_view->day_starts[end_day + 1]) {
		day_view_top_item_draw_triangle (
			top_item, cr, item_x + item_w - x,
			item_y - y + 2, E_DAY_VIEW_BAR_WIDTH, item_h - 1,
			event_num);
	}

	/* If we are editing the event we don't show the icons or the start
	 * & end times. */
	if (day_view->editing_event_day == E_DAY_VIEW_LONG_EVENT
	    && day_view->editing_event_num == event_num) {
		g_object_unref (comp);
		return;
	}

	g_object_get (G_OBJECT (event->canvas_item), "x_offset", &x_offset, NULL);

	/* Determine the position of the label, so we know where to place the
	 * icons. Note that since the top canvas never scrolls we don't need
	 * to take the scroll offset into account. It will always be 0. */
	text_x = event->canvas_item->x1 + x_offset;

	/* Draw the start & end times, if necessary. */
	min_end_time_x = item_x + E_DAY_VIEW_LONG_EVENT_X_PAD - x;

	time_width = e_day_view_get_time_string_width (day_view);

	rgba = e_utils_get_text_color_for_background (&bg_rgba);
	gdk_cairo_set_source_rgba (cr, &rgba);

	if (event->start > day_view->day_starts[start_day]) {
		offset = day_view->first_hour_shown * 60
			+ day_view->first_minute_shown + event->start_minute;
		hour = offset / 60;
		minute = offset % 60;
		/* Calculate the actual hour number to display. For 12-hour
		 * format we convert 0-23 to 12-11am/12-11pm. */
		e_day_view_convert_time_to_display (
			day_view, hour,
			&display_hour,
			&suffix, &suffix_width);
		if (e_cal_model_get_use_24_hour_format (model)) {
			g_snprintf (
				buffer, sizeof (buffer), "%i:%02i",
				display_hour, minute);
		} else {
			g_snprintf (
				buffer, sizeof (buffer), "%i:%02i%s",
				display_hour, minute, suffix);
		}

		cairo_save (cr);

		cairo_rectangle (
			cr,
			item_x - x, item_y - y,
			item_w - E_DAY_VIEW_LONG_EVENT_BORDER_WIDTH, item_h);
		cairo_clip (cr);

		time_x = item_x + E_DAY_VIEW_LONG_EVENT_X_PAD - x;
		if (display_hour < 10)
			time_x += day_view->digit_width;

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
		cairo_move_to (
			cr,
			time_x,
			item_y + E_DAY_VIEW_LONG_EVENT_BORDER_HEIGHT +
			E_DAY_VIEW_LONG_EVENT_Y_PAD - y);
		pango_cairo_show_layout (cr, layout);
		g_object_unref (layout);

		cairo_restore (cr);

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
			e_day_view_convert_time_to_display (
				day_view, hour,
				&display_hour,
				&suffix,
				&suffix_width);
			if (e_cal_model_get_use_24_hour_format (model)) {
				g_snprintf (
					buffer, sizeof (buffer),
					"%i:%02i", display_hour, minute);
			} else {
				g_snprintf (
					buffer, sizeof (buffer),
					"%i:%02i%s", display_hour, minute,
					suffix);
			}

			if (display_hour < 10)
				time_x += day_view->digit_width;

			layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), buffer);
			cairo_move_to (
				cr,
				time_x,
				item_y + E_DAY_VIEW_LONG_EVENT_Y_PAD + 1 - y);
			pango_cairo_show_layout (cr, layout);
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
		gdk_cairo_set_source_pixbuf (cr,
			e_cal_component_has_recurrences (comp) ? day_view->recurrence_icon : day_view->detached_recurrence_icon, icon_x, icon_y);
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
	categories_list = e_cal_component_get_categories_list (comp);
	for (elem = categories_list; elem; elem = elem->next) {
		const gchar *category;
		GdkPixbuf *pixbuf = NULL;

		category = (gchar *) elem->data;
		if (!e_categories_config_get_icon_for (category, &pixbuf))
			continue;

		if (icon_x <= max_icon_x) {
			gdk_cairo_set_source_pixbuf (
				cr, pixbuf,
				icon_x, icon_y);
			cairo_rectangle (
				cr,
				icon_x, icon_y,
				E_DAY_VIEW_ICON_WIDTH,
				E_DAY_VIEW_ICON_HEIGHT);
			cairo_fill (cr);
			icon_x -= icon_x_inc;

			g_clear_object (&pixbuf);
		} else {
			g_clear_object (&pixbuf);
			break;
		}
	}

	g_slist_free_full (categories_list, g_free);
	g_object_unref (comp);
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
	EDayViewTopItem *self = E_DAY_VIEW_TOP_ITEM (object);

	g_clear_object (&self->priv->day_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_day_view_top_item_parent_class)->dispose (object);
}

static void
day_view_top_item_update (GnomeCanvasItem *item,
                          const cairo_matrix_t *i2c,
                          gint flags)
{
	GnomeCanvasItemClass *canvas_item_class;

	/* Chain up to parent's update() method. */
	canvas_item_class =
		GNOME_CANVAS_ITEM_CLASS (e_day_view_top_item_parent_class);
	canvas_item_class->update (item, i2c, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}

static void
day_view_top_item_draw (GnomeCanvasItem *canvas_item,
                        cairo_t *cr,
                        gint x,
                        gint y,
                        gint width,
                        gint height)
{
	EDayViewTopItem *top_item;
	EDayView *day_view;
	gchar buffer[128];
	GtkAllocation allocation;
	GdkRectangle clip_rect;
	gint canvas_width, canvas_height, left_edge, day, date_width, date_x;
	gint item_height, event_num;
	PangoLayout *layout;
	GdkRGBA bg, fg, light, dark;
	gboolean show_dates;

	top_item = E_DAY_VIEW_TOP_ITEM (canvas_item);
	day_view = e_day_view_top_item_get_day_view (top_item);
	g_return_if_fail (day_view != NULL);
	show_dates = top_item->priv->show_dates;

	gtk_widget_get_allocation (
		GTK_WIDGET (canvas_item->canvas), &allocation);
	canvas_width = allocation.width;
	canvas_height =
		(show_dates ? 1 :
		(MAX (1, day_view->rows_in_top_display) + 1)) *
		day_view->top_row_height;
	left_edge = 0;
	item_height = day_view->top_row_height - E_DAY_VIEW_TOP_CANVAS_Y_GAP;

	e_utils_get_theme_color (GTK_WIDGET (day_view), "theme_bg_color", E_UTILS_DEFAULT_THEME_BG_COLOR, &bg);
	e_utils_get_theme_color (GTK_WIDGET (day_view), "theme_fg_color", E_UTILS_DEFAULT_THEME_FG_COLOR, &fg);
	e_utils_shade_color (&bg, &light, E_UTILS_LIGHTNESS_MULT);
	e_utils_shade_color (&bg, &dark, E_UTILS_DARKNESS_MULT);

	if (show_dates) {
		/* Draw the shadow around the dates. */
		cairo_save (cr);
		gdk_cairo_set_source_rgba (cr, &light);
		cairo_move_to (cr, left_edge - x, 1 - y);
		cairo_line_to (cr, canvas_width - 2 - x, 1 - y);
		cairo_move_to (cr, left_edge - x, 2 - y);
		cairo_line_to (cr, left_edge - x, item_height - 2 - y);
		cairo_stroke (cr);
		cairo_restore (cr);

		cairo_save (cr);
		gdk_cairo_set_source_rgba (cr, &dark);
		cairo_move_to (cr, left_edge - x, item_height - 1 - y);
		cairo_line_to (cr, canvas_width - 1 - x, item_height - 1 - y);
		cairo_move_to (cr, canvas_width - 1 - x, 1 - y);
		cairo_line_to (cr, canvas_width - 1 - x, item_height - 1 - y);
		cairo_stroke (cr);
		cairo_restore (cr);

		/* Draw the background for the dates. */
		cairo_save (cr);
		gdk_cairo_set_source_rgba (cr, &bg);
		cairo_rectangle (
			cr, left_edge + 2 - x, 2 - y,
			canvas_width - left_edge - 3,
			item_height - 3);
		cairo_fill (cr);
		cairo_restore (cr);
	}

	if (!show_dates) {
		/* Clear the main area background. */
		cairo_save (cr);
		gdk_cairo_set_source_rgba (
			cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_TOP_CANVAS]);
		cairo_rectangle (
			cr, left_edge - x, - y,
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
				gdk_cairo_set_source_rgba (
					cr, &day_view->colors
					[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_SELECTED]);
				cairo_rectangle (
					cr, rect_x - x, rect_y - y,
					rect_w, rect_h);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		}
	}

	if (show_dates) {
		gint days_shown;

		days_shown = e_day_view_get_days_shown (day_view);

		/* Draw the date. Set a clipping rectangle
		 * so we don't draw over the next day. */
		for (day = 0; day < days_shown; day++) {
			e_day_view_top_item_get_day_label (
				day_view, day, buffer, sizeof (buffer));
			clip_rect.x = day_view->day_offsets[day] - x;
			clip_rect.y = 2 - y;
			if (days_shown == 1) {
				gtk_widget_get_allocation (
					day_view->top_canvas, &allocation);
				clip_rect.width =
					allocation.width -
					day_view->day_offsets[day];
			} else
				clip_rect.width = day_view->day_widths[day];
			clip_rect.height = item_height - 2;

			cairo_save (cr);

			gdk_cairo_rectangle (cr, &clip_rect);
			cairo_clip (cr);

			layout = gtk_widget_create_pango_layout (
				GTK_WIDGET (day_view), buffer);
			pango_layout_get_pixel_size (layout, &date_width, NULL);
			date_x = day_view->day_offsets[day] +
				(clip_rect.width - date_width) / 2;

			gdk_cairo_set_source_rgba (cr, &fg);
			cairo_move_to (cr, date_x - x, 3 - y);
			pango_cairo_show_layout (cr, layout);

			g_object_unref (layout);
			cairo_restore (cr);

			/* Draw the lines down the left and right of the date cols. */
			if (day != 0) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &light);
				cairo_move_to (
					cr, day_view->day_offsets[day] - x,
					4 - y);
				cairo_line_to (
					cr, day_view->day_offsets[day] - x,
					item_height - 4 - y);
				cairo_stroke (cr);
				gdk_cairo_set_source_rgba (cr, &dark);
				cairo_move_to (
					cr, day_view->day_offsets[day] - 1 - x,
					4 - y);
				cairo_line_to (
					cr, day_view->day_offsets[day] - 1 - x,
					item_height - 4 - y);
				cairo_stroke (cr);
				cairo_restore (cr);
			}

			/* Draw the lines between each column. */
			if (day != 0) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (
					cr, &day_view->colors
					[E_DAY_VIEW_COLOR_BG_TOP_CANVAS_GRID]);
				cairo_move_to (
					cr, day_view->day_offsets[day] - x,
					item_height - y);
				cairo_line_to (
					cr, day_view->day_offsets[day] - x,
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
				top_item, event_num, cr,
				x, y, width, height);
		}
	}
}

static GnomeCanvasItem *
day_view_top_item_point (GnomeCanvasItem *item,
                         gdouble x,
                         gdouble y,
                         gint cx,
                         gint cy)
{
	return item;
}

static void
e_day_view_top_item_class_init (EDayViewTopItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

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
e_day_view_top_item_init (EDayViewTopItem *top_item)
{
	top_item->priv = e_day_view_top_item_get_instance_private (top_item);
}

void
e_day_view_top_item_get_day_label (EDayView *day_view,
                                   gint day,
                                   gchar *buffer,
                                   gint buffer_len)
{
	ECalendarView *view;
	ICalTime *day_start_tt;
	ICalTimezone *zone;
	struct tm day_start;
	const gchar *format;

	view = E_CALENDAR_VIEW (day_view);
	zone = e_calendar_view_get_timezone (view);

	day_start_tt = i_cal_time_new_from_timet_with_zone (
		day_view->day_starts[day], FALSE, zone);
	day_start = e_cal_util_icaltime_to_tm (day_start_tt);
	g_clear_object (&day_start_tt);

	if (day_view->date_format == E_DAY_VIEW_DATE_FULL)
		/* strftime format %A = full weekday name, %d = day of month,
		 * %B = full month name. Don't use any other specifiers. */
		format = _("%A %d %B");
	else if (day_view->date_format == E_DAY_VIEW_DATE_ABBREVIATED)
		/* strftime format %a = abbreviated weekday name, %d = day of month,
		 * %b = abbreviated month name. Don't use any other specifiers.
		 * xgettext:no-c-format */
		format = _("%a %d %b");
	else if (day_view->date_format == E_DAY_VIEW_DATE_NO_WEEKDAY)
		/* strftime format %d = day of month, %b = abbreviated month name.
		 * Don't use any other specifiers.
		 * xgettext:no-c-format */
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

	if (top_item->priv->day_view == day_view)
		return;

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

	if (top_item->priv->show_dates == show_dates)
		return;

	top_item->priv->show_dates = show_dates;

	g_object_notify (G_OBJECT (top_item), "show-dates");
}

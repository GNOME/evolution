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
 * EDayViewMainItem - canvas item which displays most of the appointment
 * data in the main Day/Work Week display.
 */

#include "evolution-config.h"

#include "e-day-view-main-item.h"

#include "comp-util.h"
#include "e-calendar-view.h"
#include "e-day-view-layout.h"
#include "ea-calendar.h"

struct _EDayViewMainItemPrivate {
	EDayView *day_view;
};

enum {
	PROP_0,
	PROP_DAY_VIEW
};

G_DEFINE_TYPE_WITH_PRIVATE (EDayViewMainItem, e_day_view_main_item, GNOME_TYPE_CANVAS_ITEM)

static gboolean
can_draw_in_region (cairo_region_t *draw_region,
                    gint x,
                    gint y,
                    gint width,
                    gint height)
{
	GdkRectangle rect;

	g_return_val_if_fail (draw_region != NULL, FALSE);

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;

	return cairo_region_contains_rectangle (draw_region, &rect) !=
		CAIRO_REGION_OVERLAP_OUT;
}

static gboolean
icomp_is_transparent (ICalComponent *icomp)
{
	ICalProperty *transp_prop;
	ICalPropertyTransp transp = I_CAL_TRANSP_NONE;

	g_return_val_if_fail (icomp != NULL, TRUE);

	transp_prop = i_cal_component_get_first_property (icomp, I_CAL_TRANSP_PROPERTY);
	if (transp_prop) {
		transp = i_cal_property_get_transp (transp_prop);
		g_object_unref (transp_prop);
	}

	return transp_prop && (transp == I_CAL_TRANSP_TRANSPARENT || transp == I_CAL_TRANSP_TRANSPARENTNOCONFLICT);
}

static void
day_view_main_item_draw_long_events_in_vbars (EDayViewMainItem *main_item,
                                              cairo_t *cr,
                                              gint x,
                                              gint y,
                                              gint width,
                                              gint height,
                                              cairo_region_t *draw_region)
{
	EDayView *day_view;
	EDayViewEvent *event;
	ECalendarView *cal_view;
	gint time_divisions;
	gint event_num, start_day, end_day, day, bar_y1, bar_y2, grid_x;

	day_view = e_day_view_main_item_get_day_view (main_item);

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	for (event_num = 0; event_num < day_view->long_events->len; event_num++) {
		gboolean first = TRUE;
		event = &g_array_index (day_view->long_events, EDayViewEvent, event_num);

		if (!is_comp_data_valid (event))
			continue;

		/* If the event is TRANSPARENT, skip it. */
		if (icomp_is_transparent (event->comp_data->icalcomp)) {
			continue;
		}

		if (!e_day_view_find_long_event_days (
			event,
			e_day_view_get_days_shown (day_view),
			day_view->day_starts,
			&start_day, &end_day)) {
			continue;
		}

		for (day = start_day; day <= end_day; day++) {
			grid_x = day_view->day_offsets[day] + 1 - x;

			/* Skip if it isn't visible. */
			if (grid_x >= width
			    || grid_x + E_DAY_VIEW_BAR_WIDTH <= 0)
				continue;

			if (event->start <= day_view->day_starts[day]) {
				bar_y1 = 0;
			} else {
				bar_y1 = event->start_minute * day_view->row_height / time_divisions - y;
			}

			if (event->end >= day_view->day_starts[day + 1]) {
				bar_y2 = height;
			} else {
				bar_y2 = event->end_minute * day_view->row_height / time_divisions - y;
			}

			if (bar_y1 < height && bar_y2 > 0 && bar_y2 > bar_y1 && can_draw_in_region (draw_region, grid_x, bar_y1, E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1)) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND]);

				if (first) {
					GdkRGBA rgba;

					first = FALSE;

					if (e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data, &rgba)) {
						gdk_cairo_set_source_rgba (cr, &rgba);
					}
				}

				cairo_rectangle (cr, grid_x, bar_y1, E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		}
	}
}

static void
day_view_main_item_draw_day_event (EDayViewMainItem *main_item,
                                   cairo_t *cr,
                                   gint x,
                                   gint y,
                                   gint width,
                                   gint height,
                                   gint day,
                                   gint event_num,
                                   cairo_region_t *draw_region)
{
	EDayView *day_view;
	EDayViewEvent *event;
	ECalModel *model;
	ECalendarView *cal_view;
	gint time_divisions;
	gint item_x, item_y, item_w, item_h, bar_y1, bar_y2;
	GdkRGBA bg_rgba;
	ECalComponent *comp;
	gint num_icons, icon_x = 0, icon_y, icon_x_inc = 0, icon_y_inc = 0;
	gint max_icon_w, max_icon_h;
	gboolean draw_reminder_icon, draw_recurrence_icon, draw_detached_recurrence_icon, draw_timezone_icon, draw_meeting_icon;
	gboolean draw_attach_icon;
	ECalComponentTransparency transparency;
	cairo_pattern_t *pat;
	gint i;
	gdouble radius, x0, y0, rect_height, rect_width, text_x_offset = 0.0;
	gdouble date_fraction;
	gboolean short_event = FALSE, resize_flag = FALSE, is_editing;
	const gchar *end_resize_suffix;
	gint start_hour, start_display_hour, start_minute, start_suffix_width;
	gint end_hour, end_display_hour, end_minute, end_suffix_width;
	gboolean show_span = FALSE, format_time;
	gint offset, interval;
	const gchar *start_suffix;
	const gchar *end_suffix;
	gint scroll_flag = 0;
	gint row_y;
	PangoLayout *layout;
	gboolean draw_flat_events;

	day_view = e_day_view_main_item_get_day_view (main_item);
	draw_flat_events = e_day_view_get_draw_flat_events (day_view);

	cal_view = E_CALENDAR_VIEW (day_view);
	model = e_calendar_view_get_model (cal_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	/* If the event is currently being dragged, don't draw it. It will
	 * be drawn in the special drag items. */
	if (day_view->drag_event_day == day && day_view->drag_event_num == event_num)
		return;

	/* Get the position of the event. If it is not shown skip it.*/
	if (!e_day_view_get_event_position (day_view, day, event_num,
					    &item_x, &item_y,
					    &item_w, &item_h))
		return;

	item_x -= x;
	item_y -= y;

	if (!can_draw_in_region (draw_region, item_x, item_y, item_w, item_h))
		return;

	gdk_cairo_set_source_rgba (
		cr,
		&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	if (!is_array_index_in_bounds (day_view->events[day], event_num))
		return;

	event = &g_array_index (
		day_view->events[day], EDayViewEvent, event_num);

	if (!is_comp_data_valid (event))
		return;

	/* Fill in the event background. Note that for events in the first
	 * column of the day, we might not want to paint over the vertical bar,
	 * since that is used for multiple events. But then you can't see
	 * where the event in the first column finishes. The border is drawn
	 * along with the event using cairo */

	bg_rgba.red = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].red;
	bg_rgba.green = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].green;
	bg_rgba.blue = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].blue;
	bg_rgba.alpha = 1.0;

	if (!e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data, &bg_rgba)) {
		bg_rgba.red = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].red;
		bg_rgba.green = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].green;
		bg_rgba.blue = day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND].blue;
		bg_rgba.alpha = 1.0;
	}

	is_editing = day_view->editing_event_day == day && day_view->editing_event_num == event_num;

	if (event->canvas_item)
		g_object_get (event->canvas_item, "x_offset", &text_x_offset, NULL);

	/* Draw shadow around the event when selected */
	if (!draw_flat_events && is_editing && (gtk_widget_has_focus (day_view->main_canvas))) {
		/* For embossing Item selection */
		item_x -= 1;
		item_y -= 2;

		if (MAX (0, item_w - 31.5) != 0) {
			/* Vertical Line */
			cairo_save (cr);
			pat = cairo_pattern_create_linear (
				item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 6.5, item_y + 13.75,
				item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 13.75, item_y + 13.75);
			cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
			cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
			cairo_set_source (cr, pat);
			cairo_rectangle (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 6.5, item_y + 14.75, 7.0, item_h - 22.0);
			cairo_fill (cr);
			cairo_pattern_destroy (pat);

			/* Arc at the right */
			pat = cairo_pattern_create_radial (
				item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 3, item_y + 13.5, 5.0,
				item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 13.5, 12.0);
			cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
			cairo_pattern_add_color_stop_rgba (pat, 0.25, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
			cairo_set_source (cr, pat);
			cairo_arc (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 13.5, 8.0, 11 * G_PI / 8, G_PI / 8);
			cairo_fill (cr);
			cairo_pattern_destroy (pat);

			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_set_line_width (cr, 1.25);
			cairo_move_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 5, item_y + 9.5);
			cairo_line_to (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 9.5, item_y + 15);
			cairo_stroke (cr);

			/* Horizontal line */
			pat = cairo_pattern_create_linear (
				item_x + E_DAY_VIEW_BAR_WIDTH + 15, item_y + item_h,
				item_x + E_DAY_VIEW_BAR_WIDTH + 15, item_y + item_h + 7);
			cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
			cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
			cairo_set_source (cr, pat);
			cairo_rectangle (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 16.5, item_y + item_h, item_w - 31.5, 7.0);
			cairo_fill (cr);
			cairo_pattern_destroy (pat);

			/* Bottom arc */
			pat = cairo_pattern_create_radial (
				item_x + E_DAY_VIEW_BAR_WIDTH + 12.5, item_y + item_h - 5, 5.0,
				item_x + E_DAY_VIEW_BAR_WIDTH + 12.5, item_y + item_h - 5, 12.0);
			cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
			cairo_pattern_add_color_stop_rgba (pat, 0.7, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
			cairo_set_source (cr, pat);
			cairo_arc (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 13, item_y + item_h - 5, 12.0, 3 * G_PI / 8, 9 * G_PI / 8);
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
			pat = cairo_pattern_create_radial (
				item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 1.0,
						item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 12.0);
			cairo_pattern_add_color_stop_rgba (pat, 1, 1, 1, 1, 0.3);
			cairo_pattern_add_color_stop_rgba (pat, 0.8, 0, 0, 0, 0.2);
			cairo_pattern_add_color_stop_rgba (pat, 0, 0, 0, 0, 1);
			cairo_set_source (cr, pat);
			cairo_arc (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH + 1, item_y + item_h - 4.5, 12.0, 15 * G_PI / 8,  5 * G_PI / 8);
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
			x0 = item_x + E_DAY_VIEW_BAR_WIDTH + 9;
			y0 = item_y + 10;
			rect_width = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 7, 0);
			rect_height = item_h - 7;

			radius = 20;

			draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_fill (cr);
			cairo_restore (cr);

			/* Extra Grid lines when clicked */
			cairo_save (cr);

			x0 = item_x + E_DAY_VIEW_BAR_WIDTH + 1;
			y0 = item_y + 2;
			rect_width = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 3, 0);
			rect_height = item_h - 4.;

			radius = 16;

			draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

			cairo_set_source_rgb (cr, 1, 1, 1);
			cairo_fill (cr);

			gdk_cairo_set_source_rgba (
				cr,
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

		item_x += 1;
		item_y += 2;
	}

	if (!draw_flat_events) {
		/* Draw the background of the event with white to play with transparency */
		cairo_save (cr);

		x0 = item_x + E_DAY_VIEW_BAR_WIDTH + 1;
		y0 = item_y + 2;
		rect_width = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 3, 0);
		rect_height = item_h - 4.;

		radius = 16;

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);

		cairo_set_source_rgba (cr, 1, 1, 1, 1.0);
		cairo_fill (cr);

		cairo_restore (cr);

		/* Here we draw the border in event color */
		cairo_save (cr);

		x0 = item_x + E_DAY_VIEW_BAR_WIDTH;
		y0 = item_y + 1.;
		rect_width = MAX (item_w - E_DAY_VIEW_BAR_WIDTH - 1., 0);
		rect_height = item_h - 2.;

		radius = 16;

		draw_curved_rectangle (cr, x0, y0, rect_width,rect_height, radius);
		cairo_set_line_width (cr, 2.);
		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_stroke (cr);
		cairo_restore (cr);
	}

	/* Fill in the Event */

	cairo_save (cr);

	if (draw_flat_events) {
		x0 = item_x + E_DAY_VIEW_BAR_WIDTH;
		y0 = item_y;
		rect_width = item_w - E_DAY_VIEW_BAR_WIDTH;
		rect_height = item_h - 2;

		cairo_rectangle (cr, x0, y0, rect_width, rect_height);
		gdk_cairo_set_source_rgba (cr, &bg_rgba);
		cairo_fill (cr);
	} else {
		x0 = item_x + E_DAY_VIEW_BAR_WIDTH + 1.75;
		y0 = item_y + 2.75;
		rect_width = item_w - E_DAY_VIEW_BAR_WIDTH - 4.5;
		rect_height = item_h - 5.5;

		radius = 14;

		draw_curved_rectangle (cr, x0, y0, rect_width, rect_height, radius);
	}

	date_fraction = (day_view->row_height - 12) / (item_h - 15.5);
	interval = event->end_minute - event->start_minute;

	if ((interval / time_divisions) >= 2)
		short_event = FALSE;
	else if ((interval % time_divisions) == 0) {
		if (((event->end_minute % time_divisions) == 0) ||
		    ((event->start_minute % time_divisions) == 0))
			short_event = TRUE;
		}
	else
		short_event = FALSE;

	if (is_editing)
		short_event = TRUE;

	pat = cairo_pattern_create_linear (
		item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + 7.75,
		item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + item_h - 7.75);
	if (!short_event) {
		cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, date_fraction, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, date_fraction, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.4 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
	} else {
		cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.4 * bg_rgba.alpha);
	}
	cairo_set_source (cr, pat);
	cairo_fill_preserve (cr);
	cairo_pattern_destroy (pat);

	cairo_set_source_rgba (cr, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.2 * bg_rgba.alpha);
	cairo_set_line_width (cr, 0.5);
	cairo_stroke (cr);
	cairo_restore (cr);

	/* Draw the right edge of the vertical bar. */
	cairo_save (cr);
	gdk_cairo_set_source_rgba (
		cr,
		&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
	cairo_set_line_width (cr, 0.7);
	cairo_move_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, item_y + 1);
	cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, item_y + item_h - 2);
	cairo_stroke (cr);
	cairo_restore (cr);

	gdk_cairo_set_source_rgba (
		cr,
		&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Draw the vertical colored bar showing when the appointment
	 * begins & ends. */
	bar_y1 = event->start_minute * day_view->row_height / time_divisions - y;
	bar_y2 = event->end_minute * day_view->row_height / time_divisions - y;

	scroll_flag = bar_y2;

	/* When an item is being resized, we fill the bar up to the new row. */
	if (day_view->resize_drag_pos != E_CALENDAR_VIEW_POS_NONE
	    && day_view->resize_event_day == day
	    && day_view->resize_event_num == event_num) {
		resize_flag = TRUE;

		if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_TOP_EDGE)
			bar_y1 = item_y + 1;

		else if (day_view->resize_drag_pos == E_CALENDAR_VIEW_POS_BOTTOM_EDGE) {
			GdkRGBA fg_rgba;
			gchar *end_regsizeime;

			bar_y2 = item_y + item_h - 1;

			end_minute = event->end_minute;

			end_hour = end_minute / 60;
			end_minute = end_minute % 60;

			e_day_view_convert_time_to_display (
				day_view, end_hour,
				&end_display_hour,
				&end_resize_suffix,
				&end_suffix_width);

			cairo_save (cr);
			cairo_rectangle (
				cr, item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + 2.75,
				item_w - E_DAY_VIEW_BAR_WIDTH - 4.5,
				item_h - 5.5);
			cairo_clip (cr);
			cairo_new_path (cr);

			if (e_cal_model_get_use_24_hour_format (model)) {
				cairo_translate (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH - 35, item_y + item_h - 8 - 14);
				end_regsizeime = g_strdup_printf (
					"%2i:%02i",
					end_display_hour, end_minute);

			} else {
				cairo_translate (cr, item_x + item_w - E_DAY_VIEW_BAR_WIDTH - 51, item_y + item_h - 8 - 14);
				end_regsizeime = g_strdup_printf (
					"%2i:%02i%s",
					end_display_hour, end_minute,
					end_resize_suffix);
			}

			layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (main_item)->canvas), end_regsizeime);
			cairo_set_font_size (cr, 13);
			fg_rgba = e_utils_get_text_color_for_background (&bg_rgba);
			gdk_cairo_set_source_rgba (cr, &fg_rgba);
			pango_cairo_update_layout (cr, layout);
			pango_cairo_show_layout (cr, layout);
			g_object_unref (layout);
			g_free (end_regsizeime);

			cairo_close_path (cr);
			cairo_restore (cr);
		}
	} else if (bar_y2 > scroll_flag)
		event->end_minute += time_divisions;
	else if (bar_y2 < scroll_flag)
		event->end_minute -= time_divisions;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

	/* Only fill it in if the event isn't TRANSPARENT. */
	transparency = e_cal_component_get_transparency (comp);
	if (transparency != E_CAL_COMPONENT_TRANSP_TRANSPARENT) {
		cairo_save (cr);
		pat = cairo_pattern_create_linear (
			item_x + E_DAY_VIEW_BAR_WIDTH, item_y + 1,
			item_x + E_DAY_VIEW_BAR_WIDTH, item_y + item_h - 1);
		cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.7 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, 0.5, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.7 * bg_rgba.alpha);
		cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.2 * bg_rgba.alpha);

		cairo_rectangle (
			cr, item_x + 1, bar_y1,
			E_DAY_VIEW_BAR_WIDTH - 2, bar_y2 - bar_y1);

		cairo_set_source (cr, pat);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);
		cairo_restore (cr);

		/* This is for achieving the white stripes in vbar across event color */
		for (i = 0; i <= (bar_y2 - bar_y1); i+=4) {
			cairo_set_source_rgb (cr, 1, 1, 1);
			cairo_set_line_width (cr, 0.3);
			cairo_move_to (cr, item_x + 1, bar_y1 + i);
			cairo_line_to (cr, item_x + E_DAY_VIEW_BAR_WIDTH - 1, bar_y1 + i);
			cairo_stroke (cr);
		}
	}

	gdk_cairo_set_source_rgba (
		cr,
		&day_view->colors[E_DAY_VIEW_COLOR_EVENT_VBAR]);

	/* Draw the reminder & recurrence icons, if needed. */
	if (!resize_flag && (!is_editing || text_x_offset > E_DAY_VIEW_ICON_X_PAD)) {
		GSList *categories_pixbufs = NULL, *pixbufs;

		num_icons = 0;
		draw_reminder_icon = FALSE;
		draw_recurrence_icon = FALSE;
		draw_detached_recurrence_icon = FALSE;
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

		if (e_cal_component_has_recurrences (comp)) {
			draw_recurrence_icon = TRUE;
			num_icons++;
		} else if (e_cal_component_is_instance (comp)) {
			draw_detached_recurrence_icon = TRUE;
			num_icons++;
		}
		if (e_cal_component_has_attachments (comp)) {
			draw_attach_icon = TRUE;
			num_icons++;
		}
		/* If the DTSTART or DTEND are in a different timezone to our current
		 * timezone, we display the timezone icon. */
		if (event->different_timezone) {
			draw_timezone_icon = TRUE;
			num_icons++;
		}

		if (e_cal_component_has_attendees (comp)) {
			draw_meeting_icon = TRUE;
			num_icons++;
		}

		num_icons += cal_comp_util_get_n_icons (comp, &categories_pixbufs);

		if (num_icons != 0) {
			if (item_h >= (E_DAY_VIEW_ICON_HEIGHT + E_DAY_VIEW_ICON_Y_PAD) * num_icons) {
				icon_x_inc = 0;
				icon_y_inc = E_DAY_VIEW_ICON_HEIGHT
					+ E_DAY_VIEW_ICON_Y_PAD;
			} else {
				icon_x_inc = E_DAY_VIEW_ICON_WIDTH
					+ E_DAY_VIEW_ICON_X_PAD;
				icon_y_inc = 0;
			}

			#define fit_in_event() (icon_x + icon_x_inc < item_x + item_w && icon_y + icon_y_inc < item_y + item_h)
			#define draw_pixbuf(pf) \
				max_icon_w = item_x + item_w - icon_x - E_DAY_VIEW_EVENT_BORDER_WIDTH; \
				max_icon_h = item_y + item_h - icon_y - E_DAY_VIEW_EVENT_BORDER_HEIGHT; \
 \
				if (can_draw_in_region (draw_region, icon_x, icon_y, max_icon_w, max_icon_h)) { \
					cairo_save (cr); \
					cairo_rectangle (cr, icon_x, icon_y, max_icon_w, max_icon_h); \
					cairo_clip (cr); \
					cairo_new_path (cr); \
					gdk_cairo_set_source_pixbuf (cr, pf, icon_x, icon_y); \
					cairo_paint (cr); \
					cairo_close_path (cr); \
					cairo_restore (cr); \
				} \
 \
				icon_x += icon_x_inc; \
				icon_y += icon_y_inc;

			if (draw_reminder_icon && fit_in_event ()) {
				draw_pixbuf (day_view->reminder_icon);
			}

			if (draw_recurrence_icon && fit_in_event ()) {
				draw_pixbuf (day_view->recurrence_icon);
			}
			if (draw_detached_recurrence_icon && fit_in_event ()) {
				draw_pixbuf (day_view->detached_recurrence_icon);
			}
			if (draw_attach_icon && fit_in_event ()) {
				draw_pixbuf (day_view->attach_icon);
			}
			if (draw_timezone_icon && fit_in_event ()) {
				draw_pixbuf (day_view->timezone_icon);
			}

			if (draw_meeting_icon && fit_in_event ()) {
				draw_pixbuf (day_view->meeting_icon);
			}

			/* draw categories icons */
			for (pixbufs = categories_pixbufs;
			     pixbufs && fit_in_event ();
			     pixbufs = pixbufs->next) {
				GdkPixbuf *pixbuf = pixbufs->data;

				draw_pixbuf (pixbuf);
			}

			#undef draw_pixbuf
			#undef fit_in_event

			if (icon_x_inc != 0)
				icon_x += E_DAY_VIEW_ICON_X_PAD;
		}

		/* free memory */
		g_slist_foreach (categories_pixbufs, (GFunc) g_object_unref, NULL);
		g_slist_free (categories_pixbufs);
	}

	if (!short_event) {
		GdkRGBA fg_rgba;
		gchar *text;

		if (event->start_minute % time_divisions != 0
			|| (day_view->show_event_end_times
			&& event->end_minute % time_divisions != 0)) {
				offset = day_view->first_hour_shown * 60
				+ day_view->first_minute_shown;
				show_span = TRUE;
			} else {
				offset = 0;
		}
		start_minute = offset + event->start_minute;
		end_minute = offset + event->end_minute;

		format_time = (((end_minute - start_minute) / time_divisions) >= 2) ? TRUE : FALSE;

		start_hour = start_minute / 60;
		start_minute = start_minute % 60;

		end_hour = end_minute / 60;
		end_minute = end_minute % 60;

		e_day_view_convert_time_to_display (
			day_view, start_hour,
			&start_display_hour,
			&start_suffix,
			&start_suffix_width);
		e_day_view_convert_time_to_display (
			day_view, end_hour,
			&end_display_hour,
			&end_suffix,
			&end_suffix_width);

		if (e_cal_model_get_use_24_hour_format (model)) {
			if (day_view->show_event_end_times && show_span) {
				/* 24 hour format with end time. */
				text = g_strdup_printf
					("%2i:%02i-%2i:%02i",
					 start_display_hour, start_minute,
					 end_display_hour, end_minute);
			} else if (format_time) {
				/* 24 hour format without end time. */
				text = g_strdup_printf
					("%2i:%02i",
					 start_display_hour, start_minute);
			} else {
				text = NULL;
			}
		} else {
			if (day_view->show_event_end_times && show_span) {
				/* 12 hour format with end time. */
				text = g_strdup_printf
					("%2i:%02i%s-%2i:%02i%s",
					 start_display_hour, start_minute,
					 start_suffix,
					 end_display_hour, end_minute, end_suffix);
			} else {
				/* 12 hour format without end time. */
				text = g_strdup_printf
					("%2i:%02i%s",
					 start_display_hour, start_minute,
					 start_suffix);
			}
		}

		if (day_view->row_height > 0 && event->canvas_item && item_h / day_view->row_height < 2) {
			gchar *item_text = NULL;

			g_object_get (event->canvas_item, "text", &item_text, NULL);

			if (item_text && item_text[0] == ' ' && item_text[1] == '\n') {
				gchar *tmp;
				tmp = g_strconcat (text, " ", item_text + 2, NULL);
				g_free (text);
				text = tmp;
			}

			g_free (item_text);
		}
		cairo_save (cr);
		cairo_rectangle (
			cr, item_x + E_DAY_VIEW_BAR_WIDTH + 1.75, item_y + 2.75,
			item_w - E_DAY_VIEW_BAR_WIDTH - 4.5,
			day_view->row_height);

		cairo_clip (cr);
		cairo_new_path (cr);

		if (icon_x_inc == 0)
			icon_x += 14;

		fg_rgba = e_utils_get_text_color_for_background (&bg_rgba);
		gdk_cairo_set_source_rgba (cr, &fg_rgba);

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (main_item)->canvas), text);
		if (resize_flag)
			cairo_translate (cr, item_x + E_DAY_VIEW_BAR_WIDTH + 10, item_y + 1);
		else
			cairo_translate (cr, icon_x, item_y + 1);
		cairo_set_font_size (cr, 13.0);
		pango_cairo_update_layout (cr, layout);
		pango_cairo_show_layout (cr, layout);
		g_object_unref (layout);
		g_free (text);

		cairo_close_path (cr);
		cairo_restore (cr);
	}

	g_object_unref (comp);
}

static void
day_view_main_item_draw_day_events (EDayViewMainItem *main_item,
                                    cairo_t *cr,
                                    gint x,
                                    gint y,
                                    gint width,
                                    gint height,
                                    gint day,
                                    cairo_region_t *draw_region)
{
	EDayView *day_view;
	gint event_num;

	day_view = e_day_view_main_item_get_day_view (main_item);

	for (event_num = 0; event_num < day_view->events[day]->len;
	     event_num++) {
		day_view_main_item_draw_day_event (
			main_item, cr, x, y, width, height,
			day, event_num, draw_region);
	}
}

static void
day_view_main_item_draw_events_in_vbars (EDayViewMainItem *main_item,
                                         cairo_t *cr,
                                         gint x,
                                         gint y,
                                         gint width,
                                         gint height,
                                         gint day,
                                         cairo_region_t *draw_region)
{
	EDayView *day_view;
	EDayViewEvent *event;
	ECalendarView *cal_view;
	gint time_divisions;
	gint grid_x, event_num, bar_y, bar_h;
	GdkRGBA bg_rgba;

	day_view = e_day_view_main_item_get_day_view (main_item);

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	grid_x = day_view->day_offsets[day] + 1 - x;

	/* Draw the busy times corresponding to the events in the day. */
	for (event_num = 0; event_num < day_view->events[day]->len; event_num++) {
		event = &g_array_index (day_view->events[day], EDayViewEvent, event_num);

		if (!is_comp_data_valid (event))
			continue;

		/* We can skip the events in the first column since they will
		 * draw over this anyway. */
		if (event->num_columns > 0 && event->start_row_or_col == 0) {
			continue;
		}

		bar_y = event->start_minute * day_view->row_height / time_divisions;
		bar_h = event->end_minute * day_view->row_height / time_divisions - bar_y;
		bar_y -= y;

		/* Skip it if it isn't visible. */
		if (bar_y >= height || bar_y + bar_h <= 0 || !can_draw_in_region (draw_region, grid_x, bar_y, E_DAY_VIEW_BAR_WIDTH - 2, bar_h)) {
			continue;
		}

		/* If the event is TRANSPARENT, skip it. */
		if (icomp_is_transparent (event->comp_data->icalcomp)) {
			continue;
		}

		cairo_save (cr);

		gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_EVENT_BACKGROUND]);

		if (e_cal_model_get_rgba_for_component (e_calendar_view_get_model (E_CALENDAR_VIEW (day_view)), event->comp_data, &bg_rgba)) {
			gdk_cairo_set_source_rgba (cr, &bg_rgba);
		}

		cairo_rectangle (cr, grid_x, bar_y, E_DAY_VIEW_BAR_WIDTH - 2, bar_h);

		cairo_fill (cr);

		cairo_restore (cr);
	}
}

static void
day_view_main_item_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			e_day_view_main_item_set_day_view (
				E_DAY_VIEW_MAIN_ITEM (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_main_item_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DAY_VIEW:
			g_value_set_object (
				value, e_day_view_main_item_get_day_view (
				E_DAY_VIEW_MAIN_ITEM (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
day_view_main_item_dispose (GObject *object)
{
	EDayViewMainItem *self = E_DAY_VIEW_MAIN_ITEM (object);

	g_clear_object (&self->priv->day_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_day_view_main_item_parent_class)->dispose (object);
}

static void
day_view_main_item_update (GnomeCanvasItem *item,
                           const cairo_matrix_t *i2c,
                           gint flags)
{
	GnomeCanvasItemClass *canvas_item_class;

	/* Chain up to parent's update() method. */
	canvas_item_class =
		GNOME_CANVAS_ITEM_CLASS (e_day_view_main_item_parent_class);
	canvas_item_class->update (item, i2c, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}

static void
day_view_main_item_draw (GnomeCanvasItem *canvas_item,
                         cairo_t *cr,
                         gint x,
                         gint y,
                         gint width,
                         gint height)
{
	EDayViewMainItem *main_item;
	EDayView *day_view;
	ECalendarView *cal_view;
	ECalModel *model;
	gint time_divisions;
	gint row, row_y, grid_x1, grid_x2;
	gint day, grid_y1, grid_y2;
	gint day_x, day_w;
	gint start_row, end_row, rect_x, rect_y, rect_width, rect_height;
	gint days_shown;
	ICalTime *day_start_tt, *today_tt;
	ICalTimezone *zone;
	gboolean today = FALSE;
	cairo_region_t *draw_region;
	GdkRectangle rect;

	main_item = E_DAY_VIEW_MAIN_ITEM (canvas_item);
	day_view = e_day_view_main_item_get_day_view (main_item);
	g_return_if_fail (day_view != NULL);

	days_shown = e_day_view_get_days_shown (day_view);
	if (days_shown <= 0) {
		g_warn_if_reached ();
		return;
	}

	cal_view = E_CALENDAR_VIEW (day_view);
	time_divisions = e_calendar_view_get_time_divisions (cal_view);

	model = e_calendar_view_get_model (cal_view);

	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;
	if (rect.width > 0 && rect.height > 0)
		draw_region = cairo_region_create_rectangle (&rect);
	else
		draw_region = cairo_region_create ();

	/* Paint the background colors. */

	zone = e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view));
	today_tt = i_cal_time_new_current_with_zone (zone);

	for (day = 0; day < days_shown; day++) {
		GDateWeekday weekday;

		day_start_tt = i_cal_time_new_from_timet_with_zone (day_view->day_starts[day], FALSE, zone);

		switch (i_cal_time_day_of_week (day_start_tt)) {
			case 1:
				weekday = G_DATE_SUNDAY;
				break;
			case 2:
				weekday = G_DATE_MONDAY;
				break;
			case 3:
				weekday = G_DATE_TUESDAY;
				break;
			case 4:
				weekday = G_DATE_WEDNESDAY;
				break;
			case 5:
				weekday = G_DATE_THURSDAY;
				break;
			case 6:
				weekday = G_DATE_FRIDAY;
				break;
			case 7:
				weekday = G_DATE_SATURDAY;
				break;
			default:
				weekday = G_DATE_BAD_WEEKDAY;
				break;
		}

		day_x = day_view->day_offsets[day] - x;
		day_w = day_view->day_widths[day];

		if (e_cal_model_get_work_day (model, weekday)) {
			gint work_day_start_hour;
			gint work_day_start_minute;
			gint work_day_end_hour;
			gint work_day_end_minute;
			gint work_day_start_y, work_day_end_y;

			e_cal_model_get_work_day_range_for (model, weekday,
				&work_day_start_hour, &work_day_start_minute,
				&work_day_end_hour, &work_day_end_minute);

			work_day_start_y = e_day_view_convert_time_to_position (
				day_view, work_day_start_hour, work_day_start_minute) - y;
			work_day_end_y = e_day_view_convert_time_to_position (
				day_view, work_day_end_hour, work_day_end_minute) - y;

			if (can_draw_in_region (draw_region, day_x, 0 - y, day_w, work_day_start_y - (0 - y))) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
				cairo_set_line_width (cr, 0.5);
				cairo_rectangle (cr, day_x, 0 - y, day_w, work_day_start_y - (0 - y));
				cairo_fill (cr);
				cairo_restore (cr);
			}

			if (days_shown > 1) {
				/* Check if we are drawing today */
				today = i_cal_time_compare_date_only (day_start_tt, today_tt) == 0;
			} else {
				today = FALSE;
			}

			if (can_draw_in_region (draw_region, day_x, work_day_start_y, day_w, work_day_end_y - work_day_start_y)) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &day_view->colors[today ? E_DAY_VIEW_COLOR_BG_MULTIDAY_TODAY : E_DAY_VIEW_COLOR_BG_WORKING]);
				cairo_rectangle (cr, day_x, work_day_start_y, day_w, work_day_end_y - work_day_start_y);
				cairo_fill (cr);
				cairo_restore (cr);
			}

			if (can_draw_in_region (draw_region, day_x, work_day_end_y, day_w, height - work_day_end_y)) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
				cairo_rectangle (cr, day_x, work_day_end_y, day_w, height - work_day_end_y);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		} else if (can_draw_in_region (draw_region, day_x, 0, day_w, height)) {
			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_NOT_WORKING]);
			cairo_rectangle (cr, day_x, 0, day_w, height);
			cairo_fill (cr);
			cairo_restore (cr);
		}

		g_clear_object (&day_start_tt);
	}

	g_clear_object (&today_tt);

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

			if (can_draw_in_region (draw_region, rect_x, rect_y, rect_width, rect_height)) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_SELECTED]);
				cairo_rectangle (cr, rect_x, rect_y, rect_width, rect_height);
				cairo_fill (cr);
				cairo_restore (cr);
			}
		}
	}

	/* Drawing the horizontal grid lines. */
	grid_x1 = day_view->day_offsets[0] - x;
	grid_x2 = day_view->day_offsets[days_shown] - x;

	cairo_save (cr);
	gdk_cairo_set_source_rgba (
		cr,
		&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);

	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		if (row_y >= 0 && row_y < height) {
			cairo_set_line_width (cr, 0.5);
			cairo_move_to (cr, grid_x1, row_y + 0.5);
			cairo_line_to (cr, grid_x2, row_y + 0.5);
			cairo_stroke (cr);
		}
	}
	cairo_restore (cr);

	/* Draw the vertical bars down the left of each column. */
	grid_y1 = 0;
	grid_y2 = height;
	for (day = 0; day < days_shown; day++) {
		grid_x1 = day_view->day_offsets[day] - x;

		/* Skip if it isn't visible. */
		if (grid_x1 >= width || grid_x1 + E_DAY_VIEW_BAR_WIDTH <= 0)
			continue;
		cairo_save (cr);

		gdk_cairo_set_source_rgba (
			cr,
			&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);
		cairo_move_to (cr, grid_x1, grid_y1);
		cairo_line_to (cr, grid_x1, grid_y2);
		cairo_stroke (cr);

		gdk_cairo_set_source_rgba (
			cr,
			&day_view->colors[E_DAY_VIEW_COLOR_BG_GRID]);

		cairo_move_to (cr, grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y1);
		cairo_line_to (cr, grid_x1 + E_DAY_VIEW_BAR_WIDTH - 1, grid_y2);
		cairo_stroke (cr);

		gdk_cairo_set_source_rgba (cr, &day_view->colors[E_DAY_VIEW_COLOR_BG_WORKING]);

		cairo_rectangle (
			cr, grid_x1 + 1, grid_y1,
			E_DAY_VIEW_BAR_WIDTH - 2, grid_y2 - grid_y1);

		cairo_fill (cr);

		cairo_restore (cr);

		/* Fill in the bars when the user is busy. */
		day_view_main_item_draw_events_in_vbars (
			main_item, cr, x, y,
			width, height, day, draw_region);
	}

	/* Fill in the vertical bars corresponding to the busy times from the
	 * long events. */
	day_view_main_item_draw_long_events_in_vbars (
		main_item, cr, x, y, width, height, draw_region);

	/* Draw the event borders and backgrounds, and the vertical bars
	 * down the left edges. */
	for (day = 0; day < days_shown; day++)
		day_view_main_item_draw_day_events (
			main_item, cr, x, y,
			width, height, day, draw_region);

	if (e_day_view_marcus_bains_get_show_line (day_view)) {
		ICalTime *time_now, *day_start;
		const gchar *marcus_bains_day_view_color;
		gint marcus_bains_y;
		GdkRGBA mb_color;

		cairo_save (cr);
		gdk_cairo_set_source_rgba (
			cr,
			&day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE]);

		marcus_bains_day_view_color =
			e_day_view_marcus_bains_get_day_view_color (day_view);
		if (marcus_bains_day_view_color == NULL)
			marcus_bains_day_view_color = "";

		if (gdk_rgba_parse (&mb_color, marcus_bains_day_view_color))
			gdk_cairo_set_source_rgba (cr, &mb_color);

		time_now = i_cal_time_new_current_with_zone (zone);

		for (day = 0; day < days_shown; day++) {
			day_start = i_cal_time_new_from_timet_with_zone (day_view->day_starts[day], FALSE, zone);

			if (i_cal_time_compare_date_only (day_start, time_now) == 0) {
				grid_x1 = day_view->day_offsets[day] - x + E_DAY_VIEW_BAR_WIDTH;
				grid_x2 = day_view->day_offsets[day + 1] - x - 1;
				marcus_bains_y = (i_cal_time_get_hour (time_now) * 60 + i_cal_time_get_minute (time_now)) * day_view->row_height / time_divisions - y;
				cairo_set_line_width (cr, 1.5);
				cairo_move_to (cr, grid_x1, marcus_bains_y);
				cairo_line_to (cr, grid_x2, marcus_bains_y);
				cairo_stroke (cr);
			}

			g_clear_object (&day_start);
		}
		cairo_restore (cr);

		g_clear_object (&time_now);
	}
	cairo_region_destroy (draw_region);
}

static GnomeCanvasItem *
day_view_main_item_point (GnomeCanvasItem *item,
                          gdouble x,
                          gdouble y,
                          gint cx,
                          gint cy)
{
	return item;
}

static void
e_day_view_main_item_class_init (EDayViewMainItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = day_view_main_item_set_property;
	object_class->get_property = day_view_main_item_get_property;
	object_class->dispose = day_view_main_item_dispose;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = day_view_main_item_update;
	item_class->draw = day_view_main_item_draw;
	item_class->point = day_view_main_item_point;

	g_object_class_install_property (
		object_class,
		PROP_DAY_VIEW,
		g_param_spec_object (
			"day-view",
			"Day View",
			NULL,
			E_TYPE_DAY_VIEW,
			G_PARAM_READWRITE));

	/* init the accessibility support for e_day_view */
	e_day_view_main_item_a11y_init ();
}

static void
e_day_view_main_item_init (EDayViewMainItem *main_item)
{
	main_item->priv = e_day_view_main_item_get_instance_private (main_item);
}

EDayView *
e_day_view_main_item_get_day_view (EDayViewMainItem *main_item)
{
	g_return_val_if_fail (E_IS_DAY_VIEW_MAIN_ITEM (main_item), NULL);

	return main_item->priv->day_view;
}

void
e_day_view_main_item_set_day_view (EDayViewMainItem *main_item,
                                   EDayView *day_view)
{
	g_return_if_fail (E_IS_DAY_VIEW_MAIN_ITEM (main_item));
	g_return_if_fail (E_IS_DAY_VIEW (day_view));

	if (main_item->priv->day_view == day_view)
		return;

	if (main_item->priv->day_view != NULL)
		g_object_unref (main_item->priv->day_view);

	main_item->priv->day_view = g_object_ref (day_view);

	g_object_notify (G_OBJECT (main_item), "day-view");
}

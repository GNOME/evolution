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
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#include "evolution-config.h"

#include "e-week-view-event-item.h"

#include <gtk/gtk.h>

#include "e-calendar-view.h"
#include "comp-util.h"

struct _EWeekViewEventItemPrivate {
	/* The event index in the EWeekView events array. */
	gint event_num;

	/* The span index within the event. */
	gint span_num;
};

enum {
	PROP_0,
	PROP_EVENT_NUM,
	PROP_SPAN_NUM
};

G_DEFINE_TYPE_WITH_PRIVATE (EWeekViewEventItem, e_week_view_event_item, GNOME_TYPE_CANVAS_ITEM)

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

static ECalendarViewPosition
week_view_event_item_get_position (EWeekViewEventItem *event_item,
                                   gdouble x,
                                   gdouble y)
{
	EWeekView *week_view;
	GnomeCanvasItem *item;
	GtkWidget *parent;

	item = GNOME_CANVAS_ITEM (event_item);

	parent = gtk_widget_get_parent (GTK_WIDGET (item->canvas));
	g_return_val_if_fail (E_IS_WEEK_VIEW (parent), E_CALENDAR_VIEW_POS_NONE);

	week_view = E_WEEK_VIEW (parent);

	if (x < item->x1 + E_WEEK_VIEW_EVENT_L_PAD
	    || x >= item->x2 - E_WEEK_VIEW_EVENT_R_PAD)
		return E_CALENDAR_VIEW_POS_NONE;

	/* Support left/right edge for long events only. */
	if (!e_week_view_is_one_day_event (week_view, event_item->priv->event_num)) {
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

static gboolean
week_view_event_item_double_click (EWeekViewEventItem *event_item,
                                   GdkEvent *button_event)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	GnomeCanvasItem *item;
	GtkWidget *parent;

	item = GNOME_CANVAS_ITEM (event_item);

	parent = gtk_widget_get_parent (GTK_WIDGET (item->canvas));
	g_return_val_if_fail (E_IS_WEEK_VIEW (parent), FALSE);

	week_view = E_WEEK_VIEW (parent);

	if (!is_array_index_in_bounds (week_view->events, event_item->priv->event_num))
		return TRUE;

	event = &g_array_index (
		week_view->events, EWeekViewEvent,
		event_item->priv->event_num);

	if (!is_comp_data_valid (event))
		return TRUE;

	if (week_view->editing_event_num >= 0) {
		EWeekViewEvent *editing;

		if (!is_array_index_in_bounds (
			week_view->events, week_view->editing_event_num))
			return TRUE;

		editing = &g_array_index (
			week_view->events, EWeekViewEvent,
			week_view->editing_event_num);

		/* Do not call edit of the component, if double clicked
		 * on the component, which is not on the server. */
		if (editing && event &&
			editing->comp_data == event->comp_data &&
			is_comp_data_valid (editing) &&
			(!event->comp_data || event->comp_data->is_new_component))
			return TRUE;
	}

	e_week_view_stop_editing_event (week_view);

	e_calendar_view_edit_appointment (
		E_CALENDAR_VIEW (week_view),
		event->comp_data->client,
		event->comp_data->icalcomp, EDIT_EVENT_AUTODETECT);

	return TRUE;
}

static gboolean
week_view_event_item_button_press (EWeekViewEventItem *event_item,
                                   GdkEvent *button_event)
{
	EWeekView *week_view;
	ECalendarViewPosition pos;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	GnomeCanvasItem *item;
	GtkWidget *parent;
	guint event_button = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;

	gdk_event_get_button (button_event, &event_button);
	gdk_event_get_coords (button_event, &event_x_win, &event_y_win);

	item = GNOME_CANVAS_ITEM (event_item);

	parent = gtk_widget_get_parent (GTK_WIDGET (item->canvas));
	g_return_val_if_fail (E_IS_WEEK_VIEW (parent), FALSE);

	week_view = E_WEEK_VIEW (parent);

	if (!is_array_index_in_bounds (week_view->events, event_item->priv->event_num))
		return FALSE;

	event = &g_array_index (
		week_view->events, EWeekViewEvent,
		event_item->priv->event_num);

	if (!is_array_index_in_bounds (
		week_view->spans, event->spans_index +
		event_item->priv->span_num))
		return FALSE;

	span = &g_array_index (week_view->spans, EWeekViewEventSpan,
			       event->spans_index + event_item->priv->span_num);

	pos = week_view_event_item_get_position (
		event_item, event_x_win, event_y_win);
	if (pos == E_CALENDAR_VIEW_POS_NONE)
		return FALSE;

	if (event_button == 1) {
		week_view->pressed_event_num = event_item->priv->event_num;
		week_view->pressed_span_num = event_item->priv->span_num;

		/* Ignore clicks on the event while editing. */
		if (E_TEXT (span->text_item)->editing)
			return FALSE;

		/* Remember the item clicked and the mouse position,
		 * so we can start a drag if the mouse moves. */
		week_view->drag_event_x = event_x_win;
		week_view->drag_event_y = event_y_win;

		/* FIXME: Remember the day offset from the start of the event.
		 */

		return TRUE;

	} else if (event_button == 3) {
		if (!gtk_widget_has_focus (GTK_WIDGET (week_view))) {
			gtk_widget_grab_focus (GTK_WIDGET (week_view));
			if (week_view->event_destroyed) {
				week_view->event_destroyed = FALSE;
				return FALSE;
			}

		}

		e_week_view_set_selected_time_range_visible (
			week_view, event->start, event->end);

		e_week_view_show_popup_menu (
			week_view, button_event,
			event_item->priv->event_num);
		g_signal_stop_emission_by_name (
			item->canvas, "button_press_event");

		return TRUE;
	}

	return FALSE;
}

static gboolean
week_view_event_item_button_release (EWeekViewEventItem *event_item,
                                     GdkEvent *event)
{
	EWeekView *week_view;
	GnomeCanvasItem *item;
	GtkWidget *parent;

	item = GNOME_CANVAS_ITEM (event_item);

	parent = gtk_widget_get_parent (GTK_WIDGET (item->canvas));
	g_return_val_if_fail (E_IS_WEEK_VIEW (parent), FALSE);

	week_view = E_WEEK_VIEW (parent);

	if (week_view->pressed_event_num != -1
	    && week_view->pressed_event_num == event_item->priv->event_num
	    && week_view->pressed_span_num == event_item->priv->span_num) {
		e_week_view_start_editing_event (
			week_view,
			event_item->priv->event_num,
			event_item->priv->span_num,
			NULL);
		week_view->pressed_event_num = -1;
		return TRUE;
	}

	week_view->pressed_event_num = -1;

	return FALSE;
}

static void
week_view_draw_time (EWeekView *week_view,
		     GdkRGBA bg_rgba,
                     cairo_t *cr,
                     gint time_x,
                     gint time_y,
                     gint hour,
                     gint minute)
{
	ECalModel *model;
	gint hour_to_display, suffix_width;
	gint time_y_normal_font, time_y_small_font;
	const gchar *suffix;
	gchar buffer[128];
	PangoLayout *layout;
	PangoFontDescription *small_font_desc;
	PangoContext *pango_context;
	GdkRGBA fg_rgba;

	fg_rgba = e_utils_get_text_color_for_background (&bg_rgba);

	cairo_save (cr);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));

	small_font_desc = week_view->small_font_desc;

	gdk_cairo_set_source_rgba (cr, &fg_rgba);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), NULL);
	pango_context = gtk_widget_create_pango_context (GTK_WIDGET (week_view));

	time_y_normal_font = time_y_small_font = time_y;
	if (small_font_desc)
		time_y_small_font = time_y;

	e_week_view_convert_time_to_display (
		week_view, hour, &hour_to_display,
		&suffix, &suffix_width);

	if (week_view->use_small_font && week_view->small_font_desc) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_copy (pango_context_get_font_description (pango_context));

		g_snprintf (
			buffer, sizeof (buffer), "%2i:%02i",
			hour_to_display, minute);

		/* Draw the hour. */
		if (hour_to_display < 10) {
			pango_layout_set_text (layout, buffer + 1, 1);
			cairo_move_to (
				cr,
				time_x + week_view->digit_width,
				time_y_normal_font);
			pango_cairo_show_layout (cr, layout);
		} else {
			pango_layout_set_text (layout, buffer, 2);
			cairo_move_to (
				cr,
				time_x,
				time_y_normal_font);
			pango_cairo_show_layout (cr, layout);
		}

		time_x += week_view->digit_width * 2;

		/* Draw the start minute, in the small font. */
		pango_layout_set_font_description (layout, week_view->small_font_desc);
		pango_layout_set_text (layout, buffer + 3, 2);
		cairo_move_to (
			cr,
			time_x,
			time_y_small_font);
		pango_cairo_show_layout (cr, layout);

		pango_layout_set_font_description (layout, font_desc);

		time_x += week_view->small_digit_width * 2;

		/* Draw the 'am'/'pm' suffix, if 12-hour format. */
		if (!e_cal_model_get_use_24_hour_format (model)) {
			pango_layout_set_text (layout, suffix, -1);

			cairo_move_to (
				cr,
				time_x,
				time_y_normal_font);
			pango_cairo_show_layout (cr, layout);
		}

		pango_font_description_free (font_desc);
	} else {
		/* Draw the start time in one go. */
		g_snprintf (
			buffer, sizeof (buffer), "%2i:%02i%s",
			hour_to_display, minute, suffix);
		if (hour_to_display < 10) {
			pango_layout_set_text (layout, buffer + 1, -1);
			cairo_move_to (
				cr,
				time_x + week_view->digit_width,
				time_y_normal_font);
			pango_cairo_show_layout (cr, layout);
		} else {
			pango_layout_set_text (layout, buffer, -1);
			cairo_move_to (
				cr,
				time_x,
				time_y_normal_font);
			pango_cairo_show_layout (cr, layout);
		}

	}
	g_object_unref (pango_context);
	g_object_unref (layout);

	cairo_restore (cr);
}

static void
week_view_event_item_draw_icons (EWeekViewEventItem *event_item,
                                 cairo_t *cr,
                                 gint icon_x,
                                 gint icon_y,
                                 gint x2,
                                 gboolean right_align,
                                 cairo_region_t *draw_region)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	ECalComponent *comp;
	GnomeCanvas *canvas;
	GtkWidget *parent;
	gint num_icons = 0, icon_x_inc;
	gboolean draw_reminder_icon = FALSE, draw_recurrence_icon = FALSE, draw_detached_recurrence_icon = FALSE;
	gboolean draw_timezone_icon = FALSE, draw_attach_icon = FALSE;
	gboolean draw_meeting_icon = FALSE;
	GSList *categories_pixbufs = NULL, *pixbufs;

	canvas = GNOME_CANVAS_ITEM (event_item)->canvas;
	parent = gtk_widget_get_parent (GTK_WIDGET (canvas));
	week_view = E_WEEK_VIEW (parent);

	if (e_week_view_get_multi_week_view (week_view) &&
	    !e_week_view_get_show_icons_month_view (week_view))
		return;

	if (!is_array_index_in_bounds (week_view->events, event_item->priv->event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				event_item->priv->event_num);

	if (!is_comp_data_valid (event))
		return;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (event->comp_data->icalcomp));
	if (!comp)
		return;

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

	if (e_cal_component_has_attendees (comp)) {
		draw_meeting_icon = TRUE;
		num_icons++;
	}

	if (event->different_timezone) {
		draw_timezone_icon = TRUE;
		num_icons++;
	}

	num_icons += cal_comp_util_get_n_icons (comp, &categories_pixbufs);

	icon_x_inc = E_WEEK_VIEW_ICON_WIDTH + E_WEEK_VIEW_ICON_X_PAD;

	if (right_align)
		icon_x -= icon_x_inc * num_icons;

	#define draw_pixbuf(pf) \
		if (can_draw_in_region (draw_region, icon_x, icon_y, \
		    E_WEEK_VIEW_ICON_WIDTH, E_WEEK_VIEW_ICON_HEIGHT)) { \
			cairo_save (cr); \
			gdk_cairo_set_source_pixbuf (cr, pf, icon_x, icon_y); \
			cairo_paint (cr); \
			cairo_restore (cr); \
		} \
 \
		icon_x += icon_x_inc;

	if (draw_reminder_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->reminder_icon);
	}

	if (draw_attach_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->attach_icon);
	}

	if (draw_recurrence_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->recurrence_icon);
	}

	if (draw_detached_recurrence_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->detached_recurrence_icon);
	}

	if (draw_timezone_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->timezone_icon);
	}

	if (draw_meeting_icon && icon_x + E_WEEK_VIEW_ICON_WIDTH <= x2) {
		draw_pixbuf (week_view->meeting_icon);
	}

	/* draw categories icons */
	for (pixbufs = categories_pixbufs;
	     pixbufs;
	     pixbufs = pixbufs->next) {
		GdkPixbuf *pixbuf = pixbufs->data;

		draw_pixbuf (pixbuf);
	}

	#undef draw_pixbuf

	g_slist_foreach (categories_pixbufs, (GFunc) g_object_unref, NULL);
	g_slist_free (categories_pixbufs);

	g_object_unref (comp);
}

/* This draws a little triangle to indicate that an event extends past
 * the days visible on screen. */
static void
week_view_event_item_draw_triangle (EWeekViewEventItem *event_item,
                                    cairo_t *cr,
                                    GdkRGBA bg_rgba,
                                    gint x,
                                    gint y,
                                    gint w,
                                    gint h,
                                    cairo_region_t *draw_region)
{
	EWeekView *week_view;
	EWeekViewEvent *event;
	GnomeCanvas *canvas;
	GtkWidget *parent;
	GdkPoint points[3];
	gint c1, c2;

	if (!can_draw_in_region (draw_region, x, y, w, h))
		return;

	canvas = GNOME_CANVAS_ITEM (event_item)->canvas;
	parent = gtk_widget_get_parent (GTK_WIDGET (canvas));
	week_view = E_WEEK_VIEW (parent);

	if (!is_array_index_in_bounds (week_view->events, event_item->priv->event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				event_item->priv->event_num);

	if (!is_comp_data_valid (event))
		return;

	points[0].x = x;
	points[0].y = y;
	points[1].x = x + w;
	points[1].y = y + (h / 2);
	points[2].x = x;
	points[2].y = y + h - 1;

	gdk_cairo_set_source_rgba (cr, &bg_rgba);

	cairo_save (cr);
	cairo_set_line_width (cr, 0.7);
	cairo_move_to (cr, points[0].x, points[0].y);
	cairo_line_to (cr, points[1].x, points[1].y);
	cairo_line_to (cr, points[2].x, points[2].y);
	cairo_line_to (cr, points[0].x, points[0].y);
	cairo_fill (cr);
	cairo_restore (cr);

	cairo_save (cr);
	gdk_cairo_set_source_rgba (
		cr, &week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BORDER]);

	/* If the height is odd we can use the same central point for both
	 * lines. If it is even we use different end-points. */
	c1 = c2 = y + (h / 2);
	if (h % 2 == 0)
		c1--;

	cairo_set_line_width (cr, 0.7);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, x + w, c1);
	cairo_move_to (cr, x, y + h - 1);
	cairo_line_to (cr, x + w, c2);
	cairo_restore (cr);
}

static void
week_view_event_item_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EVENT_NUM:
			e_week_view_event_item_set_event_num (
				E_WEEK_VIEW_EVENT_ITEM (object),
				g_value_get_int (value));
			return;

		case PROP_SPAN_NUM:
			e_week_view_event_item_set_span_num (
				E_WEEK_VIEW_EVENT_ITEM (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_event_item_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EVENT_NUM:
			g_value_set_int (
				value,
				e_week_view_event_item_get_event_num (
				E_WEEK_VIEW_EVENT_ITEM (object)));
			return;

		case PROP_SPAN_NUM:
			g_value_set_int (
				value,
				e_week_view_event_item_get_span_num (
				E_WEEK_VIEW_EVENT_ITEM (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_event_item_update (GnomeCanvasItem *item,
                             const cairo_matrix_t *i2c,
                             gint flags)
{
	GnomeCanvasItemClass *canvas_item_class;
	EWeekViewEventItem *event_item;
	EWeekView *week_view;
	GtkWidget *parent;
	gint event_num, span_num;
	gint span_x, span_y, span_w;

	event_item = E_WEEK_VIEW_EVENT_ITEM (item);
	parent = gtk_widget_get_parent (GTK_WIDGET (item->canvas));
	g_return_if_fail (E_IS_WEEK_VIEW (parent));

	week_view = E_WEEK_VIEW (parent);

	/* Chain up to parent's update() method. */
	canvas_item_class =
		GNOME_CANVAS_ITEM_CLASS (e_week_view_event_item_parent_class);
	canvas_item_class->update (item, i2c, flags);

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;

	event_num = e_week_view_event_item_get_event_num (event_item);
	span_num = e_week_view_event_item_get_span_num (event_item);

	if (event_num != -1 && span_num != -1) {
		if (e_week_view_get_span_position (
			week_view, event_num, span_num,
			&span_x, &span_y, &span_w)) {
			item->x1 = span_x;
			item->y1 = span_y;
			item->x2 = span_x + span_w - 1;
			item->y2 = span_y + week_view->row_height - 1;
		}
	}
}

static void
week_view_event_item_draw (GnomeCanvasItem *canvas_item,
                           cairo_t *cr,
                           gint x,
                           gint y,
                           gint width,
                           gint height)
{
	EWeekViewEventItem *event_item;
	EWeekView *week_view;
	EWeekViewEvent *event;
	EWeekViewEventSpan *span;
	ECalModel *model;
	GtkWidget *parent;
	gint x1, y1, x2, y2, time_x, time_y;
	gint icon_x, icon_y, time_width, min_end_time_x, max_icon_x;
	gint rect_x, rect_w, rect_x2 = 0;
	gboolean one_day_event, editing_span = FALSE;
	gint start_hour, start_minute, end_hour, end_minute;
	gboolean draw_start, draw_end;
	gboolean draw_start_triangle = FALSE, draw_end_triangle = FALSE;
	GdkRGBA bg_rgba;
	cairo_pattern_t *pat;
	gdouble radius, cx0, cy0, rect_height, rect_width;
	cairo_region_t *draw_region;
	GdkRectangle rect;
	gboolean draw_flat_events;

	event_item = E_WEEK_VIEW_EVENT_ITEM (canvas_item);
	parent = gtk_widget_get_parent (GTK_WIDGET (canvas_item->canvas));
	g_return_if_fail (E_IS_WEEK_VIEW (parent));

	week_view = E_WEEK_VIEW (parent);

	if (event_item->priv->event_num == -1 || event_item->priv->span_num == -1)
		return;

	g_return_if_fail (event_item->priv->event_num < week_view->events->len);

	if (!is_array_index_in_bounds (week_view->events, event_item->priv->event_num))
		return;

	event = &g_array_index (week_view->events, EWeekViewEvent,
				event_item->priv->event_num);

	if (!is_comp_data_valid (event))
		return;

	g_return_if_fail (
		event->spans_index + event_item->priv->span_num <
		week_view->spans->len);

	if (!is_array_index_in_bounds (
		week_view->spans, event->spans_index +
		event_item->priv->span_num))
		return;

	span = &g_array_index (
		week_view->spans, EWeekViewEventSpan,
		event->spans_index + event_item->priv->span_num);

	x1 = canvas_item->x1 - x;
	y1 = canvas_item->y1 - y;
	x2 = canvas_item->x2 - x;
	y2 = canvas_item->y2 - y;

	if (x1 == x2 || y1 == y2)
		return;

	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;
	if (rect.width > 0 && rect.height > 0)
		draw_region = cairo_region_create_rectangle (&rect);
	else
		draw_region = cairo_region_create ();

	if (!can_draw_in_region (draw_region, x1, y1, x2 - x1, y2 - y1)) {
		cairo_region_destroy (draw_region);
		return;
	}

	draw_flat_events = e_week_view_get_draw_flat_events (week_view);

	icon_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT + E_WEEK_VIEW_ICON_Y_PAD;

	/* Get the start & end times in 24-hour format. */
	start_hour = event->start_minute / 60;
	start_minute = event->start_minute % 60;

	/* Modulo 24 because a midnight end time will be '24' */
	end_hour = (event->end_minute / 60) % 24;
	end_minute = event->end_minute % 60;

	time_y = y1 + E_WEEK_VIEW_EVENT_BORDER_HEIGHT
		+ E_WEEK_VIEW_EVENT_TEXT_Y_PAD;

	time_width = e_week_view_get_time_string_width (week_view);

	one_day_event = e_week_view_is_one_day_event (
		week_view, event_item->priv->event_num);

	model = e_calendar_view_get_model (E_CALENDAR_VIEW (week_view));

	if (!e_cal_model_get_rgba_for_component (model, event->comp_data, &bg_rgba)) {
		bg_rgba = week_view->colors[E_WEEK_VIEW_COLOR_EVENT_BACKGROUND];
	}

	if (one_day_event) {
		time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD + 1;
		rect_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
		rect_w = x2 - x1 - E_WEEK_VIEW_EVENT_L_PAD - E_WEEK_VIEW_EVENT_R_PAD + 1;

		cx0 = rect_x;
		cy0 = y1 + 1;
		rect_width = rect_w;
		rect_height = y2 - y1 - 1;

		if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
			if (draw_flat_events) {
				cairo_save (cr);
				cairo_rectangle (cr, cx0, cy0, rect_width, rect_height);
				gdk_cairo_set_source_rgba (cr, &bg_rgba);
				cairo_fill (cr);
				cairo_restore (cr);
			} else {
				/* Here we draw the border around the event*/

				radius = 12;

				if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
					cairo_save (cr);
					draw_curved_rectangle (cr, cx0, cy0, rect_width, rect_height, radius);
					cairo_set_line_width (cr, 2.0);
					gdk_cairo_set_source_rgba (cr, &bg_rgba);
					cairo_stroke (cr);
					cairo_restore (cr);
				}

				/* Fill it in the Event */

				cx0 = rect_x + 1.5;
				cy0 = y1 + 2.75;
				rect_width = rect_w - 3.;
				rect_height = y2 - y1 - 4.5;

				radius = 8;

				if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
					cairo_save (cr);
					draw_curved_rectangle (
						cr, cx0, cy0, rect_width, rect_height, radius);

					pat = cairo_pattern_create_linear (
						rect_x + 2, y1 + 1, rect_x + 2, y2 - 7.25);
					cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
					cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.4 * bg_rgba.alpha);
					cairo_set_source (cr, pat);
					cairo_fill_preserve (cr);
					cairo_pattern_destroy (pat);
					cairo_set_source_rgba (cr, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.2 * bg_rgba.alpha);
					cairo_set_line_width (cr, 0.5);
					cairo_stroke (cr);
					cairo_restore (cr);
				}
			}
		}

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
			g_warn_if_reached ();
			draw_start = FALSE;
			draw_end = FALSE;
			break;
		}

		if (draw_start) {
			week_view_draw_time (
				week_view, bg_rgba, cr, time_x,
				time_y, start_hour, start_minute);
			time_x += time_width;
		}

		if (draw_end && (!draw_start || event->start_minute != event->end_minute)) {
			time_x += E_WEEK_VIEW_EVENT_TIME_SPACING;
			week_view_draw_time (
				week_view, bg_rgba, cr, time_x,
				time_y, end_hour, end_minute);
			time_x += time_width;
		}

		icon_x = time_x;
		if (draw_start)
			icon_x += E_WEEK_VIEW_EVENT_TIME_X_PAD;

		/* Draw the icons. */
		week_view_event_item_draw_icons (
			event_item, cr, icon_x,
			icon_y, x2, FALSE, draw_region);

	} else {
		rect_x = x1 + E_WEEK_VIEW_EVENT_L_PAD;
		rect_w = x2 - x1 - E_WEEK_VIEW_EVENT_L_PAD
			- E_WEEK_VIEW_EVENT_R_PAD + 1;

		/* Draw the triangles at the start & end, if needed.
		 * They also use the first few pixels at the edge of the
		 * event so we update rect_x & rect_w so we don't draw over
		 * them. */
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

		cx0 = rect_x;
		cy0 = y1 + 1;
		rect_width = rect_w;
		rect_height = y2 - y1 - 1;

		if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
			if (draw_flat_events) {
				cairo_save (cr);
				gdk_cairo_set_source_rgba (cr, &bg_rgba);
				cairo_rectangle (cr, cx0, cy0, rect_width, rect_height);
				cairo_fill (cr);
				cairo_restore (cr);
			} else {

				/* Here we draw the border around the event */

				radius = 12;

				if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
					cairo_save (cr);
					draw_curved_rectangle (cr, cx0, cy0, rect_width, rect_height, radius);
					cairo_set_line_width (cr, 2.0);
					gdk_cairo_set_source_rgba (cr, &bg_rgba);
					cairo_stroke (cr);
					cairo_restore (cr);
				}

				/* Here we fill it in the event*/

				cx0 = rect_x + 1.5;
				cy0 = y1 + 2.75;
				rect_width = rect_w - 3.;
				rect_height = y2 - y1 - 4.5;

				radius = 8;

				if (can_draw_in_region (draw_region, cx0, cy0, rect_width, rect_height)) {
					cairo_save (cr);
					draw_curved_rectangle (
						cr, cx0, cy0, rect_width, rect_height, radius);

					pat = cairo_pattern_create_linear (rect_x + 2, y1 + 1, rect_x + 2, y2 - 7.25);
					cairo_pattern_add_color_stop_rgba (pat, 1, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.8 * bg_rgba.alpha);
					cairo_pattern_add_color_stop_rgba (pat, 0, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.4 * bg_rgba.alpha);
					cairo_set_source (cr, pat);
					cairo_fill_preserve (cr);
					cairo_pattern_destroy (pat);
					cairo_set_source_rgba (cr, bg_rgba.red, bg_rgba.green, bg_rgba.blue, 0.2 * bg_rgba.alpha);
					cairo_set_line_width (cr, 0.5);
					cairo_stroke (cr);
					cairo_restore (cr);
				}
			}
		}

		if (draw_start_triangle) {
			week_view_event_item_draw_triangle (
				event_item, cr, bg_rgba,
				x1 + E_WEEK_VIEW_EVENT_L_PAD + 2,
				y1, -3, y2 - y1 + 1, draw_region);
		} else if (can_draw_in_region (draw_region, rect_x, y1, 1, y2 - y1)) {
			EWeekViewColors wvc;
			GdkRGBA *color;

			wvc = E_WEEK_VIEW_COLOR_EVENT_BORDER;
			color = &week_view->colors[wvc];

			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, color);
			cairo_set_line_width (cr, 0.7);
			cairo_move_to (cr, rect_x, y1);
			cairo_line_to (cr, rect_x, y2);
			cairo_stroke (cr);
			cairo_restore (cr);
		}

		if (draw_end_triangle) {
			week_view_event_item_draw_triangle (
				event_item, cr, bg_rgba,
				x2 - E_WEEK_VIEW_EVENT_R_PAD - 2,
				y1, 3, y2 - y1 + 1, draw_region);
		} else if (can_draw_in_region (draw_region, rect_x2, y2, 1, 1)) {
			EWeekViewColors wvc;
			GdkRGBA *color;

			wvc = E_WEEK_VIEW_COLOR_EVENT_BORDER;
			color = &week_view->colors[wvc];

			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, color);
			cairo_set_line_width (cr, 0.7);
			/* rect_x2 is used uninitialized here */
			cairo_move_to (cr, rect_x2, y1);
			cairo_line_to (cr, rect_x2, y2);
			cairo_stroke (cr);
			cairo_restore (cr);
		}

		if (span->text_item && E_TEXT (span->text_item)->editing)
			editing_span = TRUE;

		/* Draw the start & end times, if they are not on day
		 * boundaries. The start time would always be shown if it was
		 * needed, though it may be clipped as the window shrinks.
		 * The end time is only displayed if there is enough room.
		 * We calculate the minimum position for the end time, which
		 * depends on whether the start time is displayed. If the end
		 * time doesn't fit, then we don't draw it. */
		min_end_time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
			+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
			+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;
		if (!editing_span
		    && event->start > week_view->day_starts[span->start_day]) {
			time_x = x1 + E_WEEK_VIEW_EVENT_L_PAD
				+ E_WEEK_VIEW_EVENT_BORDER_WIDTH
				+ E_WEEK_VIEW_EVENT_EDGE_X_PAD;

			cairo_save (cr);

			cairo_rectangle (
				cr,
				x1, y1,
				x2 - x1 - E_WEEK_VIEW_EVENT_R_PAD
				- E_WEEK_VIEW_EVENT_BORDER_WIDTH + 1,
				y2 - y1 + 1);
			cairo_clip (cr);

			week_view_draw_time (
				week_view, bg_rgba, cr, time_x,
				time_y, start_hour, start_minute);

			cairo_restore (cr);

			/* We don't want the end time to be drawn over the
			 * start time, so we increase the minimum position. */
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
			 * the minimum calculated above. */
			if (time_x >= min_end_time_x) {
				week_view_draw_time (
					week_view, bg_rgba, cr, time_x,
					time_y, end_hour, end_minute);
				max_icon_x -= time_width
					+ E_WEEK_VIEW_EVENT_TIME_X_PAD;
			}
		}

		/* Draw the icons. */
		if (span->text_item
		    && (week_view->editing_event_num != event_item->priv->event_num
			|| week_view->editing_span_num != event_item->priv->span_num)) {
			icon_x = span->text_item->x1 - E_WEEK_VIEW_ICON_R_PAD - x;
			week_view_event_item_draw_icons (
				event_item, cr, icon_x,
				icon_y, max_icon_x, TRUE, draw_region);
		}
	}

	cairo_region_destroy (draw_region);
}

static GnomeCanvasItem *
week_view_event_item_point (GnomeCanvasItem *item,
                            gdouble x,
                            gdouble y,
                            gint cx,
                            gint cy)
{
	return item;
}

static gint
week_view_event_item_event (GnomeCanvasItem *item,
                            GdkEvent *event)
{
	EWeekViewEventItem *event_item;

	event_item = E_WEEK_VIEW_EVENT_ITEM (item);

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		return week_view_event_item_double_click (event_item, event);
	case GDK_BUTTON_PRESS:
		return week_view_event_item_button_press (event_item, event);
	case GDK_BUTTON_RELEASE:
		return week_view_event_item_button_release (event_item, event);
	case GDK_MOTION_NOTIFY:
		break;
	default:
		break;
	}

	return FALSE;
}

static void
e_week_view_event_item_class_init (EWeekViewEventItemClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = week_view_event_item_set_property;
	object_class->get_property = week_view_event_item_get_property;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = week_view_event_item_update;
	item_class->draw = week_view_event_item_draw;
	item_class->point = week_view_event_item_point;
	item_class->event = week_view_event_item_event;

	g_object_class_install_property (
		object_class,
		PROP_EVENT_NUM,
		g_param_spec_int (
			"event-num",
			"Event Num",
			NULL,
			G_MININT,
			G_MAXINT,
			-1,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SPAN_NUM,
		g_param_spec_int (
			"span-num",
			"Span Num",
			NULL,
			G_MININT,
			G_MAXINT,
			-1,
			G_PARAM_READWRITE));
}

static void
e_week_view_event_item_init (EWeekViewEventItem *event_item)
{
	event_item->priv = e_week_view_event_item_get_instance_private (event_item);

	event_item->priv->event_num = -1;
	event_item->priv->span_num = -1;
}

gint
e_week_view_event_item_get_event_num (EWeekViewEventItem *event_item)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW_EVENT_ITEM (event_item), -1);

	return event_item->priv->event_num;
}

void
e_week_view_event_item_set_event_num (EWeekViewEventItem *event_item,
                                      gint event_num)
{
	g_return_if_fail (E_IS_WEEK_VIEW_EVENT_ITEM (event_item));

	if (event_item->priv->event_num == event_num)
		return;

	event_item->priv->event_num = event_num;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (event_item));

	g_object_notify (G_OBJECT (event_item), "event-num");
}

gint
e_week_view_event_item_get_span_num (EWeekViewEventItem *event_item)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW_EVENT_ITEM (event_item), -1);

	return event_item->priv->span_num;
}

void
e_week_view_event_item_set_span_num (EWeekViewEventItem *event_item,
                                     gint span_num)
{
	g_return_if_fail (E_IS_WEEK_VIEW_EVENT_ITEM (event_item));

	if (event_item->priv->span_num == span_num)
		return;

	event_item->priv->span_num = span_num;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (event_item));

	g_object_notify (G_OBJECT (event_item), "span-num");
}

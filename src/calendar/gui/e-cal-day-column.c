/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-component-widget.h"

#include "e-cal-day-column.h"

/* the column width will not get below this number */
#define MIN_COLUMN_WIDTH 120

/* gap between two consecutive event columns */
#define COLUMNS_GAP 4

#define BORDER_SIZE 3

/**
 * SECTION: e-cal-day-column
 * @include: calendar/gui/e-cal-day-column.h
 * @short_description: a widget representing a single day view
 *
 * The #ECalDayColumn shows components for a single day.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.58
 **/

struct _ECalDayColumn {
	GtkFixed parent;

	ECalRangeModelSourceFilterFunc source_filter_func;
	gpointer source_filter_user_data;
	ESourceRegistry *registry;
	ICalTimezone *zone;
	ECalComponentBag *bag;
	ECalRangeModel *range_model;
	PangoAttrList *hour_attrs;
	PangoAttrList *minute_attrs;
	const gchar *am, *pm;
	gint max_hour_width;
	gint max_minute_width;
	guint time_division_minutes;
	gint n_rows;
	gint row_height;
	gint time_width;
	gint last_allocated_width;

	struct _highlight {
		gboolean active;
		gboolean clashes;
		guint start_minutes;
		guint end_minutes;
		GdkRGBA bg_rgba_freetime;
		GdkRGBA fg_rgba_freetime;
		GdkRGBA bg_rgba_clash;
		GdkRGBA fg_rgba_clash;
		ECalClient *client; /* only for pointer comparison */
		gchar *uid;
	} highlight;

	gboolean use_24hour_format;
	gboolean show_time;
};

G_DEFINE_TYPE (ECalDayColumn, e_cal_day_column, GTK_TYPE_FIXED)

enum {
	PROP_0,
	PROP_SHOW_TIME,
	PROP_TIME_DIVISION_MINUTES,
	PROP_TIMEZONE,
	PROP_USE_24HOUR_FORMAT,
	N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

typedef struct _CompData {
	GtkWidget *widget; /* (owned) ECalComponentWidget * */
	guint start_minute; /* up to 24 * 60, not within an hour */
	guint duration_minutes;
} CompData;

static gpointer
comp_data_copy (gpointer ptr)
{
	CompData *src = ptr, *des = NULL;

	if (src) {
		des = g_new0 (CompData, 1);
		des->widget = g_object_ref (src->widget);
		des->start_minute = src->start_minute;
		des->duration_minutes = src->duration_minutes;
	}

	return des;
}

static void
comp_data_free (gpointer ptr)
{
	CompData *cd = ptr;

	if (cd) {
		g_clear_object (&cd->widget);
		g_free (cd);
	}
}

static gboolean
e_cal_day_column_check_highlight_clashes_item (ECalDayColumn *self,
					       ECalComponentBagItem *item)
{
	CompData *comp_data = item->user_data;

	if (!comp_data)
		return FALSE;

	if (comp_data->start_minute < self->highlight.end_minutes &&
	    comp_data->start_minute + comp_data->duration_minutes > self->highlight.start_minutes) {
		ECalComponentWidget *comp_widget = E_CAL_COMPONENT_WIDGET (comp_data->widget);
		ECalClient *existing_client;
		ECalComponent *comp;

		comp = e_cal_component_widget_get_component (comp_widget);

		if (e_cal_component_get_transparency (comp) == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
			return FALSE;

		existing_client = e_cal_component_widget_get_client (comp_widget);

		if (existing_client != self->highlight.client ||
		    g_strcmp0 (self->highlight.uid, e_cal_component_get_uid (comp)) != 0) {
			return TRUE;
		}
	}

	return FALSE;
}

typedef struct _HighlightClashesData {
	ECalDayColumn *self;
	gboolean clashes;
} HighlightClashesData;

static gboolean
e_cal_day_column_check_highlight_clashes_foreach_cb (ECalComponentBag *bag,
						     ECalComponentBagItem *item,
						     gpointer user_data)
{
	HighlightClashesData *hcd = user_data;

	hcd->clashes = e_cal_day_column_check_highlight_clashes_item (hcd->self, item);

	return !hcd->clashes;
}

static gboolean
e_cal_day_column_check_highlight_clashes (ECalDayColumn *self)
{
	HighlightClashesData hcd;

	if (!self->highlight.active)
		return FALSE;

	hcd.self = self;
	hcd.clashes = FALSE;

	e_cal_component_bag_foreach (self->bag, e_cal_day_column_check_highlight_clashes_foreach_cb, &hcd);

	return hcd.clashes;
}

static gint
e_cal_day_column_get_required_width (ECalDayColumn *self,
				     gint *out_first_col_offset)
{
	guint n_spans;
	gint for_width;

	n_spans = e_cal_component_bag_get_n_spans (self->bag);
	for_width = MAX (1, n_spans) * (COLUMNS_GAP + MIN_COLUMN_WIDTH);

	if (self->show_time) {
		/* time_width contains the border, thus add only that on the other side */
		for_width += self->time_width + BORDER_SIZE;

		if (out_first_col_offset)
			*out_first_col_offset = self->time_width + COLUMNS_GAP;
	} else {
		for_width += (2 * BORDER_SIZE) - COLUMNS_GAP;

		if (out_first_col_offset)
			*out_first_col_offset = BORDER_SIZE;
	}

	return for_width;
}

static void
e_cal_day_column_place_component (ECalDayColumn *self,
				  ECalComponentBagItem *item,
				  gboolean is_existing,
				  gint prefer_width)
{
	GtkAllocation allocation = { 0, };
	GtkWidget *widget = GTK_WIDGET (self);
	CompData *comp_data = item->user_data;
	gdouble one_minute_height;
	guint n_spans;
	gint for_width, target_width;
	gint first_col_offset = 0;
	gint one_column_width;
	gint total_height = self->row_height * self->n_rows;

	g_return_if_fail (comp_data != NULL);

	if (!comp_data->start_minute && !comp_data->duration_minutes && item->start &&
	    !e_cal_range_model_clamp_to_minutes (self->range_model, item->start, item->duration_minutes,
	    &comp_data->start_minute, &comp_data->duration_minutes)) {
		return;
	}

	n_spans = MAX (1, e_cal_component_bag_get_n_spans (self->bag));

	/* consider allocated width only if in a scrolled window's view */
	if (prefer_width < 0 && gtk_widget_get_realized (widget) &&
	    GTK_IS_VIEWPORT (gtk_widget_get_parent (widget)))
		target_width = gtk_widget_get_allocated_width (gtk_widget_get_parent (widget));
	else
		target_width = prefer_width;
	for_width = MAX (target_width, e_cal_day_column_get_required_width (self, &first_col_offset));

	one_minute_height = ((gdouble) total_height) / (24.0 * 60.0);
	one_column_width = ((for_width - first_col_offset - ((n_spans - 1) * COLUMNS_GAP) - BORDER_SIZE) / n_spans);

	if (one_column_width < MIN_COLUMN_WIDTH)
		one_column_width = MIN_COLUMN_WIDTH;

	allocation.y = one_minute_height * comp_data->start_minute;
	allocation.height = one_minute_height * comp_data->duration_minutes;

	if (allocation.height < self->row_height)
		allocation.height = self->row_height;

	if (allocation.y + self->row_height > total_height)
		allocation.y = total_height - self->row_height;

	if (allocation.y + allocation.height > total_height) {
		if (allocation.height > self->row_height &&
		    allocation.y + self->row_height <= total_height)
			allocation.height = total_height - allocation.y;

		if (allocation.y + allocation.height > total_height)
			allocation.y = total_height - allocation.height;
	}

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		allocation.x = BORDER_SIZE + (n_spans - item->span_index - 1) * (one_column_width + COLUMNS_GAP);
	else
		allocation.x = first_col_offset + (item->span_index * (one_column_width + COLUMNS_GAP));
	allocation.width = one_column_width;

	gtk_widget_size_allocate (comp_data->widget, &allocation);
	gtk_widget_set_size_request (comp_data->widget, allocation.width, allocation.height);

	if (is_existing)
		gtk_fixed_move (GTK_FIXED (self), GTK_WIDGET (comp_data->widget), allocation.x, allocation.y);
	else
		gtk_fixed_put (GTK_FIXED (self), GTK_WIDGET (comp_data->widget), allocation.x, allocation.y);
}

typedef struct _RePlaceData {
	ECalDayColumn *self;
	gint prefer_width;
	gboolean with_durations;
} RePlaceData;

static gboolean
e_cal_day_column_re_place_foreach_cb (ECalComponentBag *bag,
				      ECalComponentBagItem *item,
				      gpointer user_data)
{
	RePlaceData *rpd = user_data;

	if (rpd->with_durations && item->start) {
		CompData *comp_data = item->user_data;

		if (comp_data) {
			e_cal_range_model_clamp_to_minutes (rpd->self->range_model, item->start, item->duration_minutes,
				&comp_data->start_minute, &comp_data->duration_minutes);
		}
	}

	e_cal_day_column_place_component (rpd->self, item, TRUE, rpd->prefer_width);

	return TRUE;
}

static void
e_cal_day_column_layout_all (ECalDayColumn *self,
			     gint prefer_width,
			     gboolean with_durations)
{
	RePlaceData rpd;

	rpd.self = self;
	rpd.prefer_width = prefer_width;
	rpd.with_durations = with_durations;

	e_cal_component_bag_foreach (self->bag, e_cal_day_column_re_place_foreach_cb, &rpd);
}

static void
e_cal_day_column_recalc_layout (ECalDayColumn *self,
				gboolean force,
				gboolean with_durations)
{
	GtkWidget *widget = GTK_WIDGET (self);
	PangoLayout *layout;
	gchar buffer[64];
	gint height = 0, width = 0, for_width, time_width, hour_height = 0;
	guint ii;

	self->max_hour_width = 0;
	self->max_minute_width = 0;

	layout = gtk_widget_create_pango_layout (widget, NULL);

	/* just some random string with an accent and a part below the base line */
	pango_layout_set_text (layout, "Řjgm", -1);
	pango_layout_get_pixel_size (layout, NULL, &height);

	pango_layout_set_attributes (layout, self->hour_attrs);

	for (ii = 0; ii < 24; ii++) {
		gint hheight = 0;
		width = 0;
		g_warn_if_fail (g_snprintf (buffer, sizeof (buffer), "%u", ii) < sizeof (buffer));
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, &hheight);

		self->max_hour_width = MAX (self->max_hour_width, width);
		hour_height = MAX (hour_height, hheight);
	}

	pango_layout_set_attributes (layout, NULL);

	if (self->time_division_minutes > 30 && hour_height > height)
		height = hour_height;

	pango_layout_set_attributes (layout, self->minute_attrs);

	width = 0;
	pango_layout_set_text (layout, self->am, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	self->max_minute_width = MAX (self->max_minute_width, width);

	width = 0;
	pango_layout_set_text (layout, self->pm, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	self->max_minute_width = MAX (self->max_minute_width, width);

	for (ii = 0; ii < 60; ii += 5) {
		width = 0;
		g_warn_if_fail (g_snprintf (buffer, sizeof (buffer), "%02u", ii) < sizeof (buffer));
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &width, NULL);

		self->max_minute_width = MAX (self->max_minute_width, width);
	}

	g_clear_object (&layout);

	time_width = BORDER_SIZE + self->max_hour_width + COLUMNS_GAP + self->max_minute_width;

	if (!force && self->row_height == height && self->time_width == time_width)
		return;

	self->time_width = time_width;
	self->row_height = height < 30 ? 30 : height;
	self->n_rows = 24 * 60 / self->time_division_minutes;

	for_width = e_cal_day_column_get_required_width (self, NULL);

	e_cal_day_column_layout_all (self, -1, with_durations);

	gtk_widget_set_size_request (widget, for_width + 25, self->row_height * self->n_rows);
	gtk_widget_queue_draw (widget);
}

static void
e_cal_day_column_bag_added_cb (ECalComponentBag *bag,
			       ECalComponentBagItem *item,
			       gpointer user_data)
{
	ECalDayColumn *self = user_data;
	CompData *comp_data = item->user_data;
	gboolean is_existing = comp_data != NULL;

	if (!is_existing) {
		comp_data = g_new0 (CompData, 1);
		e_cal_component_bag_item_set_user_data (item, comp_data, comp_data_copy, comp_data_free);
	}

	if (!comp_data->widget)
		comp_data->widget = g_object_ref_sink (e_cal_component_widget_new (item->client, item->comp, self->registry));

	if (!e_cal_range_model_clamp_to_minutes (self->range_model, item->start, item->duration_minutes,
	    &comp_data->start_minute, &comp_data->duration_minutes)) {
		return;
	}

	e_cal_day_column_place_component (self, item, is_existing, -1);

	self->highlight.clashes = e_cal_day_column_check_highlight_clashes (self);
}

static void
e_cal_day_column_bag_removed_cb (ECalComponentBag *bag,
				 GPtrArray *items, /* ECalComponentBagItem * */
				 gpointer user_data)
{
	ECalDayColumn *self = user_data;
	guint ii;

	for (ii = 0; ii < items->len; ii++) {
		ECalComponentBagItem *item = g_ptr_array_index (items, ii);
		CompData *cd = item->user_data;

		if (cd)
			g_clear_pointer (&cd->widget, gtk_widget_destroy);
	}

	self->highlight.clashes = e_cal_day_column_check_highlight_clashes (self);
}

static void
e_cal_day_column_bag_item_changed_cb (ECalComponentBag *bag,
				      ECalComponentBagItem *item,
				      gpointer user_data)
{
	ECalDayColumn *self = user_data;
	CompData *comp_data = item->user_data;

	g_return_if_fail (comp_data != NULL);

	e_cal_component_widget_update_component (E_CAL_COMPONENT_WIDGET (comp_data->widget), item->client, item->comp);

	if (!e_cal_range_model_clamp_to_minutes (self->range_model, item->start, item->duration_minutes, &comp_data->start_minute, &comp_data->duration_minutes))
		return;

	e_cal_day_column_place_component (self, item, TRUE, -1);

	self->highlight.clashes = e_cal_day_column_check_highlight_clashes (self);
}

static void
e_cal_day_column_bag_span_changed_cb (ECalComponentBag *bag,
				      GPtrArray *items, /* ECalComponentBagItem */
				      gpointer user_data)
{
	ECalDayColumn *self = user_data;
	guint ii;

	for (ii = 0; ii < items->len; ii++) {
		ECalComponentBagItem *item = g_ptr_array_index (items, ii);

		e_cal_day_column_place_component (self, item, TRUE, -1);
	}

	self->highlight.clashes = e_cal_day_column_check_highlight_clashes (self);
}

static void
e_cal_day_column_bag_notify_n_items_cb (ECalComponentBag *bag,
					GParamSpec *param,
					gpointer user_data)
{
	ECalDayColumn *self = user_data;

	if (!e_cal_component_bag_get_n_items (bag)) {
		GtkContainer *container = GTK_CONTAINER (self);
		GList *children, *link;

		children = gtk_container_get_children (container);

		for (link = children; link; link = g_list_next (link)) {
			GtkWidget *child = link->data;

			gtk_widget_destroy (child);
		}

		g_list_free (children);
	}
}

static void
e_cal_day_column_bag_notify_n_spans_cb (ECalComponentBag *bag,
					GParamSpec *param,
					gpointer user_data)
{
	ECalDayColumn *self = user_data;

	if (e_cal_component_bag_get_n_spans (bag) > 0)
		e_cal_day_column_layout_all (self, -1, FALSE);
}

static void
e_cal_day_column_style_updated (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (e_cal_day_column_parent_class)->style_updated (widget);

	e_cal_day_column_recalc_layout (E_CAL_DAY_COLUMN (widget), FALSE, FALSE);
}

static gboolean
e_cal_day_column_rectangle_intersects (const GdkRectangle *clip_rect,
				       gint xx,
				       gint yy,
				       gint width,
				       gint height)
{
	GdkRectangle rect;

	rect.x = xx;
	rect.y = yy;
	rect.width = width;
	rect.height = height;

	return gdk_rectangle_intersect (clip_rect, &rect, NULL);
}

static gboolean
e_cal_day_column_draw (GtkWidget *widget,
		       cairo_t *cr)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (widget);
	gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
	GdkRectangle clip_rect;
	GdkRGBA fg_rgba;
	gint width;
	gboolean res;

	if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
		return GTK_WIDGET_CLASS (e_cal_day_column_parent_class)->draw (widget, cr);

	cairo_save (cr);

	e_utils_get_theme_color (widget, "theme_text_color,theme_fg_color", E_UTILS_DEFAULT_THEME_TEXT_COLOR, &fg_rgba);
	gdk_cairo_set_source_rgba (cr, &fg_rgba);

	width = gtk_widget_get_allocated_width (widget);

	/* draw the time grid */
	if (self->time_division_minutes > 0) {
		gchar buffer[64];
		gint minute_pos;

		cairo_save (cr);
		cairo_set_line_width (cr, 1);

		for (minute_pos = 0; minute_pos < 24 * 60; minute_pos += self->time_division_minutes) {
			gint hour = minute_pos / 60;
			gint minute = minute_pos % 60;
			gint xx, yy = e_cal_day_column_time_to_y (self, hour, minute);

			if (!e_cal_day_column_rectangle_intersects (&clip_rect, 0, yy, width, 2))
				continue;

			cairo_save (cr);

			if (minute == 0)
				xx = 0;
			else
				xx = BORDER_SIZE + self->max_hour_width + COLUMNS_GAP;

			if (is_rtl) {
				cairo_move_to (cr, 0, yy + 0.5);
				cairo_line_to (cr, width - xx, yy + 0.5);
			} else {
				cairo_move_to (cr, xx, yy + 0.5);
				cairo_line_to (cr, width, yy + 0.5);
			}

			cairo_stroke (cr);
			cairo_restore (cr);
		}

		cairo_restore (cr);

		if (self->show_time) {
			gint xx = is_rtl ? width - self->time_width : 0;

			cairo_save (cr);

			for (minute_pos = 0; minute_pos < 24 * 60; minute_pos += self->time_division_minutes) {
				PangoLayout *layout;
				gint hour = minute_pos / 60;
				gint minute = minute_pos % 60;
				gint yy = e_cal_day_column_time_to_y (self, hour, minute);

				if (!e_cal_day_column_rectangle_intersects (&clip_rect, xx, yy, self->time_width, self->row_height * 1.5))
					continue;

				if (minute == 0) {
					gint text_width = 0;

					g_snprintf (buffer, sizeof (buffer), "%d", self->use_24hour_format ? hour : hour > 12 ? hour - 12 : hour == 0 ? 12 : hour);

					cairo_save (cr);
					layout = gtk_widget_create_pango_layout (widget, NULL);
					pango_layout_set_attributes (layout, self->hour_attrs);
					pango_layout_set_text (layout, buffer, -1);
					pango_layout_get_pixel_size (layout, &text_width, NULL);
					if (is_rtl)
						cairo_translate (cr, width - BORDER_SIZE - self->max_hour_width, BORDER_SIZE + yy);
					else
						cairo_translate (cr, BORDER_SIZE + self->max_hour_width - text_width, BORDER_SIZE + yy);
					pango_cairo_update_layout (cr, layout);
					pango_cairo_show_layout (cr, layout);
					g_clear_object (&layout);
					cairo_restore (cr);

					cairo_save (cr);
					layout = gtk_widget_create_pango_layout (widget, NULL);
					pango_layout_set_attributes (layout, self->minute_attrs);
					pango_layout_set_text (layout, self->use_24hour_format ? "00" : (hour < 12 ? self->am : self->pm), -1);
					pango_layout_get_pixel_size (layout, &text_width, NULL);
					if (is_rtl)
						cairo_translate (cr, width - BORDER_SIZE - self->max_hour_width - COLUMNS_GAP - text_width, BORDER_SIZE + yy);
					else
						cairo_translate (cr, BORDER_SIZE + self->max_hour_width + COLUMNS_GAP + self->max_minute_width - text_width, BORDER_SIZE + yy);
					pango_cairo_update_layout (cr, layout);
					pango_cairo_show_layout (cr, layout);
					g_clear_object (&layout);
					cairo_restore (cr);
				} else if (self->time_division_minutes < 30) {
					gint text_width = 0;

					g_snprintf (buffer, sizeof (buffer), "%02d", minute);

					cairo_save (cr);
					layout = gtk_widget_create_pango_layout (widget, NULL);
					pango_layout_set_attributes (layout, self->minute_attrs);
					pango_layout_set_text (layout, buffer, -1);
					pango_layout_get_pixel_size (layout, &text_width, NULL);
					if (is_rtl)
						cairo_translate (cr, width - BORDER_SIZE - self->max_hour_width - COLUMNS_GAP - text_width, BORDER_SIZE + yy);
					else
						cairo_translate (cr, BORDER_SIZE + self->max_hour_width + COLUMNS_GAP + self->max_minute_width - text_width, BORDER_SIZE + yy);
					pango_cairo_update_layout (cr, layout);
					pango_cairo_show_layout (cr, layout);
					g_clear_object (&layout);
					cairo_restore (cr);
				}
			}

			cairo_restore (cr);
		}
	}

	if (self->highlight.active) {
		guint yy_start, yy_end;

		yy_start = self->highlight.start_minutes * self->n_rows * self->row_height / (24.0 * 60.0);
		yy_end = self->highlight.end_minutes * self->n_rows * self->row_height / (24.0 * 60.0);

		if (e_cal_day_column_rectangle_intersects (&clip_rect, 0, yy_start, width, yy_end - yy_start)) {
			gint offset_x, for_width;
			GdkRGBA bg_rgba;

			offset_x = BORDER_SIZE;
			for_width = width - offset_x - BORDER_SIZE;

			if (for_width <= 0)
				for_width = width;

			if (self->highlight.clashes) {
				bg_rgba = self->highlight.bg_rgba_clash;
				fg_rgba = self->highlight.fg_rgba_clash;
			} else {
				bg_rgba = self->highlight.bg_rgba_freetime;
				fg_rgba = self->highlight.fg_rgba_freetime;
			}

			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, &bg_rgba);
			cairo_rectangle (cr, offset_x, yy_start, for_width, yy_end - yy_start);
			cairo_fill (cr);
			cairo_restore (cr);

			e_utils_shade_color (&bg_rgba, &bg_rgba, 0.8);

			cairo_save (cr);
			gdk_cairo_set_source_rgba (cr, &bg_rgba);
			cairo_rectangle (cr, offset_x, yy_start + 1, for_width, yy_end - yy_start - 2);
			cairo_stroke (cr);
			cairo_restore (cr);
		}
	}

	cairo_restore (cr);

	res = GTK_WIDGET_CLASS (e_cal_day_column_parent_class)->draw (widget, cr);

	return res;
}

static void
e_cal_day_column_get_preferred_width (GtkWidget *widget,
				      gint *out_minimum,
				      gint *out_natural)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (widget);
	gint min_width;

	GTK_WIDGET_CLASS (e_cal_day_column_parent_class)->get_preferred_width (widget, out_minimum, out_natural);

	min_width = e_cal_day_column_get_required_width (self, NULL);

	if (min_width > *out_minimum)
		*out_minimum = min_width;

	if (*out_natural < *out_minimum)
		*out_natural = *out_minimum;
}

static gboolean
e_cal_day_column_source_filter_cb (ESource *source,
				   gpointer user_data)
{
	ECalDayColumn *self = user_data;

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR) &&
	    !e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST))
		return FALSE;

	return !self->source_filter_func || self->source_filter_func (source, self->source_filter_user_data);
}

static void
e_cal_day_column_component_added_or_modified_cb (ECalRangeModel *model,
						 ECalClient *client,
						 ECalComponent *component,
						 gpointer user_data)
{
	ECalDayColumn *self = user_data;

	e_cal_component_bag_add (self->bag, client, component);
}

static void
e_cal_day_column_component_removed_cb (ECalRangeModel *model,
				       ECalClient *client,
				       const gchar *uid,
				       const gchar *rid,
				       gpointer user_data)
{
	ECalDayColumn *self = user_data;

	e_cal_component_bag_remove (self->bag, client, uid, rid);
}

static void
e_cal_day_column_set_property (GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (object);

	switch (prop_id) {
	case PROP_SHOW_TIME:
		e_cal_day_column_set_show_time (self, g_value_get_boolean (value));
		break;
	case PROP_TIME_DIVISION_MINUTES:
		e_cal_day_column_set_time_division_minutes (self, g_value_get_uint (value));
		break;
	case PROP_TIMEZONE:
		e_cal_day_column_set_timezone (self, g_value_get_object (value));
		break;
	case PROP_USE_24HOUR_FORMAT:
		e_cal_day_column_set_use_24hour_format (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_day_column_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (object);

	switch (prop_id) {
	case PROP_SHOW_TIME:
		g_value_set_boolean (value, e_cal_day_column_get_show_time (self));
		break;
	case PROP_TIME_DIVISION_MINUTES:
		g_value_set_uint (value, e_cal_day_column_get_time_division_minutes (self));
		break;
	case PROP_TIMEZONE:
		g_value_set_object (value, e_cal_day_column_get_timezone (self));
		break;
	case PROP_USE_24HOUR_FORMAT:
		g_value_set_boolean (value, e_cal_day_column_get_use_24hour_format (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_day_column_dispose (GObject *object)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (object);

	if (self->range_model)
		e_cal_range_model_prepare_dispose (self->range_model);
	g_clear_object (&self->range_model);
	g_clear_object (&self->bag);

	G_OBJECT_CLASS (e_cal_day_column_parent_class)->dispose (object);
}

static void
e_cal_day_column_finalize (GObject *object)
{
	ECalDayColumn *self = E_CAL_DAY_COLUMN (object);

	g_clear_pointer (&self->highlight.uid, g_free);
	g_clear_pointer (&self->hour_attrs, pango_attr_list_unref);
	g_clear_pointer (&self->minute_attrs, pango_attr_list_unref);
	g_clear_object (&self->zone);
	g_clear_object (&self->registry);

	G_OBJECT_CLASS (e_cal_day_column_parent_class)->finalize (object);
}

static void
e_cal_day_column_class_init (ECalDayColumnClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_cal_day_column_set_property;
	object_class->get_property = e_cal_day_column_get_property;
	object_class->dispose = e_cal_day_column_dispose;
	object_class->finalize = e_cal_day_column_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->style_updated = e_cal_day_column_style_updated;
	widget_class->draw = e_cal_day_column_draw;
	widget_class->get_preferred_width = e_cal_day_column_get_preferred_width;

	/**
	 * ECalDayColumn:show-time:
	 *
	 * Whether to show the time column.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_SHOW_TIME] = g_param_spec_boolean ("show-time", NULL, NULL,
		FALSE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalDayColumn:time-division-minutes:
	 *
	 * The time division of the grid, in minutes. It can be between 5 and 60 minutes inclusive.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_TIME_DIVISION_MINUTES] = g_param_spec_uint ("time-division-minutes", NULL, NULL,
		5, 60, 30,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalDayColumn:timezone:
	 *
	 * The timezone to be used for time calculations.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_TIMEZONE] = g_param_spec_object ("timezone", NULL, NULL,
		I_CAL_TYPE_TIMEZONE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalDayColumn:use-24hour-format:
	 *
	 * Whether to use 24-hour format in the time column.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_USE_24HOUR_FORMAT] = g_param_spec_boolean ("use-24hour-format", NULL, NULL,
		FALSE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	gtk_widget_class_set_css_name (widget_class, "ECalDayColumn");
}

static void
e_cal_day_column_init (ECalDayColumn *self)
{
	self->time_division_minutes = 30;
	self->row_height = 0;
	self->hour_attrs = pango_attr_list_new ();
	pango_attr_list_insert_before (self->hour_attrs, pango_attr_font_features_new ("tnum=1"));
	pango_attr_list_insert_before (self->hour_attrs, pango_attr_scale_new (1.2));
	self->minute_attrs = pango_attr_list_new ();
	pango_attr_list_insert_before (self->minute_attrs, pango_attr_font_features_new ("tnum=1"));
	pango_attr_list_insert_before (self->minute_attrs, pango_attr_scale_new (0.8));

	self->am = _("am");
	self->pm = _("pm");

	g_object_set (self,
		"margin-start", 5,
		"margin-end", 5,
		NULL);

	self->bag = e_cal_component_bag_new ();
	g_signal_connect (self->bag, "added",
		G_CALLBACK (e_cal_day_column_bag_added_cb), self);
	g_signal_connect (self->bag, "removed",
		G_CALLBACK (e_cal_day_column_bag_removed_cb), self);
	g_signal_connect (self->bag, "item-changed",
		G_CALLBACK (e_cal_day_column_bag_item_changed_cb), self);
	g_signal_connect (self->bag, "span-changed",
		G_CALLBACK (e_cal_day_column_bag_span_changed_cb), self);
	g_signal_connect (self->bag, "notify::n-items",
		G_CALLBACK (e_cal_day_column_bag_notify_n_items_cb), self);
	g_signal_connect (self->bag, "notify::n-spans",
		G_CALLBACK (e_cal_day_column_bag_notify_n_spans_cb), self);

	/* minimum duration is one line */
	e_cal_component_bag_set_min_duration_minutes (self->bag, self->time_division_minutes);
}

/**
 * e_cal_day_column_new:
 * @client_cache: an #EClientCache
 * @alert_sink: an #EAlertSink
 * @source_filter_func: (optional): optional filtering callback to allow/disallow chosen sources; can be %NULL
 * @source_filter_user_data: (optional): user data for the @source_filter_func
 *
 * Creates a new #ECalDayColumn. The @client_cache is used to get the #ECalClient-s from.
 * The @alert_sink is used to run operations in a dedicated thread.
 *
 * Returns: (transfer full): a new #ECalDayColumn
 *
 * Since: 3.58
 **/
ECalDayColumn *
e_cal_day_column_new (EClientCache *client_cache,
		      EAlertSink *alert_sink,
		      ECalRangeModelSourceFilterFunc source_filter_func,
		      gpointer source_filter_user_data)
{
	ECalDayColumn *self;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	self = g_object_new (E_TYPE_CAL_DAY_COLUMN,
		"width-request", 50,
		NULL);

	self->source_filter_func = source_filter_func;
	self->source_filter_user_data = source_filter_user_data;

	self->registry = e_client_cache_ref_registry (client_cache);
	self->range_model = e_cal_range_model_new (client_cache, alert_sink,
		e_cal_day_column_source_filter_cb, self);

	g_signal_connect (self->range_model, "component-added",
		G_CALLBACK (e_cal_day_column_component_added_or_modified_cb), self);
	g_signal_connect (self->range_model, "component-modified",
		G_CALLBACK (e_cal_day_column_component_added_or_modified_cb), self);
	g_signal_connect (self->range_model, "component-removed",
		G_CALLBACK (e_cal_day_column_component_removed_cb), self);

	return self;
}

/**
 * e_cal_day_column_set_time_division_minutes:
 * @self: an #ECalDayColumn
 * @minutes: a value to set
 *
 * Sets time division to use in the day grid. It can be
 * between 5 and 60 inclusive.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_set_time_division_minutes (ECalDayColumn *self,
					    guint minutes)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));
	g_return_if_fail (minutes >= 5 && minutes <= 60);

	if (self->time_division_minutes == minutes)
		return;

	self->time_division_minutes = minutes;

	/* minimum duration is one line */
	e_cal_component_bag_set_min_duration_minutes (self->bag, minutes);
	e_cal_component_bag_rebuild (self->bag);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME_DIVISION_MINUTES]);

	e_cal_day_column_recalc_layout (self, TRUE, TRUE);
}

/**
 * e_cal_day_column_get_time_division_minutes:
 * @self: an #ECalDayColumn
 *
 * Gets time division, in minutes, the day grid is shown with.
 *
 * Returns: time division, in minutes
 *
 * Since: 3.58
 **/
guint
e_cal_day_column_get_time_division_minutes (ECalDayColumn *self)
{
	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), 0);

	return self->time_division_minutes;
}

/**
 * e_cal_day_column_set_timezone:
 * @self: an #ECalDayColumn
 * @zone: (nullable): an #ICalTimezone
 *
 * Sets an #ICalTimezone to be used for the time calculations.
 * If @zone is %NULL, uses the current configured user time zone.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_set_timezone (ECalDayColumn *self,
			       ICalTimezone *zone)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));
	if (zone)
		g_return_if_fail (I_CAL_IS_TIMEZONE (zone));

	if (!zone) {
		zone = calendar_config_get_icaltimezone ();
		if (!zone)
			zone = i_cal_timezone_get_utc_timezone ();
	}

	if (g_set_object (&self->zone, zone)) {
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIMEZONE]);

		e_cal_range_model_set_timezone (self->range_model, zone);
		e_cal_component_bag_set_timezone (self->bag, zone);
	}
}

/**
 * e_cal_day_column_get_timezone:
 * @self: an #ECalDayColumn
 *
 * Gets an #ICalTimezone for the time calculations.
 *
 * Returns: (transfer none): a timezone to use for the time calculations.
 *
 * Since: 3.58
 **/
ICalTimezone *
e_cal_day_column_get_timezone (ECalDayColumn *self)
{
	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), NULL);

	return self->zone;
}

/**
 * e_cal_day_column_set_use_24hour_format:
 * @self: an #ECalDayColumn
 * @value: a value to set
 *
 * Sets whether the time column should use the 24-hour format.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_set_use_24hour_format (ECalDayColumn *self,
					gboolean value)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	if ((!self->use_24hour_format) != (!value)) {
		self->use_24hour_format = value;

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_24HOUR_FORMAT]);

		gtk_widget_queue_draw (GTK_WIDGET (self));
	}
}

/**
 * e_cal_day_column_get_use_24hour_format:
 * @self: an #ECalDayColumn
 *
 * Gets whether to use 24-hour format in the time column.
 * The default is to not use it.
 *
 * Returns: whether to use 24-hour format in the time column
 *
 * Since: 3.58
 **/
gboolean
e_cal_day_column_get_use_24hour_format (ECalDayColumn *self)
{
	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), FALSE);

	return self->use_24hour_format;
}

/**
 * e_cal_day_column_set_show_time:
 * @self: an #ECalDayColumn
 * @value: value to set
 *
 * Sets whether the time column should be shown.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_set_show_time (ECalDayColumn *self,
				gboolean value)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	if ((!self->show_time) != (!value)) {
		self->show_time = value;

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_TIME]);

		e_cal_day_column_recalc_layout (self, TRUE, FALSE);
	}
}

/**
 * e_cal_day_column_get_show_time:
 * @self: an #ECalDayColumn
 *
 * Gets whether the time column is shown. The default is to not show it.
 *
 * Returns: whether the time is shown
 *
 * Since: 3.58
 **/
gboolean
e_cal_day_column_get_show_time (ECalDayColumn *self)
{
	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), FALSE);

	return self->show_time;
}

/**
 * e_cal_day_column_set_range:
 * @self: an #ECalDayColumn
 * @start: the range start
 * @start: the range end
 *
 * Set the range to be shown in the @self. It should be 24 hours long.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_set_range (ECalDayColumn *self,
			    time_t start,
			    time_t end)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	e_cal_range_model_set_range (self->range_model, start, end - 1);
}

/**
 * e_cal_day_column_get_range:
 * @self: an #ECalDayColumn
 * @out_start: (out): location to set the range start to
 * @out_end: (out): location to set the range end to
 *
 * Get currently used range for the @self.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_get_range (ECalDayColumn *self,
			    time_t *out_start,
			    time_t *out_end)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	e_cal_range_model_get_range (self->range_model, out_start, out_end);
}

/**
 * e_cal_day_column_layout_for_width:
 * @self: an #ECalDayColumn
 * @min_width: minimum width to allocate and layout the @self for
 *
 * Layout the components in the @self in a way to use at least @min_width width.
 * It can use more, when the @min_width is too small to cover all spans.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_layout_for_width (ECalDayColumn *self,
				   guint min_width)
{
	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	e_cal_day_column_layout_all (self, min_width, FALSE);
}

/**
 * e_cal_day_column_time_to_y:
 * @self: an #ECalDayColumn
 * @hour: an hour, up to 23, inclusive
 * @minute: a minute, up to 60, inclusive
 *
 * Converts the @hour and @minute into an Y value within the @self coordinates.
 *
 * Returns: where in the height the time given as the @hour and the @minute is
 *
 * Since: 3.58
 **/
gint
e_cal_day_column_time_to_y (ECalDayColumn *self,
			    guint hour,
			    guint minute)
{
	gdouble day_minute, minute_size;

	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), -1);
	g_return_val_if_fail (hour <= 23, -1);
	g_return_val_if_fail (minute <= 60, -1);

	day_minute = (hour * 60) + minute;
	minute_size = (self->row_height * self->n_rows) / (24.0 * 60.0);

	return (gint) (day_minute * minute_size);
}

/**
 * e_cal_day_column_y_to_time:
 * @self: an #ECalDayColumn
 * @yy: an Y in pixels to convert, in the widget coordinates
 * @out_hour: (out): an hour the @yy corresponds to
 * @out_minute: (out): a minute the @yy corresponds to
 *
 * Converts the @yy, in the @self coordinates, to the corresponding
 * hour and minute.
 *
 * Returns: %TRUE the @yy was in the expected bounds and the out arguments
 *    had been set, %FALSE otherwise
 *
 * Since: 3.58
 **/
gboolean
e_cal_day_column_y_to_time (ECalDayColumn *self,
			    gint yy,
			    guint *out_hour,
			    guint *out_minute)
{
	guint day_minutes;

	g_return_val_if_fail (E_IS_CAL_DAY_COLUMN (self), FALSE);
	g_return_val_if_fail (out_hour != NULL, FALSE);
	g_return_val_if_fail (out_minute != NULL, FALSE);

	if (yy < 0 || yy > self->row_height * self->n_rows)
		return FALSE;

	day_minutes = (guint) (24.0 * 60.0 * yy / ((gdouble) (self->row_height * self->n_rows)));

	*out_hour = day_minutes / 60;
	*out_minute = day_minutes % 60;

	if (*out_hour == 24) {
		*out_hour = 23;
		*out_minute = 60;
	}

	return TRUE;
}

/**
 * e_cal_day_column_highlight_time:
 * @self: an #ECalDayColumn
 * @client: (nullable): an optional #ECalClient, or %NULL
 * @uid: (nullable): an optional UID of the component the range corresponds to
 * @hour_start: hour of the highlight start within the @self day range
 * @minute_start: minute of the highlight start within the @self day range
 * @hour_end: hour of the highlight end within the @self day range
 * @minute_end: minute of the highlight end within the @self day range
 * @bg_rgba_freetime: (nullable): a #GdkRGBA for the background when the time range does not clash with other components, or %NULL
 * @bg_rgba_clash: (nullable): a #GdkRGBA for the background when the time range does not clash with other components, or %NULL
 *
 * Sets a time range to be highlighted in the day column with a background @bg_rgba_freetime
 * or @bg_rgba_clash, depending whether the time range is or is not occupied by other components.
 * The @client and @uid provide a reference to an existing component this range references,
 * use %NULL for a new range. The hour/minute are within the day the @self shows. Using
 * hour 23 and minute 60 is a valid use case.
 *
 * When the times are out of range or the @bg_rgba_freetime or @bg_rgba_clash
 * are %NULL, then any previous highlight area is unset.
 *
 * Since: 3.58
 **/
void
e_cal_day_column_highlight_time (ECalDayColumn *self,
				 ECalClient *client,
				 const gchar *uid,
				 guint hour_start,
				 guint minute_start,
				 guint hour_end,
				 guint minute_end,
				 const GdkRGBA *bg_rgba_freetime,
				 const GdkRGBA *bg_rgba_clash)
{
	gboolean changed;

	g_return_if_fail (E_IS_CAL_DAY_COLUMN (self));

	if (hour_start < 24 && minute_start <= 60 && hour_end < 24 && minute_end <= 60 &&
	    bg_rgba_freetime && bg_rgba_clash) {
		guint start_minutes = 60 * hour_start + minute_start;
		guint end_minutes = 60 * hour_end + minute_end;

		changed = !self->highlight.active ||
			client != self->highlight.client ||
			g_strcmp0 (uid, self->highlight.uid) != 0 ||
			start_minutes != self->highlight.start_minutes ||
			end_minutes != self->highlight.end_minutes ||
			!gdk_rgba_equal (bg_rgba_freetime, &self->highlight.bg_rgba_freetime) ||
			!gdk_rgba_equal (bg_rgba_clash, &self->highlight.bg_rgba_clash);

		if (changed) {
			self->highlight.active = TRUE;

			if (start_minutes <= end_minutes) {
				self->highlight.start_minutes = start_minutes;
				self->highlight.end_minutes = end_minutes;
			} else {
				self->highlight.start_minutes = end_minutes;
				self->highlight.end_minutes = start_minutes;
			}

			self->highlight.bg_rgba_freetime = *bg_rgba_freetime;
			self->highlight.fg_rgba_freetime = e_utils_get_text_color_for_background (bg_rgba_freetime);
			self->highlight.bg_rgba_clash = *bg_rgba_clash;
			self->highlight.fg_rgba_clash = e_utils_get_text_color_for_background (bg_rgba_clash);
			self->highlight.fg_rgba_freetime.alpha = self->highlight.bg_rgba_freetime.alpha;
			self->highlight.fg_rgba_clash.alpha = self->highlight.bg_rgba_clash.alpha;

			self->highlight.client = client;
			if (g_strcmp0 (self->highlight.uid, uid) != 0) {
				g_clear_pointer (&self->highlight.uid, g_free);
				self->highlight.uid = g_strdup (uid);
			}

			self->highlight.clashes = e_cal_day_column_check_highlight_clashes (self);
		}
	} else {
		changed = self->highlight.active;

		self->highlight.active = FALSE;
		g_clear_pointer (&self->highlight.uid, g_free);
	}

	if (changed)
		gtk_widget_queue_draw (GTK_WIDGET (self));
}

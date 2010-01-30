/*
 * EWeekViewTitlesItem - displays the 'Monday', 'Tuesday' etc. at the top of
 * the Month calendar view.
 *
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
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <e-util/e-util.h>
#include "e-week-view-titles-item.h"

#define E_WEEK_VIEW_TITLES_ITEM_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_WEEK_VIEW_TITLES_ITEM, EWeekViewTitlesItemPrivate))

struct _EWeekViewTitlesItemPrivate {
	EWeekView *week_view;
};

enum {
	PROP_0,
	PROP_WEEK_VIEW
};

static gpointer parent_class;

static void
week_view_titles_item_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_VIEW:
			e_week_view_titles_item_set_week_view (
				E_WEEK_VIEW_TITLES_ITEM (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_titles_item_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEEK_VIEW:
			g_value_set_object (
				value,
				e_week_view_titles_item_get_week_view (
				E_WEEK_VIEW_TITLES_ITEM (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
week_view_titles_item_dispose (GObject *object)
{
	EWeekViewTitlesItemPrivate *priv;

	priv = E_WEEK_VIEW_TITLES_ITEM_GET_PRIVATE (object);

	if (priv->week_view != NULL) {
		g_object_unref (priv->week_view);
		priv->week_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
week_view_titles_item_update (GnomeCanvasItem *item,
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
week_view_titles_item_draw (GnomeCanvasItem *canvas_item,
                            GdkDrawable *drawable,
                            gint x,
                            gint y,
                            gint width,
                            gint height)
{
	EWeekViewTitlesItem *titles_item;
	EWeekView *week_view;
	GtkStyle *style;
	GdkGC *fg_gc, *light_gc, *dark_gc;
	gint col_width, col, date_width, date_x;
	gchar buffer[128];
	GdkRectangle clip_rect;
	GtkAllocation allocation;
	gboolean abbreviated;
	gint weekday;
	PangoLayout *layout;

	titles_item = E_WEEK_VIEW_TITLES_ITEM (canvas_item);
	week_view = e_week_view_titles_item_get_week_view (titles_item);
	g_return_if_fail (week_view != NULL);

	gtk_widget_get_allocation (
		GTK_WIDGET (canvas_item->canvas), &allocation);

	style = gtk_widget_get_style (GTK_WIDGET (week_view));
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), NULL);

	/* Draw the shadow around the dates. */
	gdk_draw_line (drawable, light_gc,
		       1 - x, 1 - y,
		       allocation.width - 2 - x, 1 - y);
	gdk_draw_line (drawable, light_gc,
		       1 - x, 2 - y,
		       1 - x, allocation.height - 1 - y);

	gdk_draw_rectangle (drawable, dark_gc, FALSE,
			    0 - x, 0 - y,
			    allocation.width - 1, allocation.height);

	/* Determine the format to use. */
	col_width = allocation.width / week_view->columns;
	abbreviated = (week_view->max_day_width + 2 >= col_width);

	/* Shift right one pixel to account for the shadow around the main
	   canvas. */
	x--;

	/* Draw the date. Set a clipping rectangle so we don't draw over the
	   next day. */
	weekday = week_view->display_start_day;
	for (col = 0; col < week_view->columns; col++) {
		if (weekday == 5 && week_view->compress_weekend)
			g_snprintf (
				buffer, sizeof (buffer), "%s/%s",
				e_get_weekday_name (G_DATE_SATURDAY, TRUE),
				e_get_weekday_name (G_DATE_SUNDAY, TRUE));
		else
			g_snprintf (
				buffer, sizeof (buffer), "%s",
				e_get_weekday_name (weekday + 1, abbreviated));

		clip_rect.x = week_view->col_offsets[col] - x;
		clip_rect.y = 2 - y;
		clip_rect.width = week_view->col_widths[col];
		clip_rect.height = allocation.height - 2;
		gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

		if (weekday == 5 && week_view->compress_weekend)
			date_width = week_view->abbr_day_widths[5]
				+ week_view->slash_width
				+ week_view->abbr_day_widths[6];
		else if (abbreviated)
			date_width = week_view->abbr_day_widths[weekday];
		else
			date_width = week_view->day_widths[weekday];

		date_x = week_view->col_offsets[col]
			+ (week_view->col_widths[col] - date_width) / 2;
		date_x = MAX (date_x, week_view->col_offsets[col]);

		pango_layout_set_text (layout, buffer, -1);
		gdk_draw_layout (drawable, fg_gc,
				 date_x - x,
				 3 - y,
				 layout);

		gdk_gc_set_clip_rectangle (fg_gc, NULL);

		/* Draw the lines down the left and right of the date cols. */
		if (col != 0) {
			gdk_draw_line (drawable, light_gc,
				       week_view->col_offsets[col] - x,
				       4 - y,
				       week_view->col_offsets[col] - x,
				       allocation.height - 4 - y);

			gdk_draw_line (drawable, dark_gc,
				       week_view->col_offsets[col] - 1 - x,
				       4 - y,
				       week_view->col_offsets[col] - 1 - x,
				       allocation.height - 4 - y);
		}

		/* Draw the lines between each column. */
		if (col != 0) {
			gdk_draw_line (drawable, style->black_gc,
				       week_view->col_offsets[col] - x,
				       allocation.height - y,
				       week_view->col_offsets[col] - x,
				       allocation.height - y);
		}

		if (weekday == 5 && week_view->compress_weekend)
			weekday += 2;
		else
			weekday++;

		weekday = weekday % 7;
	}

	g_object_unref (layout);
}

static double
week_view_titles_item_point (GnomeCanvasItem *item,
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
week_view_titles_item_class_init (EWeekViewTitlesItemClass *class)
{
	GObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EWeekViewTitlesItemPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = week_view_titles_item_set_property;
	object_class->get_property = week_view_titles_item_get_property;
	object_class->dispose = week_view_titles_item_dispose;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = week_view_titles_item_update;
	item_class->draw = week_view_titles_item_draw;
	item_class->point = week_view_titles_item_point;

	g_object_class_install_property (
		object_class,
		PROP_WEEK_VIEW,
		g_param_spec_object (
			"week-view",
			"Week View",
			NULL,
			E_TYPE_WEEK_VIEW,
			G_PARAM_READWRITE));
}

static void
week_view_titles_item_init (EWeekViewTitlesItem *titles_item)
{
	titles_item->priv = E_WEEK_VIEW_TITLES_ITEM_GET_PRIVATE (titles_item);
}

GType
e_week_view_titles_item_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EWeekViewTitlesItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) week_view_titles_item_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EWeekViewTitlesItem),
			0,     /* n_preallocs */
			(GInstanceInitFunc) week_view_titles_item_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GNOME_TYPE_CANVAS_ITEM, "EWeekViewTitlesItem",
			&type_info, 0);
	}

	return type;
}

EWeekView *
e_week_view_titles_item_get_week_view (EWeekViewTitlesItem *titles_item)
{
	g_return_val_if_fail (E_IS_WEEK_VIEW_TITLES_ITEM (titles_item), NULL);

	return titles_item->priv->week_view;
}

void
e_week_view_titles_item_set_week_view (EWeekViewTitlesItem *titles_item,
                                       EWeekView *week_view)
{
	g_return_if_fail (E_IS_WEEK_VIEW_TITLES_ITEM (titles_item));
	g_return_if_fail (E_IS_WEEK_VIEW (week_view));

	if (titles_item->priv->week_view != NULL)
		g_object_unref (titles_item->priv->week_view);

	titles_item->priv->week_view = g_object_ref (week_view);

	g_object_notify (G_OBJECT (titles_item), "week-view");
}

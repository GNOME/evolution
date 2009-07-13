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

static void e_week_view_titles_item_set_property(GObject	 *object,
						 guint		  property_id,
						 const GValue	 *value,
						 GParamSpec	 *pspec);
static void e_week_view_titles_item_update	(GnomeCanvasItem *item,
						 double		 *affine,
						 ArtSVP		 *clip_path,
						 gint		  flags);
static void e_week_view_titles_item_draw	(GnomeCanvasItem *item,
						 GdkDrawable	 *drawable,
						 gint		  x,
						 gint		  y,
						 gint		  width,
						 gint		  height);
static double e_week_view_titles_item_point	(GnomeCanvasItem *item,
						 double		  x,
						 double		  y,
						 gint		  cx,
						 gint		  cy,
						 GnomeCanvasItem **actual_item);

/* The arguments we take */
enum {
	PROP_0,
	PROP_WEEK_VIEW
};

G_DEFINE_TYPE (EWeekViewTitlesItem, e_week_view_titles_item, GNOME_TYPE_CANVAS_ITEM)

static void
e_week_view_titles_item_class_init (EWeekViewTitlesItemClass *class)
{
	GObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_week_view_titles_item_set_property;

	item_class = GNOME_CANVAS_ITEM_CLASS (class);
	item_class->update = e_week_view_titles_item_update;
	item_class->draw = e_week_view_titles_item_draw;
	item_class->point = e_week_view_titles_item_point;

	g_object_class_install_property (
		object_class,
		PROP_WEEK_VIEW,
		g_param_spec_pointer (
			"week_view",
			NULL,
			NULL,
			G_PARAM_WRITABLE));
}

static void
e_week_view_titles_item_init (EWeekViewTitlesItem *wvtitem)
{
	wvtitem->week_view = NULL;
}

static void
e_week_view_titles_item_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	EWeekViewTitlesItem *wvtitem;

	wvtitem = E_WEEK_VIEW_TITLES_ITEM (object);

	switch (property_id) {
	case PROP_WEEK_VIEW:
		wvtitem->week_view = g_value_get_pointer (value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_week_view_titles_item_update (GnomeCanvasItem *item,
				double	    *affine,
				ArtSVP	    *clip_path,
				gint		     flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_week_view_titles_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_week_view_titles_item_parent_class)->update) (item, affine, clip_path, flags);

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
e_week_view_titles_item_draw (GnomeCanvasItem  *canvas_item,
			      GdkDrawable      *drawable,
			      gint		x,
			      gint		y,
			      gint		width,
			      gint		height)
{
	EWeekViewTitlesItem *wvtitem;
	EWeekView *week_view;
	GtkStyle *style;
	GdkGC *fg_gc, *light_gc, *dark_gc;
	gint canvas_width, canvas_height, col_width, col, date_width, date_x;
	gchar buffer[128];
	GdkRectangle clip_rect;
	gboolean abbreviated;
	gint weekday;
	PangoLayout *layout;

#if 0
	g_print ("In e_week_view_titles_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif

	wvtitem = E_WEEK_VIEW_TITLES_ITEM (canvas_item);
	week_view = wvtitem->week_view;
	g_return_if_fail (week_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (week_view));
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	canvas_width = GTK_WIDGET (canvas_item->canvas)->allocation.width;
	canvas_height = GTK_WIDGET (canvas_item->canvas)->allocation.height;
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), NULL);

	/* Draw the shadow around the dates. */
	gdk_draw_line (drawable, light_gc,
		       1 - x, 1 - y,
		       canvas_width - 2 - x, 1 - y);
	gdk_draw_line (drawable, light_gc,
		       1 - x, 2 - y,
		       1 - x, canvas_height - 1 - y);

	gdk_draw_rectangle (drawable, dark_gc, FALSE,
			    0 - x, 0 - y,
			    canvas_width - 1, canvas_height);

	/* Determine the format to use. */
	col_width = canvas_width / week_view->columns;
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
		clip_rect.height = canvas_height - 2;
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
				       canvas_height - 4 - y);

			gdk_draw_line (drawable, dark_gc,
				       week_view->col_offsets[col] - 1 - x,
				       4 - y,
				       week_view->col_offsets[col] - 1 - x,
				       canvas_height - 4 - y);
		}

		/* Draw the lines between each column. */
		if (col != 0) {
			gdk_draw_line (drawable, style->black_gc,
				       week_view->col_offsets[col] - x,
				       canvas_height - y,
				       week_view->col_offsets[col] - x,
				       canvas_height - y);
		}

		if (weekday == 5 && week_view->compress_weekend)
			weekday += 2;
		else
			weekday++;

		weekday = weekday % 7;
	}

	g_object_unref (layout);
}

/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_week_view_titles_item_point (GnomeCanvasItem *item, double x, double y,
			       gint cx, gint cy,
			       GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


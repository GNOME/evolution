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
 * EDayViewTimeItem - canvas item which displays the times down the left of
 * the EDayView.
 */

#include "e-day-view-time-item.h"


/* The spacing between items in the time column. GRID_X_PAD is the space down
   either side of the column, i.e. outside the main horizontal grid lines.
   HOUR_L_PAD & HOUR_R_PAD are the spaces on the left & right side of the
   big hour number (this is inside the horizontal grid lines).
   MIN_X_PAD is the spacing either side of the minute number. The smaller
   horizontal grid lines match with this.
   60_MIN_X_PAD is the space either side of the HH:MM display used when
   we are displaying 60 mins per row (inside the main grid lines). */
#define E_DVTMI_TIME_GRID_X_PAD	4
#define E_DVTMI_HOUR_L_PAD	4
#define E_DVTMI_HOUR_R_PAD	2
#define E_DVTMI_MIN_X_PAD	2
#define E_DVTMI_60_MIN_X_PAD	4


static void e_day_view_time_item_class_init (EDayViewTimeItemClass *class);
static void e_day_view_time_item_init (EDayViewTimeItem *dvtmitem);
static void e_day_view_time_item_set_arg (GtkObject *o,
					  GtkArg *arg,
					  guint arg_id);

static void e_day_view_time_item_update (GnomeCanvasItem *item,
					 double *affine,
					 ArtSVP *clip_path, int flags);
static void e_day_view_time_item_draw (GnomeCanvasItem *item,
				       GdkDrawable *drawable,
				       int x, int y,
				       int width, int height);
static double e_day_view_time_item_point (GnomeCanvasItem *item,
					  double x, double y,
					  int cx, int cy,
					  GnomeCanvasItem **actual_item);



static GnomeCanvasItemClass *parent_class;


/* The arguments we take */
enum {
	ARG_0,
	ARG_DAY_VIEW
};


GtkType
e_day_view_time_item_get_type (void)
{
	static GtkType e_day_view_time_item_type = 0;

	if (!e_day_view_time_item_type) {
		GtkTypeInfo e_day_view_time_item_info = {
			"EDayViewTimeItem",
			sizeof (EDayViewTimeItem),
			sizeof (EDayViewTimeItemClass),
			(GtkClassInitFunc) e_day_view_time_item_class_init,
			(GtkObjectInitFunc) e_day_view_time_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_day_view_time_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &e_day_view_time_item_info);
	}

	return e_day_view_time_item_type;
}


static void
e_day_view_time_item_class_init (EDayViewTimeItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type());

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EDayViewTimeItem::day_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_DAY_VIEW);

	object_class->set_arg = e_day_view_time_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_day_view_time_item_update;
	item_class->draw        = e_day_view_time_item_draw;
	item_class->point       = e_day_view_time_item_point;
}


static void
e_day_view_time_item_init (EDayViewTimeItem *dvtmitm)
{

}


static void
e_day_view_time_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EDayViewTimeItem *dvtmitem;

	item = GNOME_CANVAS_ITEM (o);
	dvtmitem = E_DAY_VIEW_TIME_ITEM (o);
	
	switch (arg_id){
	case ARG_DAY_VIEW:
		dvtmitem->day_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_day_view_time_item_update (GnomeCanvasItem *item,
			    double *affine,
			    ArtSVP *clip_path,
			    int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, clip_path, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}


/* Returns the minimum width needed for the column, by adding up all the
   maximum widths of the strings. The string widths are all calculated in
   the style_set handlers of EDayView and EDayViewTimeCanvas. */
gint
e_day_view_time_item_get_column_width (EDayViewTimeItem *dvtmitem)
{
	EDayView *day_view;

	day_view = dvtmitem->day_view;
	g_return_val_if_fail (day_view != NULL, 0);

	/* Calculate the width of each time column. */
	if (day_view->mins_per_row == 60) {
		dvtmitem->column_width = day_view->max_small_hour_width
			+ day_view->colon_width
			+ day_view->max_minute_width
			+ E_DVTMI_60_MIN_X_PAD * 2
			+ E_DVTMI_TIME_GRID_X_PAD * 2;
	} else {
		dvtmitem->column_width = day_view->max_large_hour_width
			+ day_view->max_minute_width
			+ E_DVTMI_MIN_X_PAD * 2
			+ E_DVTMI_HOUR_L_PAD
			+ E_DVTMI_HOUR_R_PAD
			+ E_DVTMI_TIME_GRID_X_PAD * 2;
	}

	return dvtmitem->column_width;
}


/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */

static void
e_day_view_time_item_draw (GnomeCanvasItem *canvas_item,
			   GdkDrawable	   *drawable,
			   int		    x,
			   int		    y,
			   int		    width,
			   int		    height)
{
	EDayView *day_view;
	EDayViewTimeItem *dvtmitem;
	gint time_hour_x1, time_hour_x2, time_min_x1;
	gint hour, minute, hour_y, min_y, hour_r, min_r, start_y;
	gint row, row_y, min_width, hour_width;
	GtkStyle *style;
	GdkFont *small_font, *large_font;
	GdkGC *fg_gc, *dark_gc;
	gchar buffer[16];

	dvtmitem = E_DAY_VIEW_TIME_ITEM (canvas_item);
	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = GTK_WIDGET (day_view)->style;
	small_font = style->font;
	large_font = day_view->large_font;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];

	/* Step through each row, drawing the horizontal grid lines for each
	   day column and the times. */
	time_hour_x1 = E_DVTMI_TIME_GRID_X_PAD - x;
	time_hour_x2 = dvtmitem->column_width - E_DVTMI_TIME_GRID_X_PAD - x;
	if (day_view->mins_per_row == 60) {
		min_r = time_hour_x2 - E_DVTMI_60_MIN_X_PAD;
	} else {
		time_min_x1 = time_hour_x2 - E_DVTMI_MIN_X_PAD * 2
			- day_view->max_minute_width;
		hour_r = time_min_x1 - E_DVTMI_HOUR_R_PAD;
		min_r = time_hour_x2 - E_DVTMI_MIN_X_PAD;
	}

	hour = day_view->first_hour_shown;
	hour_y = large_font->ascent + 2; /* FIXME */
	minute = day_view->first_minute_shown;
	min_y = small_font->ascent + 2; /* FIXME */
	start_y = 0 - MAX (day_view->row_height, hour_y + large_font->descent);
	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {
		if (row_y > start_y) {
			/* Draw the times down the left if needed. */
			if (min_r <= 0)
				continue;

			if (day_view->mins_per_row == 60) {
				gdk_draw_line (drawable, dark_gc,
					       time_hour_x1, row_y,
					       time_hour_x2, row_y);
				sprintf (buffer, "%02i:%02i", hour, minute);
				min_width = day_view->small_hour_widths[hour] + day_view->minute_widths[minute / 5] + day_view->colon_width;
				gdk_draw_string (drawable, small_font, fg_gc,
						 min_r - min_width,
						 row_y + min_y, buffer);
			} else {
				if (minute == 0) {
					gdk_draw_line (drawable, dark_gc,
						       time_hour_x1, row_y,
						       time_hour_x2, row_y);
					sprintf (buffer, "%02i", hour);
					hour_width = day_view->large_hour_widths[hour];
					gdk_draw_string (drawable, large_font,
							 fg_gc,
							 hour_r - hour_width,
							 row_y + hour_y,
							 buffer);
				} else {
					gdk_draw_line (drawable, dark_gc,
						       time_min_x1, row_y,
						       time_hour_x2, row_y);
				}

				if (day_view->mins_per_row != 30
				    || minute != 30) {
					sprintf (buffer, "%02i", minute);
					min_width = day_view->minute_widths[minute / 5];
					gdk_draw_string (drawable, small_font,
							 fg_gc,
							 min_r - min_width,
							 row_y + min_y,
							 buffer);
				}
			}
		}

		minute += day_view->mins_per_row;
		if (minute >= 60) {
			hour++;
			minute -= 60;
		}
	}
}


static double
e_day_view_time_item_point (GnomeCanvasItem *item, double x, double y,
			    int cx, int cy,
			    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}

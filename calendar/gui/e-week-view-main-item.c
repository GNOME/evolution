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
 * EWeekViewMainItem - displays the background grid and dates for the Week and
 * Month calendar views.
 */

#include <config.h>
#include "e-week-view-main-item.h"

static void e_week_view_main_item_class_init	(EWeekViewMainItemClass *class);
static void e_week_view_main_item_init		(EWeekViewMainItem *wvmitem);

static void e_week_view_main_item_set_arg	(GtkObject	 *o,
						 GtkArg		 *arg,
						 guint		  arg_id);
static void e_week_view_main_item_update	(GnomeCanvasItem *item,
						 double		 *affine,
						 ArtSVP		 *clip_path,
						 int		  flags);
static void e_week_view_main_item_draw		(GnomeCanvasItem *item,
						 GdkDrawable	 *drawable,
						 int		  x,
						 int		  y,
						 int		  width,
						 int		  height);
static void e_week_view_main_item_draw_day	(EWeekViewMainItem *wvmitem,
						 gint		   day,
						 GDate		  *date,
						 GdkDrawable       *drawable,
						 gint		   x,
						 gint		   y,
						 gint		   width,
						 gint		   height);
static double e_week_view_main_item_point	(GnomeCanvasItem *item,
						 double		  x,
						 double		  y,
						 int		  cx,
						 int		  cy,
						 GnomeCanvasItem **actual_item);


static GnomeCanvasItemClass *parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_WEEK_VIEW
};


GtkType
e_week_view_main_item_get_type (void)
{
	static GtkType e_week_view_main_item_type = 0;

	if (!e_week_view_main_item_type) {
		GtkTypeInfo e_week_view_main_item_info = {
			"EWeekViewMainItem",
			sizeof (EWeekViewMainItem),
			sizeof (EWeekViewMainItemClass),
			(GtkClassInitFunc) e_week_view_main_item_class_init,
			(GtkObjectInitFunc) e_week_view_main_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_week_view_main_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &e_week_view_main_item_info);
	}

	return e_week_view_main_item_type;
}


static void
e_week_view_main_item_class_init (EWeekViewMainItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type());

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EWeekViewMainItem::week_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_WEEK_VIEW);

	object_class->set_arg = e_week_view_main_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_week_view_main_item_update;
	item_class->draw        = e_week_view_main_item_draw;
	item_class->point       = e_week_view_main_item_point;
}


static void
e_week_view_main_item_init (EWeekViewMainItem *wvmitem)
{
	wvmitem->week_view = NULL;
}


static void
e_week_view_main_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EWeekViewMainItem *wvmitem;

	item = GNOME_CANVAS_ITEM (o);
	wvmitem = E_WEEK_VIEW_MAIN_ITEM (o);
	
	switch (arg_id){
	case ARG_WEEK_VIEW:
		wvmitem->week_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_week_view_main_item_update (GnomeCanvasItem *item,
			      double	      *affine,
			      ArtSVP	      *clip_path,
			      int	       flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->update) (item, affine, clip_path, flags);

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
e_week_view_main_item_draw (GnomeCanvasItem  *canvas_item,
			    GdkDrawable      *drawable,
			    int		      x,
			    int		      y,
			    int		      width,
			    int		      height)
{
	EWeekViewMainItem *wvmitem;
	EWeekView *week_view;
	GDate date;
	gint num_days, day, day_x, day_y, day_w, day_h;

#if 0
	g_print ("In e_week_view_main_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif

	wvmitem = E_WEEK_VIEW_MAIN_ITEM (canvas_item);
	week_view = wvmitem->week_view;
	g_return_if_fail (week_view != NULL);

	/* Step through each of the days. */
	date = week_view->first_day_shown;

	/* If no date has been set, we just use Dec 1999/January 2000. */
	if (!g_date_valid (&date))
		g_date_set_dmy (&date, 27, 12, 1999);

	num_days = week_view->display_month ? E_WEEK_VIEW_MAX_WEEKS * 7 : 7;
	for (day = 0; day < num_days; day++) {
		e_week_view_get_day_position (week_view, day,
					      &day_x, &day_y,
					      &day_w, &day_h);
		/* Skip any days which are outside the area. */
		if (day_x < x + width && day_x + day_w >= x
		    && day_y < y + height && day_y + day_h >= y) {
			e_week_view_main_item_draw_day (wvmitem, day, &date,
							drawable,
							day_x - x, day_y - y,
							day_w, day_h);
		}
		g_date_add_days (&date, 1);
	}
}


static void
e_week_view_main_item_draw_day (EWeekViewMainItem *wvmitem,
				gint		   day,
				GDate		  *date,
				GdkDrawable       *drawable,
				gint		   x,
				gint		   y,
				gint		   width,
				gint		   height)
{
	EWeekView *week_view;
	GtkStyle *style;
	GdkGC *fg_gc, *bg_gc, *light_gc, *dark_gc, *gc, *date_gc;
	GdkGC *selected_fg_gc, *selected_bg_gc;
	GdkFont *font;
	gint right_edge, bottom_edge, date_width, date_x, line_y;
	gboolean show_day_name, show_month_name, selected;
	gchar buffer[128], *format_string;
	gint month, day_of_month, max_width;
	GdkColor *bg_color;

#if 0
	g_print ("Drawing Day:%i at %i,%i\n", day, x, y);
#endif
	week_view = wvmitem->week_view;
	style = GTK_WIDGET (week_view)->style;
	font = style->font;
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	bg_gc = style->bg_gc[GTK_STATE_PRELIGHT];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	selected_fg_gc = style->fg_gc[GTK_STATE_SELECTED];
	selected_bg_gc = style->bg_gc[GTK_STATE_SELECTED];
	gc = week_view->main_gc;

	month = g_date_month (date);
	day_of_month = g_date_day (date);
	line_y = y + E_WEEK_VIEW_DATE_T_PAD + font->ascent
		+ font->descent	+ E_WEEK_VIEW_DATE_LINE_T_PAD;

	/* Draw the background of the day. In the month view odd months are
	   one color and even months another, so you can easily see when each
	   month starts (defaults are white for odd - January, March, ... and
	   light gray for even). In the week view the background is always the
	   same color, the color used for the odd months in the month view. */
	if (week_view->display_month && (month % 2 == 0))
		bg_color = &week_view->colors[E_WEEK_VIEW_COLOR_EVEN_MONTHS];
	else
		bg_color = &week_view->colors[E_WEEK_VIEW_COLOR_ODD_MONTHS];

	gdk_gc_set_foreground (gc, bg_color);
	gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);

	/* Draw the lines on the right and bottom of the cell. The canvas is
	   sized so that the lines on the right & bottom edges will be off the
	   edge of the canvas, so we don't have to worry about them. */
	right_edge = x + width - 1;
	bottom_edge = y + height - 1;

	gdk_draw_line (drawable, fg_gc,
		       right_edge, y, right_edge, bottom_edge);
	gdk_draw_line (drawable, fg_gc,
		       x, bottom_edge, right_edge, bottom_edge);

	/* If the day is selected, draw the blue background. */
	selected = TRUE;
	if (week_view->selection_start_day == -1
	    || week_view->selection_start_day > day
	    || week_view->selection_end_day < day)
		selected = FALSE;
	if (selected) {
		if (week_view->display_month)
			gdk_draw_rectangle (drawable, selected_bg_gc, TRUE,
					    x + 2, y + 1,
					    width - 5,
					    E_WEEK_VIEW_DATE_T_PAD - 1
					    + font->ascent + font->descent);
		else
			gdk_draw_rectangle (drawable, selected_bg_gc, TRUE,
					    x + 2, y + 1,
					    width - 5, line_y - y);
	}

	/* Display the date in the top of the cell.
	   In the week view, display the long format "10 January" in all cells,
	   or abbreviate it to "10 Jan" or "10" if that doesn't fit.
	   In the month view, only use the long format for the first cell and
	   the 1st of each month, otherwise use "10". */
	show_day_name = FALSE;
	show_month_name = FALSE;
	if (!week_view->display_month) {
		show_day_name = TRUE;
		show_month_name = TRUE;
	} else if (day == 0 || day_of_month == 1) {
		show_month_name = TRUE;
	}

	/* Now find the longest form of the date that will fit. */
	max_width = width - 4;
	format_string = NULL;
	if (show_day_name) {
		if (week_view->max_abbr_day_width +
		    week_view->digit_width * 2 + week_view->space_width * 2
		    + week_view->month_widths[month - 1] < max_width)
			format_string = "%a %d %B";
	}
	if (!format_string && show_month_name) {
		if (week_view->digit_width * 2 + week_view->space_width
		    + week_view->month_widths[month - 1] < max_width)
			format_string = "%d %B";
		else if (week_view->digit_width * 2 + week_view->space_width
		    + week_view->abbr_month_widths[month - 1] < max_width)
			format_string = "%d %b";
	}

	g_date_strftime (buffer, 128, format_string ? format_string : "%d",
			 date);
	date_width = gdk_string_width (font, buffer);
	date_x = x + width - date_width - E_WEEK_VIEW_DATE_R_PAD;
	date_x = MAX (date_x, x + 1);

	if (selected)
		date_gc = selected_fg_gc;
	else
		date_gc = fg_gc;
	gdk_draw_string (drawable, font, date_gc,
			 date_x, y + E_WEEK_VIEW_DATE_T_PAD + font->ascent,
			 buffer);

	/* Draw the line under the date. */
	if (!week_view->display_month) {
		gdk_draw_line (drawable, fg_gc,
			       x + E_WEEK_VIEW_DATE_LINE_L_PAD, line_y,
			       right_edge, line_y);
	}
}




/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_week_view_main_item_point (GnomeCanvasItem *item, double x, double y,
			     int cx, int cy,
			     GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}



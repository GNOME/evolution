/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
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
 * EWeekViewMainItem - displays the background grid and dates for the Week and
 * Month calendar views.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include "e-week-view-main-item.h"
#include "ea-calendar.h"

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

/* The arguments we take */
enum {
	ARG_0,
	ARG_WEEK_VIEW
};

G_DEFINE_TYPE (EWeekViewMainItem, e_week_view_main_item, GNOME_TYPE_CANVAS_ITEM);

static void
e_week_view_main_item_class_init (EWeekViewMainItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

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

	/* init the accessibility support for e_week_view_main_item */
 	e_week_view_main_item_a11y_init ();
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
	if (GNOME_CANVAS_ITEM_CLASS (e_week_view_main_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_week_view_main_item_parent_class)->update) (item, affine, clip_path, flags);

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

	num_days = week_view->multi_week_view ? week_view->weeks_shown * 7 : 7;
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
	GdkGC *gc;
	gint right_edge, bottom_edge, date_width, date_x, line_y;
	gboolean show_day_name, show_month_name, selected;
	gchar buffer[128], *format_string;
	gint month, day_of_month, max_width;
	GdkColor *bg_color;
	PangoFontDescription *font_desc;
	PangoContext *pango_context;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;

#if 0
	g_print ("Drawing Day:%i at %i,%i\n", day, x, y);
#endif
	week_view = wvmitem->week_view;
	style = gtk_widget_get_style (GTK_WIDGET (week_view));
	gc = week_view->main_gc;

	/* Set up Pango prerequisites */
	font_desc = style->font_desc;
	pango_context = gtk_widget_get_pango_context (GTK_WIDGET (week_view));
	font_metrics = pango_context_get_metrics (pango_context, font_desc,
						  pango_context_get_language (pango_context));

	g_return_if_fail (gc != NULL);

	month = g_date_month (date);
	day_of_month = g_date_day (date);
	line_y = y + E_WEEK_VIEW_DATE_T_PAD +
		PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
		PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)) +
		E_WEEK_VIEW_DATE_LINE_T_PAD;

	/* Draw the background of the day. In the month view odd months are
	   one color and even months another, so you can easily see when each
	   month starts (defaults are white for odd - January, March, ... and
	   light gray for even). In the week view the background is always the
	   same color, the color used for the odd months in the month view. */
	if (week_view->multi_week_view && (month % 2 == 0))
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

	gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_GRID]);
	gdk_draw_line (drawable, gc,
		       right_edge, y, right_edge, bottom_edge);
	gdk_draw_line (drawable, gc,
		       x, bottom_edge, right_edge, bottom_edge);

	/* If the day is selected, draw the blue background. */
	selected = TRUE;
	if (week_view->selection_start_day == -1
	    || week_view->selection_start_day > day
	    || week_view->selection_end_day < day)
		selected = FALSE;
	if (selected) {
		if (GTK_WIDGET_HAS_FOCUS (week_view))
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_SELECTED]);
		else
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_SELECTED_UNFOCUSSED]);

		if (week_view->multi_week_view) {
			gdk_draw_rectangle (drawable, gc, TRUE,
					    x + 2, y + 1,
					    width - 5,
					    E_WEEK_VIEW_DATE_T_PAD - 1 +
					    PANGO_PIXELS (pango_font_metrics_get_ascent (font_metrics)) +
					    PANGO_PIXELS (pango_font_metrics_get_descent (font_metrics)));
		} else {
			gdk_draw_rectangle (drawable, gc, TRUE,
					    x + 2, y + 1,
					    width - 5, line_y - y);
		}
	}
	
	/* Display the date in the top of the cell.
	   In the week view, display the long format "10 January" in all cells,
	   or abbreviate it to "10 Jan" or "10" if that doesn't fit.
	   In the month view, only use the long format for the first cell and
	   the 1st of each month, otherwise use "10". */
	show_day_name = FALSE;
	show_month_name = FALSE;
	if (!week_view->multi_week_view) {
		show_day_name = TRUE;
		show_month_name = TRUE;
	} else if (day == 0 || day_of_month == 1) {
		show_month_name = TRUE;
	}

	/* Now find the longest form of the date that will fit. */
	max_width = width - 4;
	format_string = NULL;
	if (show_day_name) {
		if (week_view->max_day_width + week_view->digit_width * 2
		    + week_view->space_width * 2
		    + week_view->month_widths[month - 1] < max_width)
			/* strftime format %A = full weekday name, %d = day of
			   month, %B = full month name. You can change the
			   order but don't change the specifiers or add
			   anything. */
			format_string = _("%A %d %B");
		else if (week_view->max_abbr_day_width
			 + week_view->digit_width * 2
			 + week_view->space_width * 2
			 + week_view->abbr_month_widths[month - 1] < max_width)
			/* strftime format %a = abbreviated weekday name,
			   %d = day of month, %b = abbreviated month name.
			   You can change the order but don't change the
			   specifiers or add anything. */
			format_string = _("%a %d %b");
	}
	if (!format_string && show_month_name) {
		if (week_view->digit_width * 2 + week_view->space_width
		    + week_view->month_widths[month - 1] < max_width)
			/* strftime format %d = day of month, %B = full
			   month name. You can change the order but don't
			   change the specifiers or add anything. */
			format_string = _("%d %B");
		else if (week_view->digit_width * 2 + week_view->space_width
		    + week_view->abbr_month_widths[month - 1] < max_width)
			/* strftime format %d = day of month, %b = abbreviated
			   month name. You can change the order but don't
			   change the specifiers or add anything. */
			format_string = _("%d %b");
	}

	if (selected) {
		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_DATES_SELECTED]);
	} else if (week_view->multi_week_view) {
		struct icaltimetype tt;

		/* Check if we are drawing today */		
		tt = icaltime_from_timet_with_zone (time (NULL), FALSE,
						    e_calendar_view_get_timezone (E_CALENDAR_VIEW (week_view)));
		if (g_date_year (date) == tt.year 
		    && g_date_month (date) == tt.month
		    && g_date_day (date) == tt.day)
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_TODAY]);
		else
			gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_DATES]);
	} else {
		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_DATES]);
	}

	g_date_strftime (buffer, sizeof (buffer),
			 format_string ? format_string : "%d", date);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), buffer);
	pango_layout_get_pixel_size (layout, &date_width, NULL);
	date_x = x + width - date_width - E_WEEK_VIEW_DATE_R_PAD;
	date_x = MAX (date_x, x + 1);

	gdk_draw_layout (drawable, gc,
			 date_x,
			 y + E_WEEK_VIEW_DATE_T_PAD,
			 layout);
	g_object_unref (layout);

	/* Draw the line under the date. */
	if (!week_view->multi_week_view) {
		gdk_gc_set_foreground (gc, &week_view->colors[E_WEEK_VIEW_COLOR_GRID]);
		gdk_draw_line (drawable, gc,
			       x + E_WEEK_VIEW_DATE_LINE_L_PAD, line_y,
			       right_edge, line_y);
	}

	pango_font_metrics_unref (font_metrics);
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



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

#include <config.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkradiomenuitem.h>
#include "e-day-view-time-item.h"
#include "../../e-util/e-gui-utils.h"


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
static gint e_day_view_time_item_event (GnomeCanvasItem *item,
					GdkEvent *event);
static void e_day_view_time_item_show_popup_menu (EDayViewTimeItem *dvtmitem,
						  GdkEvent *event);
static void e_day_view_time_item_on_set_divisions (GtkWidget *item,
						   EDayViewTimeItem *dvtmitem);
static void e_day_view_time_item_on_button_press (EDayViewTimeItem *dvtmitem,
						  GdkEvent *event);
static void e_day_view_time_item_on_button_release (EDayViewTimeItem *dvtmitem,
						    GdkEvent *event);
static void e_day_view_time_item_on_motion_notify (EDayViewTimeItem *dvtmitem,
						   GdkEvent *event);
static gint e_day_view_time_item_convert_position_to_row (EDayViewTimeItem *dvtmitem,
							  gint y);


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
	item_class->event       = e_day_view_time_item_event;
}


static void
e_day_view_time_item_init (EDayViewTimeItem *dvtmitem)
{
	dvtmitem->dragging_selection = FALSE;
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


static gint
e_day_view_time_item_event (GnomeCanvasItem *item,
			    GdkEvent *event)
{
	EDayViewTimeItem *dvtmitem;

	dvtmitem = E_DAY_VIEW_TIME_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			e_day_view_time_item_on_button_press (dvtmitem, event);
		} else if (event->button.button == 3) {
			e_day_view_time_item_show_popup_menu (dvtmitem, event);
			return TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1)
			e_day_view_time_item_on_button_release (dvtmitem,
								event);
		break;

	case GDK_MOTION_NOTIFY:
		e_day_view_time_item_on_motion_notify (dvtmitem, event);
		break;

	default:
		break;
	}

	return FALSE;
}


static void
e_day_view_time_item_show_popup_menu (EDayViewTimeItem *dvtmitem,
				      GdkEvent *event)
{
	static gint divisions[] = { 60, 30, 15, 10, 5 };
	EDayView *day_view;
	gint num_divisions = sizeof (divisions) / sizeof (divisions[0]);
	GtkWidget *menu, *item;
	gchar buffer[256];
	GSList *group = NULL;
	gint current_divisions, i;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	current_divisions = e_day_view_get_mins_per_row (day_view);

	menu = gtk_menu_new ();

	/* Make sure the menu is destroyed when it disappears. */
	e_auto_kill_popup_menu_on_hide (GTK_MENU (menu));

	for (i = 0; i < num_divisions; i++) {
		sprintf (buffer, _("%02i minute divisions"), divisions[i]);
		item = gtk_radio_menu_item_new_with_label (group, buffer);
		group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);

		if (current_divisions == divisions[i])
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);

		gtk_object_set_data (GTK_OBJECT (item), "divisions",
				     GINT_TO_POINTER (divisions[i]));

		gtk_signal_connect (GTK_OBJECT (item), "toggled",
				    e_day_view_time_item_on_set_divisions,
				    dvtmitem);
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);
}


static void
e_day_view_time_item_on_set_divisions (GtkWidget *item,
				       EDayViewTimeItem *dvtmitem)
{
	EDayView *day_view;
	gint divisions;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	if (!GTK_CHECK_MENU_ITEM (item)->active)
		return;

	divisions = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item),
							  "divisions"));
	e_day_view_set_mins_per_row (day_view, divisions);
}


static void
e_day_view_time_item_on_button_press (EDayViewTimeItem *dvtmitem,
				      GdkEvent *event)
{
	EDayView *day_view;
	GnomeCanvas *canvas;
	gint row;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (dvtmitem)->canvas;

	row = e_day_view_time_item_convert_position_to_row (dvtmitem,
							    event->button.y);

	if (row == -1)
		return;

	if (!GTK_WIDGET_HAS_FOCUS (day_view))
		gtk_widget_grab_focus (GTK_WIDGET (day_view));

	if (gdk_pointer_grab (GTK_LAYOUT (canvas)->bin_window, FALSE,
			      GDK_POINTER_MOTION_MASK
			      | GDK_BUTTON_RELEASE_MASK,
			      FALSE, NULL, event->button.time) == 0) {
		e_day_view_start_selection (day_view, -1, row);
		dvtmitem->dragging_selection = TRUE;
	}
}


static void
e_day_view_time_item_on_button_release (EDayViewTimeItem *dvtmitem,
					GdkEvent *event)
{
	EDayView *day_view;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	if (dvtmitem->dragging_selection) {
		gdk_pointer_ungrab (event->button.time);
		e_day_view_finish_selection (day_view);
		e_day_view_stop_auto_scroll (day_view);
	}

	dvtmitem->dragging_selection = FALSE;
}


static void
e_day_view_time_item_on_motion_notify (EDayViewTimeItem *dvtmitem,
				       GdkEvent *event)
{
	EDayView *day_view;
	GnomeCanvas *canvas;
	gdouble window_y;
	gint y, row;

	if (!dvtmitem->dragging_selection)
		return;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (dvtmitem)->canvas;

	y = event->motion.y;
	row = e_day_view_time_item_convert_position_to_row (dvtmitem, y);

	if (row != -1) {
		gnome_canvas_world_to_window (canvas, 0, event->motion.y,
					      NULL, &window_y);
		e_day_view_update_selection (day_view, -1, row);
		e_day_view_check_auto_scroll (day_view, -1, (gint) window_y);
	}
}


/* Returns the row corresponding to the y position, or -1. */
static gint
e_day_view_time_item_convert_position_to_row (EDayViewTimeItem *dvtmitem,
					      gint y)
{
	EDayView *day_view;
	gint row;

	day_view = dvtmitem->day_view;
	g_return_val_if_fail (day_view != NULL, -1);

	if (y < 0)
		return -1;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return -1;

	return row;
}

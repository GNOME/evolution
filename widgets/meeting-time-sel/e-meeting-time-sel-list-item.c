/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
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
 * EMeetingTimeSelectorListItem - A GnomeCanvasItem covering the entire attendee
 * list. It just draws the grid lines between the rows and after the icon
 * column. 
 */

#include <config.h>
#include <time.h>
#include "../../e-util/e-canvas.h"
#include "e-meeting-time-sel-list-item.h"
#include "e-meeting-time-sel.h"

/* This is the size of our icons. */
#define E_MEETING_TIME_SELECTOR_ICON_WIDTH 19
#define E_MEETING_TIME_SELECTOR_ICON_HEIGHT 16

#include "e-meeting-time-sel-mail.xpm"
#include "e-meeting-time-sel-no-mail.xpm"

static void e_meeting_time_selector_list_item_class_init (EMeetingTimeSelectorListItemClass *mtsl_item_class);
static void e_meeting_time_selector_list_item_init (EMeetingTimeSelectorListItem *mtsl_item);
static void e_meeting_time_selector_list_item_destroy (GtkObject *object);

static void e_meeting_time_selector_list_item_set_arg (GtkObject *o, GtkArg *arg,
						     guint arg_id);
static void e_meeting_time_selector_list_item_realize (GnomeCanvasItem *item);
static void e_meeting_time_selector_list_item_unrealize (GnomeCanvasItem *item);
static void e_meeting_time_selector_list_item_update (GnomeCanvasItem *item,
						      double *affine,
						      ArtSVP *clip_path, int flags);
static void e_meeting_time_selector_list_item_draw (GnomeCanvasItem *item,
						    GdkDrawable *drawable,
						    int x, int y,
						    int width, int height);
static double e_meeting_time_selector_list_item_point (GnomeCanvasItem *item,
						       double x, double y,
						       int cx, int cy,
						       GnomeCanvasItem **actual_item);
static gint e_meeting_time_selector_list_item_event (GnomeCanvasItem *item,
						     GdkEvent	 *event);
static gboolean e_meeting_time_selector_list_item_button_press (EMeetingTimeSelectorListItem *mtsl_item,
								GdkEvent *event);


static GnomeCanvasItemClass *e_meeting_time_selector_list_item_parent_class;

/* The arguments we take */
enum {
	ARG_0,
	ARG_MEETING_TIME_SELECTOR
};


GtkType
e_meeting_time_selector_list_item_get_type (void)
{
	static GtkType e_meeting_time_selector_list_item_type = 0;

	if (!e_meeting_time_selector_list_item_type) {
		GtkTypeInfo e_meeting_time_selector_list_item_info = {
			"EMeetingTimeSelectorListItem",
			sizeof (EMeetingTimeSelectorListItem),
			sizeof (EMeetingTimeSelectorListItemClass),
			(GtkClassInitFunc) e_meeting_time_selector_list_item_class_init,
			(GtkObjectInitFunc) e_meeting_time_selector_list_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_meeting_time_selector_list_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &e_meeting_time_selector_list_item_info);
	}

	return e_meeting_time_selector_list_item_type;
}


static void
e_meeting_time_selector_list_item_class_init (EMeetingTimeSelectorListItemClass *mtsl_item_class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	e_meeting_time_selector_list_item_parent_class = gtk_type_class (gnome_canvas_item_get_type());

	object_class = (GtkObjectClass *) mtsl_item_class;
	item_class = (GnomeCanvasItemClass *) mtsl_item_class;

	gtk_object_add_arg_type ("EMeetingTimeSelectorListItem::meeting_time_selector",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_MEETING_TIME_SELECTOR);

	object_class->destroy = e_meeting_time_selector_list_item_destroy;
	object_class->set_arg = e_meeting_time_selector_list_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->realize     = e_meeting_time_selector_list_item_realize;
	item_class->unrealize   = e_meeting_time_selector_list_item_unrealize;
	item_class->update      = e_meeting_time_selector_list_item_update;
	item_class->draw        = e_meeting_time_selector_list_item_draw;
	item_class->point       = e_meeting_time_selector_list_item_point;
	item_class->event       = e_meeting_time_selector_list_item_event;
}


static void
e_meeting_time_selector_list_item_init (EMeetingTimeSelectorListItem *mtsl_item)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (mtsl_item);
	GdkColormap *colormap;

	mtsl_item->mts = NULL;

	colormap = gtk_widget_get_default_colormap ();
	mtsl_item->mail_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mtsl_item->mail_icon_mask, NULL, e_meeting_time_sel_mail_xpm);
	mtsl_item->no_mail_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mtsl_item->no_mail_icon_mask, NULL, e_meeting_time_sel_no_mail_xpm);

	item->x1 = 0;
	item->y1 = 0;
	item->x2 = 0;
	item->y2 = 0;
}


static void
e_meeting_time_selector_list_item_destroy (GtkObject *object)
{
	EMeetingTimeSelectorListItem *mtsl_item;

	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (object);

	gdk_pixmap_unref (mtsl_item->mail_icon);
	gdk_pixmap_unref (mtsl_item->no_mail_icon);
	gdk_bitmap_unref (mtsl_item->mail_icon_mask);
	gdk_bitmap_unref (mtsl_item->mail_icon_mask);

	if (GTK_OBJECT_CLASS (e_meeting_time_selector_list_item_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (e_meeting_time_selector_list_item_parent_class)->destroy)(object);
}


static void
e_meeting_time_selector_list_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMeetingTimeSelectorListItem *mtsl_item;

	item = GNOME_CANVAS_ITEM (o);
	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (o);
	
	switch (arg_id){
	case ARG_MEETING_TIME_SELECTOR:
		mtsl_item->mts = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_meeting_time_selector_list_item_realize (GnomeCanvasItem *item)
{
	GnomeCanvas *canvas;
	GdkWindow *window;
	EMeetingTimeSelectorListItem *mtsl_item;

	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->realize)
		(*GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->realize)(item);

	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (item);

	canvas = item->canvas;
	window = GTK_WIDGET (canvas)->window;

	mtsl_item->main_gc = gdk_gc_new (window);
}


static void
e_meeting_time_selector_list_item_unrealize (GnomeCanvasItem *item)
{
	EMeetingTimeSelectorListItem *mtsl_item;

	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (item);

	gdk_gc_unref (mtsl_item->main_gc);
	mtsl_item->main_gc = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->unrealize)
		(*GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->unrealize)(item);
}


static void
e_meeting_time_selector_list_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_meeting_time_selector_list_item_parent_class)->update) (item, affine, clip_path, flags);

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
e_meeting_time_selector_list_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	EMeetingTimeSelectorListItem *mtsl_item;
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorAttendee *attendee;
	GdkGC *gc;
	GdkFont *font;
	gint row, row_y, icon_x, icon_y;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (item);
	mts = mtsl_item->mts;
	gc = mtsl_item->main_gc;

	gdk_gc_set_foreground (gc, &mts->attendee_list_bg_color);
	gdk_draw_rectangle (drawable, gc, TRUE, 0, 0, width, height);

	gdk_gc_set_foreground (gc, &mts->grid_unused_color);
	gdk_draw_line (drawable, gc, 24 - x, 0, 24 - x, height);

	/* Draw the grid line across the top of the row. */
	row = y / mts->row_height;
	row_y = row * mts->row_height - y;
	while (row_y < height) {
		gdk_draw_line (drawable, gc, 0, row_y, width, row_y);
		row_y += mts->row_height;
	}

	row = y / mts->row_height;
	row_y = row * mts->row_height - y;
	icon_x = (E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH - E_MEETING_TIME_SELECTOR_ICON_WIDTH + 1) / 2 - x;
	icon_y = row_y + (mts->row_height - E_MEETING_TIME_SELECTOR_ICON_HEIGHT + 1) / 2;
	while (row < mts->attendees->len && row_y < height) {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);

		gdk_gc_set_clip_origin (gc, icon_x, icon_y);

		if (attendee->send_meeting_to) {
			pixmap = mtsl_item->mail_icon;
			mask = mtsl_item->mail_icon_mask;
		} else {
			pixmap = mtsl_item->no_mail_icon;
			mask = mtsl_item->no_mail_icon_mask;
		}

		gdk_gc_set_clip_mask (gc, mask);
		gdk_draw_pixmap (drawable, gc, pixmap, 0, 0,
				 icon_x, icon_y, 24, 24);

		row++;
		row_y += mts->row_height;
		icon_y += mts->row_height;
	}
	gdk_gc_set_clip_mask (gc, NULL);

	/* Draw 'Click here to add attendee' on the last dummy row. */
	row_y = mts->attendees->len * mts->row_height;

	font = GTK_WIDGET (mts)->style->font;
	gdk_gc_set_foreground (gc, &mts->grid_unused_color);
	gdk_draw_string (drawable, font, gc,
			 E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH
			 + E_MEETING_TIME_SELECTOR_TEXT_X_PAD - x,
			 row_y + E_MEETING_TIME_SELECTOR_TEXT_Y_PAD
			 + font->ascent + 1 - y,
			 "Click here to add attendee");
}


/* This is supposed to return the nearest item the the point and the distance.
   Since we cover the entire canvas we just return ourself and 0 for the
   distance. This is needed so that we get button/motion events. */
static double
e_meeting_time_selector_list_item_point (GnomeCanvasItem *item,
					 double x, double y,
					 int cx, int cy,
					 GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


static gint
e_meeting_time_selector_list_item_event (GnomeCanvasItem *item,
					 GdkEvent *event)
{
	EMeetingTimeSelectorListItem *mtsl_item;

	mtsl_item = E_MEETING_TIME_SELECTOR_LIST_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return e_meeting_time_selector_list_item_button_press (mtsl_item, event);
	case GDK_BUTTON_RELEASE:
		break;
	case GDK_MOTION_NOTIFY:
		break;
	default:
		break;
	}

	return FALSE;
}


static gboolean
e_meeting_time_selector_list_item_button_press (EMeetingTimeSelectorListItem *mtsl_item,
						GdkEvent *event)
{
	EMeetingTimeSelector *mts;
	EMeetingTimeSelectorAttendee *attendee;
	gint row;
	gboolean return_val;
	GtkAdjustment *adjustment;

	mts = mtsl_item->mts;
	row = event->button.y / mts->row_height;

	g_print ("In e_meeting_time_selector_list_item_button_press: %g,%g row:%i\n",
		 event->button.x, event->button.y, row);

	if (event->button.x >= E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH) {
		if (row < mts->attendees->len) {
			attendee = &g_array_index (mts->attendees, EMeetingTimeSelectorAttendee, row);
			gtk_signal_emit_by_name (GTK_OBJECT (attendee->text_item),
						 "event", event, &return_val);
			return return_val;
		} else {
			row = e_meeting_time_selector_attendee_add (mts, "",
								    NULL);

			/* Scroll down to show the last line.?? */
#if 0
			adjustment = GTK_LAYOUT (mts->display_main)->vadjustment;
			adjustment->value = adjustment->upper - adjustment->page_size;
			gtk_adjustment_value_changed (adjustment);
#endif

			attendee = &g_array_index (mts->attendees, EMeetingTimeSelectorAttendee, row);
			e_canvas_item_grab_focus (attendee->text_item);
			return TRUE;
		}
	} else {
		attendee = &g_array_index (mts->attendees,
					   EMeetingTimeSelectorAttendee, row);

		attendee->send_meeting_to = !attendee->send_meeting_to;

		gnome_canvas_request_redraw (GNOME_CANVAS_ITEM (mtsl_item)->canvas,
					     0, row * mts->row_height,
					     E_MEETING_TIME_SELECTOR_ICON_COLUMN_WIDTH,
					     (row + 1) * mts->row_height);
		return TRUE;
	}
}


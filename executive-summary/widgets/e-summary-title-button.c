/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-title-buttons.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gal/util/e-util.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "e-summary-title-button.h"

#define PARENT_TYPE (gnome_canvas_rect_get_type ())

enum {
	ARG_0,
	ARG_PIXBUF,
	ARG_X,
	ARG_Y
};

enum {
	CLICKED,
	LAST_SIGNAL
};

static void e_summary_title_button_destroy (GtkObject *object);
static void e_summary_title_button_set_arg (GtkObject *object,
					    GtkArg *arg,
					    guint arg_id);
static void e_summary_title_button_get_arg (GtkObject *object,
					    GtkArg *arg,
					    guint arg_id);
static void e_summary_title_button_class_init (ESummaryTitleButtonClass *estb_class);
static void e_summary_title_button_init (ESummaryTitleButton *estb);
static double e_summary_title_button_point (GnomeCanvasItem *item,
					    double x,
					    double y,
					    int cx,
					    int cy,
					    GnomeCanvasItem **actual_item);
static void e_summary_title_button_update (GnomeCanvasItem *item,
					   double affine[6],
					   ArtSVP *clip_path,
					   gint flags);
static void e_summary_title_button_draw (GnomeCanvasItem *item,
					 GdkDrawable *drawable,
					 int x, int y,
					 int width, int height);
static gint e_summary_title_button_event (GnomeCanvasItem *item,
					  GdkEvent *event);

static GnomeCanvasRectClass *parent_class;
static guint estb_signals[LAST_SIGNAL] = { 0 };

struct _ESummaryTitleButtonPrivate {
	GdkPixbuf *pixbuf;
	double x, y;
	int width, height;

	int in_button : 1;
	int button_down : 1;
};

static void
e_summary_title_button_destroy (GtkObject *object)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	
	estb = E_SUMMARY_TITLE_BUTTON (object);
	priv = estb->private;
	
	if (priv == NULL)
		return;
	
	gdk_pixbuf_unref (priv->pixbuf);
	
	g_free (priv);
	estb->private = NULL;
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
e_summary_title_button_set_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	gboolean update = FALSE;
	
	estb = E_SUMMARY_TITLE_BUTTON (object);
	priv = estb->private;
	
	switch (arg_id) {
	case ARG_PIXBUF:
		if (priv->pixbuf)
			gdk_pixbuf_unref (priv->pixbuf);
		
		priv->pixbuf = GTK_VALUE_POINTER (*arg);
		gdk_pixbuf_ref (priv->pixbuf);
		
		priv->width = gdk_pixbuf_get_width (priv->pixbuf);
		priv->height = gdk_pixbuf_get_height (priv->pixbuf);
		
		update = TRUE;
		break;
		
	case ARG_X: 
		priv->x = GTK_VALUE_DOUBLE (*arg);
		break;
		
	case ARG_Y:
		priv->y = GTK_VALUE_DOUBLE (*arg);
		break;
		
	default:
		break;
		
	}
	
	if (update)
		gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (estb));
	
}

static void
e_summary_title_button_get_arg (GtkObject *object,
				GtkArg *arg,
				guint arg_id)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	
	estb = E_SUMMARY_TITLE_BUTTON (object);
	priv = estb->private;
	
	switch (arg_id) {
	case ARG_PIXBUF:
		GTK_VALUE_POINTER (*arg) = priv->pixbuf;
		break;
		
	case ARG_X:
		GTK_VALUE_DOUBLE (*arg) = priv->x;
		break;
		
	case ARG_Y:
		GTK_VALUE_DOUBLE (*arg) = priv->y;
		break;
		
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
		
	}
}

static void
e_summary_title_button_init (ESummaryTitleButton *estb)
{
	estb->private = g_new0 (ESummaryTitleButtonPrivate, 1);
}

static void
e_summary_title_button_class_init (ESummaryTitleButtonClass *estb_class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;
	
	object_class = (GtkObjectClass *) estb_class;
	item_class = (GnomeCanvasItemClass *) estb_class;
	
	object_class->destroy = e_summary_title_button_destroy;
	object_class->set_arg = e_summary_title_button_set_arg;
	object_class->get_arg = e_summary_title_button_get_arg;
	
	item_class->draw = e_summary_title_button_draw;
	item_class->point = e_summary_title_button_point;
	item_class->update = e_summary_title_button_update;
	item_class->event = e_summary_title_button_event;
	
	gtk_object_add_arg_type ("ESummaryTitleButton::pixbuf",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PIXBUF);
	gtk_object_add_arg_type ("ESummaryTitleButton::x",
				 GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE,
				 ARG_X);
	gtk_object_add_arg_type ("ESummaryTitleButton::y",
				 GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE,
				 ARG_Y);
	estb_signals[CLICKED] = gtk_signal_new ("clicked", GTK_RUN_LAST,
						object_class->type,
						GTK_SIGNAL_OFFSET (ESummaryTitleButtonClass,
								   clicked),
						gtk_marshal_NONE__NONE,
						GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, estb_signals, LAST_SIGNAL);
	
	parent_class = gtk_type_class (PARENT_TYPE);
}

E_MAKE_TYPE (e_summary_title_button, "ESummaryTitleButton", 
	     ESummaryTitleButton, e_summary_title_button_class_init,
	     e_summary_title_button_init, PARENT_TYPE);

static double
e_summary_title_button_point (GnomeCanvasItem *item,
			      double x,
			      double y,
			      int cx,
			      int cy,
			      GnomeCanvasItem **actual_item)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	double d = 1.0;
	
	estb = E_SUMMARY_TITLE_BUTTON (item);
	priv = estb->private;
	
	if (x >= priv->x && x <= priv->x + gdk_pixbuf_get_width (priv->pixbuf)
	    && y >= priv->y && y <= priv->y + gdk_pixbuf_get_height (priv->pixbuf)) {
		d = 0.0;
		*actual_item = item;
	}

	return d;
}

static void
get_bounds (ESummaryTitleButton *estb,
	    double *px1, double *py1,
	    double *px2, double *py2)
{
	GnomeCanvasItem *item;
	ESummaryTitleButtonPrivate *priv;
	double x1, y1, x2, y2;
	int cx1, cy1, cx2, cy2;
	
	item = GNOME_CANVAS_ITEM (estb);
	priv = estb->private;
	
	x1 = priv->x;
	y1 = priv->y;
	x2 = x1 + priv->width;
	y2 = y1 + priv->height;
	
	gnome_canvas_item_i2w (item, &x1, &y1);
	gnome_canvas_item_i2w (item, &x2, &x2);
	gnome_canvas_w2c (item->canvas, x1, y1, &cx1, &cy1);
	gnome_canvas_w2c (item->canvas, x2, y2, &cx2, &cy2);
	
	*px1 = cx1;
	*py1 = cy1;
	*px2 = cx2;
	*py2 = cy2;
}

static void
e_summary_title_button_update (GnomeCanvasItem *item,
			       double affine[6],
			       ArtSVP *clip_path,
			       gint flags)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	double x1, y1, x2, y2;
	
	estb = E_SUMMARY_TITLE_BUTTON (item);
	priv = estb->private;
	
	get_bounds (estb, &x1, &y1, &x2, &y2);
	gnome_canvas_update_bbox (item, (int) x1, (int) y1, (int) x2, (int) y2);
}

static void
e_summary_title_button_draw (GnomeCanvasItem *item,
			     GdkDrawable *drawable,
			     int x, int y,
			     int width, int height)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;
	double i2w[6], w2c[6], i2c[6];
	int x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint c1, c2;
	GdkGC *gc;
	
	estb = E_SUMMARY_TITLE_BUTTON (item);
	priv = estb->private;
	
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->draw)
		(* GNOME_CANVAS_ITEM_CLASS (parent_class)->draw) (item, drawable, x, y, width, height);
	
	gnome_canvas_item_i2w_affine (item, i2w);
	gnome_canvas_w2c_affine (item->canvas, w2c);
	art_affine_multiply (i2c, i2w, w2c);
	
	i1.x = priv->x;
	i1.y = priv->y;
	i2.x = i1.x + priv->width + 4;
	i2.y = i1.y + priv->height + 4;
	art_affine_point (&c1, &i1, i2c);
	art_affine_point (&c2, &i2, i2c);
	x1 = c1.x;
	y1 = c1.y;
	x2 = c2.x;
	y2 = c2.y;
	
	gc = gdk_gc_new (item->canvas->layout.bin_window);
	gdk_draw_rectangle (drawable, gc,
			    FALSE, x1 - x,
			    y1 - y,
			    x2 - x1,
			    y2 - y1);
	gdk_gc_unref (gc);
	
	gdk_pixbuf_render_to_drawable_alpha (priv->pixbuf,
					     drawable,
					     0, 0,
					     x1 + 2, y1 + 2,
					     priv->width, priv->height,
					     GDK_PIXBUF_ALPHA_BILEVEL,
					     127,
					     GDK_RGB_DITHER_NORMAL,
					     0, 0);
}

static gint
e_summary_title_button_event (GnomeCanvasItem *item,
			      GdkEvent *event)
{
	ESummaryTitleButton *estb;
	ESummaryTitleButtonPrivate *priv;

	estb = E_SUMMARY_TITLE_BUTTON (item);
	priv = estb->private;

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		priv->in_button = TRUE;
		break;

	case GDK_LEAVE_NOTIFY:
		priv->in_button = FALSE;
		break;

	case GDK_BUTTON_PRESS:
		if (priv->in_button) {
			priv->button_down = TRUE;
			gnome_canvas_item_grab (item,
						GDK_LEAVE_NOTIFY_MASK |
						GDK_ENTER_NOTIFY_MASK |
						GDK_POINTER_MOTION_MASK |
						GDK_BUTTON_RELEASE_MASK,
						NULL, event->button.time);
		}
		break;
	
	case GDK_BUTTON_RELEASE:
		priv->button_down = FALSE;
		gnome_canvas_item_ungrab (item, event->button.time);

		if (priv->in_button) {
			gtk_signal_emit (GTK_OBJECT (estb), estb_signals[CLICKED]);
		}
		break;
		
	default:
		return TRUE;
	}

	return FALSE;
}

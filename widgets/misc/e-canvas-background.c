/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-canvas-background.c - background color for canvas.
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "e-canvas-background.h"

#include <math.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkkeysyms.h>
#include "gal/widgets/e-hsv-utils.h"
#include "gal/widgets/e-canvas.h"
#include "gal/widgets/e-canvas-utils.h"
#include "gal/util/e-util.h"
#include <string.h>

#define PARENT_OBJECT_TYPE gnome_canvas_item_get_type ()

#define d(x)

struct _ECanvasBackgroundPrivate {
	guint rgba;		/* Fill color, RGBA */
	GdkColor color;		/* Fill color */
	GdkBitmap *stipple;	/* Stipple for fill */
	GdkGC *gc;			/* GC for filling */
	double x1;
	double x2;
	double y1;
	double y2;

	guint needs_redraw : 1;
};

static GnomeCanvasItemClass *parent_class;

enum {
	ARG_0,
	ARG_FILL_COLOR,
	ARG_FILL_COLOR_GDK,
	ARG_FILL_COLOR_RGBA,
	ARG_FILL_STIPPLE,
	ARG_X1,
	ARG_X2,
	ARG_Y1,
	ARG_Y2,
};

static void
get_color(ECanvasBackground *ecb)
{
	int n;
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (ecb);

	n = 0;
	gdk_color_context_get_pixels (item->canvas->cc,
				      &ecb->priv->color.red,
				      &ecb->priv->color.green,
				      &ecb->priv->color.blue,
				      1,
				      &ecb->priv->color.pixel,
				      &n);
}

static void
ecb_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	double   i2c [6];
	ArtPoint c1, c2, i1, i2;
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	/* Wrong BBox's are the source of redraw nightmares */

	gnome_canvas_item_i2c_affine (GNOME_CANVAS_ITEM (ecb), i2c);
	
	i1.x = ecb->priv->x1;
	i1.y = ecb->priv->y1;
	i2.x = ecb->priv->x2;
	i2.y = ecb->priv->y2;
	art_affine_point (&c1, &i1, i2c);
	art_affine_point (&c2, &i2, i2c);

	if (ecb->priv->x1 < 0)
		c1.x = -(double)UINT_MAX;

	if (ecb->priv->y1 < 0)
		c1.y = -(double)UINT_MAX;

	if (ecb->priv->x2 < 0)
		c2.x = (double)UINT_MAX;

	if (ecb->priv->y2 < 0)
		c2.y = (double)UINT_MAX;

	*x1 = c1.x;
	*y1 = c1.y;
	*x2 = c2.x + 1;
	*y2 = c2.y + 1;
}

/*
 * GnomeCanvasItem::update method
 */
static void
ecb_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ArtPoint o1, o2;
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS (parent_class)->update (item, affine, clip_path, flags);

	o1.x = item->x1;
	o1.y = item->y1;
	o2.x = item->x2;
	o2.y = item->y2;

	ecb_bounds (item, &item->x1, &item->y1, &item->x2, &item->y2);
	if (item->x1 != o1.x ||
	    item->y1 != o1.y ||
	    item->x2 != o2.x ||
	    item->y2 != o2.y) {
		gnome_canvas_request_redraw (item->canvas, o1.x, o1.y, o2.x, o2.y);
		ecb->priv->needs_redraw = 1;
	}

	if (ecb->priv->needs_redraw) {
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1,
					     item->x2, item->y2);
		ecb->priv->needs_redraw = 0;
	}
}

/* Sets the stipple pattern for the text */
static void
set_stipple (ECanvasBackground *ecb, GdkBitmap *stipple, int use_value)
{
	if (use_value) {
		if (ecb->priv->stipple)
			gdk_bitmap_unref (ecb->priv->stipple);

		ecb->priv->stipple = stipple;
		if (stipple)
			gdk_bitmap_ref (stipple);
	}

	if (ecb->priv->gc) {
		if (stipple) {
			gdk_gc_set_stipple (ecb->priv->gc, stipple);
			gdk_gc_set_fill (ecb->priv->gc, GDK_STIPPLED);
		} else
			gdk_gc_set_fill (ecb->priv->gc, GDK_SOLID);
	}
}

static void
ecb_destroy (GtkObject *object)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (object);

	if (ecb->priv->stipple)
		gdk_bitmap_unref (ecb->priv->stipple);
	ecb->priv->stipple = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
                GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
ecb_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ECanvasBackground *ecb;

	GdkColor color = { 0, 0, 0, 0, };
	GdkColor *pcolor;
	gboolean color_changed = FALSE;

	item = GNOME_CANVAS_ITEM (o);
	ecb = E_CANVAS_BACKGROUND (o);

	switch (arg_id){
	case ARG_FILL_COLOR:
		if (GTK_VALUE_STRING (*arg))
			gdk_color_parse (GTK_VALUE_STRING (*arg), &color);

		ecb->priv->rgba = ((color.red & 0xff00) << 16 |
				   (color.green & 0xff00) << 8 |
				   (color.blue & 0xff00) |
				   0xff);
		color_changed = TRUE;
		break;

	case ARG_FILL_COLOR_GDK:
		pcolor = GTK_VALUE_BOXED (*arg);
		if (pcolor) {
			color = *pcolor;
		}

		ecb->priv->rgba = ((color.red & 0xff00) << 16 |
				   (color.green & 0xff00) << 8 |
				   (color.blue & 0xff00) |
				   0xff);
		color_changed = TRUE;
		break;

        case ARG_FILL_COLOR_RGBA:
		ecb->priv->rgba = GTK_VALUE_UINT (*arg);
		color.red = ((ecb->priv->rgba >> 24) & 0xff) * 0x101;
		color.green = ((ecb->priv->rgba >> 16) & 0xff) * 0x101;
		color.blue = ((ecb->priv->rgba >> 8) & 0xff) * 0x101;
		color_changed = TRUE;
		break;

	case ARG_FILL_STIPPLE:
		set_stipple (ecb, GTK_VALUE_BOXED (*arg), TRUE);
		break;

	case ARG_X1:
		ecb->priv->x1 = GTK_VALUE_DOUBLE (*arg);
		break;
	case ARG_X2:
		ecb->priv->x2 = GTK_VALUE_DOUBLE (*arg);
		break;
	case ARG_Y1:
		ecb->priv->y1 = GTK_VALUE_DOUBLE (*arg);
		break;
	case ARG_Y2:
		ecb->priv->y2 = GTK_VALUE_DOUBLE (*arg);
		break;
	}

	if (color_changed) {
		ecb->priv->color = color;

		if (GNOME_CANVAS_ITEM_REALIZED & GTK_OBJECT_FLAGS(item)) {
			get_color (ecb);
			if (!item->canvas->aa) {
				gdk_gc_set_foreground (ecb->priv->gc, &ecb->priv->color);
			}
		}
	}

	ecb->priv->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM(ecb));
}

static void
ecb_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ECanvasBackground *ecb;

	item = GNOME_CANVAS_ITEM (o);
	ecb = E_CANVAS_BACKGROUND (o);

	switch (arg_id){
	case ARG_FILL_COLOR_GDK:
		GTK_VALUE_BOXED (*arg) = gdk_color_copy (&ecb->priv->color);
		break;
        case ARG_FILL_COLOR_RGBA:
		GTK_VALUE_UINT (*arg) = ecb->priv->rgba;
		break;
	case ARG_FILL_STIPPLE:
		GTK_VALUE_BOXED (*arg) = ecb->priv->stipple;
		break;
	case ARG_X1:
		GTK_VALUE_DOUBLE (*arg) = ecb->priv->x1;
		break;
	case ARG_X2:
		GTK_VALUE_DOUBLE (*arg) = ecb->priv->x2;
		break;
	case ARG_Y1:
		GTK_VALUE_DOUBLE (*arg) = ecb->priv->y1;
		break;
	case ARG_Y2:
		GTK_VALUE_DOUBLE (*arg) = ecb->priv->y2;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
ecb_init (GnomeCanvasItem *item)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	ecb->priv               = g_new (ECanvasBackgroundPrivate, 1);

	ecb->priv->color        = (GdkColor) {0,};
	ecb->priv->stipple      = NULL;
	ecb->priv->gc           = NULL;
	ecb->priv->x1           = -1.0;
	ecb->priv->x2           = -1.0;
	ecb->priv->y1           = -1.0;
	ecb->priv->y2           = -1.0;
}

static void
ecb_realize (GnomeCanvasItem *item)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);
	
	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->realize)
                GNOME_CANVAS_ITEM_CLASS (parent_class)->realize (item);

	ecb->priv->gc = gdk_gc_new (item->canvas->layout.bin_window);
	get_color (ecb);
	if (!item->canvas->aa)
		gdk_gc_set_foreground (ecb->priv->gc, &ecb->priv->color);

	set_stipple (ecb, NULL, FALSE);
	
	ecb->priv->needs_redraw = 1;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (ecb));
}

static void
ecb_unrealize (GnomeCanvasItem *item)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	gdk_gc_unref (ecb->priv->gc);
	ecb->priv->gc = NULL;

	if (GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize)
                GNOME_CANVAS_ITEM_CLASS (parent_class)->unrealize (item);
}

static void
ecb_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);
	int x1, x2, y1, y2;
	double i2c [6];
	ArtPoint upper_left, lower_right, ecb_base_point;
	
	/*
	 * Find out our real position after grouping
	 */
	gnome_canvas_item_i2c_affine (item, i2c);
	ecb_base_point.x = ecb->priv->x1;
	ecb_base_point.y = ecb->priv->y1;
	art_affine_point (&upper_left, &ecb_base_point, i2c);

	ecb_base_point.x = ecb->priv->x2;
	ecb_base_point.y = ecb->priv->y2;
	art_affine_point (&lower_right, &ecb_base_point, i2c);

	x1 = 0;
	y1 = 0;
	x2 = width;
	y2 = height;
	if (ecb->priv->x1 >= 0 && upper_left.x > x1)
		x1 = upper_left.x;
	if (ecb->priv->y1 >= 0 && upper_left.y > y1)
		y1 = upper_left.y;
	if (ecb->priv->x2 >= 0 && lower_right.x < x2)
		x2 = lower_right.x;
	if (ecb->priv->y2 >= 0 && lower_right.y < y2)
		y2 = lower_right.y;

	gdk_draw_rectangle (drawable, ecb->priv->gc, TRUE,
			    x1, y1, x2 - x1, y2 - y1);
}

static double
ecb_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
	   GnomeCanvasItem **actual_item)
{
	ECanvasBackground *ecb = E_CANVAS_BACKGROUND (item);

	if (ecb->priv->x1 >= 0 && ecb->priv->x1 > x)
		return 1.0;
	if (ecb->priv->x2 >= 0 && ecb->priv->x2 < x)
		return 1.0;
	if (ecb->priv->y1 >= 0 && ecb->priv->y1 > y)
		return 1.0;
	if (ecb->priv->y2 >= 0 && ecb->priv->y2 < y)
		return 1.0;
	*actual_item = item;

	return 0.0;
}

static void
ecb_class_init (GtkObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	
	parent_class                = gtk_type_class (PARENT_OBJECT_TYPE);
	
	object_class->destroy       = ecb_destroy;
	object_class->set_arg       = ecb_set_arg;
	object_class->get_arg       = ecb_get_arg;

	item_class->update          = ecb_update;
	item_class->realize         = ecb_realize;
	item_class->unrealize       = ecb_unrealize;
	item_class->draw            = ecb_draw;
	item_class->point           = ecb_point;

	gtk_object_add_arg_type ("ECanvasBackground::fill_color", GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE, ARG_FILL_COLOR);
	gtk_object_add_arg_type ("ECanvasBackground::fill_color_gdk", GTK_TYPE_GDK_COLOR,
				 GTK_ARG_READWRITE, ARG_FILL_COLOR_GDK);
	gtk_object_add_arg_type ("ECanvasBackground::fill_color_rgba", GTK_TYPE_UINT,
				 GTK_ARG_READWRITE, ARG_FILL_COLOR_RGBA);
	gtk_object_add_arg_type ("ECanvasBackground::fill_stipple", GTK_TYPE_GDK_WINDOW,
				 GTK_ARG_READWRITE, ARG_FILL_STIPPLE);
	gtk_object_add_arg_type ("ECanvasBackground::x1", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_X1);
	gtk_object_add_arg_type ("ECanvasBackground::x2", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_X2);
	gtk_object_add_arg_type ("ECanvasBackground::y1", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_Y1);
	gtk_object_add_arg_type ("ECanvasBackground::y2", GTK_TYPE_DOUBLE,
				 GTK_ARG_READWRITE, ARG_Y2);
}

GtkType
e_canvas_background_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ECanvasBackground",
			sizeof (ECanvasBackground),
			sizeof (ECanvasBackgroundClass),
			(GtkClassInitFunc) ecb_class_init,
			(GtkObjectInitFunc) ecb_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_OBJECT_TYPE, &info);
	}

	return type;
}

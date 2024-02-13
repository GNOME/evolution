/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 */
/*
  @NOTATION@
 */
/* Widget item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include "evolution-config.h"

#include <math.h>
#include "gnome-canvas-widget.h"

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_SIZE_PIXELS
};

static void gnome_canvas_widget_dispose    (GnomeCanvasItem      *object);
static void gnome_canvas_widget_get_property (GObject            *object,
					      guint               param_id,
					      GValue             *value,
					      GParamSpec         *pspec);
static void gnome_canvas_widget_set_property (GObject            *object,
					      guint               param_id,
					      const GValue       *value,
					      GParamSpec         *pspec);

static void	gnome_canvas_widget_update	(GnomeCanvasItem *item,
						 const cairo_matrix_t *matrix,
						 gint flags);
static GnomeCanvasItem *gnome_canvas_widget_point (GnomeCanvasItem *item,
						 gdouble x,
						 gdouble y,
						 gint cx,
						 gint cy);
static void	gnome_canvas_widget_bounds	(GnomeCanvasItem *item,
						 gdouble *x1,
						 gdouble *y1,
						 gdouble *x2,
						 gdouble *y2);

static void	gnome_canvas_widget_draw	(GnomeCanvasItem *item,
						 cairo_t *cr,
						 gint x,
						 gint y,
						 gint width,
						 gint height);

G_DEFINE_TYPE (
	GnomeCanvasWidget,
	gnome_canvas_widget,
	GNOME_TYPE_CANVAS_ITEM)

static void
gnome_canvas_widget_class_init (GnomeCanvasWidgetClass *class)
{
	GObjectClass *gobject_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gobject_class->set_property = gnome_canvas_widget_set_property;
	gobject_class->get_property = gnome_canvas_widget_get_property;

	g_object_class_install_property
		(gobject_class,
		 PROP_WIDGET,
		 g_param_spec_object ("widget", NULL, NULL,
				      GTK_TYPE_WIDGET,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class,
		 PROP_X,
		 g_param_spec_double ("x", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class,
		 PROP_Y,
		 g_param_spec_double ("y", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class,
		 PROP_WIDTH,
		 g_param_spec_double ("width", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class,
		 PROP_HEIGHT,
		 g_param_spec_double ("height", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property
		(gobject_class,
		 PROP_SIZE_PIXELS,
		 g_param_spec_boolean ("size_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_class->dispose = gnome_canvas_widget_dispose;
	item_class->update = gnome_canvas_widget_update;
	item_class->point = gnome_canvas_widget_point;
	item_class->bounds = gnome_canvas_widget_bounds;
	item_class->draw = gnome_canvas_widget_draw;
}

static void
do_destroy (gpointer data,
            GObject *gone_object)
{
	GnomeCanvasWidget *witem;

	witem = data;

	if (!witem->in_destroy) {
		witem->in_destroy = TRUE;
		g_object_run_dispose (G_OBJECT (witem));
	}
}

static void
gnome_canvas_widget_init (GnomeCanvasWidget *witem)
{
	witem->x = 0.0;
	witem->y = 0.0;
	witem->width = 0.0;
	witem->height = 0.0;
	witem->size_pixels = FALSE;
}

static void
gnome_canvas_widget_dispose (GnomeCanvasItem *object)
{
	GnomeCanvasWidget *witem;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	witem = GNOME_CANVAS_WIDGET (object);

	if (witem->widget && !witem->in_destroy) {
		g_object_weak_unref (G_OBJECT (witem->widget), do_destroy, witem);
		gtk_widget_destroy (witem->widget);
		witem->widget = NULL;
	}

	GNOME_CANVAS_ITEM_CLASS (gnome_canvas_widget_parent_class)->
		dispose (object);
}

static gboolean
reposition_widget_cb (gpointer user_data)
{
	GnomeCanvasWidget *witem = user_data;

	g_return_val_if_fail (GNOME_IS_CANVAS_WIDGET (witem), FALSE);

	if (witem->widget)
		gtk_widget_queue_resize (witem->widget);

	return FALSE;
}

static void
recalc_bounds (GnomeCanvasWidget *witem)
{
	GnomeCanvasItem *item;
	gdouble wx, wy;

	item = GNOME_CANVAS_ITEM (witem);

	/* Get world coordinates */

	wx = witem->x;
	wy = witem->y;
	gnome_canvas_item_i2w (item, &wx, &wy);

	/* Get canvas pixel coordinates */

	gnome_canvas_w2c (item->canvas, wx, wy, &witem->cx, &witem->cy);

	/* Bounds */

	item->x1 = witem->cx;
	item->y1 = witem->cy;
	item->x2 = witem->cx + witem->cwidth;
	item->y2 = witem->cy + witem->cheight;

	if (witem->widget) {
		gint current_x = 0, current_y = 0;

		gtk_container_child_get (GTK_CONTAINER (item->canvas), witem->widget,
			"x", &current_x,
			"y", &current_y,
			NULL);

		if (current_x != ((gint) (witem->cx + item->canvas->zoom_xofs)) ||
		    current_y != ((gint) (witem->cy + item->canvas->zoom_yofs))) {
			gtk_layout_move (
				GTK_LAYOUT (item->canvas), witem->widget,
				witem->cx + item->canvas->zoom_xofs,
				witem->cy + item->canvas->zoom_yofs);

			/* This is needed, because the gtk_layout_move() calls gtk_widget_queue_resize(),
			   which can be silently ignored when called inside "size-allocate" handler, causing
			   misposition of the child widget. */
			g_idle_add_full (G_PRIORITY_HIGH_IDLE,
				reposition_widget_cb, g_object_ref (witem), g_object_unref);
		}
	}
}

static void
gnome_canvas_widget_set_property (GObject *object,
                                  guint param_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasWidget *witem;
	GObject *obj;
	gint update;
	gint calc_bounds;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	item = GNOME_CANVAS_ITEM (object);
	witem = GNOME_CANVAS_WIDGET (object);

	update = FALSE;
	calc_bounds = FALSE;

	switch (param_id) {
	case PROP_WIDGET:
		if (witem->widget) {
			g_object_weak_unref (G_OBJECT (witem->widget), do_destroy, witem);
			gtk_container_remove (GTK_CONTAINER (item->canvas), witem->widget);
		}

		obj = g_value_get_object (value);
		if (obj) {
			witem->widget = GTK_WIDGET (obj);
			g_object_weak_ref (obj, do_destroy, witem);
			gtk_layout_put (
				GTK_LAYOUT (item->canvas), witem->widget,
				witem->cx + item->canvas->zoom_xofs,
				witem->cy + item->canvas->zoom_yofs);
		}

		update = TRUE;
		break;

	case PROP_X:
		if (witem->x != g_value_get_double (value))
		{
			witem->x = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y:
		if (witem->y != g_value_get_double (value))
		{
			witem->y = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_WIDTH:
		if (witem->width != fabs (g_value_get_double (value)))
		{
			witem->width = fabs (g_value_get_double (value));
			update = TRUE;
		}
		break;

	case PROP_HEIGHT:
		if (witem->height != fabs (g_value_get_double (value)))
		{
			witem->height = fabs (g_value_get_double (value));
			update = TRUE;
		}
		break;

	case PROP_SIZE_PIXELS:
		if (witem->size_pixels != g_value_get_boolean (value))
		{
			witem->size_pixels = g_value_get_boolean (value);
			update = TRUE;
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

	if (update)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->update) (item, NULL, 0);

	if (calc_bounds)
		recalc_bounds (witem);
}

static void
gnome_canvas_widget_get_property (GObject *object,
                                  guint param_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	GnomeCanvasWidget *witem;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	witem = GNOME_CANVAS_WIDGET (object);

	switch (param_id) {
	case PROP_WIDGET:
		g_value_set_object (value, (GObject *) witem->widget);
		break;

	case PROP_X:
		g_value_set_double (value, witem->x);
		break;

	case PROP_Y:
		g_value_set_double (value, witem->y);
		break;

	case PROP_WIDTH:
		g_value_set_double (value, witem->width);
		break;

	case PROP_HEIGHT:
		g_value_set_double (value, witem->height);
		break;

	case PROP_SIZE_PIXELS:
		g_value_set_boolean (value, witem->size_pixels);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_widget_update (GnomeCanvasItem *item,
                            const cairo_matrix_t *matrix,
                            gint flags)
{
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	GNOME_CANVAS_ITEM_CLASS (gnome_canvas_widget_parent_class)->
		update (item, matrix, flags);

	if (witem->widget) {
		witem->cwidth = (gint) (witem->width + 0.5);
		witem->cheight = (gint) (witem->height + 0.5);

		gtk_widget_set_size_request (witem->widget, witem->cwidth, witem->cheight);
	} else {
		witem->cwidth = 0.0;
		witem->cheight = 0.0;
	}

	recalc_bounds (witem);
}

static void
gnome_canvas_widget_draw (GnomeCanvasItem *item,
                          cairo_t *cr,
                          gint x,
                          gint y,
                          gint width,
                          gint height)
{
#if 0
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	if (witem->widget)
		gtk_widget_queue_draw (witem->widget);
#endif
}

static GnomeCanvasItem *
gnome_canvas_widget_point (GnomeCanvasItem *item,
                           gdouble x,
                           gdouble y,
                           gint cx,
                           gint cy)
{
	GnomeCanvasWidget *witem;
	gdouble x1, y1, x2, y2;

	witem = GNOME_CANVAS_WIDGET (item);

	gnome_canvas_c2w (item->canvas, witem->cx, witem->cy, &x1, &y1);

	x2 = x1 + (witem->cwidth - 1);
	y2 = y1 + (witem->cheight - 1);

	/* Is point inside widget bounds? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2))
		return item;

	/* Point is outside widget bounds */
	return NULL;
}

static void
gnome_canvas_widget_bounds (GnomeCanvasItem *item,
                            gdouble *x1,
                            gdouble *y1,
                            gdouble *x2,
                            gdouble *y2)
{
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	*x1 = witem->x;
	*y1 = witem->y;

	*x2 = *x1 + witem->width;
	*y2 = *y1 + witem->height;
}

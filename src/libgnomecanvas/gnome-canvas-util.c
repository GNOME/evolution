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
/* Miscellaneous utility functions for the GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include "evolution-config.h"

/* needed for M_PI_2 under 'gcc -ansi -predantic' on GNU/Linux */
#ifndef _DEFAULT_SOURCE
#  define _DEFAULT_SOURCE 1
#endif
#include <sys/types.h>

#include <math.h>
#include "gnome-canvas.h"
#include "gnome-canvas-util.h"

/* Here are some helper functions for aa rendering: */

/**
 * gnome_canvas_item_reset_bounds:
 * @item: A canvas item
 *
 * Resets the bounding box of a canvas item to an empty rectangle.
 **/
void
gnome_canvas_item_reset_bounds (GnomeCanvasItem *item)
{
	item->x1 = 0.0;
	item->y1 = 0.0;
	item->x2 = 0.0;
	item->y2 = 0.0;
}

/**
 * gnome_canvas_update_bbox:
 * @item: the canvas item needing update
 * @x1: Left coordinate of the new bounding box
 * @y1: Top coordinate of the new bounding box
 * @x2: Right coordinate of the new bounding box
 * @y2: Bottom coordinate of the new bounding box
 *
 * Sets the bbox to the new value, requesting full repaint.
 **/
void
gnome_canvas_update_bbox (GnomeCanvasItem *item,
                          gint x1,
                          gint y1,
                          gint x2,
                          gint y2)
{
	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2, item->y2);

	item->x1 = x1;
	item->y1 = y1;
	item->x2 = x2;
	item->y2 = y2;

	gnome_canvas_request_redraw (
		item->canvas, item->x1, item->y1, item->x2, item->y2);
}

/**
 * gnome_canvas_cairo_create_scratch:
 *
 * Create a scratch #cairo_t. This is useful for measuring purposes or
 * calling functions like cairo_in_fill().
 *
 * Returns: A new cairo_t. Destroy with cairo_destroy() after use.
 **/
cairo_t *
gnome_canvas_cairo_create_scratch (void)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create (surface);
	cairo_surface_destroy (surface);

	return cr;
}

/**
 * gnome_canvas_matrix_transform_rect:
 * @matrix: a cairo matrix
 * @x1: x coordinate of top left position of rectangle (in-out)
 * @y1: y coordinate of top left position of rectangle (in-out)
 * @x2: x coordinate of bottom right position of rectangle (in-out)
 * @y2: y coordinate of bottom right position of rectangle (in-out)
 *
 * Computes the smallest rectangle containing the whole area of the given
 * rectangle after applying the transformation given in @matrix.
 **/
void
gnome_canvas_matrix_transform_rect (const cairo_matrix_t *matrix,
                                    gdouble *x1,
                                    gdouble *y1,
                                    gdouble *x2,
                                    gdouble *y2)
{
	gdouble maxx, maxy, minx, miny;
	gdouble tmpx, tmpy;

	tmpx = *x1;
	tmpy = *y1;
	cairo_matrix_transform_point (matrix, &tmpx, &tmpy);
	minx = maxx = tmpx;
	miny = maxy = tmpy;

	tmpx = *x2;
	tmpy = *y1;
	cairo_matrix_transform_point (matrix, &tmpx, &tmpy);
	minx = MIN (minx, tmpx);
	maxx = MAX (maxx, tmpx);
	miny = MIN (miny, tmpy);
	maxy = MAX (maxy, tmpy);

	tmpx = *x2;
	tmpy = *y2;
	cairo_matrix_transform_point (matrix, &tmpx, &tmpy);
	minx = MIN (minx, tmpx);
	maxx = MAX (maxx, tmpx);
	miny = MIN (miny, tmpy);
	maxy = MAX (maxy, tmpy);

	tmpx = *x1;
	tmpy = *y2;
	cairo_matrix_transform_point (matrix, &tmpx, &tmpy);
	minx = MIN (minx, tmpx);
	maxx = MAX (maxx, tmpx);
	miny = MIN (miny, tmpy);
	maxy = MAX (maxy, tmpy);

        *x1 = minx;
        *x2 = maxx;
        *y1 = miny;
        *y2 = maxy;
}


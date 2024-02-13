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
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GNOME_CANVAS_UTIL_H
#define GNOME_CANVAS_UTIL_H

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/* Sets the svp to the new value, requesting repaint on what's changed. This
 * function takes responsibility for freeing new_svp. This routine also adds the
 * svp's bbox to the item's.
 */
void gnome_canvas_item_reset_bounds (GnomeCanvasItem *item);

/* Sets the bbox to the new value, requesting full repaint. */
void gnome_canvas_update_bbox (GnomeCanvasItem *item, gint x1, gint y1, gint x2, gint y2);

/* Create a scratch cairo_t for measuring purposes */
cairo_t *gnome_canvas_cairo_create_scratch (void);

void gnome_canvas_matrix_transform_rect (const cairo_matrix_t *matrix, double *x1, double *y1, double *x2, double *y2);

G_END_DECLS

#endif

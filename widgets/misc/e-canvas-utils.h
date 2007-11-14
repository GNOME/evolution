/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright 2000, 2001, Ximian, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __E_CANVAS_UTILS__
#define __E_CANVAS_UTILS__

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

void      e_canvas_item_move_absolute      (GnomeCanvasItem *item,
					    double           dx,
					    double           dy);
void      e_canvas_item_show_area          (GnomeCanvasItem *item,
					    double           x1,
					    double           y1,
					    double           x2,
					    double           y2);
void      e_canvas_item_show_area_delayed  (GnomeCanvasItem *item,
					    double           x1,
					    double           y1,
					    double           x2,
					    double           y2,
					    gint             delay);
/* Returns TRUE if the area is already shown on the screen (including
   spacing.)  This is equivelent to returning FALSE iff show_area
   would do anything. */
gboolean  e_canvas_item_area_shown         (GnomeCanvasItem *item,
					    double           x1,
					    double           y1,
					    double           x2,
					    double           y2);

G_END_DECLS

#endif /* __E_CANVAS_UTILS__ */

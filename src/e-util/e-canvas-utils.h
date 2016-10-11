/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_CANVAS_UTILS__
#define __E_CANVAS_UTILS__

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

void      e_canvas_item_move_absolute      (GnomeCanvasItem *item,
					    gdouble           dx,
					    gdouble           dy);
void      e_canvas_item_show_area          (GnomeCanvasItem *item,
					    gdouble           x1,
					    gdouble           y1,
					    gdouble           x2,
					    gdouble           y2);
void      e_canvas_item_show_area_delayed  (GnomeCanvasItem *item,
					    gdouble           x1,
					    gdouble           y1,
					    gdouble           x2,
					    gdouble           y2,
					    gint             delay);
GSource * e_canvas_item_show_area_delayed_ex
					   (GnomeCanvasItem *item,
					    gdouble           x1,
					    gdouble           y1,
					    gdouble           x2,
					    gdouble           y2,
					    gint             delay);
/* Returns TRUE if the area is already shown on the screen (including
 * spacing.)  This is equivelent to returning FALSE iff show_area
 * would do anything. */
gboolean  e_canvas_item_area_shown         (GnomeCanvasItem *item,
					    gdouble           x1,
					    gdouble           y1,
					    gdouble           x2,
					    gdouble           y2);

G_END_DECLS

#endif /* __E_CANVAS_UTILS__ */

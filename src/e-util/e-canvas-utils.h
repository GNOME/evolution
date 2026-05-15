/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
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

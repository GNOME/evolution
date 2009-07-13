/*
 * e-canvas-background.h - background color for canvas.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CANVAS_BACKGROUND_H
#define E_CANVAS_BACKGROUND_H

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/*
 * name			type		read/write	description
 * ------------------------------------------------------------------------------------------
 * fill_color		string		W		X color specification for fill color,
 *							or NULL pointer for no color (transparent)
 * fill_color_gdk	GdkColor*	RW		Allocated GdkColor for fill
 * fill_stipple		GdkBitmap*	RW		Stipple pattern for fill
 * x1                   double		RW              Coordinates for edges of background rectangle
 * x2                   double		RW              Default is all of them = -1.
 * y1                   double		RW              Which means that the entire space is shown.
 * y2                   double		RW              If you need the rectangle to have negative coordinates, use an affine.
 */

#define E_CANVAS_BACKGROUND_TYPE            (e_canvas_background_get_type ())
#define E_CANVAS_BACKGROUND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_CANVAS_BACKGROUND_TYPE, ECanvasBackground))
#define E_CANVAS_BACKGROUND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_CANVAS_BACKGROUND_TYPE, ECanvasBackgroundClass))
#define E_IS_CANVAS_BACKGROUND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_CANVAS_BACKGROUND_TYPE))
#define E_IS_CANVAS_BACKGROUND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_CANVAS_BACKGROUND_TYPE))

typedef struct _ECanvasBackground      ECanvasBackground;
typedef struct _ECanvasBackgroundClass ECanvasBackgroundClass;
typedef struct _ECanvasBackgroundPrivate ECanvasBackgroundPrivate;

struct _ECanvasBackground {
	GnomeCanvasItem item;

	ECanvasBackgroundPrivate *priv;
};

struct _ECanvasBackgroundClass {
	GnomeCanvasItemClass parent_class;
	void        (*style_set)        (ECanvasBackground *eti, GtkStyle *previous_style);
};

/* Standard Gtk function */
GType e_canvas_background_get_type (void);

G_END_DECLS

#endif

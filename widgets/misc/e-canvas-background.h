/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-canvas-background.h - background color for canvas.
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
GtkType e_canvas_background_get_type (void);

G_END_DECLS

#endif

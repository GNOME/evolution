/* Generic bezier shape item for GnomeCanvas
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ximian.com>
 *          Rusty Conover <rconover@bangtail.net>
 */

#ifndef GNOME_CANVAS_RECT_H
#define GNOME_CANVAS_RECT_H

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/* Rect item for the canvas.
 *
 * The following object arguments are available:
 *
 * name			type			read/write	description
 * ------------------------------------------------------------------------------------------
 * fill-color		GdkRGBA*		RW		Allocated GdkRGBA for fill color,
 *								or NULL pointer for no color (transparent).
 * outline-color	GdkRGBA*		RW		Allocated GdkRGBA for outline color,
 *								or NULL pointer for no color (transparent).
 * width_pixels		uint			RW		Width of the outline in pixels.  The outline will
 *								not be scaled when the canvas zoom factor is changed.
 * width_units		gdouble			RW		Width of the outline in canvas units.  The outline
 *								will be scaled when the canvas zoom factor is changed.
 * cap_style		cairo_line_cap_t        RW		Cap ("endpoint") style for the bpath.
 * join_style		cairo_line_join_t	RW		Join ("vertex") style for the bpath.
 * wind                 cairo_fill_rule_t       RW              Winding rule for the bpath.
 * dash			XXX: disabled           RW		Dashing pattern
 * miterlimit		gdouble			RW		Minimum angle between segments, where miter join
 *								rule is applied.
 */

#define GNOME_TYPE_CANVAS_RECT            (gnome_canvas_rect_get_type ())
#define GNOME_CANVAS_RECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRect))
#define GNOME_CANVAS_RECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRectClass))
#define GNOME_IS_CANVAS_RECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_RECT))
#define GNOME_IS_CANVAS_RECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_RECT))

typedef struct _GnomeCanvasRect GnomeCanvasRect;
typedef struct _GnomeCanvasRectClass GnomeCanvasRectClass;
typedef struct _GnomeCanvasRectPrivate GnomeCanvasRectPrivate;

struct _GnomeCanvasRect {
	GnomeCanvasItem item;
	GnomeCanvasRectPrivate *priv;
};

struct _GnomeCanvasRectClass {
	GnomeCanvasItemClass parent_class;
};

GType gnome_canvas_rect_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GNOME_CANVAS_RECT_H */

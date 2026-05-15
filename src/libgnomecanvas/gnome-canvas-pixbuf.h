/*
 * SPDX-FileCopyrightText: (C) 1999 The Free Software Foundation
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Federico Mena-Quintero <federico@gimp.org>
 */

#ifndef GNOME_CANVAS_PIXBUF_H
#define GNOME_CANVAS_PIXBUF_H

#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

#define GNOME_TYPE_CANVAS_PIXBUF            (gnome_canvas_pixbuf_get_type ())
#define GNOME_CANVAS_PIXBUF(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbuf))
#define GNOME_CANVAS_PIXBUF_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbufClass))
#define GNOME_IS_CANVAS_PIXBUF(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_PIXBUF))
#define GNOME_IS_CANVAS_PIXBUF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_PIXBUF))
#define GNOME_CANVAS_PIXBUF_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbufClass))

typedef struct _GnomeCanvasPixbuf GnomeCanvasPixbuf;
typedef struct _GnomeCanvasPixbufClass GnomeCanvasPixbufClass;
typedef struct _GnomeCanvasPixbufPrivate GnomeCanvasPixbufPrivate;

struct _GnomeCanvasPixbuf {
	GnomeCanvasItem item;

	/* Private data */
	GnomeCanvasPixbufPrivate *priv;
};

struct _GnomeCanvasPixbufClass {
	GnomeCanvasItemClass parent_class;
};

GType gnome_canvas_pixbuf_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif

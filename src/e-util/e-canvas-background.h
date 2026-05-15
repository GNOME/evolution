/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CANVAS_BACKGROUND_H
#define E_CANVAS_BACKGROUND_H

#include <libgnomecanvas/gnome-canvas.h>

/* Standard GObject macros */
#define E_TYPE_CANVAS_BACKGROUND \
	(e_canvas_background_get_type ())
#define E_CANVAS_BACKGROUND(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CANVAS_BACKGROUND, ECanvasBackground))
#define E_CANVAS_BACKGROUND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CANVAS_BACKGROUND, ECanvasBackgroundClass))
#define E_IS_CANVAS_BACKGROUND(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CANVAS_BACKGROUND))
#define E_IS_CANVAS_BACKGROUND_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CANVAS_BACKGROUND))
#define E_CANVAS_BACKGROUND_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CANVAS_BACKGROUND, ECanvasBackgroundClass))

G_BEGIN_DECLS

typedef struct _ECanvasBackground ECanvasBackground;
typedef struct _ECanvasBackgroundClass ECanvasBackgroundClass;
typedef struct _ECanvasBackgroundPrivate ECanvasBackgroundPrivate;

struct _ECanvasBackground {
	GnomeCanvasItem item;
	ECanvasBackgroundPrivate *priv;
};

struct _ECanvasBackgroundClass {
	GnomeCanvasItemClass parent_class;

	void		(*style_updated)	(ECanvasBackground *eti);
};

GType		e_canvas_background_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_CANVAS_BACKGROUND */

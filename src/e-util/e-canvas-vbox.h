/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CANVAS_VBOX_H
#define E_CANVAS_VBOX_H

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

/* Standard GObject macros */
#define E_TYPE_CANVAS_VBOX \
	(e_canvas_vbox_get_type ())
#define E_CANVAS_VBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CANVAS_VBOX, ECanvasVbox))
#define E_CANVAS_VBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CANVAS_VBOX, ECanvasVboxClass))
#define E_IS_CANVAS_VBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CANVAS_VBOX))
#define E_IS_CANVAS_VBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CANVAS_VBOX))
#define E_CANVAS_VBOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CANVAS_VBOX, ECanvasVboxClass))

G_BEGIN_DECLS

typedef struct _ECanvasVbox ECanvasVbox;
typedef struct _ECanvasVboxClass ECanvasVboxClass;

struct _ECanvasVbox {
	GnomeCanvasGroup parent;

	/* item specific fields */
	GList *items; /* Of type GnomeCanvasItem */

	gdouble width;
	gdouble minimum_width;
	gdouble height;
	gdouble spacing;
};

struct _ECanvasVboxClass {
	GnomeCanvasGroupClass parent_class;

	void		(*add_item)		(ECanvasVbox *canvas_vbox,
						 GnomeCanvasItem *item);
	void		(*add_item_start)	(ECanvasVbox *canvas_vbox,
						 GnomeCanvasItem *item);
};

/*
 * To be added to a CanvasVbox, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent CanvasVbox request if its size
 * changes.
 */
GType		e_canvas_vbox_get_type		(void) G_GNUC_CONST;
void		e_canvas_vbox_add_item		(ECanvasVbox *canvas_vbox,
						 GnomeCanvasItem *item);
void		e_canvas_vbox_add_item_start	(ECanvasVbox *canvas_vbox,
						 GnomeCanvasItem *item);

G_END_DECLS

#endif /* E_CANVAS_VBOX_H */

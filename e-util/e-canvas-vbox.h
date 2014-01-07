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

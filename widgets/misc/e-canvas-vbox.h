/*
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

#ifndef __E_CANVAS_VBOX_H__
#define __E_CANVAS_VBOX_H__

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS

/* ECanvasVbox - A canvas item container.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * width        double          RW              width of the CanvasVbox
 * height       double          R               height of the CanvasVbox
 * spacing      double          RW              Spacing between items.
 */

#define E_CANVAS_VBOX_TYPE			(e_canvas_vbox_get_type ())
#define E_CANVAS_VBOX(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_CANVAS_VBOX_TYPE, ECanvasVbox))
#define E_CANVAS_VBOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_CANVAS_VBOX_TYPE, ECanvasVboxClass))
#define E_IS_CANVAS_VBOX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_CANVAS_VBOX_TYPE))
#define E_IS_CANVAS_VBOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_CANVAS_VBOX_TYPE))

typedef struct _ECanvasVbox       ECanvasVbox;
typedef struct _ECanvasVboxClass  ECanvasVboxClass;

struct _ECanvasVbox
{
	GnomeCanvasGroup parent;

	/* item specific fields */
	GList *items; /* Of type GnomeCanvasItem */

	double width;
	double minimum_width;
	double height;
	double spacing;
};

struct _ECanvasVboxClass
{
	GnomeCanvasGroupClass parent_class;

	/* Virtual methods. */
	void (* add_item) (ECanvasVbox *CanvasVbox, GnomeCanvasItem *item);
	void (* add_item_start) (ECanvasVbox *CanvasVbox, GnomeCanvasItem *item);
};

/*
 * To be added to a CanvasVbox, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent CanvasVbox request if its size
 * changes.
 */
void       e_canvas_vbox_add_item(ECanvasVbox *e_canvas_vbox, GnomeCanvasItem *item);
void       e_canvas_vbox_add_item_start(ECanvasVbox *e_canvas_vbox, GnomeCanvasItem *item);
GType      e_canvas_vbox_get_type (void);

G_END_DECLS

#endif /* __E_CANVAS_VBOX_H__ */

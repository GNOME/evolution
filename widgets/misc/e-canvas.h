/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-canvas.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_CANVAS_H__
#define __E_CANVAS_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ECanvas - A class derived from canvas for the purpose of adding
 * evolution specific canvas hacks.
 */

#define E_CANVAS_TYPE			(e_canvas_get_type ())
#define E_CANVAS(obj)			(GTK_CHECK_CAST ((obj), E_CANVAS_TYPE, ECanvas))
#define E_CANVAS_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_CANVAS_TYPE, ECanvasClass))
#define E_IS_CANVAS(obj)		(GTK_CHECK_TYPE ((obj), E_CANVAS_TYPE))
#define E_IS_CANVAS_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_CANVAS_TYPE))

typedef void		(*ECanvasItemReflowFunc)		(GnomeCanvasItem *item,
								 gint   	  flags);

typedef struct _ECanvas       ECanvas;
typedef struct _ECanvasClass  ECanvasClass;

struct _ECanvas
{
	GnomeCanvas parent;
	
	int         idle_id;	
};

struct _ECanvasClass
{
	GnomeCanvasClass parent_class;
	void (* reflow) (ECanvas *canvas);
};


GtkType    e_canvas_get_type (void);
GtkWidget *e_canvas_new      (void);

/* Used to send all of the keystroke events to a specific item as well as
 * GDK_FOCUS_CHANGE events.
 */
void e_canvas_item_grab_focus (GnomeCanvasItem *item);

void e_canvas_item_request_reflow (GnomeCanvasItem *item);
void e_canvas_item_request_parent_reflow (GnomeCanvasItem *item);
void e_canvas_item_set_reflow_callback (GnomeCanvasItem *item, ECanvasItemReflowFunc func);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CANVAS_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-reflow.h
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
#ifndef __E_REFLOW_H__
#define __E_REFLOW_H__

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EReflow - A canvas item container.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * minimum_width double         RW              minimum width of the reflow.  width >= minimum_width
 * width        double          R               width of the reflow
 * height       double          RW              height of the reflow
 */

#define E_REFLOW_TYPE			(e_reflow_get_type ())
#define E_REFLOW(obj)			(GTK_CHECK_CAST ((obj), E_REFLOW_TYPE, EReflow))
#define E_REFLOW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_REFLOW_TYPE, EReflowClass))
#define E_IS_REFLOW(obj)		(GTK_CHECK_TYPE ((obj), E_REFLOW_TYPE))
#define E_IS_REFLOW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_REFLOW_TYPE))


typedef struct _EReflow       EReflow;
typedef struct _EReflowClass  EReflowClass;

struct _EReflow
{
	GnomeCanvasGroup parent;
	
	/* item specific fields */
	GList *items; /* Of type GnomeCanvasItem */
	GList *columns; /* Of type GList of type GnomeCanvasItem (points into items) */
	gint column_count; /* Number of columnns */

	GnomeCanvasItem *empty_text;
	gchar *empty_message;
	
	double minimum_width;
	double width;
	double height;
       
	double column_width;

	int idle;

	/* These are all for when the column is being dragged. */
	gboolean column_drag;
	gdouble start_x;
	gint which_column_dragged;
	double temp_column_width;
	double previous_temp_column_width;

	guint need_height_update : 1;
	guint need_column_resize : 1;

	guint default_cursor_shown : 1;
	GdkCursor *arrow_cursor;
	GdkCursor *default_cursor;
};

struct _EReflowClass
{
	GnomeCanvasGroupClass parent_class;

	/* Virtual methods. */
	void (* add_item) (EReflow *reflow, GnomeCanvasItem *item);
};

/* 
 * To be added to a reflow, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent reflow request if its size
 * changes.
 */
void       e_reflow_add_item(EReflow *e_reflow, GnomeCanvasItem *item);
GtkType    e_reflow_get_type (void);

/* Internal usage only: */
void e_reflow_post_add_item(EReflow *e_reflow, GnomeCanvasItem *item);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_REFLOW_H__ */

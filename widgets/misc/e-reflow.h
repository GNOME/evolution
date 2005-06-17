/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-reflow.h
 * Copyright 2000, 2001, Ximian, Inc.
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

#ifndef __E_REFLOW_H__
#define __E_REFLOW_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <widgets/misc/e-reflow-model.h>
#include <widgets/misc/e-selection-model.h>
#include <e-util/e-sorter-array.h>

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
#define E_REFLOW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_REFLOW_TYPE, EReflow))
#define E_REFLOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_REFLOW_TYPE, EReflowClass))
#define E_IS_REFLOW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_REFLOW_TYPE))
#define E_IS_REFLOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_REFLOW_TYPE))


typedef struct EReflowPriv    EReflowPriv;

typedef struct _EReflow       EReflow;
typedef struct _EReflowClass  EReflowClass;

struct _EReflow
{
	GnomeCanvasGroup parent;
	
	/* item specific fields */
	EReflowModel *model;
	guint model_changed_id;
	guint comparison_changed_id;
	guint model_items_inserted_id;
	guint model_item_removed_id;
	guint model_item_changed_id;

	ESelectionModel *selection;
	guint selection_changed_id;
	guint selection_row_changed_id;
	guint cursor_changed_id;
	ESorterArray *sorter;

	GtkAdjustment *adjustment;
	guint adjustment_changed_id;
	guint adjustment_value_changed_id;
	guint set_scroll_adjustments_id;

	int *heights;
	GnomeCanvasItem **items;
	int count;
	int allocated_count;

	int *columns;
	gint column_count; /* Number of columnns */

	GnomeCanvasItem *empty_text;
	gchar *empty_message;

	double minimum_width;
	double width;
	double height;

	double column_width;

	int incarnate_idle_id;
	int do_adjustment_idle_id;

	/* These are all for when the column is being dragged. */
	gdouble start_x;
	gint which_column_dragged;
	double temp_column_width;
	double previous_temp_column_width;

	int cursor_row;

	int reflow_from_column;

	guint column_drag : 1;

	guint need_height_update : 1;
	guint need_column_resize : 1;
	guint need_reflow_columns : 1;

	guint default_cursor_shown : 1;

	guint maybe_did_something : 1;
	guint maybe_in_drag : 1;
	GdkCursor *arrow_cursor;
	GdkCursor *default_cursor;
};

struct _EReflowClass
{
	GnomeCanvasGroupClass parent_class;

	int (*selection_event) (EReflow *reflow, GnomeCanvasItem *item, GdkEvent *event);
	void (*column_width_changed) (EReflow *reflow, double width);
};

/* 
 * To be added to a reflow, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent reflow request if its size
 * changes.
 */
GtkType  e_reflow_get_type       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_REFLOW_H__ */

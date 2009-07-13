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

#ifndef __E_REFLOW_H__
#define __E_REFLOW_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <misc/e-reflow-model.h>
#include <misc/e-selection-model.h>
#include <e-util/e-sorter-array.h>

G_BEGIN_DECLS

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

	gint *heights;
	GnomeCanvasItem **items;
	gint count;
	gint allocated_count;

	gint *columns;
	gint column_count; /* Number of columnns */

	GnomeCanvasItem *empty_text;
	gchar *empty_message;

	double minimum_width;
	double width;
	double height;

	double column_width;

	gint incarnate_idle_id;
	gint do_adjustment_idle_id;

	/* These are all for when the column is being dragged. */
	gdouble start_x;
	gint which_column_dragged;
	double temp_column_width;
	double previous_temp_column_width;

	gint cursor_row;

	gint reflow_from_column;

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

	gint (*selection_event) (EReflow *reflow, GnomeCanvasItem *item, GdkEvent *event);
	void (*column_width_changed) (EReflow *reflow, double width);
};

/*
 * To be added to a reflow, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent reflow request if its size
 * changes.
 */
GType    e_reflow_get_type       (void);

G_END_DECLS

#endif /* __E_REFLOW_H__ */

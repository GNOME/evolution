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

#ifndef E_REFLOW_H
#define E_REFLOW_H

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-reflow-model.h>
#include <e-util/e-selection-model.h>
#include <e-util/e-sorter-array.h>

/* Standard GObject macros */
#define E_TYPE_REFLOW \
	(e_reflow_get_type ())
#define E_REFLOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_REFLOW, EReflow))
#define E_REFLOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_REFLOW, EReflowClass))
#define E_IS_REFLOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_REFLOW))
#define E_IS_REFLOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_REFLOW))
#define E_REFLOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_REFLOW, EReflowClass))

G_BEGIN_DECLS

typedef struct _EReflow EReflow;
typedef struct _EReflowClass EReflowClass;
typedef struct _EReflowPrivate EReflowPrivate;

struct _EReflow {
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

	gdouble minimum_width;
	gdouble width;
	gdouble height;

	gdouble column_width;

	gint incarnate_idle_id;
	gint do_adjustment_idle_id;

	/* These are all for when the column is being dragged. */
	gdouble start_x;
	gint which_column_dragged;
	gdouble temp_column_width;
	gdouble previous_temp_column_width;

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
	void (*column_width_changed) (EReflow *reflow, gdouble width);
};

/*
 * To be added to a reflow, an item must have the argument "width" as
 * a Read/Write argument and "height" as a Read Only argument.  It
 * should also do an ECanvas parent reflow request if its size
 * changes.
 */
GType    e_reflow_get_type       (void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_REFLOW_H */

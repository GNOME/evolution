/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-group.c
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

#include <config.h>

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

#include "e-util/e-util.h"

#include "e-table-group.h"
#include "e-table-group-container.h"
#include "e-table-group-leaf.h"
#include "e-table-item.h"

#define PARENT_TYPE gnome_canvas_group_get_type ()

#define ETG_CLASS(e) (E_TABLE_GROUP_CLASS(GTK_OBJECT_GET_CLASS(e)))

static GnomeCanvasGroupClass *etg_parent_class;

enum {
	CURSOR_CHANGE,
	CURSOR_ACTIVATED,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	CLICK,
	KEY_PRESS,
	START_DRAG,
	LAST_SIGNAL
};

static guint etg_signals [LAST_SIGNAL] = { 0, };

static gboolean etg_get_focus (ETableGroup      *etg);

static void
etg_dispose (GObject *object)
{
	ETableGroup *etg = E_TABLE_GROUP(object);

	if (etg->header) {
		g_object_unref (etg->header);
		etg->header = NULL;
	}

	if (etg->full_header) {
		g_object_unref (etg->full_header);
		etg->full_header = NULL;
	}

	if (etg->model) {
		g_object_unref (etg->model);
		etg->model = NULL;
	}

	if (G_OBJECT_CLASS (etg_parent_class)->dispose)
		G_OBJECT_CLASS (etg_parent_class)->dispose (object);
}

/**
 * e_table_group_new
 * @parent: The %GnomeCanvasGroup to create a child of.
 * @full_header: The full header of the %ETable.
 * @header: The current header of the %ETable.
 * @model: The %ETableModel of the %ETable.
 * @sort_info: The %ETableSortInfo of the %ETable.
 * @n: The grouping information object to group by.
 *
 * %ETableGroup is a collection of rows of an %ETable.  It's a
 * %GnomeCanvasItem.  There are two different forms.  If n < the
 * number of groupings in the given %ETableSortInfo, then the
 * %ETableGroup will need to contain other %ETableGroups, thus it
 * creates an %ETableGroupContainer.  Otherwise, it will just contain
 * an %ETableItem, and thus it creates an %ETableGroupLeaf.
 *
 * Returns: The new %ETableGroup.
 */
ETableGroup *
e_table_group_new (GnomeCanvasGroup *parent,
		   ETableHeader     *full_header,
		   ETableHeader     *header,
		   ETableModel      *model,
		   ETableSortInfo   *sort_info,
		   int               n)
{
	g_return_val_if_fail (model != NULL, NULL);

	if (n < e_table_sort_info_grouping_get_count(sort_info)) {
		return e_table_group_container_new (parent, full_header, header, model, sort_info, n);
	} else {
		return e_table_group_leaf_new (parent, full_header, header, model, sort_info);
	}
	return NULL;
}

/**
 * e_table_group_construct
 * @parent: The %GnomeCanvasGroup to create a child of.
 * @etg: The %ETableGroup to construct.
 * @full_header: The full header of the %ETable.
 * @header: The current header of the %ETable.
 * @model: The %ETableModel of the %ETable.
 *
 * This routine does the base construction of the %ETableGroup.
 */
void
e_table_group_construct (GnomeCanvasGroup *parent,
			 ETableGroup      *etg,
			 ETableHeader     *full_header,
			 ETableHeader     *header,
			 ETableModel      *model)
{
	etg->full_header = full_header;
	g_object_ref (etg->full_header);
	etg->header = header;
	g_object_ref (etg->header);
	etg->model = model;
	g_object_ref (etg->model);
	g_object_set (G_OBJECT (etg),
		"parent", parent,
		NULL);
}

/**
 * e_table_group_add
 * @etg: The %ETableGroup to add a row to
 * @row: The row to add.
 *
 * This routine adds the given row from the %ETableModel to this set
 * of rows.
 */
void
e_table_group_add (ETableGroup *etg,
		   gint row)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->add != NULL);
	ETG_CLASS (etg)->add (etg, row);
}

/**
 * e_table_group_add_array
 * @etg: The %ETableGroup to add to
 * @array: The array to add.
 * @count: The number of times to add
 *
 * This routine adds all the rows in the array to this set of rows.
 * It assumes that the array is already sorted properly.
 */
void
e_table_group_add_array (ETableGroup *etg,
			 const int *array,
			 int count)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->add_array != NULL);
	ETG_CLASS (etg)->add_array (etg, array, count);
}

/**
 * e_table_group_add_all
 * @etg: The %ETableGroup to add to
 *
 * This routine adds all the rows from the %ETableModel to this set
 * of rows.
 */
void
e_table_group_add_all (ETableGroup *etg)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->add_all != NULL);
	ETG_CLASS (etg)->add_all (etg);
}

/**
 * e_table_group_remove
 * @etg: The %ETableGroup to remove a row from
 * @row: The row to remove.
 *
 * This routine removes the given row from the %ETableModel from this
 * set of rows.
 *
 * Returns: TRUE if the row was deleted and FALSE if the row was not
 * found.
 */
gboolean
e_table_group_remove (ETableGroup *etg,
		      gint row)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), FALSE);

	g_assert (ETG_CLASS (etg)->remove != NULL);
	return ETG_CLASS (etg)->remove (etg, row);
}

/**
 * e_table_group_increment
 * @etg: The %ETableGroup to increment
 * @position: The position to increment from
 * @amount: The amount to increment.
 *
 * This routine adds amount to all rows greater than or equal to
 * position.  This is to handle when a row gets inserted into the
 * model.
 */
void
e_table_group_increment (ETableGroup *etg,
			 gint position,
			 gint amount)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->increment != NULL);
	ETG_CLASS (etg)->increment (etg, position, amount);
}

/**
 * e_table_group_increment
 * @etg: The %ETableGroup to decrement
 * @position: The position to decrement from
 * @amount: The amount to decrement
 *
 * This routine removes amount from all rows greater than or equal to
 * position.  This is to handle when a row gets deleted from the
 * model.
 */
void
e_table_group_decrement (ETableGroup *etg,
			 gint position,
			 gint amount)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->decrement != NULL);
	ETG_CLASS (etg)->decrement (etg, position, amount);
}

/**
 * e_table_group_increment
 * @etg: The %ETableGroup to count
 *
 * This routine calculates the number of rows shown in this group.
 *
 * Returns: The number of rows.
 */
gint
e_table_group_row_count (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), -1);

	g_assert (ETG_CLASS (etg)->row_count != NULL);
	return ETG_CLASS (etg)->row_count (etg);
}

/**
 * e_table_group_set_focus
 * @etg: The %ETableGroup to set
 * @direction: The direction the focus is coming from.
 * @view_col: The column to set the focus in.
 *
 * Sets the focus to this widget.  Places the focus in the view column
 * coming from direction direction.
 */
void
e_table_group_set_focus (ETableGroup *etg,
			 EFocus direction,
			 gint view_col)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->set_focus != NULL);
	ETG_CLASS (etg)->set_focus (etg, direction, view_col);
}

/**
 * e_table_group_get_focus
 * @etg: The %ETableGroup to check
 *
 * Calculates if this group has the focus.
 *
 * Returns: TRUE if this group has the focus.
 */
gboolean
e_table_group_get_focus (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), FALSE);

	g_assert (ETG_CLASS (etg)->get_focus != NULL);
	return ETG_CLASS (etg)->get_focus (etg);
}

/**
 * e_table_group_get_focus_column
 * @etg: The %ETableGroup to check
 *
 * Calculates which column in this group has the focus.
 *
 * Returns: The column index (view column).
 */
gint
e_table_group_get_focus_column (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), -1);

	g_assert (ETG_CLASS (etg)->get_focus_column != NULL);
	return ETG_CLASS (etg)->get_focus_column (etg);
}

/**
 * e_table_group_get_printable
 * @etg: %ETableGroup which will be printed
 *
 * This routine creates and returns an %EPrintable that can be used to
 * print the given %ETableGroup.
 *
 * Returns: The %EPrintable.
 */
EPrintable *
e_table_group_get_printable (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), NULL);

	g_assert (ETG_CLASS (etg)->get_printable != NULL);
	return ETG_CLASS (etg)->get_printable (etg);
}

/**
 * e_table_group_compute_location
 * @eti: %ETableGroup to look in.
 * @x: A pointer to the x location to find in the %ETableGroup.
 * @y: A pointer to the y location to find in the %ETableGroup.
 * @row: A pointer to the location to store the found row in.
 * @col: A pointer to the location to store the found col in.
 *
 * This routine locates the pixel location (*x, *y) in the
 * %ETableGroup.  If that location is in the %ETableGroup, *row and
 * *col are set to the view row and column where it was found.  If
 * that location is not in the %ETableGroup, the height of the
 * %ETableGroup is removed from the value y points to.
 */
void
e_table_group_compute_location (ETableGroup *etg, int *x, int *y, int *row, int *col)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->compute_location != NULL);
	ETG_CLASS (etg)->compute_location (etg, x, y, row, col);
}

/**
 * e_table_group_get_position
 * @eti: %ETableGroup to look in.
 * @x: A pointer to the location to store the found x location in.
 * @y: A pointer to the location to store the found y location in.
 * @row: A pointer to the row number to find.
 * @col: A pointer to the col number to find.
 *
 * This routine finds the view cell (row, col) in the #ETableGroup.
 * If that location is in the #ETableGroup *@x and *@y are set to the
 * upper left hand corner of the cell found.  If that location is not
 * in the #ETableGroup, the number of rows in the #ETableGroup is
 * removed from the value row points to.
 */
void
e_table_group_get_cell_geometry  (ETableGroup *etg,
				  int         *row,
				  int         *col,
				  int         *x,
				  int         *y,
				  int         *width,
				  int         *height)
{
	g_return_if_fail (etg != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (etg));

	g_assert (ETG_CLASS (etg)->get_cell_geometry != NULL);
	ETG_CLASS (etg)->get_cell_geometry (etg, row, col, x, y, width, height);
}

/**
 * e_table_group_cursor_change
 * @eti: %ETableGroup to emit the signal on
 * @row: The new cursor row (model row)
 *
 * This routine emits the "cursor_change" signal.
 */
void
e_table_group_cursor_change (ETableGroup *e_table_group, gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (e_table_group,
		       etg_signals [CURSOR_CHANGE], 0,
		       row);
}

/**
 * e_table_group_cursor_activated
 * @eti: %ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 *
 * This routine emits the "cursor_activated" signal.
 */
void
e_table_group_cursor_activated (ETableGroup *e_table_group, gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (e_table_group,
		       etg_signals [CURSOR_ACTIVATED], 0,
		       row);
}

/**
 * e_table_group_double_click
 * @eti: %ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "double_click" signal.
 */
void
e_table_group_double_click (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (e_table_group,
		       etg_signals [DOUBLE_CLICK], 0,
		       row, col, event);
}

/**
 * e_table_group_right_click
 * @eti: %ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "right_click" signal.
 */
gint
e_table_group_right_click (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	g_signal_emit (e_table_group,
		       etg_signals [RIGHT_CLICK], 0,
		       row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_click
 * @eti: %ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "click" signal.
 */
gint
e_table_group_click (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	g_signal_emit (e_table_group,
		       etg_signals [CLICK], 0,
		       row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_key_press
 * @eti: %ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 * @col: The cursor col (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "key_press" signal.
 */
gint
e_table_group_key_press (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	g_signal_emit (e_table_group,
		       etg_signals [KEY_PRESS], 0,
		       row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_start_drag
 * @eti: %ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 * @col: The cursor col (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "start_drag" signal.
 */
gint
e_table_group_start_drag (ETableGroup *e_table_group, gint row, gint col, GdkEvent *event)
{
	gint return_val = 0;

	g_return_val_if_fail (e_table_group != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), 0);

	g_signal_emit (e_table_group,
		       etg_signals [START_DRAG], 0,
		       row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_get_header
 * @eti: %ETableGroup to check
 *
 * This routine returns the %ETableGroup's header.
 *
 * Returns: The %ETableHeader.
 */
ETableHeader *
e_table_group_get_header (ETableGroup *etg)
{
	g_return_val_if_fail (etg != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_GROUP (etg), NULL);

	return etg->header;
}

static int
etg_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ETableGroup *etg = E_TABLE_GROUP (item);
	gboolean return_val = TRUE;

	switch (event->type) {

	case GDK_FOCUS_CHANGE:
		etg->has_focus = event->focus_change.in;
		return_val = FALSE;

	default:
		return_val = FALSE;
	}
	if (return_val == FALSE){
		if (GNOME_CANVAS_ITEM_CLASS(etg_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS(etg_parent_class)->event (item, event);
	}
	return return_val;

}

static gboolean
etg_get_focus (ETableGroup      *etg)
{
	return etg->has_focus;
}

static void
etg_class_init (GObjectClass *object_class)
{
	GnomeCanvasItemClass *item_class = (GnomeCanvasItemClass *) object_class;
	ETableGroupClass *klass = (ETableGroupClass *) object_class;

	object_class->dispose = etg_dispose;

	item_class->event = etg_event;

	klass->cursor_change = NULL;
	klass->cursor_activated = NULL;
	klass->double_click = NULL;
	klass->right_click = NULL;
	klass->click = NULL;
	klass->key_press = NULL;
	klass->start_drag = NULL;

	klass->add = NULL;
	klass->add_array = NULL;
	klass->add_all = NULL;
	klass->remove = NULL;
	klass->row_count  = NULL;
	klass->increment  = NULL;
	klass->decrement  = NULL;
	klass->set_focus  = NULL;
	klass->get_focus = etg_get_focus;
	klass->get_printable = NULL;
	klass->compute_location = NULL;
	klass->get_cell_geometry = NULL;

	etg_parent_class = g_type_class_ref (PARENT_TYPE);

	etg_signals [CURSOR_CHANGE] =
		g_signal_new ("cursor_change",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, cursor_change),
			      NULL, NULL,
			      e_util_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	etg_signals [CURSOR_ACTIVATED] =
		g_signal_new ("cursor_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, cursor_activated),
			      NULL, NULL,
			      e_util_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	etg_signals [DOUBLE_CLICK] =
		g_signal_new ("double_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, double_click),
			      NULL, NULL,
			      e_util_marshal_NONE__INT_INT_BOXED,
			      G_TYPE_NONE, 3, G_TYPE_INT,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	etg_signals [RIGHT_CLICK] =
		g_signal_new ("right_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, right_click),
			      NULL, NULL,
			      e_util_marshal_INT__INT_INT_BOXED,
			      G_TYPE_INT, 3, G_TYPE_INT, G_TYPE_INT, GDK_TYPE_EVENT);

	etg_signals [CLICK] =
		g_signal_new ("click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, click),
			      NULL, NULL,
			      e_util_marshal_INT__INT_INT_BOXED,
			      G_TYPE_INT, 3, G_TYPE_INT,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	etg_signals [KEY_PRESS] =
		g_signal_new ("key_press",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, key_press),
			      NULL, NULL,
			      e_util_marshal_INT__INT_INT_BOXED,
			      G_TYPE_INT, 3, G_TYPE_INT,
			      G_TYPE_INT, GDK_TYPE_EVENT);

	etg_signals [START_DRAG] =
		g_signal_new ("start_drag",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETableGroupClass, start_drag),
			      NULL, NULL,
			      e_util_marshal_INT__INT_INT_BOXED,
			      G_TYPE_INT, 3, G_TYPE_INT,
			      G_TYPE_INT, GDK_TYPE_EVENT);
}

E_MAKE_TYPE (e_table_group, "ETableGroup", ETableGroup, etg_class_init, NULL, PARENT_TYPE)

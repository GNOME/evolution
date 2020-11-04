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

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "e-table-group.h"
#include "e-table-group-container.h"
#include "e-table-group-leaf.h"
#include "e-table-item.h"

G_DEFINE_TYPE (
	ETableGroup,
	e_table_group,
	GNOME_TYPE_CANVAS_GROUP)

#define ETG_CLASS(e) (E_TABLE_GROUP_CLASS(G_OBJECT_GET_CLASS(e)))

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

enum {
	PROP_0,
	PROP_IS_EDITING
};

static guint etg_signals[LAST_SIGNAL] = { 0, };

static gboolean etg_get_focus (ETableGroup *table_group);

static void
etg_dispose (GObject *object)
{
	ETableGroup *table_group = E_TABLE_GROUP (object);

	g_clear_object (&table_group->header);
	g_clear_object (&table_group->full_header);
	g_clear_object (&table_group->model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_group_parent_class)->dispose (object);
}

static void
etg_get_property (GObject *object,
                  guint property_id,
                  GValue *value,
                  GParamSpec *pspec)
{
	ETableGroup *etg = E_TABLE_GROUP (object);

	switch (property_id) {
	case PROP_IS_EDITING:
		g_value_set_boolean (value, e_table_group_is_editing (etg));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

/**
 * e_table_group_new
 * @parent: The %GnomeCanvasGroup to create a child of.
 * @full_header: The full header of the #ETable.
 * @header: The current header of the #ETable.
 * @model: The #ETableModel of the #ETable.
 * @sort_info: The #ETableSortInfo of the #ETable.
 * @n: The grouping information object to group by.
 *
 * #ETableGroup is a collection of rows of an #ETable.  It's a
 * %GnomeCanvasItem.  There are two different forms.  If n < the
 * number of groupings in the given #ETableSortInfo, then the
 * #ETableGroup will need to contain other #ETableGroups, thus it
 * creates an #ETableGroupContainer.  Otherwise, it will just contain
 * an #ETableItem, and thus it creates an #ETableGroupLeaf.
 *
 * Returns: The new #ETableGroup.
 */
ETableGroup *
e_table_group_new (GnomeCanvasGroup *parent,
                   ETableHeader *full_header,
                   ETableHeader *header,
                   ETableModel *model,
                   ETableSortInfo *sort_info,
                   gint n)
{
	g_return_val_if_fail (model != NULL, NULL);

	if (n < e_table_sort_info_grouping_get_count (sort_info)) {
		return e_table_group_container_new (
			parent, full_header, header, model, sort_info, n);
	} else {
		return e_table_group_leaf_new (
			parent, full_header, header, model, sort_info);
	}
}

/**
 * e_table_group_construct
 * @parent: The %GnomeCanvasGroup to create a child of.
 * @table_group: The #ETableGroup to construct.
 * @full_header: The full header of the #ETable.
 * @header: The current header of the #ETable.
 * @model: The #ETableModel of the #ETable.
 *
 * This routine does the base construction of the #ETableGroup.
 */
void
e_table_group_construct (GnomeCanvasGroup *parent,
                         ETableGroup *table_group,
                         ETableHeader *full_header,
                         ETableHeader *header,
                         ETableModel *model)
{
	table_group->full_header = g_object_ref (full_header);
	table_group->header = g_object_ref (header);
	table_group->model = g_object_ref (model);
	g_object_set (table_group, "parent", parent, NULL);
}

/**
 * e_table_group_add
 * @table_group: The #ETableGroup to add a row to
 * @row: The row to add.
 *
 * This routine adds the given row from the #ETableModel to this set
 * of rows.
 */
void
e_table_group_add (ETableGroup *table_group,
                   gint row)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->add != NULL);
	ETG_CLASS (table_group)->add (table_group, row);
}

/**
 * e_table_group_add_array
 * @table_group: The #ETableGroup to add to
 * @array: The array to add.
 * @count: The number of times to add
 *
 * This routine adds all the rows in the array to this set of rows.
 * It assumes that the array is already sorted properly.
 */
void
e_table_group_add_array (ETableGroup *table_group,
                         const gint *array,
                         gint count)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->add_array != NULL);
	ETG_CLASS (table_group)->add_array (table_group, array, count);
}

/**
 * e_table_group_add_all
 * @table_group: The #ETableGroup to add to
 *
 * This routine adds all the rows from the #ETableModel to this set
 * of rows.
 */
void
e_table_group_add_all (ETableGroup *table_group)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->add_all != NULL);
	ETG_CLASS (table_group)->add_all (table_group);
}

/**
 * e_table_group_remove
 * @table_group: The #ETableGroup to remove a row from
 * @row: The row to remove.
 *
 * This routine removes the given row from the #ETableModel from this
 * set of rows.
 *
 * Returns: TRUE if the row was deleted and FALSE if the row was not
 * found.
 */
gboolean
e_table_group_remove (ETableGroup *table_group,
                      gint row)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), FALSE);

	g_return_val_if_fail (ETG_CLASS (table_group)->remove != NULL, FALSE);
	return ETG_CLASS (table_group)->remove (table_group, row);
}

/**
 * e_table_group_increment
 * @table_group: The #ETableGroup to increment
 * @position: The position to increment from
 * @amount: The amount to increment.
 *
 * This routine adds amount to all rows greater than or equal to
 * position.  This is to handle when a row gets inserted into the
 * model.
 */
void
e_table_group_increment (ETableGroup *table_group,
                         gint position,
                         gint amount)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->increment != NULL);
	ETG_CLASS (table_group)->increment (table_group, position, amount);
}

/**
 * e_table_group_increment
 * @table_group: The #ETableGroup to decrement
 * @position: The position to decrement from
 * @amount: The amount to decrement
 *
 * This routine removes amount from all rows greater than or equal to
 * position.  This is to handle when a row gets deleted from the
 * model.
 */
void
e_table_group_decrement (ETableGroup *table_group,
                         gint position,
                         gint amount)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->decrement != NULL);
	ETG_CLASS (table_group)->decrement (table_group, position, amount);
}

/**
 * e_table_group_increment
 * @table_group: The #ETableGroup to count
 *
 * This routine calculates the number of rows shown in this group.
 *
 * Returns: The number of rows.
 */
gint
e_table_group_row_count (ETableGroup *table_group)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), -1);

	g_return_val_if_fail (ETG_CLASS (table_group)->row_count != NULL, -1);
	return ETG_CLASS (table_group)->row_count (table_group);
}

/**
 * e_table_group_set_focus
 * @table_group: The #ETableGroup to set
 * @direction: The direction the focus is coming from.
 * @view_col: The column to set the focus in.
 *
 * Sets the focus to this widget.  Places the focus in the view column
 * coming from direction direction.
 */
void
e_table_group_set_focus (ETableGroup *table_group,
                         EFocus direction,
                         gint view_col)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->set_focus != NULL);
	ETG_CLASS (table_group)->set_focus (table_group, direction, view_col);
}

/**
 * e_table_group_get_focus
 * @table_group: The #ETableGroup to check
 *
 * Calculates if this group has the focus.
 *
 * Returns: TRUE if this group has the focus.
 */
gboolean
e_table_group_get_focus (ETableGroup *table_group)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), FALSE);

	g_return_val_if_fail (ETG_CLASS (table_group)->get_focus != NULL, FALSE);
	return ETG_CLASS (table_group)->get_focus (table_group);
}

/**
 * e_table_group_get_focus_column
 * @table_group: The #ETableGroup to check
 *
 * Calculates which column in this group has the focus.
 *
 * Returns: The column index (view column).
 */
gint
e_table_group_get_focus_column (ETableGroup *table_group)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), -1);

	g_return_val_if_fail (ETG_CLASS (table_group)->get_focus_column != NULL, -1);
	return ETG_CLASS (table_group)->get_focus_column (table_group);
}

/**
 * e_table_group_get_printable
 * @table_group: #ETableGroup which will be printed
 *
 * This routine creates and returns an %EPrintable that can be used to
 * print the given #ETableGroup.
 *
 * Returns: The %EPrintable.
 */
EPrintable *
e_table_group_get_printable (ETableGroup *table_group)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), NULL);

	g_return_val_if_fail (ETG_CLASS (table_group)->get_printable != NULL, NULL);
	return ETG_CLASS (table_group)->get_printable (table_group);
}

/**
 * e_table_group_compute_location
 * @table_group: #ETableGroup to look in.
 * @x: A pointer to the x location to find in the #ETableGroup.
 * @y: A pointer to the y location to find in the #ETableGroup.
 * @row: A pointer to the location to store the found row in.
 * @col: A pointer to the location to store the found col in.
 *
 * This routine locates the pixel location (*x, *y) in the
 * #ETableGroup.  If that location is in the #ETableGroup, *row and
 * *col are set to the view row and column where it was found.  If
 * that location is not in the #ETableGroup, the height of the
 * #ETableGroup is removed from the value y points to.
 */
void
e_table_group_compute_location (ETableGroup *table_group,
                                gint *x,
                                gint *y,
                                gint *row,
                                gint *col)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->compute_location != NULL);
	ETG_CLASS (table_group)->compute_location (table_group, x, y, row, col);
}

void
e_table_group_get_mouse_over (ETableGroup *table_group,
                              gint *row,
                              gint *col)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->get_mouse_over != NULL);
	ETG_CLASS (table_group)->get_mouse_over (table_group, row, col);
}

/**
 * e_table_group_get_position
 * @table_group: #ETableGroup to look in.
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
e_table_group_get_cell_geometry (ETableGroup *table_group,
                                 gint *row,
                                 gint *col,
                                 gint *x,
                                 gint *y,
                                 gint *width,
                                 gint *height)
{
	g_return_if_fail (E_IS_TABLE_GROUP (table_group));

	g_return_if_fail (ETG_CLASS (table_group)->get_cell_geometry != NULL);
	ETG_CLASS (table_group)->get_cell_geometry (table_group, row, col, x, y, width, height);
}

/**
 * e_table_group_cursor_change
 * @table_group: #ETableGroup to emit the signal on
 * @row: The new cursor row (model row)
 *
 * This routine emits the "cursor_change" signal.
 */
void
e_table_group_cursor_change (ETableGroup *e_table_group,
                             gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (
		e_table_group,
		etg_signals[CURSOR_CHANGE], 0,
		row);
}

/**
 * e_table_group_cursor_activated
 * @table_group: #ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 *
 * This routine emits the "cursor_activated" signal.
 */
void
e_table_group_cursor_activated (ETableGroup *e_table_group,
                                gint row)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (
		e_table_group,
		etg_signals[CURSOR_ACTIVATED], 0,
		row);
}

/**
 * e_table_group_double_click
 * @table_group: #ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "double_click" signal.
 */
void
e_table_group_double_click (ETableGroup *e_table_group,
                            gint row,
                            gint col,
                            GdkEvent *event)
{
	g_return_if_fail (e_table_group != NULL);
	g_return_if_fail (E_IS_TABLE_GROUP (e_table_group));

	g_signal_emit (
		e_table_group,
		etg_signals[DOUBLE_CLICK], 0,
		row, col, event);
}

/**
 * e_table_group_right_click
 * @table_group: #ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "right_click" signal.
 */
gboolean
e_table_group_right_click (ETableGroup *e_table_group,
                           gint row,
                           gint col,
                           GdkEvent *event)
{
	gboolean return_val = FALSE;

	g_return_val_if_fail (e_table_group != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), FALSE);

	g_signal_emit (
		e_table_group,
		etg_signals[RIGHT_CLICK], 0,
		row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_click
 * @table_group: #ETableGroup to emit the signal on
 * @row: The row clicked on (model row)
 * @col: The col clicked on (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "click" signal.
 */
gboolean
e_table_group_click (ETableGroup *table_group,
                     gint row,
                     gint col,
                     GdkEvent *event)
{
	gboolean return_val = FALSE;

	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), FALSE);

	g_signal_emit (
		table_group,
		etg_signals[CLICK], 0,
		row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_key_press
 * @table_group: #ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 * @col: The cursor col (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "key_press" signal.
 */
gboolean
e_table_group_key_press (ETableGroup *e_table_group,
                         gint row,
                         gint col,
                         GdkEvent *event)
{
	gboolean return_val = FALSE;

	g_return_val_if_fail (e_table_group != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), FALSE);

	g_signal_emit (
		e_table_group,
		etg_signals[KEY_PRESS], 0,
		row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_start_drag
 * @table_group: #ETableGroup to emit the signal on
 * @row: The cursor row (model row)
 * @col: The cursor col (model col)
 * @event: The event that caused this signal
 *
 * This routine emits the "start_drag" signal.
 */
gboolean
e_table_group_start_drag (ETableGroup *e_table_group,
                          gint row,
                          gint col,
                          GdkEvent *event)
{
	gboolean return_val = FALSE;

	g_return_val_if_fail (e_table_group != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_GROUP (e_table_group), FALSE);

	g_signal_emit (
		e_table_group,
		etg_signals[START_DRAG], 0,
		row, col, event, &return_val);

	return return_val;
}

/**
 * e_table_group_get_header
 * @table_group: #ETableGroup to check
 *
 * This routine returns the #ETableGroup's header.
 *
 * Returns: The #ETableHeader.
 */
ETableHeader *
e_table_group_get_header (ETableGroup *table_group)
{
	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), NULL);

	return table_group->header;
}

static gint
etg_event (GnomeCanvasItem *item,
           GdkEvent *event)
{
	ETableGroup *table_group = E_TABLE_GROUP (item);
	gboolean return_val = TRUE;

	switch (event->type) {

	case GDK_FOCUS_CHANGE:
		table_group->has_focus = event->focus_change.in;
		return_val = FALSE;
		break;

	default:
		return_val = FALSE;
	}
	if (return_val == FALSE) {
		if (GNOME_CANVAS_ITEM_CLASS (e_table_group_parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS (e_table_group_parent_class)->event (item, event);
	}
	return return_val;

}

static gboolean
etg_get_focus (ETableGroup *table_group)
{
	return table_group->has_focus;
}

static void
e_table_group_class_init (ETableGroupClass *class)
{
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = etg_dispose;
	object_class->get_property = etg_get_property;

	item_class->event = etg_event;

	class->cursor_change = NULL;
	class->cursor_activated = NULL;
	class->double_click = NULL;
	class->right_click = NULL;
	class->click = NULL;
	class->key_press = NULL;
	class->start_drag = NULL;

	class->add = NULL;
	class->add_array = NULL;
	class->add_all = NULL;
	class->remove = NULL;
	class->row_count = NULL;
	class->increment = NULL;
	class->decrement = NULL;
	class->set_focus = NULL;
	class->get_focus = etg_get_focus;
	class->get_printable = NULL;
	class->compute_location = NULL;
	class->get_mouse_over = NULL;
	class->get_cell_geometry = NULL;

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
			G_PARAM_READABLE));

	etg_signals[CURSOR_CHANGE] = g_signal_new (
		"cursor_change",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, cursor_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	etg_signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, cursor_activated),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	etg_signals[DOUBLE_CLICK] = g_signal_new (
		"double_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, double_click),
		NULL, NULL,
		e_marshal_VOID__INT_INT_BOXED,
		G_TYPE_NONE, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	etg_signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, right_click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	etg_signals[CLICK] = g_signal_new (
		"click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, click),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	etg_signals[KEY_PRESS] = g_signal_new (
		"key_press",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, key_press),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	etg_signals[START_DRAG] = g_signal_new (
		"start_drag",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETableGroupClass, start_drag),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__INT_INT_BOXED,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_INT,
		G_TYPE_INT,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
e_table_group_init (ETableGroup *table_group)
{
	/* nothing to do */
}

gboolean
e_table_group_is_editing (ETableGroup *table_group)
{
	static gboolean in = FALSE;
	gboolean is_editing = FALSE;

	g_return_val_if_fail (E_IS_TABLE_GROUP (table_group), FALSE);

	/* this should be called from the main thread only,
	 * and each descendant overrides the property,
	 * thus might cause no call recursion */
	if (in) {
		g_warn_if_reached ();
		return FALSE;
	}

	in = TRUE;

	g_object_get (G_OBJECT (table_group), "is-editing", &is_editing, NULL);

	in = FALSE;

	return is_editing;
}

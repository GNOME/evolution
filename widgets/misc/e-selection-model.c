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

#include <config.h>

#include <gdk/gdkkeysyms.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "e-selection-model.h"

G_DEFINE_TYPE (
	ESelectionModel,
	e_selection_model,
	G_TYPE_OBJECT)

enum {
	CURSOR_CHANGED,
	CURSOR_ACTIVATED,
	SELECTION_CHANGED,
	SELECTION_ROW_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
	PROP_0,
	PROP_SORTER,
	PROP_SELECTION_MODE,
	PROP_CURSOR_MODE
};

inline static void
add_sorter (ESelectionModel *esm, ESorter *sorter)
{
	esm->sorter = sorter;
	if (sorter) {
		g_object_ref (sorter);
	}
}

inline static void
drop_sorter (ESelectionModel *esm)
{
	if (esm->sorter) {
		g_object_unref (esm->sorter);
	}
	esm->sorter = NULL;
}

static void
esm_dispose (GObject *object)
{
	ESelectionModel *esm;

	esm = E_SELECTION_MODEL (object);

	drop_sorter (esm);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_selection_model_parent_class)->dispose (object);
}

static void
esm_get_property (GObject *object,
                  guint property_id,
                  GValue *value,
                  GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);

	switch (property_id) {
		case PROP_SORTER:
			g_value_set_object (value, esm->sorter);
			break;

		case PROP_SELECTION_MODE:
			g_value_set_int (value, esm->mode);
			break;

		case PROP_CURSOR_MODE:
			g_value_set_int (value, esm->cursor_mode);
			break;
	}
}

static void
esm_set_property (GObject *object,
                  guint property_id,
                  const GValue *value,
                  GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);

	switch (property_id) {
		case PROP_SORTER:
			drop_sorter (esm);
			add_sorter (
				esm, g_value_get_object (value) ?
				E_SORTER (g_value_get_object (value)) : NULL);
			break;

		case PROP_SELECTION_MODE:
			esm->mode = g_value_get_int (value);
			if (esm->mode == GTK_SELECTION_SINGLE) {
				gint cursor_row = e_selection_model_cursor_row (esm);
				gint cursor_col = e_selection_model_cursor_col (esm);
				e_selection_model_do_something (esm, cursor_row, cursor_col, 0);
			}
			break;

		case PROP_CURSOR_MODE:
			esm->cursor_mode = g_value_get_int (value);
			break;
	}
}

static void
e_selection_model_init (ESelectionModel *selection)
{
	selection->mode = GTK_SELECTION_MULTIPLE;
	selection->cursor_mode = E_CURSOR_SIMPLE;
	selection->old_selection = -1;
}

static void
e_selection_model_class_init (ESelectionModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = esm_dispose;
	object_class->get_property = esm_get_property;
	object_class->set_property = esm_set_property;

	signals[CURSOR_CHANGED] = g_signal_new (
		"cursor_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, cursor_changed),
		NULL, NULL,
		e_marshal_NONE__INT_INT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, cursor_activated),
		NULL, NULL,
		e_marshal_NONE__INT_INT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[SELECTION_CHANGED] = g_signal_new (
		"selection_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SELECTION_ROW_CHANGED] = g_signal_new (
		"selection_row_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, selection_row_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	g_object_class_install_property (
		object_class,
		PROP_SORTER,
		g_param_spec_object (
			"sorter",
			"Sorter",
			NULL,
			E_SORTER_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTION_MODE,
		g_param_spec_int (
			"selection_mode",
			"Selection Mode",
			NULL,
			GTK_SELECTION_NONE,
			GTK_SELECTION_MULTIPLE,
			GTK_SELECTION_SINGLE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_MODE,
		g_param_spec_int (
			"cursor_mode",
			"Cursor Mode",
			NULL,
			E_CURSOR_LINE,
			E_CURSOR_SPREADSHEET,
			E_CURSOR_LINE,
			G_PARAM_READWRITE));
}

/**
 * e_selection_model_is_row_selected
 * @selection: #ESelectionModel to check
 * @n: The row to check
 *
 * This routine calculates whether the given row is selected.
 *
 * Returns: %TRUE if the given row is selected
 */
gboolean
e_selection_model_is_row_selected (ESelectionModel *selection,
                                   gint n)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), FALSE);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_val_if_fail (class->is_row_selected != NULL, FALSE);

	return class->is_row_selected (selection, n);
}

/**
 * e_selection_model_foreach
 * @selection: #ESelectionModel to traverse
 * @callback: The callback function to call back.
 * @closure: The closure
 *
 * This routine calls the given callback function once for each
 * selected row, passing closure as the closure.
 */
void
e_selection_model_foreach (ESelectionModel *selection,
                           EForeachFunc callback,
                           gpointer closure)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));
	g_return_if_fail (callback != NULL);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->foreach != NULL);

	class->foreach (selection, callback, closure);
}

/**
 * e_selection_model_clear
 * @selection: #ESelectionModel to clear
 *
 * This routine clears the selection to no rows selected.
 */
void
e_selection_model_clear (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->clear != NULL);

	class->clear (selection);
}

/**
 * e_selection_model_selected_count
 * @selection: #ESelectionModel to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
gint
e_selection_model_selected_count (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), 0);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_val_if_fail (class->selected_count != NULL, 0);

	return class->selected_count (selection);
}

/**
 * e_selection_model_select_all
 * @selection: #ESelectionModel to select all
 *
 * This routine selects all the rows in the given
 * #ESelectionModel.
 */
void
e_selection_model_select_all (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->select_all != NULL);

	class->select_all (selection);
}

/**
 * e_selection_model_invert_selection
 * @selection: #ESelectionModel to invert
 *
 * This routine inverts all the rows in the given
 * #ESelectionModel.
 */
void
e_selection_model_invert_selection (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->invert_selection != NULL);

	class->invert_selection (selection);
}

gint
e_selection_model_row_count (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), 0);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_val_if_fail (class->row_count != NULL, 0);

	return class->row_count (selection);
}

void
e_selection_model_change_one_row (ESelectionModel *selection,
                                  gint row,
                                  gboolean grow)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->change_one_row != NULL);

	return class->change_one_row (selection, row, grow);
}

void
e_selection_model_change_cursor (ESelectionModel *selection,
                                 gint row,
                                 gint col)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->change_cursor != NULL);

	class->change_cursor (selection, row, col);
}

gint
e_selection_model_cursor_row (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), -1);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_val_if_fail (class->cursor_row != NULL, -1);

	return class->cursor_row (selection);
}

gint
e_selection_model_cursor_col (ESelectionModel *selection)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), -1);

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_val_if_fail (class->cursor_col != NULL, -1);

	return class->cursor_col (selection);
}

void
e_selection_model_select_single_row (ESelectionModel *selection,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->select_single_row != NULL);

	class->select_single_row (selection, row);
}

void
e_selection_model_toggle_single_row (ESelectionModel *selection,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->toggle_single_row != NULL);

	class->toggle_single_row (selection, row);
}

void
e_selection_model_move_selection_end (ESelectionModel *selection,
                                      gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->move_selection_end != NULL);

	class->move_selection_end (selection, row);
}

void
e_selection_model_set_selection_end (ESelectionModel *selection,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	class = E_SELECTION_MODEL_GET_CLASS (selection);
	g_return_if_fail (class->set_selection_end != NULL);

	class->set_selection_end (selection, row);
}

/**
 * e_selection_model_do_something
 * @selection: #ESelectionModel to do something to.
 * @row: The row to do something in.
 * @col: The col to do something in.
 * @state: The state in which to do something.
 *
 * This routine does whatever is appropriate as if the user clicked
 * the mouse in the given row and column.
 */
void
e_selection_model_do_something (ESelectionModel *selection,
                                guint row,
                                guint col,
                                GdkModifierType state)
{
	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;
	gint row_count;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	selection->old_selection = -1;

	if (row == -1 && col != -1)
		row = 0;
	if (col == -1 && row != -1)
		col = 0;

	row_count = e_selection_model_row_count (selection);
	if (row_count >= 0 && row < row_count) {
		switch (selection->mode) {
		case GTK_SELECTION_SINGLE:
			e_selection_model_select_single_row (selection, row);
			break;
		case GTK_SELECTION_BROWSE:
		case GTK_SELECTION_MULTIPLE:
			if (shift_p) {
				e_selection_model_set_selection_end (selection, row);
			} else {
				if (ctrl_p) {
					e_selection_model_toggle_single_row (selection, row);
				} else {
					e_selection_model_select_single_row (selection, row);
				}
			}
			break;
		default:
			g_return_if_reached ();
			break;
		}
		e_selection_model_change_cursor (selection, row, col);
		g_signal_emit (selection,
			      signals[CURSOR_CHANGED], 0,
			      row, col);
		g_signal_emit (selection,
			      signals[CURSOR_ACTIVATED], 0,
			      row, col);
	}
}

/**
 * e_selection_model_maybe_do_something
 * @selection: #ESelectionModel to do something to.
 * @row: The row to do something in.
 * @col: The col to do something in.
 * @state: The state in which to do something.
 *
 * If this row is selected, this routine just moves the cursor row and
 * column.  Otherwise, it does the same thing as
 * e_selection_model_do_something().  This is for being used on
 * right clicks and other events where if the user hit the selection,
 * they don't want it to change.
 */
gboolean
e_selection_model_maybe_do_something (ESelectionModel *selection,
                                      guint row,
                                      guint col,
                                      GdkModifierType state)
{
	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), FALSE);

	selection->old_selection = -1;

	if (e_selection_model_is_row_selected (selection, row)) {
		e_selection_model_change_cursor (selection, row, col);
		g_signal_emit (selection,
			      signals[CURSOR_CHANGED], 0,
			      row, col);
		return FALSE;
	} else {
		e_selection_model_do_something (selection, row, col, state);
		return TRUE;
	}
}

void
e_selection_model_right_click_down (ESelectionModel *selection,
                                    guint row,
                                    guint col,
                                    GdkModifierType state)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	if (selection->mode == GTK_SELECTION_SINGLE) {
		selection->old_selection =
			e_selection_model_cursor_row (selection);
		e_selection_model_select_single_row (selection, row);
	} else {
		e_selection_model_maybe_do_something (
			selection, row, col, state);
	}
}

void
e_selection_model_right_click_up (ESelectionModel *selection)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	if (selection->mode != GTK_SELECTION_SINGLE)
		return;

	if (selection->old_selection == -1)
		return;

	e_selection_model_select_single_row (
		selection, selection->old_selection);
}

void
e_selection_model_select_as_key_press (ESelectionModel *selection,
                                       guint row,
                                       guint col,
                                       GdkModifierType state)
{
	gint cursor_activated = TRUE;

	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;

	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	selection->old_selection = -1;

	switch (selection->mode) {
	case GTK_SELECTION_BROWSE:
	case GTK_SELECTION_MULTIPLE:
		if (shift_p) {
			e_selection_model_set_selection_end (selection, row);
		} else if (!ctrl_p) {
			e_selection_model_select_single_row (selection, row);
		} else
			cursor_activated = FALSE;
		break;
	case GTK_SELECTION_SINGLE:
		e_selection_model_select_single_row (selection, row);
		break;
	default:
		g_return_if_reached ();
		break;
	}
	if (row != -1) {
		e_selection_model_change_cursor (selection, row, col);
		g_signal_emit (selection,
			      signals[CURSOR_CHANGED], 0,
			      row, col);
		if (cursor_activated)
			g_signal_emit (selection,
				      signals[CURSOR_ACTIVATED], 0,
				      row, col);
	}
}

static gint
move_selection (ESelectionModel *selection,
                gboolean up,
                GdkModifierType state)
{
	gint row = e_selection_model_cursor_row (selection);
	gint col = e_selection_model_cursor_col (selection);
	gint row_count;

	/* there is no selected row when row is -1 */
	if (row != -1)
		row = e_sorter_model_to_sorted (selection->sorter, row);

	if (up)
		row--;
	else
		row++;
	if (row < 0)
		row = 0;
	row_count = e_selection_model_row_count (selection);
	if (row >= row_count)
		row = row_count - 1;
	row = e_sorter_sorted_to_model (selection->sorter, row);

	e_selection_model_select_as_key_press (selection, row, col, state);
	return TRUE;
}

/**
 * e_selection_model_key_press
 * @selection: #ESelectionModel to affect.
 * @key: The event.
 *
 * This routine does whatever is appropriate as if the user pressed
 * the given key.
 *
 * Returns: %TRUE if the #ESelectionModel used the key.
 */
gboolean
e_selection_model_key_press (ESelectionModel *selection,
                             GdkEventKey *key)
{
	g_return_val_if_fail (E_IS_SELECTION_MODEL (selection), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	selection->old_selection = -1;

	switch (key->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		return move_selection (selection, TRUE, key->state);
	case GDK_Down:
	case GDK_KP_Down:
		return move_selection (selection, FALSE, key->state);
	case GDK_space:
	case GDK_KP_Space:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			gint row = e_selection_model_cursor_row (selection);
			gint col = e_selection_model_cursor_col (selection);
			if (row == -1)
				break;

			e_selection_model_toggle_single_row (selection, row);
			g_signal_emit (selection,
				      signals[CURSOR_ACTIVATED], 0,
				      row, col);
			return TRUE;
		}
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			gint row = e_selection_model_cursor_row (selection);
			gint col = e_selection_model_cursor_col (selection);
			e_selection_model_select_single_row (selection, row);
			g_signal_emit (selection,
				      signals[CURSOR_ACTIVATED], 0,
				      row, col);
			return TRUE;
		}
		break;
	case GDK_Home:
	case GDK_KP_Home:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			gint row = 0;
			gint cursor_col = e_selection_model_cursor_col (selection);

			row = e_sorter_sorted_to_model (selection->sorter, row);
			e_selection_model_select_as_key_press (selection, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	case GDK_End:
	case GDK_KP_End:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			gint row = e_selection_model_row_count (selection) - 1;
			gint cursor_col = e_selection_model_cursor_col (selection);

			row = e_sorter_sorted_to_model (selection->sorter, row);
			e_selection_model_select_as_key_press (selection, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void
e_selection_model_cursor_changed (ESelectionModel *selection,
                                  gint row,
                                  gint col)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	g_signal_emit (selection, signals[CURSOR_CHANGED], 0, row, col);
}

void
e_selection_model_cursor_activated (ESelectionModel *selection,
                                    gint row,
                                    gint col)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	g_signal_emit (selection, signals[CURSOR_ACTIVATED], 0, row, col);
}

void
e_selection_model_selection_changed (ESelectionModel *selection)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	g_signal_emit (selection, signals[SELECTION_CHANGED], 0);
}

void
e_selection_model_selection_row_changed (ESelectionModel *selection,
                                         gint row)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	g_signal_emit (selection, signals[SELECTION_ROW_CHANGED], 0, row);
}

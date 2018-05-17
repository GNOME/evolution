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

#include "e-selection-model.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-marshal.h"

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
add_sorter (ESelectionModel *model,
            ESorter *sorter)
{
	model->sorter = sorter;
	if (sorter) {
		g_object_ref (sorter);
	}
}

inline static void
drop_sorter (ESelectionModel *model)
{
	if (model->sorter) {
		g_object_unref (model->sorter);
	}
	model->sorter = NULL;
}

static void
selection_model_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	ESelectionModel *model = E_SELECTION_MODEL (object);

	switch (property_id) {
		case PROP_SORTER:
			drop_sorter (model);
			add_sorter (
				model, g_value_get_object (value) ?
				E_SORTER (g_value_get_object (value)) : NULL);
			break;

		case PROP_SELECTION_MODE:
			model->mode = g_value_get_int (value);
			if (model->mode == GTK_SELECTION_SINGLE) {
				gint cursor_row = e_selection_model_cursor_row (model);
				gint cursor_col = e_selection_model_cursor_col (model);
				e_selection_model_do_something (model, cursor_row, cursor_col, 0);
			}
			break;

		case PROP_CURSOR_MODE:
			model->cursor_mode = g_value_get_int (value);
			break;
	}
}

static void
selection_model_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	ESelectionModel *model = E_SELECTION_MODEL (object);

	switch (property_id) {
		case PROP_SORTER:
			g_value_set_object (value, model->sorter);
			break;

		case PROP_SELECTION_MODE:
			g_value_set_int (value, model->mode);
			break;

		case PROP_CURSOR_MODE:
			g_value_set_int (value, model->cursor_mode);
			break;
	}
}

static void
selection_model_dispose (GObject *object)
{
	ESelectionModel *model;

	model = E_SELECTION_MODEL (object);

	drop_sorter (model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_selection_model_parent_class)->dispose (object);
}

static void
e_selection_model_class_init (ESelectionModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = selection_model_set_property;
	object_class->get_property = selection_model_get_property;
	object_class->dispose = selection_model_dispose;

	signals[CURSOR_CHANGED] = g_signal_new (
		"cursor_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, cursor_changed),
		NULL, NULL,
		e_marshal_VOID__INT_INT,
		G_TYPE_NONE, 2,
		G_TYPE_INT,
		G_TYPE_INT);

	signals[CURSOR_ACTIVATED] = g_signal_new (
		"cursor_activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESelectionModelClass, cursor_activated),
		NULL, NULL,
		e_marshal_VOID__INT_INT,
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
			E_TYPE_SORTER,
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

static void
e_selection_model_init (ESelectionModel *model)
{
	model->mode = GTK_SELECTION_MULTIPLE;
	model->cursor_mode = E_CURSOR_SIMPLE;
	model->old_selection = -1;
}

/**
 * e_selection_model_is_row_selected
 * @model: #ESelectionModel to check
 * @n: The row to check
 *
 * This routine calculates whether the given row is selected.
 *
 * Returns: %TRUE if the given row is selected
 */
gboolean
e_selection_model_is_row_selected (ESelectionModel *model,
                                   gint n)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), FALSE);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->is_row_selected != NULL, FALSE);

	return class->is_row_selected (model, n);
}

/**
 * e_selection_model_foreach
 * @model: #ESelectionModel to traverse
 * @callback: The callback function to call back.
 * @closure: The closure
 *
 * This routine calls the given callback function once for each
 * selected row, passing closure as the closure.
 */
void
e_selection_model_foreach (ESelectionModel *model,
                           EForeachFunc callback,
                           gpointer closure)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));
	g_return_if_fail (callback != NULL);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->foreach != NULL);

	class->foreach (model, callback, closure);
}

/**
 * e_selection_model_clear
 * @model: #ESelectionModel to clear
 *
 * This routine clears the selection to no rows selected.
 */
void
e_selection_model_clear (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->clear != NULL);

	class->clear (model);
}

/**
 * e_selection_model_selected_count
 * @model: #ESelectionModel to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
gint
e_selection_model_selected_count (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), 0);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, 0);
	g_return_val_if_fail (class->selected_count != NULL, 0);

	return class->selected_count (model);
}

/**
 * e_selection_model_select_all
 * @model: #ESelectionModel to select all
 *
 * This routine selects all the rows in the given
 * #ESelectionModel.
 */
void
e_selection_model_select_all (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->select_all != NULL);

	class->select_all (model);
}

gint
e_selection_model_row_count (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), 0);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, 0);
	g_return_val_if_fail (class->row_count != NULL, 0);

	return class->row_count (model);
}

void
e_selection_model_change_one_row (ESelectionModel *model,
                                  gint row,
                                  gboolean grow)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->change_one_row != NULL);

	return class->change_one_row (model, row, grow);
}

void
e_selection_model_change_cursor (ESelectionModel *model,
                                 gint row,
                                 gint col)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->change_cursor != NULL);

	class->change_cursor (model, row, col);
}

gint
e_selection_model_cursor_row (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), -1);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, -1);
	g_return_val_if_fail (class->cursor_row != NULL, -1);

	return class->cursor_row (model);
}

gint
e_selection_model_cursor_col (ESelectionModel *model)
{
	ESelectionModelClass *class;

	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), -1);

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_val_if_fail (class != NULL, -1);
	g_return_val_if_fail (class->cursor_col != NULL, -1);

	return class->cursor_col (model);
}

void
e_selection_model_select_single_row (ESelectionModel *model,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->select_single_row != NULL);

	class->select_single_row (model, row);
}

void
e_selection_model_toggle_single_row (ESelectionModel *model,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->toggle_single_row != NULL);

	class->toggle_single_row (model, row);
}

void
e_selection_model_move_selection_end (ESelectionModel *model,
                                      gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->move_selection_end != NULL);

	class->move_selection_end (model, row);
}

void
e_selection_model_set_selection_end (ESelectionModel *model,
                                     gint row)
{
	ESelectionModelClass *class;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	class = E_SELECTION_MODEL_GET_CLASS (model);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_selection_end != NULL);

	class->set_selection_end (model, row);
}

/**
 * e_selection_model_do_something
 * @model: #ESelectionModel to do something to.
 * @row: The row to do something in.
 * @col: The col to do something in.
 * @state: The state in which to do something.
 *
 * This routine does whatever is appropriate as if the user clicked
 * the mouse in the given row and column.
 */
void
e_selection_model_do_something (ESelectionModel *model,
                                guint row,
                                guint col,
                                GdkModifierType state)
{
	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;
	gint row_count;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	model->old_selection = -1;

	if (row == -1 && col != -1)
		row = 0;
	if (col == -1 && row != -1)
		col = 0;

	row_count = e_selection_model_row_count (model);
	if (row_count >= 0 && row < row_count) {
		switch (model->mode) {
		case GTK_SELECTION_SINGLE:
			e_selection_model_select_single_row (model, row);
			break;
		case GTK_SELECTION_BROWSE:
		case GTK_SELECTION_MULTIPLE:
			if (shift_p) {
				e_selection_model_set_selection_end (model, row);
			} else {
				if (ctrl_p) {
					e_selection_model_toggle_single_row (model, row);
				} else {
					e_selection_model_select_single_row (model, row);
				}
			}
			break;
		default:
			g_return_if_reached ();
			break;
		}
		e_selection_model_change_cursor (model, row, col);
		g_signal_emit (
			model,
			signals[CURSOR_CHANGED], 0,
			row, col);
		g_signal_emit (
			model,
			signals[CURSOR_ACTIVATED], 0,
			row, col);
	}
}

/**
 * e_selection_model_maybe_do_something
 * @model: #ESelectionModel to do something to.
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
e_selection_model_maybe_do_something (ESelectionModel *model,
                                      guint row,
                                      guint col,
                                      GdkModifierType state)
{
	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), FALSE);

	model->old_selection = -1;

	if (e_selection_model_is_row_selected (model, row)) {
		e_selection_model_change_cursor (model, row, col);
		g_signal_emit (
			model,
			signals[CURSOR_CHANGED], 0,
			row, col);
		return FALSE;
	} else {
		e_selection_model_do_something (model, row, col, state);
		return TRUE;
	}
}

void
e_selection_model_right_click_down (ESelectionModel *model,
                                    guint row,
                                    guint col,
                                    GdkModifierType state)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	if (model->mode == GTK_SELECTION_SINGLE) {
		model->old_selection =
			e_selection_model_cursor_row (model);
		e_selection_model_select_single_row (model, row);
	} else {
		e_selection_model_maybe_do_something (
			model, row, col, state);
	}
}

void
e_selection_model_right_click_up (ESelectionModel *model)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	if (model->mode != GTK_SELECTION_SINGLE)
		return;

	if (model->old_selection == -1)
		return;

	e_selection_model_select_single_row (
		model, model->old_selection);
}

void
e_selection_model_select_as_key_press (ESelectionModel *model,
                                       guint row,
                                       guint col,
                                       GdkModifierType state)
{
	gint cursor_activated = TRUE;

	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;

	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	model->old_selection = -1;

	switch (model->mode) {
	case GTK_SELECTION_BROWSE:
	case GTK_SELECTION_MULTIPLE:
		if (shift_p) {
			e_selection_model_set_selection_end (model, row);
		} else if (!ctrl_p) {
			e_selection_model_select_single_row (model, row);
		} else
			cursor_activated = FALSE;
		break;
	case GTK_SELECTION_SINGLE:
		e_selection_model_select_single_row (model, row);
		break;
	default:
		g_return_if_reached ();
		break;
	}
	if (row != -1) {
		e_selection_model_change_cursor (model, row, col);
		g_signal_emit (
			model,
			signals[CURSOR_CHANGED], 0,
			row, col);
		if (cursor_activated)
			g_signal_emit (
				model,
				signals[CURSOR_ACTIVATED], 0,
				row, col);
	}
}

static gint
move_selection (ESelectionModel *model,
                gboolean up,
                GdkModifierType state)
{
	gint row = e_selection_model_cursor_row (model);
	gint col = e_selection_model_cursor_col (model);
	gint row_count;

	/* there is no selected row when row is -1 */
	if (row != -1 && model->sorter != NULL)
		row = e_sorter_model_to_sorted (model->sorter, row);

	if (up)
		row--;
	else
		row++;
	if (row < 0)
		row = 0;
	row_count = e_selection_model_row_count (model);
	if (row >= row_count)
		row = row_count - 1;
	if (model->sorter != NULL)
		row = e_sorter_sorted_to_model (model->sorter, row);

	e_selection_model_select_as_key_press (model, row, col, state);
	return TRUE;
}

/**
 * e_selection_model_key_press
 * @model: #ESelectionModel to affect.
 * @key: The event.
 *
 * This routine does whatever is appropriate as if the user pressed
 * the given key.
 *
 * Returns: %TRUE if the #ESelectionModel used the key.
 */
gboolean
e_selection_model_key_press (ESelectionModel *model,
                             GdkEventKey *key)
{
	g_return_val_if_fail (E_IS_SELECTION_MODEL (model), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	model->old_selection = -1;

	switch (key->keyval) {
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
		return move_selection (model, TRUE, key->state);
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
		return move_selection (model, FALSE, key->state);
	case GDK_KEY_space:
	case GDK_KEY_KP_Space:
		if (model->mode != GTK_SELECTION_SINGLE) {
			gint row = e_selection_model_cursor_row (model);
			gint col = e_selection_model_cursor_col (model);
			if (row == -1)
				break;

			e_selection_model_toggle_single_row (model, row);
			g_signal_emit (
				model,
				signals[CURSOR_ACTIVATED], 0,
				row, col);
			return TRUE;
		}
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if (model->mode != GTK_SELECTION_SINGLE) {
			gint row = e_selection_model_cursor_row (model);
			gint col = e_selection_model_cursor_col (model);
			e_selection_model_select_single_row (model, row);
			g_signal_emit (
				model,
				signals[CURSOR_ACTIVATED], 0,
				row, col);
			return TRUE;
		}
		break;
	case GDK_KEY_Home:
	case GDK_KEY_KP_Home:
		if (model->cursor_mode == E_CURSOR_LINE) {
			gint row = 0;
			gint cursor_col = e_selection_model_cursor_col (model);

			if (model->sorter != NULL)
				row = e_sorter_sorted_to_model (
					model->sorter, row);
			e_selection_model_select_as_key_press (
				model, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	case GDK_KEY_End:
	case GDK_KEY_KP_End:
		if (model->cursor_mode == E_CURSOR_LINE) {
			gint row = e_selection_model_row_count (model) - 1;
			gint cursor_col = e_selection_model_cursor_col (model);

			if (model->sorter != NULL)
				row = e_sorter_sorted_to_model (
					model->sorter, row);
			e_selection_model_select_as_key_press (
				model, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void
e_selection_model_cursor_changed (ESelectionModel *model,
                                  gint row,
                                  gint col)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	g_signal_emit (model, signals[CURSOR_CHANGED], 0, row, col);
}

void
e_selection_model_cursor_activated (ESelectionModel *model,
                                    gint row,
                                    gint col)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	g_signal_emit (model, signals[CURSOR_ACTIVATED], 0, row, col);
}

void
e_selection_model_selection_changed (ESelectionModel *model)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	g_signal_emit (model, signals[SELECTION_CHANGED], 0);
}

void
e_selection_model_selection_row_changed (ESelectionModel *model,
                                         gint row)
{
	g_return_if_fail (E_IS_SELECTION_MODEL (model));

	g_signal_emit (model, signals[SELECTION_ROW_CHANGED], 0, row);
}

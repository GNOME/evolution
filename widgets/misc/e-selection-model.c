/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-selection-model.c
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include "e-selection-model.h"
#include "gal/util/e-util.h"

#define ESM_CLASS(e) ((ESelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *e_selection_model_parent_class;

enum {
	CURSOR_CHANGED,
	CURSOR_ACTIVATED,
	SELECTION_CHANGED,
	SELECTION_ROW_CHANGED,
	LAST_SIGNAL
};

static guint e_selection_model_signals [LAST_SIGNAL] = { 0, };

enum {
	ARG_0,
	ARG_SORTER,
	ARG_SELECTION_MODE,
	ARG_CURSOR_MODE,
};

inline static void
add_sorter(ESelectionModel *esm, ESorter *sorter)
{
	esm->sorter = sorter;
	if (sorter) {
		gtk_object_ref(GTK_OBJECT(sorter));
	}
}

inline static void
drop_sorter(ESelectionModel *esm)
{
	if (esm->sorter) {
		gtk_object_unref(GTK_OBJECT(esm->sorter));
	}
	esm->sorter = NULL;
}

static void
esm_destroy (GtkObject *object)
{
	ESelectionModel *esm;

	esm = E_SELECTION_MODEL (object);

	drop_sorter(esm);

	if (e_selection_model_parent_class->destroy)
		(* e_selection_model_parent_class->destroy) (object);
}

static void
esm_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModel *esm = E_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_SORTER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(esm->sorter);
		break;

	case ARG_SELECTION_MODE:
		GTK_VALUE_ENUM(*arg) = esm->mode;
		break;

	case ARG_CURSOR_MODE:
		GTK_VALUE_ENUM(*arg) = esm->cursor_mode;
		break;
	}
}

static void
esm_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModel *esm = E_SELECTION_MODEL (o);
	
	switch (arg_id){
	case ARG_SORTER:
		drop_sorter(esm);
		add_sorter(esm, GTK_VALUE_OBJECT (*arg) ? E_SORTER(GTK_VALUE_OBJECT (*arg)) : NULL);
		break;

	case ARG_SELECTION_MODE:
		esm->mode = GTK_VALUE_ENUM(*arg);
		if (esm->mode == GTK_SELECTION_SINGLE) {
			int cursor_row = e_selection_model_cursor_row(esm);
			int cursor_col = e_selection_model_cursor_col(esm);
			e_selection_model_do_something(esm, cursor_row, cursor_col, 0);
		}
		break;

	case ARG_CURSOR_MODE:
		esm->cursor_mode = GTK_VALUE_ENUM(*arg);
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
e_selection_model_class_init (ESelectionModelClass *klass)
{
	GtkObjectClass *object_class;

	e_selection_model_parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);

	object_class->destroy = esm_destroy;
	object_class->get_arg = esm_get_arg;
	object_class->set_arg = esm_set_arg;

	e_selection_model_signals [CURSOR_CHANGED] =
		gtk_signal_new ("cursor_changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ESelectionModelClass, cursor_changed),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_selection_model_signals [CURSOR_ACTIVATED] =
		gtk_signal_new ("cursor_activated",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ESelectionModelClass, cursor_activated),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_selection_model_signals [SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ESelectionModelClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_selection_model_signals [SELECTION_ROW_CHANGED] =
		gtk_signal_new ("selection_row_changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ESelectionModelClass, selection_row_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	klass->cursor_changed        = NULL;
	klass->cursor_activated      = NULL;
	klass->selection_changed     = NULL;
	klass->selection_row_changed = NULL;

	klass->is_row_selected       = NULL;
	klass->foreach               = NULL;
	klass->clear                 = NULL;
	klass->selected_count        = NULL;
	klass->select_all            = NULL;
	klass->invert_selection      = NULL;
	klass->row_count             = NULL;

	klass->change_one_row        = NULL;
	klass->change_cursor         = NULL;
	klass->cursor_row            = NULL;
	klass->cursor_col            = NULL;

	klass->select_single_row     = NULL;
	klass->toggle_single_row     = NULL;
	klass->move_selection_end    = NULL;
	klass->set_selection_end     = NULL;


	E_OBJECT_CLASS_ADD_SIGNALS (object_class, e_selection_model_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ESelectionModel::sorter", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_SORTER);
	gtk_object_add_arg_type ("ESelectionModel::selection_mode", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_SELECTION_MODE);
	gtk_object_add_arg_type ("ESelectionModel::cursor_mode", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_CURSOR_MODE);
}

E_MAKE_TYPE(e_selection_model, "ESelectionModel", ESelectionModel,
	    e_selection_model_class_init, e_selection_model_init, PARENT_TYPE);

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
				   gint                 n)
{
	if (ESM_CLASS(selection)->is_row_selected)
		return ESM_CLASS(selection)->is_row_selected (selection, n);
	else
		return FALSE;
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
e_selection_model_foreach     (ESelectionModel *selection,
			       EForeachFunc callback,
			       gpointer closure)
{
	if (ESM_CLASS(selection)->foreach)
		ESM_CLASS(selection)->foreach (selection, callback, closure);
}

/** 
 * e_selection_model_clear
 * @selection: #ESelectionModel to clear
 *
 * This routine clears the selection to no rows selected.
 */
void
e_selection_model_clear(ESelectionModel *selection)
{
	if (ESM_CLASS(selection)->clear)
		ESM_CLASS(selection)->clear (selection);
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
	if (ESM_CLASS(selection)->selected_count)
		return ESM_CLASS(selection)->selected_count (selection);
	else
		return 0;
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
	if (ESM_CLASS(selection)->select_all)
		ESM_CLASS(selection)->select_all (selection);
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
	if (ESM_CLASS(selection)->invert_selection)
		ESM_CLASS(selection)->invert_selection (selection);
}

int
e_selection_model_row_count (ESelectionModel *selection)
{
	if (ESM_CLASS(selection)->row_count)
		return ESM_CLASS(selection)->row_count (selection);
	else
		return 0;
}

void
e_selection_model_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	if (ESM_CLASS(selection)->change_one_row)
		ESM_CLASS(selection)->change_one_row (selection, row, grow);
}

void
e_selection_model_change_cursor (ESelectionModel *selection, int row, int col)
{
	if (ESM_CLASS(selection)->change_cursor)
		ESM_CLASS(selection)->change_cursor (selection, row, col);
}

int
e_selection_model_cursor_row (ESelectionModel *selection)
{
	if (ESM_CLASS(selection)->cursor_row)
		return ESM_CLASS(selection)->cursor_row (selection);
	else
		return -1;
}

int
e_selection_model_cursor_col (ESelectionModel *selection)
{
	if (ESM_CLASS(selection)->cursor_col)
		return ESM_CLASS(selection)->cursor_col (selection);
	else
		return -1;
}

void
e_selection_model_select_single_row (ESelectionModel *selection, int row)
{
	if (ESM_CLASS(selection)->select_single_row)
		ESM_CLASS(selection)->select_single_row (selection, row);
}

void
e_selection_model_toggle_single_row (ESelectionModel *selection, int row)
{
	if (ESM_CLASS(selection)->toggle_single_row)
		ESM_CLASS(selection)->toggle_single_row (selection, row);
}

void
e_selection_model_move_selection_end (ESelectionModel *selection, int row)
{
	if (ESM_CLASS(selection)->move_selection_end)
		ESM_CLASS(selection)->move_selection_end (selection, row);
}

void
e_selection_model_set_selection_end (ESelectionModel *selection, int row)
{
	if (ESM_CLASS(selection)->set_selection_end)
		ESM_CLASS(selection)->set_selection_end (selection, row);
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
				guint                 row,
				guint                 col,
				GdkModifierType       state)
{
	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;
	int row_count;

	selection->old_selection = -1;

	if (row == -1 && col != -1)
		row = 0;
	if (col == -1 && row != -1)
		col = 0;

	row_count = e_selection_model_row_count(selection);
	if (row_count >= 0 && row < row_count) {
		switch (selection->mode) {
		case GTK_SELECTION_SINGLE:
			e_selection_model_select_single_row (selection, row);
			break;
		case GTK_SELECTION_BROWSE:
		case GTK_SELECTION_MULTIPLE:
		case GTK_SELECTION_EXTENDED:
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
		}
		e_selection_model_change_cursor(selection, row, col);
		gtk_signal_emit(GTK_OBJECT(selection),
				e_selection_model_signals[CURSOR_CHANGED], row, col);
		gtk_signal_emit(GTK_OBJECT(selection),
				e_selection_model_signals[CURSOR_ACTIVATED], row, col);
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
e_selection_model_maybe_do_something      (ESelectionModel *selection,
					   guint            row,
					   guint            col,
					   GdkModifierType  state)
{
	selection->old_selection = -1;

	if (e_selection_model_is_row_selected(selection, row)) {
		e_selection_model_change_cursor(selection, row, col);
		gtk_signal_emit(GTK_OBJECT(selection),
				e_selection_model_signals[CURSOR_CHANGED], row, col);
		return FALSE;
	} else {
		e_selection_model_do_something(selection, row, col, state);
		return TRUE;
	}
}

void
e_selection_model_right_click_down (ESelectionModel *selection,
				    guint            row,
				    guint            col,
				    GdkModifierType  state)
{
	if (selection->mode == GTK_SELECTION_SINGLE) {
		selection->old_selection = e_selection_model_cursor_row (selection);
		e_selection_model_select_single_row (selection, row);
	} else {
		e_selection_model_maybe_do_something (selection, row, col, state);
	}
}

void
e_selection_model_right_click_up (ESelectionModel *selection)
{
	if (selection->mode == GTK_SELECTION_SINGLE && selection->old_selection != -1) {
		e_selection_model_select_single_row (selection, selection->old_selection);
	}
}

void
e_selection_model_select_as_key_press (ESelectionModel *selection,
				       guint            row,
				       guint            col,
				       GdkModifierType  state)
{
	int cursor_activated = TRUE;

	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;

	selection->old_selection = -1;

	switch (selection->mode) {
	case GTK_SELECTION_BROWSE:
	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
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
	}
	if (row != -1) {
		e_selection_model_change_cursor(selection, row, col);
		gtk_signal_emit(GTK_OBJECT(selection),
				e_selection_model_signals[CURSOR_CHANGED], row, col);
		if (cursor_activated)
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], row, col);
	}
}

static gint
move_selection (ESelectionModel *selection,
		gboolean              up,
		GdkModifierType       state)
{
	int row = e_selection_model_cursor_row(selection);
	int col = e_selection_model_cursor_col(selection);
	int row_count;

	row = e_sorter_model_to_sorted(selection->sorter, row);
	if (up)
		row--;
	else
		row++;
	if (row < 0)
		row = 0;
	row_count = e_selection_model_row_count(selection);
	if (row >= row_count)
		row = row_count - 1;
	row = e_sorter_sorted_to_model(selection->sorter, row);

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
gint
e_selection_model_key_press      (ESelectionModel *selection,
				  GdkEventKey          *key)
{
	selection->old_selection = -1;

	switch (key->keyval) {
	case GDK_Up:
	case GDK_KP_Up:
		return move_selection(selection, TRUE, key->state);
		break;
	case GDK_Down:
	case GDK_KP_Down:
		return move_selection(selection, FALSE, key->state);
		break;
	case GDK_space:
	case GDK_KP_Space:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			int row = e_selection_model_cursor_row(selection);
			int col = e_selection_model_cursor_col(selection);
			e_selection_model_toggle_single_row (selection, row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], row, col);
			return TRUE;
		}
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			int row = e_selection_model_cursor_row(selection);
			int col = e_selection_model_cursor_col(selection);
			e_selection_model_select_single_row (selection, row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], row, col);
			return TRUE;
		}
		break;
	case GDK_Home:
	case GDK_KP_Home:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			int row = 0;
			int cursor_col = e_selection_model_cursor_col(selection);

			row = e_sorter_sorted_to_model(selection->sorter, row);
			e_selection_model_select_as_key_press (selection, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	case GDK_End:
	case GDK_KP_End:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			int row = e_selection_model_row_count(selection) - 1;
			int cursor_col = e_selection_model_cursor_col(selection);

			row = e_sorter_sorted_to_model(selection->sorter, row);
			e_selection_model_select_as_key_press (selection, row, cursor_col, key->state);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void
e_selection_model_cursor_changed      (ESelectionModel *selection,
				       int              row,
				       int              col)
{
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals[CURSOR_CHANGED], row, col);
}

void
e_selection_model_cursor_activated    (ESelectionModel *selection,
				       int              row,
				       int              col)
{
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals[CURSOR_ACTIVATED], row, col);
}

void
e_selection_model_selection_changed   (ESelectionModel *selection)
{
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals[SELECTION_CHANGED]);
}

void
e_selection_model_selection_row_changed (ESelectionModel *selection,
					 int              row)
{
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals[SELECTION_ROW_CHANGED],
			row);
}

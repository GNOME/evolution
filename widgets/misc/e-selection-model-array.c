/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-selection-model-array.c
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

#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"

#include "e-selection-model-array.h"

#define PARENT_TYPE e_selection_model_get_type ()

static ESelectionModelClass *parent_class;

enum {
	PROP_0,
	PROP_CURSOR_ROW,
	PROP_CURSOR_COL
};

void
e_selection_model_array_confirm_row_count(ESelectionModelArray *esma)
{
	if (esma->eba == NULL) {
		int row_count = e_selection_model_array_get_row_count(esma);
		esma->eba = e_bit_array_new(row_count);
		esma->selected_row = -1;
		esma->selected_range_end = -1;
	}
}

/* FIXME: Should this deal with moving the selection if it's in single mode? */
void
e_selection_model_array_delete_rows(ESelectionModelArray *esma, int row, int count)
{
	if (esma->eba) {
		if (E_SELECTION_MODEL(esma)->mode == GTK_SELECTION_SINGLE)
			e_bit_array_delete_single_mode(esma->eba, row, count);
		else
			e_bit_array_delete(esma->eba, row, count);

		if (esma->cursor_row > row + count)
			esma->cursor_row -= count;
		else if (esma->cursor_row > row)
			esma->cursor_row = row;

		if (esma->cursor_row >= e_bit_array_bit_count (esma->eba)) {
			esma->cursor_row = e_bit_array_bit_count (esma->eba) - 1;
		} else if (esma->cursor_row < 0) {
			esma->cursor_row = -1;
		}
		if (esma->cursor_row >= 0) 
			e_bit_array_change_one_row(esma->eba, esma->cursor_row, TRUE);

		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
		e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), esma->cursor_row, esma->cursor_col);
	}
}

void
e_selection_model_array_insert_rows(ESelectionModelArray *esma, int row, int count)
{
	if (esma->eba) {
		e_bit_array_insert(esma->eba, row, count);

		if (esma->cursor_row >= row)
			esma->cursor_row += count;

		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
		e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), esma->cursor_row, esma->cursor_col);
	}
}

void
e_selection_model_array_move_row(ESelectionModelArray *esma, int old_row, int new_row)
{
	ESelectionModel *esm = E_SELECTION_MODEL(esma);

	if (esma->eba) {
		gboolean selected = e_bit_array_value_at(esma->eba, old_row);
		gboolean cursor = (esma->cursor_row == old_row);

		if (old_row < esma->cursor_row && esma->cursor_row < new_row)
			esma->cursor_row --;
		else if (new_row < esma->cursor_row && esma->cursor_row < old_row)
			esma->cursor_row ++;

		e_bit_array_move_row(esma->eba, old_row, new_row);

		if (selected) {
			if (esm->mode == GTK_SELECTION_SINGLE)
				e_bit_array_select_single_row (esma->eba, new_row);
			else
				e_bit_array_change_one_row(esma->eba, new_row, TRUE);
		}
		if (cursor) {
			esma->cursor_row = new_row;
		}
		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed(esm);
		e_selection_model_cursor_changed(esm, esma->cursor_row, esma->cursor_col);
	}
}

static void
esma_dispose (GObject *object)
{
	ESelectionModelArray *esma;

	esma = E_SELECTION_MODEL_ARRAY (object);

	if (esma->eba) {
		g_object_unref (esma->eba);
		esma->eba = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
esma_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (object);

	switch (prop_id){
	case PROP_CURSOR_ROW:
		g_value_set_int (value, esma->cursor_row);
		break;

	case PROP_CURSOR_COL:
		g_value_set_int (value, esma->cursor_col);
		break;
	}
}

static void
esma_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (object);

	switch (prop_id){
	case PROP_CURSOR_ROW:
		e_selection_model_do_something(esm, g_value_get_int (value), esma->cursor_col, 0);
		break;

	case PROP_CURSOR_COL:
		e_selection_model_do_something(esm, esma->cursor_row, g_value_get_int(value), 0);
		break;
	}
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
static gboolean
esma_is_row_selected (ESelectionModel *selection,
		      gint             n)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (esma->eba)
		return e_bit_array_value_at(esma->eba, n);
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
static void 
esma_foreach (ESelectionModel *selection,
	      EForeachFunc     callback,
	      gpointer         closure)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (esma->eba)
		e_bit_array_foreach(esma->eba, callback, closure);
}

/** 
 * e_selection_model_clear
 * @selection: #ESelectionModel to clear
 *
 * This routine clears the selection to no rows selected.
 */
static void
esma_clear(ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (esma->eba) {
		g_object_unref(esma->eba);
		esma->eba = NULL;
	}
	esma->cursor_row = -1;
	esma->cursor_col = -1;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), -1, -1);
}

#define PART(x,n) (((x) & (0x01010101 << n)) >> n)
#define SECTION(x, n) (((x) >> (n * 8)) & 0xff)

/** 
 * e_selection_model_selected_count
 * @selection: #ESelectionModel to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
static gint
esma_selected_count (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (esma->eba)
		return e_bit_array_selected_count(esma->eba);
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
static void
esma_select_all (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);

	e_bit_array_select_all(esma->eba);

	esma->cursor_col = 0;
	esma->cursor_row = 0;
	esma->selection_start_row = 0;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), 0, 0);
}

/** 
 * e_selection_model_invert_selection
 * @selection: #ESelectionModel to invert
 *
 * This routine inverts all the rows in the given
 * #ESelectionModel.
 */
static void
esma_invert_selection (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);

	e_bit_array_invert_selection(esma->eba);
	
	esma->cursor_col = -1;
	esma->cursor_row = -1;
	esma->selection_start_row = 0;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), -1, -1);
}

static int
esma_row_count (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	e_selection_model_array_confirm_row_count(esma);
	return e_bit_array_bit_count(esma->eba);
}

static void
esma_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	e_selection_model_array_confirm_row_count(esma);
	e_bit_array_change_one_row(esma->eba, row, grow);
}

static void
esma_change_cursor (ESelectionModel *selection, int row, int col)
{
	ESelectionModelArray *esma;

	g_return_if_fail(selection != NULL);
	g_return_if_fail(E_IS_SELECTION_MODEL(selection));

	esma = E_SELECTION_MODEL_ARRAY(selection);

	esma->cursor_row = row;
	esma->cursor_col = col;
}

static void
esma_change_range(ESelectionModel *selection, int start, int end, gboolean grow)
{
	int i;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (start != end) {
		if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
			for ( i = start; i < end; i++) {
				e_bit_array_change_one_row(esma->eba, e_sorter_sorted_to_model(selection->sorter, i), grow);
			}
		} else {
			e_selection_model_array_confirm_row_count(esma);
			e_bit_array_change_range(esma->eba, start, end, grow);
		}
	}
}

static int
esma_cursor_row (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	return esma->cursor_row;
}

static int
esma_cursor_col (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	return esma->cursor_col;
}

static void
esma_real_select_single_row (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);
	
	e_bit_array_select_single_row(esma->eba, row);

	esma->selection_start_row = row;
	esma->selected_row = row;
	esma->selected_range_end = row;
}

static void
esma_select_single_row (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	int selected_row = esma->selected_row;
	esma_real_select_single_row (selection, row);

	if (selected_row != -1 && esma->eba && selected_row < e_bit_array_bit_count (esma->eba)) {
		if (selected_row != row) {
			e_selection_model_selection_row_changed(selection, selected_row);
			e_selection_model_selection_row_changed(selection, row);
		}
	} else {
		e_selection_model_selection_changed(selection);
	}
}

static void
esma_toggle_single_row (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);
	e_bit_array_toggle_single_row(esma->eba, row);

	esma->selection_start_row = row;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_row_changed(E_SELECTION_MODEL(esma), row);
}

static void
esma_real_move_selection_end (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	int old_start;
	int old_end;
	int new_start;
	int new_end;
	if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
		old_start = MIN (e_sorter_model_to_sorted(selection->sorter, esma->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, esma->cursor_row));
		old_end = MAX (e_sorter_model_to_sorted(selection->sorter, esma->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, esma->cursor_row)) + 1;
		new_start = MIN (e_sorter_model_to_sorted(selection->sorter, esma->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, row));
		new_end = MAX (e_sorter_model_to_sorted(selection->sorter, esma->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, row)) + 1;
	} else {
		old_start = MIN (esma->selection_start_row, esma->cursor_row);
		old_end = MAX (esma->selection_start_row, esma->cursor_row) + 1;
		new_start = MIN (esma->selection_start_row, row);
		new_end = MAX (esma->selection_start_row, row) + 1;
	}
	/* This wouldn't work nearly so smoothly if one end of the selection weren't held in place. */
	if (old_start < new_start)
		esma_change_range(selection, old_start, new_start, FALSE);
	if (new_start < old_start)
		esma_change_range(selection, new_start, old_start, TRUE);
	if (old_end < new_end)
		esma_change_range(selection, old_end, new_end, TRUE);
	if (new_end < old_end)
		esma_change_range(selection, new_end, old_end, FALSE);
	esma->selected_row = -1;
	esma->selected_range_end = -1;
}

static void
esma_move_selection_end (ESelectionModel *selection, int row)
{
	esma_real_move_selection_end (selection, row);
	e_selection_model_selection_changed(selection);
}

static void
esma_set_selection_end (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	int selected_range_end = esma->selected_range_end;
	int view_row = e_sorter_model_to_sorted(selection->sorter, row);

	esma_real_select_single_row(selection, esma->selection_start_row);
	esma->cursor_row = esma->selection_start_row;
	esma_real_move_selection_end(selection, row);

	esma->selected_range_end = view_row;
	if (selected_range_end != -1 && view_row != -1) {
		if (selected_range_end == view_row - 1 ||
		    selected_range_end == view_row + 1) {
			e_selection_model_selection_row_changed(selection, selected_range_end);
			e_selection_model_selection_row_changed(selection, view_row);
		}
	}
	e_selection_model_selection_changed(selection);
}

int
e_selection_model_array_get_row_count (ESelectionModelArray *esma)
{
	g_return_val_if_fail(esma != NULL, 0);
	g_return_val_if_fail(E_IS_SELECTION_MODEL_ARRAY(esma), 0);

	if (E_SELECTION_MODEL_ARRAY_GET_CLASS(esma)->get_row_count)
		return E_SELECTION_MODEL_ARRAY_GET_CLASS(esma)->get_row_count (esma);
	else
		return 0;
}


static void
e_selection_model_array_init (ESelectionModelArray *esma)
{
	esma->eba = NULL;
	esma->selection_start_row = 0;
	esma->cursor_row = -1;
	esma->cursor_col = -1;

	esma->selected_row = -1;
	esma->selected_range_end = -1;
}

static void
e_selection_model_array_class_init (ESelectionModelArrayClass *klass)
{
	GObjectClass *object_class;
	ESelectionModelClass *esm_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class = G_OBJECT_CLASS(klass);
	esm_class = E_SELECTION_MODEL_CLASS(klass);

	object_class->dispose = esma_dispose;
	object_class->get_property = esma_get_property;
	object_class->set_property = esma_set_property;

	esm_class->is_row_selected    = esma_is_row_selected    ;
	esm_class->foreach            = esma_foreach            ;
	esm_class->clear              = esma_clear              ;
	esm_class->selected_count     = esma_selected_count     ;
	esm_class->select_all         = esma_select_all         ;
	esm_class->invert_selection   = esma_invert_selection   ;
	esm_class->row_count          = esma_row_count          ;

	esm_class->change_one_row     = esma_change_one_row     ;
	esm_class->change_cursor      = esma_change_cursor      ;
	esm_class->cursor_row         = esma_cursor_row         ;
	esm_class->cursor_col         = esma_cursor_col         ;

	esm_class->select_single_row  = esma_select_single_row  ;
	esm_class->toggle_single_row  = esma_toggle_single_row  ;
	esm_class->move_selection_end = esma_move_selection_end ;
	esm_class->set_selection_end  = esma_set_selection_end  ;

	klass->get_row_count          = NULL                    ;

	g_object_class_install_property (object_class, PROP_CURSOR_ROW, 
					 g_param_spec_int ("cursor_row",
							   _("Cursor Row"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CURSOR_COL, 
					 g_param_spec_int ("cursor_col",
							   _("Cursor Column"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));
}

E_MAKE_TYPE(e_selection_model_array, "ESelectionModelArray", ESelectionModelArray,
	    e_selection_model_array_class_init, e_selection_model_array_init, PARENT_TYPE)

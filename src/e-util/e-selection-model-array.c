/*
 *
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

#include <glib/gi18n.h>

#include "e-selection-model-array.h"

G_DEFINE_TYPE (
	ESelectionModelArray,
	e_selection_model_array,
	E_TYPE_SELECTION_MODEL)

enum {
	PROP_0,
	PROP_CURSOR_ROW,
	PROP_CURSOR_COL
};

void
e_selection_model_array_confirm_row_count (ESelectionModelArray *esma)
{
	if (esma->eba == NULL) {
		gint row_count = e_selection_model_array_get_row_count (esma);
		esma->eba = e_bit_array_new (row_count);
		esma->selected_row = -1;
		esma->selected_range_end = -1;
	}
}

static gint
es_row_model_to_sorted (ESelectionModelArray *esma,
                        gint model_row)
{
	if (model_row >= 0 && esma && esma->parent.sorter && e_sorter_needs_sorting (esma->parent.sorter))
		return e_sorter_model_to_sorted (esma->parent.sorter, model_row);

	return model_row;
}

static gint
es_row_sorted_to_model (ESelectionModelArray *esma,
                        gint sorted_row)
{
	if (sorted_row >= 0 && esma && esma->parent.sorter && e_sorter_needs_sorting (esma->parent.sorter))
		return e_sorter_sorted_to_model (esma->parent.sorter, sorted_row);

	return sorted_row;
}

/* FIXME: Should this deal with moving the selection if it's in single mode? */
void
e_selection_model_array_delete_rows (ESelectionModelArray *esma,
                                     gint row,
                                     gint count)
{
	if (esma->eba) {
		if (E_SELECTION_MODEL (esma)->mode == GTK_SELECTION_SINGLE)
			e_bit_array_delete_single_mode (esma->eba, row, count);
		else
			e_bit_array_delete (esma->eba, row, count);

		if (esma->cursor_row >= row && esma->cursor_row < row + count) {
			/* we should move the cursor_row, because some lines before us are going to be removed */
			if (esma->cursor_row_sorted >= e_bit_array_bit_count (esma->eba)) {
				esma->cursor_row_sorted = e_bit_array_bit_count (esma->eba) - 1;
			}

			if (esma->cursor_row_sorted >= 0) {
				esma->cursor_row = es_row_sorted_to_model (esma, esma->cursor_row_sorted);
				esma->selection_start_row = 0;
				e_bit_array_change_one_row (esma->eba, esma->cursor_row, TRUE);
			} else {
				esma->cursor_row = -1;
				esma->cursor_row_sorted = -1;
				esma->selection_start_row = 0;
			}
		} else {
			/* some code earlier changed the selected row, so just update the sorted one */
			if (esma->cursor_row >= row)
				esma->cursor_row = MAX (0, esma->cursor_row - count);

			if (esma->cursor_row >= e_bit_array_bit_count (esma->eba))
				esma->cursor_row = e_bit_array_bit_count (esma->eba) - 1;

			if (esma->cursor_row >= 0) {
				esma->cursor_row_sorted = es_row_model_to_sorted (esma, esma->cursor_row);
				esma->selection_start_row = 0;
				e_bit_array_change_one_row (esma->eba, esma->cursor_row, TRUE);
			} else {
				esma->cursor_row = -1;
				esma->cursor_row_sorted = -1;
				esma->selection_start_row = 0;
			}
		}

		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed (E_SELECTION_MODEL (esma));
		e_selection_model_cursor_changed (E_SELECTION_MODEL (esma), esma->cursor_row, esma->cursor_col);
	}
}

void
e_selection_model_array_insert_rows (ESelectionModelArray *esma,
                                     gint row,
                                     gint count)
{
	if (esma->eba) {
		e_bit_array_insert (esma->eba, row, count);

		/* just recalculate new position of the previously set cursor row */
		esma->cursor_row = es_row_sorted_to_model (esma, esma->cursor_row_sorted);

		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed (E_SELECTION_MODEL (esma));
		e_selection_model_cursor_changed (E_SELECTION_MODEL (esma), esma->cursor_row, esma->cursor_col);
	}
}

void
e_selection_model_array_move_row (ESelectionModelArray *esma,
                                  gint old_row,
                                  gint new_row)
{
	ESelectionModel *esm = E_SELECTION_MODEL (esma);

	if (esma->eba) {
		gboolean selected = e_bit_array_value_at (esma->eba, old_row);
		gboolean cursor = (esma->cursor_row == old_row);
		gint old_row_sorted, new_row_sorted;

		old_row_sorted = es_row_model_to_sorted (esma, old_row);
		new_row_sorted = es_row_model_to_sorted (esma, new_row);

		if (old_row_sorted < esma->cursor_row_sorted && esma->cursor_row_sorted < new_row_sorted)
			esma->cursor_row_sorted--;
		else if (new_row_sorted < esma->cursor_row_sorted && esma->cursor_row_sorted < old_row_sorted)
			esma->cursor_row_sorted++;

		e_bit_array_move_row (esma->eba, old_row, new_row);

		if (selected) {
			if (esm->mode == GTK_SELECTION_SINGLE)
				e_bit_array_select_single_row (esma->eba, new_row);
			else
				e_bit_array_change_one_row (esma->eba, new_row, TRUE);
		}
		if (cursor) {
			esma->cursor_row = new_row;
			esma->cursor_row_sorted = es_row_model_to_sorted (esma, esma->cursor_row);
		} else
			esma->cursor_row = es_row_sorted_to_model (esma, esma->cursor_row_sorted);

		esma->selected_row = -1;
		esma->selected_range_end = -1;
		e_selection_model_selection_changed (esm);
		e_selection_model_cursor_changed (esm, esma->cursor_row, esma->cursor_col);
	}
}

static void
esma_dispose (GObject *object)
{
	ESelectionModelArray *esma;

	esma = E_SELECTION_MODEL_ARRAY (object);
	g_clear_object (&esma->eba);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_selection_model_array_parent_class)->dispose (object);
}

static void
esma_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (object);

	switch (property_id) {
	case PROP_CURSOR_ROW:
		g_value_set_int (value, esma->cursor_row);
		break;

	case PROP_CURSOR_COL:
		g_value_set_int (value, esma->cursor_col);
		break;
	}
}

static void
esma_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (object);

	switch (property_id) {
	case PROP_CURSOR_ROW:
		e_selection_model_do_something (esm, g_value_get_int (value), esma->cursor_col, 0);
		break;

	case PROP_CURSOR_COL:
		e_selection_model_do_something (esm, esma->cursor_row, g_value_get_int (value), 0);
		break;
	}
}

static gboolean
esma_is_row_selected (ESelectionModel *selection,
                      gint n)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	if (esma->eba)
		return e_bit_array_value_at (esma->eba, n);
	else
		return FALSE;
}

static void
esma_foreach (ESelectionModel *selection,
              EForeachFunc callback,
              gpointer closure)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	if (esma->eba)
		e_bit_array_foreach (esma->eba, callback, closure);
}

static void
esma_clear (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	g_clear_object (&esma->eba);
	esma->cursor_row = -1;
	esma->cursor_col = -1;
	esma->cursor_row_sorted = -1;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_changed (E_SELECTION_MODEL (esma));
	e_selection_model_cursor_changed (E_SELECTION_MODEL (esma), -1, -1);
}

#define PART(x,n) (((x) & (0x01010101 << n)) >> n)
#define SECTION(x, n) (((x) >> (n * 8)) & 0xff)

static gint
esma_selected_count (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	if (esma->eba)
		return e_bit_array_selected_count (esma->eba);
	else
		return 0;
}

static void
esma_select_all (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);

	e_selection_model_array_confirm_row_count (esma);

	e_bit_array_select_all (esma->eba);

	esma->cursor_col = 0;
	esma->cursor_row_sorted = 0;
	esma->cursor_row = es_row_sorted_to_model (esma, esma->cursor_row_sorted);
	esma->selection_start_row = esma->cursor_row;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_changed (E_SELECTION_MODEL (esma));
	e_selection_model_cursor_changed (E_SELECTION_MODEL (esma), 0, 0);
}

static gint
esma_row_count (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	e_selection_model_array_confirm_row_count (esma);
	return e_bit_array_bit_count (esma->eba);
}

static void
esma_change_one_row (ESelectionModel *selection,
                     gint row,
                     gboolean grow)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	e_selection_model_array_confirm_row_count (esma);
	e_bit_array_change_one_row (esma->eba, row, grow);
}

static void
esma_change_cursor (ESelectionModel *selection,
                    gint row,
                    gint col)
{
	ESelectionModelArray *esma;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	esma = E_SELECTION_MODEL_ARRAY (selection);

	esma->cursor_row = row;
	esma->cursor_col = col;
	esma->cursor_row_sorted = es_row_model_to_sorted (esma, esma->cursor_row);
}

static void
esma_change_range (ESelectionModel *selection,
                   gint start,
                   gint end,
                   gboolean grow)
{
	gint i;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	if (start != end) {
		if (selection->sorter && e_sorter_needs_sorting (selection->sorter)) {
			for (i = start; i < end; i++) {
				e_bit_array_change_one_row (esma->eba, e_sorter_sorted_to_model (selection->sorter, i), grow);
			}
		} else {
			e_selection_model_array_confirm_row_count (esma);
			e_bit_array_change_range (esma->eba, start, end, grow);
		}
	}
}

static gint
esma_cursor_row (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	return esma->cursor_row;
}

static gint
esma_cursor_col (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	return esma->cursor_col;
}

static void
esma_real_select_single_row (ESelectionModel *selection,
                             gint row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);

	e_selection_model_array_confirm_row_count (esma);

	e_bit_array_select_single_row (esma->eba, row);

	esma->selection_start_row = row;
	esma->selected_row = row;
	esma->selected_range_end = row;
}

static void
esma_select_single_row (ESelectionModel *selection,
                        gint row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	gint selected_row = esma->selected_row;
	esma_real_select_single_row (selection, row);

	if (selected_row != -1 && esma->eba && selected_row < e_bit_array_bit_count (esma->eba)) {
		if (selected_row != row) {
			e_selection_model_selection_row_changed (selection, selected_row);
			e_selection_model_selection_row_changed (selection, row);
		}
	} else {
		e_selection_model_selection_changed (selection);
	}
}

static void
esma_toggle_single_row (ESelectionModel *selection,
                        gint row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);

	e_selection_model_array_confirm_row_count (esma);
	e_bit_array_toggle_single_row (esma->eba, row);

	esma->selection_start_row = row;
	esma->selected_row = -1;
	esma->selected_range_end = -1;
	e_selection_model_selection_row_changed (E_SELECTION_MODEL (esma), row);
}

static void
esma_real_move_selection_end (ESelectionModel *selection,
                              gint row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	gint old_start;
	gint old_end;
	gint new_start;
	gint new_end;
	if (selection->sorter && e_sorter_needs_sorting (selection->sorter)) {
		old_start = MIN (
			e_sorter_model_to_sorted (selection->sorter, esma->selection_start_row),
			e_sorter_model_to_sorted (selection->sorter, esma->cursor_row));
		old_end = MAX (
			e_sorter_model_to_sorted (selection->sorter, esma->selection_start_row),
			e_sorter_model_to_sorted (selection->sorter, esma->cursor_row)) + 1;
		new_start = MIN (
			e_sorter_model_to_sorted (selection->sorter, esma->selection_start_row),
			e_sorter_model_to_sorted (selection->sorter, row));
		new_end = MAX (
			e_sorter_model_to_sorted (selection->sorter, esma->selection_start_row),
			e_sorter_model_to_sorted (selection->sorter, row)) + 1;
	} else {
		old_start = MIN (esma->selection_start_row, esma->cursor_row);
		old_end = MAX (esma->selection_start_row, esma->cursor_row) + 1;
		new_start = MIN (esma->selection_start_row, row);
		new_end = MAX (esma->selection_start_row, row) + 1;
	}
	/* This wouldn't work nearly so smoothly if one end of the selection weren't held in place. */
	if (old_start < new_start)
		esma_change_range (selection, old_start, new_start, FALSE);
	if (new_start < old_start)
		esma_change_range (selection, new_start, old_start, TRUE);
	if (old_end < new_end)
		esma_change_range (selection, old_end, new_end, TRUE);
	if (new_end < old_end)
		esma_change_range (selection, new_end, old_end, FALSE);
	esma->selected_row = -1;
	esma->selected_range_end = -1;
}

static void
esma_move_selection_end (ESelectionModel *selection,
                         gint row)
{
	esma_real_move_selection_end (selection, row);
	e_selection_model_selection_changed (selection);
}

static void
esma_set_selection_end (ESelectionModel *selection,
                        gint row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (selection);
	gint selected_range_end = esma->selected_range_end;
	gint view_row = e_sorter_model_to_sorted (selection->sorter, row);

	esma_real_select_single_row (selection, esma->selection_start_row);
	esma->cursor_row = esma->selection_start_row;
	esma->cursor_row_sorted = es_row_model_to_sorted (esma, esma->cursor_row);
	esma_real_move_selection_end (selection, row);

	esma->selected_range_end = view_row;
	if (selected_range_end != -1 && view_row != -1) {
		if (selected_range_end == view_row - 1 ||
		    selected_range_end == view_row + 1) {
			e_selection_model_selection_row_changed (selection, selected_range_end);
			e_selection_model_selection_row_changed (selection, view_row);
		}
	}
	e_selection_model_selection_changed (selection);
}

gint
e_selection_model_array_get_row_count (ESelectionModelArray *esma)
{
	ESelectionModelArrayClass *klass;

	g_return_val_if_fail (esma != NULL, 0);
	g_return_val_if_fail (E_IS_SELECTION_MODEL_ARRAY (esma), 0);

	klass = E_SELECTION_MODEL_ARRAY_GET_CLASS (esma);
	g_return_val_if_fail (klass != NULL, 0);

	if (klass->get_row_count)
		return klass->get_row_count (esma);
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
	esma->cursor_row_sorted = -1;

	esma->selected_row = -1;
	esma->selected_range_end = -1;
}

static void
e_selection_model_array_class_init (ESelectionModelArrayClass *class)
{
	GObjectClass *object_class;
	ESelectionModelClass *esm_class;

	object_class = G_OBJECT_CLASS (class);
	esm_class = E_SELECTION_MODEL_CLASS (class);

	object_class->dispose = esma_dispose;
	object_class->get_property = esma_get_property;
	object_class->set_property = esma_set_property;

	esm_class->is_row_selected = esma_is_row_selected;
	esm_class->foreach = esma_foreach;
	esm_class->clear = esma_clear;
	esm_class->selected_count = esma_selected_count;
	esm_class->select_all = esma_select_all;
	esm_class->row_count = esma_row_count;

	esm_class->change_one_row = esma_change_one_row;
	esm_class->change_cursor = esma_change_cursor;
	esm_class->cursor_row = esma_cursor_row;
	esm_class->cursor_col = esma_cursor_col;

	esm_class->select_single_row = esma_select_single_row;
	esm_class->toggle_single_row = esma_toggle_single_row;
	esm_class->move_selection_end = esma_move_selection_end;
	esm_class->set_selection_end = esma_set_selection_end;

	class->get_row_count = NULL;

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_ROW,
		g_param_spec_int (
			"cursor_row",
			"Cursor Row",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_COL,
		g_param_spec_int (
			"cursor_col",
			"Cursor Column",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));
}


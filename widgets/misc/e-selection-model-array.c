/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-selection-model-array.c: a Selection Model
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-selection-model-array.h"
#include "gal/util/e-util.h"
#include <gdk/gdkkeysyms.h>

#define ESMA_CLASS(e) ((ESelectionModelArrayClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE e_selection_model_get_type ()

#define ONES ((guint32) 0xffffffff)

#define BOX(n) ((n) / 32)
#define OFFSET(n) (31 - ((n) % 32))
#define BITMASK(n) ((guint32)(((guint32) 0x1) << OFFSET((n))))
#define BITMASK_LEFT(n) ((((n) % 32) == 0) ? 0 : (ONES << (32 - ((n) % 32))))
#define BITMASK_RIGHT(n) ((guint32)(((guint32) ONES) >> ((n) % 32)))

static ESelectionModelClass *parent_class;

enum {
	ARG_0,
	ARG_CURSOR_ROW,
	ARG_CURSOR_COL,
};

void
e_selection_model_array_confirm_row_count(ESelectionModelArray *esma)
{
	if (esma->row_count < 0) {
		esma->row_count = e_selection_model_array_get_row_count(esma);
		g_free(esma->selection);
		esma->selection = g_new0(gint, (esma->row_count + 31) / 32);
	}
}

static void
e_selection_model_array_insert_row_real(ESelectionModelArray *esma, int row)
{
	int box;
	int i;
	if(esma->row_count >= 0) {
		/* Add another word if needed. */
		if ((esma->row_count & 0x1f) == 0) {
			esma->selection = g_renew(gint, esma->selection, (esma->row_count >> 5) + 1);
			esma->selection[esma->row_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = BOX(row);
		/* Shift all words to the right of our box right one bit. */
		for (i = esma->row_count >> 5; i > box; i--) {
			esma->selection[i] = (esma->selection[i] >> 1) | (esma->selection[i - 1] << 31);
		}

		/* Shift right half of box one bit to the right. */
		esma->selection[box] = (esma->selection[box] & BITMASK_LEFT(row)) | ((esma->selection[box] & BITMASK_RIGHT(row)) >> 1);
		esma->row_count ++;
	}
	if (esma->cursor_row >= row)
		esma->cursor_row ++;
}

static void
e_selection_model_array_delete_row_real(ESelectionModelArray *esma, int row)
{
	int box;
	int i;
	int last;
	int selected = FALSE;
	if(esma->row_count >= 0) {
		guint32 bitmask;
		box = row >> 5;
		last = esma->row_count >> 5;

		/* Build bitmasks for the left and right half of the box */
		bitmask = BITMASK_RIGHT(row) >> 1;
		selected = e_selection_model_is_row_selected(E_SELECTION_MODEL(esma), row);
		/* Shift right half of box one bit to the left. */
		esma->selection[box] = (esma->selection[box] & BITMASK_LEFT(row))| ((esma->selection[box] & bitmask) << 1);

		/* Shift all words to the right of our box left one bit. */
		if (box < last) {
			esma->selection[box] &= esma->selection[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				esma->selection[i] = (esma->selection[i] << 1) | (esma->selection[i + 1] >> 31);
			}
			/* this over-runs our memory! */
			/*esma->selection[i] = esma->selection[i] << 1; */
		}
		esma->row_count --;
		/* Remove the last word if not needed. */
		if ((esma->row_count & 0x1f) == 0) {
			esma->selection = g_renew(gint, esma->selection, esma->row_count >> 5);
		}
		if (selected && E_SELECTION_MODEL(esma)->mode == GTK_SELECTION_SINGLE) {
			e_selection_model_select_single_row (E_SELECTION_MODEL(esma), row > 0 ? row - 1 : 0);
		}
	}
	if (esma->cursor_row >= row && esma->cursor_row > 0)
		esma->cursor_row --;
}

/* FIXME : Improve efficiency here. */
void
e_selection_model_array_delete_rows(ESelectionModelArray *esma, int row, int count)
{
	int i;
	for (i = 0; i < count; i++)
		e_selection_model_array_delete_row_real(esma, row);
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), esma->cursor_row, esma->cursor_col);
}

/* FIXME : Improve efficiency here. */
void
e_selection_model_array_insert_rows(ESelectionModelArray *esma, int row, int count)
{
	int i;
	for (i = 0; i < count; i++)
		e_selection_model_array_insert_row_real(esma, row);
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), esma->cursor_row, esma->cursor_col);
}

/* FIXME: Implement this more efficiently. */
void
e_selection_model_array_move_row(ESelectionModelArray *esma, int old_row, int new_row)
{
	ESelectionModel *esm = E_SELECTION_MODEL(esma);
	gint selected = e_selection_model_is_row_selected(esm, old_row);
	gint cursor = esma->cursor_row == old_row;

	e_selection_model_array_delete_row_real(esma, old_row);
	e_selection_model_array_insert_row_real(esma, new_row);

	if (selected) {
		if (esm->mode == GTK_SELECTION_SINGLE)
			e_selection_model_select_single_row (esm, new_row);
		else
			e_selection_model_change_one_row(esm, new_row, TRUE);
	}
	if (cursor) {
		esma->cursor_row = new_row;
	}
	e_selection_model_selection_changed(esm);
	e_selection_model_cursor_changed(esm, esma->cursor_row, esma->cursor_col);
}

static void
esma_destroy (GtkObject *object)
{
	ESelectionModelArray *esma;

	esma = E_SELECTION_MODEL_ARRAY (object);

	g_free(esma->selection);
}

static void
esma_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (o);

	switch (arg_id){
	case ARG_CURSOR_ROW:
		GTK_VALUE_INT(*arg) = esma->cursor_row;
		break;

	case ARG_CURSOR_COL:
		GTK_VALUE_INT(*arg) = esma->cursor_col;
		break;
	}
}

static void
esma_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModel *esm = E_SELECTION_MODEL (o);
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY (o);

	switch (arg_id){
	case ARG_CURSOR_ROW:
		e_selection_model_do_something(esm, GTK_VALUE_INT(*arg), esma->cursor_col, 0);
		break;

	case ARG_CURSOR_COL:
		e_selection_model_do_something(esm, esma->cursor_row, GTK_VALUE_INT(*arg), 0);
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
	if (esma->row_count < n || esma->row_count == 0)
		return 0;
	else
		return (esma->selection[BOX(n)] >> OFFSET(n)) & 0x1;
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
	int i;
	int last = (esma->row_count + 31) / 32;
	for (i = 0; i < last; i++) {
		if (esma->selection[i]) {
			int j;
			guint32 value = esma->selection[i];
			for (j = 0; j < 32; j++) {
				if (value & 0x80000000) {
					callback(i * 32 + j, closure);
				}
				value <<= 1;
			}
		}
	}
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
	g_free(esma->selection);
	esma->selection = NULL;
	esma->row_count = -1;
	esma->cursor_row = -1;
	esma->cursor_col = -1;
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
	gint count;
	int i;
	int last;

	if (!esma->selection)
		return 0;

	count = 0;

	last = BOX(esma->row_count - 1);

	for (i = 0; i <= last; i++) {
		int j;
		guint32 thiscount = 0;
		for (j = 0; j < 8; j++)
			thiscount += PART(esma->selection[i], j);
		for (j = 0; j < 4; j++)
			count += SECTION(thiscount, j);
	}

	return count;
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
	int i;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);
	
	if (!esma->selection)
		esma->selection = g_new0 (gint, (esma->row_count + 31) / 32);
	
	for (i = 0; i < (esma->row_count + 31) / 32; i ++) {
		esma->selection[i] = ONES;
	}

	/* need to zero out the bits corresponding to the rows not
	   selected in the last full 32 bit mask */
	if (esma->row_count % 32) {
		int unselected_mask = 0;
		int num_unselected_in_last_byte = 32 - esma->row_count % 32;

		for (i = 0; i < num_unselected_in_last_byte; i ++)
			unselected_mask |= 1 << i;

		esma->selection[(esma->row_count + 31) / 32 - 1] &= ~unselected_mask;
	}

	esma->cursor_col = 0;
	esma->cursor_row = 0;
	esma->selection_start_row = 0;
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
	int i;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);

	e_selection_model_array_confirm_row_count(esma);
	
	if (!esma->selection)
		esma->selection = g_new0 (gint, (esma->row_count + 31) / 32);
	
	for (i = 0; i < (esma->row_count + 31) / 32; i ++) {
		esma->selection[i] = ~esma->selection[i];
	}
	
	esma->cursor_col = -1;
	esma->cursor_row = -1;
	esma->selection_start_row = 0;
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(esma), -1, -1);
}

static int
esma_row_count (ESelectionModel *selection)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	e_selection_model_array_confirm_row_count(esma);
	return esma->row_count;
}

#define OPERATE(object, i,mask,grow) ((grow) ? (((object)->selection[(i)]) |= ((guint32) ~(mask))) : (((object)->selection[(i)]) &= (mask)))

static void
esma_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	int i;
	i = BOX(row);

	OPERATE(E_SELECTION_MODEL_ARRAY(selection), i, ~BITMASK(row), grow);
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
	int i, last;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (start != end) {
		if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
			for ( i = start; i < end; i++) {
				e_selection_model_change_one_row(selection, e_sorter_sorted_to_model(selection->sorter, i), grow);
			}
		} else {
			i = BOX(start);
			last = BOX(end);

			if (i == last) {
				OPERATE(esma, i, BITMASK_LEFT(start) | BITMASK_RIGHT(end), grow);
			} else {
				OPERATE(esma, i, BITMASK_LEFT(start), grow);
				if (grow)
					for (i ++; i < last; i++)
						esma->selection[i] = ONES;
				else
					for (i ++; i < last; i++)
						esma->selection[i] = 0;
				OPERATE(esma, i, BITMASK_RIGHT(end), grow);
			}
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
esma_select_single_row (ESelectionModel *selection, int row)
{
	int i;
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	for (i = 0; i < ((esma->row_count + 31) / 32); i++) {
		if (!((i == BOX(row) && esma->selection[i] == BITMASK(row)) ||
		      (i != BOX(row) && esma->selection[i] == 0))) {
			g_free(esma->selection);
			esma->selection = g_new0(gint, (esma->row_count + 31) / 32);
			esma->selection[BOX(row)] = BITMASK(row);

			e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
			break;
		}
	}

	esma->selection_start_row = row;
}

static void
esma_toggle_single_row (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	if (esma->selection[BOX(row)] & BITMASK(row))
		esma->selection[BOX(row)] &= ~BITMASK(row);
	else
		esma->selection[BOX(row)] |= BITMASK(row);
	esma->selection_start_row = row;
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
}

static void
esma_move_selection_end (ESelectionModel *selection, int row)
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
	e_selection_model_selection_changed(E_SELECTION_MODEL(esma));
}

static void
esma_set_selection_end (ESelectionModel *selection, int row)
{
	ESelectionModelArray *esma = E_SELECTION_MODEL_ARRAY(selection);
	esma_select_single_row(selection, esma->selection_start_row);
	esma->cursor_row = esma->selection_start_row;
	e_selection_model_move_selection_end(selection, row);
}

int
e_selection_model_array_get_row_count (ESelectionModelArray *esma)
{
	g_return_val_if_fail(esma != NULL, 0);
	g_return_val_if_fail(E_IS_SELECTION_MODEL_ARRAY(esma), 0);

	if (ESMA_CLASS(esma)->get_row_count)
		return ESMA_CLASS(esma)->get_row_count (esma);
	else
		return 0;
}


static void
e_selection_model_array_init (ESelectionModelArray *esma)
{
	esma->selection = NULL;
	esma->row_count = -1;
	esma->selection_start_row = 0;
	esma->cursor_row = -1;
	esma->cursor_col = -1;
}

static void
e_selection_model_array_class_init (ESelectionModelArrayClass *klass)
{
	GtkObjectClass *object_class;
	ESelectionModelClass *esm_class;

	parent_class = gtk_type_class (e_selection_model_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);
	esm_class = E_SELECTION_MODEL_CLASS(klass);

	object_class->destroy = esma_destroy;
	object_class->get_arg = esma_get_arg;
	object_class->set_arg = esma_set_arg;

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

	gtk_object_add_arg_type ("ESelectionModelArray::cursor_row", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_ROW);
	gtk_object_add_arg_type ("ESelectionModelArray::cursor_col", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_COL);
}

E_MAKE_TYPE(e_selection_model_array, "ESelectionModelArray", ESelectionModelArray,
	    e_selection_model_array_class_init, e_selection_model_array_init, PARENT_TYPE);

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-selection-model.c: a Selection Model
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-selection-model.h"
#include "gal/util/e-util.h"
#include <gdk/gdkkeysyms.h>

#define ESM_CLASS(e) ((ESelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

#define ONES ((guint32) 0xffffffff)

#define BOX(n) ((n) / 32)
#define OFFSET(n) (31 - ((n) % 32))
#define BITMASK(n) ((guint32)(((guint32) 0x1) << OFFSET((n))))
#define BITMASK_LEFT(n) ((((n) % 32) == 0) ? 0 : (ONES << (32 - ((n) % 32))))
#define BITMASK_RIGHT(n) ((guint32)(((guint32) ONES) >> ((n) % 32)))

static GtkObjectClass *e_selection_model_parent_class;

static void esm_select_single_row (ESelectionModel *selection, int row);

enum {
	CURSOR_CHANGED,
	CURSOR_ACTIVATED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint e_selection_model_signals [LAST_SIGNAL] = { 0, };

enum {
	ARG_0,
	ARG_SORTER,
	ARG_CURSOR_ROW,
	ARG_CURSOR_COL,
	ARG_SELECTION_MODE,
	ARG_CURSOR_MODE,
};

gboolean
e_selection_model_confirm_row_count(ESelectionModel *esm)
{
	if (esm->row_count < 0) {
		esm->row_count = e_selection_model_get_row_count(esm);
		if (esm->row_count < 0)
			return FALSE;
		g_free(esm->selection);
		esm->selection = g_new0(gint, (esm->row_count + 31) / 32);
	}
	return TRUE;
}

static void
e_selection_model_insert_row_real(ESelectionModel *esm, int row)
{
	int box;
	int i;
	if(esm->row_count >= 0) {
		/* Add another word if needed. */
		if ((esm->row_count & 0x1f) == 0) {
			esm->selection = g_renew(gint, esm->selection, (esm->row_count >> 5) + 1);
			esm->selection[esm->row_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = BOX(row);
		/* Shift all words to the right of our box right one bit. */
		for (i = esm->row_count >> 5; i > box; i--) {
			esm->selection[i] = (esm->selection[i] >> 1) | (esm->selection[i - 1] << 31);
		}

		/* Shift right half of box one bit to the right. */
		esm->selection[box] = (esm->selection[box] & BITMASK_LEFT(row)) | ((esm->selection[box] & BITMASK_RIGHT(row)) >> 1);
		esm->row_count ++;
	}
	if (esm->cursor_row >= row)
		esm->cursor_row ++;
}

static void
e_selection_model_delete_row_real(ESelectionModel *esm, int row)
{
	int box;
	int i;
	int last;
	int selected = FALSE;
	if(esm->row_count >= 0) {
		guint32 bitmask;
		box = row >> 5;
		last = esm->row_count >> 5;

		/* Build bitmasks for the left and right half of the box */
		bitmask = BITMASK_RIGHT(row) >> 1;
		selected = e_selection_model_is_row_selected(esm, row);
		/* Shift right half of box one bit to the left. */
		esm->selection[box] = (esm->selection[box] & BITMASK_LEFT(row))| ((esm->selection[box] & bitmask) << 1);

		/* Shift all words to the right of our box left one bit. */
		if (box < last) {
			esm->selection[box] &= esm->selection[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				esm->selection[i] = (esm->selection[i] << 1) | (esm->selection[i + 1] >> 31);
			}
			/* this over-runs our memory! */
			/*esm->selection[i] = esm->selection[i] << 1; */
		}
		esm->row_count --;
		/* Remove the last word if not needed. */
		if ((esm->row_count & 0x1f) == 0) {
			esm->selection = g_renew(gint, esm->selection, esm->row_count >> 5);
		}
		if (selected && esm->mode == GTK_SELECTION_SINGLE) {
			esm_select_single_row (esm, row > 0 ? row - 1 : 0);
		}
	}
	if (esm->cursor_row >= row && esm->cursor_row > 0)
		esm->cursor_row --;
}

/* FIXME : Improve efficiency here. */
void
e_selection_model_delete_rows(ESelectionModel *esm, int row, int count)
{
	int i;
	for (i = 0; i < count; i++)
		e_selection_model_delete_row_real(esm, row);
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [SELECTION_CHANGED]);
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [CURSOR_CHANGED], esm->cursor_row, esm->cursor_col);
}

/* FIXME : Improve efficiency here. */
void
e_selection_model_insert_rows(ESelectionModel *esm, int row, int count)
{
	int i;
	for (i = 0; i < count; i++)
		e_selection_model_insert_row_real(esm, row);
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [SELECTION_CHANGED]);
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [CURSOR_CHANGED], esm->cursor_row, esm->cursor_col);
}

/* FIXME: Implement this more efficiently. */
void
e_selection_model_move_row(ESelectionModel *esm, int old_row, int new_row)
{
	gint selected = e_selection_model_is_row_selected(esm, old_row);
	gint cursor = esm->cursor_row == old_row;

	e_selection_model_delete_row_real(esm, old_row);
	e_selection_model_insert_row_real(esm, new_row);

	if (selected) {
		if (esm->mode == GTK_SELECTION_SINGLE)
			esm_select_single_row (esm, new_row);
		else
			e_selection_model_change_one_row(esm, new_row, TRUE);
	}
	if (cursor) {
		esm->cursor_row = new_row;
	}
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [SELECTION_CHANGED]);
	gtk_signal_emit(GTK_OBJECT(esm),
			e_selection_model_signals [CURSOR_CHANGED], esm->cursor_row, esm->cursor_col);
}

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

	g_free(esm->selection);
}

static void
esm_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModel *esm = E_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_SORTER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(esm->sorter);
		break;

	case ARG_CURSOR_ROW:
		GTK_VALUE_INT(*arg) = esm->cursor_row;
		break;

	case ARG_CURSOR_COL:
		GTK_VALUE_INT(*arg) = esm->cursor_col;
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

	case ARG_CURSOR_ROW:
		e_selection_model_do_something(esm, GTK_VALUE_INT(*arg), esm->cursor_col, 0);
		break;

	case ARG_CURSOR_COL:
		e_selection_model_do_something(esm, esm->cursor_row, GTK_VALUE_INT(*arg), 0);
		break;

	case ARG_SELECTION_MODE:
		esm->mode = GTK_VALUE_ENUM(*arg);
		if (esm->mode == GTK_SELECTION_SINGLE) {
			e_selection_model_do_something(esm, esm->cursor_row, esm->cursor_col, 0);
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
	selection->selection = NULL;
	selection->row_count = -1;
	selection->selection_start_row = 0;
	selection->cursor_row = -1;
	selection->cursor_col = -1;
	selection->mode = GTK_SELECTION_MULTIPLE;
	selection->cursor_mode = E_CURSOR_SIMPLE;
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
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectionModelClass, cursor_changed),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_selection_model_signals [CURSOR_ACTIVATED] =
		gtk_signal_new ("cursor_activated",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectionModelClass, cursor_activated),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_selection_model_signals [SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectionModelClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	klass->cursor_changed    = NULL;
	klass->cursor_activated  = NULL;
	klass->selection_changed = NULL;

	gtk_object_class_add_signals (object_class, e_selection_model_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ESelectionModel::sorter", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_SORTER);
	gtk_object_add_arg_type ("ESelectionModel::cursor_row", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_ROW);
	gtk_object_add_arg_type ("ESelectionModel::cursor_col", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_COL);
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
	if (selection->row_count < n || selection->row_count == 0)
		return 0;
	else
		return (selection->selection[BOX(n)] >> OFFSET(n)) & 0x1;
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
	int i;
	int last = (selection->row_count + 31) / 32;
	for (i = 0; i < last; i++) {
		if (selection->selection[i]) {
			int j;
			guint32 value = selection->selection[i];
			for (j = 0; j < 32; j++) {
				if (value & 0x80000000) {
					callback(i * 32 + j, closure);
				}
				value <<= 1;
			}
		}
	}
}

#define OPERATE(object, i,mask,grow) ((grow) ? (((object)->selection[(i)]) |= ((guint32) ~(mask))) : (((object)->selection[(i)]) &= (mask)))

void
e_selection_model_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	int i;
	i = BOX(row);

	OPERATE(selection, i, ~BITMASK(row), grow);
}

static void
change_selection(ESelectionModel *selection, int start, int end, gboolean grow)
{
	int i, last;
	if (start != end) {
		if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
			for ( i = start; i < end; i++) {
				e_selection_model_change_one_row(selection, e_sorter_sorted_to_model(selection->sorter, i), grow);
			}
		} else {
			i = BOX(start);
			last = BOX(end);

			if (i == last) {
				OPERATE(selection, i, BITMASK_LEFT(start) | BITMASK_RIGHT(end), grow);
			} else {
				OPERATE(selection, i, BITMASK_LEFT(start), grow);
				if (grow)
					for (i ++; i < last; i++)
						selection->selection[i] = ONES;
				else
					for (i ++; i < last; i++)
						selection->selection[i] = 0;
				OPERATE(selection, i, BITMASK_RIGHT(end), grow);
			}
		}
	}
}

static void
esm_select_single_row (ESelectionModel *selection, int row)
{
	int i;
	for (i = 0; i < ((selection->row_count + 31) / 32); i++) {
		if (!((i == BOX(row) && selection->selection[i] == BITMASK(row)) ||
		      (i != BOX(row) && selection->selection[i] == 0))) {
			g_free(selection->selection);
			selection->selection = g_new0(gint, (selection->row_count + 31) / 32);
			selection->selection[BOX(row)] = BITMASK(row);

			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals [SELECTION_CHANGED]);
			break;
		}
	}

	selection->selection_start_row = row;
}

static void
esm_toggle_single_row (ESelectionModel *selection, int row)
{
	if (selection->selection[BOX(row)] & BITMASK(row))
		selection->selection[BOX(row)] &= ~BITMASK(row);
	else
		selection->selection[BOX(row)] |= BITMASK(row);
	selection->selection_start_row = row;
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals [SELECTION_CHANGED]);
}

static void
esm_move_selection_end (ESelectionModel *selection, int row)
{
	int old_start;
	int old_end;
	int new_start;
	int new_end;
	if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
		old_start = MIN (e_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, selection->cursor_row));
		old_end = MAX (e_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, selection->cursor_row)) + 1;
		new_start = MIN (e_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, row));
		new_end = MAX (e_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, row)) + 1;
	} else {
		old_start = MIN (selection->selection_start_row, selection->cursor_row);
		old_end = MAX (selection->selection_start_row, selection->cursor_row) + 1;
		new_start = MIN (selection->selection_start_row, row);
		new_end = MAX (selection->selection_start_row, row) + 1;
	}
	/* This wouldn't work nearly so smoothly if one end of the selection weren't held in place. */
	if (old_start < new_start)
		change_selection(selection, old_start, new_start, FALSE);
	if (new_start < old_start)
		change_selection(selection, new_start, old_start, TRUE);
	if (old_end < new_end)
		change_selection(selection, old_end, new_end, TRUE);
	if (new_end < old_end)
		change_selection(selection, new_end, old_end, FALSE);
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals [SELECTION_CHANGED]);
}

static void
esm_set_selection_end (ESelectionModel *selection, int row)
{
	esm_select_single_row(selection, selection->selection_start_row);
	selection->cursor_row = selection->selection_start_row;
	esm_move_selection_end(selection, row);
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

	if (row == -1 && col != -1)
		row = 0;
	if (col == -1 && row != -1)
		col = 0;

	if (!e_selection_model_confirm_row_count(selection))
		return;
	if (selection->row_count >= 0 && row < selection->row_count) {
		switch (selection->mode) {
		case GTK_SELECTION_SINGLE:
			esm_select_single_row (selection, row);
			break;
		case GTK_SELECTION_BROWSE:
		case GTK_SELECTION_MULTIPLE:
		case GTK_SELECTION_EXTENDED:
			if (shift_p) {
				esm_set_selection_end (selection, row);
			} else {
				if (ctrl_p) {
					esm_toggle_single_row (selection, row);
				} else {
					esm_select_single_row (selection, row);
				}
			}
			break;
		}
		if (selection->cursor_row != row ||
		    selection->cursor_col != col) {
			selection->cursor_row = row;
			selection->cursor_col = col;
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_CHANGED], row, col);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], row, col);
		}
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
void
e_selection_model_maybe_do_something      (ESelectionModel *selection,
						 guint                 row,
						 guint                 col,
						 GdkModifierType       state)
{
	if (e_selection_model_is_row_selected(selection, row)) {
		selection->cursor_row = row;
		selection->cursor_col = col;
	} else {
		e_selection_model_do_something(selection, row, col, state);
	}
}

static gint
move_selection (ESelectionModel *selection,
		gboolean              up,
		GdkModifierType       state)
{
	int row = selection->cursor_row;
	int col = selection->cursor_col;
	int cursor_activated = TRUE;

	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;

	row = e_sorter_model_to_sorted(selection->sorter, row);
	if (up)
		row--;
	else
		row++;
	if (row < 0)
		row = 0;
	if (row >= selection->row_count)
		row = selection->row_count - 1;
	row = e_sorter_sorted_to_model(selection->sorter, row);

	switch (selection->mode) {
	case GTK_SELECTION_BROWSE:
		if (shift_p) {
			esm_set_selection_end (selection, row);
		} else if (!ctrl_p) {
			esm_select_single_row (selection, row);
		} else
			cursor_activated = FALSE;
		break;
	case GTK_SELECTION_SINGLE:
	case GTK_SELECTION_MULTIPLE:
	case GTK_SELECTION_EXTENDED:
		esm_select_single_row (selection, row);
		break;
	}
	if (row != -1) {
		selection->cursor_row = row;
		gtk_signal_emit(GTK_OBJECT(selection),
				e_selection_model_signals[CURSOR_CHANGED], row, col);
		if (cursor_activated)
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], row, col);
	}
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
	switch (key->keyval) {
	case GDK_Up:
		return move_selection(selection, TRUE, key->state);
		break;
	case GDK_Down:
		return move_selection(selection, FALSE, key->state);
		break;
	case GDK_space:
	case GDK_KP_Space:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			esm_toggle_single_row (selection, selection->cursor_row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], selection->cursor_row, selection->cursor_col);
			return TRUE;
		}
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if (selection->mode != GTK_SELECTION_SINGLE) {
			esm_select_single_row (selection, selection->cursor_row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], selection->cursor_row, selection->cursor_col);
			return TRUE;
		}
		break;
	case GDK_Home:
	case GDK_KP_Home:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			int row = 0;

			row = e_sorter_sorted_to_model(selection->sorter, row);
			selection->cursor_row = row;

			esm_select_single_row (selection, selection->cursor_row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_CHANGED], selection->cursor_row, selection->cursor_col);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], selection->cursor_row, selection->cursor_col);
			return TRUE;
		}
		break;
	case GDK_End:
	case GDK_KP_End:
		if (selection->cursor_mode == E_CURSOR_LINE) {
			int row = selection->row_count - 1;

			row = e_sorter_sorted_to_model(selection->sorter, row);
			selection->cursor_row = row;

			esm_select_single_row (selection, selection->cursor_row);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_CHANGED], selection->cursor_row, selection->cursor_col);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_selection_model_signals[CURSOR_ACTIVATED], selection->cursor_row, selection->cursor_col);
			return TRUE;
		}
		break;
	}
	return FALSE;
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
	g_free(selection->selection);
	selection->selection = NULL;
	selection->row_count = -1;
	selection->cursor_row = -1;
	selection->cursor_col = -1;
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals [CURSOR_CHANGED], -1, -1);
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals [SELECTION_CHANGED]);
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
gint
e_selection_model_selected_count (ESelectionModel *selection)
{
	gint count;
	int i;
	int last;

	if (!selection->selection)
		return 0;

	count = 0;

	last = BOX(selection->row_count - 1);

	for (i = 0; i <= last; i++) {
		int j;
		guint32 thiscount = 0;
		for (j = 0; j < 8; j++)
			thiscount += PART(selection->selection[i], j);
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
void
e_selection_model_select_all (ESelectionModel *selection)
{
	int i;

	if (!e_selection_model_confirm_row_count(selection))
		return;
	
	if (!selection->selection)
		selection->selection = g_new0 (gint, (selection->row_count + 31) / 32);
	
	for (i = 0; i < (selection->row_count + 31) / 32; i ++) {
		selection->selection[i] = ONES;
	}

	/* need to zero out the bits corresponding to the rows not
	   selected in the last full 32 bit mask */
	if (selection->row_count % 32) {
		int unselected_mask = 0;
		int num_unselected_in_last_byte = 32 - selection->row_count % 32;

		for (i = 0; i < num_unselected_in_last_byte; i ++)
			unselected_mask |= 1 << i;

		selection->selection[(selection->row_count + 31) / 32 - 1] &= ~unselected_mask;
	}

	selection->cursor_col = 0;
	selection->cursor_row = 0;
	selection->selection_start_row = 0;
	gtk_signal_emit (GTK_OBJECT (selection),
			 e_selection_model_signals [CURSOR_CHANGED], 0, 0);
	gtk_signal_emit (GTK_OBJECT (selection),
			 e_selection_model_signals [SELECTION_CHANGED]);
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
	int i;

	if (!e_selection_model_confirm_row_count(selection))
			return;
	
	if (!selection->selection)
		selection->selection = g_new0 (gint, (selection->row_count + 31) / 32);
	
	for (i = 0; i < (selection->row_count + 31) / 32; i ++) {
		selection->selection[i] = ~selection->selection[i];
	}
	
	selection->cursor_col = -1;
	selection->cursor_row = -1;
	selection->selection_start_row = 0;
	gtk_signal_emit (GTK_OBJECT (selection),
			 e_selection_model_signals [CURSOR_CHANGED], -1, -1);
	gtk_signal_emit (GTK_OBJECT (selection),
			 e_selection_model_signals [SELECTION_CHANGED]);
}

int
e_selection_model_get_row_count (ESelectionModel *selection)
{
	g_return_val_if_fail(selection != NULL, 0);
	g_return_val_if_fail(E_IS_SELECTION_MODEL(selection), 0);

	if (ESM_CLASS(selection)->get_row_count)
		return ESM_CLASS(selection)->get_row_count (selection);
	else
		return 0;
}

void
e_selection_model_change_cursor (ESelectionModel *selection, int row)
{
	g_return_if_fail(selection != NULL);
	g_return_if_fail(E_IS_SELECTION_MODEL(selection));

	selection->cursor_row = row;
	if (row == -1)
		selection->cursor_col = -1;
	else if (selection->cursor_col == -1)
		selection->cursor_col = 0;
	gtk_signal_emit(GTK_OBJECT(selection),
			e_selection_model_signals[CURSOR_CHANGED], selection->cursor_row, selection->cursor_col);
}

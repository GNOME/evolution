/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-selection-model.c: a Table Selection Model
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-selection-model.h"
#include "e-util/e-util.h"

#define ETSM_CLASS(e) ((ETableSelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

#define ONES ((guint32) 0xffffffff)

#define BOX(n) ((n) / 32)
#define OFFSET(n) (31 - ((n) % 32))
#define BITMASK(n) ((guint32)(((guint32) 0x1) << OFFSET(n)))
#define BITMASK_LEFT(n) ((guint32)(((guint32) ONES) << (32 - ((n) % 32))))
#define BITMASK_RIGHT(n) ((guint32)(((guint32) ONES) >> ((n) % 32)))

static GtkObjectClass *e_table_selection_model_parent_class;

enum {
	CURSOR_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint e_table_selection_model_signals [LAST_SIGNAL] = { 0, };

enum {
	ARG_0,
	ARG_MODEL,
	ARG_SORTER,
	ARG_CURSOR_ROW,
	ARG_CURSOR_COL,
};

static void
model_changed(ETableModel *etm, ETableSelectionModel *etsm)
{
	e_table_selection_model_clear(etsm);
}

#if 0
static void
model_row_inserted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	int box;
	int i;
	if(etsm->row_count >= 0) {
		/* Add another word if needed. */
		if ((etsm->row_count & 0x1f) == 0) {
			etsm->selection = g_renew(etsm->selection, gint, (etsm->row_count >> 5) + 1);
			etsm->selection[etsm->row_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = BOX(row);
		/* Shift all words to the right of our box right one bit. */
		for (i = etsm->row_count >> 5; i > box; i--) {
			etsm->selection[i] = (etsm->selection[i] >> 1) | (etsm->selection[i - 1] << 31);
		}

		/* Shift right half of box one bit to the right. */
		etsm->selection[box] = (etsm->selection[box] & BITMASK_LEFT(row)) | ((etsm->selection[box] & BITMASK_RIGHT(row)) >> 1);
		etsm->row_count ++;
	}
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	int box;
	int i;
	int last;
	if(etsm->row_count >= 0) {
		guint32 bitmask;
		box = row >> 5;
		last = etsm->row_count >> 5;

		/* Build bitmasks for the left and right half of the box */
		bitmask = BITMASK_RIGHT(row) >> 1;
		/* Shift right half of box one bit to the left. */
		etsm->selection[box] = (etsm->selection[box] & BITMASK_LEFT(row))| ((etsm->selection[box] & bitmask) << 1);

		/* Shift all words to the right of our box left one bit. */
		if (box < last) {
			etsm->selection[box] &= etsm->selection[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				etsm->selection[i] = (etsm->selection[i] << 1) | (etsm->selection[i + 1] >> 31);
			}
			etsm->selection[i] = etsm->selection[i] << 1;
		}
		etsm->row_count --;
		/* Remove the last word if not needed. */
		if ((etsm->row_count & 0x1f) == 0) {
			etsm->selection = g_renew(etsm->selection, gint, etsm->row_count >> 5);
		}
	}
}

#else

static void
model_row_inserted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	model_changed(etm, etsm);
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	model_changed(etm, etsm);
}
#endif

inline static void
add_model(ETableSelectionModel *etsm, ETableModel *model)
{
	etsm->model = model;
	if (model) {
		gtk_object_ref(GTK_OBJECT(model));
		etsm->model_changed_id = gtk_signal_connect(GTK_OBJECT(model), "model_changed",
							    GTK_SIGNAL_FUNC(model_changed), etsm);
		etsm->model_row_inserted_id = gtk_signal_connect(GTK_OBJECT(model), "model_row_inserted",
								 GTK_SIGNAL_FUNC(model_row_inserted), etsm);
		etsm->model_row_deleted_id = gtk_signal_connect(GTK_OBJECT(model), "model_row_deleted",
								GTK_SIGNAL_FUNC(model_row_deleted), etsm);
	}
}

inline static void
drop_model(ETableSelectionModel *etsm)
{
	if (etsm->model) {
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_changed_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_row_inserted_id);
		gtk_signal_disconnect(GTK_OBJECT(etsm->model),
				      etsm->model_row_deleted_id);
		gtk_object_unref(GTK_OBJECT(etsm->model));
	}
	etsm->model = NULL;
}

inline static void
add_sorter(ETableSelectionModel *etsm, ETableSorter *sorter)
{
	etsm->sorter = sorter;
	if (sorter) {
		gtk_object_ref(GTK_OBJECT(sorter));
	}
}

inline static void
drop_sorter(ETableSelectionModel *etsm)
{
	if (etsm->sorter) {
		gtk_object_unref(GTK_OBJECT(etsm->sorter));
	}
	etsm->sorter = NULL;
}

static void
etsm_destroy (GtkObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);

	drop_model(etsm);
	drop_sorter(etsm);

	g_free(etsm->selection);
}

static void
etsm_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_MODEL:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etsm->model);
		break;

	case ARG_SORTER:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(etsm->sorter);
		break;

	case ARG_CURSOR_ROW:
		GTK_VALUE_INT(*arg) = etsm->cursor_row;
		break;

	case ARG_CURSOR_COL:
		GTK_VALUE_INT(*arg) = etsm->cursor_col;
		break;
	}
}

static void
etsm_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSelectionModel *etsm = E_TABLE_SELECTION_MODEL (o);
	
	switch (arg_id){
	case ARG_MODEL:
		drop_model(etsm);
		add_model(etsm, GTK_VALUE_OBJECT (*arg) ? E_TABLE_MODEL(GTK_VALUE_OBJECT (*arg)) : NULL);
		break;

	case ARG_SORTER:
		drop_sorter(etsm);
		add_sorter(etsm, GTK_VALUE_OBJECT (*arg) ? E_TABLE_SORTER(GTK_VALUE_OBJECT (*arg)) : NULL);
		break;

	case ARG_CURSOR_ROW:
		e_table_selection_model_do_something(etsm, GTK_VALUE_INT(*arg), etsm->cursor_col, 0);
		break;

	case ARG_CURSOR_COL:
		e_table_selection_model_do_something(etsm, etsm->cursor_row, GTK_VALUE_INT(*arg), 0);
		break;
	}
}

static void
e_table_selection_model_init (ETableSelectionModel *selection)
{
	selection->selection = NULL;
	selection->row_count = -1;
	selection->model = NULL;
}

static void
e_table_selection_model_class_init (ETableSelectionModelClass *klass)
{
	GtkObjectClass *object_class;

	e_table_selection_model_parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);

	object_class->destroy = etsm_destroy;
	object_class->get_arg = etsm_get_arg;
	object_class->set_arg = etsm_set_arg;

	e_table_selection_model_signals [CURSOR_CHANGED] =
		gtk_signal_new ("cursor_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSelectionModelClass, cursor_changed),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_table_selection_model_signals [SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSelectionModelClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	klass->cursor_changed = NULL;
	klass->selection_changed = NULL;

	gtk_object_class_add_signals (object_class, e_table_selection_model_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ETableSelectionModel::model", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_MODEL);
	gtk_object_add_arg_type ("ETableSelectionModel::sorter", GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE, ARG_SORTER);
	gtk_object_add_arg_type ("ETableSelectionModel::cursor_row", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_ROW);
	gtk_object_add_arg_type ("ETableSelectionModel::cursor_col", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_COL);
}

E_MAKE_TYPE(e_table_selection_model, "ETableSelectionModel", ETableSelectionModel,
	    e_table_selection_model_class_init, e_table_selection_model_init, PARENT_TYPE);

ETableSelectionModel *
e_table_selection_model_new (void)
{
	return gtk_type_new (e_table_selection_model_get_type ());
}

gboolean
e_table_selection_model_is_row_selected (ETableSelectionModel *selection,
					 gint                 n)
{
	if (selection->row_count < n)
		return 0;
	else
		return (selection->selection[BOX(n)] >> OFFSET(n)) & 0x1;
}

void 
e_table_selection_model_foreach     (ETableSelectionModel *selection,
				     ETableForeachFunc callback,
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

#define OPERATE(object, mask, grow) ((grow) ? ((object) |= (~(mask))) : ((object) &= (mask)))

static void
change_one_row(ETableSelectionModel *selection, int row, gboolean grow)
{
	int i;
	i = BOX(row);

	OPERATE(selection->selection[i], BITMASK_LEFT(row) | BITMASK_RIGHT(row + 1), grow);
}

static void
change_selection(ETableSelectionModel *selection, int start, int end, gboolean grow)
{
	int i, last;
	if (start != end) {
		if (selection->sorter && e_table_sorter_needs_sorting(selection->sorter)) {
			for ( i = start; i < end; i++) {
				change_one_row(selection, e_table_sorter_sorted_to_model(selection->sorter, i), grow);
			}
		} else {
			i = BOX(start);
			last = BOX(end);

			if (i == last) {
				OPERATE(selection->selection[i], BITMASK_LEFT(start) | BITMASK_RIGHT(end), grow);
			} else {
				OPERATE(selection->selection[i], BITMASK_LEFT(start), grow);
				if (grow)
					for (i ++; i < last; i++)
						selection->selection[i] = ONES;
				else
					for (i ++; i < last; i++)
						selection->selection[i] = 0;
				OPERATE(selection->selection[i], BITMASK_RIGHT(end), grow);
			}
		}
	}
}

void             e_table_selection_model_do_something      (ETableSelectionModel *selection,
							    guint                 row,
							    guint                 col,
							    GdkModifierType       state)
{
	gint shift_p = state & GDK_SHIFT_MASK;
	gint ctrl_p = state & GDK_CONTROL_MASK;
	if (selection->row_count < 0) {
		if (selection->model) {
			selection->row_count = e_table_model_row_count(selection->model);
			g_free(selection->selection);
			selection->selection = g_new0(gint, (selection->row_count + 31) / 32);
		}
	}
	if (selection->row_count >= 0 && row < selection->row_count) {
		if (shift_p) {
			int old_start;
			int old_end;
			int new_start;
			int new_end;
			if (selection->sorter && e_table_sorter_needs_sorting(selection->sorter)) {
				old_start = MIN (e_table_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
						 e_table_sorter_model_to_sorted(selection->sorter, selection->cursor_row));
				old_end = MAX (e_table_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
					       e_table_sorter_model_to_sorted(selection->sorter, selection->cursor_row)) + 1;
				new_start = MIN (e_table_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
						 e_table_sorter_model_to_sorted(selection->sorter, row));
				new_end = MAX (e_table_sorter_model_to_sorted(selection->sorter, selection->selection_start_row),
					       e_table_sorter_model_to_sorted(selection->sorter, row)) + 1;
			} else {
				old_start = MIN (selection->selection_start_row, selection->cursor_row);
				old_end = MAX (selection->selection_start_row, selection->cursor_row) + 1;
				new_start = MIN (selection->selection_start_row, row);
				new_end = MAX (selection->selection_start_row, row) + 1;
			}
			/* This wouldn't work nearly so smoothly if one end of the selection held in place. */
			if (old_start < new_start)
				change_selection(selection, old_start, new_start, FALSE);
			if (new_start < old_start)
				change_selection(selection, new_start, old_start, TRUE);
			if (old_end < new_end)
				change_selection(selection, old_end, new_end, TRUE);
			if (new_end < old_end)
				change_selection(selection, new_end, old_end, FALSE);
			gtk_signal_emit(GTK_OBJECT(selection),
					e_table_selection_model_signals [SELECTION_CHANGED]);
		} else {
			if (ctrl_p) {
				if (selection->selection[BOX(row)] & BITMASK(row))
					selection->selection[BOX(row)] &= ~BITMASK(row);
				else
					selection->selection[BOX(row)] |= BITMASK(row);
				gtk_signal_emit(GTK_OBJECT(selection),
						e_table_selection_model_signals [SELECTION_CHANGED]);
			} else {
				int i;
				for (i = 0; i < ((selection->row_count + 31) / 32); i++) {
					if (!((i == BOX(row) && selection->selection[i] == BITMASK(row)) ||
					      (i != BOX(row) && selection->selection[i] == 0))) {
						g_free(selection->selection);
						selection->selection = g_new0(gint, (selection->row_count + 31) / 32);
						selection->selection[BOX(row)] = BITMASK(row);

						gtk_signal_emit(GTK_OBJECT(selection),
								e_table_selection_model_signals [SELECTION_CHANGED]);
						break;
					}
				}
			}
			selection->selection_start_row = row;
		}
		if (selection->cursor_row != row ||
		    selection->cursor_col != col) {
			selection->cursor_row = row;
			selection->cursor_col = col;
			gtk_signal_emit(GTK_OBJECT(selection),
					e_table_selection_model_signals[CURSOR_CHANGED], row, col);
		}
	}
}

void             e_table_selection_model_maybe_do_something      (ETableSelectionModel *selection,
								  guint                 row,
								  guint                 col,
								  GdkModifierType       state)
{
	if (e_table_selection_model_is_row_selected(selection, row)) {
		selection->cursor_row = row;
		selection->cursor_col = col;
	} else {
		e_table_selection_model_do_something(selection, row, col, state);
	}
}

void
e_table_selection_model_clear(ETableSelectionModel *selection)
{
	g_free(selection->selection);
	selection->selection = NULL;
	selection->row_count = -1;
	selection->cursor_row = -1;
	selection->cursor_col = -1;
	gtk_signal_emit(GTK_OBJECT(selection),
			e_table_selection_model_signals [CURSOR_CHANGED], -1, -1);
	gtk_signal_emit(GTK_OBJECT(selection),
			e_table_selection_model_signals [SELECTION_CHANGED]);
}

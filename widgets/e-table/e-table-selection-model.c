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
					  

static GtkObjectClass *e_table_selection_model_parent_class;

enum {
	SELECTION_MODEL_CHANGED,
	GROUP_SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint e_table_selection_model_signals [LAST_SIGNAL] = { 0, };

static void
model_changed(ETableModel *etm, ETableSelectionModel *etsm)
{
	g_free(etsm->selection);
	etsm->selection = NULL;
	etsm->row_count = -1;
}

static void
model_row_inserted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	int box;
	int i;
	int offset;
	if(etsm->row_count >= 0) {
		guint32 bitmask1 = 0xffff;
		guint32 bitmask2;
		if ((etsm->row_count & 0x1f) == 0) {
			etsm->selection = e_realloc(etsm->selection, (etsm->row_count >> 5) + 1);
			etsm->selection[etsm->row_count >> 5] = 0;
		}
		box = row >> 5;
		for (i = etsm->row_count >> 5; i > box; i--) {
			etsm->selection[i] = (etsm->selection[i] >> 1) | (etsm->selection[i - 1] << 31)
		}

		offset = row & 0x1f;
		bitmask1 = bitmask1 << (32 - offset);
		bitmask2 = ~bitmask1;
		etsm->selection[i] = (etsm->selection[i] & bitmask1) | ((etsm->selection[i] & bitmask2) >> 1);
		etsm->row_count ++;
	}
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	int box;
	int i;
	int offset;
	if(etsm->row_count >= 0) {
		guint32 bitmask1 = 0xffff;
		guint32 bitmask2;
		box = row >> 5;
		last = etsm->row_count >> 5

		offset = row & 0x1f;
		bitmask1 = bitmask1 << (32 - offset);
		bitmask2 = (~bitmask1) >> 1;
		etsm->selection[box] = (etsm->selection[box] & bitmask1) | ((etsm->selection[box] & bitmask2) << 1);

		if (box < last) {
			etsm->selection[box] &= etsm->selection[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				etsm->selection[i] = (etsm->selection[i] << 1) | (etsm->selection[i + 1] >> 31);
			}
			etsm->selection[i] = etsm->selection[i] << 1;
		}
		etsm->row_count --;
		if ((etsm->row_count & 0x1f) == 0) {
			etsm->selection = e_realloc(etsm->selection, etsm->row_count >> 5);
		}
	}
}

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
	if (etsm->model)
		gtk_object_unref(GTK_OBJECT(etsm->model));
	etsm->model = NULL;
}

static void
etsm_destroy (GtkObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);
	
	g_free(etsm->selction);
}

static void
e_table_selection_model_init (ETableSelectionModel *selection)
{
	selection->selection = NULL;
	selection->row_count = -1;
}

static void
e_table_selection_model_class_init (ETableSelectionModelClass *klass)
{
	GtkObjectClass *object_class;

	e_table_selection_model_parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);
	
	object_class->destroy = etsm_destroy;

	e_table_selection_model_signals [SELECTION_MODEL_CHANGED] =
		gtk_signal_new ("selection_model_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSelectionModelClass, selection_model_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_table_selection_model_signals [GROUP_SELECTION_CHANGED] =
		gtk_signal_new ("group_selection_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSelectionModelClass, group_selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	klass->selection_model_changed = NULL;
	klass->group_selection_changed = NULL;

	gtk_object_class_add_signals (object_class, e_table_selection_model_signals, LAST_SIGNAL);
}

E_MAKE_TYPE(e_table_selection_model, "ETableSelectionModel", ETableSelectionModel,
	    e_table_selection_model_class_init, e_table_selection_model_init, PARENT_TYPE);

static void
e_table_selection_model_selection_model_changed (ETableSelectionModel *selection)
{
	g_return_if_fail (selection != NULL);
	g_return_if_fail (E_IS_TABLE_SELECTION_MODEL (selection));
	
	if (selection->frozen) {
		selection->selection_model_changed = 1;
	} else {
		gtk_signal_emit (GTK_OBJECT (selection),
				 e_table_selection_model_signals [SELECTION_MODEL_CHANGED]);
	}
}

static void
e_table_selection_model_group_selection_changed (ETableSelectionModel *selection)
{
	g_return_if_fail (selection != NULL);
	g_return_if_fail (E_IS_TABLE_SELECTION_MODEL (selection));
	
	if (selection->frozen) {
		selection->group_selection_changed = 1;
	} else {
		gtk_signal_emit (GTK_OBJECT (selection),
				 e_table_selection_model_signals [GROUP_SELECTION_CHANGED]);
	}
}

void 
e_table_selection_model_freeze             (ETableSelectionModel *selection)
{
	selection->frozen = 1;
}

void
e_table_selection_model_thaw               (ETableSelectionModel *selection)
{
	selection->frozen = 0;
	if (selection->selection_model_changed) {
		selection->selection_model_changed = 0;
		e_table_selection_model_selection_model_changed(selection);
	}
	if (selection->group_selection_changed) {
		selection->group_selection_changed = 0;
		e_table_selection_model_group_selection_changed(selection);
	}
}


guint
e_table_selection_model_grouping_get_count (ETableSelectionModel *selection)
{
	return selection->group_count;
}

static void
e_table_selection_model_grouping_real_truncate  (ETableSelectionModel *selection, int length)
{
	if (length < selection->group_count) {
		selection->group_count = length;
	}
	if (length > selection->group_count) {
		selection->groupings = g_realloc(selection->groupings, length * sizeof(ETableSortColumn));
		selection->group_count = length;
	}
}

void
e_table_selection_model_grouping_truncate  (ETableSelectionModel *selection, int length)
{
	e_table_selection_model_grouping_real_truncate(selection, length);
	e_table_selection_model_group_selection_changed(selection);
}

ETableSortColumn
e_table_selection_model_grouping_get_nth   (ETableSelectionModel *selection, int n)
{
	if (n < selection->group_count) {
		return selection->groupings[n];
	} else {
		ETableSortColumn fake = {0, 0};
		return fake;
	}
}

void
e_table_selection_model_grouping_set_nth   (ETableSelectionModel *selection, int n, ETableSortColumn column)
{
	if (n >= selection->group_count) {
		e_table_selection_model_grouping_real_truncate(selection, n + 1);
	}
	selection->groupings[n] = column;
	e_table_selection_model_group_selection_changed(selection);
}


guint
e_table_selection_model_sorting_get_count (ETableSelectionModel *selection)
{
	return selection->sort_count;
}

static void
e_table_selection_model_sorting_real_truncate  (ETableSelectionModel *selection, int length)
{
	if (length < selection->sort_count) {
		selection->sort_count = length;
	}
	if (length > selection->sort_count) {
		selection->sortings = g_realloc(selection->sortings, length * sizeof(ETableSortColumn));
		selection->sort_count = length;
	}
}

void
e_table_selection_model_sorting_truncate  (ETableSelectionModel *selection, int length)
{
	e_table_selection_model_sorting_real_truncate  (selection, length);
	e_table_selection_model_selection_model_changed(selection);
}

ETableSortColumn
e_table_selection_model_sorting_get_nth   (ETableSelectionModel *selection, int n)
{
	if (n < selection->sort_count) {
		return selection->sortings[n];
	} else {
		ETableSortColumn fake = {0, 0};
		return fake;
	}
}

void
e_table_selection_model_sorting_set_nth   (ETableSelectionModel *selection, int n, ETableSortColumn column)
{
	if (n >= selection->sort_count) {
		e_table_selection_model_sorting_real_truncate(selection, n + 1);
	}
	selection->sortings[n] = column;
	e_table_selection_model_selection_model_changed(selection);
}


ETableSelectionModel *
e_table_selection_model_new (void)
{
	return gtk_type_new (e_table_selection_model_get_type ());
}

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

		/* Add another word if needed. */
		if ((etsm->row_count & 0x1f) == 0) {
			etsm->selection = g_realloc(etsm->selection, (etsm->row_count >> 5) + 1);
			etsm->selection[etsm->row_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = row >> 5;
		/* Shift all words to the right of our box right one bit. */
		for (i = etsm->row_count >> 5; i > box; i--) {
			etsm->selection[i] = (etsm->selection[i] >> 1) | (etsm->selection[i - 1] << 31);
		}

		/* Build bitmasks for the left and right half of the box */
		offset = row & 0x1f;
		bitmask1 = bitmask1 << (32 - offset);
		bitmask2 = ~bitmask1;
		/* Shift right half of box one bit to the right. */
		etsm->selection[box] = (etsm->selection[box] & bitmask1) | ((etsm->selection[box] & bitmask2) >> 1);
		etsm->row_count ++;
	}
}

static void
model_row_deleted(ETableModel *etm, int row, ETableSelectionModel *etsm)
{
	int box;
	int i;
	int last;
	int offset;
	if(etsm->row_count >= 0) {
		guint32 bitmask1 = 0xffff;
		guint32 bitmask2;
		box = row >> 5;
		last = etsm->row_count >> 5;

		/* Build bitmasks for the left and right half of the box */
		offset = row & 0x1f;
		bitmask1 = bitmask1 << (32 - offset);
		bitmask2 = (~bitmask1) >> 1;
		/* Shift right half of box one bit to the left. */
		etsm->selection[box] = (etsm->selection[box] & bitmask1) | ((etsm->selection[box] & bitmask2) << 1);

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
			etsm->selection = g_realloc(etsm->selection, etsm->row_count >> 5);
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

static void
etsm_destroy (GtkObject *object)
{
	ETableSelectionModel *etsm;

	etsm = E_TABLE_SELECTION_MODEL (object);
	
	g_free(etsm->selection);
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
#if 0
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
#endif
	gtk_object_class_add_signals (object_class, e_table_selection_model_signals, LAST_SIGNAL);
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
					 int                   n)
{
	if (selection->row_count < n)
		return 0;
	else
		return ((selection->selection[n / 32]) >> (31 - (n % 32))) & 0x1;
}

GList *
e_table_selection_model_get_selection_list (ETableSelectionModel *selection)
{
	int i;
	GList *list = NULL;
	if (selection->row_count < 0)
		return NULL;
	for (i = selection->row_count / 32; i >= 0; i--) {
		if (selection->selection[i]) {
			int j;
			guint32 value = selection->selection[i];
			for (j = 31; j >= 0; j--) {
				if (value & 0x1) {
					list = g_list_prepend(list, (void *) (i * 32 + j));
				}
				value >>= 1;
			}
		}
	}

	return NULL;
}

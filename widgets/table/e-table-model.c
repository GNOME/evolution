/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-model.c: a Table Model
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-table-model.h"

#define ETM_CLASS(e) ((ETableModelClass *)((GtkObject *)e)->klass)

static GtkObjectClass *e_table_model_parent_class;

enum {
	MODEL_CHANGED,
	MODEL_ROW_CHANGED,
	MODEL_CELL_CHANGED,
	ROW_SELECTION,
	LAST_SIGNAL
};

static guint e_table_model_signals [LAST_SIGNAL] = { 0, };

int
e_table_model_column_count (ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), 0);

	return ETM_CLASS (e_table_model)->column_count (e_table_model);
}


int
e_table_model_row_count (ETableModel *e_table_model)
{
	g_return_val_if_fail (e_table_model != NULL, 0);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), 0);

	return ETM_CLASS (e_table_model)->row_count (e_table_model);
}

void *
e_table_model_value_at (ETableModel *e_table_model, int col, int row)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	return ETM_CLASS (e_table_model)->value_at (e_table_model, col, row);
}

void
e_table_model_set_value_at (ETableModel *e_table_model, int col, int row, const void *data)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	ETM_CLASS (e_table_model)->set_value_at (e_table_model, col, row, data);

	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_ROW_CHANGED], row);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_CELL_CHANGED], col, row);

	/*
	 * Notice that "model_changed" is not emitted
	 */
}

gboolean
e_table_model_is_cell_editable (ETableModel *e_table_model, int col, int row)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	return ETM_CLASS (e_table_model)->is_cell_editable (e_table_model, col, row);
}

static void
e_table_model_destroy (GtkObject *object)
{
	if (e_table_model_parent_class->destroy)
		(*e_table_model_parent_class->destroy)(object);
}

static void
e_table_model_class_init (GtkObjectClass *object_class)
{
	e_table_model_parent_class = gtk_type_class (gtk_object_get_type ());
	
	object_class->destroy = e_table_model_destroy;

	e_table_model_signals [MODEL_CHANGED] =
		gtk_signal_new ("model_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_table_model_signals [MODEL_ROW_CHANGED] =
		gtk_signal_new ("model_row_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_row_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	e_table_model_signals [MODEL_CELL_CHANGED] =
		gtk_signal_new ("model_cell_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_cell_changed),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, e_table_model_signals, LAST_SIGNAL);
}

GtkType
e_table_model_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableModel",
			sizeof (ETableModel),
			sizeof (ETableModelClass),
			(GtkClassInitFunc) e_table_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

void
e_table_model_changed (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));
	
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_CHANGED]);
}

void
e_table_model_row_changed (ETableModel *e_table_model, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_ROW_CHANGED], row);
}

void
e_table_model_cell_changed (ETableModel *e_table_model, int col, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_CELL_CHANGED], col, row);
}

void
e_table_model_freeze (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	e_table_model->frozen = TRUE;
	return ETM_CLASS (e_table_model)->thaw (e_table_model);
}

void
e_table_model_thaw (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	e_table_model->frozen = FALSE;
	if (ETM_CLASS(e_table_model)->thaw)
		ETM_CLASS (e_table_model)->thaw (e_table_model);
}

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

#define PARENT_TYPE gtk_object_get_type ()

#define d(x)

d(static gint depth = 0);


static GtkObjectClass *e_table_model_parent_class;

enum {
	MODEL_CHANGED,
	MODEL_PRE_CHANGE,
	MODEL_ROW_CHANGED,
	MODEL_CELL_CHANGED,
	MODEL_ROW_INSERTED,
	MODEL_ROW_DELETED,
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
}

gboolean
e_table_model_is_cell_editable (ETableModel *e_table_model, int col, int row)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	return ETM_CLASS (e_table_model)->is_cell_editable (e_table_model, col, row);
}

void
e_table_model_append_row (ETableModel *e_table_model, ETableModel *source, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_CLASS (e_table_model)->append_row)
		ETM_CLASS (e_table_model)->append_row (e_table_model, source, row);
}

void *
e_table_model_duplicate_value (ETableModel *e_table_model, int col, const void *value)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->duplicate_value)
		return ETM_CLASS (e_table_model)->duplicate_value (e_table_model, col, value);
	else
		return NULL;
}

void
e_table_model_free_value (ETableModel *e_table_model, int col, void *value)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	if (ETM_CLASS (e_table_model)->free_value)
		ETM_CLASS (e_table_model)->free_value (e_table_model, col, value);
}

void *
e_table_model_initialize_value (ETableModel *e_table_model, int col)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->initialize_value)
		return ETM_CLASS (e_table_model)->initialize_value (e_table_model, col);
	else
		return NULL;
}

gboolean
e_table_model_value_is_empty (ETableModel *e_table_model, int col, const void *value)
{
	g_return_val_if_fail (e_table_model != NULL, FALSE);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), FALSE);

	if (ETM_CLASS (e_table_model)->value_is_empty)
		return ETM_CLASS (e_table_model)->value_is_empty (e_table_model, col, value);
	else
		return FALSE;
}

char *
e_table_model_value_to_string (ETableModel *e_table_model, int col, const void *value)
{
	g_return_val_if_fail (e_table_model != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_MODEL (e_table_model), NULL);

	if (ETM_CLASS (e_table_model)->value_to_string)
		return ETM_CLASS (e_table_model)->value_to_string (e_table_model, col, value);
	else
		return g_strdup("");
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
	ETableModelClass *klass = E_TABLE_MODEL_CLASS(object_class);
	e_table_model_parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = e_table_model_destroy;

	e_table_model_signals [MODEL_CHANGED] =
		gtk_signal_new ("model_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_table_model_signals [MODEL_PRE_CHANGE] =
		gtk_signal_new ("model_pre_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_pre_change),
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

	e_table_model_signals [MODEL_ROW_INSERTED] =
		gtk_signal_new ("model_row_inserted",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_row_inserted),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	e_table_model_signals [MODEL_ROW_DELETED] =
		gtk_signal_new ("model_row_deleted",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableModelClass, model_row_deleted),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, e_table_model_signals, LAST_SIGNAL);

	klass->column_count = NULL;     
	klass->row_count = NULL;        
	klass->value_at = NULL;         
	klass->set_value_at = NULL;     
	klass->is_cell_editable = NULL; 
	klass->append_row = NULL;

	klass->duplicate_value = NULL;  
	klass->free_value = NULL;       
	klass->initialize_value = NULL; 
	klass->value_is_empty = NULL;   
	klass->value_to_string = NULL;
	klass->model_changed = NULL;    
	klass->model_row_changed = NULL;
	klass->model_cell_changed = NULL;
	klass->model_row_inserted = NULL;
	klass->model_row_deleted = NULL;
}


guint
e_table_model_get_type (void)
{
  static guint type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"ETableModel",
	sizeof (ETableModel),
	sizeof (ETableModelClass),
	(GtkClassInitFunc) e_table_model_class_init,
	NULL,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      type = gtk_type_unique (PARENT_TYPE, &info);
    }

  return type;
}

#if d(!)0
static void
print_tabs (void)
{
	int i;
	for (i = 0; i < depth; i++)
		g_print("\t");
}
#endif

void
e_table_model_pre_change (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));
	
	d(print_tabs());
	d(g_print("Emitting pre_change on model 0x%p.\n", e_table_model));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_PRE_CHANGE]);
	d(depth--);
}

void
e_table_model_changed (ETableModel *e_table_model)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));
	
	d(print_tabs());
	d(g_print("Emitting model_changed on model 0x%p.\n", e_table_model));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_CHANGED]);
	d(depth--);
}

void
e_table_model_row_changed (ETableModel *e_table_model, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	d(print_tabs());
	d(g_print("Emitting row_changed on model 0x%p, row %d.\n", e_table_model, row));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_ROW_CHANGED], row);
	d(depth--);
}

void
e_table_model_cell_changed (ETableModel *e_table_model, int col, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	d(print_tabs());
	d(g_print("Emitting cell_changed on model 0x%p, row %d, col %d.\n", e_table_model, row, col));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_CELL_CHANGED], col, row);
	d(depth--);
}

void
e_table_model_row_inserted (ETableModel *e_table_model, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	d(print_tabs());
	d(g_print("Emitting row_inserted on model 0x%p, row %d.\n", e_table_model, row));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_ROW_INSERTED], row);
	d(depth--);
}

void
e_table_model_row_deleted (ETableModel *e_table_model, int row)
{
	g_return_if_fail (e_table_model != NULL);
	g_return_if_fail (E_IS_TABLE_MODEL (e_table_model));

	d(print_tabs());
	d(g_print("Emitting row_deleted on model 0x%p, row %d.\n", e_table_model, row));
	d(depth++);
	gtk_signal_emit (GTK_OBJECT (e_table_model),
			 e_table_model_signals [MODEL_ROW_DELETED], row);
	d(depth--);
}

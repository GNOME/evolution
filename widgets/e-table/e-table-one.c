/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-model.c: a one table model implementation that uses function
 * pointers to simplify the creation of new, exotic and colorful tables in
 * no time.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-table-one.h"

#define PARENT_TYPE e_table_model_get_type ()

static int
one_column_count (ETableModel *etm)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_column_count(one->source);
	else
		return 0;
}

static int
one_row_count (ETableModel *etm)
{
	return 1;
}

static void *
one_value_at (ETableModel *etm, int col, int row)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->data)
		return one->data[col];
	else
		return NULL;
}

static void
one_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->data && one->source) {
		e_table_model_free_value(one->source, col, one->data[col]);
		one->data[col] = e_table_model_duplicate_value(one->source, col, val);
	}
}

static gboolean
one_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_is_cell_editable(one->source, 0, row);
	else
		return FALSE;
}

/* The default for one_duplicate_value is to return the raw value. */
static void *
one_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_duplicate_value(one->source, col, value);
	else
		return (void *)value;
}

static void
one_free_value (ETableModel *etm, int col, void *value)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		e_table_model_free_value(one->source, col, value);
}

static void *
one_initialize_value (ETableModel *etm, int col)
{
	ETableOne *one = E_TABLE_ONE(etm);
	
	if (one->source)
		return e_table_model_initialize_value (one->source, col);
	else
		return NULL;
}

static gboolean
one_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETableOne *one = E_TABLE_ONE(etm);
	
	if (one->source)
		return e_table_model_value_is_empty (one->source, col, value);
	else
		return FALSE;
}

static char *
one_value_to_string (ETableModel *etm, int col, const void *value)
{
	ETableOne *one = E_TABLE_ONE(etm);
	
	if (one->source)
		return e_table_model_value_to_string (one->source, col, value);
	else
		return g_strdup("");
}

static void
one_destroy (GtkObject *object)
{
	ETableOne *one = E_TABLE_ONE(object);
	
	if (one->source) {
		int i;
		int col_count;

		col_count = e_table_model_column_count(one->source);
		
		if (one->data) {
			for (i = 0; i < col_count; i++) {
				e_table_model_free_value(one->source, i, one->data[i]);
			}
		}

		gtk_object_unref(GTK_OBJECT(one->source));
	}

	g_free(one->data);
}

static void
e_table_one_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	model_class->column_count = one_column_count;
	model_class->row_count = one_row_count;
	model_class->value_at = one_value_at;
	model_class->set_value_at = one_set_value_at;
	model_class->is_cell_editable = one_is_cell_editable;
	model_class->duplicate_value = one_duplicate_value;
	model_class->free_value = one_free_value;
	model_class->initialize_value = one_initialize_value;
	model_class->value_is_empty = one_value_is_empty;
	model_class->value_to_string = one_value_to_string;

	object_class->destroy = one_destroy;
}

static void
e_table_one_init (GtkObject *object)
{
	ETableOne *one = E_TABLE_ONE(object);

	one->source = NULL;
	one->data = NULL;
}

GtkType
e_table_one_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableOne",
			sizeof (ETableOne),
			sizeof (ETableOneClass),
			(GtkClassInitFunc) e_table_one_class_init,
			(GtkObjectInitFunc) e_table_one_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

ETableModel *
e_table_one_new (ETableModel *source)
{
	ETableOne *eto;
	int col_count;
	int i;

	eto = gtk_type_new (e_table_one_get_type ());

	eto->source = source;
	
	col_count = e_table_model_column_count(source);
	eto->data = g_new(void *, col_count);
	for (i = 0; i < col_count; i++) {
		eto->data[i] = e_table_model_initialize_value(source, i);
	}
	
	if (source)
		gtk_object_ref(GTK_OBJECT(source));
	
	return (ETableModel *) eto;
}

void
e_table_one_commit (ETableOne *one)
{
	if (one->source) {
		int empty = TRUE;
		int col;
		int cols = e_table_model_column_count(one->source);
		for (col = 0; col < cols; col++) {
			if (!e_table_model_value_is_empty(one->source, col, one->data[col])) {
				empty = FALSE;
				break;
			}
		}
		if (!empty) {
			e_table_model_append_row(one->source, E_TABLE_MODEL(one), 0);
		}
	}
}

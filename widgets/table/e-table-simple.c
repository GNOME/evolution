/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-model.c: a simple table model implementation that uses function
 * pointers to simplify the creation of new, exotic and colorful tables in
 * no time.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */

#include <config.h>
#include "e-table-simple.h"

#define PARENT_TYPE e_table_model_get_type ()

static int
simple_column_count (ETableModel *etm)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->col_count)
		return simple->col_count (etm, simple->data);
	else
		return 0;
}

static int
simple_row_count (ETableModel *etm)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->row_count)
		return simple->row_count (etm, simple->data);
	else
		return 0;
}

static void *
simple_value_at (ETableModel *etm, int col, int row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->value_at)
		return simple->value_at (etm, col, row, simple->data);
	else
		return NULL;
}

static void
simple_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->set_value_at)
		simple->set_value_at (etm, col, row, val, simple->data);
}

static gboolean
simple_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->is_cell_editable)
		return simple->is_cell_editable (etm, col, row, simple->data);
	else
		return FALSE;
}

/* The default for simple_duplicate_value is to return the raw value. */
static void *
simple_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->duplicate_value)
		return simple->duplicate_value (etm, col, value, simple->data);
	else
		return (void *)value;
}

static void
simple_free_value (ETableModel *etm, int col, void *value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->free_value)
		simple->free_value (etm, col, value, simple->data);
}

static void *
simple_initialize_value (ETableModel *etm, int col)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);
	
	if (simple->initialize_value)
		return simple->initialize_value (etm, col, simple->data);
	else
		return NULL;
}

static gboolean
simple_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);
	
	if (simple->value_is_empty)
		return simple->value_is_empty (etm, col, value, simple->data);
	else
		return FALSE;
}

static void
e_table_simple_class_init (GtkObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	model_class->column_count = simple_column_count;
	model_class->row_count = simple_row_count;
	model_class->value_at = simple_value_at;
	model_class->set_value_at = simple_set_value_at;
	model_class->is_cell_editable = simple_is_cell_editable;
	model_class->duplicate_value = simple_duplicate_value;
	model_class->free_value = simple_free_value;
	model_class->initialize_value = simple_initialize_value;
	model_class->value_is_empty = simple_value_is_empty;
}

GtkType
e_table_simple_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"ETableSimple",
			sizeof (ETableSimple),
			sizeof (ETableSimpleClass),
			(GtkClassInitFunc) e_table_simple_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

ETableModel *
e_table_simple_new (ETableSimpleColumnCountFn col_count,
		    ETableSimpleRowCountFn row_count,
		    ETableSimpleValueAtFn value_at,
		    ETableSimpleSetValueAtFn set_value_at,
		    ETableSimpleIsCellEditableFn is_cell_editable,
		    ETableSimpleDuplicateValueFn duplicate_value,
		    ETableSimpleFreeValueFn free_value,
		    ETableSimpleInitializeValueFn initialize_value,
		    ETableSimpleValueIsEmptyFn value_is_empty,
		    void *data)
{
	ETableSimple *et;

	et = gtk_type_new (e_table_simple_get_type ());

	et->col_count = col_count;
	et->row_count = row_count;
	et->value_at = value_at;
	et->set_value_at = set_value_at;
	et->is_cell_editable = is_cell_editable;
	et->duplicate_value = duplicate_value;
	et->free_value = free_value;
	et->initialize_value = initialize_value;
	et->value_is_empty = value_is_empty;
	et->data = data;
	
	return (ETableModel *) et;
}

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

#define PARENT_TYPE e_table_model_get_type()

static int
simple_column_count (ETableModel *etm)
{
	ETableSimple *simple = (ETableSimple *)etm;

	return simple->col_count (etm, simple->data);
}

static int
simple_row_count (ETableModel *etm)
{
	ETableSimple *simple = (ETableSimple *)etm;

	return simple->row_count (etm, simple->data);
}

static void *
simple_value_at (ETableModel *etm, int col, int row)
{
	ETableSimple *simple = (ETableSimple *)etm;

	return simple->value_at (etm, col, row, simple->data);
}

static void
simple_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETableSimple *simple = (ETableSimple *)etm;

	simple->set_value_at (etm, col, row, val, simple->data);
}

static gboolean
simple_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSimple *simple = (ETableSimple *)etm;

	return simple->is_cell_editable (etm, col, row, simple->data);
}

static void
simple_thaw (ETableModel *etm)
{
	ETableSimple *simple = (ETableSimple *)etm;

	simple->thaw (etm, simple->data);
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
	model_class->thaw = simple_thaw;
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
		    ETableSimpleThawFn thaw,
		    void *data)
{
	ETableSimple *et;

	et = gtk_type_new (e_table_simple_get_type ());

	et->col_count = col_count;
	et->row_count = row_count;
	et->value_at = value_at;
	et->set_value_at = set_value_at;
	et->is_cell_editable = is_cell_editable;
	et->thaw = thaw;
	et->data = data;
	
	return (ETableModel *) et;
}

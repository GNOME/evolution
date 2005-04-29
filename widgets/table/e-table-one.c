/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-one.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "gal/util/e-util.h"

#include "e-table-one.h"

static ETableModelClass *parent_class = NULL;

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
		return e_table_model_is_cell_editable(one->source, col, -1);
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
one_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
one_dispose (GObject *object)
{
	ETableOne *one = E_TABLE_ONE (object);


	if (one->data) {
		int i;
		int col_count;

		if (one->source) {
			col_count = e_table_model_column_count(one->source);

			for (i = 0; i < col_count; i++)
				e_table_model_free_value(one->source, i, one->data[i]);
		}
		
		g_free (one->data);
	}
	one->data = NULL;

	if (one->source)
		g_object_unref(one->source);
	one->source = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_table_one_class_init (GObjectClass *object_class)
{
	ETableModelClass *model_class = (ETableModelClass *) object_class;

	parent_class = g_type_class_peek_parent (object_class);

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

	object_class->dispose = one_dispose;
	object_class->finalize = one_finalize;
}

static void
e_table_one_init (GObject *object)
{
	ETableOne *one = E_TABLE_ONE(object);

	one->source = NULL;
	one->data = NULL;
}

E_MAKE_TYPE(e_table_one, "ETableOne", ETableOne, e_table_one_class_init, e_table_one_init, E_TABLE_MODEL_TYPE)


ETableModel *
e_table_one_new (ETableModel *source)
{
	ETableOne *eto;
	int col_count;
	int i;

	eto = g_object_new (E_TABLE_ONE_TYPE, NULL);
	eto->source = source;
	
	col_count = e_table_model_column_count(source);
	eto->data = g_new(void *, col_count);
	for (i = 0; i < col_count; i++) {
		eto->data[i] = e_table_model_initialize_value(source, i);
	}
	
	if (source)
		g_object_ref(source);
	
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

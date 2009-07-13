/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "e-util/e-util.h"

#include "e-table-one.h"

G_DEFINE_TYPE (ETableOne, e_table_one, E_TABLE_MODEL_TYPE)

static gint
one_column_count (ETableModel *etm)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_column_count(one->source);
	else
		return 0;
}

static gint
one_row_count (ETableModel *etm)
{
	return 1;
}

static gpointer
one_value_at (ETableModel *etm, gint col, gint row)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->data)
		return one->data[col];
	else
		return NULL;
}

static void
one_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer val)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->data && one->source) {
		e_table_model_free_value(one->source, col, one->data[col]);
		one->data[col] = e_table_model_duplicate_value(one->source, col, val);
	}
}

static gboolean
one_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_is_cell_editable(one->source, col, -1);
	else
		return FALSE;
}

/* The default for one_duplicate_value is to return the raw value. */
static gpointer
one_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_duplicate_value(one->source, col, value);
	else
		return (gpointer)value;
}

static void
one_free_value (ETableModel *etm, gint col, gpointer value)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		e_table_model_free_value(one->source, col, value);
}

static gpointer
one_initialize_value (ETableModel *etm, gint col)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_initialize_value (one->source, col);
	else
		return NULL;
}

static gboolean
one_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ETableOne *one = E_TABLE_ONE(etm);

	if (one->source)
		return e_table_model_value_is_empty (one->source, col, value);
	else
		return FALSE;
}

static gchar *
one_value_to_string (ETableModel *etm, gint col, gconstpointer value)
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
	G_OBJECT_CLASS (e_table_one_parent_class)->finalize (object);
}

static void
one_dispose (GObject *object)
{
	ETableOne *one = E_TABLE_ONE (object);

	if (one->data) {
		gint i;
		gint col_count;

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

	G_OBJECT_CLASS (e_table_one_parent_class)->dispose (object);
}

static void
e_table_one_class_init (ETableOneClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *model_class = E_TABLE_MODEL_CLASS (klass);

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
e_table_one_init (ETableOne *one)
{
	one->source = NULL;
	one->data = NULL;
}

ETableModel *
e_table_one_new (ETableModel *source)
{
	ETableOne *eto;
	gint col_count;
	gint i;

	eto = g_object_new (E_TABLE_ONE_TYPE, NULL);
	eto->source = source;

	col_count = e_table_model_column_count(source);
	eto->data = g_new(gpointer , col_count);
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
		gint empty = TRUE;
		gint col;
		gint cols = e_table_model_column_count(one->source);
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

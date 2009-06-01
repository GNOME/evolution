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
 *		Miguel de Icaza <miguel.ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "e-util/e-util.h"

#include "e-table-simple.h"

G_DEFINE_TYPE (ETableSimple, e_table_simple, E_TABLE_MODEL_TYPE)

static gint
simple_column_count (ETableModel *etm)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->col_count)
		return simple->col_count (etm, simple->data);
	else
		return 0;
}

static gint
simple_row_count (ETableModel *etm)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->row_count)
		return simple->row_count (etm, simple->data);
	else
		return 0;
}

static void
simple_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->append_row)
		simple->append_row (etm, source, row, simple->data);
}

static gpointer
simple_value_at (ETableModel *etm, gint col, gint row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->value_at)
		return simple->value_at (etm, col, row, simple->data);
	else
		return NULL;
}

static void
simple_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer val)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->set_value_at)
		simple->set_value_at (etm, col, row, val, simple->data);
}

static gboolean
simple_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->is_cell_editable)
		return simple->is_cell_editable (etm, col, row, simple->data);
	else
		return FALSE;
}

static gboolean
simple_has_save_id (ETableModel *etm)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->has_save_id)
		return simple->has_save_id (etm, simple->data);
	else
		return FALSE;
}

static gchar *
simple_get_save_id (ETableModel *etm, gint row)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->get_save_id)
		return simple->get_save_id (etm, row, simple->data);
	else
		return NULL;
}

/* The default for simple_duplicate_value is to return the raw value. */
static gpointer
simple_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->duplicate_value)
		return simple->duplicate_value (etm, col, value, simple->data);
	else
		return (gpointer)value;
}

static void
simple_free_value (ETableModel *etm, gint col, gpointer value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->free_value)
		simple->free_value (etm, col, value, simple->data);
}

static gpointer
simple_initialize_value (ETableModel *etm, gint col)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->initialize_value)
		return simple->initialize_value (etm, col, simple->data);
	else
		return NULL;
}

static gboolean
simple_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->value_is_empty)
		return simple->value_is_empty (etm, col, value, simple->data);
	else
		return FALSE;
}

static gchar *
simple_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	ETableSimple *simple = E_TABLE_SIMPLE(etm);

	if (simple->value_to_string)
		return simple->value_to_string (etm, col, value, simple->data);
	else
		return g_strdup ("");
}

static void
e_table_simple_class_init (ETableSimpleClass *klass)
{
	ETableModelClass *model_class = E_TABLE_MODEL_CLASS (klass);

	model_class->column_count      = simple_column_count;
	model_class->row_count         = simple_row_count;
	model_class->append_row        = simple_append_row;

	model_class->value_at          = simple_value_at;
	model_class->set_value_at      = simple_set_value_at;
	model_class->is_cell_editable  = simple_is_cell_editable;

	model_class->has_save_id       = simple_has_save_id;
	model_class->get_save_id       = simple_get_save_id;

	model_class->duplicate_value   = simple_duplicate_value;
	model_class->free_value        = simple_free_value;
	model_class->initialize_value  = simple_initialize_value;
	model_class->value_is_empty    = simple_value_is_empty;
	model_class->value_to_string   = simple_value_to_string;
}

static void
e_table_simple_init (ETableSimple *simple)
{
	/* nothing to do */
}

/**
 * e_table_simple_new:
 * @col_count:
 * @row_count:
 * @value_at:
 * @set_value_at:
 * @is_cell_editable:
 * @duplicate_value:
 * @free_value:
 * @initialize_value:
 * @value_is_empty:
 * @value_to_string:
 * @data: closure pointer.
 *
 * This initializes a new ETableSimpleModel object.  ETableSimpleModel is
 * an implementaiton of the abstract class ETableModel.  The ETableSimpleModel
 * is designed to allow people to easily create ETableModels without having
 * to create a new GType derived from ETableModel every time they need one.
 *
 * Instead, ETableSimpleModel uses a setup based in callback functions, every
 * callback function signature mimics the signature of each ETableModel method
 * and passes the extra @data pointer to each one of the method to provide them
 * with any context they might want to use.
 *
 * Returns: An ETableSimpleModel object (which is also an ETableModel
 * object).
 */
ETableModel *
e_table_simple_new  (ETableSimpleColumnCountFn      col_count,
		     ETableSimpleRowCountFn         row_count,
		     ETableSimpleAppendRowFn        append_row,

		     ETableSimpleValueAtFn          value_at,
		     ETableSimpleSetValueAtFn       set_value_at,
		     ETableSimpleIsCellEditableFn   is_cell_editable,

		     ETableSimpleHasSaveIdFn        has_save_id,
		     ETableSimpleGetSaveIdFn        get_save_id,

		     ETableSimpleDuplicateValueFn   duplicate_value,
		     ETableSimpleFreeValueFn        free_value,
		     ETableSimpleInitializeValueFn  initialize_value,
		     ETableSimpleValueIsEmptyFn     value_is_empty,
		     ETableSimpleValueToStringFn    value_to_string,
		     void                          *data)
{
	ETableSimple *et = g_object_new (E_TABLE_SIMPLE_TYPE, NULL);

	et->col_count         = col_count;
	et->row_count         = row_count;
	et->append_row        = append_row;

	et->value_at          = value_at;
	et->set_value_at      = set_value_at;
	et->is_cell_editable  = is_cell_editable;

	et->has_save_id       = has_save_id;
	et->get_save_id       = get_save_id;

	et->duplicate_value   = duplicate_value;
	et->free_value        = free_value;
	et->initialize_value  = initialize_value;
	et->value_is_empty    = value_is_empty;
	et->value_to_string   = value_to_string;
	et->data              = data;

	return (ETableModel *) et;
}

gpointer
e_table_simple_string_duplicate_value (ETableModel *etm, gint col, gconstpointer val, gpointer data)
{
	return g_strdup (val);
}

void
e_table_simple_string_free_value (ETableModel *etm, gint col, gpointer val, gpointer data)
{
	g_free (val);
}

gpointer
e_table_simple_string_initialize_value (ETableModel *etm, gint col, gpointer data)
{
	return g_strdup ("");
}

gboolean
e_table_simple_string_value_is_empty (ETableModel *etm, gint col, gconstpointer val, gpointer data)
{
	return !(val && * (gchar *) val);
}

gchar *
e_table_simple_string_value_to_string (ETableModel *etm, gint col, gconstpointer val, gpointer data)
{
	return g_strdup (val);
}

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

#include "e-table-memory-callbacks.h"

G_DEFINE_TYPE (ETableMemoryCalbacks, e_table_memory_callbacks, E_TABLE_MEMORY_TYPE)

static gint
etmc_column_count (ETableModel *etm)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->col_count)
		return etmc->col_count (etm, etmc->data);
	else
		return 0;
}

static gpointer
etmc_value_at (ETableModel *etm, gint col, gint row)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->value_at)
		return etmc->value_at (etm, col, row, etmc->data);
	else
		return NULL;
}

static void
etmc_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer val)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->set_value_at)
		etmc->set_value_at (etm, col, row, val, etmc->data);
}

static gboolean
etmc_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->is_cell_editable)
		return etmc->is_cell_editable (etm, col, row, etmc->data);
	else
		return FALSE;
}

/* The default for etmc_duplicate_value is to return the raw value. */
static gpointer
etmc_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->duplicate_value)
		return etmc->duplicate_value (etm, col, value, etmc->data);
	else
		return (gpointer)value;
}

static void
etmc_free_value (ETableModel *etm, gint col, gpointer value)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->free_value)
		etmc->free_value (etm, col, value, etmc->data);
}

static gpointer
etmc_initialize_value (ETableModel *etm, gint col)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->initialize_value)
		return etmc->initialize_value (etm, col, etmc->data);
	else
		return NULL;
}

static gboolean
etmc_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->value_is_empty)
		return etmc->value_is_empty (etm, col, value, etmc->data);
	else
		return FALSE;
}

static gchar *
etmc_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->value_to_string)
		return etmc->value_to_string (etm, col, value, etmc->data);
	else
		return g_strdup ("");
}

static void
etmc_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	ETableMemoryCalbacks *etmc = E_TABLE_MEMORY_CALLBACKS(etm);

	if (etmc->append_row)
		etmc->append_row (etm, source, row, etmc->data);
}

static void
e_table_memory_callbacks_class_init (ETableMemoryCalbacksClass *klass)
{
	ETableModelClass *model_class = E_TABLE_MODEL_CLASS (klass);

	model_class->column_count     = etmc_column_count;
	model_class->value_at         = etmc_value_at;
	model_class->set_value_at     = etmc_set_value_at;
	model_class->is_cell_editable = etmc_is_cell_editable;
	model_class->duplicate_value  = etmc_duplicate_value;
	model_class->free_value       = etmc_free_value;
	model_class->initialize_value = etmc_initialize_value;
	model_class->value_is_empty   = etmc_value_is_empty;
	model_class->value_to_string  = etmc_value_to_string;
	model_class->append_row       = etmc_append_row;

}

static void
e_table_memory_callbacks_init (ETableMemoryCalbacks *etmc)
{
	/* nothing to do */
}

/**
 * e_table_memory_callbacks_new:
 * @col_count:
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
 * This initializes a new ETableMemoryCalbacksModel object.  ETableMemoryCalbacksModel is
 * an implementaiton of the abstract class ETableModel.  The ETableMemoryCalbacksModel
 * is designed to allow people to easily create ETableModels without having
 * to create a new GType derived from ETableModel every time they need one.
 *
 * Instead, ETableMemoryCalbacksModel uses a setup based in callback functions, every
 * callback function signature mimics the signature of each ETableModel method
 * and passes the extra @data pointer to each one of the method to provide them
 * with any context they might want to use.
 *
 * Returns: An ETableMemoryCalbacksModel object (which is also an ETableModel
 * object).
 */
ETableModel *
e_table_memory_callbacks_new (ETableMemoryCalbacksColumnCountFn col_count,
			      ETableMemoryCalbacksValueAtFn value_at,
			      ETableMemoryCalbacksSetValueAtFn set_value_at,
			      ETableMemoryCalbacksIsCellEditableFn is_cell_editable,
			      ETableMemoryCalbacksDuplicateValueFn duplicate_value,
			      ETableMemoryCalbacksFreeValueFn free_value,
			      ETableMemoryCalbacksInitializeValueFn initialize_value,
			      ETableMemoryCalbacksValueIsEmptyFn value_is_empty,
			      ETableMemoryCalbacksValueToStringFn value_to_string,
			      gpointer data)
{
	ETableMemoryCalbacks *et;

	et                   = g_object_new (E_TABLE_MEMORY_CALLBACKS_TYPE, NULL);

	et->col_count        = col_count;
	et->value_at         = value_at;
	et->set_value_at     = set_value_at;
	et->is_cell_editable = is_cell_editable;
	et->duplicate_value  = duplicate_value;
	et->free_value       = free_value;
	et->initialize_value = initialize_value;
	et->value_is_empty   = value_is_empty;
	et->value_to_string  = value_to_string;
	et->data             = data;

	return (ETableModel *) et;
 }

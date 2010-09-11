/*
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

#include <string.h>

#include "e-util/e-util.h"

#include "e-table-memory-store.h"

#define STORE_LOCATOR(etms, col, row) (*((etms)->priv->store + (row) * (etms)->priv->col_count + (col)))

struct _ETableMemoryStorePrivate {
	gint col_count;
	ETableMemoryStoreColumnInfo *columns;
	gpointer *store;
};

G_DEFINE_TYPE (ETableMemoryStore, e_table_memory_store, E_TABLE_MEMORY_TYPE)

static gpointer
duplicate_value (ETableMemoryStore *etms, gint col, gconstpointer val)
{
	switch (etms->priv->columns[col].type) {
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING:
		return g_strdup (val);
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF:
		if (val)
			g_object_ref ((gpointer) val);
		return (gpointer) val;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT:
		if (val)
			g_object_ref ((gpointer) val);
		return (gpointer) val;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM:
		if (etms->priv->columns[col].custom.duplicate_value)
			return etms->priv->columns[col].custom.duplicate_value (E_TABLE_MODEL (etms), col, val, NULL);
		break;
	default:
		break;
	}
	return (gpointer) val;
}

static void
free_value (ETableMemoryStore *etms, gint col, gpointer value)
{
	switch (etms->priv->columns[col].type) {
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING:
		g_free (value);
		break;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF:
		if (value)
			g_object_unref (value);
		break;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT:
		if (value)
			g_object_unref (value);
		break;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM:
		if (etms->priv->columns[col].custom.free_value)
			etms->priv->columns[col].custom.free_value (E_TABLE_MODEL (etms), col, value, NULL);
		break;
	default:
		break;
	}
}

static gint
etms_column_count (ETableModel *etm)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	return etms->priv->col_count;
}

static gpointer
etms_value_at (ETableModel *etm, gint col, gint row)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	return STORE_LOCATOR (etms, col, row);
}

static void
etms_set_value_at (ETableModel *etm, gint col, gint row, gconstpointer val)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	e_table_model_pre_change (etm);

	STORE_LOCATOR (etms, col, row) = duplicate_value (etms, col, val);

	e_table_model_cell_changed (etm, col, row);
}

static gboolean
etms_is_cell_editable (ETableModel *etm, gint col, gint row)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	return etms->priv->columns[col].editable;
}

/* The default for etms_duplicate_value is to return the raw value. */
static gpointer
etms_duplicate_value (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	return duplicate_value (etms, col, value);
}

static void
etms_free_value (ETableModel *etm, gint col, gpointer value)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	free_value (etms, col, value);
}

static gpointer
etms_initialize_value (ETableModel *etm, gint col)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	switch (etms->priv->columns[col].type) {
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING:
		return g_strdup ("");
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF:
		return NULL;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM:
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT:
		if (etms->priv->columns[col].custom.initialize_value)
			return etms->priv->columns[col].custom.initialize_value (E_TABLE_MODEL (etms), col, NULL);
		break;
	default:
		break;
	}
	return NULL;
}

static gboolean
etms_value_is_empty (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	switch (etms->priv->columns[col].type) {
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING:
		return !(value && *(gchar *) value);
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF:
		return value == NULL;
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM:
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT:
		if (etms->priv->columns[col].custom.value_is_empty)
			return etms->priv->columns[col].custom.value_is_empty (E_TABLE_MODEL (etms), col, value, NULL);
		break;
	default:
		break;
	}
	return value == NULL;
}

static gchar *
etms_value_to_string (ETableModel *etm, gint col, gconstpointer value)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);

	switch (etms->priv->columns[col].type) {
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_STRING:
		return g_strdup (value);
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_PIXBUF:
		return g_strdup ("");
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_CUSTOM:
	case E_TABLE_MEMORY_STORE_COLUMN_TYPE_OBJECT:
		if (etms->priv->columns[col].custom.value_is_empty)
			return etms->priv->columns[col].custom.value_to_string (E_TABLE_MODEL (etms), col, value, NULL);
		break;
	default:
		break;
	}
	return g_strdup_printf ("%d", GPOINTER_TO_INT (value));
}

static void
etms_append_row (ETableModel *etm, ETableModel *source, gint row)
{
	ETableMemoryStore *etms = E_TABLE_MEMORY_STORE (etm);
	gpointer *new_data;
	gint i;
	gint row_count;

	new_data = g_new (gpointer , etms->priv->col_count);

	for (i = 0; i < etms->priv->col_count; i++) {
		new_data[i] = e_table_model_value_at (source, i, row);
	}

	row_count = e_table_model_row_count (E_TABLE_MODEL (etms));

	e_table_memory_store_insert_array (etms, row_count, new_data, NULL);
}

static void
etms_finalize (GObject *obj)
{
	ETableMemoryStore *etms = (ETableMemoryStore *) obj;

	if (etms->priv) {
		e_table_memory_store_clear (etms);

		g_free (etms->priv->columns);
		g_free (etms->priv->store);
		g_free (etms->priv);
	}

	if (G_OBJECT_CLASS (e_table_memory_store_parent_class)->finalize)
		G_OBJECT_CLASS (e_table_memory_store_parent_class)->finalize (obj);
}

static void
e_table_memory_store_init (ETableMemoryStore *etms)
{
	etms->priv            = g_new (ETableMemoryStorePrivate, 1);

	etms->priv->col_count = 0;
	etms->priv->columns   = NULL;
	etms->priv->store     = NULL;
}

static void
e_table_memory_store_class_init (ETableMemoryStoreClass *klass)
{
	ETableModelClass *model_class = E_TABLE_MODEL_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	      = etms_finalize;

	model_class->column_count     = etms_column_count;
	model_class->value_at         = etms_value_at;
	model_class->set_value_at     = etms_set_value_at;
	model_class->is_cell_editable = etms_is_cell_editable;
	model_class->duplicate_value  = etms_duplicate_value;
	model_class->free_value       = etms_free_value;
	model_class->initialize_value = etms_initialize_value;
	model_class->value_is_empty   = etms_value_is_empty;
	model_class->value_to_string  = etms_value_to_string;
	model_class->append_row       = etms_append_row;
}

/**
 * e_table_memory_store_new:
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
 * This initializes a new ETableMemoryStoreModel object.  ETableMemoryStoreModel is
 * an implementaiton of the abstract class ETableModel.  The ETableMemoryStoreModel
 * is designed to allow people to easily create ETableModels without having
 * to create a new GType derived from ETableModel every time they need one.
 *
 * Instead, ETableMemoryStoreModel uses a setup based in callback functions, every
 * callback function signature mimics the signature of each ETableModel method
 * and passes the extra @data pointer to each one of the method to provide them
 * with any context they might want to use.
 *
 * Returns: An ETableMemoryStoreModel object (which is also an ETableModel
 * object).
 */
ETableModel *
e_table_memory_store_new (ETableMemoryStoreColumnInfo *columns)
{
	ETableMemoryStore *et = g_object_new (E_TABLE_MEMORY_STORE_TYPE, NULL);

	if (e_table_memory_store_construct (et, columns)) {
		return (ETableModel *) et;
	} else {
		g_object_unref (et);
		return NULL;
	}
}

ETableModel *
e_table_memory_store_construct (ETableMemoryStore *etms, ETableMemoryStoreColumnInfo *columns)
{
	gint i;
	for (i = 0; columns[i].type != E_TABLE_MEMORY_STORE_COLUMN_TYPE_TERMINATOR; i++)
		/* Intentionally blank */;
	etms->priv->col_count = i;

	etms->priv->columns = g_new (ETableMemoryStoreColumnInfo, etms->priv->col_count + 1);

	memcpy (etms->priv->columns, columns, (etms->priv->col_count + 1) * sizeof (ETableMemoryStoreColumnInfo));

	return E_TABLE_MODEL (etms);
}

void
e_table_memory_store_adopt_value_at (ETableMemoryStore *etms, gint col, gint row, gpointer value)
{
	e_table_model_pre_change (E_TABLE_MODEL (etms));

	STORE_LOCATOR (etms, col, row) = value;

	e_table_model_cell_changed (E_TABLE_MODEL (etms), col, row);
}

/* The size of these arrays is the number of columns. */
void
e_table_memory_store_insert_array (ETableMemoryStore *etms, gint row, gpointer *store, gpointer data)
{
	gint row_count;
	gint i;

	row_count = e_table_model_row_count (E_TABLE_MODEL (etms)) + 1;
	if (row == -1)
		row = row_count - 1;
	etms->priv->store = g_realloc (etms->priv->store, etms->priv->col_count * row_count * sizeof (gpointer));
	memmove (etms->priv->store + etms->priv->col_count * (row + 1),
		 etms->priv->store + etms->priv->col_count * row,
		 etms->priv->col_count * (row_count - row - 1) * sizeof (gpointer));

	for (i = 0; i < etms->priv->col_count; i++) {
		STORE_LOCATOR (etms, i, row) = duplicate_value (etms, i, store[i]);
	}

	e_table_memory_insert (E_TABLE_MEMORY (etms), row, data);
}

void
e_table_memory_store_insert (ETableMemoryStore *etms, gint row, gpointer data, ...)
{
	gpointer *store;
	va_list args;
	gint i;

	store = g_new (gpointer , etms->priv->col_count + 1);

	va_start (args, data);
	for (i = 0; i < etms->priv->col_count; i++) {
		store[i] = va_arg (args, gpointer );
	}
	va_end (args);

	e_table_memory_store_insert_array (etms, row, store, data);

	g_free (store);
}

void
e_table_memory_store_insert_adopt_array (ETableMemoryStore *etms, gint row, gpointer *store, gpointer data)
{
	gint row_count;
	gint i;

	row_count = e_table_model_row_count (E_TABLE_MODEL (etms)) + 1;
	if (row == -1)
		row = row_count - 1;
	etms->priv->store = g_realloc (etms->priv->store, etms->priv->col_count * row_count * sizeof (gpointer));
	memmove (etms->priv->store + etms->priv->col_count * (row + 1),
		 etms->priv->store + etms->priv->col_count * row,
		 etms->priv->col_count * (row_count - row - 1) * sizeof (gpointer));

	for (i = 0; i < etms->priv->col_count; i++) {
		STORE_LOCATOR (etms, i, row) = store[i];
	}

	e_table_memory_insert (E_TABLE_MEMORY (etms), row, data);
}

void
e_table_memory_store_insert_adopt (ETableMemoryStore *etms, gint row, gpointer data, ...)
{
	gpointer *store;
	va_list args;
	gint i;

	store = g_new (gpointer , etms->priv->col_count + 1);

	va_start (args, data);
	for (i = 0; i < etms->priv->col_count; i++) {
		store[i] = va_arg (args, gpointer );
	}
	va_end (args);

	e_table_memory_store_insert_adopt_array (etms, row, store, data);

	g_free (store);
}

/**
 * e_table_memory_store_change_array:
 * @etms: the ETabelMemoryStore.
 * @row:  the row we're changing.
 * @store: an array of new values to fill the row
 * @data: the new closure to associate with this row.
 *
 * frees existing values associated with a row and replaces them with
 * duplicates of the values in store.
 *
 */
void
e_table_memory_store_change_array (ETableMemoryStore *etms, gint row, gpointer *store, gpointer data)
{
	gint i;

	g_return_if_fail (row >= 0 && row < e_table_model_row_count (E_TABLE_MODEL (etms)));

	e_table_model_pre_change (E_TABLE_MODEL (etms));

	for (i = 0; i < etms->priv->col_count; i++) {
		free_value (etms, i, STORE_LOCATOR (etms, i, row));
		STORE_LOCATOR (etms, i, row) = duplicate_value (etms, i, store[i]);
	}

	e_table_memory_set_data (E_TABLE_MEMORY (etms), row, data);
	e_table_model_row_changed (E_TABLE_MODEL (etms), row);
}

/**
 * e_table_memory_store_change:
 * @etms: the ETabelMemoryStore.
 * @row:  the row we're changing.
 * @data: the new closure to associate with this row.
 *
 * a varargs version of e_table_memory_store_change_array.  you must
 * pass in etms->col_count args.
 */
void
e_table_memory_store_change (ETableMemoryStore *etms, gint row, gpointer data, ...)
{
	gpointer *store;
	va_list args;
	gint i;

	g_return_if_fail (row >= 0 && row < e_table_model_row_count (E_TABLE_MODEL (etms)));

	store = g_new0 (gpointer , etms->priv->col_count + 1);

	va_start (args, data);
	for (i = 0; i < etms->priv->col_count; i++) {
		store[i] = va_arg (args, gpointer );
	}
	va_end (args);

	e_table_memory_store_change_array (etms, row, store, data);

	g_free (store);
}

/**
 * e_table_memory_store_change_adopt_array:
 * @etms: the ETableMemoryStore
 * @row: the row we're changing.
 * @store: an array of new values to fill the row
 * @data: the new closure to associate with this row.
 *
 * frees existing values for the row and stores the values from store
 * into it.  This function differs from
 * e_table_memory_storage_change_adopt_array in that it does not
 * duplicate the data.
 */
void
e_table_memory_store_change_adopt_array (ETableMemoryStore *etms, gint row, gpointer *store, gpointer data)
{
	gint i;

	g_return_if_fail (row >= 0 && row < e_table_model_row_count (E_TABLE_MODEL (etms)));

	for (i = 0; i < etms->priv->col_count; i++) {
		free_value (etms, i, STORE_LOCATOR (etms, i, row));
		STORE_LOCATOR (etms, i, row) = store[i];
	}

	e_table_memory_set_data (E_TABLE_MEMORY (etms), row, data);
	e_table_model_row_changed (E_TABLE_MODEL (etms), row);
}

/**
 * e_table_memory_store_change_adopt
 * @etms: the ETabelMemoryStore.
 * @row:  the row we're changing.
 * @data: the new closure to associate with this row.
 *
 * a varargs version of e_table_memory_store_change_adopt_array.  you
 * must pass in etms->col_count args.
 */
void
e_table_memory_store_change_adopt (ETableMemoryStore *etms, gint row, gpointer data, ...)
{
	gpointer *store;
	va_list args;
	gint i;

	g_return_if_fail (row >= 0 && row < e_table_model_row_count (E_TABLE_MODEL (etms)));

	store = g_new0 (gpointer , etms->priv->col_count + 1);

	va_start (args, data);
	for (i = 0; i < etms->priv->col_count; i++) {
		store[i] = va_arg (args, gpointer );
	}
	va_end (args);

	e_table_memory_store_change_adopt_array (etms, row, store, data);

	g_free (store);
}

void
e_table_memory_store_remove (ETableMemoryStore *etms, gint row)
{
	ETableModel *model;
	gint column_count, row_count;
	gint i;

	model = E_TABLE_MODEL (etms);
	column_count = e_table_model_column_count (model);

	for (i = 0; i < column_count; i++)
		e_table_model_free_value (model, i, e_table_model_value_at (model, i, row));

	row_count = e_table_model_row_count (E_TABLE_MODEL (etms)) - 1;
	memmove (etms->priv->store + etms->priv->col_count * row,
		 etms->priv->store + etms->priv->col_count * (row + 1),
		 etms->priv->col_count * (row_count - row) * sizeof (gpointer));
	etms->priv->store = g_realloc (etms->priv->store, etms->priv->col_count * row_count * sizeof (gpointer));

	e_table_memory_remove (E_TABLE_MEMORY (etms), row);
}

void
e_table_memory_store_clear (ETableMemoryStore *etms)
{
	ETableModel *model;
	gint row_count, column_count;
	gint i, j;

	model = E_TABLE_MODEL (etms);
	row_count = e_table_model_row_count (model);
	column_count = e_table_model_column_count (model);

	for (i = 0; i < row_count; i++) {
		for (j = 0; j < column_count; j++) {
			e_table_model_free_value (model, j, e_table_model_value_at (model, j, i));
		}
	}

	e_table_memory_clear (E_TABLE_MEMORY (etms));

	g_free (etms->priv->store);
	etms->priv->store = NULL;
}

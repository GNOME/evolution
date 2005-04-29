/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-subset.c - Implements a table that contains a subset of another table.
 * Copyright 1999, 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#include <stdlib.h>

#include "gal/util/e-util.h"

#include "e-table-subset.h"

static void etss_proxy_model_pre_change_real (ETableSubset *etss, ETableModel *etm);
static void etss_proxy_model_no_change_real (ETableSubset *etss, ETableModel *etm);
static void etss_proxy_model_changed_real (ETableSubset *etss, ETableModel *etm);
static void etss_proxy_model_row_changed_real (ETableSubset *etss, ETableModel *etm, int row);
static void etss_proxy_model_cell_changed_real (ETableSubset *etss, ETableModel *etm, int col, int row);
static void etss_proxy_model_rows_inserted_real (ETableSubset *etss, ETableModel *etm, int row, int count);
static void etss_proxy_model_rows_deleted_real (ETableSubset *etss, ETableModel *etm, int row, int count);

#define d(x)

static ETableModelClass *etss_parent_class;

#define ETSS_CLASS(object) (E_TABLE_SUBSET_GET_CLASS(object))

#define VALID_ROW(etss, row) (row >= -1 && row < etss->n_map)
#define MAP_ROW(etss, row) (row == -1 ? -1 : etss->map_table[row])

static gint
etss_get_view_row (ETableSubset *etss, int row)
{
	const int n = etss->n_map;
	const int * const map_table = etss->map_table;
	int i;

	int end = MIN(etss->n_map, etss->last_access + 10);
	int start = MAX(0, etss->last_access - 10);
	int initial = MAX (MIN (etss->last_access, end), start);

	for (i = initial; i < end; i++) {
		if (map_table [i] == row){
			d(g_print("a) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return i;
		}
	}

	for (i = initial - 1; i >= start; i--) {
		if (map_table [i] == row){
			d(g_print("b) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return i;
		}
	}

	for (i = 0; i < n; i++){
		if (map_table [i] == row){
			d(g_print("c) Found %d from %d\n", i, etss->last_access));
			etss->last_access = i;
			return i;
		}
	}
	return -1;
}

static void
etss_dispose (GObject *object)
{
	ETableSubset *etss = E_TABLE_SUBSET (object);

	if (etss->source) {
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_pre_change_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_no_change_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_row_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_cell_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_rows_inserted_id);
		g_signal_handler_disconnect (G_OBJECT (etss->source),
					     etss->table_model_rows_deleted_id);

		g_object_unref (etss->source);
		etss->source = NULL;

		etss->table_model_changed_id = 0;
		etss->table_model_row_changed_id = 0;
		etss->table_model_cell_changed_id = 0;
		etss->table_model_rows_inserted_id = 0;
		etss->table_model_rows_deleted_id = 0;
	}

	G_OBJECT_CLASS (etss_parent_class)->dispose (object);
}

static void
etss_finalize (GObject *object)
{
	ETableSubset *etss = E_TABLE_SUBSET (object);

	g_free (etss->map_table);
	etss->map_table = NULL;

	G_OBJECT_CLASS (etss_parent_class)->finalize (object);
}

static int
etss_column_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_column_count (etss->source);
}

static int
etss_row_count (ETableModel *etm)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return etss->n_map;
}

static void *
etss_value_at (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	g_return_val_if_fail (VALID_ROW (etss, row), NULL);

	etss->last_access = row;
	d(g_print("g) Setting last_access to %d\n", row));
	return e_table_model_value_at (etss->source, col, MAP_ROW(etss, row));
}

static void
etss_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETableSubset *etss = (ETableSubset *)etm;

	g_return_if_fail (VALID_ROW (etss, row));

	etss->last_access = row;
	d(g_print("h) Setting last_access to %d\n", row));
	e_table_model_set_value_at (etss->source, col, MAP_ROW(etss, row), val);
}

static gboolean
etss_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	g_return_val_if_fail (VALID_ROW (etss, row), FALSE);

	return e_table_model_is_cell_editable (etss->source, col, MAP_ROW(etss, row));
}

static gboolean
etss_has_save_id (ETableModel *etm)
{
	return TRUE;
}

static char *
etss_get_save_id (ETableModel *etm, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;

	g_return_val_if_fail (VALID_ROW (etss, row), NULL);

	if (e_table_model_has_save_id (etss->source))
		return e_table_model_get_save_id (etss->source, MAP_ROW(etss, row));
	else
		return g_strdup_printf ("%d", MAP_ROW(etss, row));
}

static void
etss_append_row (ETableModel *etm, ETableModel *source, int row)
{
	ETableSubset *etss = (ETableSubset *)etm;
	e_table_model_append_row (etss->source, source, row);
}

static void *
etss_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_duplicate_value (etss->source, col, value);
}

static void
etss_free_value (ETableModel *etm, int col, void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	e_table_model_free_value (etss->source, col, value);
}

static void *
etss_initialize_value (ETableModel *etm, int col)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_initialize_value (etss->source, col);
}

static gboolean
etss_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_value_is_empty (etss->source, col, value);
}

static char *
etss_value_to_string (ETableModel *etm, int col, const void *value)
{
	ETableSubset *etss = (ETableSubset *)etm;

	return e_table_model_value_to_string (etss->source, col, value);
}

static void
etss_class_init (GObjectClass *object_class)
{
	ETableSubsetClass *klass         = (ETableSubsetClass *) object_class;
	ETableModelClass *table_class    = (ETableModelClass *) object_class;

	etss_parent_class                = g_type_class_peek_parent (klass);
	
	object_class->dispose            = etss_dispose;
	object_class->finalize           = etss_finalize;

	table_class->column_count        = etss_column_count;
	table_class->row_count           = etss_row_count;
	table_class->append_row          = etss_append_row;

	table_class->value_at            = etss_value_at;
	table_class->set_value_at        = etss_set_value_at;
	table_class->is_cell_editable    = etss_is_cell_editable;

	table_class->has_save_id         = etss_has_save_id;
	table_class->get_save_id         = etss_get_save_id;

	table_class->duplicate_value     = etss_duplicate_value;
	table_class->free_value          = etss_free_value;
	table_class->initialize_value    = etss_initialize_value;
	table_class->value_is_empty      = etss_value_is_empty;
	table_class->value_to_string     = etss_value_to_string;

	klass->proxy_model_pre_change    = etss_proxy_model_pre_change_real;
	klass->proxy_model_no_change     = etss_proxy_model_no_change_real;
	klass->proxy_model_changed       = etss_proxy_model_changed_real;
	klass->proxy_model_row_changed   = etss_proxy_model_row_changed_real;
	klass->proxy_model_cell_changed  = etss_proxy_model_cell_changed_real;
	klass->proxy_model_rows_inserted = etss_proxy_model_rows_inserted_real;
	klass->proxy_model_rows_deleted  = etss_proxy_model_rows_deleted_real;
}

static void
etss_init (ETableSubset *etss)
{
	etss->last_access = 0;
}

E_MAKE_TYPE(e_table_subset, "ETableSubset", ETableSubset, etss_class_init, etss_init, E_TABLE_MODEL_TYPE)

static void
etss_proxy_model_pre_change_real (ETableSubset *etss, ETableModel *etm)
{
	e_table_model_pre_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_no_change_real (ETableSubset *etss, ETableModel *etm)
{
	e_table_model_no_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_changed_real (ETableSubset *etss, ETableModel *etm)
{
	e_table_model_changed (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_row_changed_real (ETableSubset *etss, ETableModel *etm, int row)
{
	int view_row = etss_get_view_row (etss, row);
	if (view_row != -1)
		e_table_model_row_changed (E_TABLE_MODEL (etss), view_row);
	else
		e_table_model_no_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_cell_changed_real (ETableSubset *etss, ETableModel *etm, int col, int row)
{
	int view_row = etss_get_view_row (etss, row);
	if (view_row != -1)
		e_table_model_cell_changed (E_TABLE_MODEL (etss), col, view_row);
	else
		e_table_model_no_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_rows_inserted_real (ETableSubset *etss, ETableModel *etm, int row, int count)
{
	e_table_model_no_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_rows_deleted_real (ETableSubset *etss, ETableModel *etm, int row, int count)
{
	e_table_model_no_change (E_TABLE_MODEL (etss));
}

static void
etss_proxy_model_pre_change (ETableModel *etm, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_pre_change)
		(ETSS_CLASS(etss)->proxy_model_pre_change) (etss, etm);
}

static void
etss_proxy_model_no_change (ETableModel *etm, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_no_change)
		(ETSS_CLASS(etss)->proxy_model_no_change) (etss, etm);
}

static void
etss_proxy_model_changed (ETableModel *etm, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_changed)
		(ETSS_CLASS(etss)->proxy_model_changed) (etss, etm);
}

static void
etss_proxy_model_row_changed (ETableModel *etm, int row, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_row_changed)
		(ETSS_CLASS(etss)->proxy_model_row_changed) (etss, etm, row);
}

static void
etss_proxy_model_cell_changed (ETableModel *etm, int row, int col, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_cell_changed)
		(ETSS_CLASS(etss)->proxy_model_cell_changed) (etss, etm, col, row);
}

static void
etss_proxy_model_rows_inserted (ETableModel *etm, int row, int col, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_rows_inserted)
		(ETSS_CLASS(etss)->proxy_model_rows_inserted) (etss, etm, row, col);
}

static void
etss_proxy_model_rows_deleted (ETableModel *etm, int row, int col, ETableSubset *etss)
{
	if (ETSS_CLASS(etss)->proxy_model_rows_deleted)
		(ETSS_CLASS(etss)->proxy_model_rows_deleted) (etss, etm, row, col);
}

ETableModel *
e_table_subset_construct (ETableSubset *etss, ETableModel *source, int nvals)
{
	unsigned int *buffer;
	int i;

	if (nvals) {
		buffer = (unsigned int *) g_malloc (sizeof (unsigned int) * nvals);
		if (buffer == NULL)
			return NULL;
	} else
		buffer = NULL;
	etss->map_table = buffer;
	etss->n_map = nvals;
	etss->source = source;
	g_object_ref (source);
	
	/* Init */
	for (i = 0; i < nvals; i++)
		etss->map_table [i] = i;

	etss->table_model_pre_change_id    = g_signal_connect (G_OBJECT (source), "model_pre_change",
							G_CALLBACK (etss_proxy_model_pre_change), etss);
	etss->table_model_no_change_id     = g_signal_connect (G_OBJECT (source), "model_no_change",
							G_CALLBACK (etss_proxy_model_no_change), etss);
	etss->table_model_changed_id       = g_signal_connect (G_OBJECT (source), "model_changed",
							G_CALLBACK (etss_proxy_model_changed), etss);
	etss->table_model_row_changed_id   = g_signal_connect (G_OBJECT (source), "model_row_changed",
							G_CALLBACK (etss_proxy_model_row_changed), etss);
	etss->table_model_cell_changed_id  = g_signal_connect (G_OBJECT (source), "model_cell_changed",
							G_CALLBACK (etss_proxy_model_cell_changed), etss);
	etss->table_model_rows_inserted_id = g_signal_connect (G_OBJECT (source), "model_rows_inserted",
							G_CALLBACK (etss_proxy_model_rows_inserted), etss);
	etss->table_model_rows_deleted_id  = g_signal_connect (G_OBJECT (source), "model_rows_deleted",
							G_CALLBACK (etss_proxy_model_rows_deleted), etss);
	
	return E_TABLE_MODEL (etss);
}

ETableModel *
e_table_subset_new (ETableModel *source, const int nvals)
{
	ETableSubset *etss = g_object_new (E_TABLE_SUBSET_TYPE, NULL);

	if (e_table_subset_construct (etss, source, nvals) == NULL){
		g_object_unref (etss);
		return NULL;
	}

	return (ETableModel *) etss;
}

int
e_table_subset_model_to_view_row  (ETableSubset *ets,
				   int           model_row)
{
	int i;
	for (i = 0; i < ets->n_map; i++) {
		if (ets->map_table[i] == model_row)
			return i;
	}
	return -1;
}

int
e_table_subset_view_to_model_row  (ETableSubset *ets,
				   int           view_row)
{
	if (view_row >= 0 && view_row < ets->n_map)
		return ets->map_table[view_row];
	else
		return -1;
}

ETableModel *
e_table_subset_get_toplevel (ETableSubset *table)
{
	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (E_IS_TABLE_SUBSET (table), NULL);

	if (E_IS_TABLE_SUBSET (table->source))
		return e_table_subset_get_toplevel (E_TABLE_SUBSET (table->source));
	else
		return table->source;
}

void
e_table_subset_print_debugging  (ETableSubset *table_model)
{
	int i;
	for (i = 0; i < table_model->n_map; i++) {
		g_print("%8d\n", table_model->map_table[i]);
	}
}

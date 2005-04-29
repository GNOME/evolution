/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-sorter.c
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

#include <stdlib.h>
#include <string.h>

#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"

#include "e-table-sorter.h"

#define d(x)

/* The arguments we take */
enum {
	PROP_0,
	PROP_SORT_INFO
};

#define PARENT_TYPE e_sorter_get_type()

#define INCREMENT_AMOUNT 100

static ESorterClass *parent_class;

static void    	ets_model_changed      (ETableModel *etm, ETableSorter *ets);
static void    	ets_model_row_changed  (ETableModel *etm, int row, ETableSorter *ets);
static void    	ets_model_cell_changed (ETableModel *etm, int col, int row, ETableSorter *ets);
static void    	ets_model_rows_inserted (ETableModel *etm, int row, int count, ETableSorter *ets);
static void    	ets_model_rows_deleted (ETableModel *etm, int row, int count, ETableSorter *ets);
static void    	ets_sort_info_changed  (ETableSortInfo *info, ETableSorter *ets);
static void    	ets_clean              (ETableSorter *ets);
static void    	ets_sort               (ETableSorter *ets);
static void    	ets_backsort           (ETableSorter *ets);

static gint    	ets_model_to_sorted           (ESorter *sorter, int row);
static gint    	ets_sorted_to_model           (ESorter *sorter, int row);
static void    	ets_get_model_to_sorted_array (ESorter *sorter, int **array, int *count);
static void    	ets_get_sorted_to_model_array (ESorter *sorter, int **array, int *count);
static gboolean ets_needs_sorting             (ESorter *ets);

static void
ets_dispose (GObject *object)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	if (ets->sort_info) {
		if (ets->table_model_changed_id)
			g_signal_handler_disconnect (ets->source,
						     ets->table_model_changed_id);
		if (ets->table_model_row_changed_id)
			g_signal_handler_disconnect (ets->source,
						     ets->table_model_row_changed_id);
		if (ets->table_model_cell_changed_id)
			g_signal_handler_disconnect (ets->source,
						     ets->table_model_cell_changed_id);
		if (ets->table_model_rows_inserted_id)
			g_signal_handler_disconnect (ets->source,
						     ets->table_model_rows_inserted_id);
		if (ets->table_model_rows_deleted_id)
			g_signal_handler_disconnect (ets->source,
						     ets->table_model_rows_deleted_id);
		if (ets->sort_info_changed_id)
			g_signal_handler_disconnect (ets->sort_info,
						     ets->sort_info_changed_id);
		if (ets->group_info_changed_id)
			g_signal_handler_disconnect (ets->sort_info,
						     ets->group_info_changed_id);

		ets->table_model_changed_id = 0;
		ets->table_model_row_changed_id = 0;
		ets->table_model_cell_changed_id = 0;
		ets->table_model_rows_inserted_id = 0;
		ets->table_model_rows_deleted_id = 0;
		ets->sort_info_changed_id = 0;
		ets->group_info_changed_id = 0;

		g_object_unref(ets->sort_info);
		ets->sort_info = NULL;
	}

	if (ets->full_header)
		g_object_unref(ets->full_header);
	ets->full_header = NULL;

	if (ets->source)
		g_object_unref(ets->source);
	ets->source = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ets_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	switch (prop_id) {
	case PROP_SORT_INFO:
		if (ets->sort_info) {
			if (ets->sort_info_changed_id)
				g_signal_handler_disconnect(ets->sort_info, ets->sort_info_changed_id);
			if (ets->group_info_changed_id)
				g_signal_handler_disconnect(ets->sort_info, ets->group_info_changed_id);
			g_object_unref(ets->sort_info);
		}

		ets->sort_info = E_TABLE_SORT_INFO(g_value_get_object (value));
		g_object_ref(ets->sort_info);
		ets->sort_info_changed_id = g_signal_connect (ets->sort_info, "sort_info_changed",
							      G_CALLBACK (ets_sort_info_changed), ets);
		ets->group_info_changed_id = g_signal_connect (ets->sort_info, "group_info_changed",
							       G_CALLBACK (ets_sort_info_changed), ets);

		ets_clean (ets);
		break;
	default:
		break;
	}
}

static void
ets_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETableSorter *ets = E_TABLE_SORTER (object);
	switch (prop_id) {
	case PROP_SORT_INFO:
		g_value_set_object (value, ets->sort_info);
		break;
	}
}

static void
ets_class_init (ETableSorterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	ESorterClass *sorter_class = E_SORTER_CLASS(klass);

	parent_class                            = g_type_class_peek_parent (klass);

	object_class->dispose                   = ets_dispose;
	object_class->set_property              = ets_set_property;
	object_class->get_property              = ets_get_property;

	sorter_class->model_to_sorted           = ets_model_to_sorted           ;
	sorter_class->sorted_to_model           = ets_sorted_to_model           ;
	sorter_class->get_model_to_sorted_array = ets_get_model_to_sorted_array ;
	sorter_class->get_sorted_to_model_array = ets_get_sorted_to_model_array ;		
	sorter_class->needs_sorting             = ets_needs_sorting             ;

	g_object_class_install_property (object_class, PROP_SORT_INFO, 
					 g_param_spec_object ("sort_info",
							      _("Sort Info"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TABLE_SORT_INFO_TYPE,
							      G_PARAM_READWRITE));
}

static void
ets_init (ETableSorter *ets)
{
	ets->full_header = NULL;
	ets->sort_info = NULL;
	ets->source = NULL;

	ets->needs_sorting = -1;

	ets->table_model_changed_id = 0;
	ets->table_model_row_changed_id = 0;
	ets->table_model_cell_changed_id = 0;
	ets->table_model_rows_inserted_id = 0;
	ets->table_model_rows_deleted_id = 0;
	ets->sort_info_changed_id = 0;
	ets->group_info_changed_id = 0;
}

E_MAKE_TYPE(e_table_sorter, "ETableSorter", ETableSorter, ets_class_init, ets_init, PARENT_TYPE)

ETableSorter *
e_table_sorter_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorter *ets = g_object_new (E_TABLE_SORTER_TYPE, NULL);
	
	ets->sort_info = sort_info;
	g_object_ref(ets->sort_info);
	ets->full_header = full_header;
	g_object_ref(ets->full_header);
	ets->source = source;
	g_object_ref(ets->source);

	ets->table_model_changed_id = g_signal_connect (source, "model_changed",
							G_CALLBACK (ets_model_changed), ets);
	ets->table_model_row_changed_id = g_signal_connect (source, "model_row_changed",
							G_CALLBACK (ets_model_row_changed), ets);
	ets->table_model_cell_changed_id = g_signal_connect (source, "model_cell_changed",
						        G_CALLBACK (ets_model_cell_changed), ets);
	ets->table_model_rows_inserted_id = g_signal_connect (source, "model_rows_inserted",
							G_CALLBACK (ets_model_rows_inserted), ets);
	ets->table_model_rows_deleted_id = g_signal_connect (source, "model_rows_deleted",
							G_CALLBACK (ets_model_rows_deleted), ets);
	ets->sort_info_changed_id = g_signal_connect (sort_info, "sort_info_changed",
							G_CALLBACK (ets_sort_info_changed), ets);
	ets->group_info_changed_id = g_signal_connect (sort_info, "group_info_changed",
							G_CALLBACK (ets_sort_info_changed), ets);
	
	return ets;
}

static void
ets_model_changed (ETableModel *etm, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_row_changed (ETableModel *etm, int row, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_cell_changed (ETableModel *etm, int col, int row, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_rows_inserted (ETableModel *etm, int row, int count, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_model_rows_deleted (ETableModel *etm, int row, int count, ETableSorter *ets)
{
	ets_clean(ets);
}

static void
ets_sort_info_changed (ETableSortInfo *info, ETableSorter *ets)
{
	d(g_print ("sort info changed\n"));
	ets_clean(ets);
}

static ETableSorter *ets_closure;
static void **vals_closure;
static int cols_closure;
static int *ascending_closure;
static GCompareFunc *compare_closure;

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static int
qsort_callback(const void *data1, const void *data2)
{
	gint row1 = *(int *)data1;
	gint row2 = *(int *)data2;
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(ets_closure->sort_info) + e_table_sort_info_grouping_get_count(ets_closure->sort_info);
	int comp_val = 0;
	int ascending = 1;
	for (j = 0; j < sort_count; j++) {
		comp_val = (*(compare_closure[j]))(vals_closure[cols_closure * row1 + j], vals_closure[cols_closure * row2 + j]);
		ascending = ascending_closure[j];
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}

static void
ets_clean(ETableSorter *ets)
{
	g_free(ets->sorted);
	ets->sorted = NULL;

	g_free(ets->backsorted);
	ets->backsorted = NULL;

	ets->needs_sorting = -1;
}


static void
ets_sort(ETableSorter *ets)
{
	int rows;
	int i;
	int j;
	int cols;
	int group_cols;

	if (ets->sorted)
		return;

	rows = e_table_model_row_count(ets->source);
	group_cols = e_table_sort_info_grouping_get_count(ets->sort_info);
	cols = e_table_sort_info_sorting_get_count(ets->sort_info) + group_cols;

	ets->sorted = g_new(int, rows);
	for (i = 0; i < rows; i++)
		ets->sorted[i] = i;

	cols_closure = cols;
	ets_closure = ets;

	vals_closure = g_new(void *, rows * cols);
	ascending_closure = g_new(int, cols);
	compare_closure = g_new(GCompareFunc, cols);

	for (j = 0; j < cols; j++) {
		ETableSortColumn column;
		ETableCol *col;

		if (j < group_cols)
			column = e_table_sort_info_grouping_get_nth(ets->sort_info, j);
		else
			column = e_table_sort_info_sorting_get_nth(ets->sort_info, j - group_cols);

		col = e_table_header_get_column_by_col_idx(ets->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (ets->full_header, e_table_header_count (ets->full_header) - 1);

		for (i = 0; i < rows; i++) {
			vals_closure[i * cols + j] = e_table_model_value_at (ets->source, col->col_idx, i);
		}

		compare_closure[j] = col->compare;
		ascending_closure[j] = column.ascending;
	}

		qsort(ets->sorted, rows, sizeof(int), qsort_callback);

	g_free(vals_closure);
	g_free(ascending_closure);
	g_free(compare_closure);
}

static void
ets_backsort(ETableSorter *ets)
{
	int i, rows;

	if (ets->backsorted)
		return;

	ets_sort(ets);

	rows = e_table_model_row_count(ets->source);
	ets->backsorted = g_new0(int, rows);

	for (i = 0; i < rows; i++) {
		ets->backsorted[ets->sorted[i]] = i;
	}
}


static gint
ets_model_to_sorted (ESorter *es, int row)
{
	ETableSorter *ets = E_TABLE_SORTER(es);
	int rows = e_table_model_row_count(ets->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (ets_needs_sorting(es))
		ets_backsort(ets);

	if (ets->backsorted)
		return ets->backsorted[row];
	else
		return row;
}

static gint
ets_sorted_to_model (ESorter *es, int row)
{
	ETableSorter *ets = E_TABLE_SORTER(es);
	int rows = e_table_model_row_count(ets->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (ets_needs_sorting(es))
		ets_sort(ets);

	if (ets->sorted)
		return ets->sorted[row];
	else
		return row;
}

static void
ets_get_model_to_sorted_array (ESorter *es, int **array, int *count)
{
	ETableSorter *ets = E_TABLE_SORTER(es);
	if (array || count) {
		ets_backsort(ets);

		if (array)
			*array = ets->backsorted;
		if (count)
			*count = e_table_model_row_count(ets->source);
	}
}

static void
ets_get_sorted_to_model_array (ESorter *es, int **array, int *count)
{
	ETableSorter *ets = E_TABLE_SORTER(es);
	if (array || count) {
		ets_sort(ets);

		if (array)
			*array = ets->sorted;
		if (count)
			*count = e_table_model_row_count(ets->source);
	}
}


static gboolean
ets_needs_sorting(ESorter *es)
{
	ETableSorter *ets = E_TABLE_SORTER(es);
	if (ets->needs_sorting < 0) {
		if (e_table_sort_info_sorting_get_count(ets->sort_info) + e_table_sort_info_grouping_get_count(ets->sort_info))
			ets->needs_sorting = 1;
		else
			ets->needs_sorting = 0;
	}
	return ets->needs_sorting;
}

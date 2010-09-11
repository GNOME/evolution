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

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "e-table-sorter.h"
#include "e-table-sorting-utils.h"

#define d(x)

/* The arguments we take */
enum {
	PROP_0,
	PROP_SORT_INFO
};

/* workaround for avoiding API breakage */
#define ets_get_type e_table_sorter_get_type
G_DEFINE_TYPE (ETableSorter, ets, E_SORTER_TYPE)

#define INCREMENT_AMOUNT 100

static void	ets_model_changed      (ETableModel *etm, ETableSorter *ets);
static void	ets_model_row_changed  (ETableModel *etm, gint row, ETableSorter *ets);
static void	ets_model_cell_changed (ETableModel *etm, gint col, gint row, ETableSorter *ets);
static void	ets_model_rows_inserted (ETableModel *etm, gint row, gint count, ETableSorter *ets);
static void	ets_model_rows_deleted (ETableModel *etm, gint row, gint count, ETableSorter *ets);
static void	ets_sort_info_changed  (ETableSortInfo *info, ETableSorter *ets);
static void	ets_clean              (ETableSorter *ets);
static void	ets_sort               (ETableSorter *ets);
static void	ets_backsort           (ETableSorter *ets);

static gint	ets_model_to_sorted           (ESorter *sorter, gint row);
static gint	ets_sorted_to_model           (ESorter *sorter, gint row);
static void	ets_get_model_to_sorted_array (ESorter *sorter, gint **array, gint *count);
static void	ets_get_sorted_to_model_array (ESorter *sorter, gint **array, gint *count);
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

		g_object_unref (ets->sort_info);
		ets->sort_info = NULL;
	}

	if (ets->full_header)
		g_object_unref (ets->full_header);
	ets->full_header = NULL;

	if (ets->source)
		g_object_unref (ets->source);
	ets->source = NULL;

	G_OBJECT_CLASS (ets_parent_class)->dispose (object);
}

static void
ets_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	switch (prop_id) {
	case PROP_SORT_INFO:
		if (ets->sort_info) {
			if (ets->sort_info_changed_id)
				g_signal_handler_disconnect (ets->sort_info, ets->sort_info_changed_id);
			if (ets->group_info_changed_id)
				g_signal_handler_disconnect (ets->sort_info, ets->group_info_changed_id);
			g_object_unref (ets->sort_info);
		}

		ets->sort_info = E_TABLE_SORT_INFO (g_value_get_object (value));
		g_object_ref (ets->sort_info);
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
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ESorterClass *sorter_class = E_SORTER_CLASS (klass);

	object_class->dispose                   = ets_dispose;
	object_class->set_property              = ets_set_property;
	object_class->get_property              = ets_get_property;

	sorter_class->model_to_sorted           = ets_model_to_sorted;
	sorter_class->sorted_to_model           = ets_sorted_to_model;
	sorter_class->get_model_to_sorted_array = ets_get_model_to_sorted_array;
	sorter_class->get_sorted_to_model_array = ets_get_sorted_to_model_array;
	sorter_class->needs_sorting             = ets_needs_sorting;

	g_object_class_install_property (object_class, PROP_SORT_INFO,
					 g_param_spec_object ("sort_info",
							      "Sort Info",
							      NULL,
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

ETableSorter *
e_table_sorter_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorter *ets = g_object_new (E_TABLE_SORTER_TYPE, NULL);

	ets->sort_info = sort_info;
	g_object_ref (ets->sort_info);
	ets->full_header = full_header;
	g_object_ref (ets->full_header);
	ets->source = source;
	g_object_ref (ets->source);

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
	ets_clean (ets);
}

static void
ets_model_row_changed (ETableModel *etm, gint row, ETableSorter *ets)
{
	ets_clean (ets);
}

static void
ets_model_cell_changed (ETableModel *etm, gint col, gint row, ETableSorter *ets)
{
	ets_clean (ets);
}

static void
ets_model_rows_inserted (ETableModel *etm, gint row, gint count, ETableSorter *ets)
{
	ets_clean (ets);
}

static void
ets_model_rows_deleted (ETableModel *etm, gint row, gint count, ETableSorter *ets)
{
	ets_clean (ets);
}

static void
ets_sort_info_changed (ETableSortInfo *info, ETableSorter *ets)
{
	d(g_print ("sort info changed\n"));
	ets_clean (ets);
}

struct qsort_data {
	ETableSorter *ets;
	gpointer *vals;
	gint cols;
	gint *ascending;
	GCompareDataFunc *compare;
	gpointer cmp_cache;
};

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static gint
qsort_callback (gconstpointer data1, gconstpointer data2, gpointer user_data)
{
	struct qsort_data *qd = (struct qsort_data *) user_data;
	gint row1 = *(gint *)data1;
	gint row2 = *(gint *)data2;
	gint j;
	gint sort_count = e_table_sort_info_sorting_get_count (qd->ets->sort_info) + e_table_sort_info_grouping_get_count (qd->ets->sort_info);
	gint comp_val = 0;
	gint ascending = 1;
	for (j = 0; j < sort_count; j++) {
		comp_val = (*(qd->compare[j]))(qd->vals[qd->cols * row1 + j], qd->vals[qd->cols * row2 + j], qd->cmp_cache);
		ascending = qd->ascending[j];
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
ets_clean (ETableSorter *ets)
{
	g_free (ets->sorted);
	ets->sorted = NULL;

	g_free (ets->backsorted);
	ets->backsorted = NULL;

	ets->needs_sorting = -1;
}

static void
ets_sort (ETableSorter *ets)
{
	gint rows;
	gint i;
	gint j;
	gint cols;
	gint group_cols;
	struct qsort_data qd;

	if (ets->sorted)
		return;

	rows = e_table_model_row_count (ets->source);
	group_cols = e_table_sort_info_grouping_get_count (ets->sort_info);
	cols = e_table_sort_info_sorting_get_count (ets->sort_info) + group_cols;

	ets->sorted = g_new (int, rows);
	for (i = 0; i < rows; i++)
		ets->sorted[i] = i;

	qd.cols = cols;
	qd.ets = ets;

	qd.vals = g_new (gpointer , rows * cols);
	qd.ascending = g_new (int, cols);
	qd.compare = g_new (GCompareDataFunc, cols);
	qd.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	for (j = 0; j < cols; j++) {
		ETableSortColumn column;
		ETableCol *col;

		if (j < group_cols)
			column = e_table_sort_info_grouping_get_nth (ets->sort_info, j);
		else
			column = e_table_sort_info_sorting_get_nth (ets->sort_info, j - group_cols);

		col = e_table_header_get_column_by_col_idx (ets->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (ets->full_header, e_table_header_count (ets->full_header) - 1);

		for (i = 0; i < rows; i++) {
			qd.vals[i * cols + j] = e_table_model_value_at (ets->source, col->col_idx, i);
		}

		qd.compare[j] = col->compare;
		qd.ascending[j] = column.ascending;
	}

	g_qsort_with_data (ets->sorted, rows, sizeof (gint), qsort_callback, &qd);

	g_free (qd.vals);
	g_free (qd.ascending);
	g_free (qd.compare);
	e_table_sorting_utils_free_cmp_cache (qd.cmp_cache);
}

static void
ets_backsort (ETableSorter *ets)
{
	gint i, rows;

	if (ets->backsorted)
		return;

	ets_sort (ets);

	rows = e_table_model_row_count (ets->source);
	ets->backsorted = g_new0 (int, rows);

	for (i = 0; i < rows; i++) {
		ets->backsorted[ets->sorted[i]] = i;
	}
}

static gint
ets_model_to_sorted (ESorter *es, gint row)
{
	ETableSorter *ets = E_TABLE_SORTER (es);
	gint rows = e_table_model_row_count (ets->source);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < rows, -1);

	if (ets_needs_sorting (es))
		ets_backsort (ets);

	if (ets->backsorted)
		return ets->backsorted[row];
	else
		return row;
}

static gint
ets_sorted_to_model (ESorter *es, gint row)
{
	ETableSorter *ets = E_TABLE_SORTER (es);
	gint rows = e_table_model_row_count (ets->source);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < rows, -1);

	if (ets_needs_sorting (es))
		ets_sort (ets);

	if (ets->sorted)
		return ets->sorted[row];
	else
		return row;
}

static void
ets_get_model_to_sorted_array (ESorter *es, gint **array, gint *count)
{
	ETableSorter *ets = E_TABLE_SORTER (es);
	if (array || count) {
		ets_backsort (ets);

		if (array)
			*array = ets->backsorted;
		if (count)
			*count = e_table_model_row_count(ets->source);
	}
}

static void
ets_get_sorted_to_model_array (ESorter *es, gint **array, gint *count)
{
	ETableSorter *ets = E_TABLE_SORTER (es);
	if (array || count) {
		ets_sort (ets);

		if (array)
			*array = ets->sorted;
		if (count)
			*count = e_table_model_row_count(ets->source);
	}
}

static gboolean
ets_needs_sorting (ESorter *es)
{
	ETableSorter *ets = E_TABLE_SORTER (es);
	if (ets->needs_sorting < 0) {
		if (e_table_sort_info_sorting_get_count (ets->sort_info) + e_table_sort_info_grouping_get_count (ets->sort_info))
			ets->needs_sorting = 1;
		else
			ets->needs_sorting = 0;
	}
	return ets->needs_sorting;
}

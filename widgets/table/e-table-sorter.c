/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-sorted.c: Implements a table that sorts another table
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "e-util/e-util.h"
#include "e-table-sorter.h"

#define PARENT_TYPE gtk_object_get_type()

#define INCREMENT_AMOUNT 100

static GtkObjectClass *parent_class;

static void ets_model_changed      (ETableModel *etm, ETableSorter *ets);
static void ets_model_row_changed  (ETableModel *etm, int row, ETableSorter *ets);
static void ets_model_cell_changed (ETableModel *etm, int col, int row, ETableSorter *ets);
static void ets_sort_info_changed  (ETableSortInfo *info, ETableSorter *ets);
static void ets_clean              (ETableSorter *ets);
static void ets_sort               (ETableSorter *ets);
static void ets_backsort           (ETableSorter *ets);

static void
ets_destroy (GtkObject *object)
{
	ETableSorter *ets = E_TABLE_SORTER (object);

	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_row_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->source),
			       ets->table_model_cell_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (ets->sort_info),
			       ets->sort_info_changed_id);

	ets->table_model_changed_id = 0;
	ets->table_model_row_changed_id = 0;
	ets->table_model_cell_changed_id = 0;
	ets->sort_info_changed_id = 0;
	
	if (ets->sort_info)
		gtk_object_unref(GTK_OBJECT(ets->sort_info));
	if (ets->full_header)
		gtk_object_unref(GTK_OBJECT(ets->full_header));
	if (ets->source)
		gtk_object_unref(GTK_OBJECT(ets->source));

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
ets_class_init (ETableSorterClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = ets_destroy;
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
	ets->sort_info_changed_id = 0;
}

E_MAKE_TYPE(e_table_sorter, "ETableSorter", ETableSorter, ets_class_init, ets_init, PARENT_TYPE);

ETableSorter *
e_table_sorter_new (ETableModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETableSorter *ets = gtk_type_new (E_TABLE_SORTER_TYPE);
	
	ets->sort_info = sort_info;
	gtk_object_ref(GTK_OBJECT(ets->sort_info));
	ets->full_header = full_header;
	gtk_object_ref(GTK_OBJECT(ets->full_header));
	ets->source = source;
	gtk_object_ref(GTK_OBJECT(ets->source));

	ets->table_model_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_changed",
							   GTK_SIGNAL_FUNC (ets_model_changed), ets);
	ets->table_model_row_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_row_changed",
							       GTK_SIGNAL_FUNC (ets_model_row_changed), ets);
	ets->table_model_cell_changed_id = gtk_signal_connect (GTK_OBJECT (source), "model_cell_changed",
								GTK_SIGNAL_FUNC (ets_model_cell_changed), ets);
	ets->sort_info_changed_id = gtk_signal_connect (GTK_OBJECT (sort_info), "sort_info_changed",
							 GTK_SIGNAL_FUNC (ets_sort_info_changed), ets);
	
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
ets_sort_info_changed (ETableSortInfo *info, ETableSorter *ets)
{
	ets_clean(ets);
}

static ETableSorter *ets_closure;
void **vals_closure;
int cols_closure;
int *ascending_closure;
GCompareFunc *compare_closure;

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

		if (column.column > e_table_header_count (ets->full_header))
			col = e_table_header_get_column (ets->full_header, e_table_header_count (ets->full_header) - 1);
		else
			col = e_table_header_get_column (ets->full_header, column.column);

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

gboolean
e_table_sorter_needs_sorting(ETableSorter *ets)
{
	if (ets->needs_sorting < 0) {
		if (e_table_sort_info_sorting_get_count(ets->sort_info) + e_table_sort_info_grouping_get_count(ets->sort_info))
			ets->needs_sorting = 1;
		else
			ets->needs_sorting = 0;
	}
	return ets->needs_sorting;
}


gint
e_table_sorter_model_to_sorted (ETableSorter *sorter, int row)
{
	int rows = e_table_model_row_count(sorter->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (e_table_sorter_needs_sorting(sorter)) {
		ets_backsort(sorter);
		return sorter->backsorted[row];
	} else
		return row;
}

gint
e_table_sorter_sorted_to_model (ETableSorter *sorter, int row)
{
	int rows = e_table_model_row_count(sorter->source);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < rows, -1);

	if (e_table_sorter_needs_sorting(sorter)) {
		ets_sort(sorter);
		return sorter->sorted[row];
	} else
		return row;
}

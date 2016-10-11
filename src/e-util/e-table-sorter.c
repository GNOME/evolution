/*
 * e-table-sorter.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "e-table-sorter.h"
#include "e-table-sorting-utils.h"

#define d(x)

enum {
	PROP_0,
	PROP_SORT_INFO
};

/* Forward Declarations */
static void	e_table_sorter_interface_init	(ESorterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	ETableSorter,
	e_table_sorter,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SORTER,
		e_table_sorter_interface_init))

struct qsort_data {
	ETableSorter *table_sorter;
	gpointer *vals;
	gint cols;
	gint *ascending;
	GCompareDataFunc *compare;
	gpointer cmp_cache;
};

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static gint
qsort_callback (gconstpointer data1,
                gconstpointer data2,
                gpointer user_data)
{
	struct qsort_data *qd = (struct qsort_data *) user_data;
	gint row1 = *(gint *) data1;
	gint row2 = *(gint *) data2;
	gint j;
	gint sort_count;
	gint comp_val = 0;
	gint ascending = 1;

	sort_count =
		e_table_sort_info_sorting_get_count (
			qd->table_sorter->sort_info) +
		e_table_sort_info_grouping_get_count (
			qd->table_sorter->sort_info);

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
table_sorter_clean (ETableSorter *table_sorter)
{
	g_free (table_sorter->sorted);
	table_sorter->sorted = NULL;

	g_free (table_sorter->backsorted);
	table_sorter->backsorted = NULL;

	table_sorter->needs_sorting = -1;
}

static void
table_sorter_sort (ETableSorter *table_sorter)
{
	gint rows;
	gint i;
	gint j;
	gint cols;
	gint group_cols;
	struct qsort_data qd;

	if (table_sorter->sorted)
		return;

	rows = e_table_model_row_count (table_sorter->source);
	group_cols = e_table_sort_info_grouping_get_count (table_sorter->sort_info);
	cols = e_table_sort_info_sorting_get_count (table_sorter->sort_info) + group_cols;

	table_sorter->sorted = g_new (int, rows);
	for (i = 0; i < rows; i++)
		table_sorter->sorted[i] = i;

	qd.cols = cols;
	qd.table_sorter = table_sorter;

	qd.vals = g_new (gpointer , rows * cols);
	qd.ascending = g_new (int, cols);
	qd.compare = g_new (GCompareDataFunc, cols);
	qd.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;
		GtkSortType sort_type;

		if (j < group_cols)
			spec = e_table_sort_info_grouping_get_nth (
				table_sorter->sort_info,
				j, &sort_type);
		else
			spec = e_table_sort_info_sorting_get_nth (
				table_sorter->sort_info,
				j - group_cols, &sort_type);

		col = e_table_header_get_column_by_spec (
			table_sorter->full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (
				table_sorter->full_header) - 1;
			col = e_table_header_get_column (
				table_sorter->full_header, last);
		}

		for (i = 0; i < rows; i++) {
			qd.vals[i * cols + j] = e_table_model_value_at (
				table_sorter->source,
				col->spec->model_col, i);
		}

		qd.compare[j] = col->compare;
		qd.ascending[j] = (sort_type == GTK_SORT_ASCENDING);
	}

	g_qsort_with_data (table_sorter->sorted, rows, sizeof (gint), qsort_callback, &qd);

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;
		GtkSortType sort_type;

		if (j < group_cols)
			spec = e_table_sort_info_grouping_get_nth (
				table_sorter->sort_info,
				j, &sort_type);
		else
			spec = e_table_sort_info_sorting_get_nth (
				table_sorter->sort_info,
				j - group_cols, &sort_type);

		col = e_table_header_get_column_by_spec (
			table_sorter->full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (
				table_sorter->full_header) - 1;
			col = e_table_header_get_column (
				table_sorter->full_header, last);
		}

		for (i = 0; i < rows; i++) {
			e_table_model_free_value (table_sorter->source, col->spec->model_col, qd.vals[i * cols + j]);
		}
	}

	g_free (qd.vals);
	g_free (qd.ascending);
	g_free (qd.compare);
	e_table_sorting_utils_free_cmp_cache (qd.cmp_cache);
}

static void
table_sorter_backsort (ETableSorter *table_sorter)
{
	gint i, rows;

	if (table_sorter->backsorted)
		return;

	table_sorter_sort (table_sorter);

	rows = e_table_model_row_count (table_sorter->source);
	table_sorter->backsorted = g_new0 (int, rows);

	for (i = 0; i < rows; i++) {
		table_sorter->backsorted[table_sorter->sorted[i]] = i;
	}
}

static void
table_sorter_model_changed_cb (ETableModel *table_model,
                               ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_model_row_changed_cb (ETableModel *table_model,
                                   gint row,
                                   ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_model_cell_changed_cb (ETableModel *table_model,
                                    gint col,
                                    gint row,
                                    ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_model_rows_inserted_cb (ETableModel *table_model,
                                     gint row,
                                     gint count,
                                     ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_model_rows_deleted_cb (ETableModel *table_model,
                                    gint row,
                                    gint count,
                                    ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_sort_info_changed_cb (ETableSortInfo *sort_info,
                                   ETableSorter *table_sorter)
{
	table_sorter_clean (table_sorter);
}

static void
table_sorter_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (object);

	switch (property_id) {
	case PROP_SORT_INFO:
		if (table_sorter->sort_info) {
			if (table_sorter->sort_info_changed_id)
				g_signal_handler_disconnect (
					table_sorter->sort_info,
					table_sorter->sort_info_changed_id);
			if (table_sorter->group_info_changed_id)
				g_signal_handler_disconnect (
					table_sorter->sort_info,
					table_sorter->group_info_changed_id);
			g_object_unref (table_sorter->sort_info);
		}

		table_sorter->sort_info = g_value_dup_object (value);

		table_sorter->sort_info_changed_id = g_signal_connect (
			table_sorter->sort_info, "sort_info_changed",
			G_CALLBACK (table_sorter_sort_info_changed_cb),
			table_sorter);
		table_sorter->group_info_changed_id = g_signal_connect (
			table_sorter->sort_info, "group_info_changed",
			G_CALLBACK (table_sorter_sort_info_changed_cb),
			table_sorter);

		table_sorter_clean (table_sorter);
		break;
	default:
		break;
	}
}

static void
table_sorter_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (object);
	switch (property_id) {
	case PROP_SORT_INFO:
		g_value_set_object (value, table_sorter->sort_info);
		break;
	}
}

static void
table_sorter_dispose (GObject *object)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (object);

	if (table_sorter->table_model_changed_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->source,
			table_sorter->table_model_changed_id);
		table_sorter->table_model_changed_id = 0;
	}

	if (table_sorter->table_model_row_changed_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->source,
			table_sorter->table_model_row_changed_id);
		table_sorter->table_model_row_changed_id = 0;
	}

	if (table_sorter->table_model_cell_changed_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->source,
			table_sorter->table_model_cell_changed_id);
		table_sorter->table_model_cell_changed_id = 0;
	}

	if (table_sorter->table_model_rows_inserted_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->source,
			table_sorter->table_model_rows_inserted_id);
		table_sorter->table_model_rows_inserted_id = 0;
	}

	if (table_sorter->table_model_rows_deleted_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->source,
			table_sorter->table_model_rows_deleted_id);
		table_sorter->table_model_rows_deleted_id = 0;
	}

	if (table_sorter->sort_info_changed_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->sort_info,
			table_sorter->sort_info_changed_id);
		table_sorter->sort_info_changed_id = 0;
	}

	if (table_sorter->group_info_changed_id > 0) {
		g_signal_handler_disconnect (
			table_sorter->sort_info,
			table_sorter->group_info_changed_id);
		table_sorter->group_info_changed_id = 0;
	}

	g_clear_object (&table_sorter->sort_info);
	g_clear_object (&table_sorter->full_header);
	g_clear_object (&table_sorter->source);

	table_sorter_clean (table_sorter);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_table_sorter_parent_class)->dispose (object);
}

static gint
table_sorter_model_to_sorted (ESorter *sorter,
                              gint row)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (sorter);
	gint rows = e_table_model_row_count (table_sorter->source);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < rows, -1);

	if (e_sorter_needs_sorting (sorter))
		table_sorter_backsort (table_sorter);

	if (table_sorter->backsorted)
		return table_sorter->backsorted[row];
	else
		return row;
}

static gint
table_sorter_sorted_to_model (ESorter *sorter,
                              gint row)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (sorter);
	gint rows = e_table_model_row_count (table_sorter->source);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < rows, -1);

	if (e_sorter_needs_sorting (sorter))
		table_sorter_sort (table_sorter);

	if (table_sorter->sorted)
		return table_sorter->sorted[row];
	else
		return row;
}

static void
table_sorter_get_model_to_sorted_array (ESorter *sorter,
                                        gint **array,
                                        gint *count)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (sorter);

	if (array || count) {
		table_sorter_backsort (table_sorter);

		if (array)
			*array = table_sorter->backsorted;
		if (count)
			*count = e_table_model_row_count(table_sorter->source);
	}
}

static void
table_sorter_get_sorted_to_model_array (ESorter *sorter,
                                        gint **array,
                                        gint *count)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (sorter);

	if (array || count) {
		table_sorter_sort (table_sorter);

		if (array)
			*array = table_sorter->sorted;
		if (count)
			*count = e_table_model_row_count(table_sorter->source);
	}
}

static gboolean
table_sorter_needs_sorting (ESorter *sorter)
{
	ETableSorter *table_sorter = E_TABLE_SORTER (sorter);

	if (table_sorter->needs_sorting < 0) {
		if (e_table_sort_info_sorting_get_count (table_sorter->sort_info) + e_table_sort_info_grouping_get_count (table_sorter->sort_info))
			table_sorter->needs_sorting = 1;
		else
			table_sorter->needs_sorting = 0;
	}
	return table_sorter->needs_sorting;
}

static void
e_table_sorter_class_init (ETableSorterClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = table_sorter_set_property;
	object_class->get_property = table_sorter_get_property;
	object_class->dispose = table_sorter_dispose;

	g_object_class_install_property (
		object_class,
		PROP_SORT_INFO,
		g_param_spec_object (
			"sort_info",
			"Sort Info",
			NULL,
			E_TYPE_TABLE_SORT_INFO,
			G_PARAM_READWRITE));
}

static void
e_table_sorter_interface_init (ESorterInterface *iface)
{
	iface->model_to_sorted = table_sorter_model_to_sorted;
	iface->sorted_to_model = table_sorter_sorted_to_model;
	iface->get_model_to_sorted_array = table_sorter_get_model_to_sorted_array;
	iface->get_sorted_to_model_array = table_sorter_get_sorted_to_model_array;
	iface->needs_sorting = table_sorter_needs_sorting;
}

static void
e_table_sorter_init (ETableSorter *table_sorter)
{
	table_sorter->needs_sorting = -1;
}

ETableSorter *
e_table_sorter_new (ETableModel *source,
                    ETableHeader *full_header,
                    ETableSortInfo *sort_info)
{
	ETableSorter *table_sorter;

	table_sorter = g_object_new (E_TYPE_TABLE_SORTER, NULL);
	table_sorter->sort_info = g_object_ref (sort_info);
	table_sorter->full_header = g_object_ref (full_header);
	table_sorter->source = g_object_ref (source);

	table_sorter->table_model_changed_id = g_signal_connect (
		source, "model_changed",
		G_CALLBACK (table_sorter_model_changed_cb), table_sorter);

	table_sorter->table_model_row_changed_id = g_signal_connect (
		source, "model_row_changed",
		G_CALLBACK (table_sorter_model_row_changed_cb), table_sorter);

	table_sorter->table_model_cell_changed_id = g_signal_connect (
		source, "model_cell_changed",
		G_CALLBACK (table_sorter_model_cell_changed_cb), table_sorter);

	table_sorter->table_model_rows_inserted_id = g_signal_connect (
		source, "model_rows_inserted",
		G_CALLBACK (table_sorter_model_rows_inserted_cb), table_sorter);

	table_sorter->table_model_rows_deleted_id = g_signal_connect (
		source, "model_rows_deleted",
		G_CALLBACK (table_sorter_model_rows_deleted_cb), table_sorter);

	table_sorter->sort_info_changed_id = g_signal_connect (
		sort_info, "sort_info_changed",
		G_CALLBACK (table_sorter_sort_info_changed_cb), table_sorter);

	table_sorter->group_info_changed_id = g_signal_connect (
		sort_info, "group_info_changed",
		G_CALLBACK (table_sorter_sort_info_changed_cb), table_sorter);

	return table_sorter;
}


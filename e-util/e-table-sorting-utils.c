/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-table-sorting-utils.h"

#include <string.h>
#include <camel/camel.h>

#include "e-misc-utils.h"

#define d(x)

/* This takes source rows. */
static gint
etsu_compare (ETableModel *source,
              ETableSortInfo *sort_info,
              ETableHeader *full_header,
              gint row1,
              gint row2,
              gpointer cmp_cache)
{
	gint j;
	gint sort_count = e_table_sort_info_sorting_get_count (sort_info);
	gint comp_val = 0;
	GtkSortType sort_type = GTK_SORT_ASCENDING;

	for (j = 0; j < sort_count; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;
		gpointer value1, value2;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &sort_type);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		value1 = e_table_model_value_at (source, col->spec->compare_col, row1);
		value2 = e_table_model_value_at (source, col->spec->compare_col, row2);

		comp_val = (*col->compare) (value1, value2, cmp_cache);

		e_table_model_free_value (source, col->spec->compare_col, value1);
		e_table_model_free_value (source, col->spec->compare_col, value2);

		if (comp_val != 0)
			break;
	}

	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}

	if (sort_type == GTK_SORT_DESCENDING)
		comp_val = -comp_val;

	return comp_val;
}

typedef struct {
	gint cols;
	gpointer *vals;
	GtkSortType *sort_type;
	GCompareDataFunc *compare;
	gpointer cmp_cache;
} ETableSortClosure;

typedef struct {
	ETreeModel *tree;
	ETableSortInfo *sort_info;
	ETableHeader *full_header;
	gpointer cmp_cache;
} ETreeSortClosure;

/* FIXME: Make it not cache the second and later columns (as if anyone cares.) */

static gint
e_sort_callback (gconstpointer data1,
                 gconstpointer data2,
                 gpointer user_data)
{
	gint row1 = *(gint *) data1;
	gint row2 = *(gint *) data2;
	ETableSortClosure *closure = user_data;
	gint j;
	gint sort_count = closure->cols;
	gint comp_val = 0;
	GtkSortType sort_type = GTK_SORT_ASCENDING;

	for (j = 0; j < sort_count; j++) {
		comp_val = (*(closure->compare[j])) (
			closure->vals[closure->cols * row1 + j],
			closure->vals[closure->cols * row2 + j],
			closure->cmp_cache);
		sort_type = closure->sort_type[j];
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}

	if (sort_type == GTK_SORT_DESCENDING)
		comp_val = -comp_val;

	return comp_val;
}

void
e_table_sorting_utils_sort (ETableModel *source,
                            ETableSortInfo *sort_info,
                            ETableHeader *full_header,
                            gint *map_table,
                            gint rows)
{
	gint total_rows;
	gint i;
	gint j;
	gint cols;
	ETableSortClosure closure;

	g_return_if_fail (E_IS_TABLE_MODEL (source));
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (E_IS_TABLE_HEADER (full_header));

	total_rows = e_table_model_row_count (source);
	cols = e_table_sort_info_sorting_get_count (sort_info);
	closure.cols = cols;

	closure.vals = g_new (gpointer, total_rows * cols);
	closure.sort_type = g_new (GtkSortType, cols);
	closure.compare = g_new (GCompareDataFunc, cols);
	closure.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &closure.sort_type[j]);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		for (i = 0; i < rows; i++) {
			closure.vals[map_table[i] * cols + j] = e_table_model_value_at (source, col->spec->compare_col, map_table[i]);
		}
		closure.compare[j] = col->compare;
	}

	g_qsort_with_data (
		map_table, rows, sizeof (gint), e_sort_callback, &closure);

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &closure.sort_type[j]);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		for (i = 0; i < rows; i++) {
			e_table_model_free_value (source, col->spec->compare_col, closure.vals[map_table[i] * cols + j]);
		}
	}

	g_free (closure.vals);
	g_free (closure.sort_type);
	g_free (closure.compare);
	e_table_sorting_utils_free_cmp_cache (closure.cmp_cache);
}

gboolean
e_table_sorting_utils_affects_sort (ETableSortInfo *sort_info,
                                    ETableHeader *full_header,
                                    gint compare_col)
{
	gint j;
	gint cols;

	g_return_val_if_fail (E_IS_TABLE_SORT_INFO (sort_info), TRUE);
	g_return_val_if_fail (E_IS_TABLE_HEADER (full_header), TRUE);

	cols = e_table_sort_info_sorting_get_count (sort_info);

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, NULL);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		if (compare_col == col->spec->compare_col)
			return TRUE;
	}

	return FALSE;
}

/* FIXME: This could be done in time log n instead of time n with a binary search. */
gint
e_table_sorting_utils_insert (ETableModel *source,
                              ETableSortInfo *sort_info,
                              ETableHeader *full_header,
                              gint *map_table,
                              gint rows,
                              gint row)
{
	gint i;
	gpointer cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	i = 0;
	/* handle insertions when we have a 'sort group' */
	while (i < rows && etsu_compare (source, sort_info, full_header, map_table[i], row, cmp_cache) < 0)
		i++;

	e_table_sorting_utils_free_cmp_cache (cmp_cache);

	return i;
}

/* FIXME: This could be done in time log n instead of time n with a binary search. */
gint
e_table_sorting_utils_check_position (ETableModel *source,
                                      ETableSortInfo *sort_info,
                                      ETableHeader *full_header,
                                      gint *map_table,
                                      gint rows,
                                      gint view_row)
{
	gint i;
	gint row;
	gpointer cmp_cache;

	i = view_row;
	row = map_table[i];
	cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	i = view_row;
	if (i < rows - 1 && etsu_compare (source, sort_info, full_header, map_table[i + 1], row, cmp_cache) < 0) {
		i++;
		while (i < rows - 1 && etsu_compare (source, sort_info, full_header, map_table[i], row, cmp_cache) < 0)
			i++;
	} else if (i > 0 && etsu_compare (source, sort_info, full_header, map_table[i - 1], row, cmp_cache) > 0) {
		i--;
		while (i > 0 && etsu_compare (source, sort_info, full_header, map_table[i], row, cmp_cache) > 0)
			i--;
	}

	e_table_sorting_utils_free_cmp_cache (cmp_cache);

	return i;
}

/* This takes source rows. */
static gint
etsu_tree_compare (ETreeModel *source,
                   ETableSortInfo *sort_info,
                   ETableHeader *full_header,
                   ETreePath path1,
                   ETreePath path2,
                   gpointer cmp_cache)
{
	gint j;
	gint sort_count = e_table_sort_info_sorting_get_count (sort_info);
	gint comp_val = 0;
	GtkSortType sort_type = GTK_SORT_ASCENDING;

	for (j = 0; j < sort_count; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &sort_type);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		comp_val = (*col->compare) (
			e_tree_model_value_at (
				source, path1, col->spec->compare_col),
			e_tree_model_value_at (
				source, path2, col->spec->compare_col),
			cmp_cache);
		if (comp_val != 0)
			break;
	}

	if (sort_type == GTK_SORT_DESCENDING)
		comp_val = -comp_val;

	return comp_val;
}

static gint
e_sort_tree_callback (gconstpointer data1,
                      gconstpointer data2,
                      gpointer user_data)
{
	ETreePath *path1 = *(ETreePath *) data1;
	ETreePath *path2 = *(ETreePath *) data2;
	ETreeSortClosure *closure = user_data;

	return etsu_tree_compare (closure->tree, closure->sort_info, closure->full_header, path1, path2, closure->cmp_cache);
}

void
e_table_sorting_utils_tree_sort (ETreeModel *source,
                                 ETableSortInfo *sort_info,
                                 ETableHeader *full_header,
                                 ETreePath *map_table,
                                 gint count)
{
	ETableSortClosure closure;
	gint cols;
	gint i, j;
	gint *map;
	ETreePath *map_copy;

	g_return_if_fail (E_IS_TREE_MODEL (source));
	g_return_if_fail (E_IS_TABLE_SORT_INFO (sort_info));
	g_return_if_fail (E_IS_TABLE_HEADER (full_header));

	cols = e_table_sort_info_sorting_get_count (sort_info);
	closure.cols = cols;

	closure.vals = g_new (gpointer , count * cols);
	closure.sort_type = g_new (GtkSortType, cols);
	closure.compare = g_new (GCompareDataFunc, cols);
	closure.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &closure.sort_type[j]);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		for (i = 0; i < count; i++) {
			closure.vals[i * cols + j] = e_tree_model_sort_value_at (source, map_table[i], col->spec->compare_col);
		}
		closure.compare[j] = col->compare;
	}

	map = g_new (int, count);
	for (i = 0; i < count; i++) {
		map[i] = i;
	}

	g_qsort_with_data (
		map, count, sizeof (gint), e_sort_callback, &closure);

	map_copy = g_new (ETreePath, count);
	for (i = 0; i < count; i++) {
		map_copy[i] = map_table[i];
	}
	for (i = 0; i < count; i++) {
		map_table[i] = map_copy[map[i]];
	}

	for (j = 0; j < cols; j++) {
		ETableColumnSpecification *spec;
		ETableCol *col;

		spec = e_table_sort_info_sorting_get_nth (
			sort_info, j, &closure.sort_type[j]);

		col = e_table_header_get_column_by_spec (full_header, spec);
		if (col == NULL) {
			gint last = e_table_header_count (full_header) - 1;
			col = e_table_header_get_column (full_header, last);
		}

		for (i = 0; i < count; i++) {
			e_tree_model_free_value (source, col->spec->compare_col, closure.vals[i * cols + j]);
		}
	}

	g_free (map);
	g_free (map_copy);

	g_free (closure.vals);
	g_free (closure.sort_type);
	g_free (closure.compare);
	e_table_sorting_utils_free_cmp_cache (closure.cmp_cache);
}

/* FIXME: This could be done in time log n instead of time n with a binary search. */
gint
e_table_sorting_utils_tree_check_position (ETreeModel *source,
                                           ETableSortInfo *sort_info,
                                           ETableHeader *full_header,
                                           ETreePath *map_table,
                                           gint count,
                                           gint old_index)
{
	gint i;
	ETreePath path;
	gpointer cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	i = old_index;
	path = map_table[i];

	if (i < count - 1 && etsu_tree_compare (source, sort_info, full_header, map_table[i + 1], path, cmp_cache) < 0) {
		i++;
		while (i < count - 1 && etsu_tree_compare (source, sort_info, full_header, map_table[i], path, cmp_cache) < 0)
			i++;
	} else if (i > 0 && etsu_tree_compare (source, sort_info, full_header, map_table[i - 1], path, cmp_cache) > 0) {
		i--;
		while (i > 0 && etsu_tree_compare (source, sort_info, full_header, map_table[i], path, cmp_cache) > 0)
			i--;
	}

	e_table_sorting_utils_free_cmp_cache (cmp_cache);

	return i;
}

/* FIXME: This does not pay attention to making sure that it's a stable insert.  This needs to be fixed. */
gint
e_table_sorting_utils_tree_insert (ETreeModel *source,
                                   ETableSortInfo *sort_info,
                                   ETableHeader *full_header,
                                   ETreePath *map_table,
                                   gint count,
                                   ETreePath path)
{
	gsize start;
	gsize end;
	ETreeSortClosure closure;

	closure.tree = source;
	closure.sort_info = sort_info;
	closure.full_header = full_header;
	closure.cmp_cache = e_table_sorting_utils_create_cmp_cache ();

	e_bsearch (
		&path, map_table, count, sizeof (ETreePath),
		e_sort_tree_callback, &closure, &start, &end);

	e_table_sorting_utils_free_cmp_cache (closure.cmp_cache);

	return end;
}

/**
 * e_table_sorting_utils_create_cmp_cache:
 *
 * Creates a new compare cache, which is storing pairs of string keys and
 * string values.  This can be accessed by
 * e_table_sorting_utils_lookup_cmp_cache() and
 * e_table_sorting_utils_add_to_cmp_cache().
 *
 * Returned pointer should be freed with
 * e_table_sorting_utils_free_cmp_cache().
 **/
gpointer
e_table_sorting_utils_create_cmp_cache (void)
{
	return g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) camel_pstring_free,
		(GDestroyNotify) g_free);
}

/**
 * e_table_sorting_utils_free_cmp_cache:
 * @cmp_cache: a compare cache; cannot be %NULL
 *
 * Frees a compare cache previously created with
 * e_table_sorting_utils_create_cmp_cache().
 **/
void
e_table_sorting_utils_free_cmp_cache (gpointer cmp_cache)
{
	g_return_if_fail (cmp_cache != NULL);

	g_hash_table_destroy (cmp_cache);
}

/**
 * e_table_sorting_utils_add_to_cmp_cache:
 * @cmp_cache: a compare cache; cannot be %NULL
 * @key: unique key to a cache; cannot be %NULL
 * @value: value to store for a key
 *
 * Adds a new value for a given key to a compare cache. If such key
 * already exists in a cache then its value will be replaced.
 * Note: Given @value will be stolen and later freed with g_free.
 **/
void
e_table_sorting_utils_add_to_cmp_cache (gpointer cmp_cache,
                                        const gchar *key,
                                        gchar *value)
{
	g_return_if_fail (cmp_cache != NULL);
	g_return_if_fail (key != NULL);

	g_hash_table_insert (
		cmp_cache, (gchar *) camel_pstring_strdup (key), value);
}

/**
 * e_table_sorting_utils_lookup_cmp_cache:
 * @cmp_cache: a compare cache
 * @key: unique key to a cache
 *
 * Looks ups @key in @cmp_cache, which is passed in GCompareDataFunc as 'data'.
 * Returns %NULL when not found or the cache wasn't provided.
 **/
const gchar *
e_table_sorting_utils_lookup_cmp_cache (gpointer cmp_cache,
                                        const gchar *key)
{
	g_return_val_if_fail (key != NULL, NULL);

	if (cmp_cache == NULL)
		return NULL;

	return g_hash_table_lookup (cmp_cache, key);
}

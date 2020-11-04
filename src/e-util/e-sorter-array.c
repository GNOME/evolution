/*
 * e-sorter-array.c
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

#include "e-sorter-array.h"

#include <string.h>

#include "e-misc-utils.h"

/* Forward Declarations */
static void	e_sorter_array_interface_init	(ESorterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	ESorterArray,
	e_sorter_array,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SORTER,
		e_sorter_array_interface_init))

static gint
esort_callback (gconstpointer data1,
                gconstpointer data2,
                gpointer user_data)
{
	ESorterArray *sorter_array = user_data;
	gint ret_val;
	gint int1, int2;

	int1 = *(gint *) data1;
	int2 = *(gint *) data2;

	ret_val = sorter_array->compare (
		int1, int2,
		sorter_array->cmp_cache,
		sorter_array->closure);
	if (ret_val != 0)
		return ret_val;

	if (int1 < int2)
		return -1;
	if (int1 > int2)
		return 1;
	return 0;
}

static void
sorter_array_sort (ESorterArray *sorter_array)
{
	gint rows;
	gint i;

	if (sorter_array->sorted)
		return;

	rows = sorter_array->rows;

	sorter_array->sorted = g_new (gint, rows);
	for (i = 0; i < rows; i++)
		sorter_array->sorted[i] = i;

	if (sorter_array->compare) {
		if (sorter_array->create_cmp_cache)
			sorter_array->cmp_cache =
				sorter_array->create_cmp_cache (
				sorter_array->closure);

		g_qsort_with_data (
			sorter_array->sorted, rows, sizeof (gint),
			esort_callback, sorter_array);

		g_clear_pointer (&sorter_array->cmp_cache, g_hash_table_destroy);
	}
}

static void
sorter_array_backsort (ESorterArray *sorter_array)
{
	gint i, rows;

	if (sorter_array->backsorted)
		return;

	sorter_array_sort (sorter_array);

	rows = sorter_array->rows;

	sorter_array->backsorted = g_new0 (gint, rows);

	for (i = 0; i < rows; i++)
		sorter_array->backsorted[sorter_array->sorted[i]] = i;
}

static void
sorter_array_finalize (GObject *object)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (object);

	e_sorter_array_clean (sorter_array);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_sorter_array_parent_class)->finalize (object);
}

static gint
sorter_array_model_to_sorted (ESorter *sorter,
                              gint row)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (sorter);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < sorter_array->rows, -1);

	if (e_sorter_needs_sorting (sorter))
		sorter_array_backsort (sorter_array);

	if (sorter_array->backsorted)
		return sorter_array->backsorted[row];
	else
		return row;
}

static gint
sorter_array_sorted_to_model (ESorter *sorter,
                              gint row)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (sorter);

	g_return_val_if_fail (row >= 0, -1);
	g_return_val_if_fail (row < sorter_array->rows, -1);

	if (e_sorter_needs_sorting (sorter))
		sorter_array_sort (sorter_array);

	if (sorter_array->sorted)
		return sorter_array->sorted[row];
	else
		return row;
}

static void
sorter_array_get_model_to_sorted_array (ESorter *sorter,
                                        gint **array,
                                        gint *count)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (sorter);

	if (array || count) {
		sorter_array_backsort (sorter_array);

		if (array)
			*array = sorter_array->backsorted;
		if (count)
			*count = sorter_array->rows;
	}
}

static void
sorter_array_get_sorted_to_model_array (ESorter *sorter,
                                        gint **array,
                                        gint *count)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (sorter);

	if (array || count) {
		sorter_array_sort (sorter_array);

		if (array)
			*array = sorter_array->sorted;
		if (count)
			*count = sorter_array->rows;
	}
}

static gboolean
sorter_array_needs_sorting (ESorter *sorter)
{
	ESorterArray *sorter_array = E_SORTER_ARRAY (sorter);

	return sorter_array->compare != NULL;
}

static void
e_sorter_array_class_init (ESorterArrayClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = sorter_array_finalize;
}

static void
e_sorter_array_interface_init (ESorterInterface *iface)
{
	iface->model_to_sorted = sorter_array_model_to_sorted;
	iface->sorted_to_model = sorter_array_sorted_to_model;
	iface->get_model_to_sorted_array = sorter_array_get_model_to_sorted_array;
	iface->get_sorted_to_model_array = sorter_array_get_sorted_to_model_array;
	iface->needs_sorting = sorter_array_needs_sorting;
}

static void
e_sorter_array_init (ESorterArray *sorter_array)
{
}

ESorterArray *
e_sorter_array_new (ECreateCmpCacheFunc create_cmp_cache,
                    ECompareRowsFunc compare,
                    gpointer closure)
{
	ESorterArray *sorter_array;

	sorter_array = g_object_new (E_TYPE_SORTER_ARRAY, NULL);
	sorter_array->create_cmp_cache = create_cmp_cache;
	sorter_array->compare = compare;
	sorter_array->closure = closure;

	return sorter_array;
}

void
e_sorter_array_clean (ESorterArray *sorter_array)
{
	g_return_if_fail (E_IS_SORTER_ARRAY (sorter_array));

	g_free (sorter_array->sorted);
	sorter_array->sorted = NULL;

	g_free (sorter_array->backsorted);
	sorter_array->backsorted = NULL;
}

void
e_sorter_array_set_count (ESorterArray *sorter_array,
                          gint count)
{
	g_return_if_fail (E_IS_SORTER_ARRAY (sorter_array));

	e_sorter_array_clean (sorter_array);
	sorter_array->rows = count;
}

void
e_sorter_array_append (ESorterArray *sorter_array,
                       gint count)
{
	gint i;

	g_return_if_fail (E_IS_SORTER_ARRAY (sorter_array));

	g_free (sorter_array->backsorted);
	sorter_array->backsorted = NULL;

	if (sorter_array->sorted) {
		sorter_array->sorted = g_renew (
			gint, sorter_array->sorted,
			sorter_array->rows + count);
		for (i = 0; i < count; i++) {
			gint value = sorter_array->rows;
			gsize pos;

			e_bsearch (
				&value,
				sorter_array->sorted,
				sorter_array->rows,
				sizeof (gint),
				esort_callback,
				sorter_array,
				&pos, NULL);
			memmove (
				sorter_array->sorted + pos + 1,
				sorter_array->sorted + pos,
				sizeof (gint) * (sorter_array->rows - pos));
			sorter_array->sorted[pos] = value;
			sorter_array->rows++;
		}
	} else {
		sorter_array->rows += count;
	}
}


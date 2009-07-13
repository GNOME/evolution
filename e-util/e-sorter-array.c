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

#include "e-sorter-array.h"
#include "e-util.h"

#define d(x)

#define INCREMENT_AMOUNT 100

G_DEFINE_TYPE (ESorterArray, e_sorter_array, E_SORTER_TYPE)

static void	esa_sort               (ESorterArray *esa);
static void	esa_backsort           (ESorterArray *esa);

static gint	esa_model_to_sorted           (ESorter *sorter, gint row);
static gint	esa_sorted_to_model           (ESorter *sorter, gint row);
static void	esa_get_model_to_sorted_array (ESorter *sorter, gint **array, gint *count);
static void	esa_get_sorted_to_model_array (ESorter *sorter, gint **array, gint *count);
static gboolean esa_needs_sorting             (ESorter *esa);

#define ESA_NEEDS_SORTING(esa) (((ESorterArray *) (esa))->compare != NULL)

static gint
esort_callback(gconstpointer data1, gconstpointer data2, gpointer user_data)
{
	ESorterArray *esa = user_data;
	gint ret_val;
	gint int1, int2;

	int1 = *(gint *)data1;
	int2 = *(gint *)data2;

	ret_val = esa->compare (int1, int2, esa->closure);
	if (ret_val != 0)
		return ret_val;

	if (int1 < int2)
		return -1;
	if (int1 > int2)
		return 1;
	return 0;
}

static void
esa_sort(ESorterArray *esa)
{
	gint rows;
	gint i;

	if (esa->sorted)
		return;

	rows = esa->rows;

	esa->sorted = g_new(int, rows);
	for (i = 0; i < rows; i++)
		esa->sorted[i] = i;

	if (esa->compare)
		g_qsort_with_data (
			esa->sorted, rows, sizeof(gint),
			esort_callback, esa);
}

static void
esa_backsort(ESorterArray *esa)
{
	gint i, rows;

	if (esa->backsorted)
		return;

	esa_sort(esa);

	rows = esa->rows;

	esa->backsorted = g_new0(int, rows);

	for (i = 0; i < rows; i++) {
		esa->backsorted[esa->sorted[i]] = i;
	}
}

static gint
esa_model_to_sorted (ESorter *es, gint row)
{
	ESorterArray *esa = E_SORTER_ARRAY(es);

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < esa->rows, -1);

	if (ESA_NEEDS_SORTING(es))
		esa_backsort(esa);

	if (esa->backsorted)
		return esa->backsorted[row];
	else
		return row;
}

static gint
esa_sorted_to_model (ESorter *es, gint row)
{
	ESorterArray *esa = (ESorterArray *) es;

	g_return_val_if_fail(row >= 0, -1);
	g_return_val_if_fail(row < esa->rows, -1);

	if (ESA_NEEDS_SORTING(es))
		esa_sort(esa);

	if (esa->sorted)
		return esa->sorted[row];
	else
		return row;
}

static void
esa_get_model_to_sorted_array (ESorter *es, gint **array, gint *count)
{
	ESorterArray *esa = E_SORTER_ARRAY(es);
	if (array || count) {
		esa_backsort(esa);

		if (array)
			*array = esa->backsorted;
		if (count)
			*count = esa->rows;
	}
}

static void
esa_get_sorted_to_model_array (ESorter *es, gint **array, gint *count)
{
	ESorterArray *esa = E_SORTER_ARRAY(es);
	if (array || count) {
		esa_sort(esa);

		if (array)
			*array = esa->sorted;
		if (count)
			*count = esa->rows;
	}
}

static gboolean
esa_needs_sorting(ESorter *es)
{
	ESorterArray *esa = E_SORTER_ARRAY(es);
	return esa->compare != NULL;
}

void
e_sorter_array_clean(ESorterArray *esa)
{
	g_free(esa->sorted);
	esa->sorted = NULL;

	g_free(esa->backsorted);
	esa->backsorted = NULL;
}

void
e_sorter_array_set_count  (ESorterArray *esa, gint count)
{
	e_sorter_array_clean (esa);
	esa->rows = count;
}

void
e_sorter_array_append  (ESorterArray *esa, gint count)
{
	gint i;
	g_free(esa->backsorted);
	esa->backsorted = NULL;

	if (esa->sorted) {
		esa->sorted = g_renew(int, esa->sorted, esa->rows + count);
		for (i = 0; i < count; i++) {
			gint value = esa->rows;
			gsize pos;
			e_bsearch (&value, esa->sorted, esa->rows, sizeof (gint), esort_callback, esa, &pos, NULL);
			memmove (esa->sorted + pos + 1, esa->sorted + pos, sizeof (gint) * (esa->rows - pos));
			esa->sorted[pos] = value;
			esa->rows ++;
		}
	} else {
		esa->rows += count;
	}
}

ESorterArray *
e_sorter_array_construct  (ESorterArray *esa,
			   ECompareRowsFunc  compare,
			   gpointer      closure)
{
	esa->compare = compare;
	esa->closure = closure;
	return esa;
}

ESorterArray *
e_sorter_array_new (ECompareRowsFunc compare, gpointer closure)
{
	ESorterArray *esa = g_object_new (E_SORTER_ARRAY_TYPE, NULL);

	return e_sorter_array_construct (esa, compare, closure);
}

static void
e_sorter_array_class_init (ESorterArrayClass *klass)
{
	ESorterClass *sorter_class = E_SORTER_CLASS(klass);

	sorter_class->model_to_sorted           = esa_model_to_sorted;
	sorter_class->sorted_to_model           = esa_sorted_to_model;
	sorter_class->get_model_to_sorted_array = esa_get_model_to_sorted_array;
	sorter_class->get_sorted_to_model_array = esa_get_sorted_to_model_array;
	sorter_class->needs_sorting             = esa_needs_sorting;
}

static void
e_sorter_array_init (ESorterArray *esa)
{
	esa->rows       = 0;
	esa->compare    = NULL;
	esa->closure    = NULL;
	esa->sorted     = NULL;
	esa->backsorted = NULL;
}


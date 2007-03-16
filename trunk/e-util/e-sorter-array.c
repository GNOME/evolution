/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-sorter-array.c
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

#include "e-sorter-array.h"
#include "e-util.h"

#define d(x)

#define PARENT_TYPE E_SORTER_TYPE

#define INCREMENT_AMOUNT 100

static ESorterClass *parent_class;

static void    	esa_sort               (ESorterArray *esa);
static void    	esa_backsort           (ESorterArray *esa);

static gint    	esa_model_to_sorted           (ESorter *sorter, int row);
static gint    	esa_sorted_to_model           (ESorter *sorter, int row);
static void    	esa_get_model_to_sorted_array (ESorter *sorter, int **array, int *count);
static void    	esa_get_sorted_to_model_array (ESorter *sorter, int **array, int *count);
static gboolean esa_needs_sorting             (ESorter *esa);

#define ESA_NEEDS_SORTING(esa) (((ESorterArray *) (esa))->compare != NULL)

static int
esort_callback(const void *data1, const void *data2, gpointer user_data)
{
	ESorterArray *esa = user_data;
	int ret_val;
	int int1, int2;

	int1 = *(int *)data1;
	int2 = *(int *)data2;

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
	int rows;
	int i;

	if (esa->sorted)
		return;

	rows = esa->rows;

	esa->sorted = g_new(int, rows);
	for (i = 0; i < rows; i++)
		esa->sorted[i] = i;

	if (esa->compare)
		e_sort (esa->sorted, rows, sizeof(int), esort_callback, esa);
}

static void
esa_backsort(ESorterArray *esa)
{
	int i, rows;

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
esa_model_to_sorted (ESorter *es, int row)
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
esa_sorted_to_model (ESorter *es, int row)
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
esa_get_model_to_sorted_array (ESorter *es, int **array, int *count)
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
esa_get_sorted_to_model_array (ESorter *es, int **array, int *count)
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
e_sorter_array_set_count  (ESorterArray *esa, int count)
{
	e_sorter_array_clean (esa);
	esa->rows = count;
}

void
e_sorter_array_append  (ESorterArray *esa, int count)
{
	int i;
	g_free(esa->backsorted);
	esa->backsorted = NULL;

	if (esa->sorted) {
		esa->sorted = g_renew(int, esa->sorted, esa->rows + count);
		for (i = 0; i < count; i++) {
			int value = esa->rows;
			size_t pos;
			e_bsearch (&value, esa->sorted, esa->rows, sizeof (int), esort_callback, esa, &pos, NULL);
			memmove (esa->sorted + pos + 1, esa->sorted + pos, sizeof (int) * (esa->rows - pos));
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
esa_class_init (ESorterArrayClass *klass)
{
	ESorterClass *sorter_class = E_SORTER_CLASS(klass);

	parent_class                            = g_type_class_ref (PARENT_TYPE);

	sorter_class->model_to_sorted           = esa_model_to_sorted           ;
	sorter_class->sorted_to_model           = esa_sorted_to_model           ;
	sorter_class->get_model_to_sorted_array = esa_get_model_to_sorted_array ;
	sorter_class->get_sorted_to_model_array = esa_get_sorted_to_model_array ;		
	sorter_class->needs_sorting             = esa_needs_sorting             ;
}

static void
esa_init (ESorterArray *esa)
{
	esa->rows       = 0;
	esa->compare    = NULL;
	esa->closure    = NULL;
	esa->sorted     = NULL;
	esa->backsorted = NULL;
}

E_MAKE_TYPE(e_sorter_array, "ESorterArray", ESorterArray, esa_class_init, esa_init, PARENT_TYPE)

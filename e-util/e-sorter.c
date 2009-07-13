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

#include "e-sorter.h"
#include "e-util.h"

#define d(x)

#define PARENT_TYPE G_TYPE_OBJECT

G_DEFINE_TYPE (ESorter, e_sorter, G_TYPE_OBJECT)

static gint es_model_to_sorted (ESorter *es, gint row);
static gint es_sorted_to_model (ESorter *es, gint row);
static void es_get_model_to_sorted_array (ESorter *es, gint **array, gint *count);
static void es_get_sorted_to_model_array (ESorter *es, gint **array, gint *count);
static gboolean es_needs_sorting(ESorter *es);

static void
e_sorter_class_init (ESorterClass *klass)
{
	klass->model_to_sorted           = es_model_to_sorted;
	klass->sorted_to_model           = es_sorted_to_model;
	klass->get_model_to_sorted_array = es_get_model_to_sorted_array;
	klass->get_sorted_to_model_array = es_get_sorted_to_model_array;
	klass->needs_sorting             = es_needs_sorting;
}

static void
e_sorter_init (ESorter *es)
{
}

ESorter *
e_sorter_new (void)
{
	ESorter *es = g_object_new (E_SORTER_TYPE, NULL);

	return es;
}

static gint
es_model_to_sorted (ESorter *es, gint row)
{
	return row;
}

static gint
es_sorted_to_model (ESorter *es, gint row)
{
	return row;
}

static void
es_get_model_to_sorted_array (ESorter *es, gint **array, gint *count)
{
}

static void
es_get_sorted_to_model_array (ESorter *es, gint **array, gint *count)
{
}

static gboolean
es_needs_sorting(ESorter *es)
{
	return FALSE;
}

gint
e_sorter_model_to_sorted (ESorter *es, gint row)
{
	g_return_val_if_fail(es != NULL, -1);
	g_return_val_if_fail(row >= 0, -1);

	if (E_SORTER_GET_CLASS(es)->model_to_sorted)
		return E_SORTER_GET_CLASS(es)->model_to_sorted (es, row);
	else
		return -1;
}

gint
e_sorter_sorted_to_model (ESorter *es, gint row)
{
	g_return_val_if_fail(es != NULL, -1);
	g_return_val_if_fail(row >= 0, -1);

	if (E_SORTER_GET_CLASS(es)->sorted_to_model)
		return E_SORTER_GET_CLASS(es)->sorted_to_model (es, row);
	else
		return -1;
}

void
e_sorter_get_model_to_sorted_array (ESorter *es, gint **array, gint *count)
{
	g_return_if_fail(es != NULL);

	if (E_SORTER_GET_CLASS(es)->get_model_to_sorted_array)
		E_SORTER_GET_CLASS(es)->get_model_to_sorted_array (es, array, count);
}

void
e_sorter_get_sorted_to_model_array (ESorter *es, gint **array, gint *count)
{
	g_return_if_fail(es != NULL);

	if (E_SORTER_GET_CLASS(es)->get_sorted_to_model_array)
		E_SORTER_GET_CLASS(es)->get_sorted_to_model_array (es, array, count);
}

gboolean
e_sorter_needs_sorting(ESorter *es)
{
	g_return_val_if_fail (es != NULL, FALSE);

	if (E_SORTER_GET_CLASS(es)->needs_sorting)
		return E_SORTER_GET_CLASS(es)->needs_sorting (es);
	else
		return FALSE;
}

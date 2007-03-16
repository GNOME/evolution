/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-sorter.c
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

#include "e-sorter.h"
#include "e-util.h"

#define d(x)

#define PARENT_TYPE G_TYPE_OBJECT

static GObjectClass *parent_class;

static gint es_model_to_sorted (ESorter *es, int row);
static gint es_sorted_to_model (ESorter *es, int row);
static void es_get_model_to_sorted_array (ESorter *es, int **array, int *count);
static void es_get_sorted_to_model_array (ESorter *es, int **array, int *count);
static gboolean es_needs_sorting(ESorter *es);

static void
es_class_init (ESorterClass *klass)
{
	parent_class                     = g_type_class_ref (PARENT_TYPE);

	klass->model_to_sorted           = es_model_to_sorted;
	klass->sorted_to_model           = es_sorted_to_model;
	klass->get_model_to_sorted_array = es_get_model_to_sorted_array;
	klass->get_sorted_to_model_array = es_get_sorted_to_model_array;
	klass->needs_sorting             = es_needs_sorting;
}

static void
es_init (ESorter *es)
{
}

E_MAKE_TYPE(e_sorter, "ESorter", ESorter, es_class_init, es_init, PARENT_TYPE)

ESorter *
e_sorter_new (void)
{
	ESorter *es = g_object_new (E_SORTER_TYPE, NULL);
	
	return es;
}


static gint
es_model_to_sorted (ESorter *es, int row)
{
	return row;
}

static gint
es_sorted_to_model (ESorter *es, int row)
{
	return row;
}


static void
es_get_model_to_sorted_array (ESorter *es, int **array, int *count)
{
}

static void
es_get_sorted_to_model_array (ESorter *es, int **array, int *count)
{
}


static gboolean
es_needs_sorting(ESorter *es)
{
	return FALSE;
}

gint
e_sorter_model_to_sorted (ESorter *es, int row)
{
	g_return_val_if_fail(es != NULL, -1);
	g_return_val_if_fail(row >= 0, -1);

	if (E_SORTER_GET_CLASS(es)->model_to_sorted)
		return E_SORTER_GET_CLASS(es)->model_to_sorted (es, row);
	else
		return -1;
}

gint
e_sorter_sorted_to_model (ESorter *es, int row)
{
	g_return_val_if_fail(es != NULL, -1);
	g_return_val_if_fail(row >= 0, -1);

	if (E_SORTER_GET_CLASS(es)->sorted_to_model)
		return E_SORTER_GET_CLASS(es)->sorted_to_model (es, row);
	else
		return -1;
}


void
e_sorter_get_model_to_sorted_array (ESorter *es, int **array, int *count)
{
	g_return_if_fail(es != NULL);

	if (E_SORTER_GET_CLASS(es)->get_model_to_sorted_array)
		E_SORTER_GET_CLASS(es)->get_model_to_sorted_array (es, array, count);
}

void
e_sorter_get_sorted_to_model_array (ESorter *es, int **array, int *count)
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

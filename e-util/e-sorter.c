/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-sorted.c: Virtual sorter class
 *
 * Author:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000,2001 Ximian, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include <string.h>
#include "gal/util/e-util.h"
#include "e-sorter.h"

#define d(x)

#define PARENT_TYPE gtk_object_get_type()

static GtkObjectClass *parent_class;

#define ES_CLASS(es) ((ESorterClass *)((GtkObject *)(es))->klass)

static gint es_model_to_sorted (ESorter *es, int row);
static gint es_sorted_to_model (ESorter *es, int row);
static void es_get_model_to_sorted_array (ESorter *es, int **array, int *count);
static void es_get_sorted_to_model_array (ESorter *es, int **array, int *count);
static gboolean es_needs_sorting(ESorter *es);

static void
es_class_init (ESorterClass *klass)
{
	parent_class                     = gtk_type_class (PARENT_TYPE);

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

E_MAKE_TYPE(e_sorter, "ESorter", ESorter, es_class_init, es_init, PARENT_TYPE);

ESorter *
e_sorter_new (void)
{
	ESorter *es = gtk_type_new (E_SORTER_TYPE);
	
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

	if (ES_CLASS(es)->model_to_sorted)
		return ES_CLASS(es)->model_to_sorted (es, row);
	else
		return -1;
}

gint
e_sorter_sorted_to_model (ESorter *es, int row)
{
	g_return_val_if_fail(es != NULL, -1);
	g_return_val_if_fail(row >= 0, -1);

	if (ES_CLASS(es)->sorted_to_model)
		return ES_CLASS(es)->sorted_to_model (es, row);
	else
		return -1;
}


void
e_sorter_get_model_to_sorted_array (ESorter *es, int **array, int *count)
{
	g_return_if_fail(es != NULL);

	if (ES_CLASS(es)->get_model_to_sorted_array)
		ES_CLASS(es)->get_model_to_sorted_array (es, array, count);
}

void
e_sorter_get_sorted_to_model_array (ESorter *es, int **array, int *count)
{
	g_return_if_fail(es != NULL);

	if (ES_CLASS(es)->get_sorted_to_model_array)
		ES_CLASS(es)->get_sorted_to_model_array (es, array, count);
}


gboolean
e_sorter_needs_sorting(ESorter *es)
{
	g_return_val_if_fail (es != NULL, FALSE);

	if (ES_CLASS(es)->needs_sorting)
		return ES_CLASS(es)->needs_sorting (es);
	else
		return FALSE;
}

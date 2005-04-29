/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-simple.c
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

#include "gal/util/e-util.h"

#include "e-tree-simple.h"

static int
simple_column_count (ETableModel *etm)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	if (simple->col_count)
		return simple->col_count (etm, simple->model_data);
	else
		return 0;
}

/* The default for simple_duplicate_value is to return the raw value. */
static void *
simple_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	if (simple->duplicate_value)
		return simple->duplicate_value (etm, col, value, simple->model_data);
	else
		return (void *)value;
}

static void
simple_free_value (ETableModel *etm, int col, void *value)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	if (simple->free_value)
		simple->free_value (etm, col, value, simple->model_data);
}

static void *
simple_initialize_value (ETableModel *etm, int col)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);
	
	if (simple->initialize_value)
		return simple->initialize_value (etm, col, simple->model_data);
	else
		return NULL;
}

static gboolean
simple_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);
	
	if (simple->value_is_empty)
		return simple->value_is_empty (etm, col, value, simple->model_data);
	else
		return FALSE;
}

static char *
simple_value_to_string (ETableModel *etm, int col, const void *value)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);
	
	if (simple->value_to_string)
		return simple->value_to_string (etm, col, value, simple->model_data);
	else
		return g_strdup ("");
}

static void *
simple_value_at (ETreeModel *etm, ETreePath *node, int col)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	return simple->value_at (etm, node, col, simple->model_data);
}

static GdkPixbuf *
simple_icon_at (ETreeModel *etm, ETreePath *node)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	return simple->icon_at (etm, node, simple->model_data);
}

static void
simple_set_value_at (ETreeModel *etm, ETreePath *node, int col, const void *val)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	simple->set_value_at (etm, node, col, val, simple->model_data);
}

static gboolean
simple_is_editable (ETreeModel *etm, ETreePath *node, int col)
{
	ETreeSimple *simple = E_TREE_SIMPLE(etm);

	return simple->is_editable (etm, node, col, simple->model_data);
}

static void
e_tree_simple_class_init (GObjectClass *object_class)
{
	ETreeModelClass *model_class        = (ETreeModelClass *) object_class;
	ETableModelClass *table_model_class = (ETableModelClass *) object_class;

	table_model_class->column_count     = simple_column_count;
	table_model_class->duplicate_value  = simple_duplicate_value;
	table_model_class->free_value       = simple_free_value;
	table_model_class->initialize_value = simple_initialize_value;
	table_model_class->value_is_empty   = simple_value_is_empty;
	table_model_class->value_to_string  = simple_value_to_string;

	model_class      ->icon_at          = simple_icon_at;
	model_class      ->value_at         = simple_value_at;
	model_class      ->set_value_at     = simple_set_value_at;
	model_class      ->is_editable      = simple_is_editable;
}

E_MAKE_TYPE(e_tree_simple, "ETreeSimple", ETreeSimple, e_tree_simple_class_init, NULL, E_TREE_MODEL_TYPE)

/**
 * e_tree_simple_new:
 * @col_count: 
 * @duplicate_value: 
 * @free_value: 
 * @initialize_value: 
 * @value_is_empty: 
 * @value_to_string: 
 * @icon_at: 
 * @value_at: 
 * @set_value_at: 
 * @is_editable: 
 * @model_data: 
 * 
 * This initializes a new ETreeSimpleModel object.  ETreeSimpleModel is
 * an implementaiton of the abstract class ETreeModel.  The ETreeSimpleModel
 * is designed to allow people to easily create ETreeModels without having
 * to create a new GtkType derived from ETreeModel every time they need one.
 *
 * Instead, ETreeSimpleModel uses a setup based in callback functions, every
 * callback function signature mimics the signature of each ETreeModel method
 * and passes the extra @data pointer to each one of the method to provide them
 * with any context they might want to use. 
 * 
 * ETreeSimple is to ETreeModel as ETableSimple is to ETableModel.
 *
 * Return value: An ETreeSimple object (which is also an ETreeModel
 * object).
 **/
ETreeModel *
e_tree_simple_new  (ETableSimpleColumnCountFn     col_count,
		    ETableSimpleDuplicateValueFn  duplicate_value,
		    ETableSimpleFreeValueFn       free_value,
		    ETableSimpleInitializeValueFn initialize_value,
		    ETableSimpleValueIsEmptyFn    value_is_empty,
		    ETableSimpleValueToStringFn   value_to_string,

		    ETreeSimpleIconAtFn      	  icon_at,
		    ETreeSimpleValueAtFn     	  value_at,
		    ETreeSimpleSetValueAtFn  	  set_value_at,
		    ETreeSimpleIsEditableFn  	  is_editable,

		    gpointer                 	  model_data)
{
	ETreeSimple *etg = g_object_new (E_TREE_SIMPLE_TYPE, NULL);

	etg->col_count        = col_count;
	etg->duplicate_value  = duplicate_value;
	etg->free_value       = free_value;
	etg->initialize_value = initialize_value;
	etg->value_is_empty   = value_is_empty;
	etg->value_to_string  = value_to_string;

	etg->icon_at          = icon_at;
	etg->value_at         = value_at;
	etg->set_value_at     = set_value_at;
	etg->is_editable      = is_editable;

	etg->model_data       = model_data;

	return (ETreeModel*)etg;
}


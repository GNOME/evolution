/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-memory-callbacks.c
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

#include <gtk/gtk.h>

#include "gal/util/e-util.h"

#include "e-tree-memory-callbacks.h"

#define PARENT_TYPE E_TREE_MEMORY_TYPE

static GdkPixbuf *
etmc_icon_at (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	return etmc->icon_at (etm, node, etmc->model_data);
}

static int
etmc_column_count (ETreeModel *etm)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->column_count)
		return etmc->column_count (etm, etmc->model_data);
	else
		return 0;
}


static gboolean
etmc_has_save_id (ETreeModel *etm)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->has_save_id)
		return etmc->has_save_id (etm, etmc->model_data);
	else
		return FALSE;
}

static char *
etmc_get_save_id (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->get_save_id)
		return etmc->get_save_id (etm, node, etmc->model_data);
	else
		return NULL;
}

static gboolean
etmc_has_get_node_by_id (ETreeModel *etm)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->has_get_node_by_id)
		return etmc->has_get_node_by_id (etm, etmc->model_data);
	else
		return FALSE;
}

static ETreePath
etmc_get_node_by_id (ETreeModel *etm, const char *save_id)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->get_node_by_id)
		return etmc->get_node_by_id (etm, save_id, etmc->model_data);
	else
		return NULL;
}


static void *
etmc_value_at (ETreeModel *etm, ETreePath node, int col)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	return etmc->value_at (etm, node, col, etmc->model_data);
}

static void
etmc_set_value_at (ETreeModel *etm, ETreePath node, int col, const void *val)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	etmc->set_value_at (etm, node, col, val, etmc->model_data);
}

static gboolean
etmc_is_editable (ETreeModel *etm, ETreePath node, int col)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	return etmc->is_editable (etm, node, col, etmc->model_data);
}


/* The default for etmc_duplicate_value is to return the raw value. */
static void *
etmc_duplicate_value (ETreeModel *etm, int col, const void *value)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->duplicate_value)
		return etmc->duplicate_value (etm, col, value, etmc->model_data);
	else
		return (void *)value;
}

static void
etmc_free_value (ETreeModel *etm, int col, void *value)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);

	if (etmc->free_value)
		etmc->free_value (etm, col, value, etmc->model_data);
}

static void *
etmc_initialize_value (ETreeModel *etm, int col)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);
	
	if (etmc->initialize_value)
		return etmc->initialize_value (etm, col, etmc->model_data);
	else
		return NULL;
}

static gboolean
etmc_value_is_empty (ETreeModel *etm, int col, const void *value)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);
	
	if (etmc->value_is_empty)
		return etmc->value_is_empty (etm, col, value, etmc->model_data);
	else
		return FALSE;
}

static char *
etmc_value_to_string (ETreeModel *etm, int col, const void *value)
{
	ETreeMemoryCallbacks *etmc = E_TREE_MEMORY_CALLBACKS(etm);
	
	if (etmc->value_to_string)
		return etmc->value_to_string (etm, col, value, etmc->model_data);
	else
		return g_strdup ("");
}

static void
e_tree_memory_callbacks_class_init (GtkObjectClass *object_class)
{
	ETreeModelClass *model_class        = (ETreeModelClass *) object_class;

	model_class->icon_at            = etmc_icon_at;

	model_class->column_count       = etmc_column_count;

	model_class->has_save_id        = etmc_has_save_id;
	model_class->get_save_id        = etmc_get_save_id;

	model_class->has_get_node_by_id = etmc_has_get_node_by_id;
	model_class->get_node_by_id     = etmc_get_node_by_id;

	model_class->value_at           = etmc_value_at;
	model_class->set_value_at       = etmc_set_value_at;
	model_class->is_editable        = etmc_is_editable;

	model_class->duplicate_value    = etmc_duplicate_value;
	model_class->free_value         = etmc_free_value;
	model_class->initialize_value   = etmc_initialize_value;
	model_class->value_is_empty     = etmc_value_is_empty;
	model_class->value_to_string    = etmc_value_to_string;
}

E_MAKE_TYPE(e_tree_memory_callbacks, "ETreeMemoryCallbacks", ETreeMemoryCallbacks, e_tree_memory_callbacks_class_init, NULL, PARENT_TYPE)

/**
 * e_tree_memory_callbacks_new:
 * 
 * This initializes a new ETreeMemoryCallbacksModel object.
 * ETreeMemoryCallbacksModel is an implementaiton of the somewhat
 * abstract class ETreeMemory.  The ETreeMemoryCallbacksModel is
 * designed to allow people to easily create ETreeMemorys without
 * having to create a new GtkType derived from ETreeMemory every time
 * they need one.
 *
 * Instead, ETreeMemoryCallbacksModel uses a setup based in callback functions, every
 * callback function signature mimics the signature of each ETreeModel method
 * and passes the extra @data pointer to each one of the method to provide them
 * with any context they might want to use. 
 * 
 * ETreeMemoryCallbacks is to ETreeMemory as ETableSimple is to ETableModel.
 *
 * Return value: An ETreeMemoryCallbacks object (which is also an
 * ETreeMemory and thus an ETreeModel object).
 *
 */
ETreeModel *
e_tree_memory_callbacks_new  (ETreeMemoryCallbacksIconAtFn icon_at,

			      ETreeMemoryCallbacksColumnCountFn        column_count,

			      ETreeMemoryCallbacksHasSaveIdFn          has_save_id,
			      ETreeMemoryCallbacksGetSaveIdFn          get_save_id,

			      ETreeMemoryCallbacksHasGetNodeByIdFn     has_get_node_by_id,
			      ETreeMemoryCallbacksGetNodeByIdFn        get_node_by_id,

			      ETreeMemoryCallbacksValueAtFn            value_at,
			      ETreeMemoryCallbacksSetValueAtFn         set_value_at,
			      ETreeMemoryCallbacksIsEditableFn         is_editable,

			      ETreeMemoryCallbacksDuplicateValueFn     duplicate_value,
			      ETreeMemoryCallbacksFreeValueFn          free_value,
			      ETreeMemoryCallbacksInitializeValueFn    initialize_value,
			      ETreeMemoryCallbacksValueIsEmptyFn       value_is_empty,
			      ETreeMemoryCallbacksValueToStringFn      value_to_string,

			      gpointer                                 model_data)
{
	ETreeMemoryCallbacks *etmc;

	etmc = g_object_new (E_TREE_MEMORY_CALLBACKS_TYPE, NULL);

	etmc->icon_at            = icon_at;

	etmc->column_count       = column_count;

	etmc->has_save_id        = has_save_id;
	etmc->get_save_id        = get_save_id;

	etmc->has_get_node_by_id = has_get_node_by_id;
	etmc->get_node_by_id     = get_node_by_id;

	etmc->value_at           = value_at;
	etmc->set_value_at       = set_value_at;
	etmc->is_editable        = is_editable;

	etmc->duplicate_value    = duplicate_value;
	etmc->free_value         = free_value;
	etmc->initialize_value   = initialize_value;
	etmc->value_is_empty     = value_is_empty;
	etmc->value_to_string    = value_to_string;

	etmc->model_data         = model_data;

	return (ETreeModel*)etmc;
}


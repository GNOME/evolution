/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-reflow-model.c
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

#include "e-util-marshal.h"

#include "e-util/e-util.h"

#include "e-reflow-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

#define d(x)

d(static gint depth = 0;)


static GObjectClass *e_reflow_model_parent_class;

enum {
	MODEL_CHANGED,
	COMPARISON_CHANGED,
	MODEL_ITEMS_INSERTED,
	MODEL_ITEM_CHANGED,
	MODEL_ITEM_REMOVED,
	LAST_SIGNAL
};

static guint e_reflow_model_signals [LAST_SIGNAL] = { 0, };

/**
 * e_reflow_model_set_width:
 * @e_reflow_model: The e-reflow-model to operate on
 * @width: The new value for the width of each item.
 */
void
e_reflow_model_set_width (EReflowModel *e_reflow_model, int width)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));

	E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->set_width (e_reflow_model, width);
}

/**
 * e_reflow_model_count:
 * @e_reflow_model: The e-reflow-model to operate on
 *
 * Returns: the number of items in the reflow model.
 */
int
e_reflow_model_count (EReflowModel *e_reflow_model)
{
	g_return_val_if_fail (e_reflow_model != NULL, 0);
	g_return_val_if_fail (E_IS_REFLOW_MODEL (e_reflow_model), 0);

	return E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->count (e_reflow_model);
}

/**
 * e_reflow_model_height:
 * @e_reflow_model: The e-reflow-model to operate on
 * @n: The item number to get the height of.
 * @parent: The parent GnomeCanvasItem.
 *
 * Returns: the height of the nth item.
 */
int
e_reflow_model_height (EReflowModel *e_reflow_model, int n, GnomeCanvasGroup *parent)
{
	g_return_val_if_fail (e_reflow_model != NULL, 0);
	g_return_val_if_fail (E_IS_REFLOW_MODEL (e_reflow_model), 0);

	return E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->height (e_reflow_model, n, parent);
}

/**
 * e_reflow_model_incarnate:
 * @e_reflow_model: The e-reflow-model to operate on
 * @n: The item to create.
 * @parent: The parent GnomeCanvasItem to create a child of.
 *
 * Create a GnomeCanvasItem to represent the nth piece of data.
 *
 * Returns: the new GnomeCanvasItem.
 */
GnomeCanvasItem *
e_reflow_model_incarnate (EReflowModel *e_reflow_model, int n, GnomeCanvasGroup *parent)
{
	g_return_val_if_fail (e_reflow_model != NULL, NULL);
	g_return_val_if_fail (E_IS_REFLOW_MODEL (e_reflow_model), NULL);

	return E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->incarnate (e_reflow_model, n, parent);
}

/**
 * e_reflow_model_compare:
 * @e_reflow_model: The e-reflow-model to operate on
 * @n1: The first item to compare
 * @n2: The second item to compare
 *
 * Compares item n1 and item n2 to see which should come first.
 *
 * Returns: strcmp like semantics for the comparison value.
 */
int
e_reflow_model_compare (EReflowModel *e_reflow_model, int n1, int n2)
{
#if 0
	g_return_val_if_fail (e_reflow_model != NULL, 0);
	g_return_val_if_fail (E_IS_REFLOW_MODEL (e_reflow_model), 0);
#endif

	return E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->compare (e_reflow_model, n1, n2);
}

/**
 * e_reflow_model_reincarnate:
 * @e_reflow_model: The e-reflow-model to operate on
 * @n: The item to create.
 * @item: The item to reuse.
 *
 * Update item to represent the nth piece of data.
 */
void
e_reflow_model_reincarnate (EReflowModel *e_reflow_model, int n, GnomeCanvasItem *item)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));

	E_REFLOW_MODEL_GET_CLASS (e_reflow_model)->reincarnate (e_reflow_model, n, item);
}

static void
e_reflow_model_class_init (GObjectClass *object_class)
{
	EReflowModelClass *klass = E_REFLOW_MODEL_CLASS(object_class);
	e_reflow_model_parent_class = g_type_class_ref (PARENT_TYPE);

	e_reflow_model_signals [MODEL_CHANGED] =
		g_signal_new ("model_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EReflowModelClass, model_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_reflow_model_signals [COMPARISON_CHANGED] =
		g_signal_new ("comparison_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EReflowModelClass, comparison_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_reflow_model_signals [MODEL_ITEMS_INSERTED] =
		g_signal_new ("model_items_inserted",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EReflowModelClass, model_items_inserted),
			      NULL, NULL,
			      e_util_marshal_NONE__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	e_reflow_model_signals [MODEL_ITEM_CHANGED] =
		g_signal_new ("model_item_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EReflowModelClass, model_item_changed),
			      NULL, NULL,
			      e_util_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	e_reflow_model_signals [MODEL_ITEM_REMOVED] =
		g_signal_new ("model_item_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EReflowModelClass, model_item_removed),
			      NULL, NULL,
			      e_util_marshal_NONE__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	klass->set_width            = NULL;
	klass->count                = NULL;
	klass->height               = NULL;
	klass->incarnate            = NULL;
	klass->reincarnate          = NULL;

	klass->model_changed        = NULL;
	klass->comparison_changed   = NULL;
	klass->model_items_inserted = NULL;
	klass->model_item_removed   = NULL;
	klass->model_item_changed   = NULL;
}

static void
e_reflow_model_init (GObject *object)
{
}

E_MAKE_TYPE(e_reflow_model, "EReflowModel", EReflowModel,
	    e_reflow_model_class_init, e_reflow_model_init, PARENT_TYPE)

#if d(!)0
static void
print_tabs (void)
{
	int i;
	for (i = 0; i < depth; i++)
		g_print("\t");
}
#endif

/**
 * e_reflow_model_changed:
 * @e_reflow_model: the reflow model to notify of the change
 *
 * Use this function to notify any views of this reflow model that
 * the contents of the reflow model have changed.  This will emit
 * the signal "model_changed" on the @e_reflow_model object.
 *
 * It is preferable to use the e_reflow_model_item_changed() signal to
 * notify of smaller changes than to invalidate the entire model, as
 * the views might have ways of caching the information they render
 * from the model.
 */
void
e_reflow_model_changed (EReflowModel *e_reflow_model)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));
	
	d(print_tabs());
	d(g_print("Emitting model_changed on model 0x%p.\n", e_reflow_model));
	d(depth++);
	g_signal_emit (e_reflow_model,
		       e_reflow_model_signals [MODEL_CHANGED], 0);
	d(depth--);
}

/**
 * e_reflow_model_comparison_changed:
 * @e_reflow_model: the reflow model to notify of the change
 *
 * Use this function to notify any views of this reflow model that the
 * sorting has changed.  The actual contents of the items hasn't, so
 * there's no need to re-query the model for the heights of the
 * individual items.
 */
void
e_reflow_model_comparison_changed (EReflowModel *e_reflow_model)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));
	
	d(print_tabs());
	d(g_print("Emitting comparison_changed on model 0x%p.\n", e_reflow_model));
	d(depth++);
	g_signal_emit (e_reflow_model,
		       e_reflow_model_signals [COMPARISON_CHANGED], 0);
	d(depth--);
}

/**
 * e_reflow_model_items_inserted:
 * @e_reflow_model: The model changed.
 * @position: The position the items were insert in.
 * @count: The number of items inserted.
 * 
 * Use this function to notify any views of the reflow model that a number of items have been inserted.
 **/
void
e_reflow_model_items_inserted (EReflowModel *e_reflow_model, int position, int count)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));

	d(print_tabs());
	d(g_print("Emitting items_inserted on model 0x%p, position=%d, count=%d.\n", e_reflow_model, position, count));
	d(depth++);
	g_signal_emit (e_reflow_model,
		       e_reflow_model_signals [MODEL_ITEMS_INSERTED], 0,
		       position, count);
	d(depth--);
}

/**
 * e_reflow_model_item_removed:
 * @e_reflow_model: The model changed.
 * @n: The position from which the items were removed.
 * 
 * Use this function to notify any views of the reflow model that an
 * item has been removed.
 **/
void
e_reflow_model_item_removed    (EReflowModel     *e_reflow_model,
				int               n)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));

	d(print_tabs());
	d(g_print("Emitting item_removed on model 0x%p, n=%d.\n", e_reflow_model, n));
	d(depth++);
	g_signal_emit (e_reflow_model,
		       e_reflow_model_signals [MODEL_ITEM_REMOVED], 0,
		       n);
	d(depth--);
}


/**
 * e_reflow_model_item_changed:
 * @e_reflow_model: the reflow model to notify of the change
 * @item: the item that was changed in the model.
 *
 * Use this function to notify any views of the reflow model that the
 * contents of item @item have changed in model such that the height
 * has changed or the item needs to be reincarnated.  This function
 * will emit the "model_item_changed" signal on the @e_reflow_model
 * object
 */
void
e_reflow_model_item_changed (EReflowModel *e_reflow_model, int n)
{
	g_return_if_fail (e_reflow_model != NULL);
	g_return_if_fail (E_IS_REFLOW_MODEL (e_reflow_model));

	d(print_tabs());
	d(g_print("Emitting item_changed on model 0x%p, n=%d.\n", e_reflow_model, n));
	d(depth++);
	g_signal_emit (e_reflow_model,
		       e_reflow_model_signals [MODEL_ITEM_CHANGED], 0,
		       n);
	d(depth--);
}

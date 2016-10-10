/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-reflow-model.h"

#include "e-misc-utils.h"

G_DEFINE_TYPE (EReflowModel, e_reflow_model, G_TYPE_OBJECT)

#define d(x)

d (static gint depth = 0;)

enum {
	MODEL_CHANGED,
	COMPARISON_CHANGED,
	MODEL_ITEMS_INSERTED,
	MODEL_ITEM_CHANGED,
	MODEL_ITEM_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

/**
 * e_reflow_model_set_width:
 * @reflow_model: The e-reflow-model to operate on
 * @width: The new value for the width of each item.
 */
void
e_reflow_model_set_width (EReflowModel *reflow_model,
                          gint width)
{
	EReflowModelClass *class;

	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_if_fail (class->set_width != NULL);

	class->set_width (reflow_model, width);
}

/**
 * e_reflow_model_count:
 * @reflow_model: The e-reflow-model to operate on
 *
 * Returns: the number of items in the reflow model.
 */
gint
e_reflow_model_count (EReflowModel *reflow_model)
{
	EReflowModelClass *class;

	g_return_val_if_fail (E_IS_REFLOW_MODEL (reflow_model), 0);

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_val_if_fail (class->count != NULL, 0);

	return class->count (reflow_model);
}

/**
 * e_reflow_model_height:
 * @reflow_model: The e-reflow-model to operate on
 * @n: The item number to get the height of.
 * @parent: The parent GnomeCanvasItem.
 *
 * Returns: the height of the nth item.
 */
gint
e_reflow_model_height (EReflowModel *reflow_model,
                       gint n,
                       GnomeCanvasGroup *parent)
{
	EReflowModelClass *class;

	g_return_val_if_fail (E_IS_REFLOW_MODEL (reflow_model), 0);

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_val_if_fail (class->height != NULL, 0);

	return class->height (reflow_model, n, parent);
}

/**
 * e_reflow_model_incarnate:
 * @reflow_model: The e-reflow-model to operate on
 * @n: The item to create.
 * @parent: The parent GnomeCanvasItem to create a child of.
 *
 * Create a GnomeCanvasItem to represent the nth piece of data.
 *
 * Returns: the new GnomeCanvasItem.
 */
GnomeCanvasItem *
e_reflow_model_incarnate (EReflowModel *reflow_model,
                          gint n,
                          GnomeCanvasGroup *parent)
{
	EReflowModelClass *class;

	g_return_val_if_fail (E_IS_REFLOW_MODEL (reflow_model), NULL);

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_val_if_fail (class->incarnate != NULL, NULL);

	return class->incarnate (reflow_model, n, parent);
}

/**
 * e_reflow_model_create_cmp_cache:
 * @reflow_model: The e-reflow-model to operate on
 *
 * Creates a compare cache for quicker sorting. The sorting function
 * may not depend on the cache, but it should benefit from it if available.
 *
 * Returns: Newly created GHashTable with cached compare values. This will be
 * automatically freed with g_hash_table_destroy() when no longer needed.
 **/
GHashTable *
e_reflow_model_create_cmp_cache (EReflowModel *reflow_model)
{
	EReflowModelClass *class;

	g_return_val_if_fail (E_IS_REFLOW_MODEL (reflow_model), NULL);

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);

	if (class->create_cmp_cache == NULL)
		return NULL;

	return class->create_cmp_cache (reflow_model);
}

/**
 * e_reflow_model_compare:
 * @reflow_model: The e-reflow-model to operate on
 * @n1: The first item to compare
 * @n2: The second item to compare
 * @cmp_cache: #GHashTable of cached compare values, created by
 *    e_reflow_model_create_cmp_cache(). This can be NULL, when
 *    caching is not available, even when @reflow_model defines
 *    the create_cmp_cache function.
 *
 * Compares item n1 and item n2 to see which should come first.
 *
 * Returns: strcmp like semantics for the comparison value.
 */
gint
e_reflow_model_compare (EReflowModel *reflow_model,
                        gint n1,
                        gint n2,
                        GHashTable *cmp_cache)
{
	EReflowModelClass *class;

	g_return_val_if_fail (E_IS_REFLOW_MODEL (reflow_model), 0);

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_val_if_fail (class->compare != NULL, 0);

	return class->compare (reflow_model, n1, n2, cmp_cache);
}

/**
 * e_reflow_model_reincarnate:
 * @reflow_model: The e-reflow-model to operate on
 * @n: The item to create.
 * @item: The item to reuse.
 *
 * Update item to represent the nth piece of data.
 */
void
e_reflow_model_reincarnate (EReflowModel *reflow_model,
                            gint n,
                            GnomeCanvasItem *item)
{
	EReflowModelClass *class;

	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	class = E_REFLOW_MODEL_GET_CLASS (reflow_model);
	g_return_if_fail (class->reincarnate != NULL);

	class->reincarnate (reflow_model, n, item);
}

static void
e_reflow_model_class_init (EReflowModelClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	class->set_width = NULL;
	class->count = NULL;
	class->height = NULL;
	class->incarnate = NULL;
	class->reincarnate = NULL;

	class->model_changed = NULL;
	class->comparison_changed = NULL;
	class->model_items_inserted = NULL;
	class->model_item_removed = NULL;
	class->model_item_changed = NULL;

	signals[MODEL_CHANGED] = g_signal_new (
		"model_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowModelClass, model_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMPARISON_CHANGED] = g_signal_new (
		"comparison_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowModelClass, comparison_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[MODEL_ITEMS_INSERTED] = g_signal_new (
		"model_items_inserted",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowModelClass, model_items_inserted),
		NULL, NULL,
		e_marshal_NONE__INT_INT,
		G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

	signals[MODEL_ITEM_CHANGED] = g_signal_new (
		"model_item_changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowModelClass, model_item_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	signals[MODEL_ITEM_REMOVED] = g_signal_new (
		"model_item_removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EReflowModelClass, model_item_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
e_reflow_model_init (EReflowModel *e_reflow_model)
{
}

#if d(!)0
static void
print_tabs (void)
{
	gint i;
	for (i = 0; i < depth; i++)
		g_print ("\t");
}
#endif

/**
 * e_reflow_model_changed:
 * @reflow_model: the reflow model to notify of the change
 *
 * Use this function to notify any views of this reflow model that
 * the contents of the reflow model have changed.  This will emit
 * the signal "model_changed" on the @reflow_model object.
 *
 * It is preferable to use the e_reflow_model_item_changed() signal to
 * notify of smaller changes than to invalidate the entire model, as
 * the views might have ways of caching the information they render
 * from the model.
 */
void
e_reflow_model_changed (EReflowModel *reflow_model)
{
	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	d (print_tabs ());
	d (g_print ("Emitting model_changed on model 0x%p.\n", reflow_model));
	d (depth++);
	g_signal_emit (reflow_model, signals[MODEL_CHANGED], 0);
	d (depth--);
}

/**
 * e_reflow_model_comparison_changed:
 * @reflow_model: the reflow model to notify of the change
 *
 * Use this function to notify any views of this reflow model that the
 * sorting has changed.  The actual contents of the items hasn't, so
 * there's no need to re-query the model for the heights of the
 * individual items.
 */
void
e_reflow_model_comparison_changed (EReflowModel *reflow_model)
{
	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	d (print_tabs ());
	d (g_print (
		"Emitting comparison_changed on model 0x%p.\n",
		reflow_model));
	d (depth++);
	g_signal_emit (reflow_model, signals[COMPARISON_CHANGED], 0);
	d (depth--);
}

/**
 * e_reflow_model_items_inserted:
 * @reflow_model: The model changed.
 * @position: The position the items were insert in.
 * @count: The number of items inserted.
 *
 * Use this function to notify any views of the reflow model that a number
 * of items have been inserted.
 **/
void
e_reflow_model_items_inserted (EReflowModel *reflow_model,
                               gint position,
                               gint count)
{
	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	d (print_tabs ());
	d (depth++);
	g_signal_emit (
		reflow_model,
		signals[MODEL_ITEMS_INSERTED], 0,
		position, count);
	d (depth--);
}

/**
 * e_reflow_model_item_removed:
 * @reflow_model: The model changed.
 * @n: The position from which the items were removed.
 *
 * Use this function to notify any views of the reflow model that an
 * item has been removed.
 **/
void
e_reflow_model_item_removed (EReflowModel *reflow_model,
                             gint n)
{
	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	d (print_tabs ());
	d (depth++);
	g_signal_emit (reflow_model, signals[MODEL_ITEM_REMOVED], 0, n);
	d (depth--);
}

/**
 * e_reflow_model_item_changed:
 * @reflow_model: the reflow model to notify of the change
 * @n: the item that was changed in the model.
 *
 * Use this function to notify any views of the reflow model that the
 * contents of item @item have changed in model such that the height
 * has changed or the item needs to be reincarnated.  This function
 * will emit the "model_item_changed" signal on the @reflow_model
 * object
 */
void
e_reflow_model_item_changed (EReflowModel *reflow_model,
                             gint n)
{
	g_return_if_fail (E_IS_REFLOW_MODEL (reflow_model));

	d (print_tabs ());
	d (g_print ("Emitting item_changed on model 0x%p, n=%d.\n", reflow_model, n));
	d (depth++);
	g_signal_emit (reflow_model, signals[MODEL_ITEM_CHANGED], 0, n);
	d (depth--);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-reflow-model.c: a Reflow Model
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2001 Ximian, Inc.
 */
#include <config.h>
#include "e-reflow-model.h"
#include <gtk/gtksignal.h>

#define ERM_CLASS(e) ((EReflowModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

#define d(x)

d(static gint depth = 0);


static GtkObjectClass *e_reflow_model_parent_class;

enum {
	MODEL_CHANGED,
	MODEL_ITEMS_INSERTED,
	MODEL_ITEM_CHANGED,
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

	ERM_CLASS (e_reflow_model)->set_width (e_reflow_model, width);
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

	return ERM_CLASS (e_reflow_model)->count (e_reflow_model);
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

	return ERM_CLASS (e_reflow_model)->height (e_reflow_model, n, parent);
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

	return ERM_CLASS (e_reflow_model)->incarnate (e_reflow_model, n, parent);
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

	return ERM_CLASS (e_reflow_model)->compare (e_reflow_model, n1, n2);
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

	ERM_CLASS (e_reflow_model)->reincarnate (e_reflow_model, n, item);
}

static void
e_reflow_model_class_init (GtkObjectClass *object_class)
{
	EReflowModelClass *klass = E_REFLOW_MODEL_CLASS(object_class);
	e_reflow_model_parent_class = gtk_type_class (PARENT_TYPE);

	e_reflow_model_signals [MODEL_CHANGED] =
		gtk_signal_new ("model_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EReflowModelClass, model_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_reflow_model_signals [MODEL_ITEMS_INSERTED] =
		gtk_signal_new ("model_items_inserted",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EReflowModelClass, model_items_inserted),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_reflow_model_signals [MODEL_ITEM_CHANGED] =
		gtk_signal_new ("model_item_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EReflowModelClass, model_item_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, e_reflow_model_signals, LAST_SIGNAL);

	klass->set_width            = NULL;
	klass->count                = NULL;
	klass->height               = NULL;
	klass->incarnate            = NULL;
	klass->reincarnate          = NULL;

	klass->model_changed        = NULL;
	klass->model_items_inserted = NULL;
	klass->model_item_changed   = NULL;
}


guint
e_reflow_model_get_type (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"EReflowModel",
			sizeof (EReflowModel),
			sizeof (EReflowModelClass),
			(GtkClassInitFunc) e_reflow_model_class_init,
			NULL,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

  return type;
}

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
	gtk_signal_emit (GTK_OBJECT (e_reflow_model),
			 e_reflow_model_signals [MODEL_CHANGED]);
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
	gtk_signal_emit (GTK_OBJECT (e_reflow_model),
			 e_reflow_model_signals [MODEL_ITEMS_INSERTED], position, count);
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
	gtk_signal_emit (GTK_OBJECT (e_reflow_model),
			 e_reflow_model_signals [MODEL_ITEM_CHANGED], n);
	d(depth--);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-simple.c: a Tree Model that offers a function pointer
 * interface to using ETreeModel, similar to ETableSimple.
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.  */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-util/e-util.h"
#include "e-tree-simple.h"

#define PARENT_TYPE E_TREE_MODEL_TYPE

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
e_tree_simple_class_init (GtkObjectClass *object_class)
{
	ETreeModelClass *model_class = (ETreeModelClass *) object_class;

	model_class->icon_at = simple_icon_at;
	model_class->value_at = simple_value_at;
	model_class->set_value_at = simple_set_value_at;
	model_class->is_editable = simple_is_editable;
}

E_MAKE_TYPE(e_tree_simple, "ETreeSimple", ETreeSimple, e_tree_simple_class_init, NULL, PARENT_TYPE)

ETreeModel *
e_tree_simple_new (ETreeSimpleIconAtFn icon_at,
		   ETreeSimpleValueAtFn value_at,
		   ETreeSimpleSetValueAtFn set_value_at,
		   ETreeSimpleIsEditableFn is_editable,
		   gpointer model_data)
{
	ETreeSimple *etg;

	etg = gtk_type_new (e_tree_simple_get_type ());

	e_tree_model_construct (E_TREE_MODEL (etg));

	etg->icon_at = icon_at;
	etg->value_at = value_at;
	etg->set_value_at = set_value_at;
	etg->is_editable = is_editable;
	etg->model_data = model_data;

	return (ETreeModel*)etg;
}


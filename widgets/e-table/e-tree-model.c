/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-model.c: a Tree Model
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * Adapted from the gtree code and ETableModel.
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-util/e-util.h"
#include "e-tree-model.h"

#define ETM_CLASS(e) ((ETreeModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE E_TABLE_MODEL_TYPE

static ETableModel *e_tree_model_parent_class;

/* virtual methods */

static ETreePath*
etree_get_root (ETreeModel *etm)
{
	/* shouldn't be called */
	g_assert(0);
	return NULL;
}

static void*
etree_value_at (ETreeModel *etm, ETreePath* node, int col)
{
	/* shouldn't be called */
	g_assert(0);
	return NULL;
}

static void
etree_set_value_at (ETreeModel *etm, ETreePath* node, int col, const void *val)
{
	/* shouldn't be called */
	g_assert(0);
}

static gboolean
etree_is_editable (ETreeModel *etm, ETreePath* node, int col)
{
	/* shouldn't be called */
	g_assert(0);
	return FALSE;
}

static gboolean
etree_is_expanded (ETreeModel *etm, ETreePath* node)
{
	/* shouldn't be called */
	g_assert(0);
	return FALSE;
}

static guint
etree_get_children (ETreeModel *etm, ETreePath* node, ETreePath ***paths)
{
	/* shouldn't be called */
	g_assert(0);
	return 0;
}

static void
etree_release_paths (ETreeModel *etm, ETreePath **paths, guint num_paths)
{
	/* shouldn't be called */
	g_assert(0);
}

static void
etree_set_expanded (ETreeModel *etm, ETreePath* node, gboolean expanded)
{
	/* shouldn't be called */
	g_assert(0);
}

static void
etree_destroy (GtkObject *object)
{
}

guint
e_tree_model_node_num_visible_descendents (ETreeModel *etm, ETreePath *node)
{
	int count = 1;
	if (e_tree_model_node_is_expanded (etm, node)) {
		ETreePath **paths;
		int i;
		int num_paths;

		num_paths = e_tree_model_node_get_children (etm, node, &paths);

		for (i = 0; i < num_paths; i ++)
			count += e_tree_model_node_num_visible_descendents(etm, paths[i]);

		e_tree_model_release_paths (etm, paths, num_paths);
	}

	return count;
}

static int
etable_row_count (ETableModel *etm)
{
	return e_tree_model_node_num_visible_descendents (E_TREE_MODEL (etm), e_tree_model_get_root (E_TREE_MODEL (etm)));
}

static void *
etable_value_at (ETableModel *etm, int col, int row)
{
	ETreeModel *etree = E_TREE_MODEL(etm);
	ETreeModelClass *et_class = ETM_CLASS(etm);
	ETreePath* node = e_tree_model_node_at_row (etree, row);

	g_return_val_if_fail (node, NULL);

	if (col == -1)
		return node;
	else if (col == -2)
		return etm;
	else
		return et_class->value_at (etree, node, col);
}

static void
etable_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETreeModel *etree = E_TREE_MODEL(etm);
	ETreeModelClass *et_class = ETM_CLASS(etm);
	ETreePath* node = e_tree_model_node_at_row (etree, row);

	g_return_if_fail (node);

	et_class->set_value_at (etree, node, col, val);
}

static gboolean
etable_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETreeModel *etree = E_TREE_MODEL(etm);
	ETreeModelClass *et_class = ETM_CLASS(etm);
	ETreePath* node = e_tree_model_node_at_row (etree, row);

	g_return_val_if_fail (node, FALSE);

	return et_class->is_editable (etree, node, col);
}

static void
e_tree_model_class_init (GtkObjectClass *klass)
{
	ETableModelClass *table_class = (ETableModelClass *) klass;
	ETreeModelClass *tree_class = (ETreeModelClass *) klass;

	e_tree_model_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etree_destroy;

	table_class->row_count        = etable_row_count;
	table_class->value_at         = etable_value_at;
	table_class->set_value_at     = etable_set_value_at;
	table_class->is_cell_editable = etable_is_cell_editable;
#if 0
	table_class->duplicate_value  = etable_duplicate_value;
	table_class->free_value       = etable_free_value;
	table_class->initialize_value = etable_initialize_value;
	table_class->value_is_empty   = etable_value_is_empty;
	table_class->thaw             = etable_thaw;
#endif

	tree_class->get_root          = etree_get_root;
	tree_class->value_at          = etree_value_at;
	tree_class->set_value_at      = etree_set_value_at;
	tree_class->is_editable       = etree_is_editable;

	tree_class->get_children      = etree_get_children;
	tree_class->release_paths     = etree_release_paths;
	tree_class->is_expanded       = etree_is_expanded;
	tree_class->set_expanded      = etree_set_expanded;
}

E_MAKE_TYPE(e_tree_model, "ETreeModel", ETreeModel, e_tree_model_class_init, NULL, PARENT_TYPE)

/* signals */

ETreeModel *
e_tree_model_new ()
{
	ETreeModel *et;

	et = gtk_type_new (e_tree_model_get_type ());

	return et;
}

static ETreePath *
e_tree_model_node_at_row_1 (ETreeModel *etree, int *row, ETreePath *node)
{
	ETreePath *ret = NULL;

	if (*row == 0)
		ret = node;
	else if (e_tree_model_node_is_expanded (etree, node)) {
		int num_children;
		int i;
		ETreePath **paths;

		num_children = e_tree_model_node_get_children (etree, node, &paths);

		for (i = 0; i < num_children; i ++) {
			ETreePath *p;

			(*row) --;

			p = e_tree_model_node_at_row_1 (etree, row, paths[i]);

			if (p) {
				ret = p;
				break;
			}
		}

		/* XXX need to find why this release is causing problems */
		/* e_tree_model_release_paths (etree, paths, num_children); */
	}

	return ret;
}

ETreePath *
e_tree_model_get_root (ETreeModel *etree)
{
	return ETM_CLASS(etree)->get_root(etree);
}

ETreePath *
e_tree_model_node_at_row (ETreeModel *etree, int row)
{
	/* XXX icky, perform a depth first search of the tree.  we need this optimized sorely */
	return e_tree_model_node_at_row_1 (etree, &row, ETM_CLASS(etree)->get_root(etree));
}

ETreePath *
e_tree_model_node_get_next (ETreeModel *etree, ETreePath *node)
{
	return ETM_CLASS(etree)->get_next(etree, node);
}

ETreePath *
e_tree_model_node_get_prev (ETreeModel *etree, ETreePath *node)
{
	return ETM_CLASS(etree)->get_prev(etree, node);
}

guint
e_tree_model_node_depth (ETreeModel *etree, ETreePath *path)
{
	return g_list_length (path) - 1;
}

ETreePath *
e_tree_model_node_get_parent (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail (path, NULL);

	if (path->next == NULL)
		return NULL;
	else
		return g_list_copy (path->next);
}

gboolean
e_tree_model_node_is_root (ETreeModel *etree, ETreePath *path)
{
	return (e_tree_model_node_depth (etree, path) == 0);
}

gboolean
e_tree_model_node_is_expandable (ETreeModel *etree, ETreePath *path)
{
	return (e_tree_model_node_get_children (etree, path, NULL) > 0);
}

gboolean
e_tree_model_node_is_expanded (ETreeModel *etree, ETreePath *path)
{
	return ETM_CLASS(etree)->is_expanded (etree, path);
}

void
e_tree_model_node_set_expanded (ETreeModel *etree, ETreePath *path, gboolean expanded)
{
	ETM_CLASS(etree)->set_expanded (etree, path, expanded);
}

guint
e_tree_model_node_get_children (ETreeModel *etree, ETreePath *path, ETreePath ***paths)
{
	return ETM_CLASS(etree)->get_children (etree, path, paths);
}

void
e_tree_model_release_paths (ETreeModel *etree, ETreePath **paths, guint num_paths)
{
	ETM_CLASS(etree)->release_paths (etree, paths, num_paths);
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-gnode.c: a Tree Model that reflects a GNode structure visually.
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-util/e-util.h"
#include "e-tree-gnode.h"

#define PARENT_TYPE E_TREE_MODEL_TYPE

static ETreePath *
gnode_get_root (ETreeModel *etm)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  ETreePath *path = NULL;

  path = g_list_append(path, etg->root);

  return path;
}

static ETreePath *
gnode_get_prev (ETreeModel *etm, ETreePath *node)
{
  ETreePath *prev_path;

  GNode *gnode;
  GNode *prev_sibling;

  g_return_val_if_fail (node && node->data, NULL);

  gnode = (GNode*)node->data;
  prev_sibling = g_node_prev_sibling(gnode);

  if (!prev_sibling)
	  return NULL;

  prev_path = g_list_copy (node->next);
  prev_path = g_list_prepend (prev_path, prev_sibling);
  return prev_path;
}

static ETreePath *
gnode_get_next (ETreeModel *etm, ETreePath *node)
{
  ETreePath *next_path;
  GNode *gnode;
  GNode *next_sibling;

  g_return_val_if_fail (node && node->data, NULL);

  gnode = (GNode*)node->data;
  next_sibling = g_node_next_sibling(gnode);

  if (!next_sibling)
	  return NULL;

  next_path = g_list_copy (node->next);
  next_path = g_list_prepend (next_path, next_sibling);
  return next_path;
}

static void *
gnode_value_at (ETreeModel *etm, ETreePath *node, int col)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;

  g_return_val_if_fail (node && node->data, NULL);

  gnode = (GNode*)node->data;

  return etg->value_at (etm, gnode, col, etg->data);
}

static void
gnode_set_value_at (ETreeModel *etm, ETreePath *node, int col, const void *val)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;

  g_return_if_fail (node && node->data);

  gnode = (GNode*)node->data;

  /* XXX */
}

static gboolean
gnode_is_editable (ETreeModel *etm, ETreePath *node, int col)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;

  g_return_val_if_fail (node && node->data, FALSE);

  gnode = (GNode*)node->data;

  /* XXX */
  return FALSE;
}

static guint
gnode_get_children (ETreeModel *etm, ETreePath *node, ETreePath ***paths)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;
  guint n_children;

  g_return_val_if_fail (node && node->data, 0);

  gnode = (GNode*)node->data;

  n_children = g_node_n_children (gnode);

  if (paths)
    {
      int i;
      (*paths) = g_malloc (sizeof (ETreePath*) * n_children);
      for (i = 0; i < n_children; i ++) {
	(*paths)[i] = g_list_copy (node);
	(*paths)[i] = g_list_prepend ((*paths)[i], g_node_nth_child (gnode, i));
      }
    }

  return n_children;
}

static void
gnode_release_paths (ETreeModel *etm, ETreePath **paths, guint num_paths)
{
	guint i;
	g_return_if_fail (paths);

	for (i = 0; i < num_paths; i ++)
		g_list_free (paths[i]);
	g_free (paths);
}

static gboolean
gnode_is_expanded (ETreeModel *etm, ETreePath *node)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;

  g_return_val_if_fail (node && node->data, FALSE);

  gnode = (GNode*)node->data;

  return (gboolean)gnode->data;
}

static void
gnode_set_expanded (ETreeModel *etm, ETreePath *node, gboolean expanded)
{
  ETreeGNode *etg = E_TREE_GNODE (etm);
  GNode *gnode;
  int num_descendents;

  g_return_if_fail (node && node->data);

  gnode = (GNode*)node->data;

  /* XXX */
  gnode->data = (gpointer)expanded;

  e_table_model_changed (E_TABLE_MODEL(etm));
}

static void
e_tree_gnode_class_init (GtkObjectClass *object_class)
{
	ETreeModelClass *model_class = (ETreeModelClass *) object_class;

	model_class->get_root = gnode_get_root;
	model_class->get_next = gnode_get_next;
	model_class->get_prev = gnode_get_prev;
	model_class->value_at = gnode_value_at;
	model_class->set_value_at = gnode_set_value_at;
	model_class->is_editable = gnode_is_editable;
	model_class->get_children = gnode_get_children;
	model_class->release_paths = gnode_release_paths;
	model_class->is_expanded = gnode_is_expanded;
	model_class->set_expanded = gnode_set_expanded;
}

E_MAKE_TYPE(e_tree_gnode, "ETreeGNode", ETreeGNode, e_tree_gnode_class_init, NULL, PARENT_TYPE)

ETreeModel *
e_tree_gnode_new (GNode *root_node,
		  ETreeGNodeValueAtFn value_at,
		  void *data)
{
	ETreeGNode *etg;

	etg = gtk_type_new (e_tree_gnode_get_type ());

	etg->root = root_node;

	etg->value_at = value_at;
	etg->data = data;

	return (ETreeModel*)etg;
}

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-tree-memory.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-xml-utils.h"

#define E_TREE_MEMORY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TREE_MEMORY, ETreeMemoryPrivate))

G_DEFINE_TYPE (ETreeMemory, e_tree_memory, E_TYPE_TREE_MODEL)

struct _ETreeMemoryPrivate {
	GNode *root;

	/* whether nodes are created expanded
	 * or collapsed by default */
	gboolean expanded_default;

	gint frozen;
};

static void
tree_memory_finalize (GObject *object)
{
	ETreeMemoryPrivate *priv;

	priv = E_TREE_MEMORY_GET_PRIVATE (object);

	if (priv->root != NULL)
		g_node_destroy (priv->root);

	G_OBJECT_CLASS (e_tree_memory_parent_class)->finalize (object);
}

static ETreePath
tree_memory_get_root (ETreeModel *etm)
{
	ETreeMemory *tree_memory = E_TREE_MEMORY (etm);

	return tree_memory->priv->root;
}

static ETreePath
tree_memory_get_parent (ETreeModel *etm,
                        ETreePath path)
{
	return ((GNode *) path)->parent;
}

static ETreePath
tree_memory_get_first_child (ETreeModel *etm,
                             ETreePath path)
{
	return g_node_first_child ((GNode *) path);
}

static ETreePath
tree_memory_get_next (ETreeModel *etm,
                      ETreePath path)
{
	return g_node_next_sibling ((GNode *) path);
}

static gboolean
tree_memory_is_root (ETreeModel *etm,
                     ETreePath path)
{
	return G_NODE_IS_ROOT ((GNode *) path);
}

static gboolean
tree_memory_is_expandable (ETreeModel *etm,
                           ETreePath path)
{
	return (g_node_first_child ((GNode *) path) != NULL);
}

static guint
tree_memory_get_n_children (ETreeModel *etm,
                            ETreePath path)
{
	return g_node_n_children ((GNode *) path);
}

static guint
tree_memory_depth (ETreeModel *etm,
                   ETreePath path)
{
	return g_node_depth ((GNode *) path);
}

static gboolean
tree_memory_get_expanded_default (ETreeModel *etm)
{
	ETreeMemory *tree_memory = E_TREE_MEMORY (etm);

	return tree_memory->priv->expanded_default;
}

static void
e_tree_memory_class_init (ETreeMemoryClass *class)
{
	GObjectClass *object_class;
	ETreeModelClass *tree_model_class;

	g_type_class_add_private (class, sizeof (ETreeMemoryPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = tree_memory_finalize;

	tree_model_class = E_TREE_MODEL_CLASS (class);
	tree_model_class->get_root = tree_memory_get_root;
	tree_model_class->get_next = tree_memory_get_next;
	tree_model_class->get_first_child = tree_memory_get_first_child;
	tree_model_class->get_parent = tree_memory_get_parent;

	tree_model_class->is_root = tree_memory_is_root;
	tree_model_class->is_expandable = tree_memory_is_expandable;
	tree_model_class->get_n_children = tree_memory_get_n_children;
	tree_model_class->depth = tree_memory_depth;
	tree_model_class->get_expanded_default = tree_memory_get_expanded_default;
}

static void
e_tree_memory_init (ETreeMemory *tree_memory)
{
	tree_memory->priv = E_TREE_MEMORY_GET_PRIVATE (tree_memory);
}

/**
 * e_tree_memory_node_insert:
 * @tree_memory:
 * @parent_node:
 * @position:
 * @data:
 *
 *
 *
 * Returns:
 **/
ETreePath
e_tree_memory_node_insert (ETreeMemory *tree_memory,
                           ETreePath parent_node,
                           gint position,
                           gpointer data)
{
	GNode *new_path;
	GNode *parent_path = parent_node;

	g_return_val_if_fail (E_IS_TREE_MEMORY (tree_memory), NULL);

	if (parent_path == NULL)
		g_return_val_if_fail (tree_memory->priv->root == NULL, NULL);

	if (!tree_memory->priv->frozen)
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));

	new_path = g_node_new (data);

	if (parent_path != NULL) {
		g_node_insert (parent_path, position, new_path);
		if (!tree_memory->priv->frozen)
			e_tree_model_node_inserted (
				E_TREE_MODEL (tree_memory),
				parent_path, new_path);
	} else {
		tree_memory->priv->root = new_path;
		if (!tree_memory->priv->frozen)
			e_tree_model_node_changed (
				E_TREE_MODEL (tree_memory), new_path);
	}

	return new_path;
}

/**
 * e_tree_memory_node_remove:
 * @tree_memory:
 * @node:
 *
 *
 *
 * Returns:
 **/
gpointer
e_tree_memory_node_remove (ETreeMemory *tree_memory,
                           ETreePath node)
{
	GNode *path = node;
	GNode *parent = path->parent;
	GNode *sibling;
	gpointer ret = path->data;
	gint old_position = 0;

	g_return_val_if_fail (E_IS_TREE_MEMORY (tree_memory), NULL);

	if (!tree_memory->priv->frozen) {
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));
		for (old_position = 0, sibling = path;
		     sibling;
		     old_position++, sibling = sibling->prev)
			/* Empty intentionally*/;
		old_position--;
	}

	/* unlink this node - we only have to unlink the root node being
	 * removed, since the others are only references from this node */
	g_node_unlink (path);

	/*printf("removing %d nodes from position %d\n", visible, base);*/
	if (!tree_memory->priv->frozen)
		e_tree_model_node_removed (
			E_TREE_MODEL (tree_memory),
			parent, path, old_position);

	g_node_destroy (path);

	if (path == tree_memory->priv->root)
		tree_memory->priv->root = NULL;

	if (!tree_memory->priv->frozen)
		e_tree_model_node_deleted (E_TREE_MODEL (tree_memory), path);

	return ret;
}

/**
 * e_tree_memory_freeze:
 * @tree_memory: the ETreeModel to freeze.
 *
 * This function prepares an ETreeModel for a period of much change.
 * All signals regarding changes to the tree are deferred until we
 * thaw the tree.
 *
 **/
void
e_tree_memory_freeze (ETreeMemory *tree_memory)
{
	g_return_if_fail (E_IS_TREE_MEMORY (tree_memory));

	if (tree_memory->priv->frozen == 0)
		e_tree_model_pre_change (E_TREE_MODEL (tree_memory));

	tree_memory->priv->frozen++;
}

/**
 * e_tree_memory_thaw:
 * @tree_memory: the ETreeMemory to thaw.
 *
 * This function thaws an ETreeMemory.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 *
 **/
void
e_tree_memory_thaw (ETreeMemory *tree_memory)
{
	g_return_if_fail (E_IS_TREE_MEMORY (tree_memory));

	if (tree_memory->priv->frozen > 0)
		tree_memory->priv->frozen--;

	if (tree_memory->priv->frozen == 0)
		e_tree_model_node_changed (
			E_TREE_MODEL (tree_memory),
			tree_memory->priv->root);
}

/**
 * e_tree_memory_set_expanded_default
 *
 * Sets the state of nodes to be append to a thread.
 * They will either be expanded or collapsed, according to
 * the value of @expanded.
 */
void
e_tree_memory_set_expanded_default (ETreeMemory *tree_memory,
                                    gboolean expanded)
{
	g_return_if_fail (E_IS_TREE_MEMORY (tree_memory));

	tree_memory->priv->expanded_default = expanded;
}

/**
 * e_tree_memory_node_get_data:
 * @tree_memory:
 * @path:
 *
 *
 *
 * Return value:
 **/
gpointer
e_tree_memory_node_get_data (ETreeMemory *tree_memory,
                             ETreePath path)
{
	g_return_val_if_fail (path != NULL, NULL);

	return ((GNode *) path)->data;
}

/**
 * e_tree_memory_node_set_data:
 * @tree_memory:
 * @path:
 * @data:
 *
 *
 **/
void
e_tree_memory_node_set_data (ETreeMemory *tree_memory,
                             ETreePath path,
                             gpointer data)
{
	g_return_if_fail (path != NULL);

	((GNode *) path)->data = data;
}


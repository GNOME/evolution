/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-memory.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Chris Toshok <toshok@ximian.com>
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"

#include "e-tree-memory.h"

#define TREEPATH_CHUNK_AREA_SIZE (30 * sizeof (ETreeMemoryPath))

static ETreeModelClass *parent_class;
static GMemChunk  *node_chunk;

enum {
	FILL_IN_CHILDREN,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

typedef struct ETreeMemoryPath ETreeMemoryPath;

struct ETreeMemoryPath {
	gpointer         node_data;

	guint            children_computed : 1;

	/* parent/child/sibling pointers */
	ETreeMemoryPath *parent;
	ETreeMemoryPath *next_sibling;
	ETreeMemoryPath *prev_sibling;
	ETreeMemoryPath *first_child;
	ETreeMemoryPath *last_child;

	gint             num_children;
};

struct ETreeMemoryPriv {
	ETreeMemoryPath *root;
	gboolean         expanded_default; /* whether nodes are created expanded or collapsed by default */
	gint             frozen;
	GFunc            destroy_func;
	gpointer         destroy_user_data;
};


/* ETreeMemoryPath functions */

static inline void
check_children (ETreeMemory *memory, ETreePath node)
{
	ETreeMemoryPath *path = node;
	if (!path->children_computed) {
		g_signal_emit (G_OBJECT (memory), signals[FILL_IN_CHILDREN], 0, node);
		path->children_computed = TRUE;
	}
}

static int
e_tree_memory_path_depth (ETreeMemoryPath *path)
{
	int depth = 0;

	g_return_val_if_fail(path != NULL, -1);

	for ( path = path->parent; path; path = path->parent)
		depth ++;
	return depth;
}

static void
e_tree_memory_path_insert (ETreeMemoryPath *parent, int position, ETreeMemoryPath *child)
{
	g_return_if_fail (position <= parent->num_children && position >= -1);

	child->parent = parent;

	if (parent->first_child == NULL)
		parent->first_child = child;

	if (position == -1 || position == parent->num_children) {
		child->prev_sibling = parent->last_child;
		if (parent->last_child)
			parent->last_child->next_sibling = child;
		parent->last_child = child;
	} else {
		ETreeMemoryPath *c;
		for (c = parent->first_child; c; c = c->next_sibling) {
			if (position == 0) {
				child->next_sibling = c;
				child->prev_sibling = c->prev_sibling;

				if (child->next_sibling)
					child->next_sibling->prev_sibling = child;
				if (child->prev_sibling)
					child->prev_sibling->next_sibling = child;

				if (parent->first_child == c)
					parent->first_child = child;
				break;
			}
			position --;
		}
	}

	parent->num_children++;
}

static void
e_tree_path_unlink (ETreeMemoryPath *path)
{
	ETreeMemoryPath *parent = path->parent;

	/* unlink first/last child if applicable */
	if (parent) {
		if (path == parent->first_child)
			parent->first_child = path->next_sibling;
		if (path == parent->last_child)
			parent->last_child = path->prev_sibling;

		parent->num_children --;
	}

	/* unlink prev/next sibling links */
	if (path->next_sibling)
		path->next_sibling->prev_sibling = path->prev_sibling;
	if (path->prev_sibling)
		path->prev_sibling->next_sibling = path->next_sibling;

	path->parent = NULL;
	path->next_sibling = NULL;
	path->prev_sibling = NULL;
}



/**
 * e_tree_memory_freeze:
 * @etmm: the ETreeModel to freeze.
 * 
 * This function prepares an ETreeModel for a period of much change.
 * All signals regarding changes to the tree are deferred until we
 * thaw the tree.
 * 
 **/
void
e_tree_memory_freeze(ETreeMemory *etmm)
{
	ETreeMemoryPriv *priv = etmm->priv;

	if (priv->frozen == 0)
		e_tree_model_pre_change(E_TREE_MODEL(etmm));

	priv->frozen ++;
}

/**
 * e_tree_memory_thaw:
 * @etmm: the ETreeMemory to thaw.
 * 
 * This function thaws an ETreeMemory.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 * 
 **/
void
e_tree_memory_thaw(ETreeMemory *etmm)
{
	ETreeMemoryPriv *priv = etmm->priv;

	if (priv->frozen > 0)
		priv->frozen --;
	if (priv->frozen == 0) {
		e_tree_model_node_changed(E_TREE_MODEL(etmm), priv->root);
	}
}


/* virtual methods */

static void
etmm_dispose (GObject *object)
{
	ETreeMemory *etmm = E_TREE_MEMORY (object);
	ETreeMemoryPriv *priv = etmm->priv;

	if (priv) {
	/* XXX lots of stuff to free here */

		if (priv->root)
			e_tree_memory_node_remove (etmm, priv->root);

		g_free (priv);
	}
	etmm->priv = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static ETreePath
etmm_get_root (ETreeModel *etm)
{
	ETreeMemoryPriv *priv = E_TREE_MEMORY(etm)->priv;
	return priv->root;
}

static ETreePath
etmm_get_parent (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->parent;
}

static ETreePath
etmm_get_first_child (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->first_child;
}

static ETreePath
etmm_get_last_child (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->last_child;
}

static ETreePath
etmm_get_next (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->next_sibling;
}

static ETreePath
etmm_get_prev (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;
	return path->prev_sibling;
}

static gboolean
etmm_is_root (ETreeModel *etm, ETreePath node)
{
 	ETreeMemoryPath *path = node;
	return e_tree_memory_path_depth (path) == 0;
}

static gboolean
etmm_is_expandable (ETreeModel *etm, ETreePath node)
{
	ETreeMemoryPath *path = node;

	check_children (E_TREE_MEMORY (etm), node);
	return path->first_child != NULL;
}

static guint
etmm_get_children (ETreeModel *etm, ETreePath node, ETreePath **nodes)
{
	ETreeMemoryPath *path = node;
	guint n_children;

	check_children (E_TREE_MEMORY (etm), node);

	n_children = path->num_children;

	if (nodes) {
		ETreeMemoryPath *p;
		int i = 0;

		(*nodes) = g_new (ETreePath, n_children);
		for (p = path->first_child; p; p = p->next_sibling) {
			(*nodes)[i++] = p;
		}
	}

	return n_children;
}

static guint
etmm_depth (ETreeModel *etm, ETreePath path)
{
	return e_tree_memory_path_depth(path);
}

static gboolean
etmm_get_expanded_default (ETreeModel *etm)
{
	ETreeMemory *etmm = E_TREE_MEMORY (etm);
	ETreeMemoryPriv *priv = etmm->priv;

	return priv->expanded_default;
}

static void
etmm_clear_children_computed (ETreeMemoryPath *path)
{
	for (path = path->first_child; path; path = path->next_sibling) {
		path->children_computed = FALSE;
		etmm_clear_children_computed (path);
	}
}

static void
etmm_node_request_collapse (ETreeModel *etm, ETreePath node)
{
	if (node)
		etmm_clear_children_computed (node);

	if (parent_class->node_request_collapse) {
		parent_class->node_request_collapse (etm, node);
	}
}


static void
e_tree_memory_class_init (ETreeMemoryClass *klass)
{
	ETreeModelClass *tree_class = (ETreeModelClass *) klass;
	GObjectClass  *object_class = (GObjectClass *) klass;

	parent_class                     = g_type_class_peek_parent (klass);
	
	node_chunk                       = g_mem_chunk_create (ETreeMemoryPath, TREEPATH_CHUNK_AREA_SIZE, G_ALLOC_AND_FREE);

	signals [FILL_IN_CHILDREN] =
		g_signal_new ("fill_in_children",
			      E_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeMemoryClass, fill_in_children),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	object_class->dispose             = etmm_dispose;

	tree_class->get_root              = etmm_get_root;
	tree_class->get_prev              = etmm_get_prev;
	tree_class->get_next              = etmm_get_next;
	tree_class->get_first_child       = etmm_get_first_child;
	tree_class->get_last_child        = etmm_get_last_child;
	tree_class->get_parent            = etmm_get_parent;

	tree_class->is_root               = etmm_is_root;
	tree_class->is_expandable         = etmm_is_expandable;
	tree_class->get_children          = etmm_get_children;
	tree_class->depth                 = etmm_depth;
	tree_class->get_expanded_default  = etmm_get_expanded_default;

	tree_class->node_request_collapse = etmm_node_request_collapse;

	klass->fill_in_children           = NULL;
}

static void
e_tree_memory_init (GObject *object)
{
	ETreeMemory *etmm = (ETreeMemory *)object;

	ETreeMemoryPriv *priv;

	priv = g_new0 (ETreeMemoryPriv, 1);
	etmm->priv = priv;

	priv->root = NULL;
	priv->frozen = 0;
	priv->expanded_default = 0;
	priv->destroy_func = NULL;
	priv->destroy_user_data = NULL;
}

E_MAKE_TYPE(e_tree_memory, "ETreeMemory", ETreeMemory, e_tree_memory_class_init, e_tree_memory_init, E_TREE_MODEL_TYPE)



/**
 * e_tree_memory_construct:
 * @etree: 
 * 
 * 
 **/
void
e_tree_memory_construct (ETreeMemory *etmm)
{
}

/**
 * e_tree_memory_new
 *
 * XXX docs here.
 *
 * return values: a newly constructed ETreeMemory.
 */
ETreeMemory *
e_tree_memory_new (void)
{
	return (ETreeMemory *) g_object_new (E_TREE_MEMORY_TYPE, NULL);
}

void
e_tree_memory_set_expanded_default         (ETreeMemory *etree, gboolean expanded)
{
	etree->priv->expanded_default = expanded;
}

/**
 * e_tree_memory_node_get_data:
 * @etmm: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
gpointer
e_tree_memory_node_get_data (ETreeMemory *etmm, ETreePath node)
{
	ETreeMemoryPath *path = node;

	g_return_val_if_fail (path, NULL);

	return path->node_data;
}

/**
 * e_tree_memory_node_set_data:
 * @etmm: 
 * @node: 
 * @node_data: 
 * 
 * 
 **/
void
e_tree_memory_node_set_data (ETreeMemory *etmm, ETreePath node, gpointer node_data)
{
	ETreeMemoryPath *path = node;

	g_return_if_fail (path);

	path->node_data = node_data;
}

/**
 * e_tree_memory_node_insert:
 * @tree_model: 
 * @parent_path: 
 * @position: 
 * @node_data: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath
e_tree_memory_node_insert (ETreeMemory *tree_model,
			   ETreePath parent_node,
			   int position,
			   gpointer node_data)
{
	ETreeMemoryPriv *priv;
	ETreeMemoryPath *new_path;
	ETreeMemoryPath *parent_path = parent_node;

	g_return_val_if_fail(tree_model != NULL, NULL);

	priv = tree_model->priv;

	g_return_val_if_fail (parent_path != NULL || priv->root == NULL, NULL);

	priv = tree_model->priv;

	if (!tree_model->priv->frozen)
		e_tree_model_pre_change(E_TREE_MODEL(tree_model));

	new_path = g_chunk_new0 (ETreeMemoryPath, node_chunk);

	new_path->node_data = node_data;
	new_path->children_computed = FALSE;

	if (parent_path != NULL) {
		e_tree_memory_path_insert (parent_path, position, new_path);
		if (!tree_model->priv->frozen)
			e_tree_model_node_inserted (E_TREE_MODEL(tree_model), parent_path, new_path);
	} else {
		priv->root = new_path;
		if (!tree_model->priv->frozen)
			e_tree_model_node_changed(E_TREE_MODEL(tree_model), new_path);
	}

	return new_path;
}

ETreePath e_tree_memory_node_insert_id     (ETreeMemory *etree, ETreePath parent, int position, gpointer node_data, char *id)
{
	return e_tree_memory_node_insert(etree, parent, position, node_data);
}

/**
 * e_tree_memory_node_insert_before:
 * @etree: 
 * @parent: 
 * @sibling: 
 * @node_data: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath
e_tree_memory_node_insert_before (ETreeMemory *etree,
				  ETreePath parent,
				  ETreePath sibling,
				  gpointer node_data)
{
	ETreeMemoryPath *child;
	ETreeMemoryPath *parent_path = parent;
	ETreeMemoryPath *sibling_path = sibling;
	int position = 0;

	g_return_val_if_fail(etree != NULL, NULL);

	if (sibling != NULL) {
		for (child = parent_path->first_child; child; child = child->next_sibling) {
			if (child == sibling_path)
				break;
			position ++;
		}
	} else
		position = parent_path->num_children;
	return e_tree_memory_node_insert (etree, parent, position, node_data);
}

/* just blows away child data, doesn't take into account unlinking/etc */
static void
child_free(ETreeMemory *etree, ETreeMemoryPath *node)
{
	ETreeMemoryPath *child, *next;

	child = node->first_child;
	while (child) {
		next = child->next_sibling;
		child_free(etree, child);
		child = next;
	}

	if (etree->priv->destroy_func) {
		etree->priv->destroy_func (node->node_data, etree->priv->destroy_user_data);
	}

	g_chunk_free(node, node_chunk);
}

/**
 * e_tree_memory_node_remove:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gpointer
e_tree_memory_node_remove (ETreeMemory *etree, ETreePath node)
{
	ETreeMemoryPath *path = node;
	ETreeMemoryPath *parent = path->parent;
	ETreeMemoryPath *sibling;
	gpointer ret = path->node_data;
	int old_position = 0;

	g_return_val_if_fail(etree != NULL, NULL);

	if (!etree->priv->frozen) {
		e_tree_model_pre_change(E_TREE_MODEL(etree));
		for (old_position = 0, sibling = path;
		     sibling; 
		     old_position++, sibling = sibling->prev_sibling)
			/* Empty intentionally*/;
		old_position --;
	}

	/* unlink this node - we only have to unlink the root node being removed,
	   since the others are only references from this node */
	e_tree_path_unlink (path);

	/*printf("removing %d nodes from position %d\n", visible, base);*/
	if (!etree->priv->frozen)
		e_tree_model_node_removed(E_TREE_MODEL(etree), parent, path, old_position);

	child_free(etree, path);

	if (path == etree->priv->root)
		etree->priv->root = NULL;

	if (!etree->priv->frozen)
		e_tree_model_node_deleted(E_TREE_MODEL(etree), path);

	return ret;
}

typedef struct {
	ETreeMemory *memory;
	gpointer closure;
	ETreeMemorySortCallback callback;
} MemoryAndClosure;

static int
sort_callback(const void *data1, const void *data2, gpointer user_data)
{
	ETreePath path1 = *(ETreePath *)data1;
	ETreePath path2 = *(ETreePath *)data2;
	MemoryAndClosure *mac = user_data;
	return (*mac->callback) (mac->memory, path1, path2, mac->closure);
}

void
e_tree_memory_sort_node             (ETreeMemory             *etmm,
				     ETreePath                node,
				     ETreeMemorySortCallback  callback,
				     gpointer                 user_data)
{
	ETreeMemoryPath **children;
	ETreeMemoryPath *child;
	int count;
	int i;
	ETreeMemoryPath *path = node;
	MemoryAndClosure mac;
	ETreeMemoryPath *last;

	e_tree_model_pre_change (E_TREE_MODEL (etmm));
	
	i = 0;
	for (child = path->first_child; child; child = child->next_sibling)
		i++;

	children = g_new(ETreeMemoryPath *, i);

	count = i;

	for (child = path->first_child, i = 0;
	     child;
	     child = child->next_sibling, i++) {
		children[i] = child;
	}

	mac.memory = etmm;
	mac.closure = user_data;
	mac.callback = callback;

	e_sort (children, count, sizeof (ETreeMemoryPath *), sort_callback, &mac);

	path->first_child = NULL;
	last = NULL;
	for (i = 0;
	     i < count;
	     i++) {
		children[i]->prev_sibling = last;
		if (last)
			last->next_sibling = children[i];
		else
			path->first_child = children[i];
		last = children[i];
	}
	if (last)
		last->next_sibling = NULL;

	path->last_child = last;

	g_free(children);

	e_tree_model_node_changed(E_TREE_MODEL(etmm), node);
}

void
e_tree_memory_set_node_destroy_func  (ETreeMemory             *etmm,
				      GFunc                    destroy_func,
				      gpointer                 user_data)
{
	etmm->priv->destroy_func = destroy_func;
	etmm->priv->destroy_user_data = user_data;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-model.c: a Tree Model
 *
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *
 * Adapted from the gtree code and ETableModel.
 *
 * (C) 2000 Ximian, Inc.
 */
#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <gtk/gtksignal.h>
#include <stdlib.h>
#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"
#include "e-tree-model.h"

#define ETM_CLASS(e) ((ETreeModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE E_TABLE_MODEL_TYPE

#define TREEPATH_CHUNK_AREA_SIZE (30 * sizeof (ETreePath))

static ETableModel *e_tree_model_parent_class;

struct ETreeModelPriv {
	GMemChunk  *node_chunk;
	ETreePath  *root;
	gboolean    root_visible;
	GArray     *row_array; /* used in the mapping between ETable and our tree */
	GHashTable *expanded_state; /* used for loading/saving expanded state */
	GString    *sort_group;	/* for caching the last sort group info */
	gboolean    expanded_default; /* whether nodes are created expanded or collapsed by default */
	gint        frozen;
};

struct ETreePath {
	gboolean             expanded;
	gboolean             expanded_set;
	guint                visible_descendents;
	char                *save_id;
	ETreePathCompareFunc compare;
	gpointer             node_data;

	/* parent/child/sibling pointers */
	ETreePath           *parent;
	ETreePath           *next_sibling;
	ETreePath           *prev_sibling;
	ETreePath           *first_child;
	ETreePath           *last_child;
	guint32              num_children;
};

enum {
	NODE_CHANGED,
	NODE_INSERTED,
	NODE_REMOVED,
	NODE_COLLAPSED,
	NODE_EXPANDED,
	LAST_SIGNAL
};

static guint e_tree_model_signals [LAST_SIGNAL] = {0, };

static void add_visible_descendents_to_array (ETreeModel *etm, ETreePath *node, int *row, int *count);


/* ETreePath functions */

static int
e_tree_path_depth (ETreePath *path)
{
	int depth = 0;
	while (path) {
		depth ++;
		path = path->parent;
	}
	return depth;
}

static void
e_tree_path_insert (ETreePath *parent, int position, ETreePath *child)
{
	g_return_if_fail (position <= parent->num_children || position == -1);

	child->parent = parent;

	if (parent->first_child == NULL)
		parent->first_child = child;

	if (position == -1 || position == parent->num_children) {
		child->prev_sibling = parent->last_child;
		if (parent->last_child)
			parent->last_child->next_sibling = child;
		parent->last_child = child;
	}
	else {
		ETreePath *c;
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
e_tree_path_unlink (ETreePath *path)
{
	ETreePath *parent = path->parent;

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
 * e_tree_model_node_traverse:
 * @model: 
 * @path: 
 * @func: 
 * @data: 
 * 
 * 
 **/
void
e_tree_model_node_traverse (ETreeModel *model, ETreePath *path, ETreePathFunc func, gpointer data)
{
	ETreePath *child;

	g_return_if_fail (path);

	child = path->first_child;

	while (child) {
		ETreePath *next_child = child->next_sibling;
		e_tree_model_node_traverse (model, child, func, data);
		if (func (model, child, data) == TRUE)
			return;

		child = next_child;
	}
}



/**
 * e_tree_model_freeze:
 * @etm: the ETreeModel to freeze.
 * 
 * This function prepares an ETreeModel for a period of much change.
 * All signals regarding changes to the tree are deferred until we
 * thaw the tree.
 * 
 **/
void
e_tree_model_freeze(ETreeModel *etm)
{
	ETreeModelPriv *priv = etm->priv;

	priv->frozen ++;
}

/**
 * e_tree_model_thaw:
 * @etm: the ETreeModel to thaw.
 * 
 * This function thaws an ETreeModel.  All the defered signals can add
 * up to a lot, we don't know - so we just emit a model_changed
 * signal.
 * 
 **/
void
e_tree_model_thaw(ETreeModel *etm)
{
	ETreeModelPriv *priv = etm->priv;

	if (priv->frozen > 0)
		priv->frozen --;
	if (priv->frozen == 0) {
		e_table_model_changed(E_TABLE_MODEL(etm));
	}
}


/* virtual methods */

static void
etree_destroy (GtkObject *object)
{
	ETreeModel *etree = E_TREE_MODEL (object);
	ETreeModelPriv *priv = etree->priv;

	/* XXX lots of stuff to free here */

	if (priv->root)
		e_tree_model_node_remove (etree, priv->root);

	g_array_free (priv->row_array, TRUE);
	g_hash_table_destroy (priv->expanded_state);

	g_string_free(priv->sort_group, TRUE);

	g_free (priv);

	GTK_OBJECT_CLASS (e_tree_model_parent_class)->destroy (object);
}

static ETreePath*
etree_get_root (ETreeModel *etm)
{
	ETreeModelPriv *priv = etm->priv;
	return priv->root;
}

static ETreePath*
etree_get_parent (ETreeModel *etm, ETreePath *path)
{
	g_return_val_if_fail (path, NULL);
	
	return path->parent;
}

static ETreePath*
etree_get_next (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return node->next_sibling;
}

static ETreePath*
etree_get_prev (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return node->prev_sibling;
}

static ETreePath*
etree_get_first_child (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return node->first_child;
}

static ETreePath*
etree_get_last_child (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return node->last_child;
}

static guint
etree_get_children (ETreeModel *etm, ETreePath* node, ETreePath ***paths)
{
	guint n_children;

	g_return_val_if_fail (node, 0);

	n_children = node->num_children;

	if (paths) {
		ETreePath *p;
		int i = 0;
		(*paths) = g_malloc (sizeof (ETreePath*) * n_children);
		for (p = node->first_child; p; p = p->next_sibling) {
			(*paths)[i++] = p;
		}
	}

	return n_children;
}

static gboolean
etree_is_expanded (ETreeModel *etm, ETreePath* node)
{
	g_return_val_if_fail (node, FALSE);
	
	return node->expanded;
}

static gboolean
etree_is_visible (ETreeModel *etm, ETreePath* node)
{
	g_return_val_if_fail (node, FALSE);

	for (node = node->parent; node; node = node->parent) {
		if (!node->expanded)
			return FALSE;
	}

	return TRUE;
}

static void
etree_set_expanded (ETreeModel *etm, ETreePath* node, gboolean expanded)
{
	ETreeModelPriv *priv = etm->priv;
	ETreePath *child;
	int row;

	g_return_if_fail (node);

	node->expanded_set = TRUE;

	if (node->expanded == expanded)
		return;

	if (expanded) {
		gboolean allow_expand = TRUE;
		e_tree_model_node_expanded (etm, node, &allow_expand);
		if (!allow_expand)
			return;
	}

	node->expanded = expanded;
	if (node->save_id) {
		g_hash_table_insert (priv->expanded_state, node->save_id, (gpointer)expanded);
	}

	/* if the node wasn't visible at present */
	if ((row = e_tree_model_row_of_node (etm, node)) == -1) {
		if (!expanded) {
			e_tree_model_node_collapsed (etm, node);
		}
		return;
	}
		
	row++;

	if (expanded) {
		ETreePath *parent;

		if (e_tree_model_node_is_visible (etm, node)) {
			node->visible_descendents = 0;
			for (child = node->first_child; child;
			     child = child->next_sibling) {
				add_visible_descendents_to_array (etm, child, &row, &node->visible_descendents);
			}
		}
		/* now iterate back up the tree, adding to our
		   ancestors' visible descendents */

		for (parent = node->parent; parent; parent = parent->parent) {
			parent->visible_descendents += node->visible_descendents;
		}
	}
	else {
		int i;
		ETreePath *parent;

		if (e_tree_model_node_is_visible (etm, node)) {
			for (i = 0; i < node->visible_descendents; i ++) {
				priv->row_array = g_array_remove_index (priv->row_array, row);
				e_table_model_row_deleted (E_TABLE_MODEL (etm), row);
			}
		}
		/* now iterate back up the tree, subtracting from our
		   ancestors' visible descendents */

		for (parent = node->parent; parent; parent = parent->parent) {
			parent->visible_descendents -= node->visible_descendents;
		}

		node->visible_descendents = 0;

		e_tree_model_node_collapsed (etm, node);
	}
}

/**
 * e_tree_model_set_expanded_default:
 * @etree: The ETreeModel we're setting the default expanded behavior on.
 * @expanded: Whether or not newly inserted parent nodes should be expanded by default.
 * 
 * 
 **/
void
e_tree_model_show_node (ETreeModel *etm, ETreePath* node)
{
	ETreePath *parent;

	parent = e_tree_model_node_get_parent(etm, node);
	if (parent) {
		e_tree_model_show_node(etm, parent);
		e_tree_model_node_set_expanded(etm, parent, TRUE);
	}
}

void
e_tree_model_set_expanded_default (ETreeModel *etree,
				   gboolean expanded)
{
	ETreeModelPriv *priv = etree->priv;

	priv->expanded_default = expanded;
}

/* fairly naive implementation */
static void
etree_set_expanded_recurse (ETreeModel *etm, ETreePath* node, gboolean expanded)
{
	ETreePath *child;

	e_tree_model_node_set_expanded (etm, node, expanded);

	for (child = node->first_child; child; child = child->next_sibling)
		e_tree_model_node_set_expanded_recurse (etm, child, expanded);
}

static ETreePath *
etree_node_at_row (ETreeModel *etree, int row)
{
	ETreeModelPriv *priv = etree->priv;

	g_return_val_if_fail (row < priv->row_array->len, NULL);

	return g_array_index (priv->row_array, ETreePath*, row);
}


/* ETable analogs */
static void*
etree_value_at (ETreeModel *etm, ETreePath* node, int col)
{
	/* shouldn't be called */
	g_assert (0);
	return NULL;
}

static GdkPixbuf*
etree_icon_at (ETreeModel *etm, ETreePath* node)
{
	/* shouldn't be called */
	g_assert (0);
	return NULL;
}

static void
etree_set_value_at (ETreeModel *etm, ETreePath* node, int col, const void *val)
{
	/* shouldn't be called */
	g_assert (0);
}

static gboolean
etree_is_editable (ETreeModel *etm, ETreePath* node, int col)
{
	/* shouldn't be called */
	g_assert(0);
	return FALSE;
}


/* ETable virtual functions we map */
static int
etable_row_count (ETableModel *etm)
{
	ETreeModel *tree = E_TREE_MODEL (etm);
	ETreeModelPriv *priv = tree->priv;
	return priv->row_array->len;
}

static void *
etable_value_at (ETableModel *etm, int col, int row)
{
	ETreeModel *etree = E_TREE_MODEL(etm);
	ETreeModelClass *et_class = ETM_CLASS(etm);
	ETreePath* node = e_tree_model_node_at_row (etree, row);

	if (node == NULL)
		g_warning ("node is NULL for row %d in etable_value_at\n", row);

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
build_sort_group(GString *out, ETreePath *node)
{
	if (node->parent) {
		build_sort_group(out, node->parent);
	}
	g_string_sprintfa(out, "/%p", node);
}

static const char *
etable_row_sort_group(ETableModel *etm, int row)
{
	ETreeModel *etree = E_TREE_MODEL(etm);
	ETreeModelPriv *priv = etree->priv;
	ETreePath* node = e_tree_model_node_at_row (etree, row);

	g_return_val_if_fail (node, "");

	g_string_truncate(priv->sort_group, 0);
	if (node)
		build_sort_group(priv->sort_group, node);

	return priv->sort_group->str;
}

static gboolean
etable_has_sort_group(ETableModel *etm)
{
	/* could optimise for the flat &/or unexpanded tree case */
	return TRUE;
}


static void
e_tree_model_class_init (GtkObjectClass *klass)
{
	ETableModelClass *table_class = (ETableModelClass *) klass;
	ETreeModelClass *tree_class = (ETreeModelClass *) klass;

	e_tree_model_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = etree_destroy;

	e_tree_model_signals [NODE_CHANGED] =
		gtk_signal_new ("node_changed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_INSERTED] =
		gtk_signal_new ("node_inserted",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_inserted),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_REMOVED] =
		gtk_signal_new ("node_removed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_removed),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_COLLAPSED] =
		gtk_signal_new ("node_collapsed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_collapsed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_EXPANDED] =
		gtk_signal_new ("node_expanded",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_expanded),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (klass, e_tree_model_signals, LAST_SIGNAL);

	table_class->row_count        = etable_row_count;
	table_class->value_at         = etable_value_at;
	table_class->set_value_at     = etable_set_value_at;
	table_class->is_cell_editable = etable_is_cell_editable;
#if 0
	/* XX need to pass these through */
	table_class->duplicate_value  = etable_duplicate_value;
	table_class->free_value       = etable_free_value;
	table_class->initialize_value = etable_initialize_value;
	table_class->value_is_empty   = etable_value_is_empty;
	table_class->value_to_string  = etable_value_to_string;
	table_class->thaw             = etable_thaw;
#endif

	table_class->row_sort_group   = etable_row_sort_group;
	table_class->has_sort_group   = etable_has_sort_group;

	tree_class->get_root          = etree_get_root;
	tree_class->get_prev          = etree_get_prev;
	tree_class->get_next          = etree_get_next;
	tree_class->get_first_child   = etree_get_first_child;
	tree_class->get_last_child    = etree_get_last_child;
	tree_class->get_parent        = etree_get_parent;

	tree_class->value_at          = etree_value_at;
	tree_class->icon_at           = etree_icon_at;
	tree_class->set_value_at      = etree_set_value_at;
	tree_class->is_editable       = etree_is_editable;

	tree_class->get_children      = etree_get_children;
	tree_class->is_expanded       = etree_is_expanded;
	tree_class->is_visible        = etree_is_visible;
	tree_class->set_expanded      = etree_set_expanded;
	tree_class->set_expanded_recurse = etree_set_expanded_recurse;
	tree_class->node_at_row       = etree_node_at_row;
}

static void
e_tree_init (GtkObject *object)
{
	ETreeModel *etree = (ETreeModel *)object;
	e_tree_model_construct (etree);
}

E_MAKE_TYPE(e_tree_model, "ETreeModel", ETreeModel, e_tree_model_class_init, e_tree_init, PARENT_TYPE)


/* signals */

/**
 * e_tree_model_node_changed:
 * @tree_model: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
void
e_tree_model_node_changed  (ETreeModel *tree_model, ETreePath *node)
{
	int row;
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	row = e_tree_model_row_of_node (tree_model, node);
	if (row != -1)
		e_table_model_row_changed (E_TABLE_MODEL (tree_model), row);

	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_CHANGED], node);
}

/**
 * e_tree_model_node_inserted:
 * @tree_model: 
 * @parent_node: 
 * @inserted_node: 
 * 
 * 
 **/
void
e_tree_model_node_inserted (ETreeModel *tree_model,
			    ETreePath *parent_node,
			    ETreePath *inserted_node)
{
	int row;
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	row = e_tree_model_row_of_node (tree_model, inserted_node);
	if (row != -1)
		e_table_model_row_inserted (E_TABLE_MODEL (tree_model), row);
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_INSERTED],
			 parent_node, inserted_node);
}

/**
 * e_tree_model_node_removed:
 * @tree_model: 
 * @parent_node: 
 * @removed_node: 
 * 
 * 
 **/
void
e_tree_model_node_removed  (ETreeModel *tree_model, ETreePath *parent_node, ETreePath *removed_node)
{
	int row;

	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	row = e_tree_model_row_of_node (tree_model, removed_node);
	if (row != -1)
		e_table_model_row_deleted (E_TABLE_MODEL (tree_model), row);
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_REMOVED],
			 parent_node, removed_node);
}

/**
 * e_tree_model_node_collapsed:
 * @tree_model: 
 * @node: 
 * 
 * 
 **/
void
e_tree_model_node_collapsed (ETreeModel *tree_model, ETreePath *node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_COLLAPSED],
			 node);
}

/**
 * e_tree_model_node_expanded:
 * @tree_model: 
 * @node: 
 * @allow_expand: 
 * 
 * 
 **/
void
e_tree_model_node_expanded  (ETreeModel *tree_model, ETreePath *node, gboolean *allow_expand)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_EXPANDED],
			 node, allow_expand);
}



/**
 * e_tree_model_construct:
 * @etree: 
 * 
 * 
 **/
void
e_tree_model_construct (ETreeModel *etree)
{
	ETreeModelPriv *priv;

	g_return_if_fail (etree != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (etree));

	priv = g_new0 (ETreeModelPriv, 1);
	etree->priv = priv;

	priv->node_chunk = g_mem_chunk_create (ETreePath, TREEPATH_CHUNK_AREA_SIZE, G_ALLOC_AND_FREE);
	priv->root = NULL;
	priv->root_visible = TRUE;
	priv->row_array = g_array_new (FALSE, FALSE, sizeof(ETreePath*));
	priv->expanded_state = g_hash_table_new (g_str_hash, g_str_equal);
	priv->sort_group = g_string_new("");
	priv->frozen = 0;
}

/**
 * e_tree_model_new
 *
 * XXX docs here.
 *
 * return values: a newly constructed ETreeModel.
 */
ETreeModel *
e_tree_model_new ()
{
	ETreeModel *et;

	et = gtk_type_new (e_tree_model_get_type ());

	return et;
}

/**
 * e_tree_model_get_root
 * @etree: the ETreeModel of which we want the root node.
 *
 * Accessor for the root node of @etree.
 *
 * return values: the ETreePath corresponding to the root node.
 */
ETreePath *
e_tree_model_get_root (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->get_root(etree);
}

/**
 * e_tree_model_node_at_row
 * @etree: the ETreeModel.
 * @row: 
 *
 * XXX docs here.
 *
 * return values: the ETreePath corresponding to @row.
 */
ETreePath *
e_tree_model_node_at_row (ETreeModel *etree, int row)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->node_at_row (etree, row);
}

/**
 * e_tree_model_icon_of_node
 * @etree: The ETreeModel.
 * @path: The ETreePath to the node we're getting the icon of.
 *
 * XXX docs here.
 *
 * return values: the GdkPixbuf associated with this node.
 */
GdkPixbuf *
e_tree_model_icon_of_node (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->icon_at (etree, path);
}

/**
 * e_tree_model_row_of_node
 * @etree: The ETreeModel.
 * @node: The node whose row we're looking up.
 *
 * return values: an int.
 */
int
e_tree_model_row_of_node (ETreeModel *etree, ETreePath *node)
{
	ETreeModelPriv *priv;
	int i;

	g_return_val_if_fail (etree != NULL, 0);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), 0);

	priv = etree->priv;
	for (i = 0; i < priv->row_array->len; i ++)
		if (g_array_index (priv->row_array, ETreePath*, i) == node)
			return i;

	return -1;
}

/**
 * e_tree_model_root_node_set_visible
 *
 * return values: none
 */
void
e_tree_model_root_node_set_visible (ETreeModel *etm, gboolean visible)
{
	ETreeModelPriv *priv;

	g_return_if_fail (etree != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (etree));

	priv = etm->priv;
	if (visible != priv->root_visible) {
		priv->root_visible = visible;
		if (priv->root) {
			if (visible) {
				priv->row_array = g_array_insert_val (priv->row_array, 0, priv->root);
			}
			else {
				ETreePath *root_path = e_tree_model_get_root (etm);
				e_tree_model_node_set_expanded (etm, root_path, TRUE);
				priv->row_array = g_array_remove_index (priv->row_array, 0);
			}
			
			e_table_model_changed (E_TABLE_MODEL (etm));
		}
	}
}

/**
 * e_tree_model_root_node_is_visible:
 * @etm: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_root_node_is_visible (ETreeModel *etm)
{
	ETreeModelPriv *priv;

	g_return_val_if_fail (etree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), FALSE);

	priv = etm->priv;
	return priv->root_visible;
}

/**
 * e_tree_model_node_get_first_child:
 * @etree: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_get_first_child (ETreeModel *etree, ETreePath *node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->get_first_child(etree, node);
}

/**
 * e_tree_model_node_get_last_child:
 * @etree: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_get_last_child (ETreeModel *etree, ETreePath *node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->get_last_child(etree, node);
}


/**
 * e_tree_model_node_get_next:
 * @etree: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_get_next (ETreeModel *etree, ETreePath *node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->get_next(etree, node);
}

/**
 * e_tree_model_node_get_prev:
 * @etree: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_get_prev (ETreeModel *etree, ETreePath *node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	return ETM_CLASS(etree)->get_prev(etree, node);
}

/**
 * e_tree_model_node_depth:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
guint
e_tree_model_node_depth (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail (etree != NULL, 0);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), 0);

	return e_tree_path_depth (path) - 1;
}

/**
 * e_tree_model_node_get_parent:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_get_parent (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail(etree != NULL, NULL);
	return ETM_CLASS(etree)->get_parent(etree, path);
}

/**
 * e_tree_model_node_is_root:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_node_is_root (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail(etree != NULL, FALSE);
	return (e_tree_model_node_depth (etree, path) == 0);
}

/**
 * e_tree_model_node_is_expandable:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_node_is_expandable (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail(etree != NULL, FALSE);
	return (e_tree_model_node_get_children (etree, path, NULL) > 0);
}

/**
 * e_tree_model_node_is_expanded:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_node_is_expanded (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail(etree != NULL, FALSE);
	return ETM_CLASS(etree)->is_expanded (etree, path);
}

/**
 * e_tree_model_node_is_visible:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_node_is_visible (ETreeModel *etree, ETreePath *path)
{
	g_return_val_if_fail(etree != NULL, FALSE);
	return ETM_CLASS(etree)->is_visible (etree, path);
}

/**
 * e_tree_model_node_set_expanded:
 * @etree: 
 * @path: 
 * @expanded: 
 * 
 * 
 **/
void
e_tree_model_node_set_expanded (ETreeModel *etree, ETreePath *path, gboolean expanded)
{
	g_return_if_fail(etree != NULL);
	ETM_CLASS(etree)->set_expanded (etree, path, expanded);
}

/**
 * e_tree_model_node_set_expanded_recurse:
 * @etree: 
 * @path: 
 * @expanded: 
 * 
 * 
 **/
void
e_tree_model_node_set_expanded_recurse (ETreeModel *etree, ETreePath *path, gboolean expanded)
{
	g_return_if_fail(etree != NULL);
	ETM_CLASS(etree)->set_expanded_recurse (etree, path, expanded);
}

guint
e_tree_model_node_get_children (ETreeModel *etree, ETreePath *path, ETreePath ***paths)
{
	g_return_val_if_fail(etree != NULL, 0);
	return ETM_CLASS(etree)->get_children (etree, path, paths);
}

/**
 * e_tree_model_node_num_visible_descendents:
 * @etm: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
guint
e_tree_model_node_num_visible_descendents (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail(node != NULL, 0)
	return node->visible_descendents;
}

/**
 * e_tree_model_node_get_data:
 * @etm: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
gpointer
e_tree_model_node_get_data (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return node->node_data;
}

/**
 * e_tree_model_node_set_data:
 * @etm: 
 * @node: 
 * @node_data: 
 * 
 * 
 **/
void
e_tree_model_node_set_data (ETreeModel *etm, ETreePath *node, gpointer node_data)
{
	g_return_if_fail (node);

	node->node_data = node_data;
}

/**
 * e_tree_model_node_insert:
 * @tree_model: 
 * @parent_path: 
 * @position: 
 * @node_data: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath*
e_tree_model_node_insert (ETreeModel *tree_model,
			  ETreePath *parent_path,
			  int position,
			  gpointer node_data)
{
	ETreeModelPriv *priv;
	ETreePath *new_path;

	g_return_val_if_fail(tree_model != NULL, NULL)

	priv = tree_model->priv;

	g_return_val_if_fail (parent_path != NULL || priv->root == NULL, NULL);

	priv = tree_model->priv;

	new_path = g_chunk_new0 (ETreePath, priv->node_chunk);

	new_path->expanded = FALSE;
	new_path->node_data = node_data;

	if (parent_path != NULL) {

		if (parent_path->first_child == NULL
		    && !parent_path->expanded_set) {
			e_tree_model_node_set_expanded (tree_model,
							parent_path,
							priv->expanded_default);
		}

		e_tree_path_insert (parent_path, position, new_path);

		if (e_tree_model_node_is_visible (tree_model, new_path)) {
			int parent_row, child_offset = 0;
			ETreePath *n;

			/* we need to iterate back up to the root, incrementing the number of visible
			   descendents */
			for (n = parent_path; n; n = n->parent) {
				n->visible_descendents ++;
			}
				
			/* determine if we are inserting at the end of this parent */
			if (position == -1 || position == parent_path->num_children) {
				position = e_tree_model_node_num_visible_descendents (tree_model, parent_path) - 1;
			} else {
				/* if we're not inserting at the end of the array, position is the child node we're
				   inserting at, not the absolute row position - need to count expanded nodes before it too */
				int i = position;

				n = e_tree_model_node_get_first_child(tree_model, parent_path);
				while (n != NULL && i > 0) {
					child_offset += n->visible_descendents;
					n = n->next_sibling;
					i--;
				}
			}


			/* if the parent is the root, no need to search for its position since it aint there */
			if (parent_path->parent == NULL) {
				parent_row = -1;
			} else {
				parent_row = e_tree_model_row_of_node (tree_model, parent_path);
			}

			e_table_model_pre_change(E_TABLE_MODEL(tree_model));

			priv->row_array = g_array_insert_val (priv->row_array,
							      parent_row + position + 1 + child_offset, new_path);

			/* only do this if we know a changed signal isn't coming later on */
			if (priv->frozen == 0)
				e_table_model_row_inserted (E_TABLE_MODEL(tree_model), parent_row + position + 1 + child_offset);
		}

		if (parent_path->compare)
			e_tree_model_node_sort (tree_model, parent_path);
	}
	else {
		priv->root = new_path;
		if (priv->root_visible) {
			priv->row_array = g_array_insert_val (priv->row_array, 0, priv->root);
			e_table_model_row_inserted (E_TABLE_MODEL (tree_model), 0);
		}
		else {
			/* need to mark the new node as expanded or
                           we'll never see it's children */
			new_path->expanded = TRUE;
			new_path->expanded_set = TRUE;
		}
	}

	return new_path;
}

/**
 * e_tree_model_node_insert_id:
 * @tree_model: 
 * @parent_path: 
 * @position: 
 * @node_data: 
 * @save_id: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath*
e_tree_model_node_insert_id (ETreeModel *tree_model,
			     ETreePath *parent_path,
			     int position,
			     gpointer node_data,
			     const char *save_id)
{
	ETreePath *path;

	g_return_val_if_fail(tree_model != NULL, NULL)

	path = e_tree_model_node_insert (tree_model, parent_path, position, node_data);

	e_tree_model_node_set_save_id (tree_model, path, save_id);

	return path;
}

/**
 * e_tree_model_node_insert_before:
 * @etree: 
 * @parent: 
 * @sibling: 
 * @node_data: 
 * 
 * 
 * 
 * Return value: 
 **/
ETreePath *
e_tree_model_node_insert_before (ETreeModel *etree,
				 ETreePath *parent,
				 ETreePath *sibling,
				 gpointer node_data)
{
	ETreePath *child;
	int position = 0;

	g_return_val_if_fail(tree_model != NULL, NULL)

	for (child = parent->first_child; child; child = child->next_sibling) {
		if (child == sibling)
			break;
		position ++;
	}
	return e_tree_model_node_insert (etree, parent, position, node_data);
}

/* just blows away child data, doesn't take into account unlinking/etc */
static void
child_free(ETreeModel *etree, ETreePath *node)
{
	ETreePath *child, *next;
	ETreeModelPriv *priv = etree->priv;

	child = node->first_child;
	while (child) {
		next = child->next_sibling;
		child_free(etree, child);
		child = next;
	}

	if (node->save_id) {
		g_hash_table_remove(priv->expanded_state, node->save_id);
		g_free(node->save_id);
	}
	g_chunk_free(node, priv->node_chunk);
}

/**
 * e_tree_model_node_remove:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gpointer
e_tree_model_node_remove (ETreeModel *etree, ETreePath *path)
{
	ETreeModelPriv *priv = etree->priv;
	ETreePath *parent = path->parent;
	gpointer ret = path->node_data;
	int row, visible = 0, base = 0;
	gboolean dochanged;

	g_return_val_if_fail(etree != NULL, NULL)

	/* work out what range of visible rows to remove */
	if (parent) {
		if (e_tree_model_node_is_visible(etree, path)) {
			base = e_tree_model_row_of_node(etree, path);
			visible = path->visible_descendents + 1;
		}
	} else if (path == priv->root) {
		priv->root = NULL;
		if (priv->root_visible) {
			base = 0;
			visible = path->visible_descendents + 1;
		} else {
			base = 0;
			visible = path->visible_descendents;
		}
	} else {
		g_warning("Trying to remove invalid path %p", path);
		return NULL;
	}

	/* unlink this node - we only have to unlink the root node being removed,
	   since the others are only references from this node */
	e_tree_path_unlink (path);

	/*printf("removing %d nodes from position %d\n", visible, base);*/

	if (visible > 0) {
		/* fix up the parent visible counts */
		for (; parent; parent = parent->parent) {
			parent->visible_descendents -= visible;
		}

		/* if we have a lot of nodes to remove, then we dont row_deleted each one */
		/* could probably be tuned, but this'll do, since its normally only when we
		   remove the whole lot do we really care */
		dochanged = (visible > 1000) || (visible > (priv->row_array->len / 4));

		e_table_model_pre_change(E_TABLE_MODEL (etree));

		/* and physically remove them */
		if (visible == priv->row_array->len) {
			g_array_set_size(priv->row_array, 0);
		} else {
			memmove(&g_array_index(priv->row_array, ETreePath *, base),
				&g_array_index(priv->row_array, ETreePath *, base+visible),
				(priv->row_array->len - (base+visible)) * sizeof(ETreePath *));
			g_array_set_size(priv->row_array, priv->row_array->len - visible);
		}

		/* tell the system we've removed (these) nodes */
		if (priv->frozen == 0) {
			if (dochanged) {
				e_table_model_changed(E_TABLE_MODEL(etree));
			} else {
				for (row=visible-1;row>=0;row--) {
					e_table_model_row_deleted(E_TABLE_MODEL(etree), row+base);
				}
			}
		}
	}

	child_free(etree, path);

	return ret;
}

static void
add_visible_descendents_to_array (ETreeModel *etm, ETreePath *node, int *row, int *count)
{
	ETreeModelPriv *priv = etm->priv;

	/* add a row for this node */
	e_table_model_pre_change(E_TABLE_MODEL (etm));
	priv->row_array = g_array_insert_val (priv->row_array, (*row), node);
	if (priv->frozen == 0)
		e_table_model_row_inserted (E_TABLE_MODEL (etm), (*row));
	(*row) ++;
	(*count) ++;

	/* then loop over its children, calling this routine for each
           of them */
	if (node->expanded) {
		ETreePath *child;
		for (child = node->first_child; child;
		     child = child->next_sibling) {
			add_visible_descendents_to_array (etm, child, row, count);
		}
	}
}

static void
save_expanded_state_func (char *key, gboolean expanded, gpointer user_data)
{
	if (expanded) {
		xmlNode *root = (xmlNode*)user_data;
		xmlNode *node_root = xmlNewNode (NULL, (xmlChar*) "node");

		xmlAddChild (root, node_root);
		xmlNewChild (node_root, NULL, (xmlChar *) "id", (xmlChar*) key);
	}
}

/**
 * e_tree_model_save_expanded_state:
 * @etm: 
 * @filename: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_save_expanded_state (ETreeModel *etm, const char *filename)
{
	xmlDoc *doc;
	xmlNode *root;
	int fd, rv;
	xmlChar *buf;
	int buf_size;
	ETreeModelPriv *priv = etm->priv;

	g_return_val_if_fail(etm != NULL, FALSE);

	doc = xmlNewDoc ((xmlChar*) "1.0");
	root = xmlNewDocNode (doc, NULL,
			      (xmlChar *) "expanded_state",
			      NULL);
	xmlDocSetRootElement (doc, root);

	g_hash_table_foreach (priv->expanded_state,
			      (GHFunc)save_expanded_state_func,
			      root);

	fd = open (filename, O_CREAT | O_TRUNC | O_WRONLY, 0777);

	xmlDocDumpMemory (doc, &buf, &buf_size);

	if (buf == NULL) {
		g_error ("Failed to write %s: xmlBufferCreate() == NULL", filename);
		return FALSE;
	}

	rv = write (fd, buf, buf_size);
	xmlFree (buf);
	close (fd);

	if (0 > rv) {
		g_error ("Failed to write new %s: %d\n", filename, errno);
		unlink (filename);
		return FALSE;
	}

	return TRUE;
}

static char *
get_string_value (xmlNode *node,
		  const char *name)
{
	xmlNode *p;
	xmlChar *xml_string;
	char *retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return NULL;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the empty string */
		return g_strdup("");

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = g_strdup ((char *) xml_string);
	xmlFree (xml_string);

	return retval;
}

/**
 * e_tree_model_load_expanded_state:
 * @etm: 
 * @filename: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_load_expanded_state (ETreeModel *etm, const char *filename)
{
	ETreeModelPriv *priv = etm->priv;
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *child;

	g_return_val_if_fail(etm != NULL, FALSE);

	doc = xmlParseFile (filename);
	if (!doc)
		return FALSE;

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "expanded_state")) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (child = root->childs; child; child = child->next) {
		char *id;

		if (strcmp (child->name, "node")) {
			g_warning ("unknown node '%s' in %s", child->name, filename);
			continue;
		}

		id = get_string_value (child, "id");

		g_hash_table_insert (priv->expanded_state, id, (gpointer)TRUE);
	}

	xmlFreeDoc (doc);

	return TRUE;
}


/**
 * e_tree_model_node_set_save_id:
 * @etm: 
 * @node: 
 * @id: 
 * 
 * 
 **/
void
e_tree_model_node_set_save_id (ETreeModel *etm, ETreePath *node, const char *id)
{
	char *key;
	gboolean expanded_state;
	ETreeModelPriv *priv;

	g_return_if_fail(etm != NULL);
	g_return_if_fail (E_TREE_MODEL (etm));
	g_return_if_fail (node);

	priv = etm->priv;

	if (g_hash_table_lookup_extended (priv->expanded_state,
					  id, (gpointer*)&key, (gpointer*)&expanded_state)) {

		e_tree_model_node_set_expanded (etm, node,
						expanded_state);

		/* important that this comes after the e_tree_model_node_set_expanded */
		node->save_id = key;
	}
	else {
		node->save_id = g_strdup (id);

		g_hash_table_insert (priv->expanded_state, node->save_id, (gpointer)node->expanded);
	}
}



/**
 * e_tree_model_node_set_compare_function:
 * @tree_model: 
 * @node: 
 * @compare: 
 * 
 * 
 **/
void
e_tree_model_node_set_compare_function (ETreeModel *tree_model,
					ETreePath *node,
					ETreePathCompareFunc compare)
{
	gboolean need_sort;

	g_return_if_fail(etm != NULL);
	g_return_if_fail (E_TREE_MODEL (tree_model));
	g_return_if_fail (node);

	need_sort = (compare != node->compare);

	node->compare = compare;

	if (need_sort)
		e_tree_model_node_sort (tree_model, node);
}

typedef struct {
	ETreeModel *model;
	ETreePath *path;
	ETreePathCompareFunc compare;
	gboolean was_expanded;
} ETreeSortInfo;

static gint
e_tree_model_node_compare (ETreeSortInfo *info1, ETreeSortInfo *info2)
{
	return info1->compare (info1->model, info1->path, info2->path);
}

/**
 * e_tree_model_node_sort:
 * @tree_model: 
 * @node: 
 * 
 * 
 **/
void
e_tree_model_node_sort (ETreeModel *tree_model,
			ETreePath *node)
{
	int num_nodes = node->num_children;
	ETreeSortInfo *sort_info;
	int i;
	int child_index;
	gboolean node_expanded;
	ETreeModelPriv *priv = tree_model->priv;;

	node_expanded = e_tree_model_node_is_expanded (tree_model, node);

	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_TREE_MODEL (tree_model));
	g_return_if_fail (node);

	g_return_if_fail (node->compare);

	if (num_nodes == 0)
		return;

	e_table_model_pre_change(E_TABLE_MODEL (tree_model));

	sort_info = g_new (ETreeSortInfo, num_nodes);

	child_index = e_tree_model_row_of_node (tree_model, node) + 1;

	/* collect our info and remove the children */
	for (i = 0; i < num_nodes; i ++) {
		sort_info[i].path = node->first_child;
		sort_info[i].was_expanded = e_tree_model_node_is_expanded (tree_model, sort_info[i].path);
		sort_info[i].model = tree_model;
		sort_info[i].compare = node->compare;

		e_tree_model_node_set_expanded(tree_model, sort_info[i].path, FALSE);
		if (node_expanded)
			priv->row_array = g_array_remove_index (priv->row_array, child_index);
		e_tree_path_unlink (sort_info[i].path);
	}

	/* sort things */
	qsort (sort_info, num_nodes, sizeof(ETreeSortInfo), (GCompareFunc)e_tree_model_node_compare);
	
	/* reinsert the children nodes into the tree in the sorted order */
	for (i = 0; i < num_nodes; i ++) {
		e_tree_path_insert (node, i, sort_info[i].path);
		if (node_expanded)
			priv->row_array = g_array_insert_val (priv->row_array, child_index + i,
							      sort_info[i].path);
	}

	/* make another pass expanding the children as needed.  

	   XXX this used to be in the loop above, but a recent change
	   (either here or in the etable code) causes an assertion and
	   a crash */
	for (i = 0; i < num_nodes; i ++) {
		e_tree_model_node_set_expanded (tree_model, sort_info[i].path, sort_info[i].was_expanded);
	}

	g_free (sort_info);

	if (priv->frozen == 0)
		e_table_model_changed (E_TABLE_MODEL (tree_model));
}


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

typedef struct {
	gboolean expanded;
	guint visible_descendents;
	gpointer node_data;
} ENode;

enum {
	NODE_CHANGED,
	NODE_INSERTED,
	NODE_REMOVED,
	LAST_SIGNAL
};

static guint e_tree_model_signals [LAST_SIGNAL] = {0, };

static void add_visible_descendents_to_array (ETreeModel *etm, GNode *gnode, int *row, int *count);


/* virtual methods */

static void
etree_destroy (GtkObject *object)
{
	/* XXX lots of stuff to free here */
}

static ETreePath*
etree_get_root (ETreeModel *etm)
{
	return etm->root;
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

	return g_node_next_sibling(node);
}

static ETreePath*
etree_get_prev (ETreeModel *etm, ETreePath *node)
{
	g_return_val_if_fail (node, NULL);

	return g_node_prev_sibling (node);
}

static guint
etree_get_children (ETreeModel *etm, ETreePath* node, ETreePath ***paths)
{
	guint n_children;

	g_return_val_if_fail (node, 0);

	n_children = g_node_n_children (node);

	if (paths) {
		int i;
		(*paths) = g_malloc (sizeof (ETreePath*) * n_children);
		for (i = 0; i < n_children; i ++) {
			(*paths)[i] = g_node_nth_child (node, i);
		}
	}

	return n_children;
}

static gboolean
etree_is_expanded (ETreeModel *etm, ETreePath* node)
{
	g_return_val_if_fail (node && node->data, FALSE);
	
	return ((ENode*)node->data)->expanded;
}

static gboolean
etree_is_visible (ETreeModel *etm, ETreePath* node)
{
	g_return_val_if_fail (node, FALSE);

	for (node = node->parent; node; node = node->parent) {
		if (!((ENode*)node->data)->expanded)
			return FALSE;
	}

	return TRUE;
}

static void
etree_set_expanded (ETreeModel *etm, ETreePath* node, gboolean expanded)
{
	GNode *child;
	ENode *enode;
	int row;
	
	g_return_if_fail (node && node->data);

	enode = ((ENode*)node->data);

	if (enode->expanded == expanded)
		return;

	enode->expanded = expanded;

	row = e_tree_model_row_of_node (etm, node) + 1;

	if (expanded) {
		GNode *parent;

		if (e_tree_model_node_is_visible (etm, node)) {
			enode->visible_descendents = 0;
			for (child = g_node_first_child (node); child;
			     child = g_node_next_sibling (child)) {
				add_visible_descendents_to_array (etm, child, &row, &enode->visible_descendents);
			}
		}
		/* now iterate back up the tree, adding to our
		   ancestors' visible descendents */

		for (parent = node->parent; parent; parent = parent->parent) {
			ENode *parent_enode = (ENode*)parent->data;
			parent_enode->visible_descendents += enode->visible_descendents;
		}
	}
	else {
		int i;
		GNode *parent;

		if (e_tree_model_node_is_visible (etm, node)) {
			for (i = 0; i < enode->visible_descendents; i ++) {
				etm->row_array = g_array_remove_index (etm->row_array, row);
				e_table_model_row_deleted (E_TABLE_MODEL (etm), row);
			}
		}
		/* now iterate back up the tree, subtracting from our
		   ancestors' visible descendents */

		for (parent = node->parent; parent; parent = parent->parent) {
			ENode *parent_enode = (ENode*)parent->data;

			parent_enode->visible_descendents -= enode->visible_descendents;
		}

		enode->visible_descendents = 0;
	}
}

/* fairly naive implementation */
static void
etree_set_expanded_recurse (ETreeModel *etm, ETreePath* node, gboolean expanded)
{
	ETreePath **paths;
	guint num_children;
	int i;

	e_tree_model_node_set_expanded (etm, node, expanded);

	num_children = e_tree_model_node_get_children (etm, node, &paths);
	if (num_children) {
		for (i = 0; i < num_children; i ++) {
			e_tree_model_node_set_expanded_recurse (etm, paths[i], expanded);
		}

		g_free (paths);
	}
}

static ETreePath *
etree_node_at_row (ETreeModel *etree, int row)
{
	GNode *node = g_array_index (etree->row_array, GNode*, row);

	return node;
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
	return tree->row_array->len;
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
	table_class->thaw             = etable_thaw;
#endif

	tree_class->get_root          = etree_get_root;
	tree_class->get_prev          = etree_get_prev;
	tree_class->get_next          = etree_get_next;
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

E_MAKE_TYPE(e_tree_model, "ETreeModel", ETreeModel, e_tree_model_class_init, NULL, PARENT_TYPE)


/* signals */
void
e_tree_model_node_changed  (ETreeModel *tree_model, ETreePath *node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_CHANGED]);
}

void
e_tree_model_node_inserted (ETreeModel *tree_model,
			    ETreePath *parent_node,
			    ETreePath *inserted_node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_INSERTED],
			 parent_node, inserted_node);
}

void
e_tree_model_node_removed  (ETreeModel *tree_model, ETreePath *parent_node, ETreePath *removed_node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_REMOVED],
			 parent_node, removed_node);
}


void
e_tree_model_construct (ETreeModel *etree)
{
	etree->root = NULL;
	etree->root_visible = TRUE;
	etree->row_array = g_array_new (FALSE, FALSE, sizeof(GNode*));
}

ETreeModel *
e_tree_model_new ()
{
	ETreeModel *et;

	et = gtk_type_new (e_tree_model_get_type ());

	return et;
}

ETreePath *
e_tree_model_get_root (ETreeModel *etree)
{
	return ETM_CLASS(etree)->get_root(etree);
}

ETreePath *
e_tree_model_node_at_row (ETreeModel *etree, int row)
{
	return ETM_CLASS(etree)->node_at_row (etree, row);
}

GdkPixbuf *
e_tree_model_icon_of_node (ETreeModel *etree, ETreePath *path)
{
	return ETM_CLASS(etree)->icon_at (etree, path);
}

int
e_tree_model_row_of_node (ETreeModel *etree, ETreePath *node)
{
	int i;

	for (i = 0; i < etree->row_array->len; i ++)
		if (g_array_index (etree->row_array, GNode*, i) == node)
			return i;

	return -1;
}

void
e_tree_model_root_node_set_visible (ETreeModel *etm, gboolean visible)
{
	if (visible != etm->root_visible) {
		etm->root_visible = visible;
		if (etm->root) {
			if (visible) {
				etm->row_array = g_array_insert_val (etm->row_array, 0, etm->root);
			}
			else {
				ETreePath *root_path = e_tree_model_get_root (etm);
				etm->row_array = g_array_remove_index (etm->row_array, 0);
				e_tree_model_node_set_expanded (etm, root_path, TRUE);
			}
			
			e_table_model_changed (E_TABLE_MODEL (etm));
		}
	}
}

gboolean
e_tree_model_root_node_is_visible (ETreeModel *etm)
{
	return etm->root_visible;
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
	return g_node_depth (path) - 1;
}

ETreePath *
e_tree_model_node_get_parent (ETreeModel *etree, ETreePath *path)
{
	return ETM_CLASS(etree)->get_parent(etree, path);
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

gboolean
e_tree_model_node_is_visible (ETreeModel *etree, ETreePath *path)
{
	return ETM_CLASS(etree)->is_visible (etree, path);
}

void
e_tree_model_node_set_expanded (ETreeModel *etree, ETreePath *path, gboolean expanded)
{
	ETM_CLASS(etree)->set_expanded (etree, path, expanded);
}

void
e_tree_model_node_set_expanded_recurse (ETreeModel *etree, ETreePath *path, gboolean expanded)
{
	ETM_CLASS(etree)->set_expanded_recurse (etree, path, expanded);
}

guint
e_tree_model_node_get_children (ETreeModel *etree, ETreePath *path, ETreePath ***paths)
{
	return ETM_CLASS(etree)->get_children (etree, path, paths);
}

guint
e_tree_model_node_num_visible_descendents (ETreeModel *etm, ETreePath *node)
{
	ENode *enode = (ENode*)node->data;

	return enode->visible_descendents;
}

gpointer
e_tree_model_node_get_data (ETreeModel *etm, ETreePath *node)
{
	ENode *enode;

	g_return_val_if_fail (node && node->data, NULL);

	enode = (ENode*)node->data;

	return enode->node_data;
}

void
e_tree_model_node_set_data (ETreeModel *etm, ETreePath *node, gpointer node_data)
{
	ENode *enode;

	g_return_if_fail (node && node->data);

	enode = (ENode*)node->data;

	enode->node_data = node_data;
}

ETreePath*
e_tree_model_node_insert (ETreeModel *tree_model,
			  ETreePath *parent_path,
			  int position,
			  gpointer node_data)
{
	ENode *node;
	ETreePath *new_path;

	g_return_val_if_fail (parent_path != NULL || tree_model->root == NULL, NULL);

	node = g_new0 (ENode, 1);

	node->expanded = FALSE;
	node->node_data = node_data;

	if (parent_path != NULL) {

		new_path = g_node_new (node);

		g_node_insert (parent_path, position, new_path);

		if (e_tree_model_node_is_visible (tree_model, new_path)) {
			int parent_row;
			GNode *node;

			/* we need to iterate back up to the root, incrementing the number of visible
			   descendents */
			for (node = parent_path; node; node = node->parent) {
				ENode *parent_enode = (ENode*)node->data;

				parent_enode->visible_descendents ++;
			}
				
			/* finally, insert a row into the table */
			parent_row = e_tree_model_row_of_node (tree_model, parent_path);

			tree_model->row_array = g_array_insert_val (tree_model->row_array,
								    parent_row + position + 1, new_path);

			e_table_model_row_inserted (E_TABLE_MODEL(tree_model), parent_row + position + 1);
		}
	}
	else {
		tree_model->root = g_node_new (node);
		if (tree_model->root_visible)
			tree_model->row_array = g_array_insert_val (tree_model->row_array, 0, tree_model->root);
		new_path = tree_model->root;

		e_table_model_row_inserted (E_TABLE_MODEL (tree_model), 0);
	}

	return new_path;
}
			  
ETreePath *
e_tree_model_node_insert_before (ETreeModel *etree,
				 ETreePath *parent,
				 ETreePath *sibling,
				 gpointer node_data)
{
	return e_tree_model_node_insert (etree, parent,
					 g_node_child_position (parent, sibling),
					 node_data);
}

gpointer
e_tree_model_node_remove (ETreeModel *etree, ETreePath *path)
{
	GNode *parent = path->parent;
	ENode *enode = (ENode*)path->data;
	gpointer ret = enode->node_data;

	g_return_val_if_fail (!g_node_first_child(path), NULL);

	/* clean up the display */
	if (parent) {
		if (e_tree_model_node_is_visible (etree, path)) {
			int row = e_tree_model_row_of_node (etree, path);
			etree->row_array = g_array_remove_index (etree->row_array, row);
			e_table_model_row_deleted (E_TABLE_MODEL (etree), row);

			/* we need to iterate back up to the root, incrementing the number of visible
			   descendents */
			for (; parent; parent = parent->parent) {
				ENode *parent_enode = (ENode*)parent->data;

				parent_enode->visible_descendents --;
			}
		}
	}
	else if (path == etree->root) {
		etree->root = NULL;
		if (etree->root_visible) {
			etree->row_array = g_array_remove_index (etree->row_array, 0);
			e_table_model_row_deleted (E_TABLE_MODEL (etree), 0);
		}
	}
	else {
		/* XXX invalid path */
		return NULL;
	}


	/* now free up the storage from that path */
	g_node_destroy (path);
	g_free (enode);

	return ret;
}


static void
add_visible_descendents_to_array (ETreeModel *etm, GNode *gnode, int *row, int *count)
{
	GNode *child;
	ENode *enode;

	/* add a row for this node */
	etm->row_array = g_array_insert_val (etm->row_array, (*row), gnode);
	e_table_model_row_inserted (E_TABLE_MODEL (etm), (*row)++);
	(*count) ++;

	/* then loop over its children, calling this routine for each
           of them */
	enode = (ENode*)gnode->data;
	if (enode->expanded) {
		for (child = g_node_first_child (gnode); child;
		     child = g_node_next_sibling (child)) {
			add_visible_descendents_to_array (etm, child, row, count);
		}
	}
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-model.c: a Tree Model
 *
 * Author:
 *   Chris Toshok (toshok@ximian.com)
 *   Chris Lahey  <clahey@ximian.com>
 *
 * Adapted from the gtree code and ETableModel.
 *
 * (C) 2000, 2001 Ximian, Inc.
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

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *parent_class;

enum {
	PRE_CHANGE,
	NODE_CHANGED,
	NODE_DATA_CHANGED,
	NODE_COL_CHANGED,
	NODE_INSERTED,
	NODE_REMOVED,
	LAST_SIGNAL
};

static guint e_tree_model_signals [LAST_SIGNAL] = {0, };


static void
e_tree_model_class_init (GtkObjectClass *klass)
{
	ETreeModelClass *tree_class = (ETreeModelClass *) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	e_tree_model_signals [PRE_CHANGE] =
		gtk_signal_new ("pre_change",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, pre_change),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_tree_model_signals [NODE_CHANGED] =
		gtk_signal_new ("node_changed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_DATA_CHANGED] =
		gtk_signal_new ("node_data_changed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_data_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	e_tree_model_signals [NODE_COL_CHANGED] =
		gtk_signal_new ("node_col_changed",
				GTK_RUN_LAST,
				klass->type,
				GTK_SIGNAL_OFFSET (ETreeModelClass, node_col_changed),
				gtk_marshal_NONE__POINTER_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_INT);

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
				e_marshal_NONE__POINTER_POINTER_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_POINTER, GTK_TYPE_INT);

	gtk_object_class_add_signals (klass, e_tree_model_signals, LAST_SIGNAL);

	tree_class->get_root             = NULL;

	tree_class->get_parent           = NULL;
	tree_class->get_first_child      = NULL;
	tree_class->get_last_child       = NULL;
	tree_class->get_next             = NULL;
	tree_class->get_prev             = NULL;

	tree_class->is_root              = NULL;
	tree_class->is_expandable        = NULL;
	tree_class->get_children         = NULL;
	tree_class->depth                = NULL;

	tree_class->icon_at              = NULL;

	tree_class->get_expanded_default = NULL;
	tree_class->column_count         = NULL;

	tree_class->has_save_id          = NULL;
	tree_class->get_save_id          = NULL;

	tree_class->value_at             = NULL;
	tree_class->set_value_at         = NULL;
	tree_class->is_editable          = NULL;

	tree_class->duplicate_value      = NULL;
	tree_class->free_value           = NULL;
	tree_class->initialize_value     = NULL;
	tree_class->value_is_empty       = NULL;
	tree_class->value_to_string      = NULL;

	tree_class->pre_change           = NULL;
	tree_class->node_changed         = NULL;
	tree_class->node_data_changed    = NULL;
	tree_class->node_col_changed     = NULL;
	tree_class->node_inserted        = NULL;
	tree_class->node_removed         = NULL;
}

static void
e_tree_init (GtkObject *object)
{
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
e_tree_model_pre_change  (ETreeModel *tree_model)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [PRE_CHANGE]);
}

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
e_tree_model_node_changed  (ETreeModel *tree_model, ETreePath node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_CHANGED], node);
}

/**
 * e_tree_model_node_data_changed:
 * @tree_model: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
void
e_tree_model_node_data_changed  (ETreeModel *tree_model, ETreePath node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_DATA_CHANGED], node);
}

/**
 * e_tree_model_node_col_changed:
 * @tree_model: 
 * @node: 
 * 
 * 
 * 
 * Return value: 
 **/
void
e_tree_model_node_col_changed  (ETreeModel *tree_model, ETreePath node, int col)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_COL_CHANGED], node, col);
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
			    ETreePath parent_node,
			    ETreePath inserted_node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

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
e_tree_model_node_removed  (ETreeModel *tree_model, ETreePath parent_node, ETreePath removed_node, int old_position)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	gtk_signal_emit (GTK_OBJECT (tree_model),
			 e_tree_model_signals [NODE_REMOVED],
			 parent_node, removed_node, old_position);
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
ETreePath 
e_tree_model_get_root (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_root)
		return ETM_CLASS(etree)->get_root(etree);
	else
		return NULL;
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
ETreePath 
e_tree_model_node_get_parent (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail(etree != NULL, NULL);
	if (ETM_CLASS(etree)->get_parent)
		return ETM_CLASS(etree)->get_parent(etree, node);
	else
		return NULL;
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
ETreePath 
e_tree_model_node_get_first_child (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_first_child)
		return ETM_CLASS(etree)->get_first_child(etree, node);
	else
		return NULL;
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
ETreePath 
e_tree_model_node_get_last_child (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_last_child)
		return ETM_CLASS(etree)->get_last_child(etree, node);
	else
		return NULL;
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
ETreePath 
e_tree_model_node_get_next (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_next)
		return ETM_CLASS(etree)->get_next(etree, node);
	else
		return NULL;
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
ETreePath 
e_tree_model_node_get_prev (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_prev)
		return ETM_CLASS(etree)->get_prev(etree, node);
	else
		return NULL;
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
e_tree_model_node_is_root (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail(etree != NULL, FALSE);

	if (ETM_CLASS(etree)->is_root)
		return ETM_CLASS(etree)->is_root(etree, node);
	else
		return FALSE;
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
e_tree_model_node_is_expandable (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail(etree != NULL, FALSE);

	if (ETM_CLASS(etree)->is_expandable)
		return ETM_CLASS(etree)->is_expandable(etree, node);
	else
		return FALSE;
}

guint
e_tree_model_node_get_children (ETreeModel *etree, ETreePath node, ETreePath **nodes)
{
	g_return_val_if_fail(etree != NULL, 0);
	if (ETM_CLASS(etree)->get_children)
		return ETM_CLASS(etree)->get_children (etree, node, nodes);
	else
		return 0;
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
e_tree_model_node_depth (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, 0);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), 0);

	if (ETM_CLASS(etree)->depth)
		return ETM_CLASS(etree)->depth(etree, node);
	else
		return 0;
}

/**
 * e_tree_model_icon_at
 * @etree: The ETreeModel.
 * @path: The ETreePath to the node we're getting the icon of.
 *
 * XXX docs here.
 *
 * return values: the GdkPixbuf associated with this node.
 */
GdkPixbuf *
e_tree_model_icon_at (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->icon_at)
		return ETM_CLASS(etree)->icon_at (etree, node);
	else
		return NULL;
}

/**
 * e_tree_model_get_expanded_default
 * @etree: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether nodes should be expanded by default.
 */
gboolean
e_tree_model_get_expanded_default (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), FALSE);

	if (ETM_CLASS(etree)->get_expanded_default)
		return ETM_CLASS(etree)->get_expanded_default (etree);
	else
		return FALSE;
}

/**
 * e_tree_model_column_count
 * @etree: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: The number of columns
 */
gint
e_tree_model_column_count (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, 0);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), 0);

	if (ETM_CLASS(etree)->column_count)
		return ETM_CLASS(etree)->column_count (etree);
	else
		return 0;
}

/**
 * e_tree_model_has_save_id
 * @etree: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree has valid save id data.
 */
gboolean
e_tree_model_has_save_id (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), FALSE);

	if (ETM_CLASS(etree)->has_save_id)
		return ETM_CLASS(etree)->has_save_id (etree);
	else
		return FALSE;
}

/**
 * e_tree_model_get_save_id
 * @etree: The ETreeModel.
 * @node: The ETreePath.
 *
 * XXX docs here.
 *
 * return values: The save id for this path.
 */
gchar *
e_tree_model_get_save_id (ETreeModel *etree, ETreePath node)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_save_id)
		return ETM_CLASS(etree)->get_save_id (etree, node);
	else
		return NULL;
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
void *
e_tree_model_value_at (ETreeModel *etree, ETreePath node, int col)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->value_at)
		return ETM_CLASS(etree)->value_at (etree, node, col);
	else
		return NULL;
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
void
e_tree_model_set_value_at (ETreeModel *etree, ETreePath node, int col, const void *val)
{
	g_return_if_fail (etree != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (etree));

	if (ETM_CLASS(etree)->set_value_at)
		ETM_CLASS(etree)->set_value_at (etree, node, col, val);
}

/**
 * e_tree_model_node_is_editable:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_node_is_editable (ETreeModel *etree, ETreePath node, int col)
{
	g_return_val_if_fail(etree != NULL, FALSE);

	if (ETM_CLASS(etree)->is_editable)
		return ETM_CLASS(etree)->is_editable(etree, node, col);
	else
		return FALSE;
}

/**
 * e_tree_model_duplicate_value:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
void *
e_tree_model_duplicate_value (ETreeModel *etree, int col, const void *value)
{
	g_return_val_if_fail(etree != NULL, NULL);

	if (ETM_CLASS(etree)->duplicate_value)
		return ETM_CLASS(etree)->duplicate_value(etree, col, value);
	else
		return NULL;
}

/**
 * e_tree_model_free_value:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
void
e_tree_model_free_value (ETreeModel *etree, int col, void *value)
{
	g_return_if_fail(etree != NULL);

	if (ETM_CLASS(etree)->free_value)
		ETM_CLASS(etree)->free_value(etree, col, value);
}

/**
 * e_tree_model_initialize_value:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
void *
e_tree_model_initialize_value (ETreeModel *etree, int col)
{
	g_return_val_if_fail(etree != NULL, NULL);

	if (ETM_CLASS(etree)->initialize_value)
		return ETM_CLASS(etree)->initialize_value(etree, col);
	else
		return NULL;
}

/**
 * e_tree_model_value_is_empty:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
e_tree_model_value_is_empty (ETreeModel *etree, int col, const void *value)
{
	g_return_val_if_fail(etree != NULL, TRUE);

	if (ETM_CLASS(etree)->value_is_empty)
		return ETM_CLASS(etree)->value_is_empty(etree, col, value);
	else
		return TRUE;
}

/**
 * e_tree_model_value_to_string:
 * @etree: 
 * @path: 
 * 
 * 
 * 
 * Return value: 
 **/
char *
e_tree_model_value_to_string (ETreeModel *etree, int col, const void *value)
{
	g_return_val_if_fail(etree != NULL, g_strdup(""));

	if (ETM_CLASS(etree)->value_to_string)
		return ETM_CLASS(etree)->value_to_string(etree, col, value);
	else
		return g_strdup("");
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
e_tree_model_node_traverse (ETreeModel *model, ETreePath path, ETreePathFunc func, gpointer data)
{
	ETreePath child;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (model));
	g_return_if_fail (path != NULL);

	child = e_tree_model_node_get_first_child (model, path);

	while (child) {
		ETreePath next_child;

		next_child = e_tree_model_node_get_next (model, child);
		e_tree_model_node_traverse (model, child, func, data);
		if (func (model, child, data) == TRUE)
			return;

		child = next_child;
	}
}

/**
 * e_tree_model_node_traverse_preorder:
 * @model: 
 * @path: 
 * @func: 
 * @data: 
 * 
 * 
 **/
void
e_tree_model_node_traverse_preorder (ETreeModel *model, ETreePath path, ETreePathFunc func, gpointer data)
{
	ETreePath child;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (model));
	g_return_if_fail (path != NULL);

	child = e_tree_model_node_get_first_child (model, path);

	while (child) {
		ETreePath next_child;

		if (func (model, child, data) == TRUE)
			return;

		next_child = e_tree_model_node_get_next (model, child);
		e_tree_model_node_traverse_preorder (model, child, func, data);

		child = next_child;
	}
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-model.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey  <clahey@ximian.com>
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-tree-model.h"

#define ETM_CLASS(e) (E_TREE_MODEL_GET_CLASS(e))

#define d(x)

static GObjectClass *parent_class;

enum {
	PRE_CHANGE,
	NO_CHANGE,
	NODE_CHANGED,
	NODE_DATA_CHANGED,
	NODE_COL_CHANGED,
	NODE_INSERTED,
	NODE_REMOVED,
	NODE_DELETED,
	NODE_REQUEST_COLLAPSE,
	LAST_SIGNAL
};

static guint e_tree_model_signals [LAST_SIGNAL] = {0, };


static void
e_tree_model_class_init (GObjectClass *klass)
{
	ETreeModelClass *tree_class = (ETreeModelClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	e_tree_model_signals [PRE_CHANGE] =
		g_signal_new ("pre_change",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, pre_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_tree_model_signals [NO_CHANGE] =
		g_signal_new ("no_change",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, no_change),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_tree_model_signals [NODE_CHANGED] =
		g_signal_new ("node_changed",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_tree_model_signals [NODE_DATA_CHANGED] =
		g_signal_new ("node_data_changed",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_data_changed),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_tree_model_signals [NODE_COL_CHANGED] =
		g_signal_new ("node_col_changed",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_col_changed),
			      (GSignalAccumulator) NULL, NULL,
			      e_util_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);

	e_tree_model_signals [NODE_INSERTED] =
		g_signal_new ("node_inserted",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_inserted),
			      (GSignalAccumulator) NULL, NULL,
			      e_util_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	e_tree_model_signals [NODE_REMOVED] =
		g_signal_new ("node_removed",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_removed),
			      (GSignalAccumulator) NULL, NULL,
			      e_util_marshal_VOID__POINTER_POINTER_INT,
			      G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_INT);

	e_tree_model_signals [NODE_DELETED] =
		g_signal_new ("node_deleted",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_deleted),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	e_tree_model_signals [NODE_REQUEST_COLLAPSE] =
		g_signal_new ("node_request_collapse",
			      E_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ETreeModelClass, node_request_collapse),
			      (GSignalAccumulator) NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	tree_class->get_root              = NULL;

	tree_class->get_parent            = NULL;
	tree_class->get_first_child       = NULL;
	tree_class->get_last_child        = NULL;
	tree_class->get_next              = NULL;
	tree_class->get_prev              = NULL;

	tree_class->is_root               = NULL;
	tree_class->is_expandable         = NULL;
	tree_class->get_children          = NULL;
	tree_class->depth                 = NULL;

	tree_class->icon_at               = NULL;

	tree_class->get_expanded_default  = NULL;
	tree_class->column_count          = NULL;

	tree_class->has_save_id           = NULL;
	tree_class->get_save_id           = NULL;
	tree_class->has_get_node_by_id    = NULL;
	tree_class->get_node_by_id        = NULL;

	tree_class->has_change_pending    = NULL;

	tree_class->value_at              = NULL;
	tree_class->set_value_at          = NULL;
	tree_class->is_editable           = NULL;

	tree_class->duplicate_value       = NULL;
	tree_class->free_value            = NULL;
	tree_class->initialize_value      = NULL;
	tree_class->value_is_empty        = NULL;
	tree_class->value_to_string       = NULL;

	tree_class->pre_change            = NULL;
	tree_class->no_change             = NULL;
	tree_class->node_changed          = NULL;
	tree_class->node_data_changed     = NULL;
	tree_class->node_col_changed      = NULL;
	tree_class->node_inserted         = NULL;
	tree_class->node_removed          = NULL;
	tree_class->node_deleted          = NULL;
	tree_class->node_request_collapse = NULL;
}

E_MAKE_TYPE(e_tree_model, "ETreeModel", ETreeModel, e_tree_model_class_init, NULL, G_TYPE_OBJECT)


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
	
	d(g_print("Emitting pre_change on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [PRE_CHANGE], 0);
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
e_tree_model_no_change  (ETreeModel *tree_model)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	d(g_print("Emitting no_change on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NO_CHANGE], 0);
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
	
	d(g_print("Emitting node_changed on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_CHANGED], 0, node);
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
	
	d(g_print("Emitting node_data_changed on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_DATA_CHANGED], 0, node);
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
	
	d(g_print("Emitting node_col_changed on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_COL_CHANGED], 0, node, col);
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

	d(g_print("Emitting node_inserted on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_INSERTED], 0,
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
	
	d(g_print("Emitting node_removed on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_REMOVED], 0,
		       parent_node, removed_node, old_position);
}

/**
 * e_tree_model_node_deleted:
 * @tree_model: 
 * @deleted_node: 
 * 
 * 
 **/
void
e_tree_model_node_deleted  (ETreeModel *tree_model, ETreePath deleted_node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	d(g_print("Emitting node_deleted on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_DELETED], 0, deleted_node);
}

/**
 * e_tree_model_node_request_collapse:
 * @tree_model: 
 * @collapsed_node: 
 * 
 * 
 **/
void
e_tree_model_node_request_collapse  (ETreeModel *tree_model, ETreePath collapsed_node)
{
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	
	d(g_print("Emitting node_request_collapse on model 0x%p, a %s.\n", tree_model, gtk_type_name (GTK_OBJECT(tree_model)->klass->type)));

	g_signal_emit (G_OBJECT (tree_model), e_tree_model_signals [NODE_REQUEST_COLLAPSE], 0, collapsed_node);
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
	return (ETreeModel *) g_object_new (E_TREE_MODEL_TYPE, NULL);
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
	g_return_val_if_fail(node != NULL, FALSE);

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
 * e_tree_model_has_get_node_by_id
 * @etree: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree can quickly get a node from its save id.
 */
gboolean
e_tree_model_has_get_node_by_id (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), FALSE);

	if (ETM_CLASS(etree)->has_get_node_by_id)
		return ETM_CLASS(etree)->has_get_node_by_id (etree);
	else
		return FALSE;
}

/**
 * e_tree_model_get_node_by_id
 * @etree: The ETreeModel.
 * @node: The ETreePath.
 *
 * get_node_by_id(get_save_id(node)) should be the original node.
 * Likewise if get_node_by_id is not NULL, then
 * get_save_id(get_node_by_id(string)) should be a copy of the
 * original string.
 *
 * return values: The path for this save id.
 */
ETreePath
e_tree_model_get_node_by_id (ETreeModel *etree, const char *save_id)
{
	g_return_val_if_fail (etree != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), NULL);

	if (ETM_CLASS(etree)->get_node_by_id)
		return ETM_CLASS(etree)->get_node_by_id (etree, save_id);
	else
		return NULL;
}

/**
 * e_tree_model_has_change_pending
 * @etree: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree has valid save id data.
 */
gboolean
e_tree_model_has_change_pending (ETreeModel *etree)
{
	g_return_val_if_fail (etree != NULL, FALSE);
	g_return_val_if_fail (E_IS_TREE_MODEL (etree), FALSE);

	if (ETM_CLASS(etree)->has_change_pending)
		return ETM_CLASS(etree)->has_change_pending (etree);
	else
		return FALSE;
}

/**
 * e_tree_model_value_at:
 * @etree: The ETreeModel.
 * @node: The ETreePath to the node we're getting the data from.
 * @col: the column to retrieve data from
 * 
 * Return value: This function returns the value that is stored by the
 * @etree in column @col and node @node.  The data returned can be a
 * pointer or any data value that can be stored inside a pointer.
 *
 * The data returned is typically used by an ECell renderer.
 *
 * The data returned must be valid until the model sends a signal that
 * affect that piece of data.  node_changed and node_deleted affect
 * all data in tha t node and all nodes under that node.
 * node_data_changed affects the data in that node.  node_col_changed
 * affects the data in that node for that column.  node_inserted,
 * node_removed, and no_change don't affect any data in this way.
 **/
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
		if (func (model, child, data))
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

		if (func (model, child, data))
			return;

		next_child = e_tree_model_node_get_next (model, child);
		e_tree_model_node_traverse_preorder (model, child, func, data);

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
static ETreePath
e_tree_model_node_real_traverse (ETreeModel *model, ETreePath path, ETreePath end_path, gboolean forward_direction, ETreePathFunc func, gpointer data)
{
	ETreePath child;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	if (forward_direction)
		child = e_tree_model_node_get_first_child (model, path);
	else
		child = e_tree_model_node_get_last_child (model, path);

	while (child) {
		ETreePath result;

		if (forward_direction && (child == end_path || func (model, child, data)))
			return child;

		if ((result = e_tree_model_node_real_traverse (model, child, end_path, forward_direction, func, data)))
			return result;

		if (!forward_direction && (child == end_path || func (model, child, data)))
			return child;

		if (forward_direction)
			child = e_tree_model_node_get_next (model, child);
		else
			child = e_tree_model_node_get_prev (model, child);
	}
	return NULL;
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
ETreePath
e_tree_model_node_find (ETreeModel *model, ETreePath path, ETreePath end_path, gboolean forward_direction, ETreePathFunc func, gpointer data)
{
	ETreePath result;
	ETreePath next;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_TREE_MODEL (model), NULL);

	/* Just search the whole tree in this case. */
	if (path == NULL) {
		ETreePath root;
		root = e_tree_model_get_root (model);

		if (forward_direction && (end_path == root || func (model, root, data)))
			return root;

		if ((result = e_tree_model_node_real_traverse (model, root, end_path, forward_direction, func, data)))
			return result;

		if (!forward_direction && (end_path == root || func (model, root, data)))
			return root;

		return NULL;
	}

	while (1) {

		if (forward_direction) {
			if ((result = e_tree_model_node_real_traverse (model, path, end_path, forward_direction, func, data)))
				return result;
			next = e_tree_model_node_get_next (model, path);
		} else {
			next = e_tree_model_node_get_prev (model, path);
			if (next && (result = e_tree_model_node_real_traverse (model, next, end_path, forward_direction, func, data)))
				return result;
		}

		while (next == NULL) {
			path = e_tree_model_node_get_parent (model, path);

			if (path == NULL)
				return NULL;

			if (forward_direction)
				next = e_tree_model_node_get_next (model, path);
			else
				next = path;
		}

		if (end_path == next || func (model, next, data))
			return next;

		path = next;
	}
}


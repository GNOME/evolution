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
 *		Chris Lahey  <clahey@ximian.com>
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-tree-model.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-marshal.h"
#include "e-xml-utils.h"

#define ETM_CLASS(e) (E_TREE_MODEL_GET_CLASS(e))

#define d(x)

G_DEFINE_TYPE (ETreeModel, e_tree_model, G_TYPE_OBJECT)

enum {
	PRE_CHANGE,
	NODE_CHANGED,
	NODE_DATA_CHANGED,
	NODE_INSERTED,
	NODE_REMOVED,
	NODE_DELETED,
	REBUILT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

static void
e_tree_model_class_init (ETreeModelClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	signals[PRE_CHANGE] = g_signal_new (
		"pre_change",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, pre_change),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REBUILT] = g_signal_new (
		"rebuilt",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, rebuilt),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[NODE_CHANGED] = g_signal_new (
		"node_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, node_changed),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[NODE_DATA_CHANGED] = g_signal_new (
		"node_data_changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, node_data_changed),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[NODE_INSERTED] = g_signal_new (
		"node_inserted",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, node_inserted),
		(GSignalAccumulator) NULL, NULL,
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);

	signals[NODE_REMOVED] = g_signal_new (
		"node_removed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, node_removed),
		(GSignalAccumulator) NULL, NULL,
		e_marshal_VOID__POINTER_POINTER_INT,
		G_TYPE_NONE, 3,
		G_TYPE_POINTER,
		G_TYPE_POINTER,
		G_TYPE_INT);

	signals[NODE_DELETED] = g_signal_new (
		"node_deleted",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ETreeModelClass, node_deleted),
		(GSignalAccumulator) NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	class->get_root              = NULL;

	class->get_parent            = NULL;
	class->get_first_child       = NULL;
	class->get_next              = NULL;

	class->is_root               = NULL;
	class->is_expandable         = NULL;
	class->get_children          = NULL;
	class->depth                 = NULL;

	class->icon_at               = NULL;

	class->get_expanded_default  = NULL;
	class->column_count          = NULL;

	class->has_save_id           = NULL;
	class->get_save_id           = NULL;
	class->has_get_node_by_id    = NULL;
	class->get_node_by_id        = NULL;

	class->has_change_pending    = NULL;

	class->sort_value_at	  = NULL;
	class->value_at              = NULL;
	class->set_value_at          = NULL;
	class->is_editable           = NULL;

	class->duplicate_value       = NULL;
	class->free_value            = NULL;
	class->initialize_value      = NULL;
	class->value_is_empty        = NULL;
	class->value_to_string       = NULL;

	class->pre_change            = NULL;
	class->rebuilt		     = NULL;
	class->node_changed          = NULL;
	class->node_data_changed     = NULL;
	class->node_inserted         = NULL;
	class->node_removed          = NULL;
	class->node_deleted          = NULL;
}

static void
e_tree_model_init (ETreeModel *tree_model)
{
	/* nothing to do */
}

/* signals */

/**
 * e_tree_model_pre_change:
 * @tree_model:
 *
 * Return value:
 **/
void
e_tree_model_pre_change (ETreeModel *tree_model)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (tree_model, signals[PRE_CHANGE], 0);
}

/**
 * e_tree_model_rebuilt:
 * @tree_model:
 *
 * Return value:
 **/
void
e_tree_model_rebuilt (ETreeModel *tree_model)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (tree_model, signals[REBUILT], 0);
}
/**
 * e_tree_model_node_changed:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
void
e_tree_model_node_changed (ETreeModel *tree_model,
                           ETreePath path)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (tree_model, signals[NODE_CHANGED], 0, path);
}

/**
 * e_tree_model_node_data_changed:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
void
e_tree_model_node_data_changed (ETreeModel *tree_model,
                                ETreePath path)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (tree_model, signals[NODE_DATA_CHANGED], 0, path);
}

/**
 * e_tree_model_node_inserted:
 * @tree_model:
 * @parent_path:
 * @inserted_path:
 *
 *
 **/
void
e_tree_model_node_inserted (ETreeModel *tree_model,
                            ETreePath parent_path,
                            ETreePath inserted_path)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (
		tree_model, signals[NODE_INSERTED], 0,
		parent_path, inserted_path);
}

/**
 * e_tree_model_node_removed:
 * @tree_model:
 * @parent_path:
 * @removed_path:
 *
 *
 **/
void
e_tree_model_node_removed (ETreeModel *tree_model,
                           ETreePath parent_path,
                           ETreePath removed_path,
                           gint old_position)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (
		tree_model, signals[NODE_REMOVED], 0,
		parent_path, removed_path, old_position);
}

/**
 * e_tree_model_node_deleted:
 * @tree_model:
 * @deleted_path:
 *
 *
 **/
void
e_tree_model_node_deleted (ETreeModel *tree_model,
                           ETreePath deleted_path)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	g_signal_emit (tree_model, signals[NODE_DELETED], 0, deleted_path);
}

/**
 * e_tree_model_new
 *
 * XXX docs here.
 *
 * return values: a newly constructed ETreeModel.
 */
ETreeModel *
e_tree_model_new (void)
{
	return g_object_new (E_TYPE_TREE_MODEL, NULL);
}

/**
 * e_tree_model_get_root
 * @tree_model: the ETreeModel of which we want the root node.
 *
 * Accessor for the root node of @tree_model.
 *
 * return values: the ETreePath corresponding to the root node.
 */
ETreePath
e_tree_model_get_root (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_root)
		return ETM_CLASS (tree_model)->get_root (tree_model);
	else
		return NULL;
}

/**
 * e_tree_model_node_get_parent:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
ETreePath
e_tree_model_node_get_parent (ETreeModel *tree_model,
                              ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_parent)
		return ETM_CLASS (tree_model)->get_parent (tree_model, path);
	else
		return NULL;
}

/**
 * e_tree_model_node_get_first_child:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
ETreePath
e_tree_model_node_get_first_child (ETreeModel *tree_model,
                                   ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_first_child)
		return ETM_CLASS (tree_model)->get_first_child (tree_model, path);
	else
		return NULL;
}

/**
 * e_tree_model_node_get_next:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
ETreePath
e_tree_model_node_get_next (ETreeModel *tree_model,
                            ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_next)
		return ETM_CLASS (tree_model)->get_next (tree_model, path);
	else
		return NULL;
}

/**
 * e_tree_model_node_is_root:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
gboolean
e_tree_model_node_is_root (ETreeModel *tree_model,
                           ETreePath path)
{
	g_return_val_if_fail (tree_model != NULL, FALSE);

	if (ETM_CLASS (tree_model)->is_root)
		return ETM_CLASS (tree_model)->is_root (tree_model, path);
	else
		return FALSE;
}

/**
 * e_tree_model_node_is_expandable:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
gboolean
e_tree_model_node_is_expandable (ETreeModel *tree_model,
                                 ETreePath path)
{
	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	if (ETM_CLASS (tree_model)->is_expandable)
		return ETM_CLASS (tree_model)->is_expandable (tree_model, path);
	else
		return FALSE;
}

guint
e_tree_model_node_get_children (ETreeModel *tree_model,
                                ETreePath path,
                                ETreePath **paths)
{
	g_return_val_if_fail (tree_model != NULL, 0);
	if (ETM_CLASS (tree_model)->get_children)
		return ETM_CLASS (tree_model)->get_children (tree_model, path, paths);
	else
		return 0;
}

/**
 * e_tree_model_node_depth:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
guint
e_tree_model_node_depth (ETreeModel *tree_model,
                         ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), 0);

	if (ETM_CLASS (tree_model)->depth)
		return ETM_CLASS (tree_model)->depth (tree_model, path);
	else
		return 0;
}

/**
 * e_tree_model_icon_at
 * @tree_model: The ETreeModel.
 * @path: The ETreePath to the node we're getting the icon of.
 *
 * XXX docs here.
 *
 * return values: the GdkPixbuf associated with this node.
 */
GdkPixbuf *
e_tree_model_icon_at (ETreeModel *tree_model,
                      ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->icon_at)
		return ETM_CLASS (tree_model)->icon_at (tree_model, path);
	else
		return NULL;
}

/**
 * e_tree_model_get_expanded_default
 * @tree_model: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether nodes should be expanded by default.
 */
gboolean
e_tree_model_get_expanded_default (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), FALSE);

	if (ETM_CLASS (tree_model)->get_expanded_default)
		return ETM_CLASS (tree_model)->get_expanded_default (tree_model);
	else
		return FALSE;
}

/**
 * e_tree_model_column_count
 * @tree_model: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: The number of columns
 */
gint
e_tree_model_column_count (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), 0);

	if (ETM_CLASS (tree_model)->column_count)
		return ETM_CLASS (tree_model)->column_count (tree_model);
	else
		return 0;
}

/**
 * e_tree_model_has_save_id
 * @tree_model: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree has valid save id data.
 */
gboolean
e_tree_model_has_save_id (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), FALSE);

	if (ETM_CLASS (tree_model)->has_save_id)
		return ETM_CLASS (tree_model)->has_save_id (tree_model);
	else
		return FALSE;
}

/**
 * e_tree_model_get_save_id
 * @tree_model: The ETreeModel.
 * @path: The ETreePath.
 *
 * XXX docs here.
 *
 * return values: The save id for this path.
 */
gchar *
e_tree_model_get_save_id (ETreeModel *tree_model,
                          ETreePath path)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_save_id)
		return ETM_CLASS (tree_model)->get_save_id (tree_model, path);
	else
		return NULL;
}

/**
 * e_tree_model_has_get_node_by_id
 * @tree_model: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree can quickly get a node from its save id.
 */
gboolean
e_tree_model_has_get_node_by_id (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), FALSE);

	if (ETM_CLASS (tree_model)->has_get_node_by_id)
		return ETM_CLASS (tree_model)->has_get_node_by_id (tree_model);
	else
		return FALSE;
}

/**
 * e_tree_model_get_node_by_id
 * @tree_model: The ETreeModel.
 * @save_id:
 *
 * get_node_by_id(get_save_id(node)) should be the original node.
 * Likewise if get_node_by_id is not NULL, then
 * get_save_id(get_node_by_id(string)) should be a copy of the
 * original string.
 *
 * return values: The path for this save id.
 */
ETreePath
e_tree_model_get_node_by_id (ETreeModel *tree_model,
                             const gchar *save_id)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->get_node_by_id)
		return ETM_CLASS (tree_model)->get_node_by_id (tree_model, save_id);
	else
		return NULL;
}

/**
 * e_tree_model_has_change_pending
 * @tree_model: The ETreeModel.
 *
 * XXX docs here.
 *
 * return values: Whether this tree has valid save id data.
 */
gboolean
e_tree_model_has_change_pending (ETreeModel *tree_model)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), FALSE);

	if (ETM_CLASS (tree_model)->has_change_pending)
		return ETM_CLASS (tree_model)->has_change_pending (tree_model);
	else
		return FALSE;
}

/**
 * e_tree_model_sort_value_at:
 * @tree_model: The ETreeModel.
 * @path: The ETreePath to the node we're getting the data from.
 * @col: the column to retrieve data from
 *
 * Return value: This function returns the value that is stored by the
 * @tree_model in column @col and node @path.  The data returned can be a
 * pointer or any data value that can be stored inside a pointer.
 *
 * The data returned is typically used by an sort renderer if it wants
 * to proxy the data of cell value_at at a better sorting order.
 *
 * The data returned must be valid until the model sends a signal that
 * affect that piece of data.  node_changed and node_deleted affect
 * all data in tha t node and all nodes under that node.
 * node_data_changed affects the data in that node.  node_col_changed
 * affects the data in that node for that column.  node_inserted,
 * node_removed, and no_change don't affect any data in this way.
 **/
gpointer
e_tree_model_sort_value_at (ETreeModel *tree_model,
                            ETreePath path,
                            gint col)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->sort_value_at)
		return ETM_CLASS (tree_model)->sort_value_at (tree_model, path, col);
	else
		return NULL;
}

/**
 * e_tree_model_value_at:
 * @tree_model: The ETreeModel.
 * @path: The ETreePath to the node we're getting the data from.
 * @col: the column to retrieve data from
 *
 * Return value: This function returns the value that is stored by the
 * @tree_model in column @col and node @path.  The data returned can be a
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
gpointer
e_tree_model_value_at (ETreeModel *tree_model,
                       ETreePath path,
                       gint col)
{
	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	if (ETM_CLASS (tree_model)->value_at)
		return ETM_CLASS (tree_model)->value_at (tree_model, path, col);
	else
		return NULL;
}

void
e_tree_model_set_value_at (ETreeModel *tree_model,
                           ETreePath path,
                           gint col,
                           gconstpointer val)
{
	g_return_if_fail (E_IS_TREE_MODEL (tree_model));

	if (ETM_CLASS (tree_model)->set_value_at)
		ETM_CLASS (tree_model)->set_value_at (tree_model, path, col, val);
}

/**
 * e_tree_model_node_is_editable:
 * @tree_model:
 * @path:
 *
 *
 *
 * Return value:
 **/
gboolean
e_tree_model_node_is_editable (ETreeModel *tree_model,
                               ETreePath path,
                               gint col)
{
	g_return_val_if_fail (tree_model != NULL, FALSE);

	if (ETM_CLASS (tree_model)->is_editable)
		return ETM_CLASS (tree_model)->is_editable (tree_model, path, col);
	else
		return FALSE;
}

/**
 * e_tree_model_duplicate_value:
 * @tree_model:
 * @col:
 * @value:
 *
 *
 * Return value:
 **/
gpointer
e_tree_model_duplicate_value (ETreeModel *tree_model,
                              gint col,
                              gconstpointer value)
{
	g_return_val_if_fail (tree_model != NULL, NULL);

	if (ETM_CLASS (tree_model)->duplicate_value)
		return ETM_CLASS (tree_model)->duplicate_value (tree_model, col, value);
	else
		return NULL;
}

/**
 * e_tree_model_free_value:
 * @tree_model:
 * @col:
 * @value:
 *
 *
 * Return value:
 **/
void
e_tree_model_free_value (ETreeModel *tree_model,
                         gint col,
                         gpointer value)
{
	g_return_if_fail (tree_model != NULL);

	if (ETM_CLASS (tree_model)->free_value)
		ETM_CLASS (tree_model)->free_value (tree_model, col, value);
}

/**
 * e_tree_model_initialize_value:
 * @tree_model:
 * @col:
 *
 *
 *
 * Return value:
 **/
gpointer
e_tree_model_initialize_value (ETreeModel *tree_model,
                               gint col)
{
	g_return_val_if_fail (tree_model != NULL, NULL);

	if (ETM_CLASS (tree_model)->initialize_value)
		return ETM_CLASS (tree_model)->initialize_value (tree_model, col);
	else
		return NULL;
}

/**
 * e_tree_model_value_is_empty:
 * @tree_model:
 * @col:
 * @value:
 *
 *
 * Return value:
 **/
gboolean
e_tree_model_value_is_empty (ETreeModel *tree_model,
                             gint col,
                             gconstpointer value)
{
	g_return_val_if_fail (tree_model != NULL, TRUE);

	if (ETM_CLASS (tree_model)->value_is_empty)
		return ETM_CLASS (tree_model)->value_is_empty (tree_model, col, value);
	else
		return TRUE;
}

/**
 * e_tree_model_value_to_string:
 * @tree_model:
 * @col:
 * @value:
 *
 *
 * Return value:
 **/
gchar *
e_tree_model_value_to_string (ETreeModel *tree_model,
                              gint col,
                              gconstpointer value)
{
	g_return_val_if_fail (tree_model != NULL, g_strdup (""));

	if (ETM_CLASS (tree_model)->value_to_string)
		return ETM_CLASS (tree_model)->value_to_string (tree_model, col, value);
	else
		return g_strdup ("");
}

/**
 * e_tree_model_node_traverse:
 * @tree_model:
 * @path:
 * @func:
 * @data:
 *
 *
 **/
void
e_tree_model_node_traverse (ETreeModel *tree_model,
                            ETreePath path,
                            ETreePathFunc func,
                            gpointer data)
{
	ETreePath child;

	g_return_if_fail (E_IS_TREE_MODEL (tree_model));
	g_return_if_fail (path != NULL);

	child = e_tree_model_node_get_first_child (tree_model, path);

	while (child) {
		ETreePath next_child;

		next_child = e_tree_model_node_get_next (tree_model, child);
		e_tree_model_node_traverse (tree_model, child, func, data);
		if (func (tree_model, child, data))
			return;

		child = next_child;
	}
}

static ETreePath
e_tree_model_node_real_traverse (ETreeModel *model,
                                 ETreePath path,
                                 ETreePath end_path,
                                 ETreePathFunc func,
                                 gpointer data)
{
	ETreePath child;

	g_return_val_if_fail (E_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (path != NULL, NULL);

	child = e_tree_model_node_get_first_child (model, path);

	while (child) {
		ETreePath result;

		if (child == end_path || func (model, child, data))
			return child;

		if ((result = e_tree_model_node_real_traverse (
			model, child, end_path, func, data)))
			return result;

		child = e_tree_model_node_get_next (model, child);
	}

	return NULL;
}

/**
 * e_tree_model_node_find:
 * @tree_model:
 * @path:
 * @end_path:
 * @func:
 * @data:
 *
 *
 **/
ETreePath
e_tree_model_node_find (ETreeModel *tree_model,
                        ETreePath path,
                        ETreePath end_path,
                        ETreePathFunc func,
                        gpointer data)
{
	ETreePath result;
	ETreePath next;

	g_return_val_if_fail (E_IS_TREE_MODEL (tree_model), NULL);

	/* Just search the whole tree in this case. */
	if (path == NULL) {
		ETreePath root;
		root = e_tree_model_get_root (tree_model);

		if (end_path == root || func (tree_model, root, data))
			return root;

		result = e_tree_model_node_real_traverse (
			tree_model, root, end_path, func, data);
		if (result)
			return result;

		return NULL;
	}

	while (1) {

		if ((result = e_tree_model_node_real_traverse (
			tree_model, path, end_path, func, data)))
			return result;
		next = e_tree_model_node_get_next (tree_model, path);

		while (next == NULL) {
			path = e_tree_model_node_get_parent (tree_model, path);

			if (path == NULL)
				return NULL;

			next = e_tree_model_node_get_next (tree_model, path);
		}

		if (end_path == next || func (tree_model, next, data))
			return next;

		path = next;
	}
}


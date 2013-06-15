/*
 *
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TREE_MODEL_H_
#define _E_TREE_MODEL_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

/* Standard GObject macros */
#define E_TYPE_TREE_MODEL \
	(e_tree_model_get_type ())
#define E_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_MODEL, ETreeModel))
#define E_TREE_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_MODEL, ETreeModelClass))
#define E_IS_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_MODEL))
#define E_IS_TREE_MODEL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_MODEL))
#define E_TREE_MODEL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_MODEL, ETreeModelClass))

G_BEGIN_DECLS

typedef gpointer ETreePath;

typedef struct _ETreeModel ETreeModel;
typedef struct _ETreeModelClass ETreeModelClass;

typedef gboolean	(*ETreePathFunc)	(ETreeModel *tree_model,
						 ETreePath path,
						 gpointer data);

struct _ETreeModel {
	GObject parent;
};

struct _ETreeModelClass {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	ETreePath	(*get_root)		(ETreeModel *tree_model);

	ETreePath	(*get_parent)		(ETreeModel *tree_model,
						 ETreePath path);
	ETreePath	(*get_first_child)	(ETreeModel *tree_model,
						 ETreePath path);
	ETreePath	(*get_next)		(ETreeModel *tree_model,
						 ETreePath path);

	gboolean	(*is_root)		(ETreeModel *tree_model,
						 ETreePath path);
	gboolean	(*is_expandable)	(ETreeModel *tree_model,
						 ETreePath path);
	guint		(*get_children)		(ETreeModel *tree_model,
						 ETreePath path,
						 ETreePath **paths);
	guint		(*depth)		(ETreeModel *tree_model,
						 ETreePath path);

	GdkPixbuf *	(*icon_at)		(ETreeModel *tree_model,
						 ETreePath path);

	gboolean	(*get_expanded_default)	(ETreeModel *tree_model);
	gint		(*column_count)		(ETreeModel *tree_model);

	gboolean	(*has_save_id)		(ETreeModel *tree_model);
	gchar *		(*get_save_id)		(ETreeModel *tree_model,
						 ETreePath path);

	gboolean	(*has_get_node_by_id)	(ETreeModel *tree_model);
	ETreePath	(*get_node_by_id)	(ETreeModel *tree_model,
						 const gchar *save_id);

	gboolean	(*has_change_pending)	(ETreeModel *tree_model);

	/*
	 * ETable analogs
	 */
	gpointer	(*sort_value_at)	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
	gpointer	(*value_at)		(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
	void		(*set_value_at)		(ETreeModel *tree_model,
						 ETreePath path,
						 gint col,
						 gconstpointer val);
	gboolean	(*is_editable)		(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);

	gpointer	(*duplicate_value)	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);
	void		(*free_value)		(ETreeModel *tree_model,
						 gint col,
						 gpointer value);
	gpointer	(*initialize_value)	(ETreeModel *tree_model,
						 gint col);
	gboolean	(*value_is_empty)	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);
	gchar *		(*value_to_string)	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);

	/*
	 * Signals
	 */

	/* During node_remove, the ETreePath of the child is removed
	 * from the tree but is still a valid ETreePath.  At
	 * node_deleted, the ETreePath is no longer valid.
	 */

	void		(*pre_change)		(ETreeModel *tree_model);
	void		(*no_change)		(ETreeModel *tree_model);
	void		(*node_changed)		(ETreeModel *tree_model,
						 ETreePath path);
	void		(*node_data_changed)	(ETreeModel *tree_model,
						 ETreePath path);
	void		(*node_col_changed)	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
	void		(*node_inserted)	(ETreeModel *tree_model,
						 ETreePath parent,
						 ETreePath inserted_path);
	void		(*node_removed)		(ETreeModel *tree_model,
						 ETreePath parent,
						 ETreePath removed_path,
						 gint old_position);
	void		(*node_deleted)		(ETreeModel *tree_model,
						 ETreePath deleted_path);
	void		(*rebuilt)		(ETreeModel *tree_model);

	/* This signal requests that any viewers of the tree that
	 * collapse and expand nodes collapse this node.
	 */
	void		(*node_request_collapse)
						(ETreeModel *tree_model,
						 ETreePath path);
};

GType		e_tree_model_get_type		(void) G_GNUC_CONST;
ETreeModel *	e_tree_model_new		(void);

/* tree traversal operations */
ETreePath	e_tree_model_get_root		(ETreeModel *tree_model);
ETreePath	e_tree_model_node_get_parent	(ETreeModel *tree_model,
						 ETreePath path);
ETreePath	e_tree_model_node_get_first_child
						(ETreeModel *tree_model,
						 ETreePath path);
ETreePath	e_tree_model_node_get_next	(ETreeModel *tree_model,
						 ETreePath path);

/* node accessors */
gboolean	e_tree_model_node_is_root	(ETreeModel *tree_model,
						 ETreePath path);
gboolean	e_tree_model_node_is_expandable	(ETreeModel *tree_model,
						 ETreePath path);
guint		e_tree_model_node_get_children	(ETreeModel *tree_model,
						 ETreePath path,
						 ETreePath **paths);
guint		e_tree_model_node_depth		(ETreeModel *tree_model,
						 ETreePath path);
GdkPixbuf *	e_tree_model_icon_at		(ETreeModel *tree_model,
						 ETreePath path);
gboolean	e_tree_model_get_expanded_default
						(ETreeModel *tree_model);
gint		e_tree_model_column_count	(ETreeModel *tree_model);
gboolean	e_tree_model_has_save_id	(ETreeModel *tree_model);
gchar *		e_tree_model_get_save_id	(ETreeModel *tree_model,
						 ETreePath path);
gboolean	e_tree_model_has_get_node_by_id	(ETreeModel *tree_model);
ETreePath	e_tree_model_get_node_by_id	(ETreeModel *tree_model,
						 const gchar *save_id);
gboolean	e_tree_model_has_change_pending	(ETreeModel *tree_model);
gpointer	e_tree_model_sort_value_at	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
gpointer	e_tree_model_value_at		(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
void		e_tree_model_set_value_at	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col,
						 gconstpointer val);
gboolean	e_tree_model_node_is_editable	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
gpointer	e_tree_model_duplicate_value	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);
void		e_tree_model_free_value		(ETreeModel *tree_model,
						 gint col,
						 gpointer value);
gpointer	e_tree_model_initialize_value	(ETreeModel *tree_model,
						 gint col);
gboolean	e_tree_model_value_is_empty	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);
gchar *		e_tree_model_value_to_string	(ETreeModel *tree_model,
						 gint col,
						 gconstpointer value);

/* depth first traversal of path's descendents, calling func on each one */
void		e_tree_model_node_traverse	(ETreeModel *tree_model,
						 ETreePath path,
						 ETreePathFunc func,
						 gpointer data);
ETreePath	e_tree_model_node_find		(ETreeModel *tree_model,
						 ETreePath path,
						 ETreePath end_path,
						 ETreePathFunc func,
						 gpointer data);

/*
** Routines for emitting signals on the ETreeModel
*/
void		e_tree_model_pre_change		(ETreeModel *tree_model);
void		e_tree_model_no_change		(ETreeModel *tree_model);
void		e_tree_model_rebuilt		(ETreeModel *tree_model);
void		e_tree_model_node_changed	(ETreeModel *tree_model,
						 ETreePath path);
void		e_tree_model_node_data_changed	(ETreeModel *tree_model,
						 ETreePath path);
void		e_tree_model_node_col_changed	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
void		e_tree_model_node_inserted	(ETreeModel *tree_model,
						 ETreePath parent_path,
						 ETreePath inserted_path);
void		e_tree_model_node_removed	(ETreeModel *tree_model,
						 ETreePath parent_path,
						 ETreePath removed_path,
						 gint old_position);
void		e_tree_model_node_deleted	(ETreeModel *tree_model,
						 ETreePath deleted_path);
void		e_tree_model_node_request_collapse
						(ETreeModel *tree_model,
						 ETreePath collapsed_path);

G_END_DECLS

#endif /* _E_TREE_MODEL_H */

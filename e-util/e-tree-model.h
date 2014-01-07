/*
 * e-tree-model.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TREE_MODEL_H
#define E_TREE_MODEL_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_TREE_MODEL \
	(e_tree_model_get_type ())
#define E_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_MODEL, ETreeModel))
#define E_IS_TREE_MODEL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_MODEL))
#define E_TREE_MODEL_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_TREE_MODEL, ETreeModelInterface))

G_BEGIN_DECLS

typedef gpointer ETreePath;

typedef struct _ETreeModel ETreeModel;
typedef struct _ETreeModelInterface ETreeModelInterface;

typedef gboolean	(*ETreePathFunc)	(ETreeModel *tree_model,
						 ETreePath path,
						 gpointer data);

struct _ETreeModelInterface {
	GTypeInterface parent_interface;

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
	guint		(*get_n_nodes)		(ETreeModel *tree_model);
	guint		(*get_n_children)	(ETreeModel *tree_model,
						 ETreePath path);
	guint		(*depth)		(ETreeModel *tree_model,
						 ETreePath path);

	gboolean	(*get_expanded_default)	(ETreeModel *tree_model);
	gint		(*column_count)		(ETreeModel *tree_model);

	gchar *		(*get_save_id)		(ETreeModel *tree_model,
						 ETreePath path);

	ETreePath	(*get_node_by_id)	(ETreeModel *tree_model,
						 const gchar *save_id);

	/*
	 * ETable analogs
	 */
	gpointer	(*sort_value_at)	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
	gpointer	(*value_at)		(ETreeModel *tree_model,
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
	void		(*node_changed)		(ETreeModel *tree_model,
						 ETreePath path);
	void		(*node_data_changed)	(ETreeModel *tree_model,
						 ETreePath path);
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
};

GType		e_tree_model_get_type		(void) G_GNUC_CONST;

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
guint		e_tree_model_node_get_n_nodes	(ETreeModel *tree_model);
guint		e_tree_model_node_get_n_children
						(ETreeModel *tree_model,
						 ETreePath path);
guint		e_tree_model_node_depth		(ETreeModel *tree_model,
						 ETreePath path);
gboolean	e_tree_model_get_expanded_default
						(ETreeModel *tree_model);
gint		e_tree_model_column_count	(ETreeModel *tree_model);
gchar *		e_tree_model_get_save_id	(ETreeModel *tree_model,
						 ETreePath path);
ETreePath	e_tree_model_get_node_by_id	(ETreeModel *tree_model,
						 const gchar *save_id);
gpointer	e_tree_model_sort_value_at	(ETreeModel *tree_model,
						 ETreePath path,
						 gint col);
gpointer	e_tree_model_value_at		(ETreeModel *tree_model,
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
void		e_tree_model_rebuilt		(ETreeModel *tree_model);
void		e_tree_model_node_changed	(ETreeModel *tree_model,
						 ETreePath path);
void		e_tree_model_node_data_changed	(ETreeModel *tree_model,
						 ETreePath path);
void		e_tree_model_node_inserted	(ETreeModel *tree_model,
						 ETreePath parent_path,
						 ETreePath inserted_path);
void		e_tree_model_node_removed	(ETreeModel *tree_model,
						 ETreePath parent_path,
						 ETreePath removed_path,
						 gint old_position);
void		e_tree_model_node_deleted	(ETreeModel *tree_model,
						 ETreePath deleted_path);

G_END_DECLS

#endif /* E_TREE_MODEL_H */

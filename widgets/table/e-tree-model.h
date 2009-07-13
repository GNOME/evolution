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

#ifndef _E_TREE_MODEL_H_
#define _E_TREE_MODEL_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define E_TREE_MODEL_TYPE        (e_tree_model_get_type ())
#define E_TREE_MODEL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_MODEL_TYPE, ETreeModel))
#define E_TREE_MODEL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_MODEL_TYPE, ETreeModelClass))
#define E_IS_TREE_MODEL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_MODEL_TYPE))
#define E_IS_TREE_MODEL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_MODEL_TYPE))
#define E_TREE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TREE_MODEL_TYPE, ETreeModelClass))

typedef gpointer  ETreePath;
typedef struct ETreeModel ETreeModel;
typedef struct ETreeModelClass ETreeModelClass;
typedef gint (*ETreePathCompareFunc)(ETreeModel *model, ETreePath path1, ETreePath path2);
typedef gboolean (*ETreePathFunc)(ETreeModel *model, ETreePath path, gpointer data);

struct ETreeModel {
	GObject base;
};

struct ETreeModelClass {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	ETreePath  (*get_root)              (ETreeModel *etm);

	ETreePath  (*get_parent)            (ETreeModel *etm, ETreePath node);
	ETreePath  (*get_first_child)       (ETreeModel *etm, ETreePath node);
	ETreePath  (*get_last_child)        (ETreeModel *etm, ETreePath node);
	ETreePath  (*get_next)              (ETreeModel *etm, ETreePath node);
	ETreePath  (*get_prev)              (ETreeModel *etm, ETreePath node);

	gboolean   (*is_root)               (ETreeModel *etm, ETreePath node);
	gboolean   (*is_expandable)         (ETreeModel *etm, ETreePath node);
	guint      (*get_children)          (ETreeModel *etm, ETreePath node, ETreePath **paths);
	guint      (*depth)                 (ETreeModel *etm, ETreePath node);

	GdkPixbuf *(*icon_at)               (ETreeModel *etm, ETreePath node);

	gboolean   (*get_expanded_default)  (ETreeModel *etm);
	gint       (*column_count)          (ETreeModel *etm);

	gboolean   (*has_save_id)           (ETreeModel *etm);
	gchar     *(*get_save_id)           (ETreeModel *etm, ETreePath node);

	gboolean   (*has_get_node_by_id)    (ETreeModel *etm);
	ETreePath  (*get_node_by_id)        (ETreeModel *etm, const gchar *save_id);

	gboolean   (*has_change_pending)    (ETreeModel *etm);

	/*
	 * ETable analogs
	 */
	void      *(*sort_value_at)              (ETreeModel *etm, ETreePath node, gint col);
	void      *(*value_at)              (ETreeModel *etm, ETreePath node, gint col);
	void       (*set_value_at)          (ETreeModel *etm, ETreePath node, gint col, gconstpointer val);
	gboolean   (*is_editable)           (ETreeModel *etm, ETreePath node, gint col);

	void      *(*duplicate_value)       (ETreeModel *etm, gint col, gconstpointer value);
	void       (*free_value)            (ETreeModel *etm, gint col, gpointer value);
	void	  *(*initialize_value)      (ETreeModel *etm, gint col);
	gboolean   (*value_is_empty)        (ETreeModel *etm, gint col, gconstpointer value);
	gchar      *(*value_to_string)       (ETreeModel *etm, gint col, gconstpointer value);

	/*
	 * Signals
	 */

	/* During node_remove, the ETreePath of the child is removed
	 * from the tree but is still a valid ETreePath.  At
	 * node_deleted, the ETreePath is no longer valid.
	 */

	void       (*pre_change)            (ETreeModel *etm);
	void       (*no_change)             (ETreeModel *etm);
	void       (*node_changed)          (ETreeModel *etm, ETreePath node);
	void       (*node_data_changed)     (ETreeModel *etm, ETreePath node);
	void       (*node_col_changed)      (ETreeModel *etm, ETreePath node,   gint col);
	void       (*node_inserted)         (ETreeModel *etm, ETreePath parent, ETreePath inserted_node);
	void       (*node_removed)          (ETreeModel *etm, ETreePath parent, ETreePath removed_node, gint old_position);
	void       (*node_deleted)          (ETreeModel *etm, ETreePath deleted_node);
	void       (*rebuilt)          (ETreeModel *etm);

	/* This signal requests that any viewers of the tree that
	 * collapse and expand nodes collapse this node.
	 */
	void       (*node_request_collapse) (ETreeModel *etm, ETreePath node);
};

GType       e_tree_model_get_type                (void);
ETreeModel *e_tree_model_new                     (void);

/* tree traversal operations */
ETreePath   e_tree_model_get_root                (ETreeModel     *etree);
ETreePath   e_tree_model_node_get_parent         (ETreeModel     *etree,
						  ETreePath       path);
ETreePath   e_tree_model_node_get_first_child    (ETreeModel     *etree,
						  ETreePath       path);
ETreePath   e_tree_model_node_get_last_child     (ETreeModel     *etree,
						  ETreePath       path);
ETreePath   e_tree_model_node_get_next           (ETreeModel     *etree,
						  ETreePath       path);
ETreePath   e_tree_model_node_get_prev           (ETreeModel     *etree,
						  ETreePath       path);

/* node accessors */
gboolean    e_tree_model_node_is_root            (ETreeModel     *etree,
						  ETreePath       path);
gboolean    e_tree_model_node_is_expandable      (ETreeModel     *etree,
						  ETreePath       path);
guint       e_tree_model_node_get_children       (ETreeModel     *etree,
						  ETreePath       path,
						  ETreePath     **paths);
guint       e_tree_model_node_depth              (ETreeModel     *etree,
						  ETreePath       path);
GdkPixbuf  *e_tree_model_icon_at                 (ETreeModel     *etree,
						  ETreePath       path);
gboolean    e_tree_model_get_expanded_default    (ETreeModel     *model);
gint        e_tree_model_column_count            (ETreeModel     *model);
gboolean    e_tree_model_has_save_id             (ETreeModel     *model);
gchar      *e_tree_model_get_save_id             (ETreeModel     *model,
						  ETreePath       node);
gboolean    e_tree_model_has_get_node_by_id      (ETreeModel     *model);
ETreePath   e_tree_model_get_node_by_id          (ETreeModel     *model,
						  const gchar     *save_id);
gboolean    e_tree_model_has_change_pending      (ETreeModel     *model);
void       *e_tree_model_sort_value_at                (ETreeModel     *etree,
						  ETreePath       node,
						  gint             col);
void       *e_tree_model_value_at                (ETreeModel     *etree,
						  ETreePath       node,
						  gint             col);
void        e_tree_model_set_value_at            (ETreeModel     *etree,
						  ETreePath       node,
						  gint             col,
						  const void     *val);
gboolean    e_tree_model_node_is_editable        (ETreeModel     *etree,
						  ETreePath       node,
						  gint             col);
void       *e_tree_model_duplicate_value         (ETreeModel     *etree,
						  gint             col,
						  const void     *value);
void        e_tree_model_free_value              (ETreeModel     *etree,
						  gint             col,
						  void           *value);
void       *e_tree_model_initialize_value        (ETreeModel     *etree,
						  gint             col);
gboolean    e_tree_model_value_is_empty          (ETreeModel     *etree,
						  gint             col,
						  const void     *value);
gchar       *e_tree_model_value_to_string         (ETreeModel     *etree,
						  gint             col,
						  const void     *value);

/* depth first traversal of path's descendents, calling func on each one */
void        e_tree_model_node_traverse           (ETreeModel     *model,
						  ETreePath       path,
						  ETreePathFunc   func,
						  gpointer        data);
void        e_tree_model_node_traverse_preorder  (ETreeModel     *model,
						  ETreePath       path,
						  ETreePathFunc   func,
						  gpointer        data);
ETreePath   e_tree_model_node_find               (ETreeModel     *model,
						  ETreePath       path,
						  ETreePath       end_path,
						  gboolean        forward_direction,
						  ETreePathFunc   func,
						  gpointer        data);

/*
** Routines for emitting signals on the ETreeModel
*/
void        e_tree_model_pre_change              (ETreeModel     *tree_model);
void        e_tree_model_no_change               (ETreeModel     *tree_model);
void        e_tree_model_rebuilt                 (ETreeModel     *tree_model);
void        e_tree_model_node_changed            (ETreeModel     *tree_model,
						  ETreePath       node);
void        e_tree_model_node_data_changed       (ETreeModel     *tree_model,
						  ETreePath       node);
void        e_tree_model_node_col_changed        (ETreeModel     *tree_model,
						  ETreePath       node,
						  gint             col);
void        e_tree_model_node_inserted           (ETreeModel     *tree_model,
						  ETreePath       parent_node,
						  ETreePath       inserted_node);
void        e_tree_model_node_removed            (ETreeModel     *tree_model,
						  ETreePath       parent_node,
						  ETreePath       removed_node,
						  gint             old_position);
void        e_tree_model_node_deleted            (ETreeModel     *tree_model,
						  ETreePath       deleted_node);
void        e_tree_model_node_request_collapse   (ETreeModel     *tree_model,
						  ETreePath       deleted_node);

G_END_DECLS

#endif /* _E_TREE_MODEL_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_MODEL_H_
#define _E_TREE_MODEL_H_

#include "e-table-model.h"

#define E_TREE_MODEL_TYPE        (e_tree_model_get_type ())
#define E_TREE_MODEL(o)          (GTK_CHECK_CAST ((o), E_TREE_MODEL_TYPE, ETreeModel))
#define E_TREE_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_MODEL_TYPE, ETreeModelClass))
#define E_IS_TREE_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TREE_MODEL_TYPE))
#define E_IS_TREE_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_MODEL_TYPE))

typedef gpointer ETreePathItem;
typedef GList ETreePath;

typedef struct {
	ETableModel base;

	ETableModel *source;

	ETreePath *root_node;

	GArray  *array;

} ETreeModel;

typedef struct {
	ETableModelClass parent_class;

	/*
	 * Virtual methods
	 */
	ETreePath *(*get_root)      (ETreeModel *etm);

	ETreePath *(*get_next)      (ETreeModel *etm, ETreePath* node);
	ETreePath *(*get_prev)      (ETreeModel *etm, ETreePath* node);

	void      *(*value_at)      (ETreeModel *etm, ETreePath* node, int col);
	void       (*set_value_at)  (ETreeModel *etm, ETreePath* node, int col, const void *val);
	gboolean   (*is_editable)   (ETreeModel *etm, ETreePath* node, int col);

	guint      (*get_children)  (ETreeModel *etm, ETreePath* node, ETreePath ***paths);
	void       (*release_paths) (ETreeModel *etm, ETreePath **paths, guint num_paths);
	gboolean   (*is_expanded)   (ETreeModel *etm, ETreePath* node);
	void       (*set_expanded)  (ETreeModel *etm, ETreePath* node, gboolean expanded);

	/*
	 * Signals
	 */

} ETreeModelClass;

GtkType     e_tree_model_get_type (void);

ETreeModel *e_tree_model_new (void);

/* operations on "nodes" in the tree */
ETreePath * e_tree_model_get_root (ETreeModel *etree);
ETreePath * e_tree_model_node_at_row (ETreeModel *etree, int row);
guint e_tree_model_node_depth (ETreeModel *etree, ETreePath *path);
ETreePath *e_tree_model_node_get_parent (ETreeModel *etree, ETreePath *path);
ETreePath *e_tree_model_node_get_next (ETreeModel *etree, ETreePath *path);
ETreePath *e_tree_model_node_get_prev (ETreeModel *etree, ETreePath *path);
gboolean e_tree_model_node_is_root (ETreeModel *etree, ETreePath *path);

gboolean e_tree_model_node_is_expandable (ETreeModel *etree, ETreePath *path);
gboolean e_tree_model_node_is_expanded (ETreeModel *etree, ETreePath *path);
guint e_tree_model_node_get_children (ETreeModel *etree, ETreePath *path, ETreePath ***paths);
void e_tree_model_release_paths (ETreeModel *etree, ETreePath **paths, guint num_paths);
guint e_tree_model_node_num_visible_descendents (ETreeModel *etm, ETreePath *node);

#endif /* _E_TREE_MODEL_H */

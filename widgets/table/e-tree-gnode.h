/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _E_TREE_GNODE_H_
#define _E_TREE_GNODE_H_

#include "e-tree-model.h"

#define E_TREE_GNODE_TYPE        (e_tree_gnode_get_type ())
#define E_TREE_GNODE(o)          (GTK_CHECK_CAST ((o), E_TREE_GNODE_TYPE, ETreeGNode))
#define E_TREE_GNODE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_GNODE_TYPE, ETreeGNodeClass))
#define E_IS_TREE_GNODE(o)       (GTK_CHECK_TYPE ((o), E_TREE_GNODE_TYPE))
#define E_IS_TREE_GNODE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_GNODE_TYPE))

typedef void *(*ETreeGNodeValueAtFn)(ETreeModel *model, GNode *node, int col, void *data);


typedef struct {
	ETreeModel parent;

	GNode *root;

	ETreeGNodeValueAtFn value_at;

	void *data;
} ETreeGNode;

typedef struct {
	ETreeModelClass parent_class;
} ETreeGNodeClass;

GtkType e_tree_gnode_get_type (void);

ETreeModel *e_tree_gnode_new (GNode *tree,
			      ETreeGNodeValueAtFn value_at,
			      void *data);

#endif /* _E_TREE_GNODE_H_ */

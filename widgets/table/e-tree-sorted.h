/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_SORTED_H_
#define _E_TREE_SORTED_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/e-table/e-tree-model.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-header.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TREE_SORTED_TYPE        (e_tree_sorted_get_type ())
#define E_TREE_SORTED(o)          (GTK_CHECK_CAST ((o), E_TREE_SORTED_TYPE, ETreeSorted))
#define E_TREE_SORTED_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_SORTED_TYPE, ETreeSortedClass))
#define E_IS_TREE_SORTED(o)       (GTK_CHECK_TYPE ((o), E_TREE_SORTED_TYPE))
#define E_IS_TREE_SORTED_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_SORTED_TYPE))

typedef struct ETreeSorted ETreeSorted;
typedef struct ETreeSortedPriv ETreeSortedPriv;
typedef struct ETreeSortedClass ETreeSortedClass;

struct ETreeSorted {
	ETreeModel base;

	ETreeSortedPriv *priv;
};

struct ETreeSortedClass {
	ETreeModelClass parent_class;
};


GtkType      e_tree_sorted_get_type            (void);
void         e_tree_sorted_construct           (ETreeSorted    *etree,
						ETreeModel     *source,
						ETableHeader   *full_header,
						ETableSortInfo *sort_info);
ETreeSorted *e_tree_sorted_new                 (ETreeModel     *source,
						ETableHeader   *full_header,
						ETableSortInfo *sort_info);

ETreePath    e_tree_sorted_view_to_model_path  (ETreeSorted    *ets,
						ETreePath       view_path);
ETreePath    e_tree_sorted_model_to_view_path  (ETreeSorted    *ets,
						ETreePath       model_path);
int          e_tree_sorted_orig_position       (ETreeSorted    *ets,
						ETreePath       path);
int          e_tree_sorted_node_num_children   (ETreeSorted    *ets,
						ETreePath       path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_SORTED_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-sorted.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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

#ifndef _E_TREE_SORTED_H_
#define _E_TREE_SORTED_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-tree-model.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

#define E_TREE_SORTED_TYPE        (e_tree_sorted_get_type ())
#define E_TREE_SORTED(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_SORTED_TYPE, ETreeSorted))
#define E_TREE_SORTED_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_SORTED_TYPE, ETreeSortedClass))
#define E_IS_TREE_SORTED(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_SORTED_TYPE))
#define E_IS_TREE_SORTED_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_SORTED_TYPE))
#define E_TREE_SORTED_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TREE_SORTED_TYPE, ETreeSortedClass))

typedef struct ETreeSorted ETreeSorted;
typedef struct ETreeSortedPriv ETreeSortedPriv;
typedef struct ETreeSortedClass ETreeSortedClass;

struct ETreeSorted {
	ETreeModel base;

	ETreeSortedPriv *priv;
};

struct ETreeSortedClass {
	ETreeModelClass parent_class;

	/* Signals */
	void       (*node_resorted)         (ETreeSorted *etm, ETreePath node);
};


GType        e_tree_sorted_get_type            (void);
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

void         e_tree_sorted_node_resorted       (ETreeSorted    *tree_model,
						ETreePath       node);

ETableSortInfo* e_tree_sorted_get_sort_info       (ETreeSorted    *tree_model);
void            e_tree_sorted_set_sort_info       (ETreeSorted    *tree_model,
						   ETableSortInfo *sort_info);

G_END_DECLS

#endif /* _E_TREE_SORTED_H */

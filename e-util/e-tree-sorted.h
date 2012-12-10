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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TREE_SORTED_H_
#define _E_TREE_SORTED_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <e-util/e-table-header.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-tree-model.h>

/* Standard GObject macros */
#define E_TYPE_TREE_SORTED \
	(e_tree_sorted_get_type ())
#define E_TREE_SORTED(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_SORTED, ETreeSorted))
#define E_TREE_SORTED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_SORTED, ETreeSortedClass))
#define E_IS_TREE_SORTED(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_SORTED))
#define E_IS_TREE_SORTED_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_SORTED))
#define E_TREE_SORTED_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_SORTED, ETreeSortedClass))

G_BEGIN_DECLS

typedef struct _ETreeSorted ETreeSorted;
typedef struct _ETreeSortedClass ETreeSortedClass;
typedef struct _ETreeSortedPrivate ETreeSortedPrivate;

struct _ETreeSorted {
	ETreeModel parent;
	ETreeSortedPrivate *priv;
};

struct _ETreeSortedClass {
	ETreeModelClass parent_class;

	/* Signals */
	void		(*node_resorted)	(ETreeSorted *etm,
						 ETreePath node);
};

GType		e_tree_sorted_get_type		(void) G_GNUC_CONST;
void		e_tree_sorted_construct		(ETreeSorted *etree,
						 ETreeModel *source,
						 ETableHeader *full_header,
						 ETableSortInfo *sort_info);
ETreeSorted *	e_tree_sorted_new		(ETreeModel *source,
						 ETableHeader *full_header,
						 ETableSortInfo *sort_info);

ETreePath	e_tree_sorted_view_to_model_path
						(ETreeSorted *ets,
						 ETreePath view_path);
ETreePath	e_tree_sorted_model_to_view_path
						(ETreeSorted *ets,
						 ETreePath model_path);
gint		e_tree_sorted_orig_position	(ETreeSorted *ets,
						 ETreePath path);
gint		e_tree_sorted_node_num_children	(ETreeSorted *ets,
						 ETreePath path);

void		e_tree_sorted_node_resorted	(ETreeSorted *tree_model,
						 ETreePath node);

ETableSortInfo *e_tree_sorted_get_sort_info	(ETreeSorted *tree_model);
void		e_tree_sorted_set_sort_info	(ETreeSorted *tree_model,
						 ETableSortInfo *sort_info);

G_END_DECLS

#endif /* _E_TREE_SORTED_H */

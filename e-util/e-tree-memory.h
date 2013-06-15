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

#ifndef E_TREE_MEMORY_H
#define E_TREE_MEMORY_H

#include <e-util/e-tree-model.h>

/* Standard GObject macros */
#define E_TYPE_TREE_MEMORY \
	(e_tree_memory_get_type ())
#define E_TREE_MEMORY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_MEMORY, ETreeMemory))
#define E_TREE_MEMORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_MEMORY, ETreeMemoryClass))
#define E_IS_TREE_MEMORY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_MEMORY))
#define E_IS_TREE_MEMORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_MEMORY))
#define E_TREE_MEMORY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_MEMORY, ETreeMemoryClass))

G_BEGIN_DECLS

typedef struct _ETreeMemory ETreeMemory;
typedef struct _ETreeMemoryClass ETreeMemoryClass;
typedef struct _ETreeMemoryPrivate ETreeMemoryPrivate;

struct _ETreeMemory {
	ETreeModel parent;
	ETreeMemoryPrivate *priv;
};

struct _ETreeMemoryClass {
	ETreeModelClass parent_class;
};

GType		e_tree_memory_get_type		(void) G_GNUC_CONST;

/* node operations */
ETreePath	e_tree_memory_node_insert	(ETreeMemory *tree_memory,
						 ETreePath parent_node,
						 gint position,
						 gpointer node_data);
gpointer	e_tree_memory_node_remove	(ETreeMemory *tree_memory,
						 ETreePath node);

/* Freeze and thaw */
void		e_tree_memory_freeze		(ETreeMemory *tree_memory);
void		e_tree_memory_thaw		(ETreeMemory *tree_memory);
void		e_tree_memory_set_expanded_default
						(ETreeMemory *tree_memory,
						 gboolean expanded);
gpointer	e_tree_memory_node_get_data	(ETreeMemory *tree_memory,
						 ETreePath node);
void		e_tree_memory_node_set_data	(ETreeMemory *tree_memory,
						 ETreePath node,
						 gpointer node_data);

G_END_DECLS

#endif /* E_TREE_MEMORY_H */


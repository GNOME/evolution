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

#ifndef _E_TREE_MEMORY_H_
#define _E_TREE_MEMORY_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <table/e-tree-model.h>

G_BEGIN_DECLS

#define E_TREE_MEMORY_TYPE        (e_tree_memory_get_type ())
#define E_TREE_MEMORY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_MEMORY_TYPE, ETreeMemory))
#define E_TREE_MEMORY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_MEMORY_TYPE, ETreeMemoryClass))
#define E_IS_TREE_MEMORY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_MEMORY_TYPE))
#define E_IS_TREE_MEMORY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_MEMORY_TYPE))
#define E_TREE_MEMORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TREE_MEMORY_TYPE, ETreeMemoryClass))

typedef struct ETreeMemory ETreeMemory;
typedef struct ETreeMemoryPriv ETreeMemoryPriv;
typedef struct ETreeMemoryClass ETreeMemoryClass;

typedef gint (*ETreeMemorySortCallback) (ETreeMemory *etmm, ETreePath path1, ETreePath path2, gpointer closure);

struct ETreeMemory {
	ETreeModel base;
	ETreeMemoryPriv *priv;
};

struct ETreeMemoryClass {
	ETreeModelClass parent_class;

	/* signals */
	void (*fill_in_children) (ETreeMemory *model, ETreePath node);
};

GType        e_tree_memory_get_type               (void);
void         e_tree_memory_construct              (ETreeMemory             *etree);
ETreeMemory *e_tree_memory_new                    (void);

/* node operations */
ETreePath    e_tree_memory_node_insert            (ETreeMemory             *etree,
						   ETreePath                parent,
						   gint                      position,
						   gpointer                 node_data);
ETreePath    e_tree_memory_node_insert_id         (ETreeMemory             *etree,
						   ETreePath                parent,
						   gint                      position,
						   gpointer                 node_data,
						   gchar                    *id);
ETreePath    e_tree_memory_node_insert_before     (ETreeMemory             *etree,
						   ETreePath                parent,
						   ETreePath                sibling,
						   gpointer                 node_data);
gpointer     e_tree_memory_node_remove            (ETreeMemory             *etree,
						   ETreePath                path);

/* Freeze and thaw */
void         e_tree_memory_freeze                 (ETreeMemory             *etree);
void         e_tree_memory_thaw                   (ETreeMemory             *etree);
void         e_tree_memory_set_expanded_default   (ETreeMemory             *etree,
						   gboolean                 expanded);
gpointer     e_tree_memory_node_get_data          (ETreeMemory             *etm,
						   ETreePath                node);
void         e_tree_memory_node_set_data          (ETreeMemory             *etm,
						   ETreePath                node,
						   gpointer                 node_data);
void         e_tree_memory_sort_node              (ETreeMemory             *etm,
						   ETreePath                node,
						   ETreeMemorySortCallback  callback,
						   gpointer                 user_data);
void         e_tree_memory_set_node_destroy_func  (ETreeMemory             *etmm,
						   GFunc                    destroy_func,
						   gpointer                 user_data);

G_END_DECLS

#endif /* _E_TREE_MEMORY_H */


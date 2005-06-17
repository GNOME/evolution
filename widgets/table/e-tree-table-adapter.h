/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-table-adapter.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Chris Toshok <toshok@ximian.com>
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

#ifndef _E_TREE_TABLE_ADAPTER_H_
#define _E_TREE_TABLE_ADAPTER_H_

#include <glib-object.h>
#include <table/e-table-model.h>
#include <table/e-tree-model.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-header.h>

G_BEGIN_DECLS

#define E_TREE_TABLE_ADAPTER_TYPE        (e_tree_table_adapter_get_type ())
#define E_TREE_TABLE_ADAPTER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TREE_TABLE_ADAPTER_TYPE, ETreeTableAdapter))
#define E_TREE_TABLE_ADAPTER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TREE_TABLE_ADAPTER_TYPE, ETreeTableAdapterClass))
#define E_IS_TREE_TABLE_ADAPTER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TREE_TABLE_ADAPTER_TYPE))
#define E_IS_TREE_TABLE_ADAPTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TREE_TABLE_ADAPTER_TYPE))
#define E_TREE_TABLE_ADAPTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TREE_TABLE_ADAPTER_TYPE, ETreeTableAdapterClass))

typedef struct ETreeTableAdapterPriv ETreeTableAdapterPriv;

typedef struct {
	ETableModel  base;

	ETreeTableAdapterPriv *priv;
} ETreeTableAdapter;

typedef struct {
	ETableModelClass parent_class;
} ETreeTableAdapterClass;

GType        e_tree_table_adapter_get_type                   (void);
ETableModel *e_tree_table_adapter_new                        (ETreeModel        *source,
							      ETableSortInfo    *sort_info,
							      ETableHeader	*header);
ETableModel *e_tree_table_adapter_construct                  (ETreeTableAdapter *ets,
							      ETreeModel        *source,
		      					      ETableSortInfo    *sort_info,
							      ETableHeader	*header);

ETreePath    e_tree_table_adapter_node_get_next              (ETreeTableAdapter *etta,
							      ETreePath          path);
gboolean     e_tree_table_adapter_node_is_expanded           (ETreeTableAdapter *etta,
							      ETreePath          path);
void         e_tree_table_adapter_node_set_expanded          (ETreeTableAdapter *etta,
							      ETreePath          path,
							      gboolean           expanded);
void         e_tree_table_adapter_node_set_expanded_recurse  (ETreeTableAdapter *etta,
							      ETreePath          path,
							      gboolean           expanded);
void         e_tree_table_adapter_root_node_set_visible      (ETreeTableAdapter *etta,
							      gboolean           visible);
ETreePath    e_tree_table_adapter_node_at_row                (ETreeTableAdapter *etta,
							      int                row);
int          e_tree_table_adapter_row_of_node                (ETreeTableAdapter *etta,
							      ETreePath          path);
gboolean     e_tree_table_adapter_root_node_is_visible       (ETreeTableAdapter *etta);

void         e_tree_table_adapter_show_node                  (ETreeTableAdapter *etta,
							      ETreePath          path);

void         e_tree_table_adapter_save_expanded_state        (ETreeTableAdapter *etta,
							      const char        *filename);
void         e_tree_table_adapter_load_expanded_state        (ETreeTableAdapter *etta,
							      const char        *filename);

void         e_tree_table_adapter_set_sort_info              (ETreeTableAdapter *etta,
							      ETableSortInfo    *sort_info);

G_END_DECLS

#endif /* _E_TREE_TABLE_ADAPTER_H_ */

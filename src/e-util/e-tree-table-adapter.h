/*
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

#ifndef E_TREE_TABLE_ADAPTER_H
#define E_TREE_TABLE_ADAPTER_H

#include <libxml/tree.h>

#include <e-util/e-table-header.h>
#include <e-util/e-table-model.h>
#include <e-util/e-table-sort-info.h>
#include <e-util/e-tree-model.h>

/* Standard GObject macros */
#define E_TYPE_TREE_TABLE_ADAPTER \
	(e_tree_table_adapter_get_type ())
#define E_TREE_TABLE_ADAPTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TREE_TABLE_ADAPTER, ETreeTableAdapter))
#define E_TREE_TABLE_ADAPTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TREE_TABLE_ADAPTER, ETreeTableAdapterClass))
#define E_IS_TREE_TABLE_ADAPTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TREE_TABLE_ADAPTER))
#define E_IS_TREE_TABLE_ADAPTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TREE_TABLE_ADAPTER))
#define E_TREE_TABLE_ADAPTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TREE_TABLE_ADAPTER, ETreeTableAdapterClass))

G_BEGIN_DECLS

typedef struct _ETreeTableAdapter ETreeTableAdapter;
typedef struct _ETreeTableAdapterClass ETreeTableAdapterClass;
typedef struct _ETreeTableAdapterPrivate ETreeTableAdapterPrivate;

struct _ETreeTableAdapter {
	GObject parent;
	ETreeTableAdapterPrivate *priv;
};

struct _ETreeTableAdapterClass {
	GObjectClass parent_class;

	/* Signals */
	gboolean	(*sorting_changed)	(ETreeTableAdapter *etta);
};

GType		e_tree_table_adapter_get_type	(void) G_GNUC_CONST;
ETableModel *	e_tree_table_adapter_new	(ETreeModel *source_model,
						 ETableSortInfo *sort_info,
						 ETableHeader *header);
ETableHeader *	e_tree_table_adapter_get_header	(ETreeTableAdapter *etta);
ETableSortInfo *e_tree_table_adapter_get_sort_info
						(ETreeTableAdapter *etta);
void		e_tree_table_adapter_set_sort_info
						(ETreeTableAdapter *etta,
						 ETableSortInfo *sort_info);
gboolean	e_tree_table_adapter_get_sort_children_ascending
						(ETreeTableAdapter *etta);
void		e_tree_table_adapter_set_sort_children_ascending
						(ETreeTableAdapter *etta,
						 gboolean sort_children_ascending);
ETreeModel *	e_tree_table_adapter_get_source_model
						(ETreeTableAdapter *etta);

ETreePath	e_tree_table_adapter_node_get_next
						(ETreeTableAdapter *etta,
						 ETreePath path);
gboolean	e_tree_table_adapter_node_is_expanded
						(ETreeTableAdapter *etta,
						 ETreePath path);
void		e_tree_table_adapter_node_set_expanded
						(ETreeTableAdapter *etta,
						 ETreePath path,
						 gboolean expanded);
void		e_tree_table_adapter_node_set_expanded_recurse
						(ETreeTableAdapter *etta,
						 ETreePath path,
						 gboolean expanded);
void		e_tree_table_adapter_force_expanded_state
						(ETreeTableAdapter *etta,
						 gint state);
void		e_tree_table_adapter_root_node_set_visible
						(ETreeTableAdapter *etta,
						 gboolean visible);
ETreePath	e_tree_table_adapter_node_at_row
						(ETreeTableAdapter *etta,
						 gint row);
gint		e_tree_table_adapter_row_of_node
						(ETreeTableAdapter *etta,
						 ETreePath path);
gboolean	e_tree_table_adapter_root_node_is_visible
						(ETreeTableAdapter *etta);

void		e_tree_table_adapter_show_node	(ETreeTableAdapter *etta,
						 ETreePath path);

void		e_tree_table_adapter_save_expanded_state
						(ETreeTableAdapter *etta,
						 const gchar *filename);
void		e_tree_table_adapter_load_expanded_state
						(ETreeTableAdapter *etta,
						 const gchar *filename);

xmlDoc *	e_tree_table_adapter_save_expanded_state_xml
						(ETreeTableAdapter *etta);
void		e_tree_table_adapter_load_expanded_state_xml
						(ETreeTableAdapter *etta,
						 xmlDoc *doc);
void		e_tree_table_adapter_clear_nodes_silent
						(ETreeTableAdapter *etta);

G_END_DECLS

#endif /* E_TREE_TABLE_ADAPTER_H */

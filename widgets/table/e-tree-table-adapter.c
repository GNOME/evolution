/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-table-adapter.c
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include <tree.h>
#include <parser.h>
#include "gal/util/e-util.h"
#include "gal/util/e-xml-utils.h"
#include "e-tree-table-adapter.h"

#define PARENT_TYPE E_TABLE_MODEL_TYPE
#define d(x)

#define INCREMENT_AMOUNT 100

static ETableModelClass *parent_class;


struct ETreeTableAdapterPriv {
	ETreeModel  *source;
	int    	     n_map;
	int    	     n_vals_allocated;
	ETreePath   *map_table;
	GHashTable  *attributes;

	guint        root_visible : 1;

	int          last_access;

	int          tree_model_pre_change_id;
	int          tree_model_no_change_id;
	int          tree_model_node_changed_id;
	int          tree_model_node_data_changed_id;
	int          tree_model_node_col_changed_id;
	int          tree_model_node_inserted_id;
	int          tree_model_node_removed_id;
};

typedef struct ETreeTableAdapterNode {
	guint expanded : 1;
	guint expandable : 1;
	guint expandable_set : 1;

	/* parent/child/sibling pointers */
	guint32                num_visible_children;
} ETreeTableAdapterNode;

static ETreeTableAdapterNode *
find_node(ETreeTableAdapter *adapter, ETreePath path)
{
	ETreeTableAdapterNode *node;

	if (path == NULL)
		return NULL;

	if (e_tree_model_has_save_id(adapter->priv->source)) {
		char *save_id;
		save_id = e_tree_model_get_save_id(adapter->priv->source, path);
		node = g_hash_table_lookup(adapter->priv->attributes, save_id);
		g_free(save_id);
	} else {
		node = g_hash_table_lookup(adapter->priv->attributes, path);
	}
	if (node && !node->expandable_set) {
		node->expandable = e_tree_model_node_is_expandable(adapter->priv->source, path);
		node->expandable_set = 1;
	}

	return node;
}

static ETreeTableAdapterNode *
find_or_create_node(ETreeTableAdapter *etta, ETreePath path)
{
	ETreeTableAdapterNode *node;

	node = find_node(etta, path);

	if (!node) {
		node = g_new(ETreeTableAdapterNode, 1);
		if (e_tree_model_node_is_root(etta->priv->source, path))
			node->expanded = TRUE;
		else
			node->expanded = e_tree_model_get_expanded_default(etta->priv->source);
		node->expandable = e_tree_model_node_is_expandable(etta->priv->source, path);
		node->expandable_set = 1;
		node->num_visible_children = 0;

		if (e_tree_model_has_save_id(etta->priv->source)) {
			char *save_id;
			save_id = e_tree_model_get_save_id(etta->priv->source, path);
			g_hash_table_insert(etta->priv->attributes, save_id, node);
		} else {
			g_hash_table_insert(etta->priv->attributes, path, node);
		}
	}

	return node;
}

static void
add_expanded_node(ETreeTableAdapter *etta, char *save_id, gboolean expanded)
{
	ETreeTableAdapterNode *node;

	node = g_hash_table_lookup(etta->priv->attributes, save_id);

	if (node) {
		node->expandable_set = 0;
		node->expanded = expanded;
		return;
	}

	node = g_new(ETreeTableAdapterNode, 1);

	node->expanded = expanded;
	node->expandable = 0;
	node->expandable_set = 0;
	node->num_visible_children = 0;

	g_hash_table_insert(etta->priv->attributes, save_id, node);
}

static void
etta_expand_to(ETreeTableAdapter *etta, int size)
{
	if (size > etta->priv->n_vals_allocated) {
		etta->priv->n_vals_allocated = MAX(etta->priv->n_vals_allocated + INCREMENT_AMOUNT, size);
		etta->priv->map_table = g_renew (ETreePath, etta->priv->map_table, etta->priv->n_vals_allocated);
	}

}

static void
etta_update_parent_child_counts(ETreeTableAdapter *etta, ETreePath path, int change)
{
	for (path = e_tree_model_node_get_parent(etta->priv->source, path);
	     path;
	     path = e_tree_model_node_get_parent(etta->priv->source, path)) {
		ETreeTableAdapterNode *node = find_or_create_node(etta, path);
		node->num_visible_children += change;
	}
	etta->priv->n_map += change;
}

static int
find_next_node_maybe_deleted(ETreeTableAdapter *adapter, int row)
{
	ETreePath path = adapter->priv->map_table[row];
	if (path) {
		ETreeTableAdapterNode *current = find_node (adapter, path);

		row += (current ? current->num_visible_children : 0) + 1;
		if (row >= adapter->priv->n_map)
			return -1;
		return row;
	} else
		return -1;
}

static int
find_first_child_node_maybe_deleted(ETreeTableAdapter *adapter, int row)
{
	if (row != -1) {
		ETreePath path = adapter->priv->map_table[row];
		ETreeTableAdapterNode *current = find_node (adapter, path);
		if (current && current->expanded) {
			row ++;
			if (row >= adapter->priv->n_map)
				return -1;
			return row;
		} else
			return -1;
	} else
		return 0;
}

static int
find_next_node(ETreeTableAdapter *adapter, int row)
{
	ETreePath path = adapter->priv->map_table[row];
	if (path) {
		ETreePath next_sibling = e_tree_model_node_get_next(adapter->priv->source, path);
		ETreeTableAdapterNode *current = find_node (adapter, path);
		if (next_sibling) {
			row += (current ? current->num_visible_children : 0) + 1;
			if (row >= adapter->priv->n_map)
				return -1;
			return row;
		} else
			return -1;
	} else
		return -1;
}

static int
find_first_child_node(ETreeTableAdapter *adapter, int row)
{
	if (row != -1) {
		ETreePath path = adapter->priv->map_table[row];
		ETreePath first_child = e_tree_model_node_get_first_child(adapter->priv->source, path);
		ETreeTableAdapterNode *current = find_node (adapter, path);
		if (first_child && current && current->expanded) {
			row ++;
			if (row >= adapter->priv->n_map)
				return -1;
			return row;
		} else
			return -1;
	} else
		return 0;
}

static int
find_child_row_num_maybe_deleted(ETreeTableAdapter *etta, int row, ETreePath path)
{
	row = find_first_child_node_maybe_deleted(etta, row);

	while (row != -1 && path != etta->priv->map_table[row]) {
		row = find_next_node_maybe_deleted(etta, row);
	}

	return row;
}

static int
find_row_num(ETreeTableAdapter *etta, ETreePath path)
{
	int depth;
	ETreePath *sequence;
	int i;
	int row;

	if (etta->priv->map_table == NULL)
		return -1;
	if (etta->priv->n_map == 0)
		return -1;

	if (path == NULL)
		return -1;

	if (etta->priv->last_access != -1) {
		int end = MIN(etta->priv->n_map, etta->priv->last_access + 10);
		int start = MAX(0, etta->priv->last_access - 10);
		int initial = MAX (MIN (etta->priv->last_access, end), start);
		for (i = initial; i < end; i++) {
			if(etta->priv->map_table[i] == path) {
				d(g_print("Found last access %d at row %d. (find_row_num)\n", etta->priv->last_access, i));
				return i;
			}
		}
		for (i = initial - 1; i >= start; i--) {
			if(etta->priv->map_table[i] == path) {
				d(g_print("Found last access %d at row %d. (find_row_num)\n", etta->priv->last_access, i));
				return i;
			}
		}
	}


	depth = e_tree_model_node_depth(etta->priv->source, path);

	sequence = g_new(ETreePath, depth + 1);

	sequence[0] = path;

	for (i = 0; i < depth; i++) {
		ETreeTableAdapterNode *node;
		
		sequence[i + 1] = e_tree_model_node_get_parent(etta->priv->source, sequence[i]);

		node = find_node(etta, sequence[i + 1]);
		if (! ((node && node->expanded) || e_tree_model_get_expanded_default(etta->priv->source))) {
			g_free(sequence);
			return -1;
		}
	}

	row = 0;

	for (i = depth; i >= 0; i --) {
		while (row != -1 && sequence[i] != etta->priv->map_table[row]) {
			row = find_next_node(etta, row);
		}
		if (row == -1)
			break;
		if (i == 0)
			break;
		row = find_first_child_node(etta, row);
	}
	g_free (sequence);

	d(g_print("Didn't find last access %d. Setting to %d. (find_row_num)\n", etta->priv->last_access, row));
	etta->priv->last_access = row;
	return row;
}

static int
array_size_from_path(ETreeTableAdapter *etta, ETreePath path)
{
	int size = 1;

	ETreeTableAdapterNode *node = NULL;

	if (e_tree_model_node_is_expandable(etta->priv->source, path))
		node = find_or_create_node(etta, path);

	if (node && node->expanded) {
		ETreePath children;

		for (children = e_tree_model_node_get_first_child(etta->priv->source, path); 
		     children; 
		     children = e_tree_model_node_get_next(etta->priv->source, children)) {
			size += array_size_from_path(etta, children);
		}
	}

	return size;
}

static int
fill_array_from_path(ETreeTableAdapter *etta, ETreePath *array, ETreePath path)
{
	ETreeTableAdapterNode *node = NULL;
	int index = 0;

	array[index] = path;

	index ++;

	if (e_tree_model_node_is_expandable(etta->priv->source, path))
		node = find_or_create_node(etta, path);
	else
		node = find_node(etta, path);

	if (node && node->expanded) {
		ETreePath children;

		for (children = e_tree_model_node_get_first_child(etta->priv->source, path); 
		     children; 
		     children = e_tree_model_node_get_next(etta->priv->source, children)) {
			index += fill_array_from_path(etta, array + index, children);
		}
	}

	if (node)
		node->num_visible_children = index - 1;

	return index;
}

static void
free_string (gpointer key, gpointer value, gpointer data)
{
	g_free(key);
}

static void
etta_destroy (GtkObject *object)
{
	ETreeTableAdapter *etta = E_TREE_TABLE_ADAPTER (object);

	if (etta->priv->source && e_tree_model_has_save_id(etta->priv->source)) {
		g_hash_table_foreach(etta->priv->attributes, free_string, NULL);
	}
	g_hash_table_destroy (etta->priv->attributes);

	if (etta->priv->source) {
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_pre_change_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_no_change_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_node_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_node_data_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_node_col_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_node_inserted_id);
		gtk_signal_disconnect (GTK_OBJECT (etta->priv->source),
				       etta->priv->tree_model_node_removed_id);

		gtk_object_unref (GTK_OBJECT (etta->priv->source));
		etta->priv->source = NULL;

		etta->priv->tree_model_pre_change_id = 0;
		etta->priv->tree_model_no_change_id = 0;
		etta->priv->tree_model_node_changed_id = 0;
		etta->priv->tree_model_node_data_changed_id = 0;
		etta->priv->tree_model_node_col_changed_id = 0;
		etta->priv->tree_model_node_inserted_id = 0;
		etta->priv->tree_model_node_removed_id = 0;
	}

	g_free (etta->priv->map_table);

	g_free (etta->priv);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static int
etta_column_count (ETableModel *etm)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_column_count (etta->priv->source);
}

static gboolean
etta_has_save_id (ETableModel *etm)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_has_save_id (etta->priv->source);
}

static gchar *
etta_get_save_id (ETableModel *etm, int row)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	if (etta->priv->root_visible)
		return e_tree_model_get_save_id (etta->priv->source, etta->priv->map_table [row]);
	else
		return e_tree_model_get_save_id (etta->priv->source, etta->priv->map_table [row + 1]);
}

static gboolean
etta_has_change_pending (ETableModel *etm)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_has_change_pending (etta->priv->source);
}


static int
etta_row_count (ETableModel *etm)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	if (etta->priv->root_visible)
		return etta->priv->n_map;
	else {
		if (etta->priv->n_map > 0)
			return etta->priv->n_map - 1;
		else
			return 0;
	}
}

static void *
etta_value_at (ETableModel *etm, int col, int row)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

#if 0
	etta->priv->last_access = row;
	d(g_print("g) Setting last_access to %d\n", row));
#endif

	switch (col) {
	case -1:
		if (etta->priv->root_visible)
			return etta->priv->map_table [row];
		else
			return etta->priv->map_table [row + 1];
	case -2:
		return etta->priv->source;
	case -3:
		return etta;
	default:
		if (etta->priv->root_visible)
			return e_tree_model_value_at (etta->priv->source, etta->priv->map_table [row], col);
		else
			return e_tree_model_value_at (etta->priv->source, etta->priv->map_table [row + 1], col);
	}
}

static void
etta_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	etta->priv->last_access = row;
	d(g_print("h) Setting last_access to %d\n", row));
	if (etta->priv->root_visible)
		e_tree_model_set_value_at (etta->priv->source, etta->priv->map_table [row], col, val);
	else
		e_tree_model_set_value_at (etta->priv->source, etta->priv->map_table [row + 1], col, val);
}

static gboolean
etta_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	if (etta->priv->root_visible)
		return e_tree_model_node_is_editable (etta->priv->source, etta->priv->map_table [row], col);
	else
		return e_tree_model_node_is_editable (etta->priv->source, etta->priv->map_table [row + 1], col);
}

static void
etta_append_row (ETableModel *etm, ETableModel *source, int row)
{
#if 0
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;
	e_table_model_append_row (etta->priv->source, source, row);
#endif
}

static void *
etta_duplicate_value (ETableModel *etm, int col, const void *value)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_duplicate_value (etta->priv->source, col, value);
}

static void
etta_free_value (ETableModel *etm, int col, void *value)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	e_tree_model_free_value (etta->priv->source, col, value);
}

static void *
etta_initialize_value (ETableModel *etm, int col)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_initialize_value (etta->priv->source, col);
}

static gboolean
etta_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_value_is_empty (etta->priv->source, col, value);
}

static char *
etta_value_to_string (ETableModel *etm, int col, const void *value)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_value_to_string (etta->priv->source, col, value);
}

static void
etta_class_init (ETreeTableAdapterClass *klass)
{
	ETableModelClass *table_class   = (ETableModelClass *) klass;
	GtkObjectClass *object_class    = (GtkObjectClass *) klass;

	parent_class                    = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy           = etta_destroy;

	table_class->column_count       = etta_column_count;
	table_class->row_count          = etta_row_count;
	table_class->append_row         = etta_append_row;

	table_class->value_at           = etta_value_at;
	table_class->set_value_at       = etta_set_value_at;
	table_class->is_cell_editable   = etta_is_cell_editable;

	table_class->has_save_id        = etta_has_save_id;
	table_class->get_save_id        = etta_get_save_id;

	table_class->has_change_pending = etta_has_change_pending;
	table_class->duplicate_value    = etta_duplicate_value;
	table_class->free_value         = etta_free_value;
	table_class->initialize_value   = etta_initialize_value;
	table_class->value_is_empty     = etta_value_is_empty;
	table_class->value_to_string    = etta_value_to_string;
}

static void
etta_init (ETreeTableAdapter *etta)
{
	etta->priv                                  = g_new(ETreeTableAdapterPriv, 1);

	etta->priv->source                          = NULL;

	etta->priv->n_map                           = 0;
	etta->priv->n_vals_allocated                = 0;
	etta->priv->map_table                       = NULL;
	etta->priv->attributes                      = NULL;

	etta->priv->root_visible                    = TRUE;

	etta->priv->last_access                     = 0;

	etta->priv->tree_model_pre_change_id        = 0;
	etta->priv->tree_model_no_change_id        = 0;
	etta->priv->tree_model_node_changed_id      = 0;
	etta->priv->tree_model_node_data_changed_id = 0;
	etta->priv->tree_model_node_col_changed_id  = 0;
	etta->priv->tree_model_node_inserted_id     = 0;
	etta->priv->tree_model_node_removed_id      = 0;
}

E_MAKE_TYPE(e_tree_table_adapter, "ETreeTableAdapter", ETreeTableAdapter, etta_class_init, etta_init, PARENT_TYPE);

static void
etta_proxy_pre_change (ETreeModel *etm, ETreeTableAdapter *etta)
{
	e_table_model_pre_change(E_TABLE_MODEL(etta));
}

static void
etta_proxy_no_change (ETreeModel *etm, ETreeTableAdapter *etta)
{
	e_table_model_no_change(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_changed (ETreeModel *etm, ETreePath path, ETreeTableAdapter *etta)
{
	if (e_tree_model_node_is_root(etm, path)) {
		int size;

		size = array_size_from_path(etta, path);
		etta_expand_to(etta, size);
		etta->priv->n_map = size;
		fill_array_from_path(etta, etta->priv->map_table, path);
	} else {
		int row = find_row_num(etta, path);
		int size;
		int old_size;
		ETreeTableAdapterNode *node;

		if (row == -1)
			return;

		size = array_size_from_path(etta, path);

		node = find_node(etta, path);
		if (node)
			old_size = node->num_visible_children + 1;
		else
			old_size = 1;

		etta_expand_to(etta, etta->priv->n_map + size - old_size);

		memmove(etta->priv->map_table + row + size,
			etta->priv->map_table + row + old_size,
			(etta->priv->n_map - row - old_size) * sizeof (ETreePath));
		fill_array_from_path(etta, etta->priv->map_table + row, path);
		etta_update_parent_child_counts(etta, path, size - old_size);
	}

	e_table_model_changed(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_data_changed (ETreeModel *etm, ETreePath path, ETreeTableAdapter *etta)
{
	int row = find_row_num(etta, path);
	if (row != -1) {
		if (etta->priv->root_visible)
			e_table_model_row_changed(E_TABLE_MODEL(etta), row);
		else if (row != 0)
			e_table_model_row_changed(E_TABLE_MODEL(etta), row - 1);
		else
			e_table_model_no_change(E_TABLE_MODEL(etta));
	} else
		e_table_model_no_change(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_col_changed (ETreeModel *etm, ETreePath path, int col, ETreeTableAdapter *etta)
{
	int row = find_row_num(etta, path);
	if (row != -1) {
		if (etta->priv->root_visible)
			e_table_model_cell_changed(E_TABLE_MODEL(etta), col, row);
		else if (row != 0)
			e_table_model_cell_changed(E_TABLE_MODEL(etta), col, row - 1);
		else
			e_table_model_no_change(E_TABLE_MODEL(etta));
	} else
		e_table_model_no_change(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_inserted (ETreeModel *etm, ETreePath parent, ETreePath child, ETreeTableAdapter *etta)
{
	int row;

	if (e_tree_model_node_is_root(etm, child)) {
		row = 0;
	} else {
		ETreePath children;
		int parent_row;
		ETreeTableAdapterNode *parent_node;

		parent_row = find_row_num(etta, parent);
		if (parent_row == -1) {
			e_table_model_no_change(E_TABLE_MODEL(etta));
			return;
		}

		parent_node = find_or_create_node(etta, parent);
		if (parent_node->expandable != e_tree_model_node_is_expandable(etta->priv->source, parent)) {
			e_table_model_pre_change(E_TABLE_MODEL(etta));
			parent_node->expandable = e_tree_model_node_is_expandable(etta->priv->source, parent);
			if (etta->priv->root_visible)
				e_table_model_row_changed(E_TABLE_MODEL(etta), parent_row);
			else if (parent_row != 0)
				e_table_model_row_changed(E_TABLE_MODEL(etta), parent_row - 1);
			else
				e_table_model_no_change(E_TABLE_MODEL(etta));
		}
		if (!parent_node->expanded) {
			e_table_model_no_change(E_TABLE_MODEL(etta));
			return;
		}

		row = find_first_child_node(etta, parent_row);
		children = e_tree_model_node_get_first_child(etta->priv->source, parent);

		while (row != -1 &&
		       row <= parent_row + parent_node->num_visible_children &&
		       children != NULL &&
		       children == etta->priv->map_table[row]) {
			children = e_tree_model_node_get_next(etta->priv->source, children);
			row = find_next_node(etta, row);
		}
	}

	if (row != -1) {
		int size;

		size = array_size_from_path(etta, child);

		etta_expand_to(etta, etta->priv->n_map + size);

		memmove(etta->priv->map_table + row + size,
			etta->priv->map_table + row,
			(etta->priv->n_map - row) * sizeof (ETreePath));

		fill_array_from_path(etta, etta->priv->map_table + row, child);
		etta_update_parent_child_counts(etta, child, size);

		if (etta->priv->root_visible)
			e_table_model_rows_inserted(E_TABLE_MODEL(etta), row, size);
		else if (row != 0)
			e_table_model_rows_inserted(E_TABLE_MODEL(etta), row - 1, size);
		else
			e_table_model_rows_inserted(E_TABLE_MODEL(etta), 0, size - 1);
	} else
		e_table_model_no_change(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_removed (ETableModel *etm, ETreePath parent, ETreePath child, int old_position, ETreeTableAdapter *etta)
{
	int parent_row = find_row_num(etta, parent);
	int row = find_child_row_num_maybe_deleted(etta, parent_row, child);
	ETreeTableAdapterNode *parent_node = find_node(etta, parent);
	if (parent_row != -1 && parent_node) {
		if (parent_node->expandable != e_tree_model_node_is_expandable(etta->priv->source, parent)) {
			e_table_model_pre_change(E_TABLE_MODEL(etta));
			parent_node->expandable = e_tree_model_node_is_expandable(etta->priv->source, parent);
			if (etta->priv->root_visible)
				e_table_model_row_changed(E_TABLE_MODEL(etta), parent_row);
			else if (parent_row != 0)
				e_table_model_row_changed(E_TABLE_MODEL(etta), parent_row - 1);
			else
				e_table_model_no_change(E_TABLE_MODEL(etta));
		}
	}

	if (row != -1) {
		ETreeTableAdapterNode *node = find_node(etta, child);
		int to_remove = (node ? node->num_visible_children : 0) + 1;

		memmove(etta->priv->map_table + row,
			etta->priv->map_table + row + to_remove,
			(etta->priv->n_map - row - to_remove) * sizeof (ETreePath));

		if (parent_node)
			parent_node->num_visible_children -= to_remove;
		if (parent)
			etta_update_parent_child_counts(etta, parent, - to_remove);

		if (etta->priv->root_visible)
			e_table_model_rows_deleted(E_TABLE_MODEL(etta), row, to_remove);
		else if (row != 0)
			e_table_model_rows_deleted(E_TABLE_MODEL(etta), row - 1, to_remove);
		else
			e_table_model_rows_deleted(E_TABLE_MODEL(etta), 0, to_remove - 1);
	} else
		e_table_model_no_change(E_TABLE_MODEL(etta));
}

ETableModel *
e_tree_table_adapter_construct (ETreeTableAdapter *etta, ETreeModel *source)
{
	ETreePath root;

	etta->priv->source = source;
	gtk_object_ref (GTK_OBJECT (source));

	if (e_tree_model_has_save_id(source))
		etta->priv->attributes = g_hash_table_new(g_str_hash, g_str_equal);
	else
		etta->priv->attributes = g_hash_table_new(NULL, NULL);

	root = e_tree_model_get_root (source);

	if (root) {
		etta->priv->n_map = array_size_from_path(etta, root);
		etta->priv->n_vals_allocated = etta->priv->n_map;
		etta->priv->map_table = g_new(ETreePath, etta->priv->n_map);
		fill_array_from_path(etta, etta->priv->map_table, root);
	}

	etta->priv->tree_model_pre_change_id = gtk_signal_connect (GTK_OBJECT (source), "pre_change",
								   GTK_SIGNAL_FUNC (etta_proxy_pre_change), etta);
	etta->priv->tree_model_no_change_id = gtk_signal_connect (GTK_OBJECT (source), "no_change",
								   GTK_SIGNAL_FUNC (etta_proxy_no_change), etta);
	etta->priv->tree_model_node_changed_id = gtk_signal_connect (GTK_OBJECT (source), "node_changed",
								     GTK_SIGNAL_FUNC (etta_proxy_node_changed), etta);
	etta->priv->tree_model_node_data_changed_id = gtk_signal_connect (GTK_OBJECT (source), "node_data_changed",
									  GTK_SIGNAL_FUNC (etta_proxy_node_data_changed), etta);
	etta->priv->tree_model_node_col_changed_id = gtk_signal_connect (GTK_OBJECT (source), "node_col_changed",
									 GTK_SIGNAL_FUNC (etta_proxy_node_col_changed), etta);
	etta->priv->tree_model_node_inserted_id = gtk_signal_connect (GTK_OBJECT (source), "node_inserted",
								      GTK_SIGNAL_FUNC (etta_proxy_node_inserted), etta);
	etta->priv->tree_model_node_removed_id = gtk_signal_connect (GTK_OBJECT (source), "node_removed",
								     GTK_SIGNAL_FUNC (etta_proxy_node_removed), etta);

	return E_TABLE_MODEL (etta);
}

ETableModel *
e_tree_table_adapter_new (ETreeModel *source)
{
	ETreeTableAdapter *etta = gtk_type_new (E_TREE_TABLE_ADAPTER_TYPE);

	e_tree_table_adapter_construct (etta, source);

	return (ETableModel *) etta;
}

typedef struct {
	xmlNode *root;
	ETreeModel *tree;
} TreeAndRoot;

static void
save_expanded_state_func (gpointer keyp, gpointer value, gpointer data)
{
	gchar *key = keyp;
	ETreeTableAdapterNode *node = value;
	TreeAndRoot *tar = data;
	xmlNode *root = tar->root;
	ETreeModel *etm = tar->tree;
	xmlNode *xmlnode;

	if (node->expanded != e_tree_model_get_expanded_default(etm)) {
		xmlnode = xmlNewChild (root, NULL, "node", NULL);
		e_xml_set_string_prop_by_name(xmlnode, "id", key);
	}
}

void
e_tree_table_adapter_save_expanded_state (ETreeTableAdapter *etta, const char *filename)
{
	xmlDoc *doc;
	xmlNode *root;
	ETreeTableAdapterPriv *priv;
	TreeAndRoot tar;

	g_return_if_fail(etta != NULL);

	priv = etta->priv; 

	doc = xmlNewDoc ((xmlChar*) "1.0");
	root = xmlNewDocNode (doc, NULL,
			      (xmlChar *) "expanded_state",
			      NULL);
	xmlDocSetRootElement (doc, root);

	e_xml_set_integer_prop_by_name(root, "vers", 1);

	tar.root = root;
	tar.tree = etta->priv->source;

	g_hash_table_foreach (priv->attributes,
			      save_expanded_state_func,
			      &tar);

	xmlSaveFile (filename, doc);

	xmlFreeDoc (doc);
}

void
e_tree_table_adapter_load_expanded_state (ETreeTableAdapter *etta, const char *filename)
{
	ETreeTableAdapterPriv *priv;
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *child;
	int vers;

	g_return_if_fail(etta != NULL);

	priv = etta->priv;

	doc = xmlParseFile (filename);
	if (!doc)
		return;

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "expanded_state")) {
		xmlFreeDoc (doc);
		return;
	}

	vers = e_xml_get_integer_prop_by_name_with_default(root, "vers", 0);
	if (vers != 1) {
		xmlFreeDoc (doc);
		return;
	}

	for (child = root->xmlChildrenNode; child; child = child->next) {
		char *id;

		if (strcmp (child->name, "node")) {
			d(g_warning ("unknown node '%s' in %s", child->name, filename));
			continue;
		}

		id = e_xml_get_string_prop_by_name_with_default (child, "id", "");

		if (!strcmp(id, "")) {
			g_free(id);
			return;
		}

		add_expanded_node(etta, id, !e_tree_model_get_expanded_default(etta->priv->source));
	}

	xmlFreeDoc (doc);
}

void         e_tree_table_adapter_root_node_set_visible (ETreeTableAdapter *etta, gboolean visible)
{
	if (etta->priv->root_visible == visible)
		return;

	e_table_model_pre_change (E_TABLE_MODEL(etta));

	etta->priv->root_visible = visible;
	if (!visible) {
		ETreePath root = e_tree_model_get_root(etta->priv->source);
		if (root)
			e_tree_table_adapter_node_set_expanded(etta, root, TRUE);
	}
	e_table_model_changed(E_TABLE_MODEL(etta));
}

void         e_tree_table_adapter_node_set_expanded (ETreeTableAdapter *etta, ETreePath path, gboolean expanded)
{
	ETreeTableAdapterNode *node;
	int row;

	if (e_tree_model_node_is_root (etta->priv->source, path) && !etta->priv->root_visible)
		return;

	node = find_or_create_node(etta, path);

	if (expanded != node->expanded) {
		node->expanded = expanded;
		
		row = find_row_num(etta, path);
		if (row != -1) {
			e_table_model_pre_change (E_TABLE_MODEL(etta));

			if (etta->priv->root_visible) {
				e_table_model_pre_change (E_TABLE_MODEL(etta));
				e_table_model_row_changed(E_TABLE_MODEL(etta), row);
			} else if (row != 0) {
				e_table_model_pre_change (E_TABLE_MODEL(etta));
				e_table_model_row_changed(E_TABLE_MODEL(etta), row - 1);
			}

			if (expanded) {
				int num_children = array_size_from_path(etta, path) - 1;
				etta_expand_to(etta, etta->priv->n_map + num_children);
				memmove(etta->priv->map_table + row + 1 + num_children,
					etta->priv->map_table + row + 1,
					(etta->priv->n_map - row - 1) * sizeof (ETreePath));
				fill_array_from_path(etta, etta->priv->map_table + row, path);
				etta_update_parent_child_counts(etta, path, num_children);
				if (num_children != 0) {
					if (etta->priv->root_visible)
						e_table_model_rows_inserted(E_TABLE_MODEL(etta), row + 1, num_children);
					else
						e_table_model_rows_inserted(E_TABLE_MODEL(etta), row, num_children);
				} else
					e_table_model_no_change(E_TABLE_MODEL(etta));
			} else {
				int num_children = node->num_visible_children;
				g_assert (etta->priv->n_map >= row + 1 + num_children);
				memmove(etta->priv->map_table + row + 1,
					etta->priv->map_table + row + 1 + num_children,
					(etta->priv->n_map - row - 1 - num_children) * sizeof (ETreePath));
				node->num_visible_children = 0;
				etta_update_parent_child_counts(etta, path, - num_children);
				if (num_children != 0) {
					if (etta->priv->root_visible)
						e_table_model_rows_deleted(E_TABLE_MODEL(etta), row + 1, num_children);
					else
						e_table_model_rows_deleted(E_TABLE_MODEL(etta), row, num_children);
				} else
					e_table_model_no_change(E_TABLE_MODEL(etta));
			}
		}
	}
}

void         e_tree_table_adapter_node_set_expanded_recurse (ETreeTableAdapter *etta, ETreePath path, gboolean expanded)
{
	ETreePath children;

	e_tree_table_adapter_node_set_expanded(etta, path, expanded);

	for (children = e_tree_model_node_get_first_child(etta->priv->source, path); 
	     children; 
	     children = e_tree_model_node_get_next(etta->priv->source, children)) {
		e_tree_table_adapter_node_set_expanded_recurse(etta, children, expanded);
	}
}

ETreePath    e_tree_table_adapter_node_at_row (ETreeTableAdapter *etta, int row)
{
	if (row < 0)
		return NULL;
	if (etta->priv->root_visible) {
		if (row < etta->priv->n_map)
			return etta->priv->map_table[row];
	} else {
		if (row + 1 < etta->priv->n_map)
			return etta->priv->map_table[row + 1];
	}
	return NULL;
}

int    e_tree_table_adapter_row_of_node (ETreeTableAdapter *etta, ETreePath path)
{
	if (etta->priv->root_visible)
		return find_row_num(etta, path);
	else {
		int row_num = find_row_num (etta, path);
		if (row_num != -1)
			return row_num - 1;
		else
			return row_num;
	}
}

gboolean     e_tree_table_adapter_root_node_is_visible(ETreeTableAdapter *etta)
{
	return etta->priv->root_visible;
}

void         e_tree_table_adapter_show_node (ETreeTableAdapter *etta, ETreePath path)
{
	ETreePath parent;

	parent = e_tree_model_node_get_parent(etta->priv->source, path);

	if (parent) {
		e_tree_table_adapter_node_set_expanded(etta, parent, TRUE);
		e_tree_table_adapter_show_node(etta, parent);
	}
}

gboolean     e_tree_table_adapter_node_is_expanded (ETreeTableAdapter *etta, ETreePath path)
{
	if (e_tree_model_node_is_expandable(etta->priv->source, path)) {
		ETreeTableAdapterNode *node = find_or_create_node(etta, path);
		return node->expanded;
	} else
		return FALSE;
}

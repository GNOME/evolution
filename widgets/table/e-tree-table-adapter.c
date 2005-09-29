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

#include <glib.h>
#include <glib/gstdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"

#include "e-table-sorting-utils.h"
#include "e-tree-table-adapter.h"

#define PARENT_TYPE E_TABLE_MODEL_TYPE
#define d(x)

#define INCREMENT_AMOUNT 100

static ETableModelClass *parent_class;

typedef struct {
	ETreePath path;
	guint32 num_visible_children;
	guint32 index;

	guint expanded : 1;
	guint expandable : 1;
	guint expandable_set : 1;
} node_t;

struct ETreeTableAdapterPriv {
	ETreeModel     *source;
	ETableSortInfo *sort_info;
	ETableHeader   *header;

	int    	     n_map;
	int    	     n_vals_allocated;
	node_t     **map_table;
	GHashTable  *nodes;
	GNode       *root;

	guint        root_visible : 1;
	guint        remap_needed : 1;

	int          last_access;

	int          pre_change_id;
	int          no_change_id;
	int          node_changed_id;
	int          node_data_changed_id;
	int          node_col_changed_id;
	int          node_inserted_id;
	int          node_removed_id;
	int          node_request_collapse_id;
	int          sort_info_changed_id;
};

static GNode *
lookup_gnode(ETreeTableAdapter *etta, ETreePath path)
{
	GNode *gnode;

	if (!path)
		return NULL;

	gnode = g_hash_table_lookup(etta->priv->nodes, path);

	return gnode;
}

static void
resize_map(ETreeTableAdapter *etta, int size)
{
        if (size > etta->priv->n_vals_allocated) {
                etta->priv->n_vals_allocated = MAX(etta->priv->n_vals_allocated + INCREMENT_AMOUNT, size);
                etta->priv->map_table = g_renew (node_t *, etta->priv->map_table, etta->priv->n_vals_allocated);
        }

	etta->priv->n_map = size;
}

static void
move_map_elements(ETreeTableAdapter *etta, int to, int from, int count)
{
	if (count <= 0 || from >= etta->priv->n_map)
		return;
	memmove(etta->priv->map_table + to, etta->priv->map_table + from, count * sizeof (node_t *));
	etta->priv->remap_needed = TRUE;
}

static gint
fill_map(ETreeTableAdapter *etta, gint index, GNode *gnode)
{
	GNode *p;

	if ((gnode != etta->priv->root) || etta->priv->root_visible)
		etta->priv->map_table[index++] = gnode->data;

	for (p = gnode->children; p; p = p->next)
		index = fill_map(etta, index, p);

	etta->priv->remap_needed = TRUE;
	return index;
}

static void
remap_indices(ETreeTableAdapter *etta)
{
	int i;
	for (i = 0; i < etta->priv->n_map; i++)
		etta->priv->map_table[i]->index = i;
	etta->priv->remap_needed = FALSE;
}

static node_t *
get_node(ETreeTableAdapter *etta, ETreePath path)
{
	GNode *gnode = lookup_gnode(etta, path);

	if (!gnode)
		return NULL;

	return (node_t *)gnode->data;
}

static void
resort_node(ETreeTableAdapter *etta, GNode *gnode, gboolean recurse)
{
	node_t *node = (node_t *)gnode->data;
	ETreePath *paths, path;
	GNode *prev, *curr;
	int i, count;
	gboolean sort_needed;

	if (node->num_visible_children == 0)
		return;

	sort_needed = etta->priv->sort_info && e_table_sort_info_sorting_get_count (etta->priv->sort_info) > 0;

	for (i = 0, path = e_tree_model_node_get_first_child(etta->priv->source, node->path); path; 
	     path = e_tree_model_node_get_next(etta->priv->source, path), i++); 

	count = i;
	if (count <= 1)
		return;

	paths = g_new0(ETreePath, count);

	for (i = 0, path = e_tree_model_node_get_first_child(etta->priv->source, node->path); path; 
	     path = e_tree_model_node_get_next(etta->priv->source, path), i++)
		paths[i] = path;

	if (count > 1 && sort_needed)
		e_table_sorting_utils_tree_sort(etta->priv->source, etta->priv->sort_info, etta->priv->header, paths, count);

	prev = NULL;
	for (i = 0; i < count; i++) {
		curr = lookup_gnode(etta, paths[i]);
		if (!curr)
			continue;

		if (prev)
			prev->next = curr;
		else
			gnode->children = curr;

		curr->prev = prev;
		curr->next = NULL;
		prev = curr;
		if (recurse)
			resort_node(etta, curr, recurse);
	}

	g_free(paths);
}

static gint
get_row(ETreeTableAdapter *etta, ETreePath path)
{
	node_t *node = get_node(etta, path);
	if (!node)
		return -1;

	if (etta->priv->remap_needed)
		remap_indices(etta);

	return node->index;
}

static ETreePath
get_path (ETreeTableAdapter *etta, int row)
{
	if (row == -1 && etta->priv->n_map > 0)
		row = etta->priv->n_map - 1;
	else if (row < 0 || row >= etta->priv->n_map)
		return NULL;

	return etta->priv->map_table [row]->path;
}

static void
kill_gnode(GNode *node, ETreeTableAdapter *etta)
{
	g_hash_table_remove(etta->priv->nodes, ((node_t *)node->data)->path);

	while (node->children) {
		GNode *next = node->children->next;
		kill_gnode(node->children, etta);
		node->children = next;
	}

	g_free(node->data);
	if (node == etta->priv->root)
		etta->priv->root = NULL;
	g_node_destroy(node);
}

static void
update_child_counts(GNode *gnode, gint delta)
{
	while (gnode) {
		node_t *node = (node_t *) gnode->data;
		node->num_visible_children += delta;
		gnode = gnode->parent;
	}
}

static int
delete_children(ETreeTableAdapter *etta, GNode *gnode)
{
	node_t *node = (node_t *)gnode->data;
	int to_remove = node ? node->num_visible_children : 0;

	if (to_remove == 0)
		return 0;

	while (gnode->children) {
		GNode *next = gnode->children->next;
		kill_gnode(gnode->children, etta);
		gnode->children = next;
	}

	return to_remove;
}

static void
delete_node(ETreeTableAdapter *etta, ETreePath parent, ETreePath path)
{
	int to_remove = 1;
	int parent_row = get_row(etta, parent);
	int row = get_row(etta, path);
	GNode *gnode = lookup_gnode(etta, path);
	GNode *parent_gnode = lookup_gnode(etta, parent);

	e_table_model_pre_change(E_TABLE_MODEL(etta));

	if (row == -1) {
		e_table_model_no_change(E_TABLE_MODEL(etta));
		return;
	}

	to_remove += delete_children(etta, gnode);
	kill_gnode(gnode, etta);

	move_map_elements(etta, row, row + to_remove, etta->priv->n_map - row - to_remove);
	resize_map(etta, etta->priv->n_map - to_remove);

	if (parent_gnode != NULL) {
		node_t *parent_node = parent_gnode->data;
		gboolean expandable = e_tree_model_node_is_expandable(etta->priv->source, parent);

		update_child_counts(parent_gnode, - to_remove);
		if (parent_node->expandable != expandable) {
			e_table_model_pre_change(E_TABLE_MODEL(etta));
			parent_node->expandable = expandable;
			e_table_model_row_changed(E_TABLE_MODEL(etta), parent_row);
		}

		resort_node (etta, parent_gnode, FALSE);
	}

	e_table_model_rows_deleted(E_TABLE_MODEL(etta), row, to_remove);
}

static GNode *
create_gnode(ETreeTableAdapter *etta, ETreePath path)
{
	GNode *gnode;
	node_t *node;

	node = g_new0(node_t, 1);
	node->path = path;
	node->index = -1;
	node->expanded = e_tree_model_get_expanded_default(etta->priv->source);
	node->expandable = e_tree_model_node_is_expandable(etta->priv->source, path);
	node->expandable_set = 1;
	node->num_visible_children = 0;
	gnode = g_node_new(node);
	g_hash_table_insert(etta->priv->nodes, path, gnode);
	return gnode;
}

static gint
insert_children(ETreeTableAdapter *etta, GNode *gnode)
{
	ETreePath path, tmp;
	int count = 0;
	int pos = 0;

	path = ((node_t *)gnode->data)->path;
	for (tmp = e_tree_model_node_get_first_child(etta->priv->source, path);
	     tmp;
	     tmp = e_tree_model_node_get_next(etta->priv->source, tmp), pos++) {
		GNode *child = create_gnode(etta, tmp);
		node_t *node = (node_t *) child->data;
		if (node->expanded)
			node->num_visible_children = insert_children(etta, child);
		g_node_prepend(gnode, child);
		count += node->num_visible_children + 1;
	}
	g_node_reverse_children(gnode);
	return count;
}

static void
generate_tree(ETreeTableAdapter *etta, ETreePath path)
{
	GNode *gnode;
	node_t *node;
	int size;

	e_table_model_pre_change(E_TABLE_MODEL(etta));

	g_assert(e_tree_model_node_is_root(etta->priv->source, path));

	if (etta->priv->root)
		kill_gnode(etta->priv->root, etta);
	resize_map(etta, 0);

	gnode = create_gnode(etta, path);
	node = (node_t *) gnode->data;
	node->expanded = TRUE;
	node->num_visible_children = insert_children(etta, gnode);
	if (etta->priv->sort_info && e_table_sort_info_sorting_get_count(etta->priv->sort_info) > 0)
		resort_node(etta, gnode, TRUE);

	etta->priv->root = gnode;
	size =  etta->priv->root_visible ? node->num_visible_children + 1 : node->num_visible_children;
	resize_map(etta, size);
	fill_map(etta, 0, gnode);
	e_table_model_changed(E_TABLE_MODEL(etta));
}

static void
insert_node(ETreeTableAdapter *etta, ETreePath parent, ETreePath path)
{
	GNode *gnode, *parent_gnode;
	node_t *node, *parent_node;
	gboolean expandable;
	int size, row;

	e_table_model_pre_change(E_TABLE_MODEL(etta));

	if (get_node(etta, path)) {
		e_table_model_no_change(E_TABLE_MODEL(etta));
		return;
	}

	parent_gnode = lookup_gnode(etta, parent);
	if (!parent_gnode) {
		ETreePath grandparent = e_tree_model_node_get_parent(etta->priv->source, parent);
		if (e_tree_model_node_is_root(etta->priv->source, parent))
			generate_tree(etta, parent);
		else
			insert_node(etta, grandparent, parent);
		e_table_model_changed(E_TABLE_MODEL(etta));
		return;
	}

	parent_node = (node_t *) parent_gnode->data;

	if (parent_gnode != etta->priv->root) {
		expandable = e_tree_model_node_is_expandable(etta->priv->source, parent);
		if (parent_node->expandable != expandable) {
			e_table_model_pre_change(E_TABLE_MODEL(etta));
			parent_node->expandable = expandable;
			parent_node->expandable_set = 1;
			e_table_model_row_changed(E_TABLE_MODEL(etta), parent_node->index);
		}
	}

	if (!e_tree_table_adapter_node_is_expanded (etta, parent)) {
		e_table_model_no_change(E_TABLE_MODEL(etta));
		return;
	}

	gnode = create_gnode(etta, path);
	node = (node_t *) gnode->data;

	if (node->expanded)
		node->num_visible_children = insert_children(etta, gnode);

	g_node_append(parent_gnode, gnode);
	update_child_counts(parent_gnode, node->num_visible_children + 1);
	resort_node(etta, parent_gnode, FALSE);
	resort_node(etta, gnode, TRUE);

	size = node->num_visible_children + 1;
	resize_map(etta, etta->priv->n_map + size);
	if (parent_gnode == etta->priv->root)
		row = 0;
	else {
		gint new_size = parent_node->num_visible_children + 1;
		gint old_size = new_size - size;
		row = parent_node->index;
		move_map_elements(etta, row + new_size, row + old_size, etta->priv->n_map - row - new_size);
	}
	fill_map(etta, row, parent_gnode);
	e_table_model_rows_inserted(E_TABLE_MODEL(etta), get_row(etta, path), size);
}

typedef struct {
	GSList *paths;
	gboolean expanded;
} check_expanded_closure;

static gboolean
check_expanded(GNode *gnode, gpointer data)
{
	check_expanded_closure *closure = (check_expanded_closure *) data;
	node_t *node = (node_t *) gnode->data;

	if (node->expanded != closure->expanded)
		closure->paths = g_slist_prepend(closure->paths, node->path);

	return FALSE;
}

static void
update_node(ETreeTableAdapter *etta, ETreePath path)
{
	check_expanded_closure closure;
	ETreePath parent = e_tree_model_node_get_parent(etta->priv->source, path);
	GNode *gnode = lookup_gnode(etta, path);
	GSList *l;

	closure.expanded = e_tree_model_get_expanded_default (etta->priv->source);
	closure.paths = NULL;

	if (gnode)
		g_node_traverse(gnode, G_POST_ORDER, G_TRAVERSE_ALL, -1, check_expanded, &closure);

	if (e_tree_model_node_is_root(etta->priv->source, path))
		generate_tree(etta, path);
	else {
		delete_node(etta, parent, path);
		insert_node(etta, parent, path);
	}

	for (l = closure.paths; l; l = l->next)
		if (lookup_gnode(etta, l->data))
			e_tree_table_adapter_node_set_expanded (etta, l->data, !closure.expanded);

	g_slist_free(closure.paths);
}

static void
etta_finalize (GObject *object)
{
	ETreeTableAdapter *etta = E_TREE_TABLE_ADAPTER (object);

	if (etta->priv->root) {
		kill_gnode(etta->priv->root, etta);
		etta->priv->root = NULL;
	}

	g_hash_table_destroy (etta->priv->nodes);

	g_free (etta->priv->map_table);

	g_free (etta->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
etta_dispose (GObject *object)
{
	ETreeTableAdapter *etta = E_TREE_TABLE_ADAPTER (object);

	if (etta->priv->sort_info) {
		g_signal_handler_disconnect(G_OBJECT (etta->priv->sort_info),
				       etta->priv->sort_info_changed_id);
		g_object_unref(etta->priv->sort_info);
		etta->priv->sort_info = NULL;
	}

	if (etta->priv->header) {
		g_object_unref(etta->priv->header);
		etta->priv->header = NULL;
	}

	if (etta->priv->source) {
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->pre_change_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->no_change_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_data_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_col_changed_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_inserted_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_removed_id);
		g_signal_handler_disconnect (G_OBJECT (etta->priv->source),
				       etta->priv->node_request_collapse_id);

		g_object_unref (etta->priv->source);
		etta->priv->source = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
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

	return e_tree_model_get_save_id (etta->priv->source, get_path(etta, row));
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

	return etta->priv->n_map;
}

static void *
etta_value_at (ETableModel *etm, int col, int row)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	switch (col) {
	case -1:
		if (row == -1)
			return NULL;
		return get_path (etta, row);
	case -2:
		return etta->priv->source;
	case -3:
		return etta;
	default:
		return e_tree_model_value_at (etta->priv->source, get_path (etta, row), col);
	}
}

static void
etta_set_value_at (ETableModel *etm, int col, int row, const void *val)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	e_tree_model_set_value_at (etta->priv->source, get_path (etta, row), col, val);
}

static gboolean
etta_is_cell_editable (ETableModel *etm, int col, int row)
{
	ETreeTableAdapter *etta = (ETreeTableAdapter *)etm;

	return e_tree_model_node_is_editable (etta->priv->source, get_path (etta, row), col);
}

static void
etta_append_row (ETableModel *etm, ETableModel *source, int row)
{
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
	GObjectClass *object_class      = (GObjectClass *) klass;

	parent_class                    = g_type_class_peek_parent (klass);
	
	object_class->dispose           = etta_dispose;
	object_class->finalize          = etta_finalize;

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
	etta->priv                           = g_new(ETreeTableAdapterPriv, 1);

	etta->priv->source                   = NULL;
	etta->priv->sort_info                = NULL;

	etta->priv->n_map                    = 0;
	etta->priv->n_vals_allocated         = 0;
	etta->priv->map_table                = NULL;
	etta->priv->nodes                    = NULL;
	etta->priv->root                     = NULL;

	etta->priv->root_visible             = TRUE;
	etta->priv->remap_needed             = TRUE;

	etta->priv->pre_change_id            = 0;
	etta->priv->no_change_id             = 0;
	etta->priv->node_changed_id          = 0;
	etta->priv->node_data_changed_id     = 0;
	etta->priv->node_col_changed_id      = 0;
	etta->priv->node_inserted_id         = 0;
	etta->priv->node_removed_id          = 0;
	etta->priv->node_request_collapse_id = 0;
}

E_MAKE_TYPE(e_tree_table_adapter, "ETreeTableAdapter", ETreeTableAdapter, etta_class_init, etta_init, PARENT_TYPE)

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
	update_node(etta, path);

	e_table_model_changed(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_data_changed (ETreeModel *etm, ETreePath path, ETreeTableAdapter *etta)
{
	int row = get_row(etta, path);

	if (row == -1) {
		e_table_model_no_change(E_TABLE_MODEL(etta));
		return;
	}

	e_table_model_row_changed(E_TABLE_MODEL(etta), row);
}

static void
etta_proxy_node_col_changed (ETreeModel *etm, ETreePath path, int col, ETreeTableAdapter *etta)
{
	int row = get_row(etta, path);

	if (row == -1) {
		e_table_model_no_change(E_TABLE_MODEL(etta));
		return;
	}

	e_table_model_cell_changed(E_TABLE_MODEL(etta), col, row);
}

static void
etta_proxy_node_inserted (ETreeModel *etm, ETreePath parent, ETreePath child, ETreeTableAdapter *etta)
{
	if (e_tree_model_node_is_root(etm, child))
		generate_tree(etta, child);
	else
		insert_node(etta, parent, child);
	
	e_table_model_changed(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_removed (ETreeModel *etm, ETreePath parent, ETreePath child, int old_position, ETreeTableAdapter *etta)
{
	delete_node(etta, parent, child);
	e_table_model_changed(E_TABLE_MODEL(etta));
}

static void
etta_proxy_node_request_collapse (ETreeModel *etm, ETreePath node, ETreeTableAdapter *etta)
{
	e_tree_table_adapter_node_set_expanded(etta, node, FALSE);
}

static void
etta_sort_info_changed (ETableSortInfo *sort_info, ETreeTableAdapter *etta)
{
	if (!etta->priv->root)
		return;

	e_table_model_pre_change(E_TABLE_MODEL(etta));
	resort_node(etta, etta->priv->root, TRUE);
	fill_map(etta, 0, etta->priv->root);
	e_table_model_changed(E_TABLE_MODEL(etta));
}

ETableModel *
e_tree_table_adapter_construct (ETreeTableAdapter *etta, ETreeModel *source, ETableSortInfo *sort_info, ETableHeader *header)
{
	ETreePath root;

	etta->priv->source = source;
	g_object_ref (source);

	etta->priv->sort_info = sort_info;
	if (sort_info) {
		g_object_ref(sort_info);
		etta->priv->sort_info_changed_id = g_signal_connect (G_OBJECT (sort_info), "sort_info_changed",
				                                     G_CALLBACK (etta_sort_info_changed), etta);
	}

	etta->priv->header = header;
	if (header)
		g_object_ref(header);

	etta->priv->nodes = g_hash_table_new(NULL, NULL);

	root = e_tree_model_get_root (source);

	if (root)
		generate_tree(etta, root);

	etta->priv->pre_change_id = g_signal_connect(G_OBJECT(source), "pre_change",
					G_CALLBACK (etta_proxy_pre_change), etta);
	etta->priv->no_change_id = g_signal_connect (G_OBJECT (source), "no_change",
					G_CALLBACK (etta_proxy_no_change), etta);
	etta->priv->node_changed_id = g_signal_connect (G_OBJECT (source), "node_changed",
					G_CALLBACK (etta_proxy_node_changed), etta);
	etta->priv->node_data_changed_id = g_signal_connect (G_OBJECT (source), "node_data_changed",
					G_CALLBACK (etta_proxy_node_data_changed), etta);
	etta->priv->node_col_changed_id = g_signal_connect (G_OBJECT (source), "node_col_changed",
					G_CALLBACK (etta_proxy_node_col_changed), etta);
	etta->priv->node_inserted_id = g_signal_connect (G_OBJECT (source), "node_inserted",
					G_CALLBACK (etta_proxy_node_inserted), etta);
	etta->priv->node_removed_id = g_signal_connect (G_OBJECT (source), "node_removed",
					G_CALLBACK (etta_proxy_node_removed), etta);
	etta->priv->node_request_collapse_id = g_signal_connect (G_OBJECT (source), "node_request_collapse",
					G_CALLBACK (etta_proxy_node_request_collapse), etta);

	return E_TABLE_MODEL (etta);
}

ETableModel *
e_tree_table_adapter_new (ETreeModel *source, ETableSortInfo *sort_info, ETableHeader *header)
{
	ETreeTableAdapter *etta = g_object_new (E_TREE_TABLE_ADAPTER_TYPE, NULL);

	e_tree_table_adapter_construct (etta, source, sort_info, header);

	return (ETableModel *) etta;
}

typedef struct {
	xmlNode *root;
	gboolean expanded_default;
	ETreeModel *model;
} TreeAndRoot;

static void
save_expanded_state_func (gpointer keyp, gpointer value, gpointer data)
{
	ETreePath path = keyp;
	node_t *node = ((GNode *)value)->data;
	TreeAndRoot *tar = data;
	xmlNode *xmlnode;

	if (node->expanded != tar->expanded_default) {
		gchar *save_id = e_tree_model_get_save_id(tar->model, path);
		xmlnode = xmlNewChild (tar->root, NULL, "node", NULL);
		e_xml_set_string_prop_by_name(xmlnode, "id", save_id);
		g_free(save_id);
	}
}

void
e_tree_table_adapter_save_expanded_state (ETreeTableAdapter *etta, const char *filename)
{
	TreeAndRoot tar;
	xmlDocPtr doc;
	xmlNode *root;
	
	g_return_if_fail(etta != NULL);

	doc = xmlNewDoc ("1.0");
	root = xmlNewDocNode (doc, NULL, (xmlChar *) "expanded_state", NULL);
	xmlDocSetRootElement (doc, root);

	tar.model = etta->priv->source;
	tar.root = root;
	tar.expanded_default = e_tree_model_get_expanded_default(etta->priv->source);
	
	e_xml_set_integer_prop_by_name (root, "vers", 2);
	e_xml_set_bool_prop_by_name (root, "default", tar.expanded_default);

	g_hash_table_foreach (etta->priv->nodes, save_expanded_state_func, &tar);
	
	e_xml_save_file (filename, doc);
	xmlFreeDoc (doc);
}

static xmlDoc *
open_file (ETreeTableAdapter *etta, const char *filename)
{
	xmlDoc *doc;
	xmlNode *root;
	int vers;
	gboolean model_default, saved_default;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		return NULL;

#ifdef G_OS_WIN32
	{
		gchar *locale_filename = g_win32_locale_filename_from_utf8 (filename);
		doc = xmlParseFile (locale_filename);
		g_free (locale_filename);
	}
#else
	doc = xmlParseFile (filename);
#endif

	if (!doc)
		return NULL;

	root = xmlDocGetRootElement (doc);
	if (root == NULL || strcmp (root->name, "expanded_state")) {
		xmlFreeDoc (doc);
		return NULL;
	}

	vers = e_xml_get_integer_prop_by_name_with_default (root, "vers", 0);
	if (vers > 2) {
		xmlFreeDoc (doc);
		return NULL;
	}
	model_default = e_tree_model_get_expanded_default (etta->priv->source);
	saved_default = e_xml_get_bool_prop_by_name_with_default (root, "default", !model_default);
	if (saved_default != model_default) {
		xmlFreeDoc (doc);
		return NULL;
	}

	return doc;
}

void
e_tree_table_adapter_load_expanded_state (ETreeTableAdapter *etta, const char *filename)
{
	xmlDoc *doc;
	xmlNode *root, *child;
	gboolean model_default;

	g_return_if_fail(etta != NULL);

	doc = open_file(etta, filename);
	if (!doc)
		return;

	root = xmlDocGetRootElement (doc);

	e_table_model_pre_change(E_TABLE_MODEL(etta));

	model_default = e_tree_model_get_expanded_default(etta->priv->source);

	for (child = root->xmlChildrenNode; child; child = child->next) {
		char *id;
		ETreePath path;

		if (strcmp (child->name, "node")) {
			d(g_warning ("unknown node '%s' in %s", child->name, filename));
			continue;
		}

		id = e_xml_get_string_prop_by_name_with_default (child, "id", "");

		if (!strcmp(id, "")) {
			g_free(id);
			continue;
		}

		path = e_tree_model_get_node_by_id(etta->priv->source, id);
		if (path)
			e_tree_table_adapter_node_set_expanded(etta, path, !model_default);

		g_free (id);
	}

	xmlFreeDoc (doc);

	e_table_model_changed (E_TABLE_MODEL (etta));
}

void
e_tree_table_adapter_root_node_set_visible (ETreeTableAdapter *etta, gboolean visible)
{
	int size;

	if (etta->priv->root_visible == visible)
		return;

	e_table_model_pre_change (E_TABLE_MODEL(etta));

	etta->priv->root_visible = visible;
	if (!visible) {
		ETreePath root = e_tree_model_get_root(etta->priv->source);
		if (root)
			e_tree_table_adapter_node_set_expanded(etta, root, TRUE);
	}
	size = (visible ? 1 : 0) + (etta->priv->root ? ((node_t *)etta->priv->root->data)->num_visible_children : 0);
	resize_map(etta, size);
	if (etta->priv->root)
		fill_map(etta, 0, etta->priv->root);
	e_table_model_changed(E_TABLE_MODEL(etta));
}

void
e_tree_table_adapter_node_set_expanded (ETreeTableAdapter *etta, ETreePath path, gboolean expanded)
{
	GNode *gnode = lookup_gnode(etta, path);
	node_t *node;
	int row;

	if (!expanded && (!gnode || (e_tree_model_node_is_root (etta->priv->source, path) && !etta->priv->root_visible)))
		return;

	if (!gnode && expanded) {
		ETreePath parent = e_tree_model_node_get_parent(etta->priv->source, path);
		g_return_if_fail(parent != NULL);
		e_tree_table_adapter_node_set_expanded(etta, parent, expanded);
		gnode = lookup_gnode(etta, path);
	}
	g_return_if_fail(gnode != NULL);

	node = (node_t *) gnode->data;

	if (expanded == node->expanded)
		return;

	node->expanded = expanded;
		
	row = get_row(etta, path);
	if (row == -1)
		return;

	e_table_model_pre_change (E_TABLE_MODEL(etta));
	e_table_model_pre_change (E_TABLE_MODEL(etta));
	e_table_model_row_changed(E_TABLE_MODEL(etta), row);


	if (expanded) {
		int num_children = insert_children(etta, gnode);
		update_child_counts(gnode, num_children);
		if (etta->priv->sort_info && e_table_sort_info_sorting_get_count(etta->priv->sort_info) > 0)
			resort_node(etta, gnode, TRUE);
		resize_map(etta, etta->priv->n_map + num_children);
		move_map_elements(etta, row + 1 + num_children, row + 1, etta->priv->n_map - row - 1 - num_children);
		fill_map(etta, row, gnode);
		if (num_children != 0) {
			e_table_model_rows_inserted(E_TABLE_MODEL(etta), row + 1, num_children);
		} else
			e_table_model_no_change(E_TABLE_MODEL(etta));
	} else {
		int num_children = delete_children(etta, gnode);
		if (num_children == 0) {
			e_table_model_no_change(E_TABLE_MODEL(etta));
			return;
		}
		move_map_elements(etta, row + 1, row + 1 + num_children, etta->priv->n_map - row - 1 - num_children);
		update_child_counts(gnode, - num_children);
		resize_map(etta, etta->priv->n_map - num_children);
		e_table_model_rows_deleted(E_TABLE_MODEL(etta), row + 1, num_children);
	}
}

void
e_tree_table_adapter_node_set_expanded_recurse (ETreeTableAdapter *etta, ETreePath path, gboolean expanded)
{
	ETreePath children;

	e_tree_table_adapter_node_set_expanded(etta, path, expanded);

	for (children = e_tree_model_node_get_first_child(etta->priv->source, path); 
	     children; 
	     children = e_tree_model_node_get_next(etta->priv->source, children)) {
		e_tree_table_adapter_node_set_expanded_recurse(etta, children, expanded);
	}
}

ETreePath
e_tree_table_adapter_node_at_row (ETreeTableAdapter *etta, int row)
{
	return get_path(etta, row);
}

int
e_tree_table_adapter_row_of_node (ETreeTableAdapter *etta, ETreePath path)
{
	return get_row(etta, path);
}

gboolean     
e_tree_table_adapter_root_node_is_visible(ETreeTableAdapter *etta)
{
	return etta->priv->root_visible;
}

void         
e_tree_table_adapter_show_node (ETreeTableAdapter *etta, ETreePath path)
{
	ETreePath parent;

	parent = e_tree_model_node_get_parent(etta->priv->source, path);

	while (parent) {
		e_tree_table_adapter_node_set_expanded(etta, parent, TRUE);
		parent = e_tree_model_node_get_parent(etta->priv->source, parent);
	}
}

gboolean     
e_tree_table_adapter_node_is_expanded (ETreeTableAdapter *etta, ETreePath path)
{
	node_t *node = get_node(etta, path);
	if (!e_tree_model_node_is_expandable (etta->priv->source, path) || !node)
		return FALSE;

	return node->expanded;
}

void
e_tree_table_adapter_set_sort_info (ETreeTableAdapter *etta, ETableSortInfo *sort_info)
{
	if (etta->priv->sort_info) {
		g_signal_handler_disconnect(G_OBJECT(etta->priv->sort_info),
				            etta->priv->sort_info_changed_id);
		g_object_unref(etta->priv->sort_info);
	}

	etta->priv->sort_info = sort_info;
	if (sort_info) {
		g_object_ref(sort_info);
		etta->priv->sort_info_changed_id = g_signal_connect(G_OBJECT(sort_info), "sort_info_changed",
				                                    G_CALLBACK(etta_sort_info_changed), etta);
	}

	if (!etta->priv->root)
		return;

	e_table_model_pre_change(E_TABLE_MODEL(etta));
	resort_node(etta, etta->priv->root, TRUE);
	fill_map(etta, 0, etta->priv->root);
	e_table_model_changed(E_TABLE_MODEL(etta));
}

ETreePath
e_tree_table_adapter_node_get_next (ETreeTableAdapter *etta, ETreePath path)
{
	GNode *node = lookup_gnode (etta, path);

	if (node && node->next)
		return ((node_t *)node->next->data)->path;

	return NULL;
}

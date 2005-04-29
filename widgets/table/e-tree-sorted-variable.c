/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-sorted-variable.c
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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "gal/util/e-util.h"

#include "e-tree-sorted-variable.h"

#define d(x)

#define INCREMENT_AMOUNT 100

/* maximum insertions between an idle event that we will do without scheduling an idle sort */
#define ETSV_INSERT_MAX (4)

static ETreeModelClass *etsv_parent_class;

struct ETreePath {
	GNode node;
};

struct ETreeSortedVariablePrivate {
	GNode *root;
};

static void etsv_proxy_model_changed      (ETableModel *etm, ETreeSortedVariable *etsv);
#if 0
static void etsv_proxy_model_row_changed  (ETableModel *etm, int row, ETreeSortedVariable *etsv);
static void etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETreeSortedVariable *etsv);
#endif
static void etsv_sort_info_changed        (ETableSortInfo *info, ETreeSortedVariable *etsv);
static void etsv_sort                     (ETreeSortedVariable *etsv);
static void etsv_add                      (ETreeSortedVariable *etsv, gint                  row);
static void etsv_add_all                  (ETreeSortedVariable *etsv);

static void
etsv_dispose (GObject *object)
{
	ETreeSortedVariable *etsv = E_TREE_SORTED_VARIABLE (object);

	if (etsv->table_model_changed_id)
		g_signal_handler_disconnect (G_OBJECT (etss->source),
				             etsv->table_model_changed_id);
	etsv->table_model_changed_id = 0;

#if 0
	g_signal_handler_disconnect (etss->source,
				     etsv->table_model_row_changed_id);
	g_signal_handler_disconnect (etss->source,
				     etsv->table_model_cell_changed_id);

	etsv->table_model_row_changed_id = 0;
	etsv->table_model_cell_changed_id = 0;
#endif
	if (etsv->sort_info_changed_id)
		g_signal_handler_disconnect (etsv->sort_info,
				             etsv->sort_info_changed_id);
	etsv->sort_info_changed_id = 0;

	if (etsv->sort_idle_id)
		g_source_remove(etsv->sort_idle_id);
	etsv->sort_idle_id = 0;
	
	if (etsv->insert_idle_id)
		g_source_remove(etsv->insert_idle_id);
	etsv->insert_idle_id = 0;

	if (etsv->sort_info)
		g_object_unref(etsv->sort_info);
	etsv->sort_info = NULL;

	if (etsv->full_header)
		g_object_unref(etsv->full_header);
	etsv->full_header = NULL;

	G_OBJECT_CLASS (etsv_parent_class)->dispose (object);
}

static void
etsv_class_init (GObjectClass *object_class)
{
	ETreeSortedVariableClass *etsv_class = E_TREE_MODEL_CLASS(object_class);

	etsv_parent_class = g_type_class_peek_parent (object_class);

	object_class->dispose = etsv_dispose;

	etsv_class->add = etsv_add;
	etsv_class->add_all = etsv_add_all;
}

static void
etsv_init (ETreeSortedVariable *etsv)
{
	etsv->full_header = NULL;
	etsv->sort_info = NULL;

	etsv->table_model_changed_id = 0;
	etsv->table_model_row_changed_id = 0;
	etsv->table_model_cell_changed_id = 0;
	etsv->sort_info_changed_id = 0;

	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
}

E_MAKE_TYPE(e_tree_sorted_variable, "ETreeSortedVariable", ETreeSortedVariable, etsv_class_init, etsv_init, E_TREE_MODEL_TYPE)

static gboolean
etsv_sort_idle(ETreeSortedVariable *etsv)
{
	g_object_ref(etsv);
	etsv_sort(etsv);
	etsv->sort_idle_id = 0;
	etsv->insert_count = 0;
	g_object_unref(etsv);
	return FALSE;
}

static gboolean
etsv_insert_idle(ETreeSortedVariable *etsv)
{
	etsv->insert_count = 0;
	etsv->insert_idle_id = 0;
	return FALSE;
}


ETableModel *
e_tree_sorted_variable_new (ETreeModel *source, ETableHeader *full_header, ETableSortInfo *sort_info)
{
	ETreeSortedVariable *etsv = g_object_new (E_TREE_SORTED_VARIABLE_TYPE, NULL);
	ETreeSortedVariable *etsv = E_TABLE_SUBSET_VARIABLE (etsv);

	if (e_table_subset_variable_construct (etsv, source) == NULL){
		g_object_unref (etsv);
		return NULL;
	}

	etsv->sort_info = sort_info;
	g_object_ref(etsv->sort_info);
	etsv->full_header = full_header;
	g_object_ref(etsv->full_header);

	etsv->table_model_changed_id = g_signal_connect (source, "model_changed",
							 G_CALLBACK (etsv_proxy_model_changed), etsv);
#if 0
	etsv->table_model_row_changed_id = g_signal_connect (source, "model_row_changed",
							     G_CALLBACK (etsv_proxy_model_row_changed), etsv);
	etsv->table_model_cell_changed_id = g_signal_connect (source, "model_cell_changed",
							      G_CALLBACK (etsv_proxy_model_cell_changed), etsv);
#endif
	etsv->sort_info_changed_id = g_signal_connect (sort_info, "sort_info_changed",
						       G_CALLBACK (etsv_sort_info_changed), etsv);

	return E_TABLE_MODEL(etsv);
}

static void
etsv_proxy_model_changed (ETableModel *etm, ETreeSortedVariable *etsv)
{
	/* FIXME: do_resort (); */
}
#if 0
static void
etsv_proxy_model_row_changed (ETableModel *etm, int row, ETreeSortedVariable *etsv)
{
	ETreeSortedVariable *etsv = E_TABLE_SUBSET_VARIABLE(etsv);

	if (e_table_subset_variable_remove(etsv, row))
		e_table_subset_variable_add (etsv, row);
}

static void
etsv_proxy_model_cell_changed (ETableModel *etm, int col, int row, ETreeSortedVariable *etsv)
{
	ETreeSortedVariable *etsv = E_TABLE_SUBSET_VARIABLE(etsv);

	if (e_table_subset_variable_remove(etsv, row))
		e_table_subset_variable_add (etsv, row);
}
#endif

static void
etsv_sort_info_changed (ETableSortInfo *info, ETreeSortedVariable *etsv)
{
	etsv_sort(etsv);
}

/* This takes source rows. */
static int
etsv_compare(ETreeSortedVariable *etsv, const ETreePath *path1, const ETreePath *path2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv->sort_info);
	int comp_val = 0;
	int ascending = 1;

	for (j = 0; j < sort_count; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(etsv->sort_info, j);
		ETableCol *col;
		col = e_table_header_get_column_by_col_idx(etsv->full_header, column.column);
		if (col == NULL)
			col = e_table_header_get_column (etsv->full_header, e_table_header_count (etsv->full_header) - 1);
		comp_val = (*col->compare)(e_tree_model_value_at (etsv->source, path1, col->col_idx),
					   e_tree_model_value_at (etsv->source, path2, col->col_idx));
		ascending = column.ascending;
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}


static ETreeSortedVariable *etsv_closure;
int cols_closure;
int *ascending_closure;
int *col_idx_closure;
GCompareFunc *compare_closure;

static int
etsv_compare_closure(const ETreePath *path1, const ETreePath *path2)
{
	int j;
	int sort_count = e_table_sort_info_sorting_get_count(etsv_closure->sort_info);
	int comp_val = 0;
	int ascending = 1;
	for (j = 0; j < sort_count; j++) {

		comp_val = (*(compare_closure[j]))(e_tree_model_value_at (etsv_closure->source, path1, col_idx_closure[j]),
						   e_tree_model_value_at (etsv_closure->source, path2, col_idx_closure[j]));
		ascending = ascending_closure[j];
		if (comp_val != 0)
			break;
	}
	if (comp_val == 0) {
		if (row1 < row2)
			comp_val = -1;
		if (row1 > row2)
			comp_val = 1;
	}
	if (!ascending)
		comp_val = -comp_val;
	return comp_val;
}

static int
qsort_callback(const void *data1, const void *data2)
{
	GNode *node1 = *(GNode **)data1;
	GNode *node2 = *(GNode **)data2;
	return etsv_compare_closure(node1->data, node2->data);
}

static int
qsort_callback_source(const void *data1, const void *data2)
{
	return etsv_compare_closure(data1, data2);
}

static void
etsv_setup_closures(ETreeSortedVariable *etsv)
{
	int j;
	int cols;

	cols = e_table_sort_info_sorting_get_count(etsv->sort_info);
	cols_closure = cols;
	etsv_closure = etsv;

	ascending_closure = g_new(int, cols);
	col_idx_closure = g_new(int, cols);
	compare_closure = g_new(GCompareFunc, cols);

	for (j = 0; j < cols; j++) {
		ETableSortColumn column = e_table_sort_info_sorting_get_nth(etsv->sort_info, j);
		ETableCol *col;

		col = e_table_header_get_column_by_col_idx(etsv->full_header, column.column);
		if (col == NULL) {
			col = e_table_header_get_column (etsv->full_header, e_table_header_count (etsv->full_header) - 1);
		}

		ascending_closure[j] = column.ascending;
		col_idx_closure[j] = col->col_idx;
		compare_closure[j] = col->compare;
	}
}

static void
etsv_free_closures(ETreeSortedVariable *etsv)
{
	g_free(ascending_closure);
	g_free(col_idx_closure);
	g_free(compare_closure);

}

static void
etsv_sort_node(ETreeSortedVariable *etsv, GNode *node)
{
	gint n;
	gint i;
	GNode **children;
	GNode *child;
	GNode *prev;

	n = g_node_n_children(node);
	children = g_new(GNode *, n);
	for (i = 0, child = node->children; child && i; child = child->next, i++) {
		children[i] = child;
	}
	qsort(children, n, sizeof(GNode *), qsort_callback);

	prev = NULL;
	for (i = 0; i < n; i++) {
		children[i]->prev = prev;
		if (prev) prev->next = children[i];
		prev = children[i];
		children[i]->next = NULL;
	}
}

static void
etsv_sort_tree(ETreeSortedVariable *etsv, GNode *root)
{
	GNode *childr;

	etsv_sort_node(etsv, node);
	
	for (child = node->child; child; child = child->next) {
		etsv_sort_tree(etsv, child);
	}
}

static void
etsv_sort(ETreeSortedVariable *etsv)
{
	static int reentering = 0;
	if (reentering)
		return;
	reentering = 1;

	e_table_model_pre_change(E_TABLE_MODEL(etsv));

	etsv_setup_closures(etsv);

	etsv_sort_tree(etsv, etsv->root);

	etsv_free_closures(etsv);

	e_table_model_changed (E_TABLE_MODEL(etsv));
	reentering = 0;
}

static void
etsv_add_node (ETreeSortedVariable *etsv, ETreePath *path, GNode *root)
{
	GNode *node;
	GNode *new_node;
	for (node = root; node; node = node->next) {
		if (e_tree_model_node_is_ancestor(etsv->source, path, node->data)) {
			etsv_add_node(etsv, path, node->data);
			return;
		}
	}
	new_node = g_node_new(path);
	for (node = root; node; ) {
		if (e_tree_model_node_is_ancestor(etsv->source, node->data, path)) {
			GNode *next;
			next = node->next;
			g_node_unlink(node);
			g_node_prepend(new_node, node);
			node = next;
		} else
			node = node->next;
	}

	etsv_sort_node(etsv, new_node);


#if 0
	g_node_prepend(root, new_node);
	etsv_sort_node(etsv, root);
#else
	/* Insert sort to be a bit faster than the above prepend and then sort. */
	for (node = root; node; node = node->next) {
		if (etsv_compare(etsv, path, node->data) > 0) {
			g_node_insert_before (root, node, new_node);
			return;
		}
	}
	g_node_append(root, new_node);
#endif
}

etsv_add(ETreeSortedVariable *etsv, gint row)
{
	ETreeModel *source = etsv->source;
	ETreePath *path;

	path = e_table_model_value_at (E_TABLE_MODEL(source), -1, row);
	etsv_add_node(etsv, path, etsv->root);
}

/* Optimize by doing the qsorts as we build.  But we'll do that later. */
static void
etsv_add_all_node (ETreeSortedVariable *etsv, ETreePath *path, GNode *node)
{
	ETreeModel *source = etsv->source;
	ETreePath **children;
	int n;
	int i;

	n = e_tree_model_node_get_children(source, path, &children);
	qsort(children, n, sizeof(ETreePath *), qsort_callback_source);

	for (i = n - 1; i >= 0; i--) {
		GNode *new_child = g_node_new(children[i]);
		g_node_prepend(path, new_child);
		etsv_add_all_node (etsv, children[i], new_child)
	}

	g_free(children);
}

static void
etsv_add_all   (ETreeSortedVariable *etsv)
{
	GNode *node;
	ETreePath *path;

	e_table_model_pre_change(etm);

	if (etsv->root)
		g_node_destroy(etsv->root);

	etsv_setup_closures(etsv);

	path = e_tree_model_get_root(etsv->source);
	node = g_node_new(path);
	etsv_add_all_node(etsv, path, node);
	etsv->root = node;

	etsv_free_closures(etsv);

	e_tree_model_node_changed (etsv, etsv->root);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-tree-selection-model.c
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include "e-tree-selection-model.h"
#include <gal/util/e-bit-array.h>
#include <gal/util/e-sorter.h>
#include <gal/util/e-util.h>
#include <gdk/gdkkeysyms.h>
#include <gal/e-table/e-tree-sorted.h>
#include <gal/e-table/e-tree-table-adapter.h>

#define ETSM_CLASS(e) ((ETreeSelectionModelClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE e_selection_model_get_type ()

static ESelectionModelClass *parent_class;

enum {
	ARG_0,
	ARG_CURSOR_ROW,
	ARG_CURSOR_COL,
	ARG_MODEL,
	ARG_ETTA,
	ARG_ETS,
};

typedef struct ETreeSelectionModelNode {
	guint selected : 1;
	guint all_children_selected : 1;
	guint any_children_selected : 1;
	EBitArray *all_children_selected_array;
	EBitArray *any_children_selected_array;
	struct ETreeSelectionModelNode **children;
	int num_children;
} ETreeSelectionModelNode;

struct ETreeSelectionModelPriv {
	ETreeTableAdapter *etta;
	ETreeSorted *ets;
	ETreeModel *model;

	ETreeSelectionModelNode *root;

	ETreePath cursor_path;
	gint cursor_col;
	ETreePath selection_start_path;

	char *cursor_save_id;

	int          frozen_count;

	int          tree_model_pre_change_id;
	int          tree_model_no_change_id;
	int          tree_model_node_changed_id;
	int          tree_model_node_data_changed_id;
	int          tree_model_node_col_changed_id;
	int          tree_model_node_inserted_id;
	int          tree_model_node_removed_id;
	int          tree_model_node_deleted_id;

	int          sorted_model_node_resorted_id;

	/* Anything other than -1 means that the selection is a single
	 * row.  This being -1 does not impart any information. */
	int          selected_row;
	/* Anything other than -1 means that the selection is a all
	 * rows between selection_start_path and cursor_path where
	 * selected_range_end is the rwo number of cursor_path.  This
	 * being -1 does not impart any information. */
	int          selected_range_end;
};

/* ETreeSelectionModelNode helpers */

static ETreeSelectionModelNode *
e_tree_selection_model_node_new (void)
{
	ETreeSelectionModelNode *node = g_new(ETreeSelectionModelNode, 1);

	node->selected = 0;
	node->all_children_selected = 0;
	node->any_children_selected = 0;
	node->all_children_selected_array = NULL;
	node->any_children_selected_array = NULL;
	node->children = NULL;
	node->num_children = -1;

	return node;
}

static void
e_tree_selection_model_node_fill_children(ETreeSelectionModel *etsm, ETreePath path, ETreeSelectionModelNode *selection_node)
{
	int i;
	selection_node->num_children = e_tree_sorted_node_num_children(etsm->priv->ets, path);
	selection_node->children = g_new(ETreeSelectionModelNode *, selection_node->num_children);
	for (i = 0; i < selection_node->num_children; i++) {
		selection_node->children[i] = NULL;
	}
}

static void
e_tree_selection_model_node_free(ETreeSelectionModelNode *node)
{
	int i;

	if (node->all_children_selected_array)
		gtk_object_unref(GTK_OBJECT(node->all_children_selected_array));
	if (node->any_children_selected_array)
		gtk_object_unref(GTK_OBJECT(node->any_children_selected_array));

	for (i = 0; i < node->num_children; i++)
		if (node->children[i])
			e_tree_selection_model_node_free(node->children[i]);
	g_free(node->children);

	g_free(node);
}


/* Other helper functions */
static ETreePath
etsm_node_at_row(ETreeSelectionModel *etsm, int row)
{
	ETreePath path;

	if (!(row >= 0 && row < e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta))))
		return NULL;

	path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);

	if (path)
		path = e_tree_sorted_view_to_model_path(etsm->priv->ets, path);
 
	return path;
}

static int
etsm_row_of_node(ETreeSelectionModel *etsm, ETreePath path)
{
	path = e_tree_sorted_model_to_view_path(etsm->priv->ets, path);

	if (path)
		return e_tree_table_adapter_row_of_node(etsm->priv->etta, path);
	else
		return -1;
}

static int
etsm_cursor_row_real (ETreeSelectionModel *etsm)
{
	if (etsm->priv->cursor_path)
		return etsm_row_of_node(etsm, etsm->priv->cursor_path);
	else
		return -1;
}

static void
etsm_real_clear (ETreeSelectionModel *etsm)
{
	if (etsm->priv->root) {
		e_tree_selection_model_node_free(etsm->priv->root);
		etsm->priv->root = NULL;
	}
}

static ETreeSelectionModelNode *
etsm_find_node_unless_equals (ETreeSelectionModel *etsm,
			      ETreePath path,
			      gboolean grow)
{
	ETreeSelectionModelNode *selection_node;
	ETreeSorted *ets = etsm->priv->ets;
	ETreePath parent;

	parent = e_tree_model_node_get_parent(E_TREE_MODEL(ets), path);

	if (parent) {
		selection_node = etsm_find_node_unless_equals(etsm, parent, grow);
		if (selection_node) {
			int position = e_tree_sorted_orig_position(ets, path);
			if (selection_node->all_children_selected && grow)
				return NULL;
			if (!(selection_node->any_children_selected || grow))
				return NULL;
			if (selection_node->all_children_selected_array && e_bit_array_value_at(selection_node->all_children_selected_array, position) && grow)
				return NULL;
			if (selection_node->any_children_selected_array && ! (e_bit_array_value_at(selection_node->any_children_selected_array, position) || grow))
				return NULL;
			if (selection_node->children == NULL) {
				e_tree_selection_model_node_fill_children(etsm, parent, selection_node);
			}
			if (!selection_node->children[position]) 
				selection_node->children[position] = e_tree_selection_model_node_new();

			return selection_node->children[position];
		} else
			return NULL;
	} else {
		if (!etsm->priv->root)
			etsm->priv->root = e_tree_selection_model_node_new();
		return etsm->priv->root;
	}
}

#if 0
static ETreeSelectionModelNode *
find_or_create_node (ETreeSelectionModel *etsm,
		     ETreePath path)
{
	ETreeSelectionModelNode *selection_node;
	ETreeSelectionModelNode **place = NULL;
	ETreeSorted *ets = etsm->priv->ets;
	ETreePath parent;

	parent = e_tree_model_node_get_parent(E_TREE_MODEL(ets), path);

	if (parent) {
		selection_node = find_or_create_node(etsm, parent);
		if (selection_node) {
			int position = e_tree_sorted_orig_position(ets, path);
			if (!selection_node->children) {
				e_tree_selection_model_node_fill_children(etsm, parent, selection_node);
			}
			if (!selection_node->children[position]) 
				slection_node->children[position] = e_tree_selection_model_node_new();

			return selection_node->children[position];
		} else
			  return NULL;
	} else {
		if (!etsm->priv->root)
			etsm->priv->root = e_tree_selection_model_node_new();
		return etsm->priv->root;
	}
}
#endif

static void
update_parents (ETreeSelectionModel *etsm, ETreePath path)
{
	int i;
	int depth;
	ETreeSorted *ets = etsm->priv->ets;
	int *orig_position_sequence;
	ETreeSelectionModelNode **node_sequence;
	ETreePath parents;

	if (!etsm->priv->root)
		return;

	depth = e_tree_model_node_depth (E_TREE_MODEL(ets), path);

	orig_position_sequence = g_new(int, depth + 1);
	node_sequence = g_new(ETreeSelectionModelNode *, depth + 1);

	parents = path;

	for (i = depth; i > 0; i--) {
		if (!parents) {
			g_free(orig_position_sequence);
			g_free(node_sequence);
			return;
		}
		orig_position_sequence[i] = e_tree_sorted_orig_position(etsm->priv->ets, parents);
		parents = e_tree_model_node_get_parent(E_TREE_MODEL(etsm->priv->ets), parents);
	}

	node_sequence[0] = etsm->priv->root;
	for (i = 0; i < depth; i++) {
		node_sequence[i + 1] = NULL;

		if (node_sequence[i]->children)
			node_sequence[i + 1] = node_sequence[i]->children[orig_position_sequence[i + 1]];

		if (node_sequence[i + 1] == NULL) {
			g_free(orig_position_sequence);
			g_free(node_sequence);
			return;
		}
	}

	if (node_sequence[depth]->num_children == -1)
		e_tree_selection_model_node_fill_children(etsm, path, node_sequence[depth]);

	if (!node_sequence[depth]->all_children_selected_array)
		node_sequence[depth]->all_children_selected_array = e_bit_array_new(node_sequence[depth]->num_children);
	if (!node_sequence[depth]->any_children_selected_array)
		node_sequence[depth]->any_children_selected_array = e_bit_array_new(node_sequence[depth]->num_children);

	node_sequence[depth]->all_children_selected =
		e_bit_array_cross_and(node_sequence[depth]->all_children_selected_array) &&
		node_sequence[depth]->selected;

	node_sequence[depth]->any_children_selected =
		e_bit_array_cross_or(node_sequence[depth]->any_children_selected_array) ||
		node_sequence[depth]->selected;

	for (i = depth - 1; i >= 0; i--) {
		gboolean all_children, any_children;

		if (!node_sequence[i]->all_children_selected_array)
			node_sequence[i]->all_children_selected_array = e_bit_array_new(node_sequence[i]->num_children);
		if (!node_sequence[i]->any_children_selected_array)
			node_sequence[i]->any_children_selected_array = e_bit_array_new(node_sequence[i]->num_children);

		e_bit_array_change_one_row(node_sequence[i]->all_children_selected_array,
					   orig_position_sequence[i + 1], node_sequence[i + 1]->all_children_selected);
		e_bit_array_change_one_row(node_sequence[i]->any_children_selected_array,
					   orig_position_sequence[i + 1], node_sequence[i + 1]->any_children_selected);

		all_children = node_sequence[i]->all_children_selected;
		any_children = node_sequence[i]->any_children_selected;

		node_sequence[i]->all_children_selected =
			e_bit_array_cross_and(node_sequence[i]->all_children_selected_array) &&
			node_sequence[i]->selected;
		node_sequence[i]->any_children_selected =
			e_bit_array_cross_or(node_sequence[i]->any_children_selected_array) ||
			node_sequence[i]->selected;

		if (all_children == node_sequence[i]->all_children_selected &&
		    any_children == node_sequence[i]->any_children_selected)
			break;
	}

	g_free(orig_position_sequence);
	g_free(node_sequence);
}


static void
etsm_change_one_path(ETreeSelectionModel *etsm, ETreePath path, gboolean grow)
{
	ETreeSelectionModelNode *node;

	if (!path)
		return;

	path = e_tree_sorted_model_to_view_path(etsm->priv->ets, path);

	if (!path)
		return;

	node = etsm_find_node_unless_equals (etsm, path, grow);

	if (node) {
		node->selected = grow;
		update_parents(etsm, path);
	}
}

static void
etsm_real_select_single_path (ETreeSelectionModel *etsm, ETreePath path)
{
	etsm_real_clear (etsm);
	etsm_change_one_path(etsm, path, TRUE);
	etsm->priv->selection_start_path = path;
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
}


/* Signal handlers */

static void
etsm_freeze (ETreeSelectionModel *etsm)
{
	etsm->priv->frozen_count ++;
}

static void
etsm_unfreeze (ETreeSelectionModel *etsm)
{
	etsm->priv->frozen_count --;
}

static void
etsm_pre_change (ETreeModel *etm, ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;

	if (e_tree_model_has_get_node_by_id (etm) &&
	    e_tree_model_has_save_id (etm) &&
	    etsm->priv->cursor_path) {
		etsm->priv->cursor_save_id = e_tree_model_get_save_id (etm, etsm->priv->cursor_path);
	}

	etsm_freeze (etsm);
}

static void
etsm_no_change (ETreeModel *etm, ETreeSelectionModel *etsm)
{
	etsm_unfreeze (etsm);
}

static void
clear_tree (ETreeSelectionModel *etsm, ETreeModel *etm)
{
	int cursor_row;

	etsm_real_clear (etsm);
	etsm->priv->cursor_path = NULL;

	if (e_tree_model_has_get_node_by_id (etm) && etsm->priv->cursor_save_id) {
		ETreePath cursor_path;

		cursor_path = e_tree_model_get_node_by_id (etm, etsm->priv->cursor_save_id);
		etsm->priv->cursor_path = cursor_path;
		if (cursor_path != NULL && etsm->priv->cursor_col == -1)
			etsm->priv->cursor_col = 0;

		etsm_real_select_single_path(etsm, cursor_path);
	}

	cursor_row = etsm_cursor_row_real (etsm);
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	if (cursor_row != -1)
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), cursor_row, etsm->priv->cursor_col);
	else {
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
		e_selection_model_cursor_activated(E_SELECTION_MODEL(etsm), -1, -1);
	}

	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;
}

static void
etsm_node_changed (ETreeModel *etm, ETreePath node, ETreeSelectionModel *etsm)
{
	clear_tree (etsm, etm);
	etsm_unfreeze (etsm);
}

static void
etsm_node_data_changed (ETreeModel *etm, ETreePath node, ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;
	etsm_unfreeze (etsm);
}

static void
etsm_node_col_changed (ETreeModel *etm, ETreePath node, int col, ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;
	etsm_unfreeze (etsm);
}

static void
etsm_node_inserted (ETreeModel *etm, ETreePath parent, ETreePath child, ETreeSelectionModel *etsm)
{
	clear_tree (etsm, etm);
	etsm_unfreeze (etsm);
#if 0
	ETreeSelectionModelNode *node;
	ETreePath path;

	path = e_tree_sorted_model_to_view_path(etsm->priv->ets, parent);

	if (!path)
		return;

	node = etsm_find_node_unless_equals (etsm, path, FALSE);

	if (node) {
		node->selected = FALSE;
		update_parents(etsm, path);
	}
#endif
}

static void
etsm_node_removed (ETreeModel *etm, ETreePath parent, ETreePath child, int old_position, ETreeSelectionModel *etsm)
{
}

static void
etsm_node_deleted (ETreeModel *etm, ETreePath child, ETreeSelectionModel *etsm)
{
	clear_tree (etsm, etm);
	etsm_unfreeze (etsm);
}


static void
etsm_sorted_node_resorted (ETreeModel *etm, ETreePath node, ETreeSelectionModel *etsm)
{
	int cursor_row = etsm_cursor_row_real (etsm);

	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));

	if (cursor_row != -1)
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), cursor_row, etsm->priv->cursor_col);
	else
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
}

static void
add_model(ETreeSelectionModel *etsm, ETreeModel *model)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	priv->model = model;

	if (!priv->model)
		return;

	gtk_object_ref(GTK_OBJECT(priv->model));
	priv->tree_model_pre_change_id        = gtk_signal_connect_after (GTK_OBJECT (priv->model), "pre_change",
									  GTK_SIGNAL_FUNC (etsm_pre_change), etsm);
	priv->tree_model_no_change_id         = gtk_signal_connect_after (GTK_OBJECT (priv->model), "no_change",
									  GTK_SIGNAL_FUNC (etsm_no_change), etsm);
	priv->tree_model_node_changed_id      = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_changed",
									  GTK_SIGNAL_FUNC (etsm_node_changed), etsm);
	priv->tree_model_node_data_changed_id = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_data_changed",
									  GTK_SIGNAL_FUNC (etsm_node_data_changed), etsm);
	priv->tree_model_node_col_changed_id  = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_col_changed",
									  GTK_SIGNAL_FUNC (etsm_node_col_changed), etsm);
	priv->tree_model_node_inserted_id     = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_inserted",
									  GTK_SIGNAL_FUNC (etsm_node_inserted), etsm);
	priv->tree_model_node_removed_id      = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_removed",
									  GTK_SIGNAL_FUNC (etsm_node_removed), etsm);
	priv->tree_model_node_deleted_id      = gtk_signal_connect_after (GTK_OBJECT (priv->model), "node_deleted",
									  GTK_SIGNAL_FUNC (etsm_node_deleted), etsm);
}

static void
drop_model(ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	if (!priv->model)
		return;

	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_pre_change_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_no_change_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_data_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_col_changed_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_inserted_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_removed_id);
	gtk_signal_disconnect (GTK_OBJECT (priv->model),
			       priv->tree_model_node_deleted_id);

	gtk_object_unref (GTK_OBJECT (priv->model));
	priv->model = NULL;

	priv->tree_model_pre_change_id = 0;
	priv->tree_model_no_change_id = 0;
	priv->tree_model_node_changed_id = 0;
	priv->tree_model_node_data_changed_id = 0;
	priv->tree_model_node_col_changed_id = 0;
	priv->tree_model_node_inserted_id = 0;
	priv->tree_model_node_removed_id = 0;
	priv->tree_model_node_deleted_id = 0;
}


static void
add_ets(ETreeSelectionModel *etsm, ETreeSorted *ets)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	priv->ets = ets;

	if (!priv->ets)
		return;

	gtk_object_ref(GTK_OBJECT(priv->ets));
	priv->sorted_model_node_resorted_id      = gtk_signal_connect (GTK_OBJECT (priv->ets), "node_resorted",
								       GTK_SIGNAL_FUNC (etsm_sorted_node_resorted), etsm);
}

static void
drop_ets(ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	if (!priv->ets)
		return;

	gtk_signal_disconnect (GTK_OBJECT (priv->ets),
			       priv->sorted_model_node_resorted_id);

	gtk_object_unref (GTK_OBJECT (priv->ets));
	priv->ets = NULL;

	priv->sorted_model_node_resorted_id = 0;
}

/* Virtual functions */
static void
etsm_destroy (GtkObject *object)
{
	ETreeSelectionModel *etsm;

	etsm = E_TREE_SELECTION_MODEL (object);

	etsm_real_clear (etsm);
	etsm->priv->cursor_path = NULL;
	drop_model(etsm);
	drop_ets(etsm);

	g_free (etsm->priv->cursor_save_id);

	g_free(etsm->priv);
	etsm->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
etsm_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_CURSOR_ROW:
		GTK_VALUE_INT(*arg) = etsm_cursor_row_real(etsm);
		break;

	case ARG_CURSOR_COL:
		GTK_VALUE_INT(*arg) = etsm->priv->cursor_col;
		break;

	case ARG_MODEL:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->priv->model;
		break;

	case ARG_ETTA:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->priv->etta;
		break;

	case ARG_ETS:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->priv->ets;
		break;
	}
}

static void
etsm_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectionModel *esm = E_SELECTION_MODEL (o);
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (o);

	switch (arg_id){
	case ARG_CURSOR_ROW:
		e_selection_model_do_something(esm, GTK_VALUE_INT(*arg), etsm->priv->cursor_col, 0);
		break;

	case ARG_CURSOR_COL:
		e_selection_model_do_something(esm, etsm_cursor_row_real(etsm), GTK_VALUE_INT(*arg), 0);
		break;

	case ARG_MODEL:
		drop_model(etsm);
		add_model(etsm, (ETreeModel *) GTK_VALUE_OBJECT(*arg));
		break;

	case ARG_ETTA:
		etsm->priv->etta = (ETreeTableAdapter *) GTK_VALUE_OBJECT(*arg);
		break;

	case ARG_ETS:
		drop_ets(etsm);
		add_ets(etsm, (ETreeSorted *) GTK_VALUE_OBJECT(*arg));
		break;
	}
}

static ETreeSelectionModelNode *
etsm_recurse_is_path_selected (ETreeSelectionModel *etsm,
			       ETreePath path,
			       gboolean *is_selected)
{
	ETreeSelectionModelNode *selection_node;
	ETreeSorted *ets = etsm->priv->ets;
	ETreePath parent;

	parent = e_tree_model_node_get_parent(E_TREE_MODEL(ets), path);

	if (parent) {
		selection_node = etsm_recurse_is_path_selected (etsm, parent, is_selected);
		if (selection_node) {
			int position = e_tree_sorted_orig_position(ets, path);
			if (position < 0 || position >= selection_node->num_children) {
				*is_selected = FALSE;
				return NULL;
			}
			if (selection_node->all_children_selected) {
				*is_selected = TRUE;
				return NULL;
			}
			if (! selection_node->any_children_selected) {
				*is_selected = FALSE;
				return NULL;
			}
			if (selection_node->all_children_selected_array && e_bit_array_value_at(selection_node->all_children_selected_array, position)) {
				*is_selected = TRUE;
				return NULL;
			}
			if (selection_node->any_children_selected_array && ! e_bit_array_value_at(selection_node->any_children_selected_array, position)) {
				*is_selected = FALSE;
				return NULL;
			}
			if (!selection_node->children) {
				*is_selected = FALSE;
				return NULL;
			}
			return selection_node->children[position];
		} else
			  return NULL;
	} else {
		if (etsm->priv->root) {
			return etsm->priv->root;
		} else {
			*is_selected = FALSE;
			return NULL;
		}
	}
}

static gboolean
etsm_is_path_selected (ETreeSelectionModel *etsm,
		       ETreePath path)
{
	ETreeSelectionModelNode *selection_node;
	gboolean ret_val;

	selection_node = etsm_recurse_is_path_selected (etsm, path, &ret_val);

	if (selection_node)
		ret_val = selection_node->selected;

	return ret_val;
}

/** 
 * e_selection_model_is_row_selected
 * @selection: #ESelectionModel to check
 * @n: The row to check
 *
 * This routine calculates whether the given row is selected.
 *
 * Returns: %TRUE if the given row is selected
 */
static gboolean
etsm_is_row_selected (ESelectionModel *selection,
		      gint             row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path;

	g_return_val_if_fail(row < e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta)), FALSE);
	g_return_val_if_fail(row >= 0, FALSE);
	g_return_val_if_fail(etsm != NULL, FALSE);

	path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);
	return etsm_is_path_selected (etsm, path);
}


typedef struct {
	ETreeSelectionModel *etsm;
	EForeachFunc callback;
	gpointer closure;
} ModelAndCallback;

static void
etsm_row_foreach_cb (ETreePath path, gpointer user_data)
{
	ModelAndCallback *mac = user_data;
	int row = etsm_row_of_node(mac->etsm, path);
	if (row >= 0)
		mac->callback(row, mac->closure);
}

/** 
 * e_selection_model_foreach
 * @selection: #ESelectionModel to traverse
 * @callback: The callback function to call back.
 * @closure: The closure
 *
 * This routine calls the given callback function once for each
 * selected row, passing closure as the closure.
 */
static void 
etsm_foreach (ESelectionModel *selection,
	      EForeachFunc     callback,
	      gpointer         closure)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ModelAndCallback mac;

	mac.etsm = etsm;
	mac.callback = callback;
	mac.closure = closure;

	e_tree_selection_model_foreach(etsm, etsm_row_foreach_cb, &mac);
}

/** 
 * e_selection_model_clear
 * @selection: #ESelectionModel to clear
 *
 * This routine clears the selection to no rows selected.
 */
static void
etsm_clear(ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	etsm_real_clear (etsm);

	etsm->priv->cursor_path = NULL;
	etsm->priv->cursor_col = -1;
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
}

/* Standard functions */
static void
etsm_selected_count_all_recurse (ETreeSelectionModel *etsm,
				 ETreePath path,
				 int *count)
{
	ETreePath child;

	(*count) ++;

	child = e_tree_model_node_get_first_child(E_TREE_MODEL(etsm->priv->model), path);
	for ( ; child; child = e_tree_model_node_get_next(E_TREE_MODEL(etsm->priv->model), child))
		if (child)
			etsm_selected_count_all_recurse (etsm, child, count);
}

static void
etsm_selected_count_recurse (ETreeSelectionModel *etsm,
			     ETreeSelectionModelNode *selection_node,
			     ETreePath path,
			     int *count)
{
	if (selection_node->all_children_selected) {
		if (path)
			etsm_selected_count_all_recurse(etsm, path, count);
		return;
	}
	if (!selection_node->any_children_selected)
		return;

	if (selection_node->selected) {
		(*count) ++;
	}

	if (selection_node->children) {
		ETreePath child = e_tree_model_node_get_first_child(E_TREE_MODEL(etsm->priv->model), path);
		int i;
		for (i = 0; child && i < selection_node->num_children; i++, child = e_tree_model_node_get_next(E_TREE_MODEL(etsm->priv->model), child))
			if (selection_node->children[i])
				etsm_selected_count_recurse (etsm, selection_node->children[i], child, count);
	}
}

/** 
 * e_selection_model_selected_count
 * @selection: #ESelectionModel to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
static gint
etsm_selected_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	int count = 0;
	if (etsm->priv->root) {
		ETreePath model_root;
		model_root = e_tree_model_get_root(etsm->priv->model);
		etsm_selected_count_recurse(etsm, etsm->priv->root, model_root, &count);
		if (!e_tree_table_adapter_root_node_is_visible (etsm->priv->etta) && etsm_is_path_selected (etsm, e_tree_model_get_root(etsm->priv->model))) {
			count --;
		}
	}
	return count;
}

/** 
 * e_selection_model_select_all
 * @selection: #ESelectionModel to select all
 *
 * This routine selects all the rows in the given
 * #ESelectionModel.
 */
static void
etsm_select_all (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath root;

	root = e_tree_model_get_root(E_TREE_MODEL(etsm->priv->ets));
	if (root == NULL)
		return;

	etsm_real_clear (etsm);

	etsm->priv->root = e_tree_selection_model_node_new();
	etsm->priv->root->selected = TRUE;
	etsm->priv->root->all_children_selected = TRUE;
	etsm->priv->root->any_children_selected = TRUE;

	e_tree_selection_model_node_fill_children(etsm, root, etsm->priv->root);
	etsm->priv->root->all_children_selected_array = e_bit_array_new(etsm->priv->root->num_children);
	etsm->priv->root->any_children_selected_array = e_bit_array_new(etsm->priv->root->num_children);
	e_bit_array_select_all(etsm->priv->root->all_children_selected_array);
	e_bit_array_select_all(etsm->priv->root->any_children_selected_array);

	if (etsm->priv->cursor_col == -1)
		etsm->priv->cursor_col = 0;
	if (etsm->priv->cursor_path == NULL)
		etsm->priv->cursor_path = etsm_node_at_row(etsm, 0);
	etsm->priv->selection_start_path = etsm_node_at_row(etsm, 0);
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), etsm_cursor_row_real(etsm), etsm->priv->cursor_col);
}

static void
etsm_invert_selection_recurse (ETreeSelectionModel *etsm,
			       ETreeSelectionModelNode *selection_node)
{
	gboolean temp;
	EBitArray *temp_eba;
	selection_node->selected = ! selection_node->selected;

	temp = selection_node->all_children_selected;
	selection_node->all_children_selected = ! selection_node->any_children_selected;
	selection_node->any_children_selected = ! temp;

	temp_eba = selection_node->all_children_selected_array;
	selection_node->all_children_selected_array = selection_node->any_children_selected_array;
	selection_node->any_children_selected_array = temp_eba;
	if (selection_node->all_children_selected_array)
		e_bit_array_invert_selection(selection_node->all_children_selected_array);
	if (selection_node->any_children_selected_array)
		e_bit_array_invert_selection(selection_node->any_children_selected_array);
	if (selection_node->children) {
		int i;
		for (i = 0; i < selection_node->num_children; i++) {
			if (selection_node->children[i])
				etsm_invert_selection_recurse (etsm, selection_node->children[i]);
		}
	}
}

/** 
 * e_selection_model_invert_selection
 * @selection: #ESelectionModel to invert
 *
 * This routine inverts all the rows in the given
 * #ESelectionModel.
 */
static void
etsm_invert_selection (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	if (etsm->priv->root)
		etsm_invert_selection_recurse (etsm, etsm->priv->root);
	
	etsm->priv->cursor_col = -1;
	etsm->priv->cursor_path = NULL;
	etsm->priv->selection_start_path = etsm_node_at_row(etsm, 0);
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
}

static int
etsm_row_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	return e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta));
}

static void
etsm_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreeSelectionModelNode *node;
	ETreePath path;

	g_return_if_fail(row < e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta)));
	g_return_if_fail(row >= 0);
	g_return_if_fail(selection != NULL);

	path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);

	if (!path)
		return;

	node = etsm_find_node_unless_equals (etsm, path, grow);

	if (node) {
		node->selected = grow;
		update_parents(etsm, path);
	}
}

static void
etsm_change_cursor (ESelectionModel *selection, int row, int col)
{
	ETreeSelectionModel *etsm;

	g_return_if_fail(selection != NULL);
	g_return_if_fail(E_IS_SELECTION_MODEL(selection));

	etsm = E_TREE_SELECTION_MODEL(selection);

	if (row == -1) {
		etsm->priv->cursor_path = NULL;
	} else {
		etsm->priv->cursor_path = etsm_node_at_row(etsm, row);
	}
	etsm->priv->cursor_col = col;
}

static void
etsm_change_range(ESelectionModel *selection, int start, int end, gboolean grow)
{
	int i;
	if (start != end) {
		if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
			for ( i = start; i < end; i++) {
				e_selection_model_change_one_row(selection, e_sorter_sorted_to_model(selection->sorter, i), grow);
			}
		} else {
			for ( i = start; i < end; i++) {
				e_selection_model_change_one_row(selection, i, grow);
			}
		}
	}
}

static int
etsm_cursor_row (ESelectionModel *selection)
{
	return etsm_cursor_row_real(E_TREE_SELECTION_MODEL(selection));
}

static int
etsm_cursor_col (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	return etsm->priv->cursor_col;
}

static void
etsm_select_single_row (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	int selected_row = etsm->priv->selected_row;

	etsm_real_clear (etsm);
	etsm_change_one_row(selection, row, TRUE);
	etsm->priv->selection_start_path = etsm_node_at_row(etsm, row);

	etsm->priv->selected_row = row;
	etsm->priv->selected_range_end = -1;
	if (selected_row != -1) {
		if (selected_row != etsm->priv->selected_row) {
			e_selection_model_selection_row_changed(E_SELECTION_MODEL(etsm), selected_row);
			e_selection_model_selection_row_changed(E_SELECTION_MODEL(etsm), row);
		}
	} else {
		e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	}
}

static void
etsm_toggle_single_row (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	etsm->priv->selection_start_path = etsm_node_at_row(etsm, row);

	etsm_change_one_row(selection, row, !etsm_is_row_selected(selection, row));

	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

static void
etsm_real_move_selection_end (ETreeSelectionModel *etsm, int row)
{
	ESelectionModel *selection = E_SELECTION_MODEL (etsm);
	int old_start;
	int old_end;
	int new_start;
	int new_end;
	int start_row = etsm_row_of_node(etsm, etsm->priv->selection_start_path);
	if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
		old_start = MIN (e_sorter_model_to_sorted(selection->sorter, start_row),
				 e_sorter_model_to_sorted(selection->sorter, etsm_cursor_row_real(etsm)));
		old_end = MAX (e_sorter_model_to_sorted(selection->sorter, start_row),
			       e_sorter_model_to_sorted(selection->sorter, etsm_cursor_row_real(etsm))) + 1;
		new_start = MIN (e_sorter_model_to_sorted(selection->sorter, start_row),
				 e_sorter_model_to_sorted(selection->sorter, row));
		new_end = MAX (e_sorter_model_to_sorted(selection->sorter, start_row),
			       e_sorter_model_to_sorted(selection->sorter, row)) + 1;
	} else {
		old_start = MIN (start_row, etsm_cursor_row_real(etsm));
		old_end = MAX (start_row, etsm_cursor_row_real(etsm)) + 1;
		new_start = MIN (start_row, row);
		new_end = MAX (start_row, row) + 1;
	}
	/* This wouldn't work nearly so smoothly if one end of the selection weren't held in place. */
	if (old_start < new_start)
		etsm_change_range(selection, old_start, new_start, FALSE);
	if (new_start < old_start)
		etsm_change_range(selection, new_start, old_start, TRUE);
	if (old_end < new_end)
		etsm_change_range(selection, old_end, new_end, TRUE);
	if (new_end < old_end)
		etsm_change_range(selection, new_end, old_end, FALSE);
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;
}

static void
etsm_move_selection_end (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	etsm_real_move_selection_end (etsm, row);
	e_selection_model_selection_changed(E_SELECTION_MODEL(selection));
}

static void
etsm_set_selection_end (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	int selected_range_end = etsm->priv->selected_range_end;

	etsm_real_select_single_path(etsm, etsm->priv->selection_start_path);
	etsm->priv->cursor_path = etsm->priv->selection_start_path;
	etsm_real_move_selection_end(etsm, row);
	etsm->priv->selected_range_end = row;
	if (selected_range_end != -1 && row != -1) {
		if (selected_range_end == row - 1 ||
		    selected_range_end == row + 1) {
			e_selection_model_selection_row_changed(E_SELECTION_MODEL(etsm), selected_range_end);
			e_selection_model_selection_row_changed(E_SELECTION_MODEL(etsm), row);
			return;
		}
	}
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}


/* Standard functions */
static void
etsm_foreach_all_recurse (ETreeSelectionModel *etsm,
			  ETreePath path,
			  ETreeForeachFunc callback,
			  gpointer closure)
{
	ETreePath child;

	callback(path, closure);

	child = e_tree_model_node_get_first_child(E_TREE_MODEL(etsm->priv->model), path);
	for ( ; child; child = e_tree_model_node_get_next(E_TREE_MODEL(etsm->priv->model), child))
		if (child)
			etsm_foreach_all_recurse (etsm, child, callback, closure);
}

static void
etsm_foreach_recurse (ETreeSelectionModel *etsm,
		      ETreeSelectionModelNode *selection_node,
		      ETreePath path,
		      ETreeForeachFunc callback,
		      gpointer closure)
{
	if (selection_node->all_children_selected) {
		if (path)
			etsm_foreach_all_recurse(etsm, path, callback, closure);
		return;
	}
	if (!selection_node->any_children_selected)
		return;

	if (selection_node->selected) {
		callback(path, closure);
	}

	if (selection_node->children) {
		ETreePath child = e_tree_model_node_get_first_child(E_TREE_MODEL(etsm->priv->model), path);
		int i;
		for (i = 0; i < selection_node->num_children; i++, child = e_tree_model_node_get_next(E_TREE_MODEL(etsm->priv->model), child))
			if (selection_node->children[i])
				etsm_foreach_recurse (etsm, selection_node->children[i], child, callback, closure);
	}
}

void
e_tree_selection_model_foreach   (ETreeSelectionModel *etsm,
				  ETreeForeachFunc     callback,
				  gpointer             closure)
{
	if (etsm->priv->root) {
		ETreePath model_root;
		model_root = e_tree_model_get_root(etsm->priv->model);
		etsm_foreach_recurse(etsm, etsm->priv->root, model_root, callback, closure);
	}
}

void
e_tree_selection_model_select_single_path (ETreeSelectionModel *etsm, ETreePath path)
{
	etsm_real_select_single_path (etsm, path);

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

void
e_tree_selection_model_add_to_selection (ETreeSelectionModel *etsm, ETreePath path)
{
	etsm_change_one_path(etsm, path, TRUE);
	etsm->priv->selection_start_path = path;
	etsm->priv->selected_row = -1;
	etsm->priv->selected_range_end = -1;

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

void
e_tree_selection_model_change_cursor (ETreeSelectionModel *etsm, ETreePath path)
{
	int row;

	etsm->priv->cursor_path = path;

	row = etsm_cursor_row_real(etsm);

	E_SELECTION_MODEL (etsm)->old_selection = -1;

	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), row, etsm->priv->cursor_col);
	e_selection_model_cursor_activated(E_SELECTION_MODEL(etsm), row, etsm->priv->cursor_col);
}


static void
e_tree_selection_model_init (ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPriv *priv;
	priv                                  = g_new(ETreeSelectionModelPriv, 1);
	etsm->priv                            = priv;

	priv->etta                            = NULL;
	priv->ets                             = NULL;
	priv->model                           = NULL;

	priv->root                            = NULL;

	priv->cursor_path                     = NULL;
	priv->cursor_col                      = -1;
	priv->selection_start_path            = NULL;

	priv->cursor_save_id                  = NULL;


	priv->tree_model_pre_change_id        = 0;
	priv->tree_model_no_change_id         = 0;
	priv->tree_model_node_changed_id      = 0;
	priv->tree_model_node_data_changed_id = 0;
	priv->tree_model_node_col_changed_id  = 0;
	priv->tree_model_node_inserted_id     = 0;
	priv->tree_model_node_removed_id      = 0;
	priv->tree_model_node_deleted_id      = 0;

	priv->sorted_model_node_resorted_id   = 0;

	priv->selected_row                    = -1;
	priv->selected_range_end              = -1;
}

static void
e_tree_selection_model_class_init (ETreeSelectionModelClass *klass)
{
	GtkObjectClass *object_class;
	ESelectionModelClass *esm_class;

	parent_class = gtk_type_class (e_selection_model_get_type ());

	object_class = GTK_OBJECT_CLASS(klass);
	esm_class = E_SELECTION_MODEL_CLASS(klass);

	object_class->destroy = etsm_destroy;
	object_class->get_arg = etsm_get_arg;
	object_class->set_arg = etsm_set_arg;

	esm_class->is_row_selected    = etsm_is_row_selected    ;
	esm_class->foreach            = etsm_foreach            ;
	esm_class->clear              = etsm_clear              ;
	esm_class->selected_count     = etsm_selected_count     ;
	esm_class->select_all         = etsm_select_all         ;
	esm_class->invert_selection   = etsm_invert_selection   ;
	esm_class->row_count          = etsm_row_count          ;

	esm_class->change_one_row     = etsm_change_one_row     ;
	esm_class->change_cursor      = etsm_change_cursor      ;
	esm_class->cursor_row         = etsm_cursor_row         ;
	esm_class->cursor_col         = etsm_cursor_col         ;

	esm_class->select_single_row  = etsm_select_single_row  ;
	esm_class->toggle_single_row  = etsm_toggle_single_row  ;
	esm_class->move_selection_end = etsm_move_selection_end ;
	esm_class->set_selection_end  = etsm_set_selection_end  ;

	gtk_object_add_arg_type ("ETreeSelectionModel::cursor_row", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_ROW);
	gtk_object_add_arg_type ("ETreeSelectionModel::cursor_col", GTK_TYPE_INT,
				 GTK_ARG_READWRITE, ARG_CURSOR_COL);
	gtk_object_add_arg_type ("ETreeSelectionModel::model", E_TREE_MODEL_TYPE,
				 GTK_ARG_READWRITE, ARG_MODEL);
	gtk_object_add_arg_type ("ETreeSelectionModel::etta", E_TREE_TABLE_ADAPTER_TYPE,
				 GTK_ARG_READWRITE, ARG_ETTA);
	gtk_object_add_arg_type ("ETreeSelectionModel::ets", E_TREE_SORTED_TYPE,
				 GTK_ARG_READWRITE, ARG_ETS);
}

ESelectionModel *
e_tree_selection_model_new (void)
{
	return gtk_type_new(e_tree_selection_model_get_type());
}

E_MAKE_TYPE(e_tree_selection_model, "ETreeSelectionModel", ETreeSelectionModel,
	    e_tree_selection_model_class_init, e_tree_selection_model_init, PARENT_TYPE);

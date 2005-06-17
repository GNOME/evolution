/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-selection-model.c
 * Copyright 2000, 2001, 2003 Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Mike Kestner <mkestner@ximian.com>
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

#include "table/e-tree-table-adapter.h"
#include "e-util/e-i18n.h"
#include "e-util/e-util.h"

#include "e-tree-selection-model.h"

#define PARENT_TYPE e_selection_model_get_type ()

static ESelectionModelClass *parent_class;

enum {
	PROP_0,
	PROP_CURSOR_ROW,
	PROP_CURSOR_COL,
	PROP_MODEL,
	PROP_ETTA,
};

struct ETreeSelectionModelPriv {
	ETreeTableAdapter *etta;
	ETreeModel *model;

	GHashTable *paths;
	ETreePath cursor_path;
	ETreePath start_path;
	gint cursor_col;
	gchar *cursor_save_id;

	gint tree_model_pre_change_id;
	gint tree_model_no_change_id;
	gint tree_model_node_changed_id;
	gint tree_model_node_data_changed_id;
	gint tree_model_node_col_changed_id;
	gint tree_model_node_inserted_id;
	gint tree_model_node_removed_id;
	gint tree_model_node_deleted_id;
};

static gint
get_cursor_row (ETreeSelectionModel *etsm)
{
	if (etsm->priv->cursor_path)
		return e_tree_table_adapter_row_of_node(etsm->priv->etta, etsm->priv->cursor_path);

	return -1;
}

static void
clear_selection (ETreeSelectionModel *etsm)
{
	g_hash_table_destroy (etsm->priv->paths);
	etsm->priv->paths = g_hash_table_new (NULL, NULL);
}

static void
change_one_path (ETreeSelectionModel *etsm, ETreePath path, gboolean grow)
{
	if (!path)
		return;

	if (grow)
		g_hash_table_insert (etsm->priv->paths, path, path);
	else if (g_hash_table_lookup (etsm->priv->paths, path))
		g_hash_table_remove (etsm->priv->paths, path);
}

static void
select_single_path (ETreeSelectionModel *etsm, ETreePath path)
{
	clear_selection (etsm);
	change_one_path(etsm, path, TRUE);
	etsm->priv->cursor_path = path;
	etsm->priv->start_path = NULL;
}

static void
select_range (ETreeSelectionModel *etsm, gint start, gint end)
{
	gint i;

	if (start > end) {
		i = start;
		start = end;
		end = i;
	}

	for (i = start; i <= end; i++) {
		ETreePath path = e_tree_table_adapter_node_at_row (etsm->priv->etta, i);
		if (path)
			g_hash_table_insert (etsm->priv->paths, path, path);
	}
}

static void
free_id (ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;
}

static void
restore_cursor (ETreeSelectionModel *etsm, ETreeModel *etm)
{
	clear_selection (etsm);
	etsm->priv->cursor_path = NULL;

	if (etsm->priv->cursor_save_id) {
		etsm->priv->cursor_path = e_tree_model_get_node_by_id (etm, etsm->priv->cursor_save_id);
		if (etsm->priv->cursor_path != NULL && etsm->priv->cursor_col == -1)
			etsm->priv->cursor_col = 0;

		select_single_path(etsm, etsm->priv->cursor_path);
	}

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));

	if (etsm->priv->cursor_path) {
		gint cursor_row = get_cursor_row (etsm);
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), cursor_row, etsm->priv->cursor_col);
	} else {
		e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
		e_selection_model_cursor_activated(E_SELECTION_MODEL(etsm), -1, -1);

	}

	free_id (etsm);
}

static void
etsm_pre_change (ETreeModel *etm, ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;

	if (e_tree_model_has_get_node_by_id (etm) && e_tree_model_has_save_id (etm) && etsm->priv->cursor_path) {
		etsm->priv->cursor_save_id = e_tree_model_get_save_id (etm, etsm->priv->cursor_path);
	}
}

static void
etsm_no_change (ETreeModel *etm, ETreeSelectionModel *etsm)
{
	free_id (etsm);
}

static void
etsm_node_changed (ETreeModel *etm, ETreePath node, ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_data_changed (ETreeModel *etm, ETreePath node, ETreeSelectionModel *etsm)
{
	free_id (etsm);
}

static void
etsm_node_col_changed (ETreeModel *etm, ETreePath node, int col, ETreeSelectionModel *etsm)
{
	free_id (etsm);
}

static void
etsm_node_inserted (ETreeModel *etm, ETreePath parent, ETreePath child, ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_removed (ETreeModel *etm, ETreePath parent, ETreePath child, int old_position, ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_deleted (ETreeModel *etm, ETreePath child, ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}


static void
add_model(ETreeSelectionModel *etsm, ETreeModel *model)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	priv->model = model;

	if (!priv->model)
		return;

	g_object_ref(priv->model);
	priv->tree_model_pre_change_id      = g_signal_connect_after (G_OBJECT (priv->model), "pre_change",
									G_CALLBACK (etsm_pre_change), etsm);
	priv->tree_model_no_change_id      = g_signal_connect_after (G_OBJECT (priv->model), "no_change",
									G_CALLBACK (etsm_no_change), etsm);
	priv->tree_model_node_changed_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_changed",
									G_CALLBACK (etsm_node_changed), etsm);
	priv->tree_model_node_data_changed_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_data_changed",
									G_CALLBACK (etsm_node_data_changed), etsm);
	priv->tree_model_node_col_changed_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_col_changed",
									G_CALLBACK (etsm_node_col_changed), etsm);
	priv->tree_model_node_inserted_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_inserted",
									G_CALLBACK (etsm_node_inserted), etsm);
	priv->tree_model_node_removed_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_removed",
									G_CALLBACK (etsm_node_removed), etsm);
	priv->tree_model_node_deleted_id      = g_signal_connect_after (G_OBJECT (priv->model), "node_deleted",
									G_CALLBACK (etsm_node_deleted), etsm);
}

static void
drop_model(ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPriv *priv = etsm->priv;

	if (!priv->model)
		return;

	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_pre_change_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_no_change_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_changed_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_data_changed_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_col_changed_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_inserted_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_removed_id);
	g_signal_handler_disconnect (G_OBJECT (priv->model),
			             priv->tree_model_node_deleted_id);

	g_object_unref (priv->model);
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
etsm_dispose (GObject *object)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	drop_model(etsm);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
etsm_finalize (GObject *object)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	if (etsm->priv) {
		clear_selection (etsm);
		g_hash_table_destroy (etsm->priv->paths);
		g_free (etsm->priv);
		etsm->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
etsm_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	switch (prop_id){
	case PROP_CURSOR_ROW:
		g_value_set_int (value, get_cursor_row(etsm));
		break;

	case PROP_CURSOR_COL:
		g_value_set_int (value, etsm->priv->cursor_col);
		break;

	case PROP_MODEL:
		g_value_set_object (value, etsm->priv->model);
		break;

	case PROP_ETTA:
		g_value_set_object (value, etsm->priv->etta);
		break;
	}
}

static void
etsm_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	switch (prop_id){
	case PROP_CURSOR_ROW:
		e_selection_model_do_something(esm, g_value_get_int (value), etsm->priv->cursor_col, 0);
		break;

	case PROP_CURSOR_COL:
		e_selection_model_do_something(esm, get_cursor_row(etsm), g_value_get_int(value), 0);
		break;

	case PROP_MODEL:
		drop_model(etsm);
		add_model(etsm, E_TREE_MODEL (g_value_get_object(value)));
		break;

	case PROP_ETTA:
		etsm->priv->etta = E_TREE_TABLE_ADAPTER (g_value_get_object (value));
		break;
	}
}

static gboolean
etsm_is_path_selected (ETreeSelectionModel *etsm, ETreePath path)
{
	if (path && g_hash_table_lookup (etsm->priv->paths, path))
		return TRUE;

	return FALSE;
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
etsm_row_foreach_cb (gpointer key, gpointer value, gpointer user_data)
{
	ETreePath path = key;
	ModelAndCallback *mac = user_data;
	int row = e_tree_table_adapter_row_of_node(mac->etsm->priv->etta, path);
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

	g_hash_table_foreach(etsm->priv->paths, etsm_row_foreach_cb, &mac);
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

	clear_selection (etsm);

	etsm->priv->cursor_path = NULL;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
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

	return g_hash_table_size (etsm->priv->paths);
}

static int
etsm_row_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	return e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta));
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

	root = e_tree_model_get_root(etsm->priv->model);
	if (root == NULL)
		return;

	clear_selection (etsm);
	select_range (etsm, 0, etsm_row_count (selection) - 1);

	if (etsm->priv->cursor_path == NULL)
		etsm->priv->cursor_path = e_tree_table_adapter_node_at_row(etsm->priv->etta, 0);

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), get_cursor_row(etsm), etsm->priv->cursor_col);
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
        gint count = etsm_row_count (selection);
	gint i;

	for (i = 0; i < count; i++) {
		ETreePath path = e_tree_table_adapter_node_at_row (etsm->priv->etta, i);
		if (!path)
			continue;
		if (g_hash_table_lookup (etsm->priv->paths, path))
			g_hash_table_remove (etsm->priv->paths, path);
		else
			g_hash_table_insert (etsm->priv->paths, path, path);
	}

	etsm->priv->cursor_col = -1;
	etsm->priv->cursor_path = NULL;
	etsm->priv->start_path = NULL;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
}

static void
etsm_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path;

	g_return_if_fail(row < e_table_model_row_count(E_TABLE_MODEL(etsm->priv->etta)));
	g_return_if_fail(row >= 0);
	g_return_if_fail(selection != NULL);

	path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);

	if (!path)
		return;

	change_one_path (etsm, path, grow);
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
		etsm->priv->cursor_path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);
	}
	etsm->priv->cursor_col = col;
}

static gint
etsm_cursor_row (ESelectionModel *selection)
{
	return get_cursor_row(E_TREE_SELECTION_MODEL(selection));
}

static gint
etsm_cursor_col (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	return etsm->priv->cursor_col;
}

static void
etsm_select_single_row (ESelectionModel *selection, gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);

	g_return_if_fail (path != NULL);

	select_single_path (etsm, path);

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

static void
etsm_toggle_single_row (ESelectionModel *selection, gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path = e_tree_table_adapter_node_at_row(etsm->priv->etta, row);

	g_return_if_fail (path);

	if (g_hash_table_lookup (etsm->priv->paths, path))
		g_hash_table_remove (etsm->priv->paths, path);
	else
		g_hash_table_insert (etsm->priv->paths, path, path);

	etsm->priv->start_path = NULL;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

static void
etsm_real_move_selection_end (ETreeSelectionModel *etsm, gint row)
{
	ETreePath end_path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);
	gint start;

	g_return_if_fail (end_path);

	start = e_tree_table_adapter_row_of_node(etsm->priv->etta, etsm->priv->start_path);
	clear_selection (etsm);
	select_range (etsm, start, row);
}

static void
etsm_move_selection_end (ESelectionModel *selection, gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	g_return_if_fail (etsm->priv->cursor_path);

	etsm_real_move_selection_end (etsm, row);
	e_selection_model_selection_changed(E_SELECTION_MODEL(selection));
}

static void
etsm_set_selection_end (ESelectionModel *selection, gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	g_return_if_fail (etsm->priv->cursor_path);

	if (!etsm->priv->start_path)
		etsm->priv->start_path = etsm->priv->cursor_path;
	etsm_real_move_selection_end(etsm, row);
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

struct foreach_path_t {
	ETreeForeachFunc callback;
	gpointer closure;
};

static void
foreach_path (gpointer key, gpointer value, gpointer data)
{
	ETreePath path = key;
	struct foreach_path_t *c = data;
	c->callback (path, c->closure);
}

void
e_tree_selection_model_foreach (ETreeSelectionModel *etsm, ETreeForeachFunc callback, gpointer closure)
{
	if (etsm->priv->paths) {
		struct foreach_path_t c;
		c.callback = callback;
		c.closure = closure;
		g_hash_table_foreach(etsm->priv->paths, foreach_path, &c);
		return;
	}
}

void
e_tree_selection_model_select_single_path (ETreeSelectionModel *etsm, ETreePath path)
{
	select_single_path (etsm, path);

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

void
e_tree_selection_model_select_paths (ETreeSelectionModel *etsm, GPtrArray *paths)
{
	ETreePath path;
	int i;

	for (i=0;i<paths->len;i++) {
		path = paths->pdata[i];
		change_one_path(etsm, path, TRUE);
	}

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

void
e_tree_selection_model_add_to_selection (ETreeSelectionModel *etsm, ETreePath path)
{
	change_one_path(etsm, path, TRUE);

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

void
e_tree_selection_model_change_cursor (ETreeSelectionModel *etsm, ETreePath path)
{
	int row;

	etsm->priv->cursor_path = path;

	row = get_cursor_row(etsm);

	E_SELECTION_MODEL (etsm)->old_selection = -1;

	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), row, etsm->priv->cursor_col);
	e_selection_model_cursor_activated(E_SELECTION_MODEL(etsm), row, etsm->priv->cursor_col);
}

ETreePath
e_tree_selection_model_get_cursor (ETreeSelectionModel *etsm)
{
	return etsm->priv->cursor_path;
}


static void
e_tree_selection_model_init (ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPriv *priv;
	priv                                  = g_new(ETreeSelectionModelPriv, 1);
	etsm->priv                            = priv;

	priv->etta                            = NULL;
	priv->model                           = NULL;

	priv->paths                           = g_hash_table_new (NULL, NULL);

	priv->cursor_path                     = NULL;
	priv->start_path                      = NULL;
	priv->cursor_col                      = -1;
	priv->cursor_save_id		      = NULL;

	priv->tree_model_pre_change_id        = 0;
	priv->tree_model_no_change_id         = 0;
	priv->tree_model_node_changed_id      = 0;
	priv->tree_model_node_data_changed_id = 0;
	priv->tree_model_node_col_changed_id  = 0;
	priv->tree_model_node_inserted_id     = 0;
	priv->tree_model_node_removed_id      = 0;
	priv->tree_model_node_deleted_id      = 0;
}

static void
e_tree_selection_model_class_init (ETreeSelectionModelClass *klass)
{
	GObjectClass *object_class;
	ESelectionModelClass *esm_class;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class = G_OBJECT_CLASS(klass);
	esm_class = E_SELECTION_MODEL_CLASS(klass);

	object_class->dispose = etsm_dispose;
	object_class->finalize = etsm_finalize;
	object_class->get_property = etsm_get_property;
	object_class->set_property = etsm_set_property;

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

	g_object_class_install_property (object_class, PROP_CURSOR_ROW, 
					 g_param_spec_int ("cursor_row",
							   _("Cursor Row"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CURSOR_COL, 
					 g_param_spec_int ("cursor_col",
							   _("Cursor Column"),
							   /*_( */"XXX blurb" /*)*/,
							   0, G_MAXINT, 0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MODEL, 
					 g_param_spec_object ("model",
							      _("Model"),
							      "XXX blurb",
							      E_TREE_MODEL_TYPE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_ETTA, 
					 g_param_spec_object ("etta",
							      "ETTA",
							      "XXX blurb",
							      E_TREE_TABLE_ADAPTER_TYPE,
							      G_PARAM_READWRITE));

}

ESelectionModel *
e_tree_selection_model_new (void)
{
	return g_object_new (E_TREE_SELECTION_MODEL_TYPE, NULL);
}

E_MAKE_TYPE(e_tree_selection_model, "ETreeSelectionModel", ETreeSelectionModel,
	    e_tree_selection_model_class_init, e_tree_selection_model_init, PARENT_TYPE)

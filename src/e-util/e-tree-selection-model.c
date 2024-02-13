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
 *		Mike Kestner <mkestner@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-tree-selection-model.h"

#include <glib/gi18n.h>

#include "e-tree-table-adapter.h"

enum {
	PROP_0,
	PROP_CURSOR_ROW,
	PROP_CURSOR_COL,
	PROP_MODEL,
	PROP_ETTA
};

struct _ETreeSelectionModelPrivate {
	ETreeTableAdapter *etta;
	ETreeModel *model;

	GHashTable *paths;
	ETreePath cursor_path;
	ETreePath start_path;
	gint cursor_col;
	gchar *cursor_save_id;

	gint tree_model_pre_change_id;
	gint tree_model_node_changed_id;
	gint tree_model_node_data_changed_id;
	gint tree_model_node_inserted_id;
	gint tree_model_node_removed_id;
	gint tree_model_node_deleted_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (ETreeSelectionModel, e_tree_selection_model, E_TYPE_SELECTION_MODEL)

static gint
get_cursor_row (ETreeSelectionModel *etsm)
{
	if (etsm->priv->cursor_path)
		return e_tree_table_adapter_row_of_node (
			etsm->priv->etta, etsm->priv->cursor_path);

	return -1;
}

static void
clear_selection (ETreeSelectionModel *etsm)
{
	g_hash_table_remove_all (etsm->priv->paths);
}

static void
change_one_path (ETreeSelectionModel *etsm,
                 ETreePath path,
                 gboolean grow)
{
	if (path == NULL)
		return;

	if (grow)
		g_hash_table_add (etsm->priv->paths, path);
	else
		g_hash_table_remove (etsm->priv->paths, path);
}

static void
select_single_path (ETreeSelectionModel *etsm,
                    ETreePath path)
{
	clear_selection (etsm);
	change_one_path (etsm, path, TRUE);
	etsm->priv->cursor_path = path;
	etsm->priv->start_path = NULL;
}

static void
select_range (ETreeSelectionModel *etsm,
              gint start,
              gint end)
{
	gint i;

	if (start > end) {
		i = start;
		start = end;
		end = i;
	}

	for (i = start; i <= end; i++) {
		ETreePath path = e_tree_table_adapter_node_at_row (etsm->priv->etta, i);
		if (path != NULL)
			g_hash_table_add (etsm->priv->paths, path);
	}
}

static void
free_id (ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;
}

static void
restore_cursor (ETreeSelectionModel *etsm,
                ETreeModel *etm)
{
	clear_selection (etsm);
	etsm->priv->cursor_path = NULL;

	if (etsm->priv->cursor_save_id) {
		etsm->priv->cursor_path = e_tree_model_get_node_by_id (
			etm, etsm->priv->cursor_save_id);
		if (etsm->priv->cursor_path != NULL && etsm->priv->cursor_col == -1)
			etsm->priv->cursor_col = 0;

		select_single_path (etsm, etsm->priv->cursor_path);
	}

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));

	if (etsm->priv->cursor_path) {
		gint cursor_row = get_cursor_row (etsm);
		e_selection_model_cursor_changed (
			E_SELECTION_MODEL (etsm),
			cursor_row, etsm->priv->cursor_col);
	} else {
		e_selection_model_cursor_changed (
			E_SELECTION_MODEL (etsm), -1, -1);
		e_selection_model_cursor_activated (
			E_SELECTION_MODEL (etsm), -1, -1);

	}

	free_id (etsm);
}

static void
etsm_pre_change (ETreeModel *etm,
                 ETreeSelectionModel *etsm)
{
	g_free (etsm->priv->cursor_save_id);
	etsm->priv->cursor_save_id = NULL;

	if (etsm->priv->cursor_path != NULL)
		etsm->priv->cursor_save_id = e_tree_model_get_save_id (
			etm, etsm->priv->cursor_path);
}

static void
etsm_node_changed (ETreeModel *etm,
                   ETreePath node,
                   ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_data_changed (ETreeModel *etm,
                        ETreePath node,
                        ETreeSelectionModel *etsm)
{
	free_id (etsm);
}

static void
etsm_node_inserted (ETreeModel *etm,
                    ETreePath parent,
                    ETreePath child,
                    ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_removed (ETreeModel *etm,
                   ETreePath parent,
                   ETreePath child,
                   gint old_position,
                   ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
etsm_node_deleted (ETreeModel *etm,
                   ETreePath child,
                   ETreeSelectionModel *etsm)
{
	restore_cursor (etsm, etm);
}

static void
add_model (ETreeSelectionModel *etsm,
           ETreeModel *model)
{
	ETreeSelectionModelPrivate *priv = etsm->priv;

	priv->model = model;

	if (!priv->model)
		return;

	g_object_ref (priv->model);

	priv->tree_model_pre_change_id = g_signal_connect_after (
		priv->model, "pre_change",
		G_CALLBACK (etsm_pre_change), etsm);

	priv->tree_model_node_changed_id = g_signal_connect_after (
		priv->model, "node_changed",
		G_CALLBACK (etsm_node_changed), etsm);

	priv->tree_model_node_data_changed_id = g_signal_connect_after (
		priv->model, "node_data_changed",
		G_CALLBACK (etsm_node_data_changed), etsm);

	priv->tree_model_node_inserted_id = g_signal_connect_after (
		priv->model, "node_inserted",
		G_CALLBACK (etsm_node_inserted), etsm);

	priv->tree_model_node_removed_id = g_signal_connect_after (
		priv->model, "node_removed",
		G_CALLBACK (etsm_node_removed), etsm);

	priv->tree_model_node_deleted_id = g_signal_connect_after (
		priv->model, "node_deleted",
		G_CALLBACK (etsm_node_deleted), etsm);
}

static void
drop_model (ETreeSelectionModel *etsm)
{
	ETreeSelectionModelPrivate *priv = etsm->priv;

	if (!priv->model)
		return;

	g_signal_handler_disconnect (
		priv->model, priv->tree_model_pre_change_id);
	g_signal_handler_disconnect (
		priv->model, priv->tree_model_node_changed_id);
	g_signal_handler_disconnect (
		priv->model, priv->tree_model_node_data_changed_id);
	g_signal_handler_disconnect (
		priv->model, priv->tree_model_node_inserted_id);
	g_signal_handler_disconnect (
		priv->model, priv->tree_model_node_removed_id);
	g_signal_handler_disconnect (
		priv->model, priv->tree_model_node_deleted_id);

	g_object_unref (priv->model);
	priv->model = NULL;

	priv->tree_model_pre_change_id = 0;
	priv->tree_model_node_changed_id = 0;
	priv->tree_model_node_data_changed_id = 0;
	priv->tree_model_node_inserted_id = 0;
	priv->tree_model_node_removed_id = 0;
	priv->tree_model_node_deleted_id = 0;
}

static void
tree_selection_model_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	ESelectionModel *esm = E_SELECTION_MODEL (object);
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	switch (property_id) {
	case PROP_CURSOR_ROW:
		e_selection_model_do_something (
			esm, g_value_get_int (value),
			etsm->priv->cursor_col, 0);
		break;

	case PROP_CURSOR_COL:
		e_selection_model_do_something (
			esm, get_cursor_row (etsm),
			g_value_get_int (value), 0);
		break;

	case PROP_MODEL:
		drop_model (etsm);
		add_model (etsm, E_TREE_MODEL (g_value_get_object (value)));
		break;

	case PROP_ETTA:
		etsm->priv->etta =
			E_TREE_TABLE_ADAPTER (g_value_get_object (value));
		break;
	}
}

static void
tree_selection_model_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	switch (property_id) {
	case PROP_CURSOR_ROW:
		g_value_set_int (value, get_cursor_row (etsm));
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
tree_selection_model_dispose (GObject *object)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (object);

	drop_model (etsm);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_tree_selection_model_parent_class)->dispose (object);
}

static void
tree_selection_model_finalize (GObject *object)
{
	ETreeSelectionModel *self = E_TREE_SELECTION_MODEL (object);

	clear_selection (self);
	g_hash_table_destroy (self->priv->paths);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_tree_selection_model_parent_class)->finalize (object);
}

static gboolean
tree_selection_model_is_row_selected (ESelectionModel *selection,
                                      gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	ETreePath path;

	g_return_val_if_fail (
		row < e_selection_model_row_count (selection), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (etsm != NULL, FALSE);

	path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);

	if (path == NULL)
		return FALSE;

	return g_hash_table_contains (etsm->priv->paths, path);
}

static void
tree_selection_model_foreach (ESelectionModel *selection,
                              EForeachFunc callback,
                              gpointer closure)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	GList *list, *link;

	list = g_hash_table_get_keys (etsm->priv->paths);

	for (link = list; link != NULL; link = g_list_next (link)) {
		gint row;

		row = e_tree_table_adapter_row_of_node (
			etsm->priv->etta, (ETreePath) link->data);
		if (row >= 0)
			callback (row, closure);
	}

	g_list_free (list);
}

static void
tree_selection_model_clear (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	clear_selection (etsm);

	etsm->priv->cursor_path = NULL;
	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
	e_selection_model_cursor_changed (E_SELECTION_MODEL (etsm), -1, -1);
}

static gint
tree_selection_model_selected_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	return g_hash_table_size (etsm->priv->paths);
}

/* Helper for tree_selection_model_select_all() */
static gboolean
tree_selection_model_traverse_cb (ETreeModel *tree_model,
                                  ETreePath path,
                                  gpointer user_data)
{
	ETreeSelectionModel *etsm;

	etsm = E_TREE_SELECTION_MODEL (user_data);
	g_hash_table_add (etsm->priv->paths, path);

	return FALSE;
}

static void
tree_selection_model_select_all (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	ETreePath root;

	root = e_tree_model_get_root (etsm->priv->model);
	if (root == NULL)
		return;

	clear_selection (etsm);

	/* We want to select ALL rows regardless of expanded state.
	 * ETreeTableAdapter pretends that collapsed rows don't exist,
	 * so instead we need to iterate over the ETreeModel directly. */

	e_tree_model_node_traverse (
		etsm->priv->model, root,
		tree_selection_model_traverse_cb,
		selection);

	if (etsm->priv->cursor_path == NULL)
		etsm->priv->cursor_path = e_tree_table_adapter_node_at_row (
			etsm->priv->etta, 0);

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));

	e_selection_model_cursor_changed (
		E_SELECTION_MODEL (etsm),
		get_cursor_row (etsm), etsm->priv->cursor_col);
}

static gint
tree_selection_model_row_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	return e_table_model_row_count (E_TABLE_MODEL (etsm->priv->etta));
}

static void
tree_selection_model_change_one_row (ESelectionModel *selection,
                                     gint row,
                                     gboolean grow)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	ETreePath path;

	g_return_if_fail (
		row < e_table_model_row_count (
		E_TABLE_MODEL (etsm->priv->etta)));
	g_return_if_fail (row >= 0);
	g_return_if_fail (selection != NULL);

	path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);

	if (!path)
		return;

	change_one_path (etsm, path, grow);
}

static void
tree_selection_model_change_cursor (ESelectionModel *selection,
                                    gint row,
                                    gint col)
{
	ETreeSelectionModel *etsm;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	etsm = E_TREE_SELECTION_MODEL (selection);

	if (row == -1) {
		etsm->priv->cursor_path = NULL;
	} else {
		etsm->priv->cursor_path =
			e_tree_table_adapter_node_at_row (
			etsm->priv->etta, row);
	}
	etsm->priv->cursor_col = col;
}

static gint
tree_selection_model_cursor_row (ESelectionModel *selection)
{
	return get_cursor_row (E_TREE_SELECTION_MODEL (selection));
}

static gint
tree_selection_model_cursor_col (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	return etsm->priv->cursor_col;
}

static void
etsm_get_rows (gint row,
               gpointer d)
{
	gint **rowp = d;

	**rowp = row;
	(*rowp)++;
}

static void
tree_selection_model_select_single_row (ESelectionModel *selection,
                                        gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	ETreePath path;
	gint rows[5], *rowp = NULL, size;

	path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);
	g_return_if_fail (path != NULL);

	/* we really only care about the size=1 case (cursor changed),
	 * but this doesn't cost much */
	size = g_hash_table_size (etsm->priv->paths);
	if (size > 0 && size <= 5) {
		rowp = rows;
		tree_selection_model_foreach (selection, etsm_get_rows, &rowp);
	}

	select_single_path (etsm, path);

	if (size > 5) {
		e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
	} else {
		if (rowp) {
			gint *p = rows;

			while (p < rowp)
				e_selection_model_selection_row_changed (
					(ESelectionModel *) etsm, *p++);
		}
		e_selection_model_selection_row_changed (
			(ESelectionModel *) etsm, row);
	}
}

static void
tree_selection_model_toggle_single_row (ESelectionModel *selection,
                                        gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);
	ETreePath path;

	path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);
	g_return_if_fail (path);

	if (g_hash_table_contains (etsm->priv->paths, path))
		g_hash_table_remove (etsm->priv->paths, path);
	else
		g_hash_table_add (etsm->priv->paths, path);

	etsm->priv->start_path = NULL;

	e_selection_model_selection_row_changed ((ESelectionModel *) etsm, row);
}

static void
etsm_real_move_selection_end (ETreeSelectionModel *etsm,
                              gint row)
{
	ETreePath end_path;
	gint start;

	end_path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);
	g_return_if_fail (end_path);

	start = e_tree_table_adapter_row_of_node (
		etsm->priv->etta, etsm->priv->start_path);
	clear_selection (etsm);
	select_range (etsm, start, row);
}

static void
tree_selection_model_move_selection_end (ESelectionModel *selection,
                                         gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	g_return_if_fail (etsm->priv->cursor_path);

	etsm_real_move_selection_end (etsm, row);
	e_selection_model_selection_changed (E_SELECTION_MODEL (selection));
}

static void
tree_selection_model_set_selection_end (ESelectionModel *selection,
                                        gint row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL (selection);

	g_return_if_fail (etsm->priv->cursor_path);

	if (!etsm->priv->start_path)
		etsm->priv->start_path = etsm->priv->cursor_path;
	etsm_real_move_selection_end (etsm, row);
	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
}

static void
e_tree_selection_model_class_init (ETreeSelectionModelClass *class)
{
	GObjectClass *object_class;
	ESelectionModelClass *esm_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = tree_selection_model_set_property;
	object_class->get_property = tree_selection_model_get_property;
	object_class->dispose = tree_selection_model_dispose;
	object_class->finalize = tree_selection_model_finalize;

	esm_class = E_SELECTION_MODEL_CLASS (class);
	esm_class->is_row_selected = tree_selection_model_is_row_selected;
	esm_class->foreach = tree_selection_model_foreach;
	esm_class->clear = tree_selection_model_clear;
	esm_class->selected_count = tree_selection_model_selected_count;
	esm_class->select_all = tree_selection_model_select_all;
	esm_class->row_count = tree_selection_model_row_count;

	esm_class->change_one_row = tree_selection_model_change_one_row;
	esm_class->change_cursor = tree_selection_model_change_cursor;
	esm_class->cursor_row = tree_selection_model_cursor_row;
	esm_class->cursor_col = tree_selection_model_cursor_col;

	esm_class->select_single_row = tree_selection_model_select_single_row;
	esm_class->toggle_single_row = tree_selection_model_toggle_single_row;
	esm_class->move_selection_end = tree_selection_model_move_selection_end;
	esm_class->set_selection_end = tree_selection_model_set_selection_end;

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_ROW,
		g_param_spec_int (
			"cursor_row",
			"Cursor Row",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CURSOR_COL,
		g_param_spec_int (
			"cursor_col",
			"Cursor Column",
			NULL,
			0, G_MAXINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_TREE_MODEL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_ETTA,
		g_param_spec_object (
			"etta",
			"ETTA",
			NULL,
			E_TYPE_TREE_TABLE_ADAPTER,
			G_PARAM_READWRITE));
}

static void
e_tree_selection_model_init (ETreeSelectionModel *etsm)
{
	etsm->priv = e_tree_selection_model_get_instance_private (etsm);

	etsm->priv->paths = g_hash_table_new (NULL, NULL);
	etsm->priv->cursor_col = -1;
}

ESelectionModel *
e_tree_selection_model_new (void)
{
	return g_object_new (E_TYPE_TREE_SELECTION_MODEL, NULL);
}

void
e_tree_selection_model_foreach (ETreeSelectionModel *etsm,
                                ETreeForeachFunc callback,
                                gpointer closure)
{
	GList *list, *link;

	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));
	g_return_if_fail (callback != NULL);

	list = g_hash_table_get_keys (etsm->priv->paths);

	for (link = list; link != NULL; link = g_list_next (link))
		callback ((ETreePath) link->data, closure);

	g_list_free (list);
}

void
e_tree_selection_model_select_single_path (ETreeSelectionModel *etsm,
                                           ETreePath path)
{
	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));
	g_return_if_fail (path != NULL);

	select_single_path (etsm, path);

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
}

void
e_tree_selection_model_select_paths (ETreeSelectionModel *etsm,
                                     GPtrArray *paths)
{
	ETreePath path;
	gint i;

	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));
	g_return_if_fail (paths != NULL);

	for (i = 0; i < paths->len; i++) {
		path = paths->pdata[i];
		change_one_path (etsm, path, TRUE);
	}

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
}

void
e_tree_selection_model_add_to_selection (ETreeSelectionModel *etsm,
                                         ETreePath path)
{
	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));
	g_return_if_fail (path != NULL);

	change_one_path (etsm, path, TRUE);

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
}

void
e_tree_selection_model_change_cursor (ETreeSelectionModel *etsm,
                                      ETreePath path)
{
	gint row;

	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));
	/* XXX Not sure if path can be NULL here. */

	etsm->priv->cursor_path = path;

	row = get_cursor_row (etsm);

	E_SELECTION_MODEL (etsm)->old_selection = -1;

	e_selection_model_selection_changed (E_SELECTION_MODEL (etsm));
	e_selection_model_cursor_changed (
		E_SELECTION_MODEL (etsm), row, etsm->priv->cursor_col);
	e_selection_model_cursor_activated (
		E_SELECTION_MODEL (etsm), row, etsm->priv->cursor_col);
}

ETreePath
e_tree_selection_model_get_cursor (ETreeSelectionModel *etsm)
{
	g_return_val_if_fail (E_IS_TREE_SELECTION_MODEL (etsm), NULL);

	return etsm->priv->cursor_path;
}

gint
e_tree_selection_model_get_selection_start_row (ETreeSelectionModel *etsm)
{
	g_return_val_if_fail (E_IS_TREE_SELECTION_MODEL (etsm), -1);

	if (!etsm->priv->start_path)
		return -1;

	return e_tree_table_adapter_row_of_node (etsm->priv->etta, etsm->priv->start_path);
}

void
e_tree_selection_model_set_selection_start_row (ETreeSelectionModel *etsm,
						gint row)
{
	ETreePath path;

	g_return_if_fail (E_IS_TREE_SELECTION_MODEL (etsm));

	path = e_tree_table_adapter_node_at_row (etsm->priv->etta, row);

	if (path)
		etsm->priv->start_path = path;
}

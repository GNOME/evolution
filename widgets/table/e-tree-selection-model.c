/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-tree-selection-model.c: a Selection Model
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include "e-tree-selection-model.h"
#include "gal/util/e-util.h"
#include <gdk/gdkkeysyms.h>

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

static ETreePath
etsm_node_at_row(ETreeSelectionModel *etsm, int row)
{
	ETreePath path;

	path = e_tree_table_adapter_node_at_row(etsm->etta, row);

	if (path)
		path = e_tree_sorted_view_to_model_path(etsm->ets, path);
 
	return path;
}

static int
etsm_row_of_node(ETreeSelectionModel *etsm, ETreePath path)
{
	path = e_tree_sorted_model_to_view_path(etsm->ets, path);

	if (path)
		return e_tree_table_adapter_row_of_node(etsm->etta, path);
	else
		return 0;
}

static int
etsm_cursor_row_real (ETreeSelectionModel *etsm)
{
	if (etsm->cursor_path)
		return etsm_row_of_node(etsm, etsm->cursor_path);
	else
		return -1;
}

static void
etsm_real_clear (ETreeSelectionModel *etsm)
{
	g_hash_table_destroy(etsm->data);
	etsm->data = g_hash_table_new(NULL, NULL);
}

static void
etsm_destroy (GtkObject *object)
{
	ETreeSelectionModel *etsm;

	etsm = E_TREE_SELECTION_MODEL (object);

	g_hash_table_destroy(etsm->data);
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
		GTK_VALUE_INT(*arg) = etsm->cursor_col;
		break;

	case ARG_MODEL:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->model;
		break;

	case ARG_ETTA:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->etta;
		break;

	case ARG_ETS:
		GTK_VALUE_OBJECT(*arg) = (GtkObject *) etsm->ets;
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
		e_selection_model_do_something(esm, GTK_VALUE_INT(*arg), etsm->cursor_col, 0);
		break;

	case ARG_CURSOR_COL:
		e_selection_model_do_something(esm, etsm_cursor_row_real(etsm), GTK_VALUE_INT(*arg), 0);
		break;

	case ARG_MODEL:
		etsm->model = (ETreeModel *) GTK_VALUE_OBJECT(*arg);
		break;

	case ARG_ETTA:
		etsm->etta = (ETreeTableAdapter *) GTK_VALUE_OBJECT(*arg);
		break;

	case ARG_ETS:
		etsm->ets = (ETreeSorted *) GTK_VALUE_OBJECT(*arg);
		break;
	}
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
		      gint             n)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path;

	path = etsm_node_at_row(etsm, n);
	if (!etsm->invert_selection) {
		if (path)
			return (gboolean) g_hash_table_lookup (etsm->data, path);
		else
			return FALSE;
	} else {
		if (path)
			return !(gboolean) g_hash_table_lookup (etsm->data, path);
		else
			return TRUE;
	}
}

typedef struct {
	ETreeSelectionModel *etsm;
	EForeachFunc         callback;
	gpointer             closure;
} ModelAndCallback;

static void
etsm_foreach_callback (gpointer key, gpointer value, gpointer user_data)
{
	ModelAndCallback *mac = user_data;
	ETreePath path = key;
	int row;

	row = etsm_row_of_node(mac->etsm, path);
	if (row != -1)
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

	if (etsm->invert_selection) {
		g_warning ("FIXME: Not implemented yet line 167 of e-tree-selection-model.c");
	} else {
		ModelAndCallback mac;
		mac.etsm = etsm;
		mac.callback = callback;
		mac.closure = closure;
		g_hash_table_foreach(etsm->data, etsm_foreach_callback, &mac);
	}
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
	etsm->invert_selection = FALSE;

	etsm->cursor_path = NULL;
	etsm->cursor_col = -1;
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

	return g_hash_table_size(etsm->data);
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

	etsm_real_clear (etsm);
	etsm->invert_selection = TRUE;

	if (etsm->cursor_col == -1)
		etsm->cursor_col = 0;
	if (etsm->cursor_path == NULL)
		etsm->cursor_path = etsm_node_at_row(etsm, 0);
	etsm->selection_start_row = 0;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), etsm_cursor_row_real(etsm), etsm->cursor_col);
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

	etsm->invert_selection = ! etsm->invert_selection;
	
	etsm->cursor_col = -1;
	etsm->cursor_path = NULL;
	etsm->selection_start_row = 0;
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	e_selection_model_cursor_changed(E_SELECTION_MODEL(etsm), -1, -1);
}

static int
etsm_row_count (ESelectionModel *selection)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	return e_table_model_row_count(E_TABLE_MODEL(etsm->etta));
}

static void
etsm_change_one_row(ESelectionModel *selection, int row, gboolean grow)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path = etsm_node_at_row(etsm, row);

	if (!path)
		return;

	if ((grow && (!etsm->invert_selection)) ||
	    ((!grow) && etsm->invert_selection)    )
		g_hash_table_insert(etsm->data, path, path);
	else
		g_hash_table_remove(etsm->data, path);
}

static void
etsm_change_cursor (ESelectionModel *selection, int row, int col)
{
	ETreeSelectionModel *etsm;

	g_return_if_fail(selection != NULL);
	g_return_if_fail(E_IS_SELECTION_MODEL(selection));

	etsm = E_TREE_SELECTION_MODEL(selection);

	if (row == -1) {
		etsm->cursor_path = NULL;
	} else {
		etsm->cursor_path = etsm_node_at_row(etsm, row);
	}
	etsm->cursor_col = col;
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
	return etsm->cursor_col;
}

static void
etsm_select_single_row (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);

	etsm_real_clear (etsm);
	etsm->invert_selection = FALSE;
	etsm_change_one_row(selection, row, TRUE);
	etsm->selection_start_row = row;

	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

static void
etsm_toggle_single_row (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	ETreePath path;

	etsm->selection_start_row = row;

	path = etsm_node_at_row(etsm, row);
	if (path) {
		if (g_hash_table_lookup(etsm->data, path))
			g_hash_table_remove(etsm->data, path);
		else
			g_hash_table_insert(etsm->data, path, path);
		e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
	}
}

static void
etsm_move_selection_end (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	int old_start;
	int old_end;
	int new_start;
	int new_end;
	if (selection->sorter && e_sorter_needs_sorting(selection->sorter)) {
		old_start = MIN (e_sorter_model_to_sorted(selection->sorter, etsm->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, etsm_cursor_row_real(etsm)));
		old_end = MAX (e_sorter_model_to_sorted(selection->sorter, etsm->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, etsm_cursor_row_real(etsm))) + 1;
		new_start = MIN (e_sorter_model_to_sorted(selection->sorter, etsm->selection_start_row),
				 e_sorter_model_to_sorted(selection->sorter, row));
		new_end = MAX (e_sorter_model_to_sorted(selection->sorter, etsm->selection_start_row),
			       e_sorter_model_to_sorted(selection->sorter, row)) + 1;
	} else {
		old_start = MIN (etsm->selection_start_row, etsm_cursor_row_real(etsm));
		old_end = MAX (etsm->selection_start_row, etsm_cursor_row_real(etsm)) + 1;
		new_start = MIN (etsm->selection_start_row, row);
		new_end = MAX (etsm->selection_start_row, row) + 1;
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
	e_selection_model_selection_changed(E_SELECTION_MODEL(etsm));
}

static void
etsm_set_selection_end (ESelectionModel *selection, int row)
{
	ETreeSelectionModel *etsm = E_TREE_SELECTION_MODEL(selection);
	etsm_select_single_row(selection, etsm->selection_start_row);
	if (etsm->selection_start_row != -1)
		etsm->cursor_path = etsm_node_at_row(etsm, etsm->selection_start_row);
	else
		etsm->cursor_path = NULL;
	e_selection_model_move_selection_end(selection, row);
}


static void
e_tree_selection_model_init (ETreeSelectionModel *etsm)
{
	etsm->data = g_hash_table_new(NULL, NULL);
	etsm->invert_selection = FALSE;
	etsm->cursor_path = NULL;
	etsm->cursor_col = -1;
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

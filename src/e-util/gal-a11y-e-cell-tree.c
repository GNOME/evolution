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
 *		Tim Wo <tim.wo@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell-tree.h"

#include <atk/atk.h>
#include <glib/gi18n.h>

#include "e-cell-tree.h"
#include "e-table.h"
#include "e-tree-table-adapter.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-util.h"

#define d(x)

G_DEFINE_TYPE_WITH_CODE (GalA11yECellTree, gal_a11y_e_cell_tree, GAL_A11Y_TYPE_E_CELL,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, gal_a11y_e_cell_atk_action_interface_init))

static void
ectr_model_row_changed_cb (ETableModel *etm,
                           gint row,
                           GalA11yECell *a11y)
{
	ETreePath node;
	ETreeModel *tree_model;
	ETreeTableAdapter *tree_table_adapter;

	g_return_if_fail (a11y);
	if (a11y->row != row)
		return;

	node = e_table_model_value_at (etm, -1, a11y->row);
	tree_model = e_table_model_value_at (etm, -2, a11y->row);
	tree_table_adapter = e_table_model_value_at (etm, -3, a11y->row);

	if (node && e_tree_model_node_is_expandable (tree_model, node)) {
		gboolean is_exp = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);
		if (is_exp)
			gal_a11y_e_cell_add_state (a11y, ATK_STATE_EXPANDED, TRUE);
		else
			gal_a11y_e_cell_remove_state (a11y, ATK_STATE_EXPANDED, TRUE);
	}
}

static void
kill_view_cb (ECellView *subcell_view,
             gpointer psubcell_a11ies)
{
	GList *node;
	GList *subcell_a11ies = (GList *) psubcell_a11ies;
	GalA11yECell *subcell;

	for (node = subcell_a11ies; node != NULL; node = g_list_next (node))
	{
	    subcell = GAL_A11Y_E_CELL (node->data);
	    if (subcell && subcell->cell_view == subcell_view)
	    {
		d (fprintf (stderr, "subcell_view %p deleted before the a11y object %p\n", subcell_view, subcell));
		subcell->cell_view = NULL;
	    }
	}
}

static void
ectr_subcell_weak_ref (GalA11yECellTree *a11y,
                       GalA11yECell *subcell_a11y)
{
	ECellView *subcell_view = subcell_a11y ? subcell_a11y->cell_view : NULL;
	if (subcell_a11y && subcell_view && subcell_view->kill_view_cb_data)
	    subcell_view->kill_view_cb_data = g_list_remove (subcell_view->kill_view_cb_data, subcell_a11y);

	if (GAL_A11Y_E_CELL (a11y)->item && GAL_A11Y_E_CELL (a11y)->item->table_model) {
		g_signal_handler_disconnect (
			GAL_A11Y_E_CELL (a11y)->item->table_model,
			a11y->model_row_changed_id);
	}
	g_object_unref (a11y);
}

static void
ectr_do_action_expand (AtkAction *action)
{
	GalA11yECell *a11y;
	ETableModel *table_model;
	ETreePath node;
	ETreeModel *tree_model;
	ETreeTableAdapter *tree_table_adapter;

	a11y = GAL_A11Y_E_CELL (action);
	table_model = a11y->item->table_model;
	node = e_table_model_value_at (table_model, -1, a11y->row);
	tree_model = e_table_model_value_at (table_model, -2, a11y->row);
	tree_table_adapter = e_table_model_value_at (table_model, -3, a11y->row);

	if (node && e_tree_model_node_is_expandable (tree_model, node)) {
		e_tree_table_adapter_node_set_expanded (
			tree_table_adapter, node, TRUE);
		gal_a11y_e_cell_add_state (a11y, ATK_STATE_EXPANDED, TRUE);
	}
}

static void
ectr_do_action_collapse (AtkAction *action)
{
	GalA11yECell *a11y;
	ETableModel *table_model;
	ETreePath node;
	ETreeModel *tree_model;
	ETreeTableAdapter *tree_table_adapter;

	a11y = GAL_A11Y_E_CELL (action);
	table_model = a11y->item->table_model;
	node = e_table_model_value_at (table_model, -1, a11y->row);
	tree_model = e_table_model_value_at (table_model, -2, a11y->row);
	tree_table_adapter = e_table_model_value_at (table_model, -3, a11y->row);

	if (node && e_tree_model_node_is_expandable (tree_model, node)) {
		e_tree_table_adapter_node_set_expanded (
			tree_table_adapter, node, FALSE);
		gal_a11y_e_cell_remove_state (a11y, ATK_STATE_EXPANDED, TRUE);
	}
}

static void
gal_a11y_e_cell_tree_class_init (GalA11yECellTreeClass *class)
{
}

static void
gal_a11y_e_cell_tree_init (GalA11yECellTree *a11y)
{
}

AtkObject *
gal_a11y_e_cell_tree_new (ETableItem *item,
                          ECellView *cell_view,
                          AtkObject *parent,
                          gint model_col,
                          gint view_col,
                          gint row)
{
	AtkObject *subcell_a11y;
	GalA11yECellTree *a11y;
	ETreePath node;
	ETreeModel *tree_model;
	ETreeTableAdapter *tree_table_adapter;
	ECellView *subcell_view;

	subcell_view = e_cell_tree_view_get_subcell_view (cell_view);

	if (subcell_view && subcell_view->ecell) {
		subcell_a11y = gal_a11y_e_cell_registry_get_object (
			NULL,
			item,
			subcell_view,
			parent,
			model_col,
			view_col,
			row);
		gal_a11y_e_cell_add_action (
			GAL_A11Y_E_CELL (subcell_a11y),
			"expand",
			/* Translators: description of an "expand" action */
			_("expands the row in the ETree containing this cell"),
			NULL,
			(ACTION_FUNC) ectr_do_action_expand);

		gal_a11y_e_cell_add_action (
			GAL_A11Y_E_CELL (subcell_a11y),
			"collapse",
			/* Translators: description of a "collapse" action */
			_("collapses the row in the ETree containing this cell"),
			NULL,
			(ACTION_FUNC) ectr_do_action_collapse);

		/* init AtkStates for the cell's a11y object */
		node = e_table_model_value_at (item->table_model, -1, row);
		tree_model = e_table_model_value_at (item->table_model, -2, row);
		tree_table_adapter = e_table_model_value_at (item->table_model, -3, row);
		if (node && e_tree_model_node_is_expandable (tree_model, node)) {
			gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL (subcell_a11y), ATK_STATE_EXPANDABLE, FALSE);
			if (e_tree_table_adapter_node_is_expanded (tree_table_adapter, node))
				gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL (subcell_a11y), ATK_STATE_EXPANDED, FALSE);
		}
	} else
		subcell_a11y = NULL;

	/* create a companion a11y object, this object has type GalA11yECellTree
	 * and it connects to some signals to determine whether a tree cell is
	 * expanded or collapsed */
	a11y = g_object_new (gal_a11y_e_cell_tree_get_type (), NULL);
	gal_a11y_e_cell_construct (
		ATK_OBJECT (a11y),
		item,
		cell_view,
		parent,
		model_col,
		view_col,
		row);
	a11y->model_row_changed_id = g_signal_connect (
		item->table_model, "model_row_changed",
		G_CALLBACK (ectr_model_row_changed_cb), subcell_a11y);

	if (subcell_a11y && subcell_view)
	{
	    subcell_view->kill_view_cb = kill_view_cb;
	    if (!g_list_find (subcell_view->kill_view_cb_data, subcell_a11y))
		subcell_view->kill_view_cb_data = g_list_append (subcell_view->kill_view_cb_data, subcell_a11y);
	}

	g_object_weak_ref (G_OBJECT (subcell_a11y), (GWeakNotify) ectr_subcell_weak_ref, a11y);

	return subcell_a11y;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Tim Wo <tim.wo@sun.com>, Sun Microsystem Inc. 2003.
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include <atk/atk.h>
#include "gal-a11y-e-cell-tree.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-util.h"
#include "gal/e-table/e-cell-tree.h"
#include "gal/e-table/e-table.h"
#include "gal/e-table/e-tree-table-adapter.h"
#include <glib/gi18n.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yECellTreeClass))
static AtkObjectClass *a11y_parent_class;
#define A11Y_PARENT_TYPE (gal_a11y_e_cell_get_type ())

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

	if (e_tree_model_node_is_expandable (tree_model, node)) {
		gboolean is_exp = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);
		if (is_exp)
			gal_a11y_e_cell_add_state (a11y, ATK_STATE_EXPANDED, TRUE);
		else
			gal_a11y_e_cell_remove_state (a11y, ATK_STATE_EXPANDED, TRUE);
	}
}

static void
ectr_subcell_weak_ref (GalA11yECellTree *a11y,
		       GalA11yECell     *subcell_a11y)
{
	g_signal_handler_disconnect (GAL_A11Y_E_CELL (a11y)->item->table_model,
				     a11y->model_row_changed_id);
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

	if (e_tree_model_node_is_expandable (tree_model, node)) {
		e_tree_table_adapter_node_set_expanded (tree_table_adapter,
							node,
							TRUE);
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

	if (e_tree_model_node_is_expandable (tree_model, node)) {
		e_tree_table_adapter_node_set_expanded (tree_table_adapter,
							node,
							FALSE);
		gal_a11y_e_cell_remove_state (a11y, ATK_STATE_EXPANDED, TRUE);
	}
}

static void
ectr_class_init (GalA11yECellTreeClass *klass)
{
	a11y_parent_class        = g_type_class_ref (A11Y_PARENT_TYPE);
}

static void
ectr_init (GalA11yECellTree *a11y)
{
}

GType
gal_a11y_e_cell_tree_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellTreeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ectr_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECellTree),
			0,
			(GInstanceInitFunc) ectr_init,
			NULL /* value_cell_text */
		};

		type = g_type_register_static (A11Y_PARENT_TYPE, "GalA11yECellTree", &info, 0);
		gal_a11y_e_cell_type_add_action_interface (type);
	}

	return type;
}

AtkObject *
gal_a11y_e_cell_tree_new (ETableItem *item,
			  ECellView  *cell_view,
			  AtkObject  *parent,
			  int         model_col,
			  int         view_col,
			  int         row)
{
	AtkObject *subcell_a11y;
	GalA11yECellTree *a11y;

        ETreePath node;
        ETreeModel *tree_model;
        ETreeTableAdapter *tree_table_adapter;
 
	ECellView *subcell_view;
	subcell_view = e_cell_tree_view_get_subcell_view (cell_view);

	if (subcell_view->ecell) {
		subcell_a11y = gal_a11y_e_cell_registry_get_object (NULL,
								    item,
								    subcell_view,
								    parent,
								    model_col,
								    view_col,
								    row);
		gal_a11y_e_cell_add_action (GAL_A11Y_E_CELL (subcell_a11y),
					    _("expand"),
					    _("expands the row in the ETree containing this cell"),
					    NULL,
					    (ACTION_FUNC)ectr_do_action_expand);

		gal_a11y_e_cell_add_action (GAL_A11Y_E_CELL (subcell_a11y),
					    _("collapse"),
					    _("collapses the row in the ETree containing this cell"),
					    NULL,
					    (ACTION_FUNC)ectr_do_action_collapse);

		/* init AtkStates for the cell's a11y object */
		node = e_table_model_value_at (item->table_model, -1, row);
		tree_model = e_table_model_value_at (item->table_model, -2, row);
		tree_table_adapter = e_table_model_value_at (item->table_model, -3, row);
		if (e_tree_model_node_is_expandable (tree_model, node)) {
			gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL (subcell_a11y), ATK_STATE_EXPANDABLE, FALSE);
			if (e_tree_table_adapter_node_is_expanded (tree_table_adapter, node))
				gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL (subcell_a11y), ATK_STATE_EXPANDED, FALSE);
		}
	}
	else
		subcell_a11y = NULL;

	/* create a companion a11y object, this object has type GalA11yECellTree
	   and it connects to some signals to determine whether a tree cell is
	   expanded or collapsed */
	a11y = g_object_new (gal_a11y_e_cell_tree_get_type (), NULL);
	gal_a11y_e_cell_construct (ATK_OBJECT (a11y),
				   item,
				   cell_view,
				   parent,
				   model_col,
				   view_col,
				   row);
	a11y->model_row_changed_id =
		g_signal_connect (item->table_model, "model_row_changed",
				  G_CALLBACK (ectr_model_row_changed_cb),
				  subcell_a11y);
	g_object_weak_ref (G_OBJECT (subcell_a11y), (GWeakNotify) ectr_subcell_weak_ref, a11y);

	return subcell_a11y;
}

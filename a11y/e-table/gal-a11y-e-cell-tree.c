/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Tim Wo <tim.wo@sun.com>, Sun Microsystem Inc. 2003.
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include <atk/atkaction.h>
#include "gal-a11y-e-cell-tree.h"
#include "gal-a11y-util.h"
#include "gal/e-table/e-cell-tree.h"
#include "gal/e-table/e-table.h"
#include "gal/e-table/e-tree-table-adapter.h"

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yECellTreeClass))
static AtkObjectClass *a11y_parent_class;
#define A11Y_PARENT_TYPE (gal_a11y_e_cell_get_type ())

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
	GalA11yECell *a11y;
        GtkWidget *e_table;
        gint model_row;

	ECellView *subcell_view;
	subcell_view = e_cell_tree_view_get_subcell_view (cell_view);

	if (subcell_view->ecell) {
		a11y = gal_a11y_e_cell_registry_get_object (NULL,
							    item,
							    subcell_view,
							    parent,
							    model_col,
							    view_col,
							    row);
	} else {
		a11y = g_object_new (gal_a11y_e_cell_tree_get_type (), NULL);

		gal_a11y_e_cell_construct (a11y,
					   item,
					   cell_view,
					   parent,
					   model_col,
					   view_col,
					   row);
	}

	gal_a11y_e_cell_add_action (a11y,
				    "expand",
				    "expands the row in the ETree containing this cell",
				    NULL,
				    (ACTION_FUNC)ectr_do_action_expand);

	gal_a11y_e_cell_add_action (a11y,
				    "collapse",
				    "collapses the row in the ETree containing this cell",
				    NULL,
				    (ACTION_FUNC)ectr_do_action_collapse);

	gal_a11y_e_cell_add_state (a11y, ATK_STATE_EXPANDABLE, FALSE);

	return a11y;
}

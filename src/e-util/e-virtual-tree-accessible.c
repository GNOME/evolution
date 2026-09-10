/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <atk/atk.h>

#include "e-virtual-tree.h"
#include "e-virtual-tree-model.h"
#include "e-virtual-tree-accessible.h"

guint		_e_virtual_tree_get_n_columns	(EVirtualTree *self);
const gchar *	_e_virtual_tree_get_column_title (EVirtualTree *self,
						  guint column_index);
gchar *		_e_virtual_tree_get_cell_text	(EVirtualTree *self,
						 guint row_index,
						 guint column_index);

#define EVTA_TYPE_CELL_ACCESSIBLE (evta_cell_accessible_get_type ())
G_DECLARE_FINAL_TYPE (EvtaCellAccessible, evta_cell_accessible, EVTA, CELL_ACCESSIBLE, AtkObject)

struct _EvtaCellAccessible {
	AtkObject parent_instance;
	EVirtualTree *vtree;
	guint row;
	guint column;
};

G_DEFINE_FINAL_TYPE (EvtaCellAccessible, evta_cell_accessible, ATK_TYPE_OBJECT)

static AtkStateSet *
evta_cell_ref_state_set (AtkObject *obj)
{
	EvtaCellAccessible *cell = EVTA_CELL_ACCESSIBLE (obj);
	AtkStateSet *states;
	EVirtualTreeModel *model;

	states = ATK_OBJECT_CLASS (evta_cell_accessible_parent_class)->ref_state_set (obj);

	if (!cell->vtree)
		return states;

	model = e_virtual_tree_get_model (cell->vtree);
	if (model) {
		GObject *row_object = e_virtual_tree_model_dup_row (model, cell->row);

		if (row_object) {
			if (e_virtual_tree_model_is_expandable (model, row_object)) {
				atk_state_set_add_state (states, ATK_STATE_EXPANDABLE);
				if (e_virtual_tree_model_get_expanded (model, row_object))
					atk_state_set_add_state (states, ATK_STATE_EXPANDED);
			}
			g_object_unref (row_object);
		}
	}

	if (e_virtual_tree_row_is_selected (cell->vtree, cell->row))
		atk_state_set_add_state (states, ATK_STATE_SELECTED);

	return states;
}

static void
evta_cell_accessible_class_init (EvtaCellAccessibleClass *klass)
{
	ATK_OBJECT_CLASS (klass)->ref_state_set = evta_cell_ref_state_set;
}

static void
evta_cell_accessible_init (EvtaCellAccessible *self)
{
}

struct _EVirtualTreeAccessible {
	GtkContainerAccessible parent_instance;
	gulong model_row_count_changed_id;
	gulong selection_changed_id;
	gulong cursor_changed_id;
	gulong notify_model_id;
};

static void evta_atk_table_iface_init (AtkTableIface *iface);
static void evta_atk_selection_iface_init (AtkSelectionIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EVirtualTreeAccessible,
	e_virtual_tree_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_TABLE, evta_atk_table_iface_init)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, evta_atk_selection_iface_init))

static EVirtualTree *
evta_get_vtree (AtkObject *obj)
{
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
	if (!widget)
		return NULL;

	return E_VIRTUAL_TREE (widget);
}

static EVirtualTreeModel *
evta_get_model (AtkObject *obj)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (obj);
	if (!vtree)
		return NULL;

	return e_virtual_tree_get_model (vtree);
}

static void
evta_on_selection_changed (EVirtualTree *vtree,
			   gpointer user_data)
{
	AtkObject *atk_obj = ATK_OBJECT (user_data);

	g_signal_emit_by_name (atk_obj, "selection-changed");
}

static void
evta_on_row_count_changed (EVirtualTreeModel *model,
			   gpointer user_data)
{
	AtkObject *atk_obj = ATK_OBJECT (user_data);

	g_signal_emit_by_name (atk_obj, "visible-data-changed");
	g_signal_emit_by_name (atk_obj, "model-changed");
}

static void
evta_on_cursor_changed (EVirtualTree *vtree,
			guint cursor_row,
			GObject *cursor_object,
			gpointer user_data)
{
	AtkObject *atk_obj = ATK_OBJECT (user_data);

	g_signal_emit_by_name (atk_obj, "active-descendant-changed", NULL);
}

static void
evta_disconnect_model (EVirtualTreeAccessible *self)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;

	vtree = evta_get_vtree (ATK_OBJECT (self));
	if (!vtree)
		return;

	model = e_virtual_tree_get_model (vtree);
	if (model && self->model_row_count_changed_id) {
		g_signal_handler_disconnect (model, self->model_row_count_changed_id);
		self->model_row_count_changed_id = 0;
	}
}

static void
evta_connect_model (EVirtualTreeAccessible *self)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;

	vtree = evta_get_vtree (ATK_OBJECT (self));
	if (!vtree)
		return;

	model = e_virtual_tree_get_model (vtree);
	if (model) {
		self->model_row_count_changed_id = g_signal_connect (
			model, "row-count-changed",
			G_CALLBACK (evta_on_row_count_changed), self);
	}
}

static void
evta_on_notify_model (GObject *object,
		      GParamSpec *pspec,
		      gpointer user_data)
{
	EVirtualTreeAccessible *self = E_VIRTUAL_TREE_ACCESSIBLE (user_data);

	evta_disconnect_model (self);
	evta_connect_model (self);
}

static void
evta_initialize (AtkObject *obj,
		 gpointer data)
{
	EVirtualTreeAccessible *self = E_VIRTUAL_TREE_ACCESSIBLE (obj);
	EVirtualTree *vtree;

	ATK_OBJECT_CLASS (e_virtual_tree_accessible_parent_class)->initialize (obj, data);

	atk_object_set_role (obj, ATK_ROLE_TREE_TABLE);

	vtree = evta_get_vtree (obj);
	if (!vtree)
		return;

	self->selection_changed_id = g_signal_connect (
		vtree, "selection-changed",
		G_CALLBACK (evta_on_selection_changed), self);

	self->cursor_changed_id = g_signal_connect (
		vtree, "cursor-changed",
		G_CALLBACK (evta_on_cursor_changed), self);

	self->notify_model_id = g_signal_connect (
		vtree, "notify::model",
		G_CALLBACK (evta_on_notify_model), self);

	evta_connect_model (self);
}

static void
evta_finalize (GObject *object)
{
	EVirtualTreeAccessible *self = E_VIRTUAL_TREE_ACCESSIBLE (object);
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (self));
	if (vtree) {
		if (self->selection_changed_id) {
			g_signal_handler_disconnect (vtree, self->selection_changed_id);
			self->selection_changed_id = 0;
		}
		if (self->cursor_changed_id) {
			g_signal_handler_disconnect (vtree, self->cursor_changed_id);
			self->cursor_changed_id = 0;
		}
		if (self->notify_model_id) {
			g_signal_handler_disconnect (vtree, self->notify_model_id);
			self->notify_model_id = 0;
		}
		evta_disconnect_model (self);
	}

	G_OBJECT_CLASS (e_virtual_tree_accessible_parent_class)->finalize (object);
}

static gint
evta_get_n_children (AtkObject *obj)
{
	return 0;
}

static AtkObject *
evta_ref_child (AtkObject *obj,
		gint ii)
{
	return NULL;
}

static AtkStateSet *
evta_ref_state_set (AtkObject *obj)
{
	AtkStateSet *state_set;

	state_set = ATK_OBJECT_CLASS (e_virtual_tree_accessible_parent_class)->ref_state_set (obj);
	atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

	return state_set;
}

static AtkObject *
evta_table_ref_at (AtkTable *table,
		   gint row,
		   gint column)
{
	EVirtualTree *vtree;
	EvtaCellAccessible *cell;
	gchar *text;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return NULL;

	text = _e_virtual_tree_get_cell_text (vtree, (guint) row, (guint) column);

	cell = g_object_new (EVTA_TYPE_CELL_ACCESSIBLE, NULL);
	cell->vtree = vtree;
	cell->row = (guint) row;
	cell->column = (guint) column;

	atk_object_set_role (ATK_OBJECT (cell), ATK_ROLE_TABLE_CELL);
	atk_object_set_name (ATK_OBJECT (cell), text ? text : "");
	atk_object_set_parent (ATK_OBJECT (cell), ATK_OBJECT (table));

	g_free (text);

	return ATK_OBJECT (cell);
}

static gint
evta_table_get_index_at (AtkTable *table,
			 gint row,
			 gint column)
{
	EVirtualTree *vtree;
	guint n_cols;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return -1;

	n_cols = _e_virtual_tree_get_n_columns (vtree);
	if (n_cols == 0)
		return -1;

	return row * (gint) n_cols + column;
}

static gint
evta_table_get_column_at_index (AtkTable *table,
				gint index)
{
	EVirtualTree *vtree;
	guint n_cols;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return -1;

	n_cols = _e_virtual_tree_get_n_columns (vtree);
	if (n_cols == 0)
		return -1;

	return index % (gint) n_cols;
}

static gint
evta_table_get_row_at_index (AtkTable *table,
			     gint index)
{
	EVirtualTree *vtree;
	guint n_cols;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return -1;

	n_cols = _e_virtual_tree_get_n_columns (vtree);
	if (n_cols == 0)
		return -1;

	return index / (gint) n_cols;
}

static gint
evta_table_get_n_columns (AtkTable *table)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return 0;

	return (gint) _e_virtual_tree_get_n_columns (vtree);
}

static gint
evta_table_get_n_rows (AtkTable *table)
{
	EVirtualTreeModel *model;

	model = evta_get_model (ATK_OBJECT (table));
	if (!model)
		return 0;

	return (gint) e_virtual_tree_model_get_row_count (model);
}

static const gchar *
evta_table_get_column_description (AtkTable *table,
				   gint column)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return NULL;

	return _e_virtual_tree_get_column_title (vtree, (guint) column);
}

static AtkObject *
evta_table_get_column_header (AtkTable *table,
			      gint column)
{
	EVirtualTree *vtree;
	GtkTreeViewColumn *tvc;
	GtkWidget *button;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return NULL;

	tvc = e_virtual_tree_get_column (vtree, (guint) column);
	if (!tvc)
		return NULL;

	button = gtk_tree_view_column_get_button (tvc);
	if (!button)
		return NULL;

	return gtk_widget_get_accessible (button);
}

static gboolean
evta_table_is_row_selected (AtkTable *table,
			    gint row)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return FALSE;

	return e_virtual_tree_row_is_selected (vtree, (guint) row);
}

static gboolean
evta_table_is_selected (AtkTable *table,
			gint row,
			gint column)
{
	return evta_table_is_row_selected (table, row);
}

static gboolean
evta_table_add_row_selection (AtkTable *table,
			      gint row)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return FALSE;

	e_virtual_tree_select_row (vtree, (guint) row);

	return TRUE;
}

static gboolean
evta_table_remove_row_selection (AtkTable *table,
				 gint row)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (table));
	if (!vtree)
		return FALSE;

	e_virtual_tree_unselect_row (vtree, (guint) row);

	return TRUE;
}

static gboolean
evta_selection_add_selection (AtkSelection *selection,
			      gint ii)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return FALSE;

	e_virtual_tree_select_row (vtree, (guint) ii);

	return TRUE;
}

static gboolean
evta_selection_clear_selection (AtkSelection *selection)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return FALSE;

	e_virtual_tree_unselect_all (vtree);

	return TRUE;
}

static AtkObject *
evta_selection_ref_selection (AtkSelection *selection,
			      gint ii)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	AtkObject *cell = NULL;
	guint total, row, count;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return NULL;

	model = e_virtual_tree_get_model (vtree);
	if (!model)
		return NULL;

	total = e_virtual_tree_model_get_row_count (model);
	count = 0;

	for (row = 0; row < total; row++) {
		if (!e_virtual_tree_row_is_selected (vtree, row))
			continue;

		if ((gint) count == ii) {
			cell = evta_table_ref_at (ATK_TABLE (selection), (gint) row, 0);
			break;
		}
		count++;
	}

	return cell;
}

static gint
evta_selection_get_selection_count (AtkSelection *selection)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	guint total, row, count;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return 0;

	model = e_virtual_tree_get_model (vtree);
	if (!model)
		return 0;

	total = e_virtual_tree_model_get_row_count (model);
	count = 0;

	for (row = 0; row < total; row++) {
		if (e_virtual_tree_row_is_selected (vtree, row))
			count++;
	}

	return (gint) count;
}

static gboolean
evta_selection_is_child_selected (AtkSelection *selection,
				  gint ii)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return FALSE;

	return e_virtual_tree_row_is_selected (vtree, (guint) ii);
}

static gboolean
evta_selection_remove_selection (AtkSelection *selection,
				 gint ii)
{
	EVirtualTree *vtree;
	EVirtualTreeModel *model;
	guint total, row, count;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return FALSE;

	model = e_virtual_tree_get_model (vtree);
	if (!model)
		return FALSE;

	total = e_virtual_tree_model_get_row_count (model);
	count = 0;

	for (row = 0; row < total; row++) {
		if (!e_virtual_tree_row_is_selected (vtree, row))
			continue;

		if ((gint) count == ii) {
			e_virtual_tree_unselect_row (vtree, row);
			return TRUE;
		}
		count++;
	}

	return FALSE;
}

static gboolean
evta_selection_select_all_selection (AtkSelection *selection)
{
	EVirtualTree *vtree;

	vtree = evta_get_vtree (ATK_OBJECT (selection));
	if (!vtree)
		return FALSE;

	e_virtual_tree_select_all (vtree);

	return TRUE;
}

static void
evta_atk_table_iface_init (AtkTableIface *iface)
{
	iface->ref_at = evta_table_ref_at;
	iface->get_index_at = evta_table_get_index_at;
	iface->get_column_at_index = evta_table_get_column_at_index;
	iface->get_row_at_index = evta_table_get_row_at_index;
	iface->get_n_columns = evta_table_get_n_columns;
	iface->get_n_rows = evta_table_get_n_rows;
	iface->get_column_description = evta_table_get_column_description;
	iface->get_column_header = evta_table_get_column_header;
	iface->is_row_selected = evta_table_is_row_selected;
	iface->is_selected = evta_table_is_selected;
	iface->add_row_selection = evta_table_add_row_selection;
	iface->remove_row_selection = evta_table_remove_row_selection;
}

static void
evta_atk_selection_iface_init (AtkSelectionIface *iface)
{
	iface->add_selection = evta_selection_add_selection;
	iface->clear_selection = evta_selection_clear_selection;
	iface->ref_selection = evta_selection_ref_selection;
	iface->get_selection_count = evta_selection_get_selection_count;
	iface->is_child_selected = evta_selection_is_child_selected;
	iface->remove_selection = evta_selection_remove_selection;
	iface->select_all_selection = evta_selection_select_all_selection;
}

static void
e_virtual_tree_accessible_class_init (EVirtualTreeAccessibleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);
	GtkContainerAccessibleClass *container_class = GTK_CONTAINER_ACCESSIBLE_CLASS (klass);

	object_class->finalize = evta_finalize;

	atk_class->initialize = evta_initialize;
	atk_class->get_n_children = evta_get_n_children;
	atk_class->ref_child = evta_ref_child;
	atk_class->ref_state_set = evta_ref_state_set;

	container_class->add_gtk = NULL;
	container_class->remove_gtk = NULL;
}

static void
e_virtual_tree_accessible_init (EVirtualTreeAccessible *self)
{
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Iain Holmes <iain@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkenums.h>
#include <gtk/gtksignal.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-header-item.h>
#include <gal/e-table/e-table-item.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-tree-scrolled.h>
#include <gal/e-table/e-tree-memory.h>
#include <gal/e-table/e-tree-memory-callbacks.h>
#include <gal/e-table/e-tree-table-adapter.h>

#include <libgnome/gnome-i18n.h>

#include "e-summary-table.h"
#include "e-cell-tri.h"

#define COLS 2

#if 0 /* For translators */
char *headers[COLS] = {
	N_("Shown"),
	N_("Name")
};
#endif

#define SPEC "<ETableSpecification cursor-mode=\"line\" draw-focus=\"true\" no-headers=\"true\"> \
<ETableColumn model_col=\"0\" _title=\"Shown\" minimum_width=\"20\" resizable=\"false\" cell=\"tricell\" compare=\"integer\"/> \
<ETableColumn model_col=\"1\" _title=\"Name\" resizable=\"true\" minimum-width=\"32\" cell=\"render-name\" compare=\"string\"/> \
<ETableState> \
<column source=\"0\"/> \
<column source=\"1\"/> \
<grouping></grouping> \
</ETableState> \
</ETableSpecification>"

#define PARENT_TYPE (gtk_vbox_get_type ())

static GtkObjectClass *e_summary_table_parent_class;

struct _ESummaryTablePrivate {
	GtkWidget *etable;
	ETableExtras *extras;
	ETreeTableAdapter *adapter;
	ETreeModel *etm;

	ETreePath root_node;
};

enum {
	ITEM_CHANGED,
	LAST_SIGNAL
};
static guint32 table_signals[LAST_SIGNAL] = { 0 };

/* ETree callbacks */
static GdkPixbuf *
icon_at (ETreeModel *etm,
	 ETreePath path,
	 void *model_data)
{
	/* No icon, since the cell tree renderer takes care of +/- */
	return NULL;
}

static int
column_count (ETreeModel *etm,
	      void *data)
{
	return COLS;
}

static void *
duplicate_value (ETreeModel *etm,
		 int col,
		 const void *value,
		 void *data)
{
	switch (col) {
	case 0:
		return (void *) value;

	case 1:
		return g_strdup (value);

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static void
free_value (ETreeModel *etm,
	    int col,
	    void *value,
	    void *data)
{
	if (col == 1) {
		g_free (value);
	}
}

static void *
initialise_value (ETreeModel *etm,
		  int col,
		  void *data)
{
	switch (col) {
	case 0:
		return GINT_TO_POINTER (1);

	case 1:
		return g_strdup ("2");

	default:
		g_assert_not_reached ();

	}

	return NULL;
}

static gboolean
value_is_empty (ETreeModel *etm,
		int col,
		const void *value,
		void *data)
{
	if (col == 1) {
		return !(value && *(char *)value);
	}

	return FALSE;
}

static char *
value_to_string (ETreeModel *etm,
		 int col,
		 const void *value,
		 void *data)
{
	switch (col) {
	case 0:
		return g_strdup_printf ("%d", GPOINTER_TO_INT (value));

	case 1:
		return g_strdup (value);

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static void *
value_at (ETreeModel *etm,
	  ETreePath path,
	  int col,
	  void *model_data)
{
	GHashTable *table;
	ESummaryTable *est = E_SUMMARY_TABLE (model_data);
	ESummaryTableModelEntry *entry;

	table = est->model;
	if (e_tree_model_node_is_root (etm, path)) {
		if (col == 1) {
			return "<Root>";
		} else {
			return GINT_TO_POINTER (0);
		}
	} else {
		entry = g_hash_table_lookup (table, path);
		g_return_val_if_fail (entry != NULL, NULL);

		if (col == 1) {
			return entry->name;

		} else {
			if (entry->editable == TRUE) {
				return GINT_TO_POINTER (entry->shown ? 2 : 1);
			} else {
				return GINT_TO_POINTER (0);
			}
		}
	}
}

static void
set_value_at (ETreeModel *etm,
	      ETreePath path,
	      int col,
	      const void *val,
	      void *model_data)
{
	GHashTable *table;
	ESummaryTable *est = E_SUMMARY_TABLE (model_data);
	ESummaryTableModelEntry *entry;

	if (e_tree_model_node_is_root (etm, path)) {
		return;
	}

	if (col != 0) {
		return;
	}

	table = est->model;
	entry = g_hash_table_lookup (table, path);
	g_return_if_fail (entry != NULL);

	if (entry->editable == TRUE) {
		entry->shown = GPOINTER_TO_INT (val) == 1 ? FALSE : TRUE;
		g_signal_emit (est, table_signals[ITEM_CHANGED], 0, path);
	}
}

static gboolean
is_editable (ETreeModel *etm,
	     ETreePath path,
	     int col,
	     void *model_data)
{
	GHashTable *table;
	ESummaryTable *est = (ESummaryTable *) model_data;
	ESummaryTableModelEntry *entry;

	if (e_tree_model_node_is_root (etm, path)) {
		return FALSE;
	}

	if (col == 1) {
		return FALSE;
	}

	table = est->model;
	entry = g_hash_table_lookup (table, path);
	g_return_val_if_fail (entry != NULL, FALSE);

	return entry->editable;
}

/* GtkObject callbacks */

static void
free_model_entry (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	ESummaryTableModelEntry *entry;

	entry = value;
	g_free (entry->location);
	g_free (entry->name);
	g_free (entry);
}

static void
destroy (GtkObject *object)
{
	ESummaryTable *est;
	ESummaryTablePrivate *priv;

	est = E_SUMMARY_TABLE (object);
	priv = est->priv;

	if (priv == NULL) {
		return;
	}

	/* What do I need to free? */
	g_hash_table_foreach (est->model, free_model_entry, NULL);
	g_hash_table_destroy (est->model);
	est->model = NULL;
	
	g_free (priv);
	est->priv = NULL;

	e_summary_table_parent_class->destroy (object);
}

static void
e_summary_table_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = destroy;

	e_summary_table_parent_class = gtk_type_class (PARENT_TYPE);

	table_signals[ITEM_CHANGED] = gtk_signal_new ("item-changed",
						      GTK_RUN_LAST,
						      GTK_CLASS_TYPE (object_class),
						      GTK_SIGNAL_OFFSET (ESummaryTableClass, item_changed),
						      gtk_marshal_NONE__POINTER,
						      GTK_TYPE_NONE, 1,
						      GTK_TYPE_POINTER);
}

static void
e_summary_table_init (ESummaryTable *est)
{
	ESummaryTablePrivate *priv;
	ETreeMemory *etmm;
	ECell *cell;
	ETree *tree;

	priv = g_new (ESummaryTablePrivate, 1);
	est->priv = priv;

	priv->etm = e_tree_memory_callbacks_new (icon_at,
						 column_count,
						 
						 NULL,
						 NULL,

						 NULL,
						 NULL,

						 value_at,
						 set_value_at,
						 is_editable,

						 duplicate_value,
						 free_value,
						 initialise_value,
						 value_is_empty,
						 value_to_string,
						 est);
	gtk_object_ref (GTK_OBJECT (priv->etm));
	gtk_object_sink (GTK_OBJECT (priv->etm));

	etmm = E_TREE_MEMORY (priv->etm);
	e_tree_memory_set_expanded_default (etmm, TRUE);
	
	priv->root_node = e_tree_memory_node_insert (etmm, NULL, 0, est);

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	priv->extras = e_table_extras_new ();
	e_table_extras_add_cell (priv->extras, "render-name", e_cell_tree_new (NULL, NULL, FALSE, cell));
	e_table_extras_add_cell (priv->extras, "tricell", e_cell_tri_new ());
	
	priv->etable = e_tree_scrolled_new (priv->etm, priv->extras, SPEC, NULL);
	if (priv->etable == NULL) {
		g_warning ("Could not create ETable for ESummaryTable");
		return;
	}

	tree = e_tree_scrolled_get_tree (E_TREE_SCROLLED (priv->etable));
	e_tree_root_node_set_visible (tree, FALSE);

	gtk_box_pack_start (GTK_BOX (est), GTK_WIDGET (priv->etable),
			    TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (priv->etable));
}

E_MAKE_TYPE (e_summary_table, "ESummaryTable", ESummaryTable,
	     e_summary_table_class_init, e_summary_table_init, PARENT_TYPE);

GtkWidget *
e_summary_table_new (GHashTable *model)
{
	ESummaryTable *table;

	table = gtk_type_new (e_summary_table_get_type ());
	table->model = model;

	return GTK_WIDGET (table);
}

ETreePath
e_summary_table_add_node (ESummaryTable *table,
			  ETreePath path,
			  int position,
			  gpointer node_data)
{
	ETreeMemory *etmm;
	ETreePath p;

	g_return_val_if_fail (IS_E_SUMMARY_TABLE (table), NULL);

	if (path == NULL) {
		path = table->priv->root_node;
	}

	etmm = E_TREE_MEMORY (table->priv->etm);
	e_tree_memory_freeze (etmm);
	p = e_tree_memory_node_insert (etmm, path, position, node_data);
	e_tree_memory_thaw (etmm);

	return p;
}
	      
guint
e_summary_table_get_num_children (ESummaryTable *table,
				  ETreePath path)
{
	if (path == NULL) {
		return e_tree_model_node_get_children (table->priv->etm,
						       table->priv->root_node, NULL);
	} else {
		return e_tree_model_node_get_children (table->priv->etm, path, NULL);
	}
}

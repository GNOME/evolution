/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *   Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include <string.h>
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-e-cell.h"
#include "gal-a11y-util.h"
#include <gal/e-table/e-table-subset.h>
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-tree.h>

#include <atk/atkobject.h>
#include <atk/atktable.h>
#include <atk/atkcomponent.h>
#include <atk/atkobjectfactory.h>
#include <atk/atkregistry.h>
#include <atk/atkgobjectaccessible.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yETableItemClass))
static GObjectClass *parent_class;
static AtkComponentIface *component_parent_iface;
static GType parent_type;
static gint priv_offset;
static GQuark		quark_accessible_object = 0;
#define GET_PRIVATE(object) ((GalA11yETableItemPrivate *) (((char *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETableItemPrivate {
	AtkObject *parent;
	gint index_in_parent;
	gint cols;
	gint rows;
	gpointer *cell_data;
	int selection_change_id;
	int cursor_change_id;
	ETableCol ** columns;
	ESelectionModel *selection;
	GtkWidget *widget;
};

static gboolean gal_a11y_e_table_item_ref_selection (GalA11yETableItem *a11y,
						     ESelectionModel *selection);
static gboolean gal_a11y_e_table_item_unref_selection (GalA11yETableItem *a11y);

static gpointer *eti_reinit_data (AtkTable *table, ETableItem *item);

#if 0
static void
unref_accessible (gpointer user_data, GObject *obj_loc)
{
	GalA11yETableItem *a11y = GAL_A11Y_E_TABLE_ITEM (user_data);
	GET_PRIVATE (a11y)->item = NULL;
	g_object_unref (a11y);
}
#endif

inline static gint
view_to_model_row(ETableItem *eti, int row)
{
	if (eti->uses_source_model) {
		ETableSubset *etss = E_TABLE_SUBSET(eti->table_model);
		if (row >= 0 && row < etss->n_map) {
			eti->row_guess = row;
			return etss->map_table[row];
		} else
			return -1;
	} else
		return row;
}

inline static gint
view_to_model_col(ETableItem *eti, int col)
{
	ETableCol *ecol = e_table_header_get_column (eti->header, col);
	return ecol ? ecol->col_idx : -1;
}

inline static GObject *
eti_a11y_get_gobject (AtkObject *accessible)
{
	return atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
}

static void
eti_dispose (GObject *object)
{
	GalA11yETableItem *a11y = GAL_A11Y_E_TABLE_ITEM (object);
	GalA11yETableItemPrivate *priv = GET_PRIVATE (a11y);

	priv->parent = NULL;

	if ( priv->cell_data != NULL ) {
		g_free(priv->cell_data);
		priv->cell_data = NULL;
	}

	if (priv->columns) {
		g_free(priv->columns);
		priv->columns = NULL;
	}

	if (parent_class->dispose)
		parent_class->dispose (object);
	if (priv->selection)
		gal_a11y_e_table_item_unref_selection (a11y);
}

/* Static functions */
static AtkObject *
eti_get_parent (AtkObject *accessible)
{
	GalA11yETableItem *a11y;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), NULL);
	if (!eti_a11y_get_gobject (accessible))
		/* defunct */
		return NULL;

	a11y = GAL_A11Y_E_TABLE_ITEM (accessible);
	return GET_PRIVATE (a11y)->parent;
}

static gint
eti_get_n_children (AtkObject *accessible)
{
	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), 0);
	if (!eti_a11y_get_gobject (accessible))
		return 0;

	return atk_table_get_n_columns (ATK_TABLE (accessible)) *
		atk_table_get_n_rows (ATK_TABLE (accessible));
}

static AtkObject*
eti_ref_child (AtkObject *accessible, gint index)
{
	ETableItem *item;
	gint col, row;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), NULL);
	item = E_TABLE_ITEM (eti_a11y_get_gobject (accessible));
	if (!item)
		return NULL;

	if (index < item->cols) {
		AtkObject *child;

		/* A column header is required */
		child = atk_table_get_column_header (ATK_TABLE (accessible), index);
		if (child)
			g_object_ref (child);
		return child;
	}

	index -= item->cols;
	col = index % item->cols;
	row = index / item->cols;

	return atk_table_ref_at (ATK_TABLE (accessible), row, col);
}

static gint
eti_get_index_in_parent (AtkObject *accessible)
{
	GalA11yETableItem *a11y;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), -1);
	if (!eti_a11y_get_gobject (accessible))
		return -1;

	a11y = GAL_A11Y_E_TABLE_ITEM (accessible);
	return GET_PRIVATE (a11y)->index_in_parent;
}

static void
eti_get_extents (AtkComponent *component,
		 gint *x,
		 gint *y,
		 gint *width,
		 gint *height,
		 AtkCoordType coord_type)
{
	ETableItem *item;
	double real_width;
	double real_height;
	int fake_width;
	int fake_height;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (component)));
	if (!item)
		return;

	if (component_parent_iface &&
	    component_parent_iface->get_extents)
		component_parent_iface->get_extents (component,
						     x,
						     y,
						     &fake_width,
						     &fake_height,
						     coord_type);

	gtk_object_get (GTK_OBJECT (item),
			"width", &real_width,
			"height", &real_height,
			NULL);

	if (width)
		*width = real_width;
	if (height) 
		*height = real_height;
}

static AtkObject*
eti_ref_accessible_at_point (AtkComponent *component,
			     gint x,
			     gint y,
			     AtkCoordType coord_type)
{
	int row = -1;
	int col = -1;
	int x_origin, y_origin;
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (component)));
	if (!item)
		return NULL;

	atk_component_get_position (component,
				    &x_origin,
				    &y_origin,
				    coord_type);
	x -= x_origin;
	y -= y_origin;

	e_table_item_compute_location (item, &x, &y,
				       &row, &col);

	if (row != -1 && col != -1) {
		return atk_table_ref_at (ATK_TABLE (component), row, col);
	} else {
		return NULL;
	}
}


static void
cell_destroyed (gpointer data)
{
	gint index;
	GalA11yETableItem * item;
	GalA11yECell * cell;

	g_return_if_fail (GAL_A11Y_IS_E_CELL (data));
	cell = GAL_A11Y_E_CELL (data);
		
	item = GAL_A11Y_E_TABLE_ITEM (atk_gobject_accessible_for_object (G_OBJECT (GAL_A11Y_E_CELL(data)->item)));

	g_return_if_fail (item && GAL_A11Y_IS_E_TABLE_ITEM (item));

	g_return_if_fail (cell->row < GET_PRIVATE(item)->rows && cell->view_col < GET_PRIVATE(item)->cols);

	index = cell->row * cell->item->cols + cell->view_col;
                                                                   
	if (GET_PRIVATE (item)->cell_data && GET_PRIVATE (item)->cell_data [index] == data)
		GET_PRIVATE (item)->cell_data [index] = NULL;
}

/* atk table */
static AtkObject*
eti_ref_at (AtkTable *table, gint row, gint column)
{
	ETableItem *item;
	AtkObject* ret;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return NULL;

	if (column >= 0 &&
	    column < item->cols &&
	    row >= 0 &&
	    row < item->rows &&
	    item->cell_views_realized) {
		ECellView *cell_view = item->cell_views[column];
		ETableCol *ecol = e_table_header_get_column (item->header, column);
		gpointer * cell_data;

		cell_data = eti_reinit_data (table, item);
		if (cell_data == NULL) 
			return NULL;

		if (cell_data[row*item->cols + column] == NULL) {
			ret = gal_a11y_e_cell_registry_get_object (NULL,
							    item,
							    cell_view,
							    ATK_OBJECT (table),
							    ecol->col_idx,
							    column,
							    row);
			cell_data[row*item->cols + column] = ret;
			if (ATK_IS_OBJECT (ret))
				g_object_weak_ref (G_OBJECT (ret),
						   (GWeakNotify) cell_destroyed,
						   ret);
			else
				ret = NULL;
		} else {
			ret = (AtkObject *) cell_data[row*item->cols + column];
			if (ATK_IS_OBJECT (ret)) {
				g_object_ref (ret);
			} else { 
				ret = NULL;
			}
		}


		return ret;
	}

	return NULL;
}

static gint
eti_get_index_at (AtkTable *table, gint row, gint column)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return column + row * item->cols;
}

static gint
eti_get_column_at_index (AtkTable *table, gint index)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return index % item->cols;
}

static gint
eti_get_row_at_index (AtkTable *table, gint index)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return index / item->cols;
}

static gint
eti_get_n_columns (AtkTable *table)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return item->cols;
}

static gint
eti_get_n_rows (AtkTable *table)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return item->rows;
}

static gint
eti_get_column_extent_at (AtkTable *table,
			  gint row,
			  gint column)
{
	ETableItem *item;
	int width;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	e_table_item_get_cell_geometry (item,
					&row, 
					&column,
					NULL,
					NULL,
					&width,
					NULL);

	return width;
}

static gint
eti_get_row_extent_at (AtkTable *table,
		       gint row,
		       gint column)
{
	ETableItem *item;
	int height;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	e_table_item_get_cell_geometry (item,
					&row, 
					&column,
					NULL,
					NULL,
					NULL,
					&height);

	return height;
}

static AtkObject *
eti_get_caption (AtkTable *table)
{
	/* Unimplemented */
	return NULL;
}

static G_CONST_RETURN gchar *
eti_get_column_description (AtkTable *table,
			    gint column)
{
	ETableItem *item;
	ETableCol *ecol;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return NULL;

	ecol = e_table_header_get_column (item->header, column);

	return ecol->text;
}

static AtkObject *
eti_get_column_header (AtkTable *table, gint column)
{
	ETableItem *item;
	ETableCol *ecol;
	AtkObject *atk_obj = NULL;
	ECell *ecell;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return NULL;

	ecol = e_table_header_get_column (item->header, column);
	ecell = ecol->ecell;
	if (ecell)
		atk_obj = atk_gobject_accessible_for_object (G_OBJECT (ecell));
	return atk_obj;
}

static G_CONST_RETURN gchar *
eti_get_row_description (AtkTable *table,
			 gint row)
{
	/* Unimplemented */
	return NULL;
}

static AtkObject *
eti_get_row_header (AtkTable *table,
		    gint row)
{
	/* Unimplemented */
	return NULL;
}

static AtkObject *
eti_get_summary (AtkTable *table)
{
	/* Unimplemented */
	return NULL;
}

static gboolean 
table_is_row_selected (AtkTable *table, gint row)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	return e_selection_model_is_row_selected(item->selection, row);
}

static gboolean 
table_is_selected (AtkTable *table, gint row, gint column)
{
	return table_is_row_selected (table, row);
}

static gint
table_get_selected_rows (AtkTable *table, gint **rows_selected)
{
	ETableItem *item;
	gint n_selected, row, index_selected;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return 0;

	n_selected = e_selection_model_selected_count (item->selection);
	if (rows_selected) {
		*rows_selected = (gint *) g_malloc (n_selected * sizeof (gint));

		index_selected = 0;
		for (row = 0; row < item->rows && index_selected < n_selected; ++row) {
			if (atk_table_is_row_selected (table, row)) {
				(*rows_selected)[index_selected] = row;
				++index_selected;
			}
		}
	}
	return n_selected;
}

static gboolean 
table_add_row_selection (AtkTable *table, gint row)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	if (table_is_row_selected (table, row))
		return TRUE;
	e_selection_model_toggle_single_row (item->selection,
					     view_to_model_row (item, row));

	return TRUE;
}

static gboolean 
table_remove_row_selection (AtkTable *table, gint row)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	if (!atk_table_is_row_selected (table, row))
		return TRUE;
	e_selection_model_toggle_single_row (item->selection, row);
	return TRUE;
}

static void
eti_atk_table_iface_init (AtkTableIface *iface)
{
	iface->ref_at = eti_ref_at;
	iface->get_index_at = eti_get_index_at;
	iface->get_column_at_index = eti_get_column_at_index;
	iface->get_row_at_index = eti_get_row_at_index;
	iface->get_n_columns = eti_get_n_columns;
	iface->get_n_rows = eti_get_n_rows;
	iface->get_column_extent_at = eti_get_column_extent_at;
	iface->get_row_extent_at = eti_get_row_extent_at;
	iface->get_caption = eti_get_caption;
	iface->get_column_description = eti_get_column_description;
	iface->get_column_header = eti_get_column_header;
	iface->get_row_description = eti_get_row_description;
	iface->get_row_header = eti_get_row_header;
	iface->get_summary = eti_get_summary;

	iface->is_row_selected = table_is_row_selected;
	iface->is_selected = table_is_selected;
	iface->get_selected_rows = table_get_selected_rows;
	iface->add_row_selection = table_add_row_selection;
	iface->remove_row_selection = table_remove_row_selection;
}

static void
eti_atk_component_iface_init (AtkComponentIface *iface)
{
	component_parent_iface         = g_type_interface_peek_parent (iface);

	iface->ref_accessible_at_point = eti_ref_accessible_at_point;
	iface->get_extents             = eti_get_extents;
}

static void
eti_rows_inserted (ETableModel * model, int row, int count, 
		   AtkObject * table_item)
{
	gint n_cols,n_rows,i,j;
	gpointer *cell_data;
	GalA11yETableItem * item_a11y;
	gint old_nrows;

	g_return_if_fail (table_item);
 	item_a11y = GAL_A11Y_E_TABLE_ITEM (table_item);

        n_cols = atk_table_get_n_columns (ATK_TABLE(table_item));
	n_rows = atk_table_get_n_rows (ATK_TABLE(table_item));

	old_nrows = GET_PRIVATE(item_a11y)->rows;
	
	g_return_if_fail (n_cols > 0 && n_rows > 0);
	g_return_if_fail (old_nrows == n_rows - count);

	cell_data = GET_PRIVATE(table_item)->cell_data;
	GET_PRIVATE(table_item)->cell_data = g_realloc (cell_data, (n_rows*n_cols) * sizeof(gpointer));
	cell_data = GET_PRIVATE(table_item)->cell_data;

	GET_PRIVATE(table_item)->rows = n_rows; 

	/* If rows are insert in the middle of a table. */
	if (row + count < n_rows ) {
		memmove(&cell_data[(row+count)*n_cols], &cell_data[row*n_cols],
			(old_nrows-row)*n_cols*sizeof(gpointer));

		/* Update cell's index. */
		for (i = row + count; i < n_rows; i ++) {
			for (j = 0; j < n_cols; j ++)
				if (cell_data[i*n_cols + j] != NULL) {
					AtkObject * a11y;

					a11y = ATK_OBJECT(cell_data[i*n_cols + j]);
                                        GAL_A11Y_E_CELL(a11y)->row = i;
				}
		}
	}

	/* Clear cache for the new added rows. */
	for (i = row ; i < row+count; i ++)
		for (j = 0 ;  j < n_cols; j ++)
			cell_data [i*n_cols + j] = NULL;

	g_signal_emit_by_name (table_item, "row-inserted", row,
			       count, NULL);

        for (i = row; i < (row + count); i ++) {
                for (j = 0; j < n_cols; j ++) {
			g_signal_emit_by_name (table_item,
					       "children_changed::add",
                                               ( (i*n_cols) + j), NULL, NULL);
		}
        }

	g_signal_emit_by_name (table_item, "visible-data-changed");
}

/* 
 * reinit the eti's private data
 * make sure it is synchronized with the gal-e-table-item
 */
static gpointer *
eti_reinit_data (AtkTable *table, ETableItem *item)
{
	gpointer * cell_data;

	int oldsize, newsize;
	cell_data = GET_PRIVATE (table)->cell_data;
	if (GET_PRIVATE (table)->rows != item->rows 
		|| GET_PRIVATE (table)->cols != item->cols ) { /* reinit cell_data */
		oldsize = GET_PRIVATE (table)->cols * GET_PRIVATE (table)->rows;
		newsize = item->cols*item->rows;
		GET_PRIVATE (table)->cols = item->cols;
		GET_PRIVATE (table)->rows = item->rows;

		cell_data = g_realloc(cell_data, newsize*sizeof(gpointer));
		if (newsize > oldsize)
			memset(&cell_data[oldsize], 0, (newsize-oldsize)*sizeof(gpointer));
		
		GET_PRIVATE (table)->cell_data = cell_data;
	}
	return cell_data;
}

/* 
 * clear all the AtkObjects stored in the cell_data
 * doesn't free the cell_data or resize it.
 */
static void
eti_clear_rows (ETableModel * model, AtkObject * table_item, int row, int count)
{
	gint i,j, n_rows, n_cols;
	gpointer *cell_data;

	g_return_if_fail (model && table_item);

	cell_data = GET_PRIVATE (table_item)->cell_data;
	g_return_if_fail (cell_data);

	n_rows = GET_PRIVATE (table_item)->rows;
	n_cols = GET_PRIVATE (table_item)->cols;

	g_return_if_fail( row >= 0 && count > 0 && row+count <= n_rows);

	/* DEFUNCT the deleted cells. */
	for (i = row; i < row+count; i ++) {
		for (j = 0; j < n_cols; j ++) {
			if (cell_data[i*n_cols + j] != NULL) {
				AtkObject * a11y;

				a11y = ATK_OBJECT(cell_data[i*n_cols + j]);
				gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL(a11y), ATK_STATE_DEFUNCT, TRUE);
				cell_data[i*n_cols + j] = NULL;
			}
		}
	}

	g_signal_emit_by_name (table_item, "row-deleted", row,
			       count, NULL);

	for (i = row; i < row+count; i ++) {
		for (j = 0; j < n_cols; j ++) {
			g_signal_emit_by_name (table_item,
				"children_changed::remove",
				( (i*n_cols) + j), NULL, NULL);
		}
        }
	g_signal_emit_by_name (table_item, "visible-data-changed");
}

static void
eti_rows_deleted (ETableModel * model, int row, int count, 
		  AtkObject * table_item)
{
	gint i,j, n_rows, n_cols, old_nrows;
	gpointer *cell_data;
	
	n_rows = atk_table_get_n_rows (ATK_TABLE(table_item));
        n_cols = atk_table_get_n_columns (ATK_TABLE(table_item));

	cell_data = GET_PRIVATE(table_item)->cell_data;
	old_nrows = GET_PRIVATE(table_item)->rows;

	g_return_if_fail ( row+count <= old_nrows);
	g_return_if_fail (old_nrows == n_rows + count);
	GET_PRIVATE(table_item)->rows = n_rows;

	/* DEFUNCT the deleted cells. */
	for (i = row; i < row+count; i ++) {
		for (j = 0; j < n_cols; j ++) {
			if (cell_data[i*n_cols + j] != NULL) {
				AtkObject * a11y;

				a11y = ATK_OBJECT(cell_data[i*n_cols + j]);
				gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL(a11y), ATK_STATE_DEFUNCT, TRUE);
				cell_data[i*n_cols + j] = NULL;
			}
		}
	}

	/* If more rows left, update the a11y object. */
	if (old_nrows > row + count) {

		/* Remove the defunct cells in cache. */
		memmove (&cell_data[row*n_cols], &cell_data[(row+count)*n_cols],
			( old_nrows-row-count)*n_cols*sizeof(gpointer));

		GET_PRIVATE(table_item)->cell_data = g_realloc (cell_data, n_rows*n_cols*sizeof(gpointer));
		cell_data = GET_PRIVATE(table_item)->cell_data;

		/* Update index of cells below the deleted rows. */
		for (i = row; i < n_rows; i ++) {
			for (j = 0; j < n_cols; j ++) {
				if (cell_data[i*n_cols + j] != NULL) {
					AtkObject * a11y;

					a11y = ATK_OBJECT(cell_data[i*n_cols + j]);
					GAL_A11Y_E_CELL(a11y)->row = i;
				}
			}
		}
	}

	g_signal_emit_by_name (table_item, "row-deleted", row,
			       count, NULL);

        for (i = row; i < (row + count); i ++) {
                for (j = 0; j < n_cols; j ++) {
			g_signal_emit_by_name (table_item,
					       "children_changed::remove",
                                               ( (i*n_cols) + j), NULL, NULL);
		}
        }
	g_signal_emit_by_name (table_item, "visible-data-changed");
}

static void
eti_tree_model_node_changed_cb (ETreeModel *model, ETreePath node, ETableItem *eti)
{
	AtkObject *atk_obj;
	GalA11yETableItem *a11y;

	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (eti));
	a11y = GAL_A11Y_E_TABLE_ITEM (atk_obj);

	/* we can't figure out which rows are changed, so just clear all of them ... */
	if  (GET_PRIVATE (a11y)->rows > 0)
		eti_clear_rows (eti->table_model, atk_obj, 0, GET_PRIVATE (a11y)->rows);
}

enum {
        ETI_HEADER_UNCHANGED = 0,
        ETI_HEADER_REORDERED,
        ETI_HEADER_NEW_ADDED,
        ETI_HEADER_REMOVED,
};
                                                                                
/*
 * 1. Check what actually happened: column reorder, remove or add
 * 2. Update cache
 * 3. Emit signals
 */
static void
eti_header_structure_changed (ETableHeader *eth, AtkObject *a11y)
{
                                                                                
        gboolean reorder_found=FALSE, added_found=FALSE, removed_found=FALSE;
        GalA11yETableItem * a11y_item;
        ETableCol ** cols, **prev_cols;
        GalA11yETableItemPrivate *priv;
        gint *state = NULL, *prev_state = NULL, *reorder = NULL;
        gint i,j,n_rows,n_cols, prev_n_cols;
        gpointer * cell_data, * tmp;
                                                                                
        a11y_item = GAL_A11Y_E_TABLE_ITEM (a11y);
        priv = GET_PRIVATE (a11y_item);
                                                                                
	g_return_if_fail (priv && priv->cell_data);
	cell_data = priv->cell_data ;

	/* Assume rows do not changed. */
        n_rows = priv->rows;

        prev_n_cols = priv->cols;
        prev_cols = priv->columns;

        cols = e_table_header_get_columns (eth);
	n_cols = eth->col_count;
                                                                                
        g_return_if_fail (cols && prev_cols && n_cols > 0);
                                                                                
        /* Init to ETI_HEADER_UNCHANGED. */
        state = g_malloc0 (sizeof (gint) * n_cols);
        prev_state = g_malloc0 (sizeof (gint) * prev_n_cols);
        reorder = g_malloc0 (sizeof (gint) * n_cols);

        /* Compare with previously saved column headers. */
        for ( i = 0 ; i < n_cols && cols[i]; i ++ ) {
                for ( j = 0 ; j < prev_n_cols && prev_cols[j]; j ++ ) {
                        if ( prev_cols [j] == cols[i] && i != j ) {

                                reorder_found = TRUE;
                                state [i] = ETI_HEADER_REORDERED;
				reorder [i] = j;

                                break;
                        } else if (prev_cols[j] == cols[i]) {
                                /* OK, this column is not changed. */
                                break;
                        }
                }
                                                                                
                /* cols[i] is new added column. */
                if ( j == prev_n_cols ) {
			added_found = TRUE;
                        state[i] = ETI_HEADER_NEW_ADDED;
                }
        }

        /* Now try to find if there are removed columns. */
        for (i = 0 ; i < prev_n_cols && prev_cols[i]; i ++) {
                for (j = 0 ; j < n_cols && cols[j]; j ++)
                        if ( prev_cols [j] == cols[i] )
				break;
                                                                                
                /* Removed columns found. */
                if ( j == n_cols ) {
			removed_found = TRUE;
			prev_state[j] = ETI_HEADER_REMOVED;
                }
        }

	/* If nothing interesting just return. */
	if (!reorder_found && !added_found && !removed_found)
		return;

	/* Now update our cache. */
	tmp = g_malloc0 (n_rows*n_cols*sizeof(gpointer));
	g_return_if_fail (tmp);

	for (i = 0 ; i < n_rows; i ++) {
		for ( j = 0 ; j < n_cols; j ++ ) {
			if ( state[j] == ETI_HEADER_REORDERED ) {
				tmp [i*n_cols+j] = cell_data[i*prev_n_cols+reorder[j]];
				if (tmp[i*n_cols+j] &&  ATK_IS_OBJECT(tmp[i*n_cols+j])) {
					GAL_A11Y_E_CELL(tmp[i*n_cols+j])->view_col = j;
				}
			} else if (state[j] == ETI_HEADER_UNCHANGED) {
				tmp [i*n_cols+j] = cell_data[i*prev_n_cols+j];
			} /* else: new added, keep NULL. */
		}
	}

	g_free (cell_data);
	priv->cell_data = tmp;

	/* Emit signals */
	if (reorder_found)
        	g_signal_emit_by_name (G_OBJECT(a11y_item), "column_reordered");


	if (removed_found) {
		for (i = 0; i < prev_n_cols; i ++ ) {
			if (prev_state[i] == ETI_HEADER_REMOVED) {
				g_signal_emit_by_name (G_OBJECT(a11y_item), "column-deleted", i, 1);
				for (j = 0 ; j < n_rows; j ++)
					g_signal_emit_by_name (G_OBJECT(a11y_item), "children_changed::remove", (j*prev_n_cols+i), NULL, NULL);
			}
		}
	}

	if (added_found) {
		for ( i = 0; i < n_cols; i ++ ) {
			if (state[i] == ETI_HEADER_NEW_ADDED) {
				g_signal_emit_by_name (G_OBJECT(a11y_item), "column-inserted", i, 1);
				for (j = 0 ; j < n_rows; j ++)
					g_signal_emit_by_name (G_OBJECT(a11y_item), "children_changed::add", (j*n_cols+i), NULL, NULL);
			}
		}
	}

	priv->cols = n_cols;

	g_free (state);
	g_free (reorder);
	g_free (prev_state);

	g_free (priv->columns);
	priv->columns = cols;
}


static void
eti_real_initialize (AtkObject *obj, 
		     gpointer data)
{
	ETableItem * eti;
	ETableModel * model;

	ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);
	eti = E_TABLE_ITEM (data);

	model = eti->table_model;

	g_signal_connect (model, "model-rows-inserted",
			  G_CALLBACK (eti_rows_inserted),
			  obj);
	g_signal_connect (model, "model-rows-deleted",
			  G_CALLBACK (eti_rows_deleted),
			  obj);
	g_signal_connect (G_OBJECT (eti->header), "structure_change",
			  G_CALLBACK (eti_header_structure_changed), obj);

}

static void
eti_class_init (GalA11yETableItemClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	quark_accessible_object               = g_quark_from_static_string ("gtk-accessible-object");

	parent_class                          = g_type_class_ref (PARENT_TYPE);

	object_class->dispose                 = eti_dispose;

	atk_object_class->get_parent          = eti_get_parent;
	atk_object_class->get_n_children      = eti_get_n_children;
	atk_object_class->ref_child           = eti_ref_child;
	atk_object_class->get_index_in_parent = eti_get_index_in_parent;
	atk_object_class->initialize	      = eti_real_initialize;
}

static void
eti_init (GalA11yETableItem *a11y)
{
	GalA11yETableItemPrivate *priv;

	priv = GET_PRIVATE (a11y);

	priv->parent = NULL;
	priv->index_in_parent = -1;
	priv->selection_change_id = 0;
	priv->cursor_change_id = 0;
	priv->selection = NULL;
}

/* atk selection */

static void             atk_selection_interface_init    (AtkSelectionIface      *iface);
static gboolean         selection_add_selection    (AtkSelection           *selection,
						    gint                   i);
static gboolean         selection_clear_selection  (AtkSelection           *selection);
static AtkObject*       selection_ref_selection    (AtkSelection           *selection,
						    gint                   i);
static gint             selection_get_selection_count (AtkSelection           *selection);
static gboolean         selection_is_child_selected (AtkSelection           *selection,
						     gint                   i);

/* callbacks */
static void eti_a11y_selection_model_removed_cb (ETableItem *eti,
						 ESelectionModel *selection,
						 gpointer data);
static void eti_a11y_selection_model_added_cb (ETableItem *eti,
					       ESelectionModel *selection,
					       gpointer data);
static void eti_a11y_selection_changed_cb (ESelectionModel *selection,
					   GalA11yETableItem *a11y);
static void eti_a11y_cursor_changed_cb (ESelectionModel *selection,
					int row, int col,
					GalA11yETableItem *a11y);

/**
 * gal_a11y_e_table_item_get_type:
 * @void: 
 * 
 * Registers the &GalA11yETableItem class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yETableItem class.
 **/
GType
gal_a11y_e_table_item_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;

		GTypeInfo info = {
			sizeof (GalA11yETableItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eti_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETableItem),
			0,
			(GInstanceInitFunc) eti_init,
			NULL /* value_table_item */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) eti_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo atk_table_info = {
			(GInterfaceInitFunc) eti_atk_table_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		static const GInterfaceInfo atk_selection_info = {
			(GInterfaceInitFunc) atk_selection_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};


		factory = atk_registry_get_factory (atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (PARENT_TYPE, "GalA11yETableItem", &info, 0,
								   sizeof (GalA11yETableItemPrivate), &priv_offset);

		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TABLE, &atk_table_info);
		g_type_add_interface_static (type, ATK_TYPE_SELECTION, &atk_selection_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_table_item_new (AtkObject *parent,
			   ETableItem *item,
			   int index_in_parent)
{
	GalA11yETableItem *a11y;
	AtkObject *accessible;
	int n;

	g_return_val_if_fail (item && item->cols >= 0 && item->rows >= 0, NULL);
	a11y = g_object_new (gal_a11y_e_table_item_get_type (), NULL);

	atk_object_initialize (ATK_OBJECT (a11y), item);

	GET_PRIVATE (a11y)->parent = parent;
	GET_PRIVATE (a11y)->index_in_parent = index_in_parent;

	accessible  = ATK_OBJECT(a11y);
	accessible->role = ATK_ROLE_TREE_TABLE;

	/* Initialize cell data. */
	n = item->cols * item->rows;
	GET_PRIVATE (a11y)->cols = item->cols;
	GET_PRIVATE (a11y)->rows = item->rows;
	if (n > 0) {
		GET_PRIVATE (a11y)->cell_data = g_malloc0(n*sizeof(gpointer));
		/* memory error. */
		if ( GET_PRIVATE (a11y)->cell_data == NULL)
			return NULL;
	} else
		GET_PRIVATE (a11y)->cell_data = NULL;
		

        GET_PRIVATE (a11y)->columns = e_table_header_get_columns (item->header);                                                                                
        if ( GET_PRIVATE (a11y)->columns == NULL)
                return NULL;

	if (item) {
		g_signal_connect (G_OBJECT(item), "selection_model_removed",
				  G_CALLBACK (eti_a11y_selection_model_removed_cb), NULL);
		g_signal_connect (G_OBJECT(item), "selection_model_added",
				  G_CALLBACK (eti_a11y_selection_model_added_cb), NULL);
		if (item->selection)
			gal_a11y_e_table_item_ref_selection (a11y,
							     item->selection);

		/* find the TableItem's parent: table or tree */
		GET_PRIVATE (a11y)->widget = gtk_widget_get_parent (GTK_WIDGET (item->parent.canvas));
		if (E_IS_TREE (GET_PRIVATE (a11y)->widget)) {
			ETreeModel *model;
			model = e_tree_get_model (E_TREE (GET_PRIVATE (a11y)->widget));
			g_signal_connect (G_OBJECT(model), "node_changed",
					G_CALLBACK (eti_tree_model_node_changed_cb), item);
		} 
	}
	if (parent)
		g_object_ref (parent);

#if 0
	if (item)
		g_object_weak_ref (G_OBJECT (item),
				   unref_accessible,
				   a11y);
#endif

	return ATK_OBJECT (a11y);
}

static gboolean
gal_a11y_e_table_item_ref_selection (GalA11yETableItem *a11y,
				     ESelectionModel *selection)
{
	GalA11yETableItemPrivate *priv;

	g_return_val_if_fail (a11y && selection, FALSE);

	priv = GET_PRIVATE (a11y);
	priv->selection_change_id = g_signal_connect (
	    G_OBJECT(selection), "selection_changed",
	    G_CALLBACK (eti_a11y_selection_changed_cb), a11y);
	priv->cursor_change_id = g_signal_connect (
            G_OBJECT(selection), "cursor_changed",
	    G_CALLBACK (eti_a11y_cursor_changed_cb), a11y);

	priv->selection = selection;
	g_object_ref (selection);

	return TRUE;
}

static gboolean
gal_a11y_e_table_item_unref_selection (GalA11yETableItem *a11y)
{
	GalA11yETableItemPrivate *priv;

	g_return_val_if_fail (a11y, FALSE);

	priv = GET_PRIVATE (a11y);

	g_return_val_if_fail (priv->selection_change_id != 0, FALSE);
	g_return_val_if_fail (priv->cursor_change_id != 0, FALSE);


	g_signal_handler_disconnect (priv->selection,
				     priv->selection_change_id);
	g_signal_handler_disconnect (priv->selection,
				     priv->cursor_change_id);
	priv->cursor_change_id = 0;
	priv->selection_change_id = 0;

	g_object_unref (priv->selection);
	priv->selection = NULL;

	return TRUE;
}

/* callbacks */

static void
eti_a11y_selection_model_removed_cb (ETableItem *eti, ESelectionModel *selection,
				     gpointer data)
{
	AtkObject *atk_obj;
	GalA11yETableItem *a11y;

	g_return_if_fail (E_IS_TABLE_ITEM (eti));
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (eti));
	a11y = GAL_A11Y_E_TABLE_ITEM (atk_obj);

	if (selection == GET_PRIVATE (a11y)->selection)
		gal_a11y_e_table_item_unref_selection (a11y);
}

static void
eti_a11y_selection_model_added_cb (ETableItem *eti, ESelectionModel *selection,
				   gpointer data)
{
	AtkObject *atk_obj;
	GalA11yETableItem *a11y;

	g_return_if_fail (E_IS_TABLE_ITEM (eti));
	g_return_if_fail (E_IS_SELECTION_MODEL (selection));

	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (eti));
	a11y = GAL_A11Y_E_TABLE_ITEM (atk_obj);

	if (GET_PRIVATE (a11y)->selection)
		gal_a11y_e_table_item_unref_selection (a11y);
	gal_a11y_e_table_item_ref_selection (a11y, selection);
}

static void
eti_a11y_selection_changed_cb (ESelectionModel *selection, GalA11yETableItem *a11y)
{
	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	g_signal_emit_by_name (a11y, "selection_changed");
}

static void
eti_a11y_cursor_changed_cb (ESelectionModel *selection,
			    int row, int col,  GalA11yETableItem *a11y)
{
	AtkObject * cell;
	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	g_signal_emit_by_name (a11y, "selection_changed");

        cell = atk_table_ref_at (ATK_TABLE (a11y), row, col);
	if (cell != NULL) {
		gal_a11y_e_cell_add_state (GAL_A11Y_E_CELL (cell), ATK_STATE_FOCUSED, FALSE);

        	if (ATK_IS_OBJECT (cell))
                	g_signal_emit_by_name  (a11y,
                                        "active-descendant-changed",
                                        cell);
		atk_focus_tracker_notify (cell);
	}

}

/* atk selection */

static void atk_selection_interface_init (AtkSelectionIface *iface)
{
	g_return_if_fail (iface != NULL);
	iface->add_selection = selection_add_selection;
	iface->clear_selection = selection_clear_selection;
	iface->ref_selection = selection_ref_selection;
	iface->get_selection_count = selection_get_selection_count;
	iface->is_child_selected = selection_is_child_selected;
}

static gboolean
selection_add_selection (AtkSelection *selection, gint index)
{
	AtkTable *table;
	gint row, col;
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (selection)));
	if (!item)
		return FALSE;

	table = ATK_TABLE (selection);

	row = atk_table_get_row_at_index (table, index);
	atk_table_add_row_selection (table, row);

	col = atk_table_get_column_at_index (table, index);
	e_selection_model_change_cursor (item->selection,
					 view_to_model_row (item, row),
					 view_to_model_col (item, col));
	e_selection_model_cursor_changed (item->selection,
					  view_to_model_row (item, row),
					  view_to_model_col (item, col));
	e_selection_model_cursor_activated (item->selection,
					    view_to_model_row (item, row),
					    view_to_model_col (item, col));
	return TRUE;
}

static gboolean
selection_clear_selection (AtkSelection *selection)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (selection)));
	if (!item)
		return FALSE;

	e_selection_model_clear (item->selection);
	return TRUE;
}

static AtkObject *
selection_ref_selection (AtkSelection *selection, gint index)
{
	AtkTable *table;
	gint row, col;

	table = ATK_TABLE (selection);
	row = atk_table_get_row_at_index (table, index);
	col = atk_table_get_column_at_index (table, index);
	if (!atk_table_is_row_selected (table, row))
		return NULL;

	return atk_table_ref_at (table, row, col);
}

static gint
selection_get_selection_count (AtkSelection *selection)
{
	AtkTable *table;
	gint n_selected;

	table = ATK_TABLE (selection);
	n_selected = atk_table_get_selected_rows (table, NULL);
	if (n_selected > 0)
		n_selected *= atk_table_get_n_columns (table);
	return n_selected;
}

static gboolean
selection_is_child_selected (AtkSelection *selection, gint i)
{
	gint row;

	row = atk_table_get_row_at_index (ATK_TABLE (selection), i);
	return atk_table_is_row_selected (ATK_TABLE (selection), row);
}

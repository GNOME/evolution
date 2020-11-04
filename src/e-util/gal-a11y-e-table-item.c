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
 *		Christopher James Lahey <clahey@ximian.com>
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-table-item.h"

#include <string.h>

#include <atk/atk.h>

#include "e-canvas.h"
#include "e-selection-model.h"
#include "e-table-click-to-add.h"
#include "e-table-subset.h"
#include "e-table.h"
#include "e-tree.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-e-cell.h"
#include "gal-a11y-e-table-click-to-add.h"
#include "gal-a11y-e-table-column-header.h"
#include "gal-a11y-e-table-item-factory.h"
#include "gal-a11y-util.h"

static GObjectClass *parent_class;
static AtkComponentIface *component_parent_iface;
static GType parent_type;
static gint priv_offset;
static GQuark		quark_accessible_object = 0;
#define GET_PRIVATE(object) \
	((GalA11yETableItemPrivate *) (((gchar *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETableItemPrivate {
	ETableItem *item;
	gint cols;
	gint rows;
	gulong selection_changed_id;
	gulong selection_row_changed_id;
	gulong cursor_changed_id;
	ETableCol ** columns;
	ESelectionModel *selection;
	AtkStateSet *state_set;
	GtkWidget *widget;
	GHashTable *a11y_column_headers; /* ETableCol * ~> GalA11yETableColumnHeader * */
};

static gboolean gal_a11y_e_table_item_ref_selection (GalA11yETableItem *a11y,
						     ESelectionModel *selection);
static gboolean gal_a11y_e_table_item_unref_selection (GalA11yETableItem *a11y);

static AtkObject * eti_ref_at (AtkTable *table, gint row, gint column);

static void
free_columns (ETableCol **columns)
{
	gint ii;

	if (!columns)
		return;

	for (ii = 0; columns[ii]; ii++) {
		g_object_unref (columns[ii]);
	}

	g_free (columns);
}

static void
table_item_cell_gone_cb (gpointer user_data,
			 GObject *gone_cell)
{
	GalA11yETableItem *a11y;
	GObject *old_cell;

	a11y = GAL_A11Y_E_TABLE_ITEM (user_data);

	old_cell = g_object_get_data (G_OBJECT (a11y), "gail-focus-object");
	if (old_cell == gone_cell)
		g_object_set_data (G_OBJECT (a11y), "gail-focus-object", NULL);
}

static void
item_finalized (gpointer user_data,
                GObject *gone_item)
{
	GalA11yETableItem *a11y;
	GalA11yETableItemPrivate *priv;
	GObject *old_cell;

	a11y = GAL_A11Y_E_TABLE_ITEM (user_data);
	priv = GET_PRIVATE (a11y);

	priv->item = NULL;

	old_cell = g_object_get_data (G_OBJECT (a11y), "gail-focus-object");
	if (old_cell) {
		g_object_weak_unref (G_OBJECT (old_cell), table_item_cell_gone_cb, a11y);
		g_object_unref (old_cell);
	}
	g_object_set_data (G_OBJECT (a11y), "gail-focus-object", NULL);

	if (atk_state_set_add_state (priv->state_set, ATK_STATE_DEFUNCT))
		atk_object_notify_state_change (ATK_OBJECT (a11y), ATK_STATE_DEFUNCT, TRUE);

	if (priv->selection)
		gal_a11y_e_table_item_unref_selection (a11y);

	g_clear_pointer (&priv->columns, free_columns);
	g_object_unref (a11y);
}

static void
gal_a11y_e_table_item_state_change_cb (AtkObject *atkobject,
				       const gchar *state_name,
				       gboolean was_set,
				       gpointer user_data)
{
	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (atkobject));

	if (atk_state_type_for_name (state_name) == ATK_STATE_DEFUNCT) {
		GalA11yETableItemPrivate *priv;

		priv = GET_PRIVATE (atkobject);

		g_return_if_fail (priv != NULL);

		if (was_set)
			atk_state_set_add_state (priv->state_set, ATK_STATE_DEFUNCT);
		else
			atk_state_set_remove_state (priv->state_set, ATK_STATE_DEFUNCT);
	}
}

static AtkStateSet *
eti_ref_state_set (AtkObject *accessible)
{
	GalA11yETableItemPrivate *priv = GET_PRIVATE (accessible);

	g_object_ref (priv->state_set);

	return priv->state_set;
}

inline static gint
view_to_model_row (ETableItem *eti,
                   gint row)
{
	if (eti->uses_source_model) {
		ETableSubset *etss = E_TABLE_SUBSET (eti->table_model);
		if (row >= 0 && row < etss->n_map) {
			eti->row_guess = row;
			return etss->map_table[row];
		} else
			return -1;
	} else
		return row;
}

inline static gint
view_to_model_col (ETableItem *eti,
                   gint view_col)
{
	ETableCol *ecol = e_table_header_get_column (eti->header, view_col);

	return (ecol != NULL) ? ecol->spec->model_col : -1;
}

inline static gint
model_to_view_row (ETableItem *eti,
                   gint row)
{
	gint i;
	if (row == -1)
		return -1;
	if (eti->uses_source_model) {
		ETableSubset *etss = E_TABLE_SUBSET (eti->table_model);
		if (eti->row_guess >= 0 && eti->row_guess < etss->n_map) {
			if (etss->map_table[eti->row_guess] == row) {
				return eti->row_guess;
			}
		}
		for (i = 0; i < etss->n_map; i++) {
			if (etss->map_table[i] == row)
				return i;
		}
		return -1;
	} else
		return row;
}

inline static gint
model_to_view_col (ETableItem *eti,
                   gint model_col)
{
	gint i;
	if (model_col == -1)
		return -1;
	for (i = 0; i < eti->cols; i++) {
		ETableCol *ecol = e_table_header_get_column (eti->header, i);
		if (ecol->spec->model_col == model_col)
			return i;
	}
	return -1;
}

inline static GObject *
eti_a11y_get_gobject (AtkObject *accessible)
{
	return atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
}

static void
eti_a11y_reset_focus_object (GalA11yETableItem *a11y,
                             ETableItem *item,
                             gboolean notify)
{
	ESelectionModel * esm;
	gint cursor_row, cursor_col, view_row, view_col;
	AtkObject *cell, *old_cell;

	esm = item->selection;
	g_return_if_fail (esm);

	cursor_row = e_selection_model_cursor_row (esm);
	cursor_col = e_selection_model_cursor_col (esm);

	view_row = model_to_view_row (item, cursor_row);
	view_col = model_to_view_col (item, cursor_col);

	if (view_row == -1)
		view_row = 0;
	if (view_col == -1)
		view_col = 0;

	old_cell = (AtkObject *) g_object_get_data (G_OBJECT (a11y), "gail-focus-object");
	if (old_cell && GAL_A11Y_IS_E_CELL (old_cell))
		gal_a11y_e_cell_remove_state (
			GAL_A11Y_E_CELL (old_cell), ATK_STATE_FOCUSED, FALSE);
	if (old_cell) {
		g_object_weak_unref (G_OBJECT (old_cell), table_item_cell_gone_cb, a11y);
		g_object_unref (old_cell);
	}

	cell = eti_ref_at (ATK_TABLE (a11y), view_row, view_col);

	if (cell != NULL) {
		g_object_set_data (G_OBJECT (a11y), "gail-focus-object", cell);
		gal_a11y_e_cell_add_state (
			GAL_A11Y_E_CELL (cell), ATK_STATE_FOCUSED, FALSE);
		g_object_weak_ref (G_OBJECT (cell), table_item_cell_gone_cb, a11y);
	} else
		g_object_set_data (G_OBJECT (a11y), "gail-focus-object", NULL);

	if (notify && cell)
		g_signal_emit_by_name (a11y, "active-descendant-changed", cell);
}

static void eti_column_header_a11y_gone (gpointer user_data, GObject *a11y_col_header);

static void
eti_table_column_gone (gpointer user_data,
		       GObject *col)
{
	GalA11yETableItem *a11y = user_data;
	GalA11yETableItemPrivate *priv;
	GalA11yETableColumnHeader *a11y_col_header;

	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	priv = GET_PRIVATE (a11y);

	a11y_col_header = g_hash_table_lookup (priv->a11y_column_headers, col);
	g_hash_table_remove (priv->a11y_column_headers, col);

	if (a11y_col_header)
		g_object_weak_unref (G_OBJECT (a11y_col_header), eti_column_header_a11y_gone, a11y);
}

static void
eti_column_header_a11y_gone (gpointer user_data,
			     GObject *a11y_col_header)
{
	GalA11yETableItem *a11y = user_data;
	GalA11yETableItemPrivate *priv;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	priv = GET_PRIVATE (a11y);

	g_hash_table_iter_init (&iter, priv->a11y_column_headers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ETableCol *col = key;
		GalA11yETableColumnHeader *stored_a11y_col_header = value;

		if (((GObject *) stored_a11y_col_header) == a11y_col_header) {
			g_object_weak_unref (G_OBJECT (col), eti_table_column_gone, a11y);
			g_hash_table_remove (priv->a11y_column_headers, col);
			break;
		}
	}
}

static void
eti_dispose (GObject *object)
{
	GalA11yETableItem *a11y = GAL_A11Y_E_TABLE_ITEM (object);
	GalA11yETableItemPrivate *priv = GET_PRIVATE (a11y);
	GHashTableIter iter;
	gpointer key, value;

	g_clear_pointer (&priv->columns, free_columns);

	if (priv->item) {
		g_object_weak_unref (G_OBJECT (priv->item), item_finalized, a11y);
		priv->item = NULL;
	}

	g_clear_object (&priv->state_set);

	g_hash_table_iter_init (&iter, priv->a11y_column_headers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		ETableCol *col = key;
		GalA11yETableColumnHeader *a11y_col_header = value;

		g_object_weak_unref (G_OBJECT (col), eti_table_column_gone, a11y);
		g_object_weak_unref (G_OBJECT (a11y_col_header), eti_column_header_a11y_gone, a11y);
	}

	g_hash_table_remove_all (priv->a11y_column_headers);

	if (parent_class->dispose)
		parent_class->dispose (object);
}

static void
eti_finalize (GObject *object)
{
	GalA11yETableItem *a11y = GAL_A11Y_E_TABLE_ITEM (object);
	GalA11yETableItemPrivate *priv = GET_PRIVATE (a11y);

	g_hash_table_destroy (priv->a11y_column_headers);

	if (parent_class->finalize)
		parent_class->finalize (object);
}

/* Static functions */
static gint
eti_get_n_children (AtkObject *accessible)
{
	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), 0);
	if (!eti_a11y_get_gobject (accessible))
		return 0;

	return atk_table_get_n_columns (ATK_TABLE (accessible)) *
		(atk_table_get_n_rows (ATK_TABLE (accessible)) + 1);
}

static AtkObject *
eti_ref_child (AtkObject *accessible,
               gint index)
{
	ETableItem *item;
	gint col, row;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), NULL);
	item = E_TABLE_ITEM (eti_a11y_get_gobject (accessible));
	if (!item)
		return NULL;

	if (index < item->cols) {
		GalA11yETableItemPrivate *priv = GET_PRIVATE (accessible);
		ETableCol *ecol;
		AtkObject *child;

		ecol = e_table_header_get_column (item->header, index);
		child = g_hash_table_lookup (priv->a11y_column_headers, ecol);

		if (!child) {
			child = gal_a11y_e_table_column_header_new (ecol, item, accessible);
			if (child) {
				g_hash_table_insert (priv->a11y_column_headers, ecol, child);

				g_object_weak_ref (G_OBJECT (ecol), eti_table_column_gone, accessible);
				g_object_weak_ref (G_OBJECT (child), eti_column_header_a11y_gone, accessible);
			}
		}

		return child ? g_object_ref (child) : NULL;
	}
	index -= item->cols;

	col = index % item->cols;
	row = index / item->cols;

	return eti_ref_at (ATK_TABLE (accessible), row, col);
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
	AtkObject *parent;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (component)));
	if (!item)
		return;

	parent = ATK_OBJECT (component)->accessible_parent;
	if (parent && ATK_IS_COMPONENT (parent))
		atk_component_get_extents (
			ATK_COMPONENT (parent),
			x, y,
			width, height,
			coord_type);

	if (parent && GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD (parent)) {
		ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (
			atk_gobject_accessible_get_object (
			ATK_GOBJECT_ACCESSIBLE (parent)));
		if (etcta) {
			*width = etcta->width;
			*height = etcta->height;
		}
	}
}

static AtkObject *
eti_ref_accessible_at_point (AtkComponent *component,
                             gint x,
                             gint y,
                             AtkCoordType coord_type)
{
	gint row = -1;
	gint col = -1;
	gint x_origin, y_origin;
	ETableItem *item;
	GtkWidget *tableOrTree;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (component)));
	if (!item)
		return NULL;

	atk_component_get_extents (
		component,
		&x_origin,
		&y_origin,
		NULL,
		NULL,
		coord_type);
	x -= x_origin;
	y -= y_origin;

	tableOrTree = gtk_widget_get_parent (GTK_WIDGET (item->parent.canvas));

	if (E_IS_TREE (tableOrTree))
		e_tree_get_cell_at (E_TREE (tableOrTree), x, y, &row, &col);
	else
		e_table_get_cell_at (E_TABLE (tableOrTree), x, y, &row, &col);

	if (row != -1 && col != -1) {
		return eti_ref_at (ATK_TABLE (component), row, col);
	} else {
		return NULL;
	}
}

/* atk table */
static AtkObject *
eti_ref_at (AtkTable *table,
            gint row,
            gint column)
{
	ETableItem *item;
	AtkObject * ret;
	GalA11yETableItemPrivate *priv = GET_PRIVATE (table);

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return NULL;

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
		ret = gal_a11y_e_cell_registry_get_object (
			NULL,
			item,
			cell_view,
			ATK_OBJECT (table),
			ecol->spec->model_col,
			column,
			row);
		if (ATK_IS_OBJECT (ret)) {
			/* if current cell is focused, add FOCUSED state */
			if (e_selection_model_cursor_row (item->selection) ==
				GAL_A11Y_E_CELL (ret)->row &&
				e_selection_model_cursor_col (item->selection) ==
				GAL_A11Y_E_CELL (ret)->model_col)
				gal_a11y_e_cell_add_state (
					GAL_A11Y_E_CELL (ret),
					ATK_STATE_FOCUSED, FALSE);
		} else
			ret = NULL;

		return ret;
	}

	return NULL;
}

static gint
eti_get_index_at (AtkTable *table,
                  gint row,
                  gint column)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return column + (row + 1) * item->cols;
}

static gint
eti_get_column_at_index (AtkTable *table,
                         gint index)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return index % item->cols;
}

static gint
eti_get_row_at_index (AtkTable *table,
                      gint index)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	return index / item->cols - 1;
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
	gint width;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	e_table_item_get_cell_geometry (
		item,
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
	gint height;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return -1;

	e_table_item_get_cell_geometry (
		item,
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

static const gchar *
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
eti_get_column_header (AtkTable *table,
                       gint column)
{
	ETableItem *item;
	ETableCol *ecol;
	AtkObject *atk_obj = NULL;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return NULL;

	ecol = e_table_header_get_column (item->header, column);
	if (ecol) {
		atk_obj = gal_a11y_e_table_column_header_new (ecol, item, ATK_OBJECT (table));
	}

	return atk_obj;
}

static const gchar *
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
table_is_row_selected (AtkTable *table,
                       gint row)
{
	ETableItem *item;
	GalA11yETableItemPrivate *priv = GET_PRIVATE (table);

	if (row < 0)
		return FALSE;

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return FALSE;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	return e_selection_model_is_row_selected (
		item->selection, view_to_model_row (item, row));
}

static gboolean
table_is_selected (AtkTable *table,
                   gint row,
                   gint column)
{
	return table_is_row_selected (table, row);
}

static gint
table_get_selected_rows (AtkTable *table,
                         gint **rows_selected)
{
	ETableItem *item;
	gint n_selected, row, index_selected;
	GalA11yETableItemPrivate *priv = GET_PRIVATE (table);

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return 0;

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
table_add_row_selection (AtkTable *table,
                         gint row)
{
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	if (table_is_row_selected (table, row))
		return TRUE;
	e_selection_model_toggle_single_row (
		item->selection,
		view_to_model_row (item, row));

	return TRUE;
}

static gboolean
table_remove_row_selection (AtkTable *table,
                            gint row)
{
	ETableItem *item;
	GalA11yETableItemPrivate *priv = GET_PRIVATE (table);

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return FALSE;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	if (!atk_table_is_row_selected (table, row))
		return TRUE;

	e_selection_model_toggle_single_row (
		item->selection, view_to_model_row (item, row));

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
	component_parent_iface = g_type_interface_peek_parent (iface);

	iface->ref_accessible_at_point = eti_ref_accessible_at_point;
	iface->get_extents = eti_get_extents;
}

static void
eti_rows_inserted (ETableModel *model,
                   gint row,
                   gint count,
                   AtkObject *table_item)
{
	gint n_cols,n_rows,i,j;
	GalA11yETableItem * item_a11y;
	gint old_nrows;

	g_return_if_fail (table_item);
	item_a11y = GAL_A11Y_E_TABLE_ITEM (table_item);

	n_cols = atk_table_get_n_columns (ATK_TABLE (table_item));
	n_rows = atk_table_get_n_rows (ATK_TABLE (table_item));

	old_nrows = GET_PRIVATE (item_a11y)->rows;

	g_return_if_fail (n_cols > 0 && n_rows > 0);
	g_return_if_fail (old_nrows == n_rows - count);

	GET_PRIVATE (table_item)->rows = n_rows;

	g_signal_emit_by_name (
		table_item, "row-inserted", row,
		count, NULL);

	for (i = row; i < (row + count); i++) {
		for (j = 0; j < n_cols; j++) {
			g_signal_emit_by_name (
				table_item,
				"children_changed::add",
				(((i + 1) * n_cols) + j), NULL, NULL);
		}
	}

	g_signal_emit_by_name (table_item, "visible-data-changed");
}

static void
eti_rows_deleted (ETableModel *model,
                  gint row,
                  gint count,
                  AtkObject *table_item)
{
	gint i,j, n_rows, n_cols, old_nrows;
	ETableItem *item = E_TABLE_ITEM (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (table_item)));

	n_rows = atk_table_get_n_rows (ATK_TABLE (table_item));
	n_cols = atk_table_get_n_columns (ATK_TABLE (table_item));

	old_nrows = GET_PRIVATE (table_item)->rows;

	g_return_if_fail (row + count <= old_nrows);
	g_return_if_fail (old_nrows == n_rows + count);
	GET_PRIVATE (table_item)->rows = n_rows;

	g_signal_emit_by_name (
		table_item, "row-deleted", row,
		count, NULL);

	for (i = row; i < (row + count); i++) {
		for (j = 0; j < n_cols; j++) {
			g_signal_emit_by_name (
				table_item,
				"children_changed::remove",
				(((i + 1) * n_cols) + j), NULL, NULL);
		}
	}
	g_signal_emit_by_name (table_item, "visible-data-changed");
	eti_a11y_reset_focus_object ((GalA11yETableItem *) table_item, item, TRUE);
}

static void
eti_model_changed (ETableModel *model,
		   AtkObject *table_item)
{
	GalA11yETableItemPrivate *priv;
	gint row_count;

	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (table_item));

	priv = GET_PRIVATE (table_item);

	row_count = e_table_model_row_count (model);

	if (priv->rows != row_count) {
		priv->rows = row_count;
		g_signal_emit_by_name (table_item, "visible-data-changed");
	}
}

static void
eti_tree_model_node_changed_cb (ETreeModel *model,
                                ETreePath node,
                                ETableItem *eti)
{
	AtkObject *atk_obj;
	GalA11yETableItem *a11y;

	g_return_if_fail (E_IS_TABLE_ITEM (eti));

	atk_obj = atk_gobject_accessible_for_object (G_OBJECT (eti));
	a11y = GAL_A11Y_E_TABLE_ITEM (atk_obj);

	/* we can't figure out which rows are changed, so just send out a signal ... */
	if  (GET_PRIVATE (a11y)->rows > 0)
		g_signal_emit_by_name (a11y, "visible-data-changed");
}

enum {
        ETI_HEADER_UNCHANGED = 0,
        ETI_HEADER_REORDERED,
        ETI_HEADER_NEW_ADDED,
        ETI_HEADER_REMOVED
};

/*
 * 1. Check what actually happened: column reorder, remove or add
 * 2. Update cache
 * 3. Emit signals
 */
static void
eti_header_structure_changed (ETableHeader *eth,
                              AtkObject *a11y)
{

	gboolean reorder_found = FALSE, added_found = FALSE, removed_found = FALSE;
	GalA11yETableItem * a11y_item;
	ETableCol ** cols, **prev_cols;
	GalA11yETableItemPrivate *priv;
	gint *state = NULL, *prev_state = NULL, *reorder = NULL;
	gint i,j,n_rows,n_cols, prev_n_cols;

	a11y_item = GAL_A11Y_E_TABLE_ITEM (a11y);
	priv = GET_PRIVATE (a11y_item);

	/* Assume rows do not changed. */
	n_rows = priv->rows;

	prev_n_cols = priv->cols;
	prev_cols = priv->columns;

	cols = e_table_header_get_columns (eth);
	n_cols = eth->col_count;

	g_return_if_fail (cols && prev_cols && n_cols > 0);

	/* Init to ETI_HEADER_UNCHANGED. */
	state = g_malloc0 (sizeof (gint) * (MAX (prev_n_cols, n_cols) + 1));
	prev_state = g_malloc0 (sizeof (gint) * (MAX (prev_n_cols, n_cols) + 1));
	reorder = g_malloc0 (sizeof (gint) * (MAX (prev_n_cols, n_cols) + 1));

        /* Compare with previously saved column headers. */
	for (i = 0; i < n_cols && cols[i]; i++) {
		for (j = 0; j < prev_n_cols && prev_cols[j]; j++) {
			if (prev_cols[j] == cols[i] && i != j) {

				reorder_found = TRUE;
				state[i] = ETI_HEADER_REORDERED;
				reorder[i] = j;

				break;
			} else if (prev_cols[j] == cols[i]) {
                                /* OK, this column is not changed. */
				break;
			}
		}

                /* cols[i] is new added column. */
		if (j == prev_n_cols) {
			added_found = TRUE;
			state[i] = ETI_HEADER_NEW_ADDED;
		}
	}

        /* Now try to find if there are removed columns. */
	for (i = 0; i < prev_n_cols && prev_cols[i]; i++) {
		for (j = 0; j < n_cols && cols[j]; j++)
			if (prev_cols[j] == cols[i])
				break;

                /* Removed columns found. */
		if (j == n_cols) {
			removed_found = TRUE;
			prev_state[j] = ETI_HEADER_REMOVED;
		}
	}

	/* If nothing interesting just return. */
	if (!reorder_found && !added_found && !removed_found) {
		g_free (state);
		g_free (reorder);
		g_free (prev_state);
		free_columns (cols);
		return;
	}

	/* Emit signals */
	if (reorder_found)
		g_signal_emit_by_name (a11y_item, "column_reordered");

	if (removed_found) {
		for (i = 0; i < prev_n_cols; i++) {
			if (prev_state[i] == ETI_HEADER_REMOVED) {
				g_signal_emit_by_name (
					a11y_item, "column-deleted", i, 1);
				for (j = 0; j < n_rows; j++)
					g_signal_emit_by_name (
						a11y_item,
						"children_changed::remove",
						((j + 1) * prev_n_cols + i),
						NULL, NULL);
			}
		}
	}

	if (added_found) {
		for (i = 0; i < n_cols; i++) {
			if (state[i] == ETI_HEADER_NEW_ADDED) {
				g_signal_emit_by_name (
					a11y_item, "column-inserted", i, 1);
				for (j = 0; j < n_rows; j++)
					g_signal_emit_by_name (
						a11y_item,
						"children_changed::add",
						((j + 1) * n_cols + i),
						NULL, NULL);
			}
		}
	}

	priv->cols = n_cols;

	g_free (state);
	g_free (reorder);
	g_free (prev_state);

	free_columns (priv->columns);
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

	g_signal_connect_object (
		model, "model-rows-inserted",
		G_CALLBACK (eti_rows_inserted), obj, 0);
	g_signal_connect_object (
		model, "model-rows-deleted",
		G_CALLBACK (eti_rows_deleted), obj, 0);
	g_signal_connect_object (
		model, "model-changed",
		G_CALLBACK (eti_model_changed), obj, 0);
	g_signal_connect_object (
		eti->header, "structure_change",
		G_CALLBACK (eti_header_structure_changed), obj, 0);
}

static void
eti_class_init (GalA11yETableItemClass *class)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	quark_accessible_object =
		g_quark_from_static_string ("gtk-accessible-object");

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = eti_dispose;
	object_class->finalize = eti_finalize;

	atk_object_class->get_n_children = eti_get_n_children;
	atk_object_class->ref_child = eti_ref_child;
	atk_object_class->initialize = eti_real_initialize;
	atk_object_class->ref_state_set = eti_ref_state_set;
}

static void
eti_init (GalA11yETableItem *a11y)
{
	GalA11yETableItemPrivate *priv;

	priv = GET_PRIVATE (a11y);

	priv->selection_changed_id = 0;
	priv->selection_row_changed_id = 0;
	priv->cursor_changed_id = 0;
	priv->selection = NULL;
	priv->a11y_column_headers = g_hash_table_new (g_direct_hash, g_direct_equal);
}

/* atk selection */

static void	atk_selection_interface_init	(AtkSelectionIface *iface);
static gboolean	selection_add_selection		(AtkSelection *selection,
						 gint i);
static gboolean	selection_clear_selection	(AtkSelection *selection);
static AtkObject *
		selection_ref_selection		(AtkSelection *selection,
						 gint i);
static gint	selection_get_selection_count	(AtkSelection *selection);
static gboolean	selection_is_child_selected	(AtkSelection *selection,
						 gint i);

/* callbacks */
static void eti_a11y_selection_model_removed_cb (ETableItem *eti,
						 ESelectionModel *selection,
						 gpointer data);
static void eti_a11y_selection_model_added_cb (ETableItem *eti,
					       ESelectionModel *selection,
					       gpointer data);
static void eti_a11y_selection_changed_cb (ESelectionModel *selection,
					   GalA11yETableItem *a11y);
static void eti_a11y_selection_row_changed_cb (ESelectionModel *selection,
					   gint row,
					   GalA11yETableItem *a11y);
static void eti_a11y_cursor_changed_cb (ESelectionModel *selection,
					gint row, gint col,
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

		factory = atk_registry_get_factory (
			atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (
			PARENT_TYPE, "GalA11yETableItem", &info, 0,
			sizeof (GalA11yETableItemPrivate), &priv_offset);

		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
		g_type_add_interface_static (type, ATK_TYPE_TABLE, &atk_table_info);
		g_type_add_interface_static (type, ATK_TYPE_SELECTION, &atk_selection_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_table_item_new (ETableItem *item)
{
	GalA11yETableItem *a11y;
	AtkObject *accessible;
	ESelectionModel * esm;
	AtkObject *parent;
	const gchar *name;

	g_return_val_if_fail (item && item->cols >= 0, NULL);
	a11y = g_object_new (gal_a11y_e_table_item_get_type (), NULL);

	atk_object_initialize (ATK_OBJECT (a11y), item);

	GET_PRIVATE (a11y)->state_set = atk_state_set_new ();

	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_MANAGES_DESCENDANTS);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_SHOWING);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_VISIBLE);

	g_signal_connect (a11y, "state-change", G_CALLBACK (gal_a11y_e_table_item_state_change_cb), NULL);

	accessible = ATK_OBJECT (a11y);

	/* Initialize cell data. */
	GET_PRIVATE (a11y)->cols = item->cols;
	GET_PRIVATE (a11y)->rows = item->rows >= 0 ? item->rows : 0;

	GET_PRIVATE (a11y)->columns = e_table_header_get_columns (item->header);
	if (GET_PRIVATE (a11y)->columns == NULL) {
		g_clear_object (&a11y);
		return NULL;
	}

	GET_PRIVATE (a11y)->item = item;

	g_signal_connect (
		item, "selection_model_removed",
		G_CALLBACK (eti_a11y_selection_model_removed_cb), NULL);
	g_signal_connect (
		item, "selection_model_added",
		G_CALLBACK (eti_a11y_selection_model_added_cb), NULL);
	if (item->selection)
		gal_a11y_e_table_item_ref_selection (
			a11y,
			item->selection);

	/* find the TableItem's parent: table or tree */
	GET_PRIVATE (a11y)->widget = gtk_widget_get_parent (
		GTK_WIDGET (item->parent.canvas));
	parent = gtk_widget_get_accessible (GET_PRIVATE (a11y)->widget);
	name = atk_object_get_name (parent);
	if (name)
		atk_object_set_name (accessible, name);
	atk_object_set_parent (accessible, parent);

	if (E_IS_TREE (GET_PRIVATE (a11y)->widget)) {
		ETreeModel *model;
		model = e_tree_get_model (E_TREE (GET_PRIVATE (a11y)->widget));
		g_signal_connect (
			model, "node_changed",
			G_CALLBACK (eti_tree_model_node_changed_cb), item);
		accessible->role = ATK_ROLE_TREE_TABLE;
	} else if (E_IS_TABLE (GET_PRIVATE (a11y)->widget)) {
		accessible->role = ATK_ROLE_TABLE;
	}

	g_object_weak_ref (G_OBJECT (item), item_finalized, g_object_ref (a11y));

	esm = item->selection;

	if (esm != NULL) {
		eti_a11y_reset_focus_object (a11y, item, FALSE);
	}

	return ATK_OBJECT (a11y);
}

static gboolean
gal_a11y_e_table_item_ref_selection (GalA11yETableItem *a11y,
                                     ESelectionModel *selection)
{
	GalA11yETableItemPrivate *priv;

	g_return_val_if_fail (a11y && selection, FALSE);

	priv = GET_PRIVATE (a11y);
	priv->selection_changed_id = g_signal_connect (
		selection, "selection-changed",
		G_CALLBACK (eti_a11y_selection_changed_cb), a11y);
	priv->selection_row_changed_id = g_signal_connect (
		selection, "selection-row-changed",
		G_CALLBACK (eti_a11y_selection_row_changed_cb), a11y);
	priv->cursor_changed_id = g_signal_connect (
		selection, "cursor-changed",
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

	g_return_val_if_fail (priv->selection_changed_id != 0, FALSE);
	g_return_val_if_fail (priv->selection_row_changed_id != 0, FALSE);
	g_return_val_if_fail (priv->cursor_changed_id != 0, FALSE);

	g_signal_handler_disconnect (
		priv->selection,
		priv->selection_changed_id);
	g_signal_handler_disconnect (
		priv->selection,
		priv->selection_row_changed_id);
	g_signal_handler_disconnect (
		priv->selection,
		priv->cursor_changed_id);
	priv->cursor_changed_id = 0;
	priv->selection_row_changed_id = 0;
	priv->selection_changed_id = 0;

	g_object_unref (priv->selection);
	priv->selection = NULL;

	return TRUE;
}

/* callbacks */

static void
eti_a11y_selection_model_removed_cb (ETableItem *eti,
                                     ESelectionModel *selection,
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
eti_a11y_selection_model_added_cb (ETableItem *eti,
                                   ESelectionModel *selection,
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
eti_a11y_selection_changed_cb (ESelectionModel *selection,
                               GalA11yETableItem *a11y)
{
	GalA11yETableItemPrivate *priv = GET_PRIVATE (a11y);

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return;

	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	g_signal_emit_by_name (a11y, "selection_changed");
}

static void
eti_a11y_selection_row_changed_cb (ESelectionModel *selection,
				   gint row,
				   GalA11yETableItem *a11y)
{
	eti_a11y_selection_changed_cb (selection, a11y);
}

static void
eti_a11y_cursor_changed_cb (ESelectionModel *selection,
                            gint row,
                            gint col,
                            GalA11yETableItem *a11y)
{
	ETableItem *item;
	GalA11yETableItemPrivate *priv = GET_PRIVATE (a11y);

	g_return_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (a11y));

	if (atk_state_set_contains_state (priv->state_set, ATK_STATE_DEFUNCT))
		return;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (a11y)));

	g_return_if_fail (item);

	if (row == -1 && col == -1)
		return;
	eti_a11y_reset_focus_object (a11y, item, TRUE);
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
selection_add_selection (AtkSelection *selection,
                         gint index)
{
	AtkTable *table;
	gint row, col, cursor_row, cursor_col, model_row, model_col;
	ETableItem *item;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (selection)));
	if (!item)
		return FALSE;

	table = ATK_TABLE (selection);

	row = atk_table_get_row_at_index (table, index);
	col = atk_table_get_column_at_index (table, index);

	model_row = view_to_model_row (item, row);
	model_col = view_to_model_col (item, col);

	cursor_row = e_selection_model_cursor_row (item->selection);
	cursor_col = e_selection_model_cursor_col (item->selection);

	/* check whether is selected already */
	if (model_row == cursor_row && model_col == cursor_col)
		return TRUE;

	if (model_row != cursor_row) {
		/* we need to make the item get focus */
		e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (item), TRUE);

		/* FIXME, currently we only support single row selection */
		atk_selection_clear_selection (selection);
		atk_table_add_row_selection (table, row);
	}

	e_selection_model_change_cursor (
		item->selection,
		model_row,
		model_col);
	e_selection_model_cursor_changed (
		item->selection,
		model_row,
		model_col);
	e_selection_model_cursor_activated (
		item->selection,
		model_row,
		model_col);
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
selection_ref_selection (AtkSelection *selection,
                         gint index)
{
	AtkTable *table;
	gint row, col;

	table = ATK_TABLE (selection);
	row = atk_table_get_row_at_index (table, index);
	col = atk_table_get_column_at_index (table, index);
	if (!atk_table_is_row_selected (table, row))
		return NULL;

	return eti_ref_at (table, row, col);
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
selection_is_child_selected (AtkSelection *selection,
                             gint i)
{
	gint row;

	row = atk_table_get_row_at_index (ATK_TABLE (selection), i);
	return atk_table_is_row_selected (ATK_TABLE (selection), row);
}

void
gal_a11y_e_table_item_init (void)
{
	if (atk_get_root ())
		atk_registry_set_factory_type (
			atk_get_default_registry (),
			E_TYPE_TABLE_ITEM,
			gal_a11y_e_table_item_factory_get_type ());
}


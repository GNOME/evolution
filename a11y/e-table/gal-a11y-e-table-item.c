/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *   Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-e-cell-registry.h"
#include "gal-a11y-util.h"
#include <gal/e-table/e-table-subset.h>

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
};

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

#if 0
	if (priv->item)
		g_object_weak_unref (G_OBJECT (priv->item), unref_accessible, a11y);

	if (priv->parent)
		g_object_unref (priv->parent);
#endif
	priv->parent = NULL;

	if (parent_class->dispose)
		parent_class->dispose (object);
}

/* Static functions */
static AtkObject *
eti_get_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
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
	AtkGObjectAccessible *atk_gobj;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), 0);
	if (!eti_a11y_get_gobject (accessible))
		return 0;

	return atk_table_get_n_columns (ATK_TABLE (accessible)) *
		atk_table_get_n_rows (ATK_TABLE (accessible));
}

static AtkObject*
eti_ref_child (AtkObject *accessible, gint i)
{
	AtkGObjectAccessible *atk_gobj;
	ETableItem *item;
	gint col, row;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_ITEM (accessible), NULL);
	item = E_TABLE_ITEM (eti_a11y_get_gobject (accessible));
	if (!item)
		return NULL;

	col = i % item->cols;
	row = i / item->cols;

	return atk_table_ref_at (ATK_TABLE (accessible), row, col);
}

static gint
eti_get_index_in_parent (AtkObject *accessible)
{
	AtkGObjectAccessible *atk_gobj;
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
		return;

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


/* atk table */

static AtkObject*
eti_ref_at (AtkTable *table, gint row, gint column)
{
	ETableItem *item;

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
		return gal_a11y_e_cell_registry_get_object (NULL,
							    item,
							    cell_view,
							    ATK_OBJECT (table),
							    ecol->col_idx,
							    column,
							    row);
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
eti_get_column_header (AtkTable *table,
		       gint column)
{
	/* Unimplemented */
	return NULL;
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
	gint cursor_row, cursor_col, row_count;
	GdkModifierType state = GDK_CONTROL_MASK;
	ESelectionModel *selection;

	item = E_TABLE_ITEM (eti_a11y_get_gobject (ATK_OBJECT (table)));
	if (!item)
		return FALSE;

	if (table_is_row_selected (table, row))
		return TRUE;
	e_selection_model_toggle_single_row (item->selection, view_to_model_row (item, row));

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
}

static void
eti_init (GalA11yETableItem *a11y)
{
	GalA11yETableItemPrivate *priv;

	priv = GET_PRIVATE (a11y);

	priv->parent = NULL;
	priv->index_in_parent = -1;
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

	a11y = g_object_new (gal_a11y_e_table_item_get_type (), NULL);

	atk_object_initialize (ATK_OBJECT (a11y), item);

	GET_PRIVATE (a11y)->parent = parent;
	GET_PRIVATE (a11y)->index_in_parent = index_in_parent;

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

static AtkObject*
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

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-cell.h"
#include "gal-a11y-util.h"
#include <atk/atkobject.h>
#include <atk/atkcomponent.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yECellClass))
static GObjectClass *parent_class;
#define PARENT_TYPE (atk_object_get_type ())


#if 0
static void
unref_item (gpointer user_data, GObject *obj_loc)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (user_data);
	a11y->item = NULL;
	g_object_unref (a11y);
}

static void
unref_cell (gpointer user_data, GObject *obj_loc)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (user_data);
	a11y->cell_view = NULL;
	g_object_unref (a11y);
}
#endif 

static void
eti_dispose (GObject *object)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (object);

#if 0
	if (a11y->item)
		g_object_unref (G_OBJECT (a11y->item));  /*, unref_item, a11y); */
	if (a11y->cell_view)
		g_object_unref (G_OBJECT (a11y->cell_view)); /*, unref_cell, a11y); */
	if (a11y->parent)
		g_object_unref (a11y->parent);
#endif
	a11y->item = NULL;
	a11y->cell_view = NULL;
	a11y->parent = NULL;
	a11y->model_col = -1;
	a11y->view_col = -1;
	a11y->row = -1;

	if (parent_class->dispose)
		parent_class->dispose (object);
}

/* Static functions */
static AtkObject*
eti_get_parent (AtkObject *accessible)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (accessible);
	return a11y->parent;
}

static gint
eti_get_index_in_parent (AtkObject *accessible)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (accessible);

	return a11y->row * a11y->item->cols + a11y->view_col;
}


/* Component IFace */
static void
eti_get_extents (AtkComponent *component,
		gint *x,
		gint *y,
		gint *width,
		gint *height,
		AtkCoordType coord_type)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (component);
	int row;
	int col;
	int xval;
	int yval;

	row = a11y->row;
	col = a11y->view_col;


	e_table_item_get_cell_geometry (a11y->item,
					&row, 
					&col,
					&xval,
					&yval,
					width,
					height);

	atk_component_get_position (ATK_COMPONENT (a11y->parent),
				    x, y, coord_type);
	if (x && *x != G_MININT)
		*x += xval;
	if (y && *y != G_MININT)
		*y += yval;
}

/* Table IFace */

static void
eti_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->get_extents = eti_get_extents;
}

static void
eti_class_init (GalA11yECellClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class                          = g_type_class_ref (PARENT_TYPE);

	object_class->dispose                 = eti_dispose;

	atk_object_class->get_parent          = eti_get_parent;
	atk_object_class->get_index_in_parent = eti_get_index_in_parent;
}

static void
eti_init (GalA11yECell *a11y)
{
	a11y->item = NULL;
	a11y->cell_view = NULL;
	a11y->parent = NULL;
	a11y->model_col = -1;
	a11y->view_col = -1;
	a11y->row = -1;
}

/**
 * gal_a11y_e_cell_get_type:
 * @void: 
 * 
 * Registers the &GalA11yECell class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yECell class.
 **/
GType
gal_a11y_e_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) eti_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECell),
			0,
			(GInstanceInitFunc) eti_init,
			NULL /* value_cell */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) eti_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yECell", &info, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}
AtkObject *
gal_a11y_e_cell_new (ETableItem *item,
		     ECellView  *cell_view,
		     AtkObject  *parent,
		     int         model_col,
		     int         view_col,
		     int         row)
{
	AtkObject *a11y;

	a11y = g_object_new (gal_a11y_e_cell_get_type (), NULL);

	gal_a11y_e_cell_construct (a11y,
				   item,
				   cell_view,
				   parent,
				   model_col,
				   view_col,
				   row);
	return a11y;
}

void
gal_a11y_e_cell_construct (AtkObject  *object,
			   ETableItem *item,
			   ECellView  *cell_view,
			   AtkObject  *parent,
			   int         model_col,
			   int         view_col,
			   int         row)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (object);
	a11y->item      = item;
	a11y->cell_view = cell_view;
	a11y->parent    = parent;
	a11y->model_col = model_col;
	a11y->view_col  = view_col;
	a11y->row       = row;
	ATK_OBJECT (a11y) ->role	= ATK_ROLE_TABLE_CELL;

#if 0
	if (parent)
		g_object_ref (parent);

	if (item)
		g_object_ref (G_OBJECT (item)); /*,
						  unref_item,
						  a11y);*/
	if (cell_view)
		g_object_ref (G_OBJECT (cell_view)); /*,
						  unref_cell,
						  a11y);*/
#endif
}

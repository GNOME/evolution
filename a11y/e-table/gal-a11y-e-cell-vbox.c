/* Evolution Accessibility: gal-a11y-e-cell-vbox.c
 *
 * Copyright (C) 2004 Sun Microsystem, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Eric Zhao <eric.zhao@sun.com> Sun Microsystem Inc., 2004
 *
 */
#include "gal-a11y-e-cell-vbox.h"
#include "gal-a11y-e-cell-registry.h"
#include <gal/e-table/e-cell-vbox.h>
#include <atk/atkcomponent.h>

static GObjectClass *parent_class;
static AtkComponentIface *component_parent_iface;
#define PARENT_TYPE (gal_a11y_e_cell_get_type ())

static gint
ecv_get_n_children (AtkObject *a11y)
{
	g_return_val_if_fail (GAL_A11Y_IS_E_CELL_VBOX (a11y), 0);
	GalA11yECellVbox *gaev = GAL_A11Y_E_CELL_VBOX (a11y);
	return (gaev->a11y_subcell_count);
}

static void
subcell_destroyed (gpointer data)
{
	GalA11yECell *cell;
	AtkObject *parent;
	GalA11yECellVbox *gaev;
	
	g_return_if_fail (GAL_A11Y_IS_E_CELL (data));
	cell = GAL_A11Y_E_CELL (data);

	parent = atk_object_get_parent (ATK_OBJECT (cell));
	g_return_if_fail (GAL_A11Y_IS_E_CELL_VBOX (parent));
	gaev = GAL_A11Y_E_CELL_VBOX (parent);

	if (cell->view_col < gaev->a11y_subcell_count)
		gaev->a11y_subcells[cell->view_col] = NULL;
}

static AtkObject*
ecv_ref_child (AtkObject *a11y, gint i)
{
	GalA11yECellVbox *gaev = GAL_A11Y_E_CELL_VBOX (a11y);
	GalA11yECell *gaec = GAL_A11Y_E_CELL (a11y);
	ECellVboxView *ecvv = (ECellVboxView *) (gaec->cell_view);
	AtkObject *ret;
	if (i < gaev->a11y_subcell_count) {
		if (gaev->a11y_subcells[i] == NULL) {
			gint model_col, row;
			row = gaec->row;
			model_col = ecvv->model_cols[i];
			ECellView *subcell_view = ecvv->subcell_views[i];
			ret = gal_a11y_e_cell_registry_get_object (NULL,
				gaec->item,
				subcell_view,
				a11y,
				model_col,
				gaec->view_col, /* FIXME should the view column use a fake one or the same as its parent? */
				row);
			gaev->a11y_subcells[i] = ret;
			g_object_ref (ret);
			g_object_weak_ref (G_OBJECT (ret),
					(GWeakNotify) subcell_destroyed,
					ret);
		} else {
			ret = (AtkObject *) gaev->a11y_subcells[i];
			if (ATK_IS_OBJECT (ret))
				g_object_ref (ret);
			else
				ret = NULL;
		}
	} else {
		ret = NULL;
	}

	return ret;
}

static void
ecv_dispose (GObject *object)
{
	GalA11yECellVbox *gaev = GAL_A11Y_E_CELL_VBOX (object);
	if (gaev->a11y_subcells)
		g_free (gaev->a11y_subcells);

	if (parent_class->dispose)
		parent_class->dispose (object);
}

/* AtkComponet interface */
static AtkObject*
ecv_ref_accessible_at_point (AtkComponent *component,
			     gint x,
			     gint y,
			     AtkCoordType coord_type)
{
	gint x0, y0, width, height;
	int subcell_height, i;

	GalA11yECell *gaec = GAL_A11Y_E_CELL (component);
	ECellVboxView *ecvv = (ECellVboxView *) (gaec->cell_view);

	atk_component_get_extents (component, &x0, &y0, &width, &height, coord_type);
	x -= x0;
	y -= y0;
	if (x < 0 || x > width || y < 0 || y > height)
		return NULL;

	for (i = 0; i < ecvv->subcell_view_count; i++) {
		subcell_height = e_cell_height (ecvv->subcell_views[i], ecvv->model_cols[i], gaec->view_col, gaec->row);
		if ( 0 <= y && y <= subcell_height) {
			return ecv_ref_child ((AtkObject *)component, i);
		} else
			y -= subcell_height;
	}

	return NULL;
}

static void
ecv_class_init (GalA11yECellVboxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *a11y_class = ATK_OBJECT_CLASS (klass);
	parent_class		   = g_type_class_ref (PARENT_TYPE);

	object_class->dispose	   = ecv_dispose;

	a11y_class->get_n_children = ecv_get_n_children;
	a11y_class->ref_child	   = ecv_ref_child;
}

static void
ecv_init (GalA11yECellVbox *a11y)
{
}

static void
ecv_atk_component_iface_init (AtkComponentIface *iface)
{
	component_parent_iface         = g_type_interface_peek_parent (iface);

	iface->ref_accessible_at_point = ecv_ref_accessible_at_point;
}

GType
gal_a11y_e_cell_vbox_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellVboxClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ecv_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECellVbox),
			0,
			(GInstanceInitFunc) ecv_init,
			NULL /* value_cell */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) ecv_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yECellVbox", &info, 0);
		gal_a11y_e_cell_type_add_action_interface (type);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}

AtkObject *gal_a11y_e_cell_vbox_new	(ETableItem *item,
					 ECellView  *cell_view,
					 AtkObject  *parent, 
					 int         model_col, 
					 int         view_col, 
					 int         row)
{
	AtkObject *a11y;

	a11y = g_object_new (gal_a11y_e_cell_vbox_get_type (), NULL);
	
	gal_a11y_e_cell_construct (a11y, item, cell_view, parent, model_col, view_col, row);

	GalA11yECell *gaec = GAL_A11Y_E_CELL (a11y);
	GalA11yECellVbox *gaev = GAL_A11Y_E_CELL_VBOX (a11y);
	ECellVboxView *ecvv = (ECellVboxView *) (gaec->cell_view);
	gaev->a11y_subcell_count = ecvv->subcell_view_count; 
	gaev->a11y_subcells = g_malloc0 (sizeof(AtkObject *)*gaev->a11y_subcell_count);
	return a11y;
}

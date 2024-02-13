/*
 *
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
 *		Eric Zhao <eric.zhao@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Sun Microsystem, Inc.
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell-vbox.h"

#include <atk/atk.h>

#include "e-cell-vbox.h"
#include "gal-a11y-e-cell-registry.h"

static void ecv_atk_component_iface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GalA11yECellVbox, gal_a11y_e_cell_vbox, GAL_A11Y_TYPE_E_CELL,
	G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, ecv_atk_component_iface_init)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, gal_a11y_e_cell_atk_action_interface_init))

static gint
ecv_get_n_children (AtkObject *a11y)
{
	g_return_val_if_fail (GAL_A11Y_IS_E_CELL_VBOX (a11y), 0);

	return GAL_A11Y_E_CELL_VBOX (a11y)->a11y_subcell_count;
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

static AtkObject *
ecv_ref_child (AtkObject *a11y,
               gint i)
{
	GalA11yECellVbox *gaev = GAL_A11Y_E_CELL_VBOX (a11y);
	GalA11yECell *gaec = GAL_A11Y_E_CELL (a11y);
	ECellVboxView *ecvv = (ECellVboxView *) (gaec->cell_view);
	AtkObject *ret;
	if (i < gaev->a11y_subcell_count) {
		if (gaev->a11y_subcells[i] == NULL) {
			ECellView *subcell_view;
			gint model_col, row;
			row = gaec->row;
			model_col = ecvv->model_cols[i];
			subcell_view = ecvv->subcell_views[i];
			/* FIXME Should the view column use a fake
			 *       one or the same as its parent? */
			ret = gal_a11y_e_cell_registry_get_object (
				NULL,
				gaec->item,
				subcell_view,
				a11y,
				model_col,
				gaec->view_col,
				row);
			gaev->a11y_subcells[i] = ret;
			g_object_ref (ret);
			g_object_weak_ref (
				G_OBJECT (ret),
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
	g_free (gaev->a11y_subcells);

	G_OBJECT_CLASS (gal_a11y_e_cell_vbox_parent_class)->dispose (object);
}

/* AtkComponet interface */
static AtkObject *
ecv_ref_accessible_at_point (AtkComponent *component,
                             gint x,
                             gint y,
                             AtkCoordType coord_type)
{
	gint x0, y0, width, height;
	gint subcell_height, i;

	GalA11yECell *gaec = GAL_A11Y_E_CELL (component);
	ECellVboxView *ecvv = (ECellVboxView *) (gaec->cell_view);

	atk_component_get_extents (component, &x0, &y0, &width, &height, coord_type);
	x -= x0;
	y -= y0;
	if (x < 0 || x > width || y < 0 || y > height)
		return NULL;

	for (i = 0; i < ecvv->subcell_view_count; i++) {
		subcell_height = e_cell_height (
			ecvv->subcell_views[i], ecvv->model_cols[i],
			gaec->view_col, gaec->row);
		if (0 <= y && y <= subcell_height) {
			return ecv_ref_child ((AtkObject *) component, i);
		} else
			y -= subcell_height;
	}

	return NULL;
}

static void
gal_a11y_e_cell_vbox_class_init (GalA11yECellVboxClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	AtkObjectClass *a11y_class = ATK_OBJECT_CLASS (class);

	object_class->dispose = ecv_dispose;

	a11y_class->get_n_children = ecv_get_n_children;
	a11y_class->ref_child = ecv_ref_child;
}

static void
gal_a11y_e_cell_vbox_init (GalA11yECellVbox *a11y)
{
}

static void
ecv_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->ref_accessible_at_point = ecv_ref_accessible_at_point;
}

AtkObject *gal_a11y_e_cell_vbox_new	(ETableItem *item,
					 ECellView  *cell_view,
					 AtkObject  *parent,
					 gint         model_col,
					 gint         view_col,
					 gint         row)
{
	AtkObject *a11y;
	GalA11yECell *gaec;
	GalA11yECellVbox *gaev;
	ECellVboxView *ecvv;

	a11y = g_object_new (gal_a11y_e_cell_vbox_get_type (), NULL);

	gal_a11y_e_cell_construct (
		a11y, item, cell_view, parent, model_col, view_col, row);

	gaec = GAL_A11Y_E_CELL (a11y);
	gaev = GAL_A11Y_E_CELL_VBOX (a11y);
	ecvv = (ECellVboxView *) (gaec->cell_view);
	gaev->a11y_subcell_count = ecvv->subcell_view_count;
	gaev->a11y_subcells = g_malloc0 (sizeof (AtkObject *) * gaev->a11y_subcell_count);
	return a11y;
}

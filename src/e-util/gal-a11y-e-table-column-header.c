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
 *		Li Yuan <li.yuan@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-table-column-header.h"

#include <glib/gi18n.h>
#include <atk/atkobject.h>
#include <atk/atkregistry.h>

#include "e-table-header-item.h"
#include "gal-a11y-util.h"

static GObjectClass *parent_class;
static gint priv_offset;

#define GET_PRIVATE(object) \
	((GalA11yETableColumnHeaderPrivate *) \
	(((gchar *) object) + priv_offset))
#define PARENT_TYPE (atk_gobject_accessible_get_type ())

struct _GalA11yETableColumnHeaderPrivate {
	ETableItem *item;
	AtkObject  *parent;
	AtkStateSet *state_set;
};

static void
etch_init (GalA11yETableColumnHeader *a11y)
{
	GET_PRIVATE (a11y)->item = NULL;
	GET_PRIVATE (a11y)->parent = NULL;
	GET_PRIVATE (a11y)->state_set = NULL;
}

static AtkStateSet *
gal_a11y_e_table_column_header_ref_state_set (AtkObject *accessible)
{
	GalA11yETableColumnHeaderPrivate *priv = GET_PRIVATE (accessible);

	g_return_val_if_fail (priv->state_set, NULL);

	g_object_ref (priv->state_set);

	return priv->state_set;
}

static void
gal_a11y_e_table_column_header_real_initialize (AtkObject *obj,
                                                gpointer data)
{
	ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);
}

static void
gal_a11y_e_table_column_header_dispose (GObject *object)
{
	GalA11yETableColumnHeader *a11y = GAL_A11Y_E_TABLE_COLUMN_HEADER (object);
	GalA11yETableColumnHeaderPrivate *priv = GET_PRIVATE (a11y);

	g_clear_object (&priv->state_set);

	if (parent_class->dispose)
		parent_class->dispose (object);

}

static void
etch_class_init (GalA11yETableColumnHeaderClass *class)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = gal_a11y_e_table_column_header_dispose;

	atk_object_class->ref_state_set = gal_a11y_e_table_column_header_ref_state_set;
	atk_object_class->initialize = gal_a11y_e_table_column_header_real_initialize;
}

inline static GObject *
etch_a11y_get_gobject (AtkGObjectAccessible *accessible)
{
	return atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
}

static gboolean
gal_a11y_e_table_column_header_do_action (AtkAction *action,
                                          gint i)
{
	gboolean return_value = TRUE;
	GtkWidget *widget;
	GalA11yETableColumnHeader *a11y;
	ETableHeaderItem *ethi;
	ETableItem *item;
	ETableCol *col;

	switch (i) {
		case 0:
			a11y = GAL_A11Y_E_TABLE_COLUMN_HEADER (action);
			col = E_TABLE_COL (etch_a11y_get_gobject (
				ATK_GOBJECT_ACCESSIBLE (a11y)));
			item = GET_PRIVATE (a11y)->item;
			widget = gtk_widget_get_parent (GTK_WIDGET (item->parent.canvas));
			if (E_IS_TREE (widget)) {
				ethi = E_TABLE_HEADER_ITEM (
					e_tree_get_header_item (E_TREE (widget)));
			}
			else if (E_IS_TABLE (widget))
				ethi = E_TABLE_HEADER_ITEM (
					E_TABLE (widget)->header_item);
			else
				break;
			ethi_change_sort_state (ethi, col, E_TABLE_HEADER_ITEM_SORT_FLAG_NONE);
			break;
		default:
			return_value = FALSE;
			break;
	}
	return return_value;
}

static gint
gal_a11y_e_table_column_header_get_n_actions (AtkAction *action)
{
	return 1;
}

static const gchar *
gal_a11y_e_table_column_header_action_get_name (AtkAction *action,
                                                gint i)
{
	const gchar *return_value;

	switch (i) {
		case 0:
			return_value = _("sort");
			break;
		default:
			return_value = NULL;
			break;
	}
	return return_value;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = gal_a11y_e_table_column_header_do_action;
	iface->get_n_actions = gal_a11y_e_table_column_header_get_n_actions;
	iface->get_name = gal_a11y_e_table_column_header_action_get_name;
}

/* Component IFace */
static void
gal_a11y_e_table_column_header_get_extents (AtkComponent *component,
                                            gint *x,
                                            gint *y,
                                            gint *width,
                                            gint *height,
                                            AtkCoordType coord_type)
{
	GalA11yETableColumnHeader *a11y = GAL_A11Y_E_TABLE_COLUMN_HEADER (component);
	GtkWidget *table_or_tree;
	gint row;
	gint col;
	gint xval;
	ETableCol *ecol = E_TABLE_COL (etch_a11y_get_gobject (ATK_GOBJECT_ACCESSIBLE (a11y)));
	gint yval;

	g_return_if_fail (ecol != NULL);
	g_return_if_fail (a11y != NULL);

	row = 0;
	col = ecol->spec->model_col;

	table_or_tree = gtk_widget_get_parent (GTK_WIDGET (GET_PRIVATE (a11y)->item->parent.canvas));
	if (E_IS_TREE (table_or_tree)) {
		e_tree_get_cell_geometry (
			E_TREE (table_or_tree),
			row, col, &xval, &yval,
			width, height);
	} else {
		e_table_get_cell_geometry (
			E_TABLE (table_or_tree),
			row, col, &xval, &yval,
			width, height);
	}

	atk_component_get_position (
		ATK_COMPONENT (atk_object_get_parent (ATK_OBJECT (a11y))),
		x, y, coord_type);

	if (x && *x != G_MININT)
		*x += xval;
	if (y && *y != G_MININT)
		*y += yval;
}
static void
atk_component_interface_init (AtkComponentIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_extents = gal_a11y_e_table_column_header_get_extents;
}

GType
gal_a11y_e_table_column_header_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yETableColumnHeaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) etch_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GalA11yETableColumnHeader),
			0,
			(GInstanceInitFunc) etch_init,
			NULL
		};
		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) atk_component_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = gal_a11y_type_register_static_with_private (
			PARENT_TYPE, "GalA11yETableColumnHeader", &info, 0,
			sizeof (GalA11yETableColumnHeaderPrivate), &priv_offset);

		g_type_add_interface_static (
			type, ATK_TYPE_ACTION, &atk_action_info);
		g_type_add_interface_static (
			type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_table_column_header_new (ETableCol *ecol,
                                    ETableItem *item,
                                    AtkObject *parent)
{
	GalA11yETableColumnHeader *a11y;
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_TABLE_COL (ecol), NULL);

	a11y = g_object_new (gal_a11y_e_table_column_header_get_type (), NULL);
	accessible = ATK_OBJECT (a11y);
	atk_object_initialize (accessible, ecol);

	GET_PRIVATE (a11y)->item = item;
	GET_PRIVATE (a11y)->state_set = atk_state_set_new ();

	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_VISIBLE);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_SHOWING);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (GET_PRIVATE (a11y)->state_set, ATK_STATE_ENABLED);

	if (ecol->text)
		atk_object_set_name (accessible, ecol->text);
	atk_object_set_role (accessible, ATK_ROLE_TABLE_COLUMN_HEADER);
	atk_object_set_parent (accessible, parent);

	return accessible;
}

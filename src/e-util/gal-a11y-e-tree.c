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
 *		Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-tree.h"

#include "e-table-item.h"
#include "e-tree.h"
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-util.h"

struct _GalA11yETreePrivate {
	AtkObject *child_item;
};

static void et_atk_component_iface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GalA11yETree, gal_a11y_e_tree, GTK_TYPE_CONTAINER_ACCESSIBLE,
	G_ADD_PRIVATE (GalA11yETree)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, et_atk_component_iface_init))

/* Static functions */

static void
init_child_item (GalA11yETree *a11y)
{
	GalA11yETreePrivate *priv = a11y->priv;
	ETree *tree;
	ETableItem * eti;

	tree = E_TREE (gtk_accessible_get_widget (GTK_ACCESSIBLE (a11y)));
	g_return_if_fail (tree);

	eti = e_tree_get_item (tree);
	if (priv->child_item == NULL) {
		priv->child_item = atk_gobject_accessible_for_object (G_OBJECT (eti));
	}
}

static AtkObject *
et_ref_accessible_at_point (AtkComponent *component,
                             gint x,
                             gint y,
                             AtkCoordType coord_type)
{
	GalA11yETree *a11y = GAL_A11Y_E_TREE (component);
	init_child_item (a11y);
	return a11y->priv->child_item;
}

static gint
et_get_n_children (AtkObject *accessible)
{
	return 1;
}

static AtkObject *
et_ref_child (AtkObject *accessible,
              gint i)
{
	GalA11yETree *a11y = GAL_A11Y_E_TREE (accessible);
	if (i != 0)
		return NULL;
	init_child_item (a11y);
	g_object_ref (a11y->priv->child_item);
	return a11y->priv->child_item;
}

static AtkLayer
et_get_layer (AtkComponent *component)
{
	return ATK_LAYER_WIDGET;
}

static void
gal_a11y_e_tree_class_init (GalA11yETreeClass *class)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (class);

	atk_object_class->get_n_children = et_get_n_children;
	atk_object_class->ref_child = et_ref_child;
}

static void
et_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->ref_accessible_at_point = et_ref_accessible_at_point;
	iface->get_layer = et_get_layer;
}

static void
gal_a11y_e_tree_init (GalA11yETree *a11y)
{
	a11y->priv = gal_a11y_e_tree_get_instance_private (a11y);
	a11y->priv->child_item = NULL;
}

AtkObject *
gal_a11y_e_tree_new (GObject *widget)
{
	GalA11yETree *a11y;

	a11y = g_object_new (gal_a11y_e_tree_get_type (), NULL);

	gtk_accessible_set_widget (GTK_ACCESSIBLE (a11y), GTK_WIDGET (widget));

	return ATK_OBJECT (a11y);
}


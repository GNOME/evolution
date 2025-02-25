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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-table.h"

#include "e-table-click-to-add.h"
#include "e-table-group-container.h"
#include "e-table-group-leaf.h"
#include "e-table-group.h"
#include "e-table.h"
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-util.h"

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yETableClass))

struct _GalA11yETablePrivate {
	AtkObject *child_item;
};

static void et_atk_component_iface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GalA11yETable, gal_a11y_e_table, GTK_TYPE_CONTAINER_ACCESSIBLE,
	G_ADD_PRIVATE (GalA11yETable)
	G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, et_atk_component_iface_init))

/* Static functions */
static ETableItem *
find_first_table_item (ETableGroup *group)
{
	GnomeCanvasGroup *cgroup;
	GList *l;

	cgroup = GNOME_CANVAS_GROUP (group);

	for (l = cgroup->item_list; l; l = l->next) {
		GnomeCanvasItem *i;

		i = GNOME_CANVAS_ITEM (l->data);

		if (E_IS_TABLE_GROUP (i))
			return find_first_table_item (E_TABLE_GROUP (i));
		else if (E_IS_TABLE_ITEM (i)) {
			return E_TABLE_ITEM (i);
		}
	}

	return NULL;
}

static AtkObject *
eti_get_accessible (ETableItem *eti,
                    AtkObject *parent)
{
	AtkObject *a11y = NULL;

	g_return_val_if_fail (eti, NULL);

	a11y = atk_gobject_accessible_for_object (G_OBJECT (eti));
	g_return_val_if_fail (a11y, NULL);

	return a11y;
}

static gboolean
init_child_item (GalA11yETable *a11y)
{
	ETable *table;

	if (!a11y || !GTK_IS_ACCESSIBLE (a11y))
		return FALSE;

	table = E_TABLE (gtk_accessible_get_widget (GTK_ACCESSIBLE (a11y)));
	if (table && gtk_widget_get_mapped (GTK_WIDGET (table)) && table->group && E_IS_TABLE_GROUP_CONTAINER (table->group)) {
		ETableGroupContainer *etgc = (ETableGroupContainer *) table->group;
		GList *list;

		for (list = etgc->children; list; list = g_list_next (list)) {
			ETableGroupContainerChildNode *child_node = list->data;
			ETableGroup *child = child_node->child;
			ETableItem *eti = find_first_table_item (child);

			eti_get_accessible (eti, ATK_OBJECT (a11y));
		}
	}
	g_object_unref (a11y);
	g_object_unref (table);

	return FALSE;
}

static AtkObject *
et_ref_accessible_at_point (AtkComponent *component,
                            gint x,
                            gint y,
                            AtkCoordType coord_type)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (component);
	if (a11y->priv->child_item)
		g_object_ref (a11y->priv->child_item);
	return a11y->priv->child_item;
}

static gint
et_get_n_children (AtkObject *accessible)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (accessible);
	ETable * et;
	gint n = 0;

	et = E_TABLE (gtk_accessible_get_widget (GTK_ACCESSIBLE (a11y)));

	if (et && et->group) {
		if (E_IS_TABLE_GROUP_LEAF (et->group)) {
			if (find_first_table_item (et->group))
				n++;
		} else if (E_IS_TABLE_GROUP_CONTAINER (et->group)) {
			ETableGroupContainer *etgc = (ETableGroupContainer *) et->group;
			n = g_list_length (etgc->children);
		}
	}

	if (et && et->use_click_to_add && et->click_to_add) {
		n++;
	}
	return n;
}

static AtkObject *
et_ref_child (AtkObject *accessible,
              gint i)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (accessible);
	ETable * et;
	gint child_no;

	et = E_TABLE (gtk_accessible_get_widget (GTK_ACCESSIBLE (a11y)));
	if (!et)
		return NULL;

	child_no = et_get_n_children (accessible);
	if (i == 0 || i < child_no - 1) {
		if (E_IS_TABLE_GROUP_LEAF (et->group)) {
			ETableItem *eti = find_first_table_item (et->group);
			AtkObject *aeti;
			if (eti) {
				aeti = eti_get_accessible (eti, accessible);
				if (aeti)
					g_object_ref (aeti);
				return aeti;
			}
		} else if (E_IS_TABLE_GROUP_CONTAINER (et->group)) {
			ETableGroupContainer *etgc = (ETableGroupContainer *) et->group;
			ETableGroupContainerChildNode *child_node = g_list_nth_data (etgc->children, i);
			if (child_node) {
				ETableGroup *child = child_node->child;
				ETableItem * eti = find_first_table_item (child);
				AtkObject *aeti = eti_get_accessible (eti, accessible);
				if (aeti)
					g_object_ref (aeti);
				return aeti;
			}
		}
	}

	if (i == child_no -1) {
		ETableClickToAdd * etcta;

		if (et && et->use_click_to_add && et->click_to_add) {
			etcta = E_TABLE_CLICK_TO_ADD (et->click_to_add);
			accessible = atk_gobject_accessible_for_object (G_OBJECT (etcta));
			if (accessible)
				g_object_ref (accessible);
			return accessible;
		}
	}

	return NULL;
}

static AtkLayer
et_get_layer (AtkComponent *component)
{
	return ATK_LAYER_WIDGET;
}

static void
gal_a11y_e_table_class_init (GalA11yETableClass *class)
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
gal_a11y_e_table_init (GalA11yETable *a11y)
{
	a11y->priv = gal_a11y_e_table_get_instance_private (a11y);
	a11y->priv->child_item = NULL;
}

AtkObject *
gal_a11y_e_table_new (GObject *widget)
{
	GalA11yETable *a11y;
	ETable *table;

	table = E_TABLE (widget);

	a11y = g_object_new (gal_a11y_e_table_get_type (), NULL);

	gtk_accessible_set_widget (GTK_ACCESSIBLE (a11y), GTK_WIDGET (widget));

	/* we need to init all the children for multiple table items */
	if (table && gtk_widget_get_mapped (GTK_WIDGET (table)) && table->group && E_IS_TABLE_GROUP_CONTAINER (table->group)) {
		/* Ref it here so that it is still valid in the idle function */
		/* It will be unrefed in the idle function */
		g_object_ref (a11y);
		g_object_ref (widget);

		g_idle_add ((GSourceFunc) init_child_item, a11y);
	}

	return ATK_OBJECT (a11y);
}

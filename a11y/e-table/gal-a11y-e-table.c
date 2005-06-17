/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>

#include "a11y/gal-a11y-util.h"
#include "table/e-table.h"
#include "table/e-table-click-to-add.h"
#include "table/e-table-group.h"
#include "table/e-table-group-container.h"
#include "table/e-table-group-leaf.h"

#include "gal-a11y-e-table.h"
#include "gal-a11y-e-table-factory.h"
#include "gal-a11y-e-table-item.h"

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yETableClass))
static AtkObjectClass *parent_class;
static GType parent_type;
static gint priv_offset;
#define GET_PRIVATE(object) ((GalA11yETablePrivate *) (((char *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETablePrivate {
	AtkObject *child_item;
};

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

static AtkObject*
eti_get_accessible (ETableItem *eti, AtkObject *parent)
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

	table = E_TABLE (GTK_ACCESSIBLE (a11y)->widget);
	if (table && GTK_WIDGET_MAPPED (GTK_WIDGET (table)) && table->group && E_IS_TABLE_GROUP_CONTAINER(table->group)) {
		ETableGroupContainer *etgc =  (ETableGroupContainer *)table->group;
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

static AtkObject*
et_ref_accessible_at_point  (AtkComponent *component,
			     gint x,
			     gint y,
			     AtkCoordType coord_type)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (component);
	if (GET_PRIVATE (a11y)->child_item)
		g_object_ref (GET_PRIVATE (a11y)->child_item);
	return GET_PRIVATE (a11y)->child_item;
}

static gint
et_get_n_children (AtkObject *accessible)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (accessible);
	ETable * et;
	int n = 0;

	et = E_TABLE(GTK_ACCESSIBLE (a11y)->widget);

	if (et->group) {
		if (E_IS_TABLE_GROUP_LEAF (et->group))
			n = 1;
		else if (E_IS_TABLE_GROUP_CONTAINER (et->group)) {
			ETableGroupContainer *etgc = (ETableGroupContainer *)et->group;
			n = g_list_length (etgc->children);
		}
	}
	
	if (et && et->use_click_to_add && et->click_to_add) {
		n++;
	}
	return n;
}

static AtkObject*
et_ref_child (AtkObject *accessible,
	      gint i)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (accessible);
	ETable * et;
	gint child_no;

	et = E_TABLE(GTK_ACCESSIBLE (a11y)->widget);

	child_no = et_get_n_children (accessible);
	if (i == 0 || i < child_no - 1) {
		if (E_IS_TABLE_GROUP_LEAF (et->group)) {
			ETableItem *eti = find_first_table_item (et->group);
			AtkObject *aeti = eti_get_accessible (eti, accessible);
			if (aeti)
				g_object_ref (aeti);
			return aeti;

		} else if (E_IS_TABLE_GROUP_CONTAINER (et->group)) {
			ETableGroupContainer *etgc =  (ETableGroupContainer *) et->group;
			ETableGroupContainerChildNode *child_node = g_list_nth_data (etgc->children, i);
			if (child_node) {
				ETableGroup *child = child_node->child;
				ETableItem * eti = find_first_table_item (child);
				AtkObject *aeti =  eti_get_accessible (eti, accessible);
				if (aeti)
					g_object_ref (aeti);
				return aeti;
			}
		}
	} else if (i == child_no -1) {
        	AtkObject * accessible;
		ETableClickToAdd * etcta;

		if (et && et->use_click_to_add && et->click_to_add) {
			etcta = E_TABLE_CLICK_TO_ADD(et->click_to_add);
			accessible = atk_gobject_accessible_for_object (G_OBJECT(etcta));
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
et_class_init (GalA11yETableClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

	parent_class                              = g_type_class_ref (PARENT_TYPE);

	atk_object_class->get_n_children          = et_get_n_children;
	atk_object_class->ref_child               = et_ref_child;
}

static void
et_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->ref_accessible_at_point = et_ref_accessible_at_point;
	iface->get_layer = et_get_layer;
}

static void
et_init (GalA11yETable *a11y)
{
	GalA11yETablePrivate *priv;

	priv = GET_PRIVATE (a11y);

	priv->child_item = NULL;
}

/**
 * gal_a11y_e_table_get_type:
 * @void: 
 * 
 * Registers the &GalA11yETable class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yETable class.
 **/
GType
gal_a11y_e_table_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;

		GTypeInfo info = {
			sizeof (GalA11yETableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) et_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETable),
			0,
			(GInstanceInitFunc) et_init,
			NULL /* value_table */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) et_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		factory = atk_registry_get_factory (atk_get_default_registry (), GTK_TYPE_WIDGET);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (PARENT_TYPE, "GalA11yETable", &info, 0,
								   sizeof (GalA11yETablePrivate), &priv_offset);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_table_new (GObject *widget)
{
	GalA11yETable *a11y;
	ETable *table;

	table = E_TABLE (widget);

	a11y = g_object_new (gal_a11y_e_table_get_type (), NULL);

	GTK_ACCESSIBLE (a11y)->widget = GTK_WIDGET (widget);

	/* we need to init all the children for multiple table items */
	if (table && GTK_WIDGET_MAPPED (GTK_WIDGET (table)) && table->group && E_IS_TABLE_GROUP_CONTAINER (table->group)) {
		/* Ref it here so that it is still valid in the idle function */
		/* It will be unrefed in the idle function */
		g_object_ref (a11y);
		g_object_ref (widget);

		g_idle_add ((GSourceFunc)init_child_item, a11y);
	}

	return ATK_OBJECT (a11y);
}

void 
gal_a11y_e_table_init (void)
{
	if (atk_get_root ())
		atk_registry_set_factory_type (atk_get_default_registry (),
						E_TABLE_TYPE,
						gal_a11y_e_table_factory_get_type ());

}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-table.h"
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-util.h"
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-table-group.h>
#include <gal/e-table/e-table-group-leaf.h>

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

static void
init_child_item (GalA11yETable *a11y)
{
	GalA11yETablePrivate *priv = GET_PRIVATE (a11y);
	ETable *table = E_TABLE (GTK_ACCESSIBLE (a11y)->widget);
	if (priv->child_item == NULL) {
		priv->child_item = gal_a11y_e_table_item_new (ATK_OBJECT (a11y), E_TABLE_GROUP_LEAF (table->group)->item, 0);
	}
}

static AtkObject*
et_ref_accessible_at_point  (AtkComponent *component,
			     gint x,
			     gint y,
			     AtkCoordType coord_type)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (component);
	init_child_item (a11y);
	return GET_PRIVATE (a11y)->child_item;
}

static gint
et_get_n_children (AtkObject *accessible)
{
	return 1;
}

static AtkObject*
et_ref_child (AtkObject *accessible,
	      gint i)
{
	GalA11yETable *a11y = GAL_A11Y_E_TABLE (accessible);
	if (i != 0)
		return NULL;
	init_child_item (a11y);
	g_object_ref (GET_PRIVATE (a11y)->child_item);
	return GET_PRIVATE (a11y)->child_item;
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

	return ATK_OBJECT (a11y);
}

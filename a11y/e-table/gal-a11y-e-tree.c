/*
 * Authors: Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-tree.h"
#include "gal-a11y-util.h"
#include "gal-a11y-e-table-item.h"
#include <gal/e-table/e-tree.h>
#include <gal/e-table/e-table-item.h>

#define CS_CLASS(a11y) (G_TYPE_INSTANCE_GET_CLASS ((a11y), C_TYPE_STREAM, GalA11yETreeClass))
static AtkObjectClass *parent_class;
static GType parent_type;
static gint priv_offset;
#define GET_PRIVATE(object) ((GalA11yETreePrivate *) (((char *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETreePrivate {
	AtkObject *child_item;
};

/* Static functions */

static void
init_child_item (GalA11yETree *a11y)
{
	GalA11yETreePrivate *priv = GET_PRIVATE (a11y);
	ETree *tree = E_TREE (GTK_ACCESSIBLE (a11y)->widget);
	ETableItem * eti;

	g_return_if_fail (tree);
	eti = e_tree_get_item (tree);
	if (priv->child_item == NULL) {
		priv->child_item = atk_gobject_accessible_for_object (G_OBJECT (eti));
		if (!priv->child_item)
			priv->child_item = gal_a11y_e_table_item_new (ATK_OBJECT (a11y),eti, 0);

		g_return_if_fail (priv->child_item);
		priv->child_item->role = ATK_ROLE_TREE_TABLE;
	}
}

static AtkObject*
et_ref_accessible_at_point  (AtkComponent *component,
			     gint x,
			     gint y,
			     AtkCoordType coord_type)
{
	GalA11yETree *a11y = GAL_A11Y_E_TREE (component);
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
	GalA11yETree *a11y = GAL_A11Y_E_TREE (accessible);
	if (i != 0)
		return NULL;
	init_child_item (a11y);
	g_object_ref (GET_PRIVATE (a11y)->child_item);
	return GET_PRIVATE (a11y)->child_item;
}

static void
et_class_init (GalA11yETreeClass *klass)
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
et_init (GalA11yETree *a11y)
{
	GalA11yETreePrivate *priv;

	priv = GET_PRIVATE (a11y);

	priv->child_item = NULL;
}

/**
 * gal_a11y_e_tree_get_type:
 * @void: 
 * 
 * Registers the &GalA11yETree class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yETree class.
 **/
GType
gal_a11y_e_tree_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;

		GTypeInfo info = {
			sizeof (GalA11yETreeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) et_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETree),
			0,
			(GInstanceInitFunc) et_init,
			NULL /* value_tree */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) et_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		factory = atk_registry_get_factory (atk_get_default_registry (), GTK_TYPE_WIDGET);
		parent_type = atk_object_factory_get_accessible_type (factory);

		type = gal_a11y_type_register_static_with_private (PARENT_TYPE, "GalA11yETree", &info, 0,
								   sizeof (GalA11yETreePrivate), &priv_offset);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_tree_new (GObject *widget)
{
	GalA11yETree *a11y;
	ETree *tree;

	tree = E_TREE (widget);

	a11y = g_object_new (gal_a11y_e_tree_get_type (), NULL);

	GTK_ACCESSIBLE (a11y)->widget = GTK_WIDGET (widget);

	return ATK_OBJECT (a11y);
}

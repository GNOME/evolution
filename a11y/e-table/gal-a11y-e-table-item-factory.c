/*
 * Authors: Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 2003 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-e-table-item-factory.h"
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-e-table.h"
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-tree.h>
#include <atk/atk.h>


#define CS_CLASS(factory) (G_TYPE_INSTANCE_GET_CLASS ((factory), C_TYPE_STREAM, GalA11yETableItemFactoryClass))
static AtkObjectFactoryClass *parent_class;
#define PARENT_TYPE (ATK_TYPE_OBJECT_FACTORY)

/* Static functions */

static GType
gal_a11y_e_table_item_factory_get_accessible_type (void)
{
        return GAL_A11Y_TYPE_E_TABLE_ITEM;
}

static AtkObject*
gal_a11y_e_table_item_factory_create_accessible (GObject *obj)
{
	AtkObject *accessible;

	g_return_val_if_fail (E_IS_TABLE_ITEM(obj), NULL);
	accessible = gal_a11y_e_table_item_new(NULL, E_TABLE_ITEM (obj), 0);
                                                                                
	return accessible;
}

static void
gal_a11y_e_table_item_factory_class_init (GalA11yETableItemFactoryClass *klass)
{
	AtkObjectFactoryClass *factory_class = ATK_OBJECT_FACTORY_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	factory_class->create_accessible   = gal_a11y_e_table_item_factory_create_accessible;
	factory_class->get_accessible_type = gal_a11y_e_table_item_factory_get_accessible_type;
}

static void
gal_a11y_e_table_item_factory_init (GalA11yETableItemFactory *factory)
{
}

/**
 * gal_a11y_e_table_factory_get_type:
 * @void: 
 * 
 * Registers the &GalA11yETableFactory class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yETableFactory class.
 **/
GType
gal_a11y_e_table_item_factory_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yETableItemFactoryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_table_item_factory_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETableItemFactory),
			0,
			(GInstanceInitFunc) gal_a11y_e_table_item_factory_init,
			NULL /* value_table */
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yETableItemFactory", &info, 0);
	}

	return type;
}

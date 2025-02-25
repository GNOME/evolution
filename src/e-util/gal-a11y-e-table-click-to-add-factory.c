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

#include "gal-a11y-e-table-click-to-add-factory.h"

#include <atk/atk.h>

#include "e-table-click-to-add.h"
#include "e-table.h"
#include "gal-a11y-e-table-click-to-add.h"
#include "gal-a11y-e-table.h"

#define CS_CLASS(factory) (G_TYPE_INSTANCE_GET_CLASS ((factory), C_TYPE_STREAM, GalA11yETableClickToAddFactoryClass))
static AtkObjectFactoryClass *parent_class;
#define PARENT_TYPE (ATK_TYPE_OBJECT_FACTORY)

/* Static functions */

static GType
gal_a11y_e_table_click_to_add_factory_get_accessible_type (void)
{
	return GAL_A11Y_TYPE_E_TABLE_CLICK_TO_ADD;
}

static AtkObject *
gal_a11y_e_table_click_to_add_factory_create_accessible (GObject *obj)
{
	AtkObject * atk_object;

	g_return_val_if_fail (E_IS_TABLE_CLICK_TO_ADD (obj), NULL);

	atk_object = gal_a11y_e_table_click_to_add_new (obj);

	return atk_object;
}

static void
gal_a11y_e_table_click_to_add_factory_class_init (GalA11yETableClickToAddFactoryClass *class)
{
	AtkObjectFactoryClass *factory_class = ATK_OBJECT_FACTORY_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	factory_class->create_accessible = gal_a11y_e_table_click_to_add_factory_create_accessible;
	factory_class->get_accessible_type = gal_a11y_e_table_click_to_add_factory_get_accessible_type;
}

static void
gal_a11y_e_table_click_to_add_factory_init (GalA11yETableClickToAddFactory *factory)
{
}

/**
 * gal_a11y_e_table_click_to_add_factory_get_type:
 * @void:
 *
 * Registers the &GalA11yETableClickToAddFactory class if necessary, and returns the type ID
 * associated to it.
 *
 * Returns: The type ID of the &GalA11yETableClickToAddFactory class.
 **/
GType
gal_a11y_e_table_click_to_add_factory_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yETableClickToAddFactoryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_table_click_to_add_factory_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETableClickToAddFactory),
			0,
			(GInstanceInitFunc) gal_a11y_e_table_click_to_add_factory_init,
			NULL /* value_table */
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yETableClickToAddFactory", &info, 0);
	}

	return type;
}

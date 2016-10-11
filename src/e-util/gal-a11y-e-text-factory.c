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

#include "e-text.h"
#include "gal-a11y-e-text-factory.h"
#include "gal-a11y-e-text.h"

static AtkObjectFactoryClass *parent_class;
#define PARENT_TYPE (ATK_TYPE_OBJECT_FACTORY)

/* Static functions */

static GType
gal_a11y_e_text_factory_get_accessible_type (void)
{
	return GAL_A11Y_TYPE_E_TEXT;
}

static AtkObject *
gal_a11y_e_text_factory_create_accessible (GObject *obj)
{
	AtkObject *atk_object;

	g_return_val_if_fail (E_IS_TEXT (obj), NULL);

	atk_object = g_object_new (GAL_A11Y_TYPE_E_TEXT, NULL);
	atk_object_initialize (atk_object, obj);

	return atk_object;
}

static void
gal_a11y_e_text_factory_class_init (GalA11yETextFactoryClass *class)
{
	AtkObjectFactoryClass *factory_class = ATK_OBJECT_FACTORY_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	factory_class->create_accessible = gal_a11y_e_text_factory_create_accessible;
	factory_class->get_accessible_type = gal_a11y_e_text_factory_get_accessible_type;
}

static void
gal_a11y_e_text_factory_init (GalA11yETextFactory *factory)
{
}

/**
 * gal_a11y_e_text_factory_get_type:
 * @void:
 *
 * Registers the &GalA11yETextFactory class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GalA11yETextFactory class.
 **/
GType
gal_a11y_e_text_factory_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yETextFactoryClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_text_factory_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETextFactory),
			0,
			(GInstanceInitFunc) gal_a11y_e_text_factory_init,
			NULL /* value_text */
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yETextFactory", &info, 0);
	}

	return type;
}

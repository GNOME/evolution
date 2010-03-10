/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "gal-view-factory.h"

#include <config.h>
#include <e-util/e-util.h>

G_DEFINE_TYPE (GalViewFactory, gal_view_factory, G_TYPE_OBJECT)

/* XXX Should GalViewFactory be a GInterface? */

static void
gal_view_factory_class_init (GalViewFactoryClass *class)
{
}

static void
gal_view_factory_init (GalViewFactory *factory)
{
}

/**
 * gal_view_factory_get_title:
 * @factory: a #GalViewFactory
 *
 * Returns: The title of the factory.
 */
const gchar *
gal_view_factory_get_title (GalViewFactory *factory)
{
	GalViewFactoryClass *class;

	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	class = GAL_VIEW_FACTORY_GET_CLASS (factory);
	g_return_val_if_fail (class->get_title != NULL, NULL);

	return class->get_title (factory);
}

/**
 * gal_view_factory_get_type_code:
 * @factory: a #GalViewFactory
 *
 * Returns: The type code
 */
const gchar *
gal_view_factory_get_type_code (GalViewFactory *factory)
{
	GalViewFactoryClass *class;

	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	class = GAL_VIEW_FACTORY_GET_CLASS (factory);
	g_return_val_if_fail (class->get_type_code != NULL, NULL);

	return class->get_type_code (factory);
}

/**
 * gal_view_factory_new_view:
 * @factory: a #GalViewFactory
 * @name: the name for the view
 *
 * Returns: The new view
 */
GalView *
gal_view_factory_new_view (GalViewFactory *factory,
                           const gchar *name)
{
	GalViewFactoryClass *class;

	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	class = GAL_VIEW_FACTORY_GET_CLASS (factory);
	g_return_val_if_fail (class->new_view != NULL, NULL);

	return class->new_view (factory, name);
}


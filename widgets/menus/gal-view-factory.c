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

#include <config.h>

#include "e-util/e-util.h"

#include "gal-view-factory.h"

G_DEFINE_TYPE (GalViewFactory, gal_view_factory, G_TYPE_OBJECT)

#define d(x)

d(static gint depth = 0;)

/**
 * gal_view_factory_get_title:
 * @factory: The factory to query.
 *
 * Returns: The title of the factory.
 */
const gchar *
gal_view_factory_get_title (GalViewFactory *factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	if (GAL_VIEW_FACTORY_GET_CLASS (factory)->get_title)
		return GAL_VIEW_FACTORY_GET_CLASS (factory)->get_title (factory);
	else
		return NULL;
}

/**
 * gal_view_factory_new_view:
 * @factory: The factory to use
 * @name: the name for the view.
 *
 * Returns: The new view
 */
GalView *
gal_view_factory_new_view        (GalViewFactory *factory,
				  const gchar     *name)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	if (GAL_VIEW_FACTORY_GET_CLASS (factory)->new_view)
		return GAL_VIEW_FACTORY_GET_CLASS (factory)->new_view (factory, name);
	else
		return NULL;
}

/**
 * gal_view_factory_get_type_code:
 * @factory: The factory to use
 *
 * Returns: The type code
 */
const gchar *
gal_view_factory_get_type_code (GalViewFactory *factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	if (GAL_VIEW_FACTORY_GET_CLASS (factory)->get_type_code)
		return GAL_VIEW_FACTORY_GET_CLASS (factory)->get_type_code (factory);
	else
		return NULL;
}

static void
gal_view_factory_class_init      (GalViewFactoryClass *klass)
{
	klass->get_title = NULL;
	klass->new_view  = NULL;
}

static void
gal_view_factory_init      (GalViewFactory *factory)
{
}


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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gal-view-factory.h"

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
 * gal_view_factory_get_type_code:
 * @factory: a #GalViewFactory
 *
 * Returns: The type code
 */
const gchar *
gal_view_factory_get_type_code (GalViewFactory *factory)
{
	GalViewFactoryClass *class;
	GalViewClass *view_class;

	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	class = GAL_VIEW_FACTORY_GET_CLASS (factory);

	/* All GalView types are registered statically, so there's no
	 * harm in dereferencing the class pointer after unreffing it. */
	view_class = g_type_class_ref (class->gal_view_type);
	g_return_val_if_fail (GAL_IS_VIEW_CLASS (view_class), NULL);
	g_type_class_unref (view_class);

	return view_class->type_code;
}

/**
 * gal_view_factory_new_view:
 * @factory: a #GalViewFactory
 * @title: the title for the view
 *
 * Returns: The new view
 */
GalView *
gal_view_factory_new_view (GalViewFactory *factory,
                           const gchar *title)
{
	GalViewFactoryClass *class;

	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), NULL);

	class = GAL_VIEW_FACTORY_GET_CLASS (factory);

	return g_object_new (class->gal_view_type, "title", title, NULL);
}


/*
 *
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
#include <glib/gi18n.h>

#include "gal-view-etable.h"
#include "gal-view-factory-etable.h"

G_DEFINE_TYPE (
	GalViewFactoryEtable,
	gal_view_factory_etable,
	GAL_TYPE_VIEW_FACTORY)

static const gchar *
view_factory_etable_get_type_code (GalViewFactory *factory)
{
	return "etable";
}

static GalView *
view_factory_etable_new_view (GalViewFactory *factory,
                              const gchar *name)
{
	return gal_view_etable_new (name);
}

static void
gal_view_factory_etable_class_init (GalViewFactoryEtableClass *class)
{
	GalViewFactoryClass *view_factory_class;

	view_factory_class = GAL_VIEW_FACTORY_CLASS (class);
	view_factory_class->get_type_code = view_factory_etable_get_type_code;
	view_factory_class->new_view = view_factory_etable_new_view;
}

static void
gal_view_factory_etable_init (GalViewFactoryEtable *factory)
{
}

/**
 * gal_view_factory_etable_new:
 *
 * A new GalViewFactory for creating ETable views.  Create one of
 * these and pass it to GalViewCollection for use.
 *
 * Returns: The new GalViewFactoryEtable.
 */
GalViewFactory *
gal_view_factory_etable_new (void)
{
	return g_object_new (GAL_TYPE_VIEW_FACTORY_ETABLE, NULL);
}


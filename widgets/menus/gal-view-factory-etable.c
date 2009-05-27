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
#include "e-util/e-util.h"

#include "gal-view-etable.h"
#include "gal-view-factory-etable.h"

G_DEFINE_TYPE (GalViewFactoryEtable, gal_view_factory_etable, GAL_VIEW_FACTORY_TYPE)

static const gchar *
gal_view_factory_etable_get_title       (GalViewFactory *factory)
{
	return _("Table");
}

static GalView *
gal_view_factory_etable_new_view        (GalViewFactory *factory,
					 const gchar     *name)
{
	return gal_view_etable_new(GAL_VIEW_FACTORY_ETABLE(factory)->spec, name);
}

static const gchar *
gal_view_factory_etable_get_type_code (GalViewFactory *factory)
{
	return "etable";
}

static void
gal_view_factory_etable_dispose         (GObject *object)
{
	GalViewFactoryEtable *factory = GAL_VIEW_FACTORY_ETABLE(object);

	if (factory->spec)
		g_object_unref(factory->spec);
	factory->spec = NULL;

	if (G_OBJECT_CLASS (gal_view_factory_etable_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_factory_etable_parent_class)->dispose) (object);
}

static void
gal_view_factory_etable_class_init      (GalViewFactoryEtableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS (klass);

	view_factory_class->get_title           = gal_view_factory_etable_get_title;
	view_factory_class->new_view            = gal_view_factory_etable_new_view;
	view_factory_class->get_type_code       = gal_view_factory_etable_get_type_code;

	object_class->dispose                   = gal_view_factory_etable_dispose;
}

static void
gal_view_factory_etable_init            (GalViewFactoryEtable *factory)
{
	factory->spec = NULL;
}

/**
 * gal_view_etable_new
 * @spec: The spec to create GalViewEtables based upon.
 *
 * A new GalViewFactory for creating ETable views.  Create one of
 * these and pass it to GalViewCollection for use.
 *
 * Returns: The new GalViewFactoryEtable.
 */
GalViewFactory *
gal_view_factory_etable_new        (ETableSpecification  *spec)
{
	return gal_view_factory_etable_construct (g_object_new (GAL_VIEW_FACTORY_ETABLE_TYPE, NULL), spec);
}

/**
 * gal_view_etable_construct
 * @factory: The factory to construct
 * @spec: The spec to create GalViewEtables based upon.
 *
 * constructs the GalViewFactoryEtable.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewFactoryEtable.
 */
GalViewFactory *
gal_view_factory_etable_construct  (GalViewFactoryEtable *factory,
				    ETableSpecification  *spec)
{
	if (spec)
		g_object_ref(spec);
	factory->spec = spec;
	return GAL_VIEW_FACTORY(factory);
}


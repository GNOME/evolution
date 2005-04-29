/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory-etable.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "gal/util/e-i18n.h"
#include "gal/util/e-util.h"

#include "gal-view-etable.h"
#include "gal-view-factory-etable.h"

#define PARENT_TYPE GAL_VIEW_FACTORY_TYPE

static GalViewFactoryClass *gal_view_factory_etable_parent_class;

static const char *
gal_view_factory_etable_get_title       (GalViewFactory *factory)
{
	return _("Table");
}

static GalView *
gal_view_factory_etable_new_view        (GalViewFactory *factory,
					 const char     *name)
{
	return gal_view_etable_new(GAL_VIEW_FACTORY_ETABLE(factory)->spec, name);
}

static const char *
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
gal_view_factory_etable_class_init      (GObjectClass *object_class)
{
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS(object_class);
	gal_view_factory_etable_parent_class    = g_type_class_ref (PARENT_TYPE);

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

E_MAKE_TYPE(gal_view_factory_etable, "GalViewFactoryEtable", GalViewFactoryEtable, gal_view_factory_etable_class_init, gal_view_factory_etable_init, PARENT_TYPE)

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory.c
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

#include "gal/util/e-util.h"

#include "gal-view-factory.h"

#define PARENT_TYPE G_TYPE_OBJECT

#define d(x)

d(static gint depth = 0;)

static GObjectClass *gal_view_factory_parent_class;

/**
 * gal_view_factory_get_title:
 * @factory: The factory to query.
 *
 * Returns: The title of the factory.
 */
const char *
gal_view_factory_get_title (GalViewFactory *factory)
{
	g_return_val_if_fail (factory != NULL, 0);
	g_return_val_if_fail (GAL_IS_VIEW_FACTORY (factory), 0);

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
				  const char     *name)
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
const char *
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
gal_view_factory_class_init      (GObjectClass *object_class)
{
	GalViewFactoryClass *klass = GAL_VIEW_FACTORY_CLASS(object_class);
	gal_view_factory_parent_class = g_type_class_ref (PARENT_TYPE);
	
	klass->get_title = NULL;     
	klass->new_view  = NULL;     
}

static void
gal_view_factory_init      (GalViewFactory *factory)
{
}

E_MAKE_TYPE(gal_view_factory, "GalViewFactory", GalViewFactory, gal_view_factory_class_init, gal_view_factory_init, PARENT_TYPE)

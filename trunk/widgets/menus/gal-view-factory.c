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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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
const char *
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
gal_view_factory_class_init      (GalViewFactoryClass *klass)
{
	klass->get_title = NULL;
	klass->new_view  = NULL;
}

static void
gal_view_factory_init      (GalViewFactory *factory)
{
}


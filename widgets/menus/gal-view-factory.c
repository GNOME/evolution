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
#include "gal-view-factory.h"

#define GVF_CLASS(e) ((GalViewFactoryClass *)(GTK_OBJECT_GET_CLASS (e)))

#define PARENT_TYPE gtk_object_get_type ()

#define d(x)

d(static gint depth = 0);

static GtkObjectClass *gal_view_factory_parent_class;

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

	if (GVF_CLASS (factory)->get_title)
		return GVF_CLASS (factory)->get_title (factory);
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

	if (GVF_CLASS (factory)->new_view)
		return GVF_CLASS (factory)->new_view (factory, name);
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

	if (GVF_CLASS (factory)->get_type_code)
		return GVF_CLASS (factory)->get_type_code (factory);
	else
		return NULL;
}

static void
gal_view_factory_class_init      (GtkObjectClass *object_class)
{
	GalViewFactoryClass *klass = GAL_VIEW_FACTORY_CLASS(object_class);
	gal_view_factory_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->get_title = NULL;     
	klass->new_view  = NULL;     
}

GtkType
gal_view_factory_get_type        (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewFactory",
			sizeof (GalViewFactory),
			sizeof (GalViewFactoryClass),
			(GtkClassInitFunc) gal_view_factory_class_init,
			NULL,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

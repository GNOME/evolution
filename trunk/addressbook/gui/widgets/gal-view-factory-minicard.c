/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory-minicard.c: A View Factory
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include "gal-view-factory-minicard.h"
#include "gal-view-minicard.h"

G_DEFINE_TYPE (GalViewFactoryMinicard, gal_view_factory_minicard, GAL_VIEW_FACTORY_TYPE)

static const char *
gal_view_factory_minicard_get_title       (GalViewFactory *factory)
{
	return _("Card View");
}

static GalView *
gal_view_factory_minicard_new_view        (GalViewFactory *factory,
					   const char     *name)
{
	return gal_view_minicard_new(name);
}

static const char *
gal_view_factory_minicard_get_type_code (GalViewFactory *factory)
{
	return "minicard";
}

static void
gal_view_factory_minicard_class_init      (GalViewFactoryMinicardClass *minicard_class)
{
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS(minicard_class);

	view_factory_class->get_title           = gal_view_factory_minicard_get_title;
	view_factory_class->new_view            = gal_view_factory_minicard_new_view;
	view_factory_class->get_type_code       = gal_view_factory_minicard_get_type_code;
}

static void
gal_view_factory_minicard_init            (GalViewFactoryMinicard *factory)
{
}

/**
 * gal_view_minicard_new
 *
 * A new GalViewFactory for creating Minicard views.  Create one of
 * these and pass it to GalViewCollection for use.
 *
 * Returns: The new GalViewFactoryMinicard.
 */
GalViewFactory *
gal_view_factory_minicard_new        (void)
{
	return gal_view_factory_minicard_construct (g_object_new (GAL_TYPE_VIEW_FACTORY_MINICARD, NULL));
}

/**
 * gal_view_minicard_construct
 * @factory: The factory to construct
 *
 * constructs the GalViewFactoryMinicard.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewFactoryMinicard.
 */
GalViewFactory *
gal_view_factory_minicard_construct  (GalViewFactoryMinicard *factory)
{
	return GAL_VIEW_FACTORY(factory);
}



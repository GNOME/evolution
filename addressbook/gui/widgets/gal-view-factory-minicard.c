/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory-minicard.c: A View Factory
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "gal-view-factory-minicard.h"
#include "gal-view-minicard.h"

#define GVFE_CLASS(e) ((GalViewFactoryMinicardClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gal_view_factory_get_type ()

static GalViewFactoryClass *gal_view_factory_minicard_parent_class;

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
gal_view_factory_minicard_destroy         (GtkObject *object)
{
#if 0
	GalViewFactoryMinicard *factory = GAL_VIEW_FACTORY_MINICARD(object);
#endif
}

static void
gal_view_factory_minicard_class_init      (GtkObjectClass *object_class)
{
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS(object_class);
	gal_view_factory_minicard_parent_class    = gtk_type_class (PARENT_TYPE);

	view_factory_class->get_title           = gal_view_factory_minicard_get_title;
	view_factory_class->new_view            = gal_view_factory_minicard_new_view;
	view_factory_class->get_type_code       = gal_view_factory_minicard_get_type_code;

	object_class->destroy                   = gal_view_factory_minicard_destroy;
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
	return gal_view_factory_minicard_construct (gtk_type_new (gal_view_factory_minicard_get_type ()));
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

GtkType
gal_view_factory_minicard_get_type  (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewFactoryMinicard",
			sizeof (GalViewFactoryMinicard),
			sizeof (GalViewFactoryMinicardClass),
			(GtkClassInitFunc) gal_view_factory_minicard_class_init,
			(GtkObjectInitFunc) gal_view_factory_minicard_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

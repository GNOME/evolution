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

GType
gal_view_factory_minicard_get_type  (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (GalViewFactoryMinicardClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gal_view_factory_minicard_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (GalViewFactoryMinicard),
			0,             /* n_preallocs */
			(GInstanceInitFunc) gal_view_factory_minicard_init,
		};

		type = g_type_register_static (PARENT_TYPE, "GalViewFactoryMinicard", &info, 0);
	}

	return type;
}

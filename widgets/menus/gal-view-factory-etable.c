/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory.c: A View Factory
 *
 * Authors:
 *   Chris Lahey (clahey@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include "gal-view-factory-etable.h"
#include "gal-view-etable.h"
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#define GVFE_CLASS(e) ((GalViewFactoryEtableClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gal_view_factory_get_type ()

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
gal_view_factory_etable_destroy         (GtkObject *object)
{
	GalViewFactoryEtable *factory = GAL_VIEW_FACTORY_ETABLE(object);

	if (factory->spec)
		gtk_object_unref(GTK_OBJECT(factory->spec));
}

static void
gal_view_factory_etable_class_init      (GtkObjectClass *object_class)
{
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS(object_class);
	gal_view_factory_etable_parent_class    = gtk_type_class (PARENT_TYPE);

	view_factory_class->get_title           = gal_view_factory_etable_get_title;
	view_factory_class->new_view            = gal_view_factory_etable_new_view;
	view_factory_class->get_type_code       = gal_view_factory_etable_get_type_code;

	object_class->destroy                   = gal_view_factory_etable_destroy;
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
	return gal_view_factory_etable_construct (gtk_type_new (gal_view_factory_etable_get_type ()), spec);
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
		gtk_object_ref(GTK_OBJECT(spec));
	factory->spec = spec;
	return GAL_VIEW_FACTORY(factory);
}

GtkType
gal_view_factory_etable_get_type  (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewFactoryEtable",
			sizeof (GalViewFactoryEtable),
			sizeof (GalViewFactoryEtableClass),
			(GtkClassInitFunc) gal_view_factory_etable_class_init,
			(GtkObjectInitFunc) gal_view_factory_etable_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

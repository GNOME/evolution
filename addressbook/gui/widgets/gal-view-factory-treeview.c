/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8
       -*- */
/*
 * gal-view-factory-treeview.c: A View Factory
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
#include "gal-view-factory-treeview.h"
#include "gal-view-treeview.h"

G_DEFINE_TYPE(GalViewFactoryTreeView, gal_view_factory_treeview, GAL_VIEW_FACTORY_TYPE)

static const char *
gal_view_factory_treeview_get_title       (GalViewFactory *factory)
{
	return _("GTK Tree View");
}

static GalView *
gal_view_factory_treeview_new_view        (GalViewFactory *factory,
					   const char     *name)
{
	return gal_view_treeview_new(name);
}

static const char *
gal_view_factory_treeview_get_type_code (GalViewFactory *factory)
{
	return "treeview";
}

static void
gal_view_factory_treeview_class_init      (GalViewFactoryTreeViewClass *treeview_class)
{
	GalViewFactoryClass *view_factory_class = GAL_VIEW_FACTORY_CLASS(treeview_class);

	view_factory_class->get_title           = gal_view_factory_treeview_get_title;
	view_factory_class->new_view            = gal_view_factory_treeview_new_view;
	view_factory_class->get_type_code       = gal_view_factory_treeview_get_type_code;
}

static void
gal_view_factory_treeview_init            (GalViewFactoryTreeView *factory)
{
}

/**
 * gal_view_treeview_new
 *
 * A new GalViewFactory for creating TreeView views.  Create one of
 * these and pass it to GalViewCollection for use.
 *
 * Returns: The new GalViewFactoryTreeView.
 */
GalViewFactory *
gal_view_factory_treeview_new        (void)
{
	return gal_view_factory_treeview_construct (g_object_new (GAL_TYPE_VIEW_FACTORY_TREEVIEW, NULL));
}

/**
 * gal_view_treeview_construct
 * @factory: The factory to construct
 *
 * constructs the GalViewFactoryTreeView.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewFactoryTreeView.
 */
GalViewFactory *
gal_view_factory_treeview_construct  (GalViewFactoryTreeView *factory)
{
	return GAL_VIEW_FACTORY(factory);
}

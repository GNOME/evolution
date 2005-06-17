/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory-treeview.c: A View Factory
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#ifndef _GAL_VIEW_FACTORY_TREEVIEW_H_
#define _GAL_VIEW_FACTORY_TREEVIEW_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view-factory.h>

#define GAL_TYPE_VIEW_FACTORY_TREEVIEW        (gal_view_factory_treeview_get_type ())
#define GAL_VIEW_FACTORY_TREEVIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_TYPE_VIEW_FACTORY_TREEVIEW, GalViewFactoryTreeView))
#define GAL_VIEW_FACTORY_TREEVIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_TYPE_VIEW_FACTORY_TREEVIEW, GalViewFactoryTreeViewClass))
#define GAL_IS_VIEW_FACTORY_TREEVIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_TYPE_VIEW_FACTORY_TREEVIEW))
#define GAL_IS_VIEW_FACTORY_TREEVIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_TYPE_VIEW_FACTORY_TREEVIEW))

typedef struct {
	GalViewFactory base;
} GalViewFactoryTreeView;

typedef struct {
	GalViewFactoryClass parent_class;
} GalViewFactoryTreeViewClass;

/* Standard functions */
GType           gal_view_factory_treeview_get_type   (void);
GalViewFactory *gal_view_factory_treeview_new        (void);
GalViewFactory *gal_view_factory_treeview_construct  (GalViewFactoryTreeView *factory);

#endif /* _GAL_VIEW_FACTORY_TREEVIEW_H_ */

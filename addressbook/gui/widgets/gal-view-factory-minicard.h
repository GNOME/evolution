/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-factory-minicard.c: A View Factory
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#ifndef _GAL_VIEW_FACTORY_MINICARD_H_
#define _GAL_VIEW_FACTORY_MINICARD_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view-factory.h>

#define GAL_TYPE_VIEW_FACTORY_MINICARD        (gal_view_factory_minicard_get_type ())
#define GAL_VIEW_FACTORY_MINICARD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_TYPE_VIEW_FACTORY_MINICARD, GalViewFactoryMinicard))
#define GAL_VIEW_FACTORY_MINICARD_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_TYPE_VIEW_FACTORY_MINICARD, GalViewFactoryMinicardClass))
#define GAL_IS_VIEW_FACTORY_MINICARD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_TYPE_VIEW_FACTORY_MINICARD))
#define GAL_IS_VIEW_FACTORY_MINICARD_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_TYPE_VIEW_FACTORY_MINICARD))

typedef struct {
	GalViewFactory base;
} GalViewFactoryMinicard;

typedef struct {
	GalViewFactoryClass parent_class;
} GalViewFactoryMinicardClass;

/* Standard functions */
GType           gal_view_factory_minicard_get_type   (void);
GalViewFactory *gal_view_factory_minicard_new        (void);
GalViewFactory *gal_view_factory_minicard_construct  (GalViewFactoryMinicard *factory);

#endif /* _GAL_VIEW_FACTORY_MINICARD_H_ */

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
#include <gal/menus/gal-view-factory.h>

#define GAL_VIEW_FACTORY_MINICARD_TYPE        (gal_view_factory_minicard_get_type ())
#define GAL_VIEW_FACTORY_MINICARD(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_FACTORY_MINICARD_TYPE, GalViewFactoryMinicard))
#define GAL_VIEW_FACTORY_MINICARD_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_FACTORY_MINICARD_TYPE, GalViewFactoryMinicardClass))
#define GAL_IS_VIEW_FACTORY_MINICARD(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_FACTORY_MINICARD_TYPE))
#define GAL_IS_VIEW_FACTORY_MINICARD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_FACTORY_MINICARD_TYPE))

typedef struct {
	GalViewFactory base;
} GalViewFactoryMinicard;

typedef struct {
	GalViewFactoryClass parent_class;
} GalViewFactoryMinicardClass;

/* Standard functions */
GtkType         gal_view_factory_minicard_get_type   (void);
GalViewFactory *gal_view_factory_minicard_new        (void);
GalViewFactory *gal_view_factory_minicard_construct  (GalViewFactoryMinicard *factory);

#endif /* _GAL_VIEW_FACTORY_MINICARD_H_ */

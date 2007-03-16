/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-factory-etable.h
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

#ifndef _GAL_VIEW_FACTORY_ETABLE_H_
#define _GAL_VIEW_FACTORY_ETABLE_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view-factory.h>
#include <table/e-table-specification.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_VIEW_FACTORY_ETABLE_TYPE        (gal_view_factory_etable_get_type ())
#define GAL_VIEW_FACTORY_ETABLE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_FACTORY_ETABLE_TYPE, GalViewFactoryEtable))
#define GAL_VIEW_FACTORY_ETABLE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_FACTORY_ETABLE_TYPE, GalViewFactoryEtableClass))
#define GAL_IS_VIEW_FACTORY_ETABLE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_FACTORY_ETABLE_TYPE))
#define GAL_IS_VIEW_FACTORY_ETABLE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_FACTORY_ETABLE_TYPE))
#define GAL_VIEW_FACTORY_ETABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GAL_VIEW_FACTORY_ETABLE_TYPE, GalViewFactoryEtableClass))

typedef struct {
	GalViewFactory base;

	ETableSpecification *spec;
} GalViewFactoryEtable;

typedef struct {
	GalViewFactoryClass parent_class;
} GalViewFactoryEtableClass;

/* Standard functions */
GType           gal_view_factory_etable_get_type   (void);
GalViewFactory *gal_view_factory_etable_new        (ETableSpecification  *spec);
GalViewFactory *gal_view_factory_etable_construct  (GalViewFactoryEtable *factory,
						    ETableSpecification  *spec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAL_VIEW_FACTORY_ETABLE_H_ */

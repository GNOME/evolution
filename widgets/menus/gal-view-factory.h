/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-factory.h
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

#ifndef _GAL_VIEW_FACTORY_H_
#define _GAL_VIEW_FACTORY_H_

#include <gtk/gtkobject.h>
#include <widgets/menus/gal-view.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_VIEW_FACTORY_TYPE        (gal_view_factory_get_type ())
#define GAL_VIEW_FACTORY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_FACTORY_TYPE, GalViewFactory))
#define GAL_VIEW_FACTORY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_FACTORY_TYPE, GalViewFactoryClass))
#define GAL_IS_VIEW_FACTORY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_FACTORY_TYPE))
#define GAL_IS_VIEW_FACTORY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_FACTORY_TYPE))
#define GAL_VIEW_FACTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GAL_VIEW_FACTORY_TYPE, GalViewFactoryClass))

typedef struct {
	GObject base;
} GalViewFactory;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	const char *(*get_title)      (GalViewFactory *factory);
	const char *(*get_type_code)  (GalViewFactory *factory);
	GalView    *(*new_view)       (GalViewFactory *factory,
				       const char     *name);
} GalViewFactoryClass;

/* Standard functions */
GType       gal_view_factory_get_type       (void);

/* Query functions */
/* Returns already translated title. */
const char *gal_view_factory_get_title      (GalViewFactory *factory);

/* Returns the code for use in identifying this type of object in the
 * view list.  This identifier should identify this as being the
 * unique factory for xml files which were written out with this
 * identifier.  Thus each factory should have a unique type code.  */
const char *gal_view_factory_get_type_code  (GalViewFactory *factory);

/* Create a new view */
GalView    *gal_view_factory_new_view       (GalViewFactory *factory,
					     const char     *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _GAL_VIEW_FACTORY_H_ */

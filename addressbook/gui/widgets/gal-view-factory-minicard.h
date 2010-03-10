/*
 *
 * gal-view-factory-minicard.c: A View Factory
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GAL_VIEW_FACTORY_MINICARD_H
#define GAL_VIEW_FACTORY_MINICARD_H

#include <glib-object.h>
#include <widgets/menus/gal-view-factory.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_FACTORY_MINICARD \
	(gal_view_factory_minicard_get_type ())
#define GAL_VIEW_FACTORY_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_FACTORY_MINICARD, GalViewFactoryMinicard))
#define GAL_VIEW_FACTORY_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_FACTORY_MINICARD, GalViewFactoryMinicardClass))
#define GAL_IS_VIEW_FACTORY_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_FACTORY_MINICARD))
#define GAL_IS_VIEW_FACTORY_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_FACTORY_MINICARD))
#define GAL_VIEW_FACTORY_MINICARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_FACTORY_MINICARD, GalViewFactoryMinicardClass))

G_BEGIN_DECLS

typedef struct _GalViewFactoryMinicard GalViewFactoryMinicard;
typedef struct _GalViewFactoryMinicardClass GalViewFactoryMinicardClass;

struct _GalViewFactoryMinicard {
	GalViewFactory parent;
};

struct _GalViewFactoryMinicardClass {
	GalViewFactoryClass parent_class;
};

GType		gal_view_factory_minicard_get_type	(void);
GalViewFactory *gal_view_factory_minicard_new		(void);

G_END_DECLS

#endif /* GAL_VIEW_FACTORY_MINICARD_H */

/*
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

#ifndef GAL_VIEW_FACTORY_H
#define GAL_VIEW_FACTORY_H

#include <glib-object.h>
#include <menus/gal-view.h>

/* Standard GObject macros */
#define GAL_TYPE_VIEW_FACTORY \
	(gal_view_factory_get_type ())
#define GAL_VIEW_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_FACTORY, GalViewFactory))
#define GAL_VIEW_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_FACTORY, GalViewFactoryClass))
#define GAL_IS_VIEW_FACTORY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_FACTORY))
#define GAL_IS_VIEW_FACTORY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_FACTORY))
#define GAL_VIEW_FACTORY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_FACTORY, GalViewFactoryClass))

G_BEGIN_DECLS

typedef struct _GalViewFactory GalViewFactory;
typedef struct _GalViewFactoryClass GalViewFactoryClass;

struct _GalViewFactory {
	GObject parent;
};

struct _GalViewFactoryClass {
	GObjectClass parent_class;

	/* Methods */
	const gchar *	(*get_title)		(GalViewFactory *factory);
	const gchar *	(*get_type_code)	(GalViewFactory *factory);
	GalView *	(*new_view)		(GalViewFactory *factory,
						 const gchar *name);
};

GType		gal_view_factory_get_type	(void);
const gchar *	gal_view_factory_get_title	(GalViewFactory *factory);

/* Returns the code for use in identifying this type of object in the
 * view list.  This identifier should identify this as being the
 * unique factory for xml files which were written out with this
 * identifier.  Thus each factory should have a unique type code.  */
const gchar *	gal_view_factory_get_type_code	(GalViewFactory *factory);

GalView *	gal_view_factory_new_view	(GalViewFactory *factory,
						 const gchar *name);

G_END_DECLS

#endif /* GAL_VIEW_FACTORY_H */

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

#ifndef _GAL_VIEW_FACTORY_H_
#define _GAL_VIEW_FACTORY_H_

#include <glib-object.h>
#include <widgets/menus/gal-view.h>

G_BEGIN_DECLS

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
	const gchar *(*get_title)      (GalViewFactory *factory);
	const gchar *(*get_type_code)  (GalViewFactory *factory);
	GalView    *(*new_view)       (GalViewFactory *factory,
				       const gchar     *name);
} GalViewFactoryClass;

/* Standard functions */
GType       gal_view_factory_get_type       (void);

/* Query functions */
/* Returns already translated title. */
const gchar *gal_view_factory_get_title      (GalViewFactory *factory);

/* Returns the code for use in identifying this type of object in the
 * view list.  This identifier should identify this as being the
 * unique factory for xml files which were written out with this
 * identifier.  Thus each factory should have a unique type code.  */
const gchar *gal_view_factory_get_type_code  (GalViewFactory *factory);

/* Create a new view */
GalView    *gal_view_factory_new_view       (GalViewFactory *factory,
					     const gchar     *name);

G_END_DECLS

#endif /* _GAL_VIEW_FACTORY_H_ */

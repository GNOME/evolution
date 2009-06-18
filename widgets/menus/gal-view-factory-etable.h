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

#ifndef _GAL_VIEW_FACTORY_ETABLE_H_
#define _GAL_VIEW_FACTORY_ETABLE_H_

#include <gtk/gtk.h>
#include <widgets/menus/gal-view-factory.h>
#include <table/e-table-specification.h>

G_BEGIN_DECLS

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

G_END_DECLS

#endif /* _GAL_VIEW_FACTORY_ETABLE_H_ */

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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _GAL_VIEW_MENUS_H_
#define _GAL_VIEW_MENUS_H_

#include <libxml/tree.h>
#include <bonobo/bonobo-ui-component.h>
#include <widgets/menus/gal-view-instance.h>

#include <glib-object.h>

#define GAL_VIEW_MENUS_TYPE        (gal_view_menus_get_type ())
#define GAL_VIEW_MENUS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_VIEW_MENUS_TYPE, GalViewMenus))
#define GAL_VIEW_MENUS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_VIEW_MENUS_TYPE, GalViewMenusClass))
#define GAL_IS_VIEW_MENUS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_VIEW_MENUS_TYPE))
#define GAL_IS_VIEW_MENUS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_VIEW_MENUS_TYPE))

typedef struct _GalViewMenusPrivate GalViewMenusPrivate;

typedef struct {
	GObject base;
	GalViewMenusPrivate *priv;
} GalViewMenus;

typedef struct {
	GObjectClass parent_class;
} GalViewMenusClass;

GType         gal_view_menus_get_type               (void);
GalViewMenus *gal_view_menus_new                    (GalViewInstance   *instance);
GalViewMenus *gal_view_menus_construct              (GalViewMenus      *menus,
						     GalViewInstance   *instance);

void          gal_view_menus_set_show_define_views  (GalViewMenus      *menus,
						     gboolean           show_define_views);

void          gal_view_menus_apply                  (GalViewMenus      *menus,
						     BonoboUIComponent *component,
						     CORBA_Environment *opt_ev);
void          gal_view_menus_unmerge                (GalViewMenus      *gvm,
						     CORBA_Environment *opt_ev);
void          gal_view_menus_set_instance           (GalViewMenus      *gvm,
						     GalViewInstance   *instance);

#endif /* _GAL_VIEW_MENUS_H_ */

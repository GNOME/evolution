/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_MENUS_H_
#define _GAL_VIEW_MENUS_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>
#include <bonobo/bonobo-ui-component.h>
#include <gal/menus/gal-view-collection.h>

#define GAL_VIEW_MENUS_TYPE        (gal_view_menus_get_type ())
#define GAL_VIEW_MENUS(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_MENUS_TYPE, GalViewMenus))
#define GAL_VIEW_MENUS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_MENUS_TYPE, GalViewMenusClass))
#define GAL_IS_VIEW_MENUS(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_MENUS_TYPE))
#define GAL_IS_VIEW_MENUS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_MENUS_TYPE))

typedef struct _GalViewMenusPrivate GalViewMenusPrivate;

typedef struct {
	GtkObject base;
	GalViewMenusPrivate *priv;
} GalViewMenus;

typedef struct {
	GtkObjectClass parent_class;
} GalViewMenusClass;

GtkType       gal_view_menus_get_type   (void);
GalViewMenus *gal_view_menus_new        (GalViewCollection *collection);
GalViewMenus *gal_view_menus_construct  (GalViewMenus      *menus,
					 GalViewCollection *collection);

void          gal_view_menus_apply      (GalViewMenus      *menus,
					 BonoboUIComponent *component,
					 CORBA_Environment *ev);					      

#endif /* _GAL_VIEW_MENUS_H_ */

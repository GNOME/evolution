/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_MENUS_H_
#define _GAL_VIEW_MENUS_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>
#include <bonobo/bonobo-ui-component.h>

#define GAL_VIEW_MENUS_TYPE        (gal_view_menus_get_type ())
#define GAL_VIEW_MENUS(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_MENUS_TYPE, GalViewMenus))
#define GAL_VIEW_MENUS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_MENUS_TYPE, GalViewMenusClass))
#define GAL_IS_VIEW_MENUS(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_MENUS_TYPE))
#define GAL_IS_VIEW_MENUS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_MENUS_TYPE))

typedef struct {
	GtkObject base;
	void *priv;
} GalViewMenus;

typedef struct {
	GtkObjectClass parent_class;
} GalViewMenusClass;

GtkType       gal_view_menus_get_type  (void);
GalViewMenus *gal_view_menus_new       (void);

void          gal_view_menus_apply     (GalViewMenus      *menus,
					BonoboUIComponent *component,
					CORBA_Environment *ev);					      

#if 0
gboolean     gal_view_menus_load_from_file    (GalViewMenus   *menus,
					      const char    *filename);
void         gal_view_menus_load_from_string  (GalViewMenus   *menus,
					      const char    *xml);
void         gal_view_menus_load_from_node    (GalViewMenus   *menus,
					      const xmlNode *node);

void         gal_view_menus_save_to_file      (GalViewMenus   *menus,
					      const char    *filename);
char        *gal_view_menus_save_to_string    (GalViewMenus   *menus);
xmlNode     *gal_view_menus_save_to_node      (GalViewMenus   *menus,
					      xmlNode       *parent);
#endif

#endif /* _GAL_VIEW_MENUS_H_ */

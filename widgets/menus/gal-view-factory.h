/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_FACTORY_H_
#define _GAL_VIEW_FACTORY_H_

#include <gtk/gtkobject.h>
#include <gal/menus/gal-view.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_VIEW_FACTORY_TYPE        (gal_view_factory_get_type ())
#define GAL_VIEW_FACTORY(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_FACTORY_TYPE, GalViewFactory))
#define GAL_VIEW_FACTORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_FACTORY_TYPE, GalViewFactoryClass))
#define GAL_IS_VIEW_FACTORY(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_FACTORY_TYPE))
#define GAL_IS_VIEW_FACTORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_FACTORY_TYPE))

typedef struct {
	GtkObject base;
} GalViewFactory;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	const char *(*get_title)      (GalViewFactory *factory);
	const char *(*get_type_code)  (GalViewFactory *factory);
	GalView    *(*new_view)       (GalViewFactory *factory,
				       const char     *name);
} GalViewFactoryClass;

/* Standard functions */
GtkType     gal_view_factory_get_type       (void);

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

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_FACTORY_ETABLE_H_
#define _GAL_VIEW_FACTORY_ETABLE_H_

#include <gtk/gtkobject.h>
#include <gal/menus/gal-view-factory.h>
#include <gal/e-table/e-table-specification.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_VIEW_FACTORY_ETABLE_TYPE        (gal_view_factory_etable_get_type ())
#define GAL_VIEW_FACTORY_ETABLE(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_FACTORY_ETABLE_TYPE, GalViewFactoryEtable))
#define GAL_VIEW_FACTORY_ETABLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_FACTORY_ETABLE_TYPE, GalViewFactoryEtableClass))
#define GAL_IS_VIEW_FACTORY_ETABLE(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_FACTORY_ETABLE_TYPE))
#define GAL_IS_VIEW_FACTORY_ETABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_FACTORY_ETABLE_TYPE))

typedef struct {
	GalViewFactory base;

	ETableSpecification *spec;
} GalViewFactoryEtable;

typedef struct {
	GalViewFactoryClass parent_class;
} GalViewFactoryEtableClass;

/* Standard functions */
GtkType         gal_view_factory_etable_get_type   (void);
GalViewFactory *gal_view_factory_etable_new        (ETableSpecification  *spec);
GalViewFactory *gal_view_factory_etable_construct  (GalViewFactoryEtable *factory,
						    ETableSpecification  *spec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAL_VIEW_FACTORY_ETABLE_H_ */

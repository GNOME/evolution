/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_ETABLE_H_
#define _GAL_VIEW_ETABLE_H_

#include <gtk/gtkobject.h>
#include <gal/menus/gal-view.h>
#include <gal/e-table/e-table-state.h>
#include <gal/e-table/e-table-specification.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_VIEW_ETABLE_TYPE        (gal_view_etable_get_type ())
#define GAL_VIEW_ETABLE(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_ETABLE_TYPE, GalViewEtable))
#define GAL_VIEW_ETABLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_ETABLE_TYPE, GalViewEtableClass))
#define GAL_IS_VIEW_ETABLE(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_ETABLE_TYPE))
#define GAL_IS_VIEW_ETABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_ETABLE_TYPE))

typedef struct {
	GalView base;

	ETableSpecification *spec;
	ETableState         *state;
	char                *title;
} GalViewEtable;

typedef struct {
	GalViewClass parent_class;
} GalViewEtableClass;

/* Standard functions */
GtkType  gal_view_etable_get_type   (void);
GalView *gal_view_etable_new        (ETableSpecification *spec,
				     const gchar         *title);
GalView *gal_view_etable_construct  (GalViewEtable       *view,
				     ETableSpecification *spec,
				     const gchar         *title);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAL_VIEW_ETABLE_H_ */

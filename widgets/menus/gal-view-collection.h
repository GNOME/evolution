/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_SET_H_
#define _GAL_VIEW_SET_H_

#include <gtk/gtkobject.h>
#include <gal/menus/gal-view-factory.h>

#define GAL_VIEW_COLLECTION_TYPE        (gal_view_collection_get_type ())
#define GAL_VIEW_COLLECTION(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_COLLECTION_TYPE, GalViewCollection))
#define GAL_VIEW_COLLECTION_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_COLLECTION_TYPE, GalViewCollectionClass))
#define GAL_IS_VIEW_COLLECTION(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_COLLECTION_TYPE))
#define GAL_IS_VIEW_COLLECTION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_COLLECTION_TYPE))

typedef struct {
	GtkObject base;

	GList *view_list;
	GList *factory_list;
} GalViewCollection;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Signals
	 */
	void (*display_view) (GalViewCollection *collection,
			      GalView    *view);
} GalViewCollectionClass;

/* Standard functions */
GtkType            gal_view_collection_get_type                 (void);
GalViewCollection *gal_view_collection_new                      (void);

/* Set up the view collection */
void               gal_view_collection_set_storage_directories  (GalViewCollection *collection,
								 char              *system_dir,
								 char              *local_dir);
void               gal_view_collection_add_factory              (GalViewCollection *collection,
								 GalViewFactory    *factory);

/* Send the display view signal. */
void               gal_view_collection_display_view             (GalViewCollection *collection,
								 GalView           *view);

#endif /* _GAL_VIEW_COLLECTION_H_ */

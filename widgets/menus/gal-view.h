/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _GAL_VIEW_H_
#define _GAL_VIEW_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>

#define GAL_VIEW_TYPE        (gal_view_get_type ())
#define GAL_VIEW(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_TYPE, GalView))
#define GAL_VIEW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_TYPE, GalViewClass))
#define GAL_IS_VIEW(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_TYPE))
#define GAL_IS_VIEW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_TYPE))

typedef struct {
	GtkObject base;
} GalView;

typedef struct {
	GtkObjectClass parent_class;

	/*
	 * Virtual methods
	 */
	void        (*edit)       (GalView    *view);
	void        (*load)       (GalView    *view,
				   const char *filename);
	void        (*save)       (GalView    *view,
				   const char *filename);
	const char *(*get_title)  (GalView    *view);
	void        (*set_title)  (GalView    *view,
				   const char *title);
	GalView    *(*clone)      (GalView    *view);
} GalViewClass;

/* Standard functions */
GtkType     gal_view_get_type   (void);

/* Open an editor dialog for this view. */
void        gal_view_edit       (GalView    *view);

/* xml load and save functions */
void        gal_view_load       (GalView    *view,
				 const char *filename);
void        gal_view_save       (GalView    *view,
				 const char *filename);

/* Title functions */
const char *gal_view_get_title  (GalView    *view);
void        gal_view_set_title  (GalView    *view,
				 const char *title);

/* Cloning the view */
GalView    *gal_view_clone      (GalView    *view);


#endif /* _GAL_VIEW_H_ */

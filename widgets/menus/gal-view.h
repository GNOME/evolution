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
	char *name;
} GalView;

typedef struct {
	GtkObjectClass parent_class;
} GalViewClass;

GtkType  gal_view_get_type  (void);
GalView *gal_view_new       (void);

#endif /* _GAL_VIEW_H_ */

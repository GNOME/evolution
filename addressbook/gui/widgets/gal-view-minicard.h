/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-minicard.h: An Minicard View
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#ifndef _GAL_VIEW_MINICARD_H_
#define _GAL_VIEW_MINICARD_H_

#include <gtk/gtkobject.h>
#include <gal/menus/gal-view.h>

#define GAL_VIEW_MINICARD_TYPE        (gal_view_minicard_get_type ())
#define GAL_VIEW_MINICARD(o)          (GTK_CHECK_CAST ((o), GAL_VIEW_MINICARD_TYPE, GalViewMinicard))
#define GAL_VIEW_MINICARD_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), GAL_VIEW_MINICARD_TYPE, GalViewMinicardClass))
#define GAL_IS_VIEW_MINICARD(o)       (GTK_CHECK_TYPE ((o), GAL_VIEW_MINICARD_TYPE))
#define GAL_IS_VIEW_MINICARD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GAL_VIEW_MINICARD_TYPE))

typedef struct {
	GalView base;

	char                *title;
} GalViewMinicard;

typedef struct {
	GalViewClass parent_class;
} GalViewMinicardClass;

/* Standard functions */
GtkType  gal_view_minicard_get_type   (void);
GalView *gal_view_minicard_new        (const gchar         *title);
GalView *gal_view_minicard_construct  (GalViewMinicard       *view,
				       const gchar         *title);

#endif /* _GAL_VIEW_MINICARD_H_ */

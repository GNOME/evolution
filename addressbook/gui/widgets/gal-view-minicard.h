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

#include <widgets/menus/gal-view.h>
#include <e-minicard-view-widget.h>

#define GAL_TYPE_VIEW_MINICARD        (gal_view_minicard_get_type ())
#define GAL_VIEW_MINICARD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_TYPE_VIEW_MINICARD, GalViewMinicard))
#define GAL_VIEW_MINICARD_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_TYPE_VIEW_MINICARD, GalViewMinicardClass))
#define GAL_IS_VIEW_MINICARD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_TYPE_VIEW_MINICARD))
#define GAL_IS_VIEW_MINICARD_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_TYPE_VIEW_MINICARD))

typedef struct {
	GalView              base;

	char                *title;
	double               column_width;

	EMinicardViewWidget *emvw;
	guint                emvw_column_width_changed_id;
} GalViewMinicard;

typedef struct {
	GalViewClass parent_class;
} GalViewMinicardClass;

/* Standard functions */
GType    gal_view_minicard_get_type   (void);
GalView *gal_view_minicard_new        (const gchar         *title);
GalView *gal_view_minicard_construct  (GalViewMinicard     *view,
				       const gchar         *title);
void     gal_view_minicard_attach     (GalViewMinicard     *view,
				       EMinicardViewWidget *emvw);
void     gal_view_minicard_detach     (GalViewMinicard     *view);

#endif /* _GAL_VIEW_MINICARD_H_ */

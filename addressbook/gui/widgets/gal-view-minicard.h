/*
 * gal-view-minicard.h: An Minicard View
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _GAL_VIEW_MINICARD_H_
#define _GAL_VIEW_MINICARD_H_

#include <widgets/menus/gal-view.h>
#include <e-minicard-view-widget.h>
#include "e-addressbook-view.h"

#define GAL_TYPE_VIEW_MINICARD        (gal_view_minicard_get_type ())
#define GAL_VIEW_MINICARD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GAL_TYPE_VIEW_MINICARD, GalViewMinicard))
#define GAL_VIEW_MINICARD_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), GAL_TYPE_VIEW_MINICARD, GalViewMinicardClass))
#define GAL_IS_VIEW_MINICARD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), GAL_TYPE_VIEW_MINICARD))
#define GAL_IS_VIEW_MINICARD_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GAL_TYPE_VIEW_MINICARD))

typedef struct {
	GalView              base;

	gchar                *title;
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
				       EABView *address_view);
void     gal_view_minicard_detach     (GalViewMinicard     *view);

#endif /* _GAL_VIEW_MINICARD_H_ */

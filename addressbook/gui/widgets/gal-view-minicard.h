/*
 * gal-view-minicard.h: An Minicard View
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GAL_VIEW_MINICARD_H
#define GAL_VIEW_MINICARD_H

#include <e-util/e-util.h>
#include "e-minicard-view-widget.h"
#include "e-addressbook-view.h"

/* Standard GObject macros */
#define GAL_TYPE_VIEW_MINICARD \
	(gal_view_minicard_get_type ())
#define GAL_VIEW_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GAL_TYPE_VIEW_MINICARD, GalViewMinicard))
#define GAL_VIEW_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), GAL_TYPE_VIEW_MINICARD, GalViewMinicardClass))
#define GAL_IS_VIEW_MINICARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GAL_TYPE_VIEW_MINICARD))
#define GAL_IS_VIEW_MINICARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GAL_TYPE_VIEW_MINICARD))
#define GAL_VIEW_MINICARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GAL_TYPE_VIEW_MINICARD, GalViewMinicardClass))

G_BEGIN_DECLS

typedef struct _GalViewMinicard GalViewMinicard;
typedef struct _GalViewMinicardClass GalViewMinicardClass;

struct _GalViewMinicard {
	GalView              parent;

	gdouble               column_width;

	EMinicardViewWidget *emvw;
	guint                emvw_column_width_changed_id;
};

struct _GalViewMinicardClass {
	GalViewClass parent_class;
};

GType		gal_view_minicard_get_type	(void);
GalView *	gal_view_minicard_new		(const gchar *title);
void		gal_view_minicard_attach	(GalViewMinicard *view,
						 EAddressbookView *address_view);
void		gal_view_minicard_detach	(GalViewMinicard *view);

G_END_DECLS

#endif /* GAL_VIEW_MINICARD_H */

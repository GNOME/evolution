/*
 * gal-view-minicard.h: A Minicard View
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
#include "e-addressbook-view.h"

#define GAL_TYPE_VIEW_MINICARD	(gal_view_minicard_get_type ())

G_BEGIN_DECLS

typedef enum _ECardsSortBy {
	E_CARDS_SORT_BY_FILE_AS = 0,
	E_CARDS_SORT_BY_GIVEN_NAME,
	E_CARDS_SORT_BY_FAMILY_NAME
} ECardsSortBy;

G_DECLARE_FINAL_TYPE (GalViewMinicard, gal_view_minicard, GAL, VIEW_MINICARD, GalView)

GalView *	gal_view_minicard_new		(const gchar *title);
void		gal_view_minicard_attach	(GalViewMinicard *view,
						 EAddressbookView *address_view);
void		gal_view_minicard_detach	(GalViewMinicard *view);
ECardsSortBy	gal_view_minicard_get_sort_by	(GalViewMinicard *self);
void		gal_view_minicard_set_sort_by	(GalViewMinicard *self,
						 ECardsSortBy sort_by);

G_END_DECLS

#endif /* GAL_VIEW_MINICARD_H */

/*
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * EWeekViewTitlesItem - displays the 'Monday', 'Tuesday' etc. at the top of
 * the Month calendar view.
 */

#ifndef E_WEEK_VIEW_TITLES_ITEM_H
#define E_WEEK_VIEW_TITLES_ITEM_H

#include "e-week-view.h"

/* Standard GObject macros */
#define E_TYPE_WEEK_VIEW_TITLES_ITEM \
	(e_week_view_titles_item_get_type ())
#define E_WEEK_VIEW_TITLES_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEEK_VIEW_TITLES_ITEM, EWeekViewTitlesItem))
#define E_WEEK_VIEW_TITLES_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEEK_VIEW_TITLES_ITEM, EWeekViewTitlesItemClass))
#define E_IS_WEEK_VIEW_TITLES_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEEK_VIEW_TITLES_ITEM))
#define E_IS_WEEK_VIEW_TITLES_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEEK_VIEW_TITLES_ITEM))
#define E_WEEK_VIEW_TITLES_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEEK_VIEW_TITLES_ITEM, EWeekViewTitlesItemClass))

G_BEGIN_DECLS

typedef struct _EWeekViewTitlesItem EWeekViewTitlesItem;
typedef struct _EWeekViewTitlesItemClass EWeekViewTitlesItemClass;
typedef struct _EWeekViewTitlesItemPrivate EWeekViewTitlesItemPrivate;

struct _EWeekViewTitlesItem {
	GnomeCanvasItem parent;
	EWeekViewTitlesItemPrivate *priv;
};

struct _EWeekViewTitlesItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_week_view_titles_item_get_type (void);
EWeekView *	e_week_view_titles_item_get_week_view
						(EWeekViewTitlesItem *titles_item);
void		e_week_view_titles_item_set_week_view
						(EWeekViewTitlesItem *titles_item,
						 EWeekView *week_view);

G_END_DECLS

#endif /* E_WEEK_VIEW_TITLES_ITEM_H */

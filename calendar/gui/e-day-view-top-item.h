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
 * EDayViewTopItem - displays the top part of the Day/Work Week calendar view.
 */

#ifndef E_DAY_VIEW_TOP_ITEM_H
#define E_DAY_VIEW_TOP_ITEM_H

#include "e-day-view.h"

/* Standard GObject macros */
#define E_TYPE_DAY_VIEW_TOP_ITEM \
	(e_day_view_top_item_get_type ())
#define E_DAY_VIEW_TOP_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DAY_VIEW_TOP_ITEM, EDayViewTopItem))
#define E_DAY_VIEW_TOP_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DAY_VIEW_TOP_ITEM, EDayViewTopItemClass))
#define E_IS_DAY_VIEW_TOP_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DAY_VIEW_TOP_ITEM))
#define E_IS_DAY_VIEW_TOP_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DAY_VIEW_TOP_ITEM))
#define E_DAY_VIEW_TOP_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DAY_VIEW_TOP_ITEM, EDayViewTopItemClass))

G_BEGIN_DECLS

typedef struct _EDayViewTopItem EDayViewTopItem;
typedef struct _EDayViewTopItemClass EDayViewTopItemClass;
typedef struct _EDayViewTopItemPrivate EDayViewTopItemPrivate;

struct _EDayViewTopItem {
	GnomeCanvasItem parent;
	EDayViewTopItemPrivate *priv;
};

struct _EDayViewTopItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_day_view_top_item_get_type	(void);
void		e_day_view_top_item_get_day_label
						(EDayView *day_view,
						 gint day,
						 gchar *buffer,
						 gint buffer_len);
EDayView *	e_day_view_top_item_get_day_view (EDayViewTopItem *top_item);
void		e_day_view_top_item_set_day_view (EDayViewTopItem *top_item,
						 EDayView *day_view);
gboolean	e_day_view_top_item_get_show_dates
						(EDayViewTopItem *top_item);
void		e_day_view_top_item_set_show_dates
						(EDayViewTopItem *top_item,
						 gboolean show_dates);

G_END_DECLS

#endif /* E_DAY_VIEW_TOP_ITEM_H */

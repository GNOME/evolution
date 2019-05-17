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
 * EDayViewTimeItem - canvas item which displays the times down the left of
 * the EDayView.
 */

#ifndef E_DAY_VIEW_TIME_ITEM_H
#define E_DAY_VIEW_TIME_ITEM_H

#include "e-day-view.h"

/* Standard GObject macros */
#define E_TYPE_DAY_VIEW_TIME_ITEM \
	(e_day_view_time_item_get_type ())
#define E_DAY_VIEW_TIME_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DAY_VIEW_TIME_ITEM, EDayViewTimeItem))
#define E_DAY_VIEW_TIME_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DAY_VIEW_TIME_ITEM, EDayViewTimeItemClass))
#define E_IS_DAY_VIEW_TIME_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DAY_VIEW_TIME_ITEM))
#define E_IS_DAY_VIEW_TIME_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DAY_VIEW_TIME_ITEM))
#define E_DAY_VIEW_TIME_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DAY_VIEW_TIME_ITEM, EDayViewTimeItemClass))

G_BEGIN_DECLS

typedef struct _EDayViewTimeItem EDayViewTimeItem;
typedef struct _EDayViewTimeItemClass EDayViewTimeItemClass;
typedef struct _EDayViewTimeItemPrivate EDayViewTimeItemPrivate;

struct _EDayViewTimeItem {
	GnomeCanvasItem parent;
	EDayViewTimeItemPrivate *priv;
};

struct _EDayViewTimeItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_day_view_time_item_get_type	(void);
EDayView *	e_day_view_time_item_get_day_view
						(EDayViewTimeItem *time_item);
void		e_day_view_time_item_set_day_view
						(EDayViewTimeItem *time_item,
						 EDayView *day_view);
gint		e_day_view_time_item_get_column_width
						(EDayViewTimeItem *time_item);
ICalTimezone *	e_day_view_time_item_get_second_zone
						(EDayViewTimeItem *time_item);

G_END_DECLS

#endif /* E_DAY_VIEW_TIME_ITEM_H */

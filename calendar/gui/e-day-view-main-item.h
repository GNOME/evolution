/*
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
 * EDayViewMainItem - canvas item which displays most of the appointment
 * data in the main Day/Work Week display.
 */

#ifndef E_DAY_VIEW_MAIN_ITEM_H
#define E_DAY_VIEW_MAIN_ITEM_H

#include "e-day-view.h"

/* Standard GObject macros */
#define E_TYPE_DAY_VIEW_MAIN_ITEM \
	(e_day_view_main_item_get_type ())
#define E_DAY_VIEW_MAIN_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DAY_VIEW_MAIN_ITEM, EDayViewMainItem))
#define E_DAY_VIEW_MAIN_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DAY_VIEW_MAIN_ITEM, EDayViewMainItemClass))
#define E_IS_DAY_VIEW_MAIN_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DAY_VIEW_MAIN_ITEM))
#define E_IS_DAY_VIEW_MAIN_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DAY_VIEW_MAIN_ITEM))
#define E_DAY_VIEW_MAIN_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_TYPE \
	((obj), E_TYPE_DAY_VIEW_MAIN_ITEM, EDayViewMainItemClass))

G_BEGIN_DECLS

typedef struct _EDayViewMainItem EDayViewMainItem;
typedef struct _EDayViewMainItemClass EDayViewMainItemClass;
typedef struct _EDayViewMainItemPrivate EDayViewMainItemPrivate;

struct _EDayViewMainItem {
	GnomeCanvasItem parent;
	EDayViewMainItemPrivate *priv;
};

struct _EDayViewMainItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_day_view_main_item_get_type	(void);
EDayView *	e_day_view_main_item_get_day_view
						(EDayViewMainItem *main_item);
void		e_day_view_main_item_set_day_view
						(EDayViewMainItem *main_item,
						 EDayView *day_view);

G_END_DECLS

#endif /* E_DAY_VIEW_MAIN_ITEM_H */

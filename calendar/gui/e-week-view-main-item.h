/*
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_WEEK_VIEW_MAIN_ITEM_H_
#define _E_WEEK_VIEW_MAIN_ITEM_H_

#include "e-week-view.h"

G_BEGIN_DECLS

/*
 * EWeekViewMainItem - displays the background grid and dates for the Week and
 * Month calendar views.
 */

#define E_WEEK_VIEW_MAIN_ITEM(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        e_week_view_main_item_get_type (), EWeekViewMainItem))
#define E_WEEK_VIEW_MAIN_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k),\
	e_week_view_main_item_get_type ()))
#define E_IS_WEEK_VIEW_MAIN_ITEM(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o), \
	e_week_view_main_item_get_type ()))

typedef struct {
	GnomeCanvasItem canvas_item;

	/* The parent EWeekView widget. */
	EWeekView *week_view;
} EWeekViewMainItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

} EWeekViewMainItemClass;

GType    e_week_view_main_item_get_type      (void);

G_END_DECLS

#endif /* _E_WEEK_VIEW_MAIN_ITEM_H_ */

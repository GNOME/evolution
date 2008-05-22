/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 1999, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */
#ifndef _E_DAY_VIEW_TIME_ITEM_H_
#define _E_DAY_VIEW_TIME_ITEM_H_

#include "e-day-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EDayViewTimeItem - canvas item which displays the times down the left of
 * the EDayView.
 */

#define E_DAY_VIEW_TIME_ITEM(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        e_day_view_time_item_get_type (), EDayViewTimeItem))
#define E_DAY_VIEW_TIME_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k),\
	e_day_view_time_item_get_type ()))
#define E_IS_DAY_VIEW_TIME_ITEM(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o), \
	e_day_view_time_item_get_type ()))

typedef struct {
	GnomeCanvasItem canvas_item;

	/* The parent EDayView widget. */
	EDayView *day_view;

	/* The width of the time column. */
	gint column_width;

	/* TRUE if we are currently dragging the selection times. */
	gboolean dragging_selection;
} EDayViewTimeItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

} EDayViewTimeItemClass;


GType    e_day_view_time_item_get_type      (void);


gint	 e_day_view_time_item_get_column_width (EDayViewTimeItem *dvtmitem);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_DAY_VIEW_TIME_ITEM_H_ */

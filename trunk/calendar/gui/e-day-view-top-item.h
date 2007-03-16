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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_DAY_VIEW_TOP_ITEM_H_
#define _E_DAY_VIEW_TOP_ITEM_H_

#include "e-day-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EDayViewTopItem - displays the top part of the Day/Work Week calendar view.
 */

#define E_DAY_VIEW_TOP_ITEM(obj)     (GTK_CHECK_CAST((obj), \
        e_day_view_top_item_get_type (), EDayViewTopItem))
#define E_DAY_VIEW_TOP_ITEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k),\
	e_day_view_top_item_get_type ()))
#define E_IS_DAY_VIEW_TOP_ITEM(o)    (GTK_CHECK_TYPE((o), \
	e_day_view_top_item_get_type ()))

typedef struct {
	GnomeCanvasItem canvas_item;

	/* The parent EDayView widget. */
	EDayView *day_view;
} EDayViewTopItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

} EDayViewTopItemClass;


GtkType  e_day_view_top_item_get_type      (void);
void e_day_view_top_item_get_day_label (EDayView *day_view, gint day,
					gchar *buffer, gint buffer_len);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_DAY_VIEW_TOP_ITEM_H_ */

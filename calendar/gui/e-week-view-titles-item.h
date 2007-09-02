/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
 * Copyright 2001, Ximian, Inc.
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
#ifndef _E_WEEK_VIEW_TITLES_ITEM_H_
#define _E_WEEK_VIEW_TITLES_ITEM_H_

#include "e-week-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EWeekViewTitlesItem - displays the 'Monday', 'Tuesday' etc. at the top of
 * the Month calendar view.
 */

#define E_WEEK_VIEW_TITLES_ITEM(obj)     (GTK_CHECK_CAST((obj), \
        e_week_view_titles_item_get_type (), EWeekViewTitlesItem))
#define E_WEEK_VIEW_TITLES_ITEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k),\
	e_week_view_titles_item_get_type ()))
#define E_IS_WEEK_VIEW_TITLES_ITEM(o)    (GTK_CHECK_TYPE((o), \
	e_week_view_titles_item_get_type ()))

typedef struct {
	GnomeCanvasItem canvas_item;

	/* The parent EWeekView widget. */
	EWeekView *week_view;
} EWeekViewTitlesItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

} EWeekViewTitlesItemClass;


GtkType  e_week_view_titles_item_get_type      (void);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_WEEK_VIEW_TITLES_ITEM_H_ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#ifndef _E_WEEK_VIEW_EVENT_ITEM_H_
#define _E_WEEK_VIEW_EVENT_ITEM_H_

#include "e-week-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#define E_WEEK_VIEW_EVENT_ITEM(obj)     (GTK_CHECK_CAST((obj), \
        e_week_view_event_item_get_type (), EWeekViewEventItem))
#define E_WEEK_VIEW_EVENT_ITEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k),\
	e_week_view_event_item_get_type ()))
#define E_IS_WEEK_VIEW_EVENT_ITEM(o)    (GTK_CHECK_TYPE((o), \
	e_week_view_event_item_get_type ()))

typedef struct {
	GnomeCanvasItem canvas_item;

	/* The event index in the EWeekView events array. */
	gint event_num;

	/* The span index within the event. */
	gint span_num;
} EWeekViewEventItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

} EWeekViewEventItemClass;


GtkType  e_week_view_event_item_get_type      (void);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_WEEK_VIEW_EVENT_ITEM_H_ */

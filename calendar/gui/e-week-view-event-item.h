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

#ifndef _E_WEEK_VIEW_EVENT_ITEM_H_
#define _E_WEEK_VIEW_EVENT_ITEM_H_

#include "e-week-view.h"

G_BEGIN_DECLS

/*
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#define E_WEEK_VIEW_EVENT_ITEM(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        e_week_view_event_item_get_type (), EWeekViewEventItem))
#define E_WEEK_VIEW_EVENT_ITEM_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k),\
	e_week_view_event_item_get_type ()))
#define E_IS_WEEK_VIEW_EVENT_ITEM(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o), \
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

GType    e_week_view_event_item_get_type      (void);

G_END_DECLS

#endif /* _E_WEEK_VIEW_EVENT_ITEM_H_ */

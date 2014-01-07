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
 * EWeekViewEventItem - displays the background, times and icons for an event
 * in the week/month views. A separate EText canvas item is used to display &
 * edit the text.
 */

#ifndef E_WEEK_VIEW_EVENT_ITEM_H
#define E_WEEK_VIEW_EVENT_ITEM_H

#include "e-week-view.h"

/* Standard GObject macros */
#define E_TYPE_WEEK_VIEW_EVENT_ITEM \
	(e_week_view_event_item_get_type ())
#define E_WEEK_VIEW_EVENT_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEEK_VIEW_EVENT_ITEM, EWeekViewEventItem))
#define E_WEEK_VIEW_EVENT_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEEK_VIEW_EVENT_ITEM, EWeekViewEventItemClass))
#define E_IS_WEEK_VIEW_EVENT_ITEM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEEK_VIEW_EVENT_ITEM))
#define E_IS_WEEK_VIEW_EVENT_ITEM_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEEK_VIEW_EVENT_ITEM))
#define E_WEEK_VIEW_EVENT_ITEM_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEEK_VIEW_EVENT_ITEM, EWeekViewEventItemClass))

G_BEGIN_DECLS

typedef struct _EWeekViewEventItem EWeekViewEventItem;
typedef struct _EWeekViewEventItemClass EWeekViewEventItemClass;
typedef struct _EWeekViewEventItemPrivate EWeekViewEventItemPrivate;

struct _EWeekViewEventItem {
	GnomeCanvasItem parent;
	EWeekViewEventItemPrivate *priv;
};

struct _EWeekViewEventItemClass {
	GnomeCanvasItemClass parent_class;
};

GType		e_week_view_event_item_get_type	(void);
gint		e_week_view_event_item_get_event_num
						(EWeekViewEventItem *event_item);
void		e_week_view_event_item_set_event_num
						(EWeekViewEventItem *event_item,
						 gint event_num);
gint		e_week_view_event_item_get_span_num
						(EWeekViewEventItem *event_item);
void		e_week_view_event_item_set_span_num
						(EWeekViewEventItem *event_item,
						 gint span_num);

G_END_DECLS

#endif /* E_WEEK_VIEW_EVENT_ITEM_H */

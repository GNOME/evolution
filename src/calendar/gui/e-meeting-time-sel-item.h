/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Damon Chaplin <damon@gtk.org>
 */

/*
 * MeetingTimeSelectorItem - A GnomeCanvasItem which is used for both the main
 * display canvas and the top display (with the dates, times & All Attendees).
 * I didn't make these separate GnomeCanvasItems since they share a lot of
 * code.
 */

#ifndef _E_MEETING_TIME_SELECTOR_ITEM_H_
#define _E_MEETING_TIME_SELECTOR_ITEM_H_

#include "e-meeting-time-sel.h"

G_BEGIN_DECLS

#define E_MEETING_TIME_SELECTOR_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), e_meeting_time_selector_item_get_type (), EMeetingTimeSelectorItem))
#define E_MEETING_TIME_SELECTOR_ITEM_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), e_meeting_time_selector_item_get_type (), EMeetingTimeSelectorItemClass))
#define IS_E_MEETING_TIME_SELECTOR_ITEM(o)         (G_TYPE_CHECK_INSTANCE_TYPE((o), e_meeting_time_selector_item_get_type ()))

typedef struct _EMeetingTimeSelectorItem       EMeetingTimeSelectorItem;
typedef struct _EMeetingTimeSelectorItemClass  EMeetingTimeSelectorItemClass;

struct _EMeetingTimeSelectorItem
{
	GnomeCanvasItem canvas_item;

	/* The parent EMeetingTimeSelector widget. */
	EMeetingTimeSelector *mts;

	/* The normal, resize & busy cursors . */
	GdkCursor *normal_cursor;
	GdkCursor *resize_cursor;
	GdkCursor *busy_cursor;

	/* This remembers the last cursor set on the window. */
	GdkCursor *last_cursor_set;
};

struct _EMeetingTimeSelectorItemClass
{
	GnomeCanvasItemClass parent_class;
};

GType e_meeting_time_selector_item_get_type (void);
void e_meeting_time_selector_item_set_normal_cursor (EMeetingTimeSelectorItem *mts_item);

#endif /* _E_MEETING_TIME_SELECTOR_ITEM_H_ */

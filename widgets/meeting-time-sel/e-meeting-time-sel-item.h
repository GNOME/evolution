/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@gtk.org>
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

/*
 * MeetingTimeSelectorItem - A GnomeCanvasItem which is used for both the main
 * display canvas and the top display (with the dates, times & All Attendees).
 * I didn't make these separate GnomeCanvasItems since they share a lot of
 * code.
 */

#ifndef _E_MEETING_TIME_SELECTOR_ITEM_H_
#define _E_MEETING_TIME_SELECTOR_ITEM_H_

#include "e-meeting-time-sel.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_MEETING_TIME_SELECTOR_ITEM(obj)          (GTK_CHECK_CAST((obj), e_meeting_time_selector_item_get_type (), EMeetingTimeSelectorItem))
#define E_MEETING_TIME_SELECTOR_ITEM_CLASS(k)      (GTK_CHECK_CLASS_CAST ((k), e_meeting_time_selector_item_get_type (), EMeetingTimeSelectorItemClass))
#define IS_E_MEETING_TIME_SELECTOR_ITEM(o)         (GTK_CHECK_TYPE((o), e_meeting_time_selector_item_get_type ()))


typedef struct _EMeetingTimeSelectorItem       EMeetingTimeSelectorItem;
typedef struct _EMeetingTimeSelectorItemClass  EMeetingTimeSelectorItemClass;

struct _EMeetingTimeSelectorItem
{
	GnomeCanvasItem canvas_item;

	/* The parent EMeetingTimeSelector widget. */
	EMeetingTimeSelector *mts;

	/* This GC is used for most of the drawing. The fg/bg colors are
	   changed for each bit. */
	GdkGC *main_gc;

	/* The normal & resize cursors. */
	GdkCursor *normal_cursor;
	GdkCursor *resize_cursor;

	/* This remembers the last cursor set on the window. */
	GdkCursor *last_cursor_set;
};


struct _EMeetingTimeSelectorItemClass
{
	GnomeCanvasItemClass parent_class;
};

GtkType e_meeting_time_selector_item_get_type (void);


#endif /* _E_MEETING_TIME_SELECTOR_ITEM_H_ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
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
#ifndef _E_CALENDAR_TABLE_H_
#define _E_CALENDAR_TABLE_H_

#include <gtk/gtktable.h>
#include "calendar-model.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * ECalendarTable - displays the iCalendar objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

/* These index our colors array. */
typedef enum
{
	E_CALENDAR_TABLE_COLOR_OVERDUE,
	
	E_CALENDAR_TABLE_COLOR_LAST
} ECalendarTableColors;


#define E_CALENDAR_TABLE(obj)          GTK_CHECK_CAST (obj, e_calendar_table_get_type (), ECalendarTable)
#define E_CALENDAR_TABLE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_calendar_table_get_type (), ECalendarTableClass)
#define E_IS_CALENDAR_TABLE(obj)       GTK_CHECK_TYPE (obj, e_calendar_table_get_type ())


typedef struct _ECalendarTable       ECalendarTable;
typedef struct _ECalendarTableClass  ECalendarTableClass;

struct _ECalendarTable
{
	GtkTable table;

	CalendarModel *model;
	
	
	/* Colors for drawing. */
	GdkColor colors[E_CALENDAR_TABLE_COLOR_LAST];
};

struct _ECalendarTableClass
{
	GtkTableClass parent_class;
};


GtkType	   e_calendar_table_get_type		(void);
GtkWidget* e_calendar_table_new			(void);


void	   e_calendar_table_set_cal_client	(ECalendarTable *cal_table,
						 CalClient	*client);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CALENDAR_TABLE_H_ */

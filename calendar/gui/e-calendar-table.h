/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
#ifndef _E_CALENDAR_TABLE_H_
#define _E_CALENDAR_TABLE_H_

#include <gtk/gtktable.h>
#include <gal/e-table/e-table-scrolled.h>
#include <widgets/misc/e-cell-date-edit.h>
#include "e-activity-handler.h"
#include "e-cal-model.h"

G_BEGIN_DECLS

/*
 * ECalendarTable - displays the iCalendar objects in a table (an ETable).
 * Used for calendar events and tasks.
 */


#define E_CALENDAR_TABLE(obj)          GTK_CHECK_CAST (obj, e_calendar_table_get_type (), ECalendarTable)
#define E_CALENDAR_TABLE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_calendar_table_get_type (), ECalendarTableClass)
#define E_IS_CALENDAR_TABLE(obj)       GTK_CHECK_TYPE (obj, e_calendar_table_get_type ())


typedef struct _ECalendarTable       ECalendarTable;
typedef struct _ECalendarTableClass  ECalendarTableClass;


struct _ECalendarTable {
	GtkTable table;

	/* The model that we use */
	ECalModel *model;

	GtkWidget *etable;

	/* The ECell used to view & edit dates. */
	ECellDateEdit *dates_cell;

	/* Fields used for cut/copy/paste */
	icalcomponent *tmp_vcal;

	/* Activity ID for the EActivityHandler (i.e. the status bar).  */
	EActivityHandler *activity_handler;
	guint activity_id;
};

struct _ECalendarTableClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* user_created) (ECalendarTable *cal_table);
};


GtkType	   e_calendar_table_get_type (void);
GtkWidget* e_calendar_table_new	(void);

ECalModel *e_calendar_table_get_model (ECalendarTable *cal_table);

ETable    *e_calendar_table_get_table (ECalendarTable *cal_table);

void       e_calendar_table_open_selected (ECalendarTable *cal_table);
void       e_calendar_table_complete_selected (ECalendarTable *cal_table);
void       e_calendar_table_delete_selected (ECalendarTable *cal_table);

GSList    *e_calendar_table_get_selected (ECalendarTable *cal_table);

/* Clipboard related functions */
void       e_calendar_table_cut_clipboard       (ECalendarTable *cal_table);
void       e_calendar_table_copy_clipboard      (ECalendarTable *cal_table);
void       e_calendar_table_paste_clipboard     (ECalendarTable *cal_table);

/* These load and save the state of the table (headers shown etc.) to/from
   the given file. */
void	   e_calendar_table_load_state		(ECalendarTable *cal_table,
						 gchar		*filename);
void	   e_calendar_table_save_state		(ECalendarTable *cal_table,
						 gchar		*filename);

void       e_calendar_table_set_activity_handler (ECalendarTable *cal_table,
						  EActivityHandler *activity_handler);
void       e_calendar_table_set_status_message (ECalendarTable *cal_table,
						const gchar *message);

G_END_DECLS

#endif /* _E_CALENDAR_TABLE_H_ */

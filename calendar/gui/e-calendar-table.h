/*
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

#ifndef _E_CALENDAR_TABLE_H_
#define _E_CALENDAR_TABLE_H_

#include <gtk/gtk.h>
#include <table/e-table-scrolled.h>
#include <misc/e-cell-date-edit.h>
#include "e-activity-handler.h"
#include "e-cal-model.h"

G_BEGIN_DECLS

/*
 * ECalendarTable - displays the iCalendar objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

#define E_CALENDAR_TABLE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_calendar_table_get_type (), ECalendarTable)
#define E_CALENDAR_TABLE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_calendar_table_get_type (), ECalendarTableClass)
#define E_IS_CALENDAR_TABLE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_calendar_table_get_type ())

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

	/* We should know which calendar has been used to create object, so store it here
	   before emitting "user_created" signal and make it NULL just after the emit. */
	ECal *user_created_cal;
};

struct _ECalendarTableClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* user_created) (ECalendarTable *cal_table);
};

GType		   e_calendar_table_get_type (void);
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
						const gchar *message,
						gint percent);
void	   e_calendar_table_open_task (ECalendarTable *cal_table,
				       ECal *client,
				       icalcomponent *icalcomp,
				       gboolean assign);
ECalModelComponent * e_calendar_table_get_selected_comp (ECalendarTable *cal_table);
void e_calendar_table_hide_completed_tasks (ECalendarTable *table, GList *clients_list, gboolean config_changed);

void e_calendar_table_process_completed_tasks (ECalendarTable *table, GList *clients_list, gboolean config_changed);

gboolean ec_query_tooltip (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, GtkWidget *etable_wgt, ECalModel *model);

G_END_DECLS

#endif /* _E_CALENDAR_TABLE_H_ */

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

#include <shell/e-shell-view.h>
#include <widgets/table/e-table-scrolled.h>
#include <widgets/misc/e-cell-date-edit.h>
#include "e-cal-model.h"

/*
 * ECalendarTable - displays the iCalendar objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

/* Standard GObject macros */
#define E_TYPE_CALENDAR_TABLE \
	(e_calendar_table_get_type ())
#define E_CALENDAR_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CALENDAR_TABLE, ECalendarTable))
#define E_CALENDAR_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CALENDAR_TABLE, ECalendarTableClass))
#define E_IS_CALENDAR_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CALENDAR_TABLE))
#define E_IS_CALENDAR_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CALENDAR_TABLE))
#define E_CALENDAR_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CALENDAR_TABLE, ECalendarTableClass))

G_BEGIN_DECLS

typedef struct _ECalendarTable ECalendarTable;
typedef struct _ECalendarTableClass ECalendarTableClass;
typedef struct _ECalendarTablePrivate ECalendarTablePrivate;

struct _ECalendarTable {
	GtkTable parent;

	GtkWidget *etable;

	/* The ECell used to view & edit dates. */
	ECellDateEdit *dates_cell;

	/* Fields used for cut/copy/paste */
	icalcomponent *tmp_vcal;

	ECalendarTablePrivate *priv;
};

struct _ECalendarTableClass {
	GtkTableClass parent_class;

	/* Signals */
	void	(*open_component)		(ECalendarTable *cal_table,
						 ECalModelComponent *comp_data);
	void	(*popup_event)			(ECalendarTable *cal_table,
						 GdkEvent *event);
	void	(*status_message)		(ECalendarTable *cal_table,
						 const gchar *message,
						 gdouble percent);
	void	(*user_created)			(ECalendarTable *cal_table);
};

GType		e_calendar_table_get_type	(void);
GtkWidget *	e_calendar_table_new		(EShellView *shell_view,
						 ECalModel *model);
ECalModel *	e_calendar_table_get_model	(ECalendarTable *cal_table);
ETable *	e_calendar_table_get_table	(ECalendarTable *cal_table);
EShellView *	e_calendar_table_get_shell_view	(ECalendarTable *cal_table);
void		e_calendar_table_delete_selected(ECalendarTable *cal_table);
GSList *	e_calendar_table_get_selected	(ECalendarTable *cal_table);

/* Clipboard related functions */
void		e_calendar_table_cut_clipboard	(ECalendarTable *cal_table);
void		e_calendar_table_copy_clipboard	(ECalendarTable *cal_table);
void		e_calendar_table_paste_clipboard(ECalendarTable *cal_table);

/* These load and save the state of the table (headers shown etc.) to/from
   the given file. */
void		e_calendar_table_load_state	(ECalendarTable *cal_table,
						 const gchar *filename);
void		e_calendar_table_save_state	(ECalendarTable *cal_table,
						 const gchar *filename);

ECalModelComponent *
		e_calendar_table_get_selected_comp
						(ECalendarTable *cal_table);
void		e_calendar_table_hide_completed_tasks
						(ECalendarTable *table,
						 GList *clients_list,
						 gboolean config_changed);
void		e_calendar_table_process_completed_tasks
						(ECalendarTable *table,
						 GList *clients_list,
						 gboolean config_changed);

gboolean ec_query_tooltip (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, GtkWidget *etable_wgt, ECalModel *model);

G_END_DECLS

#endif /* _E_CALENDAR_TABLE_H_ */

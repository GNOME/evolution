/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Authors: 
 *  Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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
#ifndef _E_CAL_LIST_VIEW_H_
#define _E_CAL_LIST_VIEW_H_

#include <time.h>
#include <gtk/gtktable.h>
#include <gtk/gtktooltips.h>

#include "e-calendar-view.h"
#include "gnome-cal.h"

G_BEGIN_DECLS

/*
 * ECalListView - displays calendar events in an ETable.
 */

#define E_CAL_LIST_VIEW(obj)          GTK_CHECK_CAST (obj, e_cal_list_view_get_type (), ECalListView)
#define E_CAL_LIST_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_cal_list_view_get_type (), ECalListViewClass)
#define E_IS_CAL_LIST_VIEW(obj)       GTK_CHECK_TYPE (obj, e_cal_list_view_get_type ())


typedef struct _ECalListView       ECalListView;
typedef struct _ECalListViewClass  ECalListViewClass;

struct _ECalListView
{
	ECalendarView cal_view;

	/* The main display table */
	ETableScrolled *table_scrolled;

	/* S-expression for query and the query object */
	ECalView *query;

	/* The default category for new events */
	gchar *default_category;

	/* Date editing cell */
	ECellDateEdit *dates_cell;

	/* The last ECalendarViewEvent we returned from e_cal_list_view_get_selected_events(), to be freed */
	ECalendarViewEvent *cursor_event;

	/* Idle handler ID for setting a new ETableModel */
	gint set_table_id;
};

struct _ECalListViewClass
{
	ECalendarViewClass parent_class;
};


GtkType	   e_cal_list_view_get_type		(void);
GtkWidget *e_cal_list_view_construct            (ECalListView *cal_list_view);

GtkWidget *e_cal_list_view_new			(void);
void e_cal_list_view_load_state (ECalListView *cal_list_view, gchar *filename);
void e_cal_list_view_save_state (ECalListView *cal_list_view, gchar *filename);

gboolean   e_cal_list_view_get_range_shown      (ECalListView *cal_list_view, GDate *start_date,
						 gint *days_shown);


G_END_DECLS

#endif /* _E_CAL_LIST_VIEW_H_ */

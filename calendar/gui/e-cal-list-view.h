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
#include <gal/widgets/e-popup-menu.h>

#include "e-calendar-view.h"
#include "gnome-cal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

	/* The path to the table's state file */
	gchar *table_state_path;

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
GtkWidget *e_cal_list_view_construct            (ECalListView *cal_list_view, const gchar *table_state_path);

GtkWidget *e_cal_list_view_new			(const gchar *table_state_path);

void       e_cal_list_view_set_query		(ECalListView *cal_list_view, const gchar *sexp);
void       e_cal_list_view_set_default_category	(ECalListView *cal_list_view, const gchar *category);
gboolean   e_cal_list_view_get_range_shown      (ECalListView *cal_list_view, GDate *start_date,
						 gint *days_shown);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CAL_LIST_VIEW_H_ */

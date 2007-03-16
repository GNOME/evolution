/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* EDateTimeList - list of calendar dates/times with GtkTreeModel interface.
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:  Hans Petter Jansson  <hpj@ximian.com>
 */

#ifndef E_DATE_TIME_LIST_H
#define E_DATE_TIME_LIST_H

#include <gtk/gtktreemodel.h>
#include <libecal/e-cal-component.h>

G_BEGIN_DECLS

#define E_TYPE_DATE_TIME_LIST            (e_date_time_list_get_type ())
#define E_DATE_TIME_LIST(obj)	         (GTK_CHECK_CAST ((obj), E_TYPE_DATE_TIME_LIST, EDateTimeList))
#define E_DATE_TIME_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_DATE_TIME_LIST, EDateTimeListClass))
#define E_IS_DATE_TIME_LIST(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_DATE_TIME_LIST))
#define E_IS_DATE_TIME_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_DATE_TIME_LIST))
#define E_DATE_TIME_LIST_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_DATE_TIME_LIST, EDateTimeListClass))

typedef struct _EDateTimeList       EDateTimeList;
typedef struct _EDateTimeListClass  EDateTimeListClass;

typedef enum
{
	E_DATE_TIME_LIST_COLUMN_DESCRIPTION,

	E_DATE_TIME_LIST_NUM_COLUMNS
}
EDateTimeListColumnType;

struct _EDateTimeList
{
	GObject  parent;

	/* Private */

	gint     stamp;
	GList   *list;

	guint    columns_dirty : 1;
};

struct _EDateTimeListClass
{
	GObjectClass parent_class;
};

GtkType                     e_date_time_list_get_type         (void);
EDateTimeList              *e_date_time_list_new              (void);

const ECalComponentDateTime *e_date_time_list_get_date_time    (EDateTimeList *date_time_list,
							       GtkTreeIter *iter);
void                        e_date_time_list_set_date_time    (EDateTimeList *date_time_list,
							       GtkTreeIter *iter,
							       const ECalComponentDateTime *datetime);
void                        e_date_time_list_append           (EDateTimeList *date_time_list,
							       GtkTreeIter *iter,
							       const ECalComponentDateTime *datetime);
void                        e_date_time_list_remove           (EDateTimeList *date_time_list,
							       GtkTreeIter *iter);
void                        e_date_time_list_clear            (EDateTimeList *date_time_list);

G_END_DECLS

#endif  /* E_DATE_TIME_LIST_H */

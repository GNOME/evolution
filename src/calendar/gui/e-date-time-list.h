/*
 *
 * EDateTimeList - list of calendar dates/times with GtkTreeModel interface.
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
 *		Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_DATE_TIME_LIST_H
#define E_DATE_TIME_LIST_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

/* Standard GObject macros */
#define E_TYPE_DATE_TIME_LIST \
	(e_date_time_list_get_type ())
#define E_DATE_TIME_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_DATE_TIME_LIST, EDateTimeList))
#define E_DATE_TIME_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_DATE_TIME_LIST, EDateTimeListClass))
#define E_IS_DATE_TIME_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_DATE_TIME_LIST))
#define E_IS_DATE_TIME_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_DATE_TIME_LIST))
#define E_DATE_TIME_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_DATE_TIME_LIST, EDateTimeListClass))

G_BEGIN_DECLS

typedef struct _EDateTimeList EDateTimeList;
typedef struct _EDateTimeListClass EDateTimeListClass;
typedef struct _EDateTimeListPrivate EDateTimeListPrivate;

typedef enum {
	E_DATE_TIME_LIST_COLUMN_DESCRIPTION,
	E_DATE_TIME_LIST_NUM_COLUMNS
} EDateTimeListColumnType;

struct _EDateTimeList {
	GObject parent;

	EDateTimeListPrivate *priv;
};

struct _EDateTimeListClass {
	GObjectClass parent_class;
};

GType		e_date_time_list_get_type	(void);
EDateTimeList *	e_date_time_list_new		(void);
ICalTime *	e_date_time_list_get_date_time	(EDateTimeList *date_time_list,
						 GtkTreeIter *iter);
void		e_date_time_list_set_date_time	(EDateTimeList *date_time_list,
						 GtkTreeIter *iter,
						 const ICalTime *itt);
gboolean	e_date_time_list_get_use_24_hour_format
						(EDateTimeList *date_time_list);
void		e_date_time_list_set_use_24_hour_format
						(EDateTimeList *date_time_list,
						 gboolean use_24_hour_format);
ICalTimezone *	e_date_time_list_get_timezone	(EDateTimeList *date_time_list);
void		e_date_time_list_set_timezone	(EDateTimeList *date_time_list,
						 const ICalTimezone *zone);
void		e_date_time_list_append		(EDateTimeList *date_time_list,
						 GtkTreeIter *iter,
						 const ICalTime *itt);
void		e_date_time_list_remove		(EDateTimeList *date_time_list,
						 GtkTreeIter *iter);
void		e_date_time_list_clear		(EDateTimeList *date_time_list);

G_END_DECLS

#endif  /* E_DATE_TIME_LIST_H */

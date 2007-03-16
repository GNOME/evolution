/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* EAlarmList - list of calendar alarms with GtkTreeModel interface.
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

#ifndef E_ALARM_LIST_H
#define E_ALARM_LIST_H

#include <gtk/gtktreemodel.h>
#include <libecal/e-cal-component.h>

G_BEGIN_DECLS

#define E_TYPE_ALARM_LIST            (e_alarm_list_get_type ())
#define E_ALARM_LIST(obj)	         (GTK_CHECK_CAST ((obj), E_TYPE_ALARM_LIST, EAlarmList))
#define E_ALARM_LIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_ALARM_LIST, EAlarmListClass))
#define E_IS_ALARM_LIST(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_ALARM_LIST))
#define E_IS_ALARM_LIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_ALARM_LIST))
#define E_ALARM_LIST_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_ALARM_LIST, EAlarmListClass))

typedef struct _EAlarmList       EAlarmList;
typedef struct _EAlarmListClass  EAlarmListClass;

typedef enum
{
	E_ALARM_LIST_COLUMN_DESCRIPTION,

	E_ALARM_LIST_NUM_COLUMNS
}
EAlarmListColumnType;

struct _EAlarmList
{
	GObject  parent;

	/* Private */

	gint     stamp;
	GList   *list;

	guint    columns_dirty : 1;
};

struct _EAlarmListClass
{
	GObjectClass parent_class;
};

GtkType                  e_alarm_list_get_type  (void);
EAlarmList              *e_alarm_list_new       (void);

const ECalComponentAlarm *e_alarm_list_get_alarm (EAlarmList *alarm_list, GtkTreeIter *iter);
void                     e_alarm_list_set_alarm (EAlarmList *alarm_list, GtkTreeIter *iter,
						 const ECalComponentAlarm *datetime);
void                     e_alarm_list_append    (EAlarmList *alarm_list, GtkTreeIter *iter,
						 const ECalComponentAlarm *datetime);
void                     e_alarm_list_remove    (EAlarmList *alarm_list, GtkTreeIter *iter);
void                     e_alarm_list_clear     (EAlarmList *alarm_list);

G_END_DECLS

#endif  /* E_ALARM_LIST_H */

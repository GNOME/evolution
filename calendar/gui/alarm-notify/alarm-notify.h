/*
 *
 * Evolution calendar - Alarm notification service object
 *
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef ALARM_NOTIFY_H
#define ALARM_NOTIFY_H

#include <bonobo/bonobo-object.h>
#include <libedataserver/e-msgport.h>
#include "evolution-calendar.h"


#define TYPE_ALARM_NOTIFY            (alarm_notify_get_type ())
#define ALARM_NOTIFY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ALARM_NOTIFY, AlarmNotify))
#define ALARM_NOTIFY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_ALARM_NOTIFY,		\
				      AlarmNotifyClass))
#define IS_ALARM_NOTIFY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ALARM_NOTIFY))
#define IS_ALARM_NOTIFY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_ALARM_NOTIFY))

typedef struct _AlarmNotify AlarmNotify;
typedef struct _AlarmNotifyClass AlarmNotifyClass;

typedef struct _AlarmNotifyPrivate AlarmNotifyPrivate;

struct _AlarmNotify {
	BonoboObject object;

	/* Private data */
	AlarmNotifyPrivate *priv;
};

struct _AlarmNotifyClass {
	BonoboObjectClass parent_class;
	POA_GNOME_Evolution_Calendar_AlarmNotify__epv epv;
};

GType alarm_notify_get_type (void);

AlarmNotify *alarm_notify_new (void);

void alarm_notify_add_calendar (AlarmNotify *an, ECalSourceType source_type, ESource *source, gboolean load_afterwards);
void alarm_notify_remove_calendar (AlarmNotify *an, ECalSourceType source_type, const gchar *str_uri);

ESourceList *alarm_notify_get_selected_calendars (AlarmNotify *);



#endif

/* Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ALARM_NOTIFY_H
#define ALARM_NOTIFY_H

#include <bonobo/bonobo-object.h>
#include "evolution-calendar.h"



#define TYPE_ALARM_NOTIFY            (alarm_notify_get_type ())
#define ALARM_NOTIFY(obj)            (GTK_CHECK_CAST ((obj), TYPE_ALARM_NOTIFY, AlarmNotify))
#define ALARM_NOTIFY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_ALARM_NOTIFY,		\
				      AlarmNotifyClass))
#define IS_ALARM_NOTIFY(obj)         (GTK_CHECK_TYPE ((obj), TYPE_ALARM_NOTIFY))
#define IS_ALARM_NOTIFY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_ALARM_NOTIFY))

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
};

GtkType alarm_notify_get_type (void);

AlarmNotify *alarm_notify_construct (AlarmNotify *an,
				     GNOME_Evolution_Calendar_AlarmNotify corba_an);

GNOME_Evolution_Calendar_AlarmNotify alarm_notify_corba_object_create (BonoboObject *object);
POA_GNOME_Evolution_Calendar_AlarmNotify__epv *alarm_notify_get_epv (void);

AlarmNotify *alarm_notify_new (void);




#endif

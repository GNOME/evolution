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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "alarm-notify.h"



/* Private part of the AlarmNotify structure */
struct _AlarmNotifyPrivate {
	/* FIXME */
};



static void alarm_notify_class_init (AlarmNotifyClass *class);
static void alarm_notify_init (AlarmNotify *an);
static void alarm_notify_destroy (GtkObject *object);

static POA_GNOME_Evolution_Calendar_AlarmListener__vepv alarm_listener_vepv;

static BonoboObjectClass *parent_class;



/**
 * alarm_notify_get_type:
 * 
 * Registers the #AlarmNotify class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #AlarmNotify class.
 **/
GtkType
alarm_notify_get_type (void)
{
	static GtkType alarm_notify_type = 0;

	if (!alarm_notify_type) {
		static const GtkTypeInfo alarm_notify_info = {
			"AlarmNotify",
			sizeof (AlarmNotify),
			sizeof (AlarmNotifyClass),
			(GtkClassInitFunc) alarm_notify_class_init,
			(GtkObjectInitFunc) alarm_notify_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		alarm_notify_type = gtk_type_unique (BONOBO_OBJECT_TYPE, &alarm_notify_info);
	}

	return alarm_notify_type;
}

/* CORBA class initialization function for the alarm notify service */
static void
init_alarm_notify_corba_class (void)
{
	alarm_notify_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	alarm_notify_vepv.GNOME_Evolution_Calendar_AlarmNotify_epv = alarm_notify_get_epv ();
}

/* Class initialization function for the alarm notify service */
static void
alarm_notify_class_init (AlarmNotifyClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_OBJECT_TYPE);

	object_class->destroy = alarm_notify_destroy;

	init_alarm_notify_corba_class ();
}

/* Object initialization function for the alarm notify system */
static void
alarm_notify_init (AlarmNotify *an)
{
	AlarmNotifyPrivate *priv;

	priv = g_new0 (AlarmNotifyPrivate, 1);
	an->priv = priv;

	/* FIXME */
}

/* Destroy handler for the alarm notify system */
static void
alarm_notify_destroy (GtkObject *object)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (object));

	an = ALARM_NOTIFY (object);
	priv = an->priv;

	/* FIXME */

	g_free (priv);
	an->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/**
 * alarm_notify_get_epv:
 * 
 * Creates an EPV for the AlarmNotify CORBA class.
 * 
 * Return value: A newly-allocated EPV.
 **/
POA_GNOME_Evolution_Calendar_AlarmNotify__epv *
alarm_notify_get_epv (void)
{
	POA_GNOME_Evolution_Calendar_AlarmNotify__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Calendar_AlarmNotify__epv, 1);
	epv->addCalendar = AlarmNotify_addCalendar;
	epv->removeCalendar = AlarmNotify_removeCalendar;
	epv->die = AlarmNotify_die;
	return epv;
}

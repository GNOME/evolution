/* Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
	/* Mapping from GnomeVFSURIs to loaded clients */
	GHashTable *uri_client_hash;
};



static void alarm_notify_class_init (AlarmNotifyClass *class);
static void alarm_notify_init (AlarmNotify *an);
static void alarm_notify_destroy (GtkObject *object);

static POA_GNOME_Evolution_Calendar_AlarmNotify__vepv alarm_notify_vepv;

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

/* AlarmNotify::addCalendar method */
static void
AlarmNotify_addCalendar (PortableServer_Servant servant,
			 const CORBA_char *str_uri,
			 CORBA_Environment *ev)
{
	AlarmNotify *an;
	GnomeVFSURI *uri;
	CalClient *client;

	an = ALARM_NOTIFY (bonobo_object_from_servant (servant));

	uri = gnome_vfs_uri_new (str_uri);
	if (!uri) {
	}
}

/* AlarmNotify::removeCalendar method */
static void
AlarmNotify_removeCalendar (PortableServer_Servant servant,
			    const CORBA_char *uri,
			    CORBA_Environment *ev)
{
	AlarmNotify *an;

	an = ALARM_NOTIFY (bonobo_object_from_servant (servant));

	/* FIXME */
}

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



/**
 * alarm_notify_construct:
 * @an: An alarm notification service object.
 * @corba_an: CORBA object for the alarm notification service.
 * 
 * Constructs an alarm notification service object by binding the corresponding
 * CORBA object to it.
 * 
 * Return value: the same object as the @an argument.
 **/
AlarmNotify *
alarm_notify_construct (AlarmNotify *an,
			GNOME_Evolution_Calendar_AlarmNotify corba_an)
{
	g_return_val_if_fail (an != NULL, NULL);
	g_return_val_if_fail (IS_ALARM_NOTIFY (an), NULL);

	/* FIXME: add_interface the property bag here */

	bonobo_object_construct (BONOBO_OBJECT (an), corba_an);
	return an;
}

/**
 * alarm_notify_corba_object_create:
 * @object: #BonoboObject that will wrap the CORBA object.
 * 
 * Creates and activates the CORBA object that is wrapped by the specified alarm
 * notification service @object.
 * 
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Evolution_Calendar_AlarmNotify
alarm_notify_corba_object_create (BonoboObject *object)
{
	POA_GNOME_Evolution_Calendar_AlarmNotify *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_ALARM_NOTIFY (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Evolution_Calendar_AlarmNotify *) g_new (BonoboObjectServant, 1);
	servant->vepv = &alarm_notify_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Calendar_AlarmNotify__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Evolution_Calendar_AlarmNotify) bonobo_object_activate_servant (
		object, servant);
}

/**
 * alarm_notify_new:
 * 
 * Creates a new #AlarmNotify object.
 * 
 * Return value: A newly-created #AlarmNotify, or NULL if its corresponding
 * CORBA object could not be created.
 **/
AlarmNotify *
alarm_notify_new (void)
{
	AlarmNotify *an;
	GNOME_Evolution_Calendar_AlarmNotify corba_an;
	CORBA_Environment ev;
	gboolean result;

	an = gtk_type_new (TYPE_ALARM_NOTIFY);

	corba_an = alarm_notify_corba_object_create (BONOBO_OBJECT (an));

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (corba_an, &ev);

	if (ev._major != CORBA_NO_EXCEPTION || result) {
		g_message ("alarm_notify_new(): could not create the CORBA alarm notify service");
		bonobo_object_unref (BONOBO_OBJECT (an));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	return alarm_notify_construct (an, corba_an);
}

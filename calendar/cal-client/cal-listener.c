/* GNOME calendar listener
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

#include <config.h>
#include "cal-listener.h"



/* Private part of the CalListener structure */
typedef struct {
	/* The calendar this listener refers to */
	GNOME_Calendar_Cal cal;
} CalListenerPrivate;



/* Signal IDs */
enum {
	CAL_LOADED,
	OBJ_ADDED,
	OBJ_REMOVED,
	OBJ_CHANGED,
	LAST_SIGNAL
};

static void cal_listener_class_init (CalListenerClass *class);
static void cal_listener_init (CalListener *listener);
static void cal_listener_destroy (GtkObject *object);

static POA_GNOME_Calendar_Listener__vepv cal_listener_vepv;

static guint cal_listener_signals[LAST_SIGNAL];

static GnomeObjectClass *parent_class;



/**
 * cal_listener_get_type:
 * @void:
 *
 * Registers the #CalListener class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalListener class.
 **/
GtkType
cal_listener_get_type (void)
{
	static GtkType cal_listener_type = 0;

	if (!cal_listener_type) {
		static const GtkTypeInfo cal_listener_info = {
			"CalListener",
			sizeof (CalListener),
			sizeof (CalListenerClass),
			(GtkClassInitFunc) cal_listener_class_init,
			(GtkObjectInitFunc) cal_listener_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_listener_type = gtk_type_unique (gnome_object_get_type (), &cal_listener_info);
	}

	return cal_listener_type;
}

/* CORBA class initialization function for the calendar listener */
static void
init_cal_listener_corba_class (void)
{
	cal_listener_vepv.GNOME_Unknown_epv = gnome_object_get_epv ();
	cal_listener_vepv.GNOME_Calendar_Listener_epv = cal_listener_get_epv ();
}

/* Class initialization function for the calendar listener */
static void
cal_listener_class_init (CalListenerClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_object_get_type ());

	cal_listener_signals[CAL_LOADED] =
		gtk_signal_new ("cal_loaded",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalListenerClass, cal_loaded),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_POINTER);
	cal_listener_signals[OBJ_ADDED] =
		gtk_signal_new ("obj_added",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalListenerClass, obj_added),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	cal_listener_signals[OBJ_REMOVED] =
		gtk_signal_new ("obj_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalListenerClass, obj_removed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	cal_listener_signals[OBJ_CHANGED] =
		gtk_signal_new ("obj_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalListenerClass, obj_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, cal_listener_signals, LAST_SIGNAL);

	object_class->destroy = cal_listener_destroy;

	init_cal_listener_corba_class ();
}

/* Object initialization function for the calendar listener */
static void
cal_listener_init (CalListener *listener)
{
	CalListenerPrivate *priv;

	priv = g_new0 (CalListenerPrivate, 1);
	listener->priv = priv;

	priv->cal = CORBA_OBJECT_NIL;
}

/* Returns whether a CORBA object is nil */
static gboolean
corba_object_is_nil (CORBA_Object object)
{
	CORBA_Environment ev;
	gboolean retval;

	CORBA_exception_init (&ev);
	retval = CORBA_Object_is_nil (object, &ev);
	CORBA_exception_free (&ev);

	return retval;
}

/* Destroy handler for the calendar listener */
static void
cal_listener_destroy (GtkObject *object)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_LISTENER (object));

	listener = CAL_LISTENER (object);
	priv = listener->priv;

	CORBA_exception_init (&ev);

	if (!CORBA_Object_is_nil (priv->cal, &ev)) {
		GNOME_Unknown_unref (priv->cal, &ev);
		CORBA_Object_release (priv->cal, &ev);
	}

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/* Listener::cal_loaded method */
static void
Listener_cal_loaded (PortableServer_Servant servant,
		     GNOME_Calendar_Cal cal,
		     GNOME_Calendar_CalObj calobj,
		     CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (gnome_object_from_servant (servant));
	priv = listener->priv;

	priv->cal = CORBA_Object_duplicate (cal, ev);
	gtk_signal_emit (GTK_OBJECT (listener), cal_listener_signals[CAL_LOADED],
			 cal, calobj);
}

/* Listener::obj_added method */
static void
Listener_obj_added (PortableServer_Servant servant,
		    GNOME_Calendar_CalObj calobj,
		    CORBA_Environment *ev)
{
	CalListener *listener;

	listener = CAL_LISTENER (gnome_object_from_servant (servant));
	gtk_signal_emit (GTK_OBJECT (listener), cal_listener_signals[OBJ_ADDED],
			 calobj);
}

/* Listener::obj_removed method */
static void
Listener_obj_removed (PortableServer_Servant servant,
		      GNOME_Calendar_CalObjUID uid,
		      CORBA_Environment *ev)
{
	CalListener *listener;

	listener = CAL_LISTENER (gnome_object_from_servant (servant));
	gtk_signal_emit (GTK_OBJECT (listener), cal_listener_signals[OBJ_REMOVED],
			 uid);
}

/* Listener::obj_changed method */
static void
Listener_obj_changed (PortableServer_Servant servant,
		      GNOME_Calendar_CalObj calobj,
		      CORBA_Environment *ev)
{
	CalListener *listener;

	listener = CAL_LISTENER (gnome_object_from_servant (servant));
	gtk_signal_emit (GTK_OBJECT (listener), cal_listener_signals[OBJ_CHANGED],
			 calobj);
}

/**
 * cal_listener_get_epv:
 * @void:
 *
 * Creates an EPV for the Listener CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_GNOME_Calendar_Listener__epv *
cal_listener_get_epv (void)
{
	POA_GNOME_Calendar_Listener__epv *epv;

	epv = g_new0 (POA_GNOME_Calendar_Listener__epv, 1);
	epv->cal_loaded = Listener_cal_loaded;
	epv->obj_added = Listener_obj_added;
	epv->obj_removed = Listener_obj_removed;
	epv->obj_changed = Listener_obj_changed;

	return epv;
}



/* Returns whether a CORBA object is nil */
static gboolean
corba_object_is_nil (CORBA_Object object)
{
	CORBA_Environment ev;
	gboolean retval;

	CORBA_exception_init (&ev);
	retval = CORBA_Object_is_nil (object, &ev);
	CORBA_exception_free (&ev);

	return retval;
}

/**
 * cal_listener_construct:
 * @listener: A calendar listener.
 * @corba_listener: CORBA object for the calendar listener.
 *
 * Constructs a calendar listener by binding the corresponding CORBA object to
 * it.
 *
 * Return value: the same object as the @listener argument.
 **/
CalListener *
cal_listener_construct (CalListener *listener, GNOME_Calendar_Listener corba_listener)
{
	g_return_val_if_fail (listener != NULL, NULL);
	g_return_val_if_fail (IS_CAL_LISTENER (listener), NULL);
	g_return_val_if_fail (!corba_object_is_nil (corba_listener), NULL);

	gnome_object_construct (GNOME_OBJECT (listener), corba_listener);
	return listener;
}

/**
 * cal_listener_corba_object_create:
 * @object: #GnomeObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar listener @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Calendar_Listener
cal_listener_corba_object_create (GnomeObject *object)
{
	POA_GNOME_Calendar_Listener *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL_LISTENER (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Calendar_Listener *) g_new0 (GnomeObjectServant, 1);
	servant->vepv = &cal_listener_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Calendar_Listener__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Calendar_Listener) gnome_object_activate_servant (object, servant);
}

/**
 * cal_listener_new:
 * @void:
 *
 * Creates a new #CalListener object.
 *
 * Return value: A newly-created #CalListener, or NULL if its corresponding
 * CORBA object could not be created.
 **/
CalListener *
cal_listener_new (void)
{
	CalListener *listener;
	GNOME_Calendar_Listener corba_listener;

	listener = gtk_type_new (CAL_LISTENER_TYPE);
	corba_listener = cal_listener_corba_object_create (GNOME_OBJECT (listener));
	if (corba_object_is_nil (corba_listener)) {
		gtk_object_destroy (listener);
		return NULL;
	}

	return cal_listener_construct (listener, corba_listener);
}

/**
 * cal_listener_get_calendar:
 * @listener: A calendar listener.
 * 
 * Queries the calendar that a listener is watching.
 * 
 * Return value: The calendar that the listener is watching.
 **/
GNOME_Calendar_Cal
cal_listener_get_calendar (CalListener *listener)
{
	CalListenerPrivate *priv;

	g_return_val_if_fail (listener != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL_LISTENER (listener), CORBA_OBJECT_NIL);

	priv = listener->priv;
	return priv->cal;
}

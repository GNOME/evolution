/* Evolution calendar listener
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

#include <config.h>
#include <gtk/gtksignal.h>
#include "cal-listener.h"



/* Private part of the CalListener structure */
struct _CalListenerPrivate {
	/* The calendar this listener refers to */
	GNOME_Evolution_Calendar_Cal cal;

	/* Notification functions and their closure data */
	CalListenerCalOpenedFn cal_opened_fn;
	CalListenerObjUpdatedFn obj_updated_fn;
	CalListenerObjRemovedFn obj_removed_fn;
	gpointer fn_data;
};



static void cal_listener_class_init (CalListenerClass *class);
static void cal_listener_init (CalListener *listener);
static void cal_listener_destroy (GtkObject *object);

static POA_GNOME_Evolution_Calendar_Listener__vepv cal_listener_vepv;

static BonoboObjectClass *parent_class;



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

		cal_listener_type = gtk_type_unique (bonobo_object_get_type (), &cal_listener_info);
	}

	return cal_listener_type;
}

/* CORBA class initialization function for the calendar listener */
static void
init_cal_listener_corba_class (void)
{
	cal_listener_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	cal_listener_vepv.GNOME_Evolution_Calendar_Listener_epv = cal_listener_get_epv ();
}

/* Class initialization function for the calendar listener */
static void
cal_listener_class_init (CalListenerClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (bonobo_object_get_type ());

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
	priv->cal_opened_fn = NULL;
	priv->obj_updated_fn = NULL;
	priv->obj_removed_fn = NULL;
}

/* Destroy handler for the calendar listener */
static void
cal_listener_destroy (GtkObject *object)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	CORBA_Environment ev;
	gboolean result;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_LISTENER (object));

	listener = CAL_LISTENER (object);
	priv = listener->priv;

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (priv->cal, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_listener_destroy(): could not see if the calendar was NIL");
	else if (!result) {
		CORBA_exception_free (&ev);

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->cal, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("cal_listener_destroy(): could not release the calendar");

		priv->cal = CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);

	g_free (priv);
	listener->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/* Listener::notifyCalOpened method */
static void
Listener_notifyCalOpened (PortableServer_Servant servant,
			  GNOME_Evolution_Calendar_Listener_OpenStatus status,
			  GNOME_Evolution_Calendar_Cal cal,
			  CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;
	CORBA_Environment aev;
	GNOME_Evolution_Calendar_Cal cal_copy;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	if (priv->cal != CORBA_OBJECT_NIL) {
		g_message ("Listener_notifyCalOpened(): calendar was already open!");
		return;
	}

	CORBA_exception_init (&aev);
	cal_copy = CORBA_Object_duplicate (cal, &aev);

	if (aev._major != CORBA_NO_EXCEPTION) {
		g_message ("Listener_notifyCalOpened(): could not duplicate the calendar");
		CORBA_exception_free (&aev);
		return;
	}
	CORBA_exception_free (&aev);

	priv->cal = cal_copy;

	g_assert (priv->cal_opened_fn != NULL);
	(* priv->cal_opened_fn) (listener, status, cal, priv->fn_data);
}

/* Listener::notifyObjUpdated method */
static void
Listener_notifyObjUpdated (PortableServer_Servant servant,
			   GNOME_Evolution_Calendar_CalObjUID uid,
			   CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	g_assert (priv->obj_updated_fn != NULL);
	(* priv->obj_updated_fn) (listener, uid, priv->fn_data);
}

/* Listener::notifyObjRemoved method */
static void
Listener_notifyObjRemoved (PortableServer_Servant servant,
			   GNOME_Evolution_Calendar_CalObjUID uid,
			   CORBA_Environment *ev)
{
	CalListener *listener;
	CalListenerPrivate *priv;

	listener = CAL_LISTENER (bonobo_object_from_servant (servant));
	priv = listener->priv;

	g_assert (priv->obj_removed_fn != NULL);
	(* priv->obj_removed_fn) (listener, uid, priv->fn_data);
}

/**
 * cal_listener_get_epv:
 * @void:
 *
 * Creates an EPV for the Listener CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_GNOME_Evolution_Calendar_Listener__epv *
cal_listener_get_epv (void)
{
	POA_GNOME_Evolution_Calendar_Listener__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Calendar_Listener__epv, 1);
	epv->notifyCalOpened  = Listener_notifyCalOpened;
	epv->notifyObjUpdated = Listener_notifyObjUpdated;
	epv->notifyObjRemoved = Listener_notifyObjRemoved;
	return epv;
}



/**
 * cal_listener_construct:
 * @listener: A calendar listener.
 * @corba_listener: CORBA object for the calendar listener.
 * @cal_opened_fn: Function that will be called to notify that a calendar was
 * opened.
 * @obj_updated_fn: Function that will be called to notify that an object in the
 * calendar was updated.
 * @obj_removed_fn: Function that will be called to notify that an object in the
 * calendar was removed.
 * @fn_data: Closure data pointer that will be passed to the notification
 * functions.
 *
 * Constructs a calendar listener by binding the corresponding CORBA object to
 * it.
 *
 * Return value: the same object as the @listener argument.
 **/
CalListener *
cal_listener_construct (CalListener *listener,
			GNOME_Evolution_Calendar_Listener corba_listener,
			CalListenerCalOpenedFn cal_opened_fn,
			CalListenerObjUpdatedFn obj_updated_fn,
			CalListenerObjRemovedFn obj_removed_fn,
			gpointer fn_data)
{
	CalListenerPrivate *priv;

	g_return_val_if_fail (listener != NULL, NULL);
	g_return_val_if_fail (IS_CAL_LISTENER (listener), NULL);
	g_return_val_if_fail (cal_opened_fn != NULL, NULL);
	g_return_val_if_fail (obj_updated_fn != NULL, NULL);
	g_return_val_if_fail (obj_removed_fn != NULL, NULL);

	priv = listener->priv;

	priv->cal_opened_fn = cal_opened_fn;
	priv->obj_updated_fn = obj_updated_fn;
	priv->obj_removed_fn = obj_removed_fn;
	priv->fn_data = fn_data;

	bonobo_object_construct (BONOBO_OBJECT (listener), corba_listener);
	return listener;
}

/**
 * cal_listener_corba_object_create:
 * @object: #BonoboObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar listener @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
GNOME_Evolution_Calendar_Listener
cal_listener_corba_object_create (BonoboObject *object)
{
	POA_GNOME_Evolution_Calendar_Listener *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL_LISTENER (object), CORBA_OBJECT_NIL);

	servant = (POA_GNOME_Evolution_Calendar_Listener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cal_listener_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_Calendar_Listener__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_Evolution_Calendar_Listener) bonobo_object_activate_servant (object, servant);
}

/**
 * cal_listener_new:
 * @cal_opened_fn: Function that will be called to notify that a calendar was
 * opened.
 * @obj_updated_fn: Function that will be called to notify that an object in the
 * calendar was updated.
 * @obj_removed_fn: Function that will be called to notify that an object in the
 * calendar was removed.
 * @fn_data: Closure data pointer that will be passed to the notification
 * functions.
 *
 * Creates a new #CalListener object.
 *
 * Return value: A newly-created #CalListener, or NULL if its corresponding
 * CORBA object could not be created.
 **/
CalListener *
cal_listener_new (CalListenerCalOpenedFn cal_opened_fn,
		  CalListenerObjUpdatedFn obj_updated_fn,
		  CalListenerObjRemovedFn obj_removed_fn,
		  gpointer fn_data)
{
	CalListener *listener;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_Listener corba_listener;
	gboolean result;

	g_return_val_if_fail (cal_opened_fn != NULL, NULL);
	g_return_val_if_fail (obj_updated_fn != NULL, NULL);
	g_return_val_if_fail (obj_removed_fn != NULL, NULL);

	listener = gtk_type_new (CAL_LISTENER_TYPE);

	corba_listener = cal_listener_corba_object_create (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (corba_listener, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION || result) {
		g_message ("cal_listener_new(): could not create the CORBA listener");
		bonobo_object_unref (BONOBO_OBJECT (listener));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	return cal_listener_construct (listener,
				       corba_listener,
				       cal_opened_fn,
				       obj_updated_fn,
				       obj_removed_fn,
				       fn_data);
}

/* Evolution calendar client interface object
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
#include "cal.h"
#include "cal-backend.h"



/* Private part of the Cal structure */
typedef struct {
	/* Our backend */
	CalBackend *backend;

	/* Listener on the client we notify */
	Evolution_Calendar_Listener listener;
} CalPrivate;



static void cal_class_init (CalClass *class);
static void cal_init (Cal *cal);
static void cal_destroy (GtkObject *object);

static POA_Evolution_Calendar_Cal__vepv cal_vepv;

static BonoboObjectClass *parent_class;



/**
 * cal_get_type:
 * @void:
 *
 * Registers the #Cal class if necessary, and returns the type ID associated to
 * it.
 *
 * Return value: The type ID of the #Cal class.
 **/
GtkType
cal_get_type (void)
{
	static GtkType cal_type = 0;

	if (!cal_type) {
		static const GtkTypeInfo cal_info = {
			"Cal",
			sizeof (Cal),
			sizeof (CalClass),
			(GtkClassInitFunc) cal_class_init,
			(GtkObjectInitFunc) cal_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_type = gtk_type_unique (BONOBO_OBJECT_TYPE, &cal_info);
	}

	return cal_type;
}

/* CORBA class initialzation function for the calendar */
static void
init_cal_corba_class (void)
{
	cal_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	cal_vepv.Evolution_Calendar_Cal_epv = cal_get_epv ();
}

/* Class initialization function for the calendar */
static void
cal_class_init (CalClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_OBJECT_TYPE);

	object_class->destroy = cal_destroy;

	init_cal_corba_class ();
}

/* Object initialization function for the calendar */
static void
cal_init (Cal *cal)
{
	CalPrivate *priv;

	priv = g_new0 (CalPrivate, 1);
	cal->priv = priv;

	priv->listener = CORBA_OBJECT_NIL;
}

/* Destroy handler for the calendar */
static void
cal_destroy (GtkObject *object)
{
	Cal *cal;
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL (object));

	cal = CAL (object);
	priv = cal->priv;

	CORBA_exception_init (&ev);
	CORBA_Object_release (priv->listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("cal_destroy(): could not release the listener");

	CORBA_exception_free (&ev);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/* Cal::get_uri method */
static CORBA_char *
Cal_get_uri (PortableServer_Servant servant,
	     CORBA_Environment *ev)
{
	Cal *cal;
	CalPrivate *priv;
	GnomeVFSURI *uri;
	char *str_uri;
	CORBA_char *str_uri_copy;

	cal = CAL (bonobo_object_from_servant (servant));
	priv = cal->priv;

	uri = cal_backend_get_uri (priv->backend);
	str_uri = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	str_uri_copy = CORBA_string_dup (str_uri);
	g_free (str_uri);

	return str_uri_copy;

}

/**
 * cal_get_epv:
 * @void:
 *
 * Creates an EPV for the Cal CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_Evolution_Calendar_Cal__epv *
cal_get_epv (void)
{
	POA_Evolution_Calendar_Cal__epv *epv;

	epv = g_new0 (POA_Evolution_Calendar_Cal__epv, 1);
	epv->_get_uri = Cal_get_uri;

	return epv;
}



/**
 * cal_construct:
 * @cal: A calendar client interface.
 * @corba_cal: CORBA object for the calendar.
 * @backend: Calendar backend that this @cal presents an interface to.
 * @listener: Calendar listener for notification.
 *
 * Constructs a calendar client interface object by binding the corresponding
 * CORBA object to it.  The calendar interface is bound to the specified
 * @backend, and will notify the @listener about changes to the calendar.
 *
 * Return value: The same object as the @cal argument.
 **/
Cal *
cal_construct (Cal *cal,
	       Evolution_Calendar_Cal corba_cal,
	       CalBackend *backend,
	       Evolution_Calendar_Listener listener)
{
	CalPrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (cal != NULL, NULL);
	g_return_val_if_fail (IS_CAL (cal), NULL);
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = cal->priv;

	CORBA_exception_init (&ev);
	priv->listener = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_construct: could not duplicate the listener");
		priv->listener = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	priv->backend = backend;

	bonobo_object_construct (BONOBO_OBJECT (cal), corba_cal);
	return cal;
}

/**
 * cal_corba_object_create:
 * @object: #BonoboObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar client interface @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
Evolution_Calendar_Cal
cal_corba_object_create (BonoboObject *object)
{
	POA_Evolution_Calendar_Cal *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL (object), CORBA_OBJECT_NIL);

	servant = (POA_Evolution_Calendar_Cal *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cal_vepv;

	CORBA_exception_init (&ev);
	POA_Evolution_Calendar_Cal__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_corba_object_create(): could not init the servant");
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Evolution_Calendar_Cal) bonobo_object_activate_servant (object, servant);
}

/**
 * cal_new:
 * @backend: A calendar backend.
 * @listener: A calendar listener.
 *
 * Creates a new calendar client interface object and binds it to the specified
 * @backend and @listener objects.
 *
 * Return value: A newly-created #Cal calendar client interface object, or NULL
 * if its corresponding CORBA object could not be created.
 **/
Cal *
cal_new (CalBackend *backend, Evolution_Calendar_Listener listener)
{
	Cal *cal, *retval;
	Evolution_Calendar_Cal corba_cal;
	CORBA_Environment ev;
	gboolean ret;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	cal = CAL (gtk_type_new (CAL_TYPE));
	corba_cal = cal_corba_object_create (BONOBO_OBJECT (cal));

	CORBA_exception_init (&ev);
	ret = CORBA_Object_is_nil ((CORBA_Object) corba_cal, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || ret) {
		g_message ("cal_new(): could not create the CORBA object");
		gtk_object_unref (GTK_OBJECT (cal));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	retval = cal_construct (cal, corba_cal, backend, listener);
	if (!retval) {
		g_message ("cal_new(): could not construct the calendar client interface");
		gtk_object_unref (GTK_OBJECT (cal));
		return NULL;
	}

	return retval;
}

/* Evolution calendar client
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
#include "cal-client.h"
#include "cal-listener.h"



/* Loading state for the calendar client */
typedef enum {
	LOAD_STATE_NOT_LOADED,
	LOAD_STATE_LOADING,
	LOAD_STATE_LOADED
} LoadState;

/* Private part of the CalClient structure */
typedef struct {
	/* Load state to avoid multiple loads */
	LoadState load_state;

	/* The calendar factory we are contacting */
	Evolution_Calendar_CalFactory factory;

	/* Our calendar listener */
	CalListener *listener;
} CalClientPrivate;



/* Signal IDs */
enum {
	CAL_LOADED,
	LAST_SIGNAL
};

static void cal_client_class_init (CalClientClass *class);
static void cal_client_init (CalClient *client);
static void cal_client_destroy (GtkObject *object);

static guint cal_client_signals[LAST_SIGNAL];

static GtkObjectClass *parent_class;



/**
 * cal_client_get_type:
 * @void:
 *
 * Registers the #CalClient class if necessary, and returns the type ID assigned
 * to it.
 *
 * Return value: The type ID of the #CalClient class.
 **/
GtkType
cal_client_get_type (void)
{
	static GtkType cal_client_type = 0;

	if (!cal_client_type) {
		static const GtkTypeInfo cal_client_info = {
			"CalClient",
			sizeof (CalClient),
			sizeof (CalClientClass),
			(GtkClassInitFunc) cal_client_class_init,
			(GtkObjectInitFunc) cal_client_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_client_type = gtk_type_unique (GTK_TYPE_OBJECT, &cal_client_info);
	}

	return cal_client_type;
}

/* Class initialization function for the calendar client */
static void
cal_client_class_init (CalClientClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	cal_client_signals[CAL_LOADED] =
		gtk_signal_new ("cal_loaded",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalClientClass, cal_loaded),
				gtk_marshal_NONE__ENUM,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_ENUM);

	gtk_object_class_add_signals (object_class, cal_client_signals, LAST_SIGNAL);

	object_class->destroy = cal_client_destroy;
}

/* Object initialization function for the calendar client */
static void
cal_client_init (CalClient *client)
{
	CalClientPrivate *priv;

	priv = g_new0 (CalClientPrivate, 1);
	client->priv = priv;

	priv->factory = CORBA_OBJECT_NIL;
	priv->load_state = LOAD_STATE_NOT_LOADED;
}

/* Destroy handler for the calendar client */
static void
cal_client_destroy (GtkObject *object)
{
	CalClient *client;
	CalClientPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_CLIENT (object));

	client = CAL_CLIENT (object);
	priv = client->priv;

	/* FIXME */

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/**
 * cal_client_construct:
 * @client: A calendar client.
 * 
 * Constructs a calendar client object by contacting the calendar factory of the
 * calendar server.
 * 
 * Return value: The same object as the @client argument, or NULL if the
 * calendar factory could not be contacted.
 **/
CalClient *
cal_client_construct (CalClient *client)
{
	CalClientPrivate *priv;
	Evolution_Calendar_CalFactory factory, factory_copy;
	CORBA_Environment ev;
	int result;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;

	factory = (Evolution_Calendar_CalFactory) goad_server_activate_with_id (
		NULL,
		"calendar:cal-factory",
		GOAD_ACTIVATE_REMOTE,
		NULL);

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (factory, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_construct(): could not see if the factory is NIL");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	if (result) {
		g_message ("cal_client_construct(): could not contact Tlacuache, "
			   "the personal calendar server");
		return NULL;
	}

	CORBA_exception_init (&ev);
	factory_copy = CORBA_Object_duplicate (factory, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_construct(): could not duplicate the calendar factory");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	priv->factory = factory_copy;
	return client;
}

/**
 * cal_client_new:
 * @void: 
 * 
 * Creates a new calendar client.  It should be initialized by calling
 * cal_client_load_calendar() or cal_client_create_calendar().
 * 
 * Return value: A newly-created calendar client, or NULL if the client could
 * not be constructed because it could not contact the calendar server.
 **/
CalClient *
cal_client_new (void)
{
	CalClient *client;

	client = gtk_type_new (CAL_CLIENT_TYPE);

	if (!cal_client_construct (client)) {
		g_message ("cal_client_new(): could not construct the calendar client");
		gtk_object_unref (client);
		return NULL;
	}

	return client;
}

/**
 * cal_client_load_calendar:
 * @client: A calendar client.
 * @str_uri: URI of calendar to load.
 * 
 * Makes a calendar client initiate a request to load a calendar.  The calendar
 * client will emit the "cal_loaded" signal when the response from the server is
 * received.
 * 
 * Return value: TRUE on success, FALSE on failure to issue the load request.
 **/
gboolean
cal_client_load_calendar (CalClient *client, const char *str_uri)
{
	CalClientPrivate *priv;
	Evolution_Calendar_Listener corba_listener;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_NOT_LOADED, FALSE);

	g_return_val_if_fail (str_uri != NULL, FALSE);

	priv->listener = cal_listener_new ();
	if (!priv->listener) {
		g_message ("cal_client_load_calendar(): could not create the listener");
		return FALSE;
	}

	corba_listener = (Evolution_Calendar_Listener) bonobo_object_corba_objref (priv->listener);

	CORBA_exception_init (&ev);

	priv->load_state = LOAD_STATE_LOADING;
	Evolution_Calendar_CalFactory_load (priv->factory, str_uri, corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_load_calendar(): load request failed");
		gtk_object_unref (priv->listener);
		priv->listener = NULL;
		priv->load_state = LOAD_STATE_NOT_LOADED;
		CORBA_exception_free (&ev);
		return FALSE;
	}
	CORBA_exception_free (&ev);

	return TRUE;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include <gtk/gtksignal.h>

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif

#include "cal-client.h"
#include "cal-listener.h"

#include "cal-util/icalendar-save.h"
#include "cal-util/icalendar.h"



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

	/* The calendar client interface object we are contacting */
	Evolution_Calendar_Cal cal;
} CalClientPrivate;



/* Signal IDs */
enum {
	CAL_LOADED,
	OBJ_UPDATED,
	OBJ_REMOVED,
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
	cal_client_signals[OBJ_UPDATED] =
		gtk_signal_new ("obj_updated",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalClientClass, obj_updated),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	cal_client_signals[OBJ_REMOVED] =
		gtk_signal_new ("obj_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalClientClass, obj_removed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

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

/* Gets rid of the factory that a client knows about */
static void
destroy_factory (CalClient *client)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	int result;

	priv = client->priv;

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (priv->factory, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("destroy_factory(): could not see if the factory was nil");
		priv->factory = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	if (result)
		return;

	CORBA_exception_init (&ev);
	CORBA_Object_release (priv->factory, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("destroy_factory(): could not release the factory");

	CORBA_exception_free (&ev);
	priv->factory = CORBA_OBJECT_NIL;
}

/* Gets rid of the listener that a client knows about */
static void
destroy_listener (CalClient *client)
{
	CalClientPrivate *priv;

	priv = client->priv;

	if (!priv->listener)
		return;

	bonobo_object_unref (BONOBO_OBJECT (priv->listener));
	priv->listener = NULL;
}

/* Gets rid of the calendar client interface object that a client knows about */
static void
destroy_cal (CalClient *client)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	int result;

	priv = client->priv;

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (priv->cal, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("destroy_cal(): could not see if the "
			   "calendar client interface object was nil");
		priv->cal = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	if (result)
		return;

	CORBA_exception_init (&ev);
	Evolution_Calendar_Cal_unref (priv->cal, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("destroy_cal(): could not unref the calendar client interface object");

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	CORBA_Object_release (priv->cal, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("destroy_cal(): could not release the calendar client interface object");

	CORBA_exception_free (&ev);
	priv->cal = CORBA_OBJECT_NIL;

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

	destroy_factory (client);
	destroy_listener (client);
	destroy_cal (client);

	priv->load_state = LOAD_STATE_NOT_LOADED;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Signal handlers for the listener's signals */

/* Handle the cal_loaded signal from the listener */
static void
cal_loaded_cb (CalListener *listener,
	       Evolution_Calendar_Listener_LoadStatus status,
	       Evolution_Calendar_Cal cal,
	       gpointer data)
{
	CalClient *client;
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_Cal cal_copy;
	CalClientLoadStatus client_status;

	client = CAL_CLIENT (data);
	priv = client->priv;

	g_assert (priv->load_state == LOAD_STATE_LOADING);

	client_status = CAL_CLIENT_LOAD_ERROR;

	switch (status) {
	case Evolution_Calendar_Listener_SUCCESS:
		CORBA_exception_init (&ev);
		cal_copy = CORBA_Object_duplicate (cal, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_message ("cal_loaded(): could not duplicate the calendar client interface");
			CORBA_exception_free (&ev);
			goto error;
		}
		CORBA_exception_free (&ev);

		priv->cal = cal_copy;
		priv->load_state = LOAD_STATE_LOADED;

		client_status = CAL_CLIENT_LOAD_SUCCESS;
		goto out;

	case Evolution_Calendar_Listener_ERROR:
		client_status = CAL_CLIENT_LOAD_ERROR;
		goto error;

	case Evolution_Calendar_Listener_IN_USE:
		client_status = CAL_CLIENT_LOAD_IN_USE;
		goto error;

	case Evolution_Calendar_Listener_METHOD_NOT_SUPPORTED:
		client_status = CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED;
		goto error;

	default:
		g_assert_not_reached ();
	}

 error:

	bonobo_object_unref (BONOBO_OBJECT (priv->listener));
	priv->listener = NULL;
	priv->load_state = LOAD_STATE_NOT_LOADED;

 out:

	g_assert (priv->load_state != LOAD_STATE_LOADING);

	gtk_signal_emit (GTK_OBJECT (client), cal_client_signals[CAL_LOADED],
			 client_status);
}

/* Handle the obj_updated signal from the listener */
static void
obj_updated_cb (CalListener *listener, const Evolution_Calendar_CalObjUID uid, gpointer data)
{
	CalClient *client;

	client = CAL_CLIENT (data);
	gtk_signal_emit (GTK_OBJECT (client), cal_client_signals[OBJ_UPDATED], uid);
}

/* Handle the obj_removed signal from the listener */
static void
obj_removed_cb (CalListener *listener, const Evolution_Calendar_CalObjUID uid, gpointer data)
{
	CalClient *client;

	client = CAL_CLIENT (data);
	gtk_signal_emit (GTK_OBJECT (client), cal_client_signals[OBJ_REMOVED], uid);
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

	CORBA_exception_init (&ev);
	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;

#ifdef USING_OAF
	factory = (Evolution_Calendar_CalFactory) oaf_activate_from_id (
		"OAFIID:evolution:calendar-factory:1c915858-ece3-4a6f-9d81-ea0f108a9554",
		OAF_FLAG_NO_LOCAL, NULL, &ev);
#else
	factory = (Evolution_Calendar_CalFactory) goad_server_activate_with_id (
		NULL,
		"evolution:calendar-factory",
		GOAD_ACTIVATE_REMOTE,
		NULL);
#endif

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
		gtk_object_unref (GTK_OBJECT (client));
		return NULL;
	}

	return client;
}

/* Issues a load or create request */
static gboolean
load_or_create (CalClient *client, const char *str_uri, gboolean load)
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
		g_message ("load_or_create(): could not create the listener");
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (priv->listener), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded_cb),
			    client);
	gtk_signal_connect (GTK_OBJECT (priv->listener), "obj_updated",
			    GTK_SIGNAL_FUNC (obj_updated_cb),
			    client);
	gtk_signal_connect (GTK_OBJECT (priv->listener), "obj_removed",
			    GTK_SIGNAL_FUNC (obj_removed_cb),
			    client);

	corba_listener = (Evolution_Calendar_Listener) bonobo_object_corba_objref (
		BONOBO_OBJECT (priv->listener));

	CORBA_exception_init (&ev);

	priv->load_state = LOAD_STATE_LOADING;

	if (load)
		Evolution_Calendar_CalFactory_load (priv->factory, str_uri, corba_listener, &ev);
	else
		Evolution_Calendar_CalFactory_create (priv->factory, str_uri, corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("load_or_create(): load/create request failed");
		bonobo_object_unref (BONOBO_OBJECT (priv->listener));
		priv->listener = NULL;
		priv->load_state = LOAD_STATE_NOT_LOADED;
		CORBA_exception_free (&ev);
		return FALSE;
	}
	CORBA_exception_free (&ev);

	return TRUE;
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
	return load_or_create (client, str_uri, TRUE);
}

/**
 * cal_client_create_calendar:
 * @client: A calendar client.
 * @str_uri: URI that will contain the calendar data.
 *
 * Makes a calendar client initiate a request to create a new calendar.  The
 * calendar client will emit the "cal_loaded" signal when the response from the
 * server is received.
 *
 * Return value: TRUE on success, FALSE on failure to issue the create request.
 **/
gboolean
cal_client_create_calendar (CalClient *client, const char *str_uri)
{
	return load_or_create (client, str_uri, FALSE);
}

/**
 * cal_client_get_object:
 * @client: A calendar client.
 * @uid: Unique identifier for a calendar object.
 *
 * Queries a calendar for a calendar object based on its unique identifier.
 *
 * Return value: The string representation of a complete calendar wrapping the
 * sought object, or NULL if no object had the specified UID.  A complete
 * calendar is returned because you also need the timezone data.
 **/
CalClientGetStatus cal_client_get_object (CalClient *client,
					  const char *uid,
					  iCalObject **ico)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_CalObj calobj;
	char *obj_str = NULL;

	icalcomponent* comp = NULL;
	icalcomponent *subcomp;
	iCalObject    *ical;

	g_return_val_if_fail (client != NULL, CAL_CLIENT_GET_SYNTAX_ERROR);
	g_return_val_if_fail (IS_CAL_CLIENT (client), CAL_CLIENT_GET_SYNTAX_ERROR);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_LOADED, CAL_CLIENT_GET_SYNTAX_ERROR);

	g_return_val_if_fail (uid != NULL, CAL_CLIENT_GET_SYNTAX_ERROR);

	obj_str = NULL;

	CORBA_exception_init (&ev);
	calobj = Evolution_Calendar_Cal_get_object (priv->cal, uid, &ev);

	if (ev._major == CORBA_USER_EXCEPTION
	    && strcmp (CORBA_exception_id (&ev), ex_Evolution_Calendar_Cal_NotFound) == 0)
		goto decode;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_object(): could not get the object");
		goto decode;
	}

	obj_str = g_strdup (calobj);
	CORBA_free (calobj);

 decode:
	CORBA_exception_free (&ev);

	/* convert the string into an iCalObject */
	(*ico) = NULL;
	if (obj_str == NULL) return CAL_CLIENT_GET_SYNTAX_ERROR;
	comp = icalparser_parse_string (obj_str);
	free (obj_str);
	if (!comp) return CAL_CLIENT_GET_SYNTAX_ERROR;
	subcomp = icalcomponent_get_first_component (comp, ICAL_ANY_COMPONENT);
	if (!subcomp) return CAL_CLIENT_GET_SYNTAX_ERROR;

	while (subcomp) {
		ical = ical_object_create_from_icalcomponent (subcomp);
		if (ical->type != ICAL_EVENT && 
		    ical->type != ICAL_TODO  &&
		    ical->type != ICAL_JOURNAL) {
			g_warning ("Skipping unsupported iCalendar component");
		} else {
			if (strcasecmp (ical->uid, uid) == 0) {
				(*ico) = ical;
				(*ico)->ref_count = 1;
				return CAL_CLIENT_GET_SUCCESS;
			}
		}
		subcomp = icalcomponent_get_next_component (comp,
							   ICAL_ANY_COMPONENT);
	}

	return CAL_CLIENT_GET_NOT_FOUND;
}

/**
 * cal_client_get_uids:
 * @client: A calendar client.
 * @type: Bitmask with types of objects to return.
 *
 * Queries a calendar for a list of unique identifiers corresponding to calendar
 * objects whose type matches one of the types specified in the @type flags.
 *
 * Return value: A list of strings that are the sought UIDs.
 **/
GList *
cal_client_get_uids (CalClient *client, CalObjType type)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_CalObjUIDSeq *seq;
	int t;
	GList *uids;
	int i;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	/*g_return_val_if_fail (priv->load_state == LOAD_STATE_LOADED, NULL);*/
	if (priv->load_state != LOAD_STATE_LOADED)
		return NULL;

	t = (((type & CALOBJ_TYPE_EVENT) ? Evolution_Calendar_TYPE_EVENT : 0)
	     | ((type & CALOBJ_TYPE_TODO) ? Evolution_Calendar_TYPE_TODO : 0)
	     | ((type & CALOBJ_TYPE_JOURNAL) ? Evolution_Calendar_TYPE_JOURNAL : 0)
	     | ((type & CALOBJ_TYPE_OTHER) ? Evolution_Calendar_TYPE_OTHER : 0)
	     /*
	     | ((type & CALOBJ_TYPE_ANY) ? Evolution_Calendar_TYPE_ANY : 0)
	     */
	     );

	CORBA_exception_init (&ev);

	seq = Evolution_Calendar_Cal_get_uids (priv->cal, t, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_uids(): could not get the list of UIDs");
		CORBA_exception_free (&ev);
		return NULL;
	}

	/* Create the list */

	uids = NULL;

	for (i = 0; i < seq->_length; i++)
		uids = g_list_prepend (uids, g_strdup (seq->_buffer[i]));

	CORBA_free (seq);

	return uids;
}

/* Builds a GList of CalObjInstance structures from the CORBA sequence */
static GList *
build_object_instance_list (Evolution_Calendar_CalObjInstanceSeq *seq)
{
	GList *list;
	int i;

	/* Create the list in reverse order */
	
	list = NULL;
	for (i = 0; i < seq->_length; i++) {
		Evolution_Calendar_CalObjInstance *corba_icoi;
		CalObjInstance *icoi;

		corba_icoi = &seq->_buffer[i];
		icoi = g_new (CalObjInstance, 1);

		icoi->uid = g_strdup (corba_icoi->uid);
		icoi->start = corba_icoi->start;
		icoi->end = corba_icoi->end;

		list = g_list_prepend (list, icoi);
	}

	list = g_list_reverse (list);
	return list;
}

/**
 * cal_client_get_events_in_range:
 * @client: A calendar client.
 * @start: Start time for query.
 * @end: End time for query.
 *
 * Queries a calendar for the events that occur or recur in the specified range
 * of time.
 *
 * Return value: A list of #CalObjInstance structures.
 **/
GList *
cal_client_get_events_in_range (CalClient *client, time_t start, time_t end)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_CalObjInstanceSeq *seq;
	GList *events;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	if (priv->load_state != LOAD_STATE_LOADED)
		return NULL;

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	CORBA_exception_init (&ev);

	seq = Evolution_Calendar_Cal_get_events_in_range (priv->cal, start, end, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_events_in_range(): could not get the event range");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	events = build_object_instance_list (seq);
	CORBA_free (seq);

	return events;
}

/* Translates the CORBA representation of an AlarmType */
static enum AlarmType
uncorba_alarm_type (Evolution_Calendar_AlarmType corba_type)
{
	switch (corba_type) {
	case Evolution_Calendar_MAIL:
		return ALARM_MAIL;

	case Evolution_Calendar_PROGRAM:
		return ALARM_PROGRAM;

	case Evolution_Calendar_DISPLAY:
		return ALARM_DISPLAY;

	case Evolution_Calendar_AUDIO:
		return ALARM_AUDIO;

	default:
		g_assert_not_reached ();
		return ALARM_DISPLAY;
	}
}

/* Builds a GList of CalAlarmInstance structures from the CORBA sequence */
static GList *
build_alarm_instance_list (Evolution_Calendar_CalAlarmInstanceSeq *seq)
{
	GList *list;
	int i;

	/* Create the list in reverse order */

	list = NULL;
	for (i = 0; i < seq->_length; i++) {
		Evolution_Calendar_CalAlarmInstance *corba_ai;
		CalAlarmInstance *ai;

		corba_ai = &seq->_buffer[i];
		ai = g_new (CalAlarmInstance, 1);

		ai->uid = g_strdup (corba_ai->uid);
		ai->type = uncorba_alarm_type (corba_ai->type);
		ai->trigger = corba_ai->trigger;
		ai->occur = corba_ai->occur;

		list = g_list_prepend (list, ai);
	}

	list = g_list_reverse (list);
	return list;
}

/**
 * cal_client_get_alarms_in_range:
 * @client: A calendar client.
 * @start: Start time for query.
 * @end: End time for query.
 * 
 * Queries a calendar for the alarms that trigger in the specified range of
 * time.
 * 
 * Return value: A list of #CalAlarmInstance structures.
 **/
GList *
cal_client_get_alarms_in_range (CalClient *client, time_t start, time_t end)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_CalAlarmInstanceSeq *seq;
	GList *alarms;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	if (priv->load_state != LOAD_STATE_LOADED)
		return NULL;

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	CORBA_exception_init (&ev);

	seq = Evolution_Calendar_Cal_get_alarms_in_range (priv->cal, start, end, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_alarms_in_range(): could not get the alarm range");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	alarms = build_alarm_instance_list (seq);
	CORBA_free (seq);

	return alarms;
}

/**
 * cal_client_get_alarms_for_object:
 * @client: A calendar client.
 * @uid: Unique identifier for a calendar object.
 * @start: Start time for query.
 * @end: End time for query.
 * @alarms: Return value for the list of alarm instances.
 * 
 * Queries a calendar for the alarms of a particular object that trigger in the
 * specified range of time.
 * 
 * Return value: TRUE on success, FALSE if the object was not found.
 **/
gboolean
cal_client_get_alarms_for_object (CalClient *client, const char *uid,
				  time_t start, time_t end,
				  GList **alarms)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	Evolution_Calendar_CalAlarmInstanceSeq *seq;
	gboolean retval;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	if (priv->load_state != LOAD_STATE_LOADED)
		return FALSE;

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (start != -1 && end != -1, FALSE);
	g_return_val_if_fail (start <= end, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);

	*alarms = NULL;
	retval = FALSE;

	CORBA_exception_init (&ev);

	seq = Evolution_Calendar_Cal_get_alarms_for_object (priv->cal, uid, start, end, &ev);
	if (ev._major == CORBA_USER_EXCEPTION
	    && strcmp (CORBA_exception_id (&ev), ex_Evolution_Calendar_Cal_NotFound) == 0)
		goto out;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_alarms_for_object(): could not get the alarm range");
		goto out;
	}

	retval = TRUE;
	*alarms = build_alarm_instance_list (seq);
	CORBA_free (seq);

 out:
	CORBA_exception_free (&ev);
	return retval;

}

/**
 * cal_client_update_object:
 * @client: A calendar client.
 * @uid: Unique identifier of object to update.
 * @calobj: String representation of the new calendar object.
 *
 * Asks a calendar to update an object based on its UID.  Any existing object
 * with the specified UID will be replaced.  The client program should not
 * assume that the object is actually in the server's storage until it has
 * received the "obj_updated" notification signal.
 *
 * Return value: TRUE on success, FALSE on specifying an invalid object.
 **/
gboolean
cal_client_update_object (CalClient *client, const char *uid, const char *calobj)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	gboolean retval;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_LOADED, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	retval = FALSE;

	CORBA_exception_init (&ev);
	Evolution_Calendar_Cal_update_object (priv->cal, uid, calobj, &ev);

	if (ev._major == CORBA_USER_EXCEPTION &&
	    strcmp (CORBA_exception_id (&ev), ex_Evolution_Calendar_Cal_InvalidObject) == 0)
		goto out;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_update_object(): could not update the object");
		goto out;
	}

	retval = TRUE;

 out:
	CORBA_exception_free (&ev);
	return retval;
}

gboolean
cal_client_remove_object (CalClient *client, const char *uid)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	gboolean retval;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == LOAD_STATE_LOADED, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);

	retval = FALSE;

	CORBA_exception_init (&ev);
	Evolution_Calendar_Cal_remove_object (priv->cal, uid, &ev);

	if (ev._major == CORBA_USER_EXCEPTION &&
	    strcmp (CORBA_exception_id (&ev), ex_Evolution_Calendar_Cal_NotFound) == 0)
		goto out;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_remove_object(): could not remove the object");
		goto out;
	}

	retval = TRUE;

 out:
	CORBA_exception_free (&ev);
	return retval;
}

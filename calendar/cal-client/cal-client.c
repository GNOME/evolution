/* Evolution calendar client
 *
 * Copyright (C) 2001 Ximian, Inc.
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
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <liboaf/liboaf.h>

#include "cal-client-types.h"
#include "cal-client.h"
#include "cal-listener.h"



/* Private part of the CalClient structure */
struct _CalClientPrivate {
	/* Load state to avoid multiple loads */
	CalClientLoadState load_state;

	/* URI of the calendar that is being loaded or is already loaded, or
	 * NULL if we are not loaded.
	 */
	char *uri;

	/* The calendar factory we are contacting */
	GNOME_Evolution_Calendar_CalFactory factory;

	/* Our calendar listener implementation */
	CalListener *listener;

	/* The calendar client interface object we are contacting */
	GNOME_Evolution_Calendar_Cal cal;

	/* An array of CalTimezone structs containing information on builtin
	   timezones. We cache this so we only request it once from the
	   server. */
	GArray *timezone_info;
};



/* Signal IDs */
enum {
	CAL_OPENED,
	OBJ_UPDATED,
	OBJ_REMOVED,
	LAST_SIGNAL
};

static void cal_client_class_init (CalClientClass *class);
static void cal_client_init (CalClient *client);
static void cal_client_destroy (GtkObject *object);

static void cal_client_free_builtin_timezone_info (GArray	*zones);

static guint cal_client_signals[LAST_SIGNAL];

static GtkObjectClass *parent_class;



/**
 * cal_client_get_type:
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

	cal_client_signals[CAL_OPENED] =
		gtk_signal_new ("cal_opened",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalClientClass, cal_opened),
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

	priv->load_state = CAL_CLIENT_LOAD_NOT_LOADED;
	priv->uri = NULL;
	priv->factory = CORBA_OBJECT_NIL;
	priv->timezone_info = NULL;
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
	GNOME_Evolution_Calendar_Cal_unref (priv->cal, &ev);
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

	priv->load_state = CAL_CLIENT_LOAD_NOT_LOADED;

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->timezone_info) {
		cal_client_free_builtin_timezone_info (priv->timezone_info);
		priv->timezone_info = NULL;
	}

	g_free (priv);
	client->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Signal handlers for the listener's signals */

/* Handle the cal_opened notification from the listener */
static void
cal_opened_cb (CalListener *listener,
	       GNOME_Evolution_Calendar_Listener_OpenStatus status,
	       GNOME_Evolution_Calendar_Cal cal,
	       gpointer data)
{
	CalClient *client;
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_Cal cal_copy;
	CalClientOpenStatus client_status;

	client = CAL_CLIENT (data);
	priv = client->priv;

	g_assert (priv->load_state == CAL_CLIENT_LOAD_LOADING);
	g_assert (priv->uri != NULL);

	client_status = CAL_CLIENT_OPEN_ERROR;

	switch (status) {
	case GNOME_Evolution_Calendar_Listener_SUCCESS:
		CORBA_exception_init (&ev);
		cal_copy = CORBA_Object_duplicate (cal, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_message ("cal_opened_cb(): could not duplicate the "
				   "calendar client interface");
			CORBA_exception_free (&ev);
			goto error;
		}
		CORBA_exception_free (&ev);

		priv->cal = cal_copy;
		priv->load_state = CAL_CLIENT_LOAD_LOADED;

		client_status = CAL_CLIENT_OPEN_SUCCESS;
		goto out;

	case GNOME_Evolution_Calendar_Listener_ERROR:
		client_status = CAL_CLIENT_OPEN_ERROR;
		goto error;

	case GNOME_Evolution_Calendar_Listener_NOT_FOUND:
		client_status = CAL_CLIENT_OPEN_NOT_FOUND;
		goto error;

	case GNOME_Evolution_Calendar_Listener_METHOD_NOT_SUPPORTED:
		client_status = CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED;
		goto error;

	default:
		g_assert_not_reached ();
	}

 error:

	bonobo_object_unref (BONOBO_OBJECT (priv->listener));
	priv->listener = NULL;

	/* We free the priv->uri and set the priv->load_state until after the
	 * "cal_opened" signal has been emitted so that handlers will be able to
	 * access this information.
	 */

 out:

	/* We are *not* inside a signal handler (this is just a simple callback
	 * called from the listener), so there is not a temporary reference to
	 * the client object.  We ref() so that we can safely emit our own
	 * signal and clean up.
	 */

	gtk_object_ref (GTK_OBJECT (client));

	gtk_signal_emit (GTK_OBJECT (client), cal_client_signals[CAL_OPENED],
			 client_status);

	if (client_status != CAL_CLIENT_OPEN_SUCCESS) {
		priv->load_state = CAL_CLIENT_LOAD_NOT_LOADED;
		g_free (priv->uri);
		priv->uri = NULL;
	}

	g_assert (priv->load_state != CAL_CLIENT_LOAD_LOADING);

	gtk_object_unref (GTK_OBJECT (client));
}

/* Handle the obj_updated signal from the listener */
static void
obj_updated_cb (CalListener *listener, const GNOME_Evolution_Calendar_CalObjUID uid, gpointer data)
{
	CalClient *client;

	client = CAL_CLIENT (data);
	gtk_signal_emit (GTK_OBJECT (client), cal_client_signals[OBJ_UPDATED], uid);
}

/* Handle the obj_removed signal from the listener */
static void
obj_removed_cb (CalListener *listener, const GNOME_Evolution_Calendar_CalObjUID uid, gpointer data)
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
	GNOME_Evolution_Calendar_CalFactory factory, factory_copy;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;

	CORBA_exception_init (&ev);
	factory = (GNOME_Evolution_Calendar_CalFactory) oaf_activate_from_id (
		"OAFIID:GNOME_Evolution_Wombat_CalendarFactory",
		0, NULL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_construct(): Could not activate the calendar factory");
		CORBA_exception_free (&ev);
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
 *
 * Creates a new calendar client.  It should be initialized by calling
 * cal_client_open_calendar().
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

/**
 * cal_client_open_calendar:
 * @client: A calendar client.
 * @str_uri: URI of calendar to open.
 * @only_if_exists: FALSE if the calendar should be opened even if there
 * was no storage for it, i.e. to create a new calendar or load an existing
 * one if it already exists.  TRUE if it should only try to load calendars
 * that already exist.
 *
 * Makes a calendar client initiate a request to open a calendar.  The calendar
 * client will emit the "cal_opened" signal when the response from the server is
 * received.
 *
 * Return value: TRUE on success, FALSE on failure to issue the open request.
 **/
gboolean
cal_client_open_calendar (CalClient *client, const char *str_uri, gboolean only_if_exists)
{
	CalClientPrivate *priv;
	GNOME_Evolution_Calendar_Listener corba_listener;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_NOT_LOADED, FALSE);
	g_assert (priv->uri == NULL);

	g_return_val_if_fail (str_uri != NULL, FALSE);

	priv->listener = cal_listener_new (cal_opened_cb,
					   obj_updated_cb,
					   obj_removed_cb,
					   client);
	if (!priv->listener) {
		g_message ("cal_client_open_calendar(): could not create the listener");
		return FALSE;
	}

	corba_listener = (GNOME_Evolution_Calendar_Listener) bonobo_object_corba_objref (
		BONOBO_OBJECT (priv->listener));
	
	CORBA_exception_init (&ev);

	priv->load_state = CAL_CLIENT_LOAD_LOADING;
	priv->uri = g_strdup (str_uri);

	GNOME_Evolution_Calendar_CalFactory_open (priv->factory, str_uri, only_if_exists,
						  corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);

		g_message ("cal_client_open_calendar(): open request failed");
		bonobo_object_unref (BONOBO_OBJECT (priv->listener));
		priv->listener = NULL;
		priv->load_state = CAL_CLIENT_LOAD_NOT_LOADED;
		g_free (priv->uri);
		priv->uri = NULL;

		return FALSE;
	}
	CORBA_exception_free (&ev);

	return TRUE;
}

/**
 * cal_client_get_load_state:
 * @client: A calendar client.
 * 
 * Queries the state of loading of a calendar client.
 * 
 * Return value: A #CalClientLoadState value indicating whether the client has
 * not been loaded with cal_client_open_calendar() yet, whether it is being
 * loaded, or whether it is already loaded.
 **/
CalClientLoadState
cal_client_get_load_state (CalClient *client)
{
	CalClientPrivate *priv;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	return priv->load_state;
}

/**
 * cal_client_get_uri:
 * @client: A calendar client.
 * 
 * Queries the URI that is open in a calendar client.
 * 
 * Return value: The URI of the calendar that is already loaded or is being
 * loaded, or NULL if the client has not started a load request yet.
 **/
const char *
cal_client_get_uri (CalClient *client)
{
	CalClientPrivate *priv;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	return priv->uri;
}

/* Converts our representation of a calendar component type into its CORBA representation */
static GNOME_Evolution_Calendar_CalObjType
corba_obj_type (CalObjType type)
{
	return (((type & CALOBJ_TYPE_EVENT) ? GNOME_Evolution_Calendar_TYPE_EVENT : 0)
		| ((type & CALOBJ_TYPE_TODO) ? GNOME_Evolution_Calendar_TYPE_TODO : 0)
		| ((type & CALOBJ_TYPE_JOURNAL) ? GNOME_Evolution_Calendar_TYPE_JOURNAL : 0));
}

/**
 * cal_client_get_n_objects:
 * @client: A calendar client.
 * @type: Type of objects that will be counted.
 * 
 * Counts the number of calendar components of the specified @type.  This can be
 * used to count how many events, to-dos, or journals there are, for example.
 * 
 * Return value: Number of components.
 **/
int
cal_client_get_n_objects (CalClient *client, CalObjType type)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	int n;
	int t;

	g_return_val_if_fail (client != NULL, -1);
	g_return_val_if_fail (IS_CAL_CLIENT (client), -1);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, -1);

	t = corba_obj_type (type);

	CORBA_exception_init (&ev);
	n = GNOME_Evolution_Calendar_Cal_countObjects (priv->cal, t, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_n_objects(): could not get the number of objects");
		CORBA_exception_free (&ev);
		return -1;
	}

	CORBA_exception_free (&ev);
	return n;
}

/**
 * cal_client_get_object:
 * @client: A calendar client.
 * @uid: Unique identifier for a calendar component.
 * @comp: Return value for the calendar component object.
 *
 * Queries a calendar for a calendar component object based on its unique
 * identifier.
 *
 * Return value: Result code based on the status of the operation.
 **/
CalClientGetStatus
cal_client_get_object (CalClient *client, const char *uid, CalComponent **comp)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObj comp_str;
	CalClientGetStatus retval;
	icalcomponent *icalcomp;

	g_return_val_if_fail (client != NULL, CAL_CLIENT_GET_NOT_FOUND);
	g_return_val_if_fail (IS_CAL_CLIENT (client), CAL_CLIENT_GET_NOT_FOUND);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, CAL_CLIENT_GET_NOT_FOUND);

	g_return_val_if_fail (uid != NULL, CAL_CLIENT_GET_NOT_FOUND);
	g_return_val_if_fail (comp != NULL, CAL_CLIENT_GET_NOT_FOUND);

	retval = CAL_CLIENT_GET_NOT_FOUND;
	*comp = NULL;

	CORBA_exception_init (&ev);
	comp_str = GNOME_Evolution_Calendar_Cal_getObject (priv->cal, (char *) uid, &ev);

	if (ev._major == CORBA_USER_EXCEPTION
	    && strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_Calendar_Cal_NotFound) == 0)
		goto out;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_object(): could not get the object");
		goto out;
	}

	icalcomp = icalparser_parse_string (comp_str);
	CORBA_free (comp_str);

	if (!icalcomp) {
		retval = CAL_CLIENT_GET_SYNTAX_ERROR;
		goto out;
	}

	*comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (*comp, icalcomp)) {
		icalcomponent_free (icalcomp);
		gtk_object_unref (GTK_OBJECT (*comp));
		*comp = NULL;

		retval = CAL_CLIENT_GET_SYNTAX_ERROR;
		goto out;
	}

	retval = CAL_CLIENT_GET_SUCCESS;

 out:

	CORBA_exception_free (&ev);
	return retval;
}

/* Builds an UID list out of a CORBA UID sequence */
static GList *
build_uid_list (GNOME_Evolution_Calendar_CalObjUIDSeq *seq)
{
	GList *uids;
	int i;

	uids = NULL;

	for (i = 0; i < seq->_length; i++)
		uids = g_list_prepend (uids, g_strdup (seq->_buffer[i]));

	return uids;
}

/**
 * cal_client_get_uids:
 * @client: A calendar client.
 * @type: Bitmask with types of objects to return.
 *
 * Queries a calendar for a list of unique identifiers corresponding to calendar
 * objects whose type matches one of the types specified in the @type flags.
 *
 * Return value: A list of strings that are the sought UIDs.  This should be
 * freed using the cal_obj_uid_list_free() function.
 **/
GList *
cal_client_get_uids (CalClient *client, CalObjType type)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjUIDSeq *seq;
	int t;
	GList *uids;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, NULL);

	t = corba_obj_type (type);

	CORBA_exception_init (&ev);

	seq = GNOME_Evolution_Calendar_Cal_getUIDs (priv->cal, t, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_uids(): could not get the list of UIDs");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	uids = build_uid_list (seq);
	CORBA_free (seq);

	return uids;
}

/* Builds a GList of CalClientChange structures from the CORBA sequence */
static GList *
build_change_list (GNOME_Evolution_Calendar_CalObjChangeSeq *seq)
{
	GList *list = NULL;
	icalcomponent *icalcomp;
	int i;

	/* Create the list in reverse order */
	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalObjChange *corba_coc;
		CalClientChange *ccc;

		corba_coc = &seq->_buffer[i];
		ccc = g_new (CalClientChange, 1);

		icalcomp = icalparser_parse_string (corba_coc->calobj);
		if (!icalcomp)
			continue;

		ccc->comp = cal_component_new ();
		if (!cal_component_set_icalcomponent (ccc->comp, icalcomp)) {
			icalcomponent_free (icalcomp);
			gtk_object_unref (GTK_OBJECT (ccc->comp));
			continue;
		}
		ccc->type = corba_coc->type;

		list = g_list_prepend (list, ccc);
	}

	list = g_list_reverse (list);

	return list;
}

GList *
cal_client_get_changes (CalClient *client, CalObjType type, const char *change_id)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjChangeSeq *seq;
	int t;
	GList *changes;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, NULL);

	t = corba_obj_type (type);
	CORBA_exception_init (&ev);

	seq = GNOME_Evolution_Calendar_Cal_getChanges (priv->cal, t, change_id, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_changes(): could not get the list of changes");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	changes = build_change_list (seq);
	CORBA_free (seq);

	return changes;
}

/* FIXME: Not used? */
#if 0
/* Builds a GList of CalObjInstance structures from the CORBA sequence */
static GList *
build_object_instance_list (GNOME_Evolution_Calendar_CalObjInstanceSeq *seq)
{
	GList *list;
	int i;

	/* Create the list in reverse order */

	list = NULL;
	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalObjInstance *corba_icoi;
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
#endif

/**
 * cal_client_get_objects_in_range:
 * @client: A calendar client.
 * @type: Bitmask with types of objects to return.
 * @start: Start time for query.
 * @end: End time for query.
 *
 * Queries a calendar for the objects that occur or recur in the specified range
 * of time.
 *
 * Return value: A list of UID strings.  This should be freed using the
 * cal_obj_uid_list_free() function.
 **/
GList *
cal_client_get_objects_in_range (CalClient *client, CalObjType type, time_t start, time_t end)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalObjUIDSeq *seq;
	GList *uids;
	int t;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	CORBA_exception_init (&ev);

	t = corba_obj_type (type);

	seq = GNOME_Evolution_Calendar_Cal_getObjectsInRange (priv->cal, t, start, end, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_objects_in_range(): could not get the objects");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	uids = build_uid_list (seq);
	CORBA_free (seq);

	return uids;
}

/* Callback used when an object is updated and we must update the copy we have */
static void
generate_instances_obj_updated_cb (CalClient *client, const char *uid, gpointer data)
{
	GHashTable *uid_comp_hash;
	CalComponent *comp;
	CalClientGetStatus status;
	const char *comp_uid;

	uid_comp_hash = data;

	comp = g_hash_table_lookup (uid_comp_hash, uid);
	if (!comp)
		/* OK, so we don't care about new objects that may indeed be in
		 * the requested time range.  We only care about the ones that
		 * were returned by the first query to
		 * cal_client_get_objects_in_range().
		 */
		return;

	g_hash_table_remove (uid_comp_hash, uid);
	gtk_object_unref (GTK_OBJECT (comp));

	status = cal_client_get_object (client, uid, &comp);

	switch (status) {
	case CAL_CLIENT_GET_SUCCESS:
		/* The hash key comes from the component's internal data */
		cal_component_get_uid (comp, &comp_uid);
		g_hash_table_insert (uid_comp_hash, (char *) comp_uid, comp);
		break;

	case CAL_CLIENT_GET_NOT_FOUND:
		/* No longer in the server, too bad */
		break;

	case CAL_CLIENT_GET_SYNTAX_ERROR:
		g_message ("obj_updated_cb(): Syntax error when getting "
			   "object `%s'; ignoring...", uid);
		break;
		
	}
}

/* Callback used when an object is removed and we must delete the copy we have */
static void
generate_instances_obj_removed_cb (CalClient *client, const char *uid, gpointer data)
{
	GHashTable *uid_comp_hash;
	CalComponent *comp;

	uid_comp_hash = data;

	comp = g_hash_table_lookup (uid_comp_hash, uid);
	if (!comp)
		return;

	g_hash_table_remove (uid_comp_hash, uid);
	gtk_object_unref (GTK_OBJECT (comp));
}

/* Adds a component to the list; called from g_hash_table_foreach() */
static void
add_component (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp;
	GList **list;

	comp = CAL_COMPONENT (value);
	list = data;

	*list = g_list_prepend (*list, comp);
}

/* Gets a list of components that recur within the specified range of time.  It
 * ensures that the resulting list of CalComponent objects contains only objects
 * that are actually in the server at the time the initial
 * cal_client_get_objects_in_range() query ends.
 */
static GList *
get_objects_atomically (CalClient *client, CalObjType type, time_t start, time_t end)
{
	GList *uids;
	GHashTable *uid_comp_hash;
	GList *objects;
	guint obj_updated_id;
	guint obj_removed_id;
	GList *l;

	uids = cal_client_get_objects_in_range (client, type, start, end);

	uid_comp_hash = g_hash_table_new (g_str_hash, g_str_equal);

	/* While we are getting the actual object data, keep track of changes */

	obj_updated_id = gtk_signal_connect (GTK_OBJECT (client), "obj_updated",
					     GTK_SIGNAL_FUNC (generate_instances_obj_updated_cb),
					     uid_comp_hash);

	obj_removed_id = gtk_signal_connect (GTK_OBJECT (client), "obj_removed",
					     GTK_SIGNAL_FUNC (generate_instances_obj_removed_cb),
					     uid_comp_hash);

	/* Get the objects */

	for (l = uids; l; l = l->next) {
		CalComponent *comp;
		CalClientGetStatus status;
		char *uid;
		const char *comp_uid;

		uid = l->data;

		status = cal_client_get_object (client, uid, &comp);

		switch (status) {
		case CAL_CLIENT_GET_SUCCESS:
			/* The hash key comes from the component's internal data
			 * instead of the duped UID from the list of UIDS.
			 */
			cal_component_get_uid (comp, &comp_uid);
			g_hash_table_insert (uid_comp_hash, (char *) comp_uid, comp);
			break;

		case CAL_CLIENT_GET_NOT_FOUND:
			/* Object disappeared from the server, so don't log it */
			break;

		case CAL_CLIENT_GET_SYNTAX_ERROR:
			g_message ("get_objects_atomically(): Syntax error when getting "
				   "object `%s'; ignoring...", uid);
			break;

		default:
			g_assert_not_reached ();
		}
	}

	cal_obj_uid_list_free (uids);

	/* Now our state is consistent with the server, so disconnect from the
	 * notification signals and generate the final list of components.
	 */

	gtk_signal_disconnect (GTK_OBJECT (client), obj_updated_id);
	gtk_signal_disconnect (GTK_OBJECT (client), obj_removed_id);

	objects = NULL;
	g_hash_table_foreach (uid_comp_hash, add_component, &objects);
	g_hash_table_destroy (uid_comp_hash);

	return objects;
}

struct comp_instance {
	CalComponent *comp;
	time_t start;
	time_t end;
};

/* Called from cal_recur_generate_instances(); adds an instance to the list */
static gboolean
add_instance (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	GList **list;
	struct comp_instance *ci;

	list = data;

	ci = g_new (struct comp_instance, 1);

	ci->comp = comp;
	gtk_object_ref (GTK_OBJECT (ci->comp));
	
	ci->start = start;
	ci->end = end;

	*list = g_list_prepend (*list, ci);

	return TRUE;
}

/* Used from g_list_sort(); compares two struct comp_instance structures */
static gint
compare_comp_instance (gconstpointer a, gconstpointer b)
{
	const struct comp_instance *cia, *cib;
	time_t diff;

	cia = a;
	cib = b;

	diff = cia->start - cib->start;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

/**
 * cal_client_generate_instances:
 * @client: A calendar client.
 * @type: Bitmask with types of objects to return.
 * @start: Start time for query.
 * @end: End time for query.
 * @cb: Callback for each generated instance.
 * @cb_data: Closure data for the callback.
 * 
 * Does a combination of cal_client_get_objects_in_range() and
 * cal_recur_generate_instances().  It fetches the list of objects in an atomic
 * way so that the generated instances are actually in the server at the time
 * the initial cal_client_get_objects_in_range() query ends.
 *
 * The callback function should do a gtk_object_ref() of the calendar component
 * it gets passed if it intends to keep it around.
 **/
void
cal_client_generate_instances (CalClient *client, CalObjType type,
			       time_t start, time_t end,
			       CalRecurInstanceFn cb, gpointer cb_data)
{
	CalClientPrivate *priv;
	GList *objects;
	GList *instances;
	GList *l;

	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_CAL_CLIENT (client));

	priv = client->priv;
	g_return_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED);

	g_return_if_fail (start != -1 && end != -1);
	g_return_if_fail (start <= end);
	g_return_if_fail (cb != NULL);

	/* Generate objects */

	objects = get_objects_atomically (client, type, start, end);
	instances = NULL;

	for (l = objects; l; l = l->next) {
		CalComponent *comp;

		comp = l->data;
		cal_recur_generate_instances (comp, start, end, add_instance, &instances);
		gtk_object_unref (GTK_OBJECT (comp));
	}

	g_list_free (objects);

	/* Generate instances and spew them out */

	instances = g_list_sort (instances, compare_comp_instance);

	for (l = instances; l; l = l->next) {
		struct comp_instance *ci;
		gboolean result;
		
		ci = l->data;
		
		result = (* cb) (ci->comp, ci->start, ci->end, cb_data);

		if (!result)
			break;
	}

	/* Clean up */

	for (l = instances; l; l = l->next) {
		struct comp_instance *ci;

		ci = l->data;
		gtk_object_unref (GTK_OBJECT (ci->comp));
		g_free (ci);
	}

	g_list_free (instances);
}

/* Builds a list of CalAlarmInstance structures */
static GSList *
build_alarm_instance_list (CalComponent *comp, GNOME_Evolution_Calendar_CalAlarmInstanceSeq *seq)
{
	GSList *alarms;
	int i;

	alarms = NULL;

	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalAlarmInstance *corba_instance;
		CalComponentAlarm *alarm;
		const char *auid;
		CalAlarmInstance *instance;

		corba_instance = seq->_buffer + i;

		/* Since we want the in-commponent auid, we look for the alarm
		 * in the component and fetch its "real" auid.
		 */

		alarm = cal_component_get_alarm (comp, corba_instance->auid);
		if (!alarm)
			continue;

		auid = cal_component_alarm_get_uid (alarm);
		cal_component_alarm_free (alarm);

		instance = g_new (CalAlarmInstance, 1);
		instance->auid = auid;
		instance->trigger = corba_instance->trigger;
		instance->occur = corba_instance->occur;

		alarms = g_slist_prepend (alarms, instance);
	}

	return g_slist_reverse (alarms);
}

/* Builds a list of CalComponentAlarms structures */
static GSList *
build_component_alarms_list (GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq)
{
	GSList *comp_alarms;
	int i;

	comp_alarms = NULL;

	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalComponentAlarms *corba_alarms;
		CalComponent *comp;
		CalComponentAlarms *alarms;
		icalcomponent *icalcomp;

		corba_alarms = seq->_buffer + i;

		icalcomp = icalparser_parse_string (corba_alarms->calobj);
		if (!icalcomp)
			continue;

		comp = cal_component_new ();
		if (!cal_component_set_icalcomponent (comp, icalcomp)) {
			icalcomponent_free (icalcomp);
			gtk_object_unref (GTK_OBJECT (comp));
			continue;
		}

		alarms = g_new (CalComponentAlarms, 1);
		alarms->comp = comp;
		alarms->alarms = build_alarm_instance_list (comp, &corba_alarms->alarms);

		comp_alarms = g_slist_prepend (comp_alarms, alarms);
	}

	return comp_alarms;
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
 * Return value: A list of #CalComponentAlarms structures.  This should be freed
 * using the cal_client_free_alarms() function, or by freeing each element
 * separately with cal_component_alarms_free() and then freeing the list with
 * g_slist_free().
 **/
GSList *
cal_client_get_alarms_in_range (CalClient *client, time_t start, time_t end)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq;
	GSList *alarms;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	CORBA_exception_init (&ev);

	seq = GNOME_Evolution_Calendar_Cal_getAlarmsInRange (priv->cal, start, end, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_alarms_in_range(): could not get the alarm range");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	alarms = build_component_alarms_list (seq);
	CORBA_free (seq);

	return alarms;
}

/**
 * cal_client_free_alarms:
 * @comp_alarms: A list of #CalComponentAlarms structures.
 * 
 * Frees a list of #CalComponentAlarms structures as returned by
 * cal_client_get_alarms_in_range().
 **/
void
cal_client_free_alarms (GSList *comp_alarms)
{
	GSList *l;

	for (l = comp_alarms; l; l = l->next) {
		CalComponentAlarms *alarms;

		alarms = l->data;
		g_assert (alarms != NULL);

		cal_component_alarms_free (alarms);
	}

	g_slist_free (comp_alarms);
}

/**
 * cal_client_get_alarms_for_object:
 * @client: A calendar client.
 * @uid: Unique identifier for a calendar component.
 * @start: Start time for query.
 * @end: End time for query.
 * @alarms: Return value for the component's alarm instances.  Will return NULL
 * if no instances occur within the specified time range.  This should be freed
 * using the cal_component_alarms_free() function.
 *
 * Queries a calendar for the alarms of a particular object that trigger in the
 * specified range of time.
 *
 * Return value: TRUE on success, FALSE if the object was not found.
 **/
gboolean
cal_client_get_alarms_for_object (CalClient *client, const char *uid,
				  time_t start, time_t end,
				  CalComponentAlarms **alarms)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalComponentAlarms *corba_alarms;
	gboolean retval;
	icalcomponent *icalcomp;
	CalComponent *comp;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (start != -1 && end != -1, FALSE);
	g_return_val_if_fail (start <= end, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);

	*alarms = NULL;
	retval = FALSE;

	CORBA_exception_init (&ev);

	corba_alarms = GNOME_Evolution_Calendar_Cal_getAlarmsForObject (priv->cal, (char *) uid,
									start, end, &ev);
	if (ev._major == CORBA_USER_EXCEPTION
	    && strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_Calendar_Cal_NotFound) == 0)
		goto out;
	else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_alarms_for_object(): could not get the alarm range");
		goto out;
	}

	icalcomp = icalparser_parse_string (corba_alarms->calobj);
	if (!icalcomp)
		goto out;

	comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (comp, icalcomp)) {
		icalcomponent_free (icalcomp);
		gtk_object_unref (GTK_OBJECT (comp));
		goto out;
	}

	retval = TRUE;

	*alarms = g_new (CalComponentAlarms, 1);
	(*alarms)->comp = comp;
	(*alarms)->alarms = build_alarm_instance_list (comp, &corba_alarms->alarms);
	CORBA_free (corba_alarms);

 out:
	CORBA_exception_free (&ev);
	return retval;
}

/**
 * cal_client_update_object:
 * @client: A calendar client.
 * @comp: A calendar component object.
 *
 * Asks a calendar to update a component.  Any existing component with the
 * specified component's UID will be replaced.  The client program should not
 * assume that the object is actually in the server's storage until it has
 * received the "obj_updated" notification signal.
 *
 * Return value: TRUE on success, FALSE on specifying an invalid component.
 **/
gboolean
cal_client_update_object (CalClient *client, CalComponent *comp)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	gboolean retval;
	char *obj_string;
	const char *uid;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, FALSE);

	g_return_val_if_fail (comp != NULL, FALSE);

	retval = FALSE;

	cal_component_commit_sequence (comp);
	obj_string = cal_component_get_as_string (comp);

	cal_component_get_uid (comp, &uid);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Cal_updateObject (priv->cal, (char *) uid, obj_string, &ev);
	g_free (obj_string);

	if (ev._major == CORBA_USER_EXCEPTION &&
	    strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_Calendar_Cal_InvalidObject) == 0)
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

/**
 * cal_client_remove_object:
 * @client: A calendar client.
 * @uid: Unique identifier of the calendar component to remove.
 * 
 * Asks a calendar to remove a component.  If the server is able to remove the
 * component, all clients will be notified and they will emit the "obj_removed"
 * signal.
 * 
 * Return value: TRUE on success, FALSE on specifying a UID for a component that
 * is not in the server.  Returning FALSE is normal; the object may have
 * disappeared from the server before the client has had a chance to receive the
 * corresponding notification.
 **/
gboolean
cal_client_remove_object (CalClient *client, const char *uid)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	gboolean retval;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_CLIENT (client), FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);

	retval = FALSE;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Cal_removeObject (priv->cal, (char *) uid, &ev);

	if (ev._major == CORBA_USER_EXCEPTION &&
	    strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_Calendar_Cal_NotFound) == 0)
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

/* Builds a list of CalComponentAlarms structures */
static GArray *
build_timezone_info_array (GNOME_Evolution_Calendar_CalTimezoneInfoSeq *seq)
{
	GArray *zones;
	CalTimezoneInfo zone;
	int i;

	zones = g_array_new (FALSE, FALSE, sizeof (CalTimezoneInfo));

	for (i = 0; i < seq->_length; i++) {
		GNOME_Evolution_Calendar_CalTimezoneInfo *tzinfo;

		tzinfo = seq->_buffer + i;

		zone.location = g_strdup (tzinfo->location);
		zone.latitude = tzinfo->latitude;
		zone.longitude = tzinfo->longitude;

		g_array_append_val (zones, zone);
	}

	return zones;
}

/**
 * cal_client_get_builtin_timezone_info:
 * @client: A calendar client.
 *
 * Returns information on the builtin timezones, i.e. their names and
 * locations. This is so we can use the map to select a timezone.
 *
 * Return value: An array of #CalTimezoneInfo structures. The caller should not
 * change or free this array. The CalClient will free it when it is destroyed.
 **/
GArray *
cal_client_get_builtin_timezone_info (CalClient *client)
{
	CalClientPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_CalTimezoneInfoSeq *seq;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;

	/* If we have already got this data from the server just return it. */
	if (priv->timezone_info)
		return priv->timezone_info;

	CORBA_exception_init (&ev);

	seq = GNOME_Evolution_Calendar_Cal_getBuiltinTimezoneInfo (priv->cal,
								   &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_client_get_builtin_timezone_info(): could not get the builtin timezone info");
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	priv->timezone_info = build_timezone_info_array (seq);
	CORBA_free (seq);

	return priv->timezone_info;
}

/**
 * cal_client_free_builtin_timezone_info:
 * @zones: An array of timezone info returned from
 * cal_client_get_builtin_timezone_info().
 *
 * Frees the builtin timezone information structures.
 **/
static void
cal_client_free_builtin_timezone_info (GArray	*zones)
{
	CalTimezoneInfo *zone;
	int i;

	for (i = 0; i < zones->len; i++) {
		zone = &g_array_index (zones, CalTimezoneInfo, i);
		g_free (zone->location);
	}

	g_array_free (zones, TRUE);
}

/**
 * cal_client_get_query:
 * @client: A calendar client.
 * @sexp: S-expression representing the query.
 * 
 * Creates a live query object from a loaded calendar.
 * 
 * Return value: A query object that will emit notification signals as calendar
 * components are added and removed from the query in the server.
 **/
CalQuery *
cal_client_get_query (CalClient *client, const char *sexp)
{
	CalClientPrivate *priv;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (IS_CAL_CLIENT (client), NULL);

	priv = client->priv;
	g_return_val_if_fail (priv->load_state == CAL_CLIENT_LOAD_LOADED, FALSE);

	g_return_val_if_fail (sexp != NULL, NULL);

	return cal_query_new (priv->cal, sexp);
}

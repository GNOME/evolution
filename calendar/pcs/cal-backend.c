/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>    
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>

#include "cal-backend.h"
#include "libversit/vcc.h"



/* A category that exists in some of the objects of the calendar */
typedef struct {
	/* Category name, also used as the key in the categories hash table */
	char *name;

	/* Number of objects that have this category */
	int refcount;
} CalBackendCategory;

/* Private part of the CalBackend structure */
struct _CalBackendPrivate {
	/* The uri for this backend */
	char *uri;

	/* The kind of components for this backend */
	icalcomponent_kind kind;
	
	/* List of Cal objects */
	GMutex *clients_mutex;
	GList *clients;

	GMutex *queries_mutex;
	EList *queries;
	
	/* Hash table of live categories, temporary hash of
	 * added/removed categories, and idle handler for sending
	 * category_changed.
	 */
	GHashTable *categories;
	GHashTable *changed_categories;
	guint category_idle_id;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_URI,
	PROP_KIND
};

/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	OPENED,
	REMOVED,
	LAST_SIGNAL
};
static guint cal_backend_signals[LAST_SIGNAL];

static void cal_backend_class_init (CalBackendClass *class);
static void cal_backend_init (CalBackend *backend);
static void cal_backend_finalize (GObject *object);

static void notify_categories_changed (CalBackend *backend);

#define CLASS(backend) (CAL_BACKEND_CLASS (G_OBJECT_GET_CLASS (backend)))

static GObjectClass *parent_class;



/**
 * cal_backend_get_type:
 * @void:
 *
 * Registers the #CalBackend class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalBackend class.
 **/
GType
cal_backend_get_type (void)
{
	static GType cal_backend_type = 0;

	if (!cal_backend_type) {
		static GTypeInfo info = {
                        sizeof (CalBackendClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_backend_class_init,
                        NULL, NULL,
                        sizeof (CalBackend),
                        0,
                        (GInstanceInitFunc) cal_backend_init,
                };
		cal_backend_type = g_type_register_static (G_TYPE_OBJECT, "CalBackend", &info, 0);
	}

	return cal_backend_type;
}

static void
cal_backend_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	CalBackend *backend;
	CalBackendPrivate *priv;
	
	backend = CAL_BACKEND (object);
	priv = backend->priv;
	
	switch (property_id) {
	case PROP_URI:
		g_free (priv->uri);
		priv->uri = g_value_dup_string (value);
		break;
	case PROP_KIND:
		priv->kind = g_value_get_ulong (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
cal_backend_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
 	CalBackend *backend;
	CalBackendPrivate *priv;
	
	backend = CAL_BACKEND (object);
	priv = backend->priv;

	switch (property_id) {
	case PROP_URI:
		g_value_set_string (value, cal_backend_get_uri (backend));
		break;
	case PROP_KIND:
		g_value_set_ulong (value, cal_backend_get_kind (backend));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

/* Class initialization function for the calendar backend */
static void
cal_backend_class_init (CalBackendClass *class)
{
	GObjectClass *object_class;

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class = (GObjectClass *) class;

	object_class->set_property = cal_backend_set_property;
	object_class->get_property = cal_backend_get_property;
	object_class->finalize = cal_backend_finalize;

	g_object_class_install_property (object_class, PROP_URI, 
					 g_param_spec_string ("uri", NULL, NULL, "",
							      G_PARAM_READABLE | G_PARAM_WRITABLE
							      | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_KIND, 
					 g_param_spec_ulong ("kind", NULL, NULL, 
							     ICAL_NO_COMPONENT, ICAL_XLICMIMEPART_COMPONENT, 
							     ICAL_NO_COMPONENT,
							     G_PARAM_READABLE | G_PARAM_WRITABLE
							     | G_PARAM_CONSTRUCT_ONLY));	
	cal_backend_signals[LAST_CLIENT_GONE] =
		g_signal_new ("last_client_gone",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, last_client_gone),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	cal_backend_signals[OPENED] =
		g_signal_new ("opened",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, opened),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);
	cal_backend_signals[REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	class->last_client_gone = NULL;
	class->opened = NULL;
	class->obj_updated = NULL;

	class->get_cal_address = NULL;
	class->get_alarm_email_address = NULL;
	class->get_static_capabilities = NULL;
	class->open = NULL;
	class->is_loaded = NULL;
	class->is_read_only = NULL;
	class->start_query = NULL;
	class->get_mode = NULL;
	class->set_mode = NULL;	
	class->get_object = NULL;
	class->get_default_object = NULL;
	class->get_object_list = NULL;
	class->get_free_busy = NULL;
	class->get_changes = NULL;
	class->discard_alarm = NULL;
	class->create_object = NULL;
	class->modify_object = NULL;
	class->remove_object = NULL;
	class->receive_objects = NULL;
	class->send_objects = NULL;
	class->get_timezone = NULL;
	class->add_timezone = NULL;
	class->set_default_timezone = NULL;
}

/* Object initialization func for the calendar backend */
void
cal_backend_init (CalBackend *backend)
{
	CalBackendPrivate *priv;

	priv = g_new0 (CalBackendPrivate, 1);
	backend->priv = priv;

	priv->clients = NULL;
	priv->clients_mutex = g_mutex_new ();

	/* FIXME bonobo_object_ref/unref? */
	priv->queries = e_list_new((EListCopyFunc) g_object_ref, (EListFreeFunc) g_object_unref, NULL);
	priv->queries_mutex = g_mutex_new ();
	
	priv->categories = g_hash_table_new (g_str_hash, g_str_equal);
	priv->changed_categories = g_hash_table_new (g_str_hash, g_str_equal);
}

/* Used from g_hash_table_foreach(), frees a CalBackendCategory structure */
static void
free_category_cb (gpointer key, gpointer value, gpointer data)
{
	CalBackendCategory *c = value;

	g_free (c->name);
	g_free (c);
}

static gboolean
prune_changed_categories (gpointer key, gpointer value, gpointer data)
{
	CalBackendCategory *c = value;

	if (!c->refcount)
		free_category_cb (key, value, data);
	return TRUE;
}

void
cal_backend_finalize (GObject *object)
{
	CalBackend *backend = (CalBackend *)object;
	CalBackendPrivate *priv;

	priv = backend->priv;

	g_assert (priv->clients == NULL);

	g_object_unref (priv->queries);

	g_hash_table_foreach_remove (priv->changed_categories, prune_changed_categories, NULL);
	g_hash_table_destroy (priv->changed_categories);

	g_hash_table_foreach (priv->categories, free_category_cb, NULL);
	g_hash_table_destroy (priv->categories);

	g_mutex_free (priv->clients_mutex);
	g_mutex_free (priv->queries_mutex);

	if (priv->category_idle_id)
		g_source_remove (priv->category_idle_id);

	g_free (priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}



/**
 * cal_backend_get_uri:
 * @backend: A calendar backend.
 *
 * Queries the URI of a calendar backend, which must already have an open
 * calendar.
 *
 * Return value: The URI where the calendar is stored.
 **/
const char *
cal_backend_get_uri (CalBackend *backend)
{
	CalBackendPrivate *priv;
	
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = backend->priv;
	
	return priv->uri;
}

icalcomponent_kind
cal_backend_get_kind (CalBackend *backend)
{
	CalBackendPrivate *priv;
	
	g_return_val_if_fail (backend != NULL, ICAL_NO_COMPONENT);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), ICAL_NO_COMPONENT);

	priv = backend->priv;
	
	return priv->kind;
}

static void
cal_destroy_cb (gpointer data, GObject *where_cal_was)
{
	CalBackend *backend = CAL_BACKEND (data);

	cal_backend_remove_client (backend, (Cal *) where_cal_was);
}

static void
listener_died_cb (gpointer cnx, gpointer data)
{
	Cal *cal = CAL (data);

	cal_backend_remove_client (cal_get_backend (cal), cal);
}

static void
last_client_gone (CalBackend *backend)
{
	g_signal_emit (backend, cal_backend_signals[LAST_CLIENT_GONE], 0);
}

void
cal_backend_add_client (CalBackend *backend, Cal *cal)
{
	CalBackendPrivate *priv;
	
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = backend->priv;
	
	bonobo_object_set_immortal (BONOBO_OBJECT (cal), TRUE);

	g_object_weak_ref (G_OBJECT (cal), cal_destroy_cb, backend);

	ORBit_small_listen_for_broken (cal_get_listener (cal), G_CALLBACK (listener_died_cb), cal);

	g_mutex_lock (priv->clients_mutex);
	priv->clients = g_list_append (priv->clients, cal);
	g_mutex_unlock (priv->clients_mutex);

	/* Tell the new client about the list of categories.
	 * (Ends up telling all the other clients too, but *shrug*.)
	 */
	/* FIXME This doesn't seem right at all */
	notify_categories_changed (backend);
}

void
cal_backend_remove_client (CalBackend *backend, Cal *cal)
{
	CalBackendPrivate *priv;
	
	/* XXX this needs a bit more thinking wrt the mutex - we
	   should be holding it when we check to see if clients is
	   NULL */
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	priv = backend->priv;

	/* Disconnect */
	g_mutex_lock (priv->clients_mutex);
	priv->clients = g_list_remove (priv->clients, cal);
	g_mutex_unlock (priv->clients_mutex);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!priv->clients)
		last_client_gone (backend);
}

void
cal_backend_add_query (CalBackend *backend, Query *query)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_mutex_lock (backend->priv->queries_mutex);

	e_list_append (backend->priv->queries, query);
	
	g_mutex_unlock (backend->priv->queries_mutex);
}

EList *
cal_backend_get_queries (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	return g_object_ref (backend->priv->queries);
}


/**
 * cal_backend_get_cal_address:
 * @backend: A calendar backend.
 *
 * Queries the cal address associated with a calendar backend, which
 * must already have an open calendar.
 *
 * Return value: The cal address associated with the calendar.
 **/
void
cal_backend_get_cal_address (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_cal_address != NULL);
	(* CLASS (backend)->get_cal_address) (backend, cal);
}

void
cal_backend_get_alarm_email_address (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_alarm_email_address != NULL);
	(* CLASS (backend)->get_alarm_email_address) (backend, cal);
}

void
cal_backend_get_ldap_attribute (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_ldap_attribute != NULL);
	(* CLASS (backend)->get_ldap_attribute) (backend, cal);
}

void
cal_backend_get_static_capabilities (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_static_capabilities != NULL);
	(* CLASS (backend)->get_static_capabilities) (backend, cal);
}

/**
 * cal_backend_open:
 * @backend: A calendar backend.
 * @uristr: URI that contains the calendar data.
 * @only_if_exists: Whether the calendar should be opened only if it already
 * exists.  If FALSE, a new calendar will be created when the specified @uri
 * does not exist.
 *
 * Opens a calendar backend with data from a calendar stored at the specified
 * URI.
 *
 * Return value: An operation status code.
 **/
void
cal_backend_open (CalBackend *backend, Cal *cal, gboolean only_if_exists)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->open != NULL);
	(* CLASS (backend)->open) (backend, cal, only_if_exists);
}

void
cal_backend_remove (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->remove != NULL);
	(* CLASS (backend)->remove) (backend, cal);
}

/**
 * cal_backend_is_loaded:
 * @backend: A calendar backend.
 * 
 * Queries whether a calendar backend has been loaded yet.
 * 
 * Return value: TRUE if the backend has been loaded with data, FALSE
 * otherwise.
 **/
gboolean
cal_backend_is_loaded (CalBackend *backend)
{
	gboolean result;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	g_assert (CLASS (backend)->is_loaded != NULL);
	result = (* CLASS (backend)->is_loaded) (backend);

	return result;
}

/**
 * cal_backend_is_read_only
 * @backend: A calendar backend.
 *
 * Queries whether a calendar backend is read only or not.
 *
 * Return value: TRUE if the calendar is read only, FALSE otherwise.
 */
void
cal_backend_is_read_only (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->is_read_only != NULL);
	(* CLASS (backend)->is_read_only) (backend, cal);
}

void 
cal_backend_start_query (CalBackend *backend, Query *query)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->start_query != NULL);
	(* CLASS (backend)->start_query) (backend, query);
}

/**
 * cal_backend_get_mode:
 * @backend: A calendar backend. 
 * 
 * Queries whether a calendar backend is connected remotely.
 * 
 * Return value: The current mode the calendar is in
 **/
CalMode
cal_backend_get_mode (CalBackend *backend)
{
	CalMode result;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	g_assert (CLASS (backend)->get_mode != NULL);
	result = (* CLASS (backend)->get_mode) (backend);

	return result;
}


/**
 * cal_backend_set_mode:
 * @backend: A calendar backend
 * @mode: Mode to change to
 * 
 * Sets the mode of the calendar
 * 
 **/
void
cal_backend_set_mode (CalBackend *backend, CalMode mode)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->set_mode != NULL);
	(* CLASS (backend)->set_mode) (backend, mode);
}

void
cal_backend_get_default_object (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_default_object != NULL);
	(* CLASS (backend)->get_default_object) (backend, cal);
}

/**
 * cal_backend_get_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 * @rid: ID for the object's recurrence to get.
 *
 * Queries a calendar backend for a calendar object based on its unique
 * identifier and its recurrence ID (if a recurrent appointment).
 *
 * Return value: The string representation of a complete calendar wrapping the
 * the sought object, or NULL if no object had the specified UID.
 **/
void
cal_backend_get_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uid != NULL);

	g_assert (CLASS (backend)->get_object != NULL);
	(* CLASS (backend)->get_object) (backend, cal, uid, rid);
}

/**
 * cal_backend_get_object_list:
 * @backend: 
 * @type: 
 * 
 * 
 * 
 * Return value: 
 **/
void
cal_backend_get_object_list (CalBackend *backend, Cal *cal, const char *sexp)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->get_object_list != NULL);
	return (* CLASS (backend)->get_object_list) (backend, cal, sexp);
}

/**
 * cal_backend_get_free_busy:
 * @backend: A calendar backend.
 * @users: List of users to get free/busy information for.
 * @start: Start time for query.
 * @end: End time for query.
 * 
 * Gets a free/busy object for the given time interval
 * 
 * Return value: a list of CalObj's
 **/
void
cal_backend_get_free_busy (CalBackend *backend, Cal *cal, GList *users, time_t start, time_t end)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (start != -1 && end != -1);
	g_return_if_fail (start <= end);

	g_assert (CLASS (backend)->get_free_busy != NULL);
	(* CLASS (backend)->get_free_busy) (backend, cal, users, start, end);
}

/**
 * cal_backend_get_changes:
 * @backend: A calendar backend
 * @change_id: A unique uid for the callers change list
 * 
 * Builds a sequence of objects and the type of change that occurred on them since
 * the last time the give change_id was seen
 * 
 * Return value: A list of the objects that changed and the type of change
 **/
void
cal_backend_get_changes (CalBackend *backend, Cal *cal, const char *change_id) 
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (change_id != NULL);

	g_assert (CLASS (backend)->get_changes != NULL);
	(* CLASS (backend)->get_changes) (backend, cal, change_id);
}

/**
 * cal_backend_discard_alarm
 * @backend: A calendar backend.
 * @uid: UID of the component to discard the alarm from.
 * @auid: Alarm ID.
 *
 * Discards an alarm from the given component. This allows the specific backend
 * to do whatever is needed to really discard the alarm.
 *
 **/
void
cal_backend_discard_alarm (CalBackend *backend, Cal *cal, const char *uid, const char *auid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uid != NULL);
	g_return_if_fail (auid != NULL);

	g_assert (CLASS (backend)->discard_alarm != NULL);
	(* CLASS (backend)->discard_alarm) (backend, cal, uid, auid);
}

void
cal_backend_create_object (CalBackend *backend, Cal *cal, const char *calobj)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (calobj != NULL);

	g_assert (CLASS (backend)->create_object != NULL);
	(* CLASS (backend)->create_object) (backend, cal, calobj);
}

void
cal_backend_modify_object (CalBackend *backend, Cal *cal, const char *calobj, CalObjModType mod)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (calobj != NULL);

	g_assert (CLASS (backend)->modify_object != NULL);
	(* CLASS (backend)->modify_object) (backend, cal, calobj, mod);
}

/**
 * cal_backend_remove_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the object to remove.
 * @rid: A recurrence ID.
 * 
 * Removes an object in a calendar backend.  The backend will notify all of its
 * clients about the change.
 * 
 **/
void
cal_backend_remove_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid, CalObjModType mod)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uid != NULL);

	g_assert (CLASS (backend)->remove_object != NULL);
	(* CLASS (backend)->remove_object) (backend, cal, uid, rid, mod);
}

void
cal_backend_receive_objects (CalBackend *backend, Cal *cal, const char *calobj)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (calobj != NULL);

	g_assert (CLASS (backend)->receive_objects != NULL);
	return (* CLASS (backend)->receive_objects) (backend, cal, calobj);
}

void
cal_backend_send_objects (CalBackend *backend, Cal *cal, const char *calobj)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (calobj != NULL);

	g_assert (CLASS (backend)->send_objects != NULL);
	return (* CLASS (backend)->send_objects) (backend, cal, calobj);
}

/**
 * cal_backend_get_timezone:
 * @backend: A calendar backend.
 * @tzid: Unique identifier of a VTIMEZONE object. Note that this must not be
 * NULL.
 * 
 * Returns the icaltimezone* corresponding to the TZID, or NULL if the TZID
 * can't be found.
 * 
 * Returns: The icaltimezone* corresponding to the given TZID, or NULL.
 **/
void
cal_backend_get_timezone (CalBackend *backend, Cal *cal, const char *tzid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (tzid != NULL);

	g_assert (CLASS (backend)->get_timezone != NULL);
	(* CLASS (backend)->get_timezone) (backend, cal, tzid);
}

/**
 * cal_backend_set_default_timezone:
 * @backend: A calendar backend.
 * @tzid: The TZID identifying the timezone.
 * 
 * Sets the default timezone for the calendar, which is used to resolve
 * DATE and floating DATE-TIME values.
 * 
 * Returns: TRUE if the VTIMEZONE data for the timezone was found, or FALSE if
 * not.
 **/
void
cal_backend_set_default_timezone (CalBackend *backend, Cal *cal, const char *tzid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (tzid != NULL);

	g_assert (CLASS (backend)->set_default_timezone != NULL);
	(* CLASS (backend)->set_default_timezone) (backend, cal, tzid);
}

/**
 * cal_backend_add_timezone
 * @backend: A calendar backend.
 * @tzobj: The timezone object, in a string.
 *
 * Add a timezone object to the given backend.
 *
 * Returns: TRUE if successful, or FALSE if not.
 */
void
cal_backend_add_timezone (CalBackend *backend, Cal *cal, const char *tzobj)
{
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (tzobj != NULL);
	g_return_if_fail (CLASS (backend)->add_timezone != NULL);

	(* CLASS (backend)->add_timezone) (backend, cal, tzobj);
}

icaltimezone *
cal_backend_internal_get_default_timezone (CalBackend *backend)
{
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (CLASS (backend)->internal_get_default_timezone != NULL, NULL);

	return (* CLASS (backend)->internal_get_default_timezone) (backend);
}

icaltimezone *
cal_backend_internal_get_timezone (CalBackend *backend, const char *tzid)
{
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (tzid != NULL, NULL);
	g_return_val_if_fail (CLASS (backend)->internal_get_timezone != NULL, NULL);

	return (* CLASS (backend)->internal_get_timezone) (backend, tzid);
}

/**
 * cal_backend_notify_object_created:
 * @backend: A calendar backend.
 * @calobj: iCalendar representation of new object
 *
 * Notifies each of the backend's listeners about a new object.
 *
 * cal_notify_object_created() calls this for you. You only need to
 * call cal_backend_notify_object_created() yourself to report objects
 * created by non-PCS clients.
 **/
void
cal_backend_notify_object_created (CalBackend *backend, const char *calobj)
{
	EList *queries;
	EIterator *iter;
	Query *query;

	queries = cal_backend_get_queries (backend);
	iter = e_list_get_iterator (queries);

	while (e_iterator_is_valid (iter)) {
		query = QUERY (e_iterator_get (iter));

		bonobo_object_ref (query);
		if (query_object_matches (query, calobj))		
			query_notify_objects_added_1 (query, calobj);
		bonobo_object_unref (query);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);
}

/**
 * cal_backend_notify_object_modified:
 * @backend: A calendar backend.
 * @old_object: iCalendar representation of the original form of the object
 * @object: iCalendar representation of the new form of the object
 *
 * Notifies each of the backend's listeners about a modified object.
 *
 * cal_notify_object_modified() calls this for you. You only need to
 * call cal_backend_notify_object_modified() yourself to report objects
 * modified by non-PCS clients.
 **/
void
cal_backend_notify_object_modified (CalBackend *backend, 
				    const char *old_object, const char *object)
{
	EList *queries;
	EIterator *iter;
	Query *query;
	gboolean old_match, new_match;

	queries = cal_backend_get_queries (backend);
	iter = e_list_get_iterator (queries);

	while (e_iterator_is_valid (iter)) {
		query = QUERY (e_iterator_get (iter));
		
		bonobo_object_ref (query);

		old_match = query_object_matches (query, old_object);
		new_match = query_object_matches (query, object);
		if (old_match && new_match)
			query_notify_objects_modified_1 (query, object);
		else if (new_match)
			query_notify_objects_added_1 (query, object);
		else /* if (old_match) */ {
			icalcomponent *comp;

			comp = icalcomponent_new_from_string ((char *)old_object);
			query_notify_objects_removed_1 (query, icalcomponent_get_uid (comp));
			icalcomponent_free (comp);
		}

		bonobo_object_unref (query);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);
}

/**
 * cal_backend_notify_object_removed:
 * @backend: A calendar backend.
 * @uid: the UID of the removed object
 * @old_object: iCalendar representation of the removed object
 *
 * Notifies each of the backend's listeners about a removed object.
 *
 * cal_notify_object_removed() calls this for you. You only need to
 * call cal_backend_notify_object_removed() yourself to report objects
 * removed by non-PCS clients.
 **/
void
cal_backend_notify_object_removed (CalBackend *backend, const char *uid,
				   const char *old_object)
{
	EList *queries;
	EIterator *iter;
	Query *query;

	queries = cal_backend_get_queries (backend);
	iter = e_list_get_iterator (queries);

	while (e_iterator_is_valid (iter)) {
		query = QUERY (e_iterator_get (iter));

		bonobo_object_ref (query);
		if (query_object_matches (query, old_object))
			query_notify_objects_removed_1 (query, uid);
		bonobo_object_unref (query);

		e_iterator_next (iter);
	}
	g_object_unref (iter);
	g_object_unref (queries);
}

/**
 * cal_backend_notify_mode:
 * @backend: A calendar backend.
 * @status: Status of the mode set
 * @mode: the current mode
 *
 * Notifies each of the backend's listeners about the results of a
 * setMode call.
 **/
void
cal_backend_notify_mode (CalBackend *backend,
			 GNOME_Evolution_Calendar_Listener_SetModeStatus status, 
			 GNOME_Evolution_Calendar_CalMode mode)
{
	CalBackendPrivate *priv = backend->priv;
	GList *l;

	for (l = priv->clients; l; l = l->next)
		cal_notify_mode (l->data, status, mode);
}

/**
 * cal_backend_notify_error:
 * @backend: A calendar backend.
 * @message: Error message
 *
 * Notifies each of the backend's listeners about an error
 **/
void
cal_backend_notify_error (CalBackend *backend, const char *message)
{
	CalBackendPrivate *priv = backend->priv;
	GList *l;

	for (l = priv->clients; l; l = l->next)
		cal_notify_error (l->data, message);
}

static void
add_category_cb (gpointer name, gpointer category, gpointer data)
{
	GNOME_Evolution_Calendar_StringSeq *seq = data;

	seq->_buffer[seq->_length++] = CORBA_string_dup (name);
}

static void
notify_categories_changed (CalBackend *backend)
{
	CalBackendPrivate *priv = backend->priv;
	GNOME_Evolution_Calendar_StringSeq *seq;
	GList *l;

	/* Build the sequence of category names */
	seq = GNOME_Evolution_Calendar_StringSeq__alloc ();
	seq->_length = 0;
	seq->_maximum = g_hash_table_size (priv->categories);
	seq->_buffer = CORBA_sequence_CORBA_string_allocbuf (seq->_maximum);
	CORBA_sequence_set_release (seq, TRUE);

	g_hash_table_foreach (priv->categories, add_category_cb, seq);

	/* Notify the clients */
	for (l = priv->clients; l; l = l->next)
		cal_notify_categories_changed (l->data, seq);

	CORBA_free (seq);
}

static gboolean
idle_notify_categories_changed (gpointer data)
{
	CalBackend *backend = CAL_BACKEND (data);
	CalBackendPrivate *priv = backend->priv;

	if (g_hash_table_size (priv->changed_categories)) {
		notify_categories_changed (backend);
		g_hash_table_foreach_remove (priv->changed_categories, prune_changed_categories, NULL);
	}

	priv->category_idle_id = 0;
	
	return FALSE;
}

/**
 * cal_backend_ref_categories:
 * @backend: A calendar backend
 * @categories: a list of categories
 *
 * Adds 1 to the refcount of each of the named categories. If any of
 * the categories are new, clients will be notified of the updated
 * category list at idle time.
 **/
void
cal_backend_ref_categories (CalBackend *backend, GSList *categories)
{
	CalBackendPrivate *priv;
	CalBackendCategory *c;
	const char *name;

	priv = backend->priv;

	while (categories) {
		name = categories->data;
		c = g_hash_table_lookup (priv->categories, name);

		if (c)
			c->refcount++;
		else {
			/* See if it was recently removed */

			c = g_hash_table_lookup (priv->changed_categories, name);
			if (c && c->refcount == 0) {
				/* Move it back to the set of live categories */
				g_hash_table_remove (priv->changed_categories, c->name);

				c->refcount = 1;
				g_hash_table_insert (priv->categories, c->name, c);
			} else {
				/* Create a new category */
				c = g_new (CalBackendCategory, 1);
				c->name = g_strdup (name);
				c->refcount = 1;
				g_hash_table_insert (priv->categories, c->name, c);
				g_hash_table_insert (priv->changed_categories, c->name, c);
			}
		}

		categories = categories->next;
	}

	if (g_hash_table_size (priv->changed_categories) &&
	    !priv->category_idle_id)
		priv->category_idle_id = g_idle_add (idle_notify_categories_changed, backend);
}

/**
 * cal_backend_unref_categories:
 * @backend: A calendar backend
 * @categories: a list of categories
 *
 * Subtracts 1 from the refcount of each of the named categories. If
 * any of the refcounts go down to 0, clients will be notified of the
 * updated category list at idle time.
 **/
void
cal_backend_unref_categories (CalBackend *backend, GSList *categories)
{
	CalBackendPrivate *priv;
	CalBackendCategory *c;
	const char *name;

	priv = backend->priv;

	while (categories) {
		name = categories->data;
		c = g_hash_table_lookup (priv->categories, name);

		if (c) {
			g_assert (c != NULL);
			g_assert (c->refcount > 0);

			c->refcount--;

			if (c->refcount == 0) {
				g_hash_table_remove (priv->categories, c->name);
				g_hash_table_insert (priv->changed_categories, c->name, c);
			}
		}

		categories = categories->next;
	}

	if (g_hash_table_size (priv->changed_categories) &&
	    !priv->category_idle_id)
		priv->category_idle_id = g_idle_add (idle_notify_categories_changed, backend);
}

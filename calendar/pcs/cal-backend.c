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
	/* List of Cal objects with their listeners */
	GList *clients;

	/* Hash table of live categories, temporary hash of
	 * added/removed categories, and idle handler for sending
	 * category_changed.
	 */
	GHashTable *categories;
	GHashTable *changed_categories;
	guint category_idle_id;
};

/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	CAL_ADDED,
	OPENED,
	OBJ_UPDATED,
	OBJ_REMOVED,
	LAST_SIGNAL
};
static guint cal_backend_signals[LAST_SIGNAL];

static void cal_backend_class_init (CalBackendClass *class);
static void cal_backend_init (CalBackend *backend);
static void cal_backend_finalize (GObject *object);

static char *get_object (CalBackend *backend, const char *uid);

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

/* Class initialization function for the calendar backend */
static void
cal_backend_class_init (CalBackendClass *class)
{
	GObjectClass *object_class;

	parent_class = (GObjectClass *) g_type_class_peek_parent (class);

	object_class = (GObjectClass *) class;

	cal_backend_signals[LAST_CLIENT_GONE] =
		g_signal_new ("last_client_gone",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, last_client_gone),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	cal_backend_signals[CAL_ADDED] =
		g_signal_new ("cal_added",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, cal_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
	cal_backend_signals[OPENED] =
		g_signal_new ("opened",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, opened),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);
	cal_backend_signals[OBJ_UPDATED] =
		g_signal_new ("obj_updated",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, obj_updated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	cal_backend_signals[OBJ_REMOVED] =
		g_signal_new ("obj_removed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalBackendClass, obj_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	object_class->finalize = cal_backend_finalize;

	class->last_client_gone = NULL;
	class->opened = NULL;
	class->obj_updated = NULL;
	class->obj_removed = NULL;

	class->get_uri = NULL;
	class->get_cal_address = NULL;
	class->get_alarm_email_address = NULL;
	class->get_static_capabilities = NULL;
	class->open = NULL;
	class->is_loaded = NULL;
	class->is_read_only = NULL;
	class->get_query = NULL;
	class->get_mode = NULL;
	class->set_mode = NULL;	
	class->get_n_objects = NULL;
	class->get_object = get_object;
	class->get_object_component = NULL;
	class->get_timezone_object = NULL;
	class->get_uids = NULL;
	class->get_objects_in_range = NULL;
	class->get_free_busy = NULL;
	class->get_changes = NULL;
	class->get_alarms_in_range = NULL;
	class->get_alarms_for_object = NULL;
	class->discard_alarm = NULL;
	class->update_objects = NULL;
	class->remove_object = NULL;
	class->send_object = NULL;
}

/* Object initialization func for the calendar backend */
void
cal_backend_init (CalBackend *backend)
{
	CalBackendPrivate *priv;

	priv = g_new0 (CalBackendPrivate, 1);
	backend->priv = priv;

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

	g_hash_table_foreach_remove (priv->changed_categories, prune_changed_categories, NULL);
	g_hash_table_destroy (priv->changed_categories);

	g_hash_table_foreach (priv->categories, free_category_cb, NULL);
	g_hash_table_destroy (priv->categories);

	if (priv->category_idle_id)
		g_source_remove (priv->category_idle_id);

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
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_uri != NULL);
	return (* CLASS (backend)->get_uri) (backend);
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
const char *
cal_backend_get_cal_address (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_cal_address != NULL);
	return (* CLASS (backend)->get_cal_address) (backend);
}

const char *
cal_backend_get_alarm_email_address (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_alarm_email_address != NULL);
	return (* CLASS (backend)->get_alarm_email_address) (backend);
}

const char *
cal_backend_get_ldap_attribute (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_ldap_attribute != NULL);
	return (* CLASS (backend)->get_ldap_attribute) (backend);
}

const char *
cal_backend_get_static_capabilities (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_static_capabilities != NULL);
	return (* CLASS (backend)->get_static_capabilities) (backend);
}

/* Callback used when a Cal is destroyed */
static void
cal_destroy_cb (gpointer data, GObject *where_cal_was)
{
	CalBackend *backend = CAL_BACKEND (data);
	CalBackendPrivate *priv = backend->priv;

	priv->clients = g_list_remove (priv->clients, where_cal_was);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!priv->clients)
		cal_backend_last_client_gone (backend);
}

/**
 * cal_backend_add_cal:
 * @backend: A calendar backend.
 * @cal: A calendar client interface object.
 *
 * Adds a calendar client interface object to a calendar @backend.
 * The calendar backend must already have an open calendar.
 **/
void
cal_backend_add_cal (CalBackend *backend, Cal *cal)
{
	CalBackendPrivate *priv = backend->priv;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (IS_CAL (cal));

	/* we do not keep a (strong) reference to the Cal since the
	 * Calendar user agent owns it */
	g_object_weak_ref (G_OBJECT (cal), cal_destroy_cb, backend);

	priv->clients = g_list_prepend (priv->clients, cal);

	/* Tell the new client about the list of categories.
	 * (Ends up telling all the other clients too, but *shrug*.)
	 */
	notify_categories_changed (backend);

	/* notify backend that a new Cal has been added */
	g_signal_emit (backend, cal_backend_signals[CAL_ADDED], 0, cal);
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
CalBackendOpenStatus
cal_backend_open (CalBackend *backend, const char *uristr, gboolean only_if_exists)
{
	CalBackendOpenStatus result;

	g_return_val_if_fail (backend != NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (uristr != NULL, CAL_BACKEND_OPEN_ERROR);

	g_assert (CLASS (backend)->open != NULL);
	result = (* CLASS (backend)->open) (backend, uristr, only_if_exists);

	return result;
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
gboolean
cal_backend_is_read_only (CalBackend *backend)
{
	gboolean result;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	g_assert (CLASS (backend)->is_read_only != NULL);
	result = (* CLASS (backend)->is_read_only) (backend);

	return result;	
}

/**
 * cal_backend_get_query:
 * @backend: A calendar backend.
 * @ql: The query listener.
 * @sexp: Search expression.
 *
 * Create a query object for this backend.
 */
Query *
cal_backend_get_query (CalBackend *backend,
		       GNOME_Evolution_Calendar_QueryListener ql,
		       const char *sexp)
{
	Query *result;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	if (CLASS (backend)->get_query != NULL)
		result = (* CLASS (backend)->get_query) (backend, ql, sexp);
	else
		result = query_new (backend, ql, sexp);

	return result;
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

/**
 * cal_backend_get_n_objects:
 * @backend: A calendar backend.
 * @type: Types of objects that will be included in the count.
 * 
 * Queries the number of calendar objects of a particular type.
 * 
 * Return value: Number of objects of the specified @type.
 **/
int
cal_backend_get_n_objects (CalBackend *backend, CalObjType type)
{
	g_return_val_if_fail (backend != NULL, -1);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), -1);

	g_assert (CLASS (backend)->get_n_objects != NULL);
	return (* CLASS (backend)->get_n_objects) (backend, type);
}

/* Default cal_backend_get_object implementation */
static char *
get_object (CalBackend *backend, const char *uid)
{
	CalComponent *comp;

	comp = cal_backend_get_object_component (backend, uid);
	if (!comp)
		return NULL;

	return cal_component_get_as_string (comp);
}

char *
cal_backend_get_default_object (CalBackend *backend, CalObjType type)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_default_object != NULL);
	return (* CLASS (backend)->get_default_object) (backend, type);
}

/**
 * cal_backend_get_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 *
 * Queries a calendar backend for a calendar object based on its unique
 * identifier.
 *
 * Return value: The string representation of a complete calendar wrapping the
 * the sought object, or NULL if no object had the specified UID.
 **/
char *
cal_backend_get_object (CalBackend *backend, const char *uid)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	g_assert (CLASS (backend)->get_object != NULL);
	return (* CLASS (backend)->get_object) (backend, uid);
}

/**
 * cal_backend_get_object_component:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 *
 * Queries a calendar backend for a calendar object based on its unique
 * identifier. It returns the CalComponent rather than the string
 * representation.
 *
 * Return value: The CalComponent of the sought object, or NULL if no object
 * had the specified UID.
 **/
CalComponent *
cal_backend_get_object_component (CalBackend *backend, const char *uid)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	g_assert (CLASS (backend)->get_object_component != NULL);
	return (* CLASS (backend)->get_object_component) (backend, uid);
}

/**
 * cal_backend_get_timezone_object:
 * @backend: A calendar backend.
 * @tzid: Unique identifier for a calendar VTIMEZONE object.
 *
 * Queries a calendar backend for a VTIMEZONE calendar object based on its
 * unique TZID identifier.
 *
 * Return value: The string representation of a VTIMEZONE component, or NULL
 * if no VTIMEZONE object had the specified TZID.
 **/
char *
cal_backend_get_timezone_object (CalBackend *backend, const char *tzid)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (tzid != NULL, NULL);

	g_assert (CLASS (backend)->get_timezone_object != NULL);
	return (* CLASS (backend)->get_timezone_object) (backend, tzid);
}

/**
 * cal_backend_get_type_by_uid
 * @backend: A calendar backend.
 * @uid: Unique identifier for a Calendar object.
 *
 * Returns the type of the object identified by the @uid argument
 */
CalObjType
cal_backend_get_type_by_uid (CalBackend *backend, const char *uid)
{
	icalcomponent *icalcomp;
	char *comp_str;
	CalObjType type = CAL_COMPONENT_NO_TYPE;

	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail (uid != NULL, CAL_COMPONENT_NO_TYPE);

	comp_str = cal_backend_get_object (backend, uid);
	if (!comp_str)
		return CAL_COMPONENT_NO_TYPE;

	icalcomp = icalparser_parse_string (comp_str);
	if (icalcomp) {
		switch (icalcomponent_isa (icalcomp)) {
		case ICAL_VEVENT_COMPONENT :
			type = CALOBJ_TYPE_EVENT;
			break;
		case ICAL_VTODO_COMPONENT :
			type = CALOBJ_TYPE_TODO;
			break;
		case ICAL_VJOURNAL_COMPONENT :
			type = CALOBJ_TYPE_JOURNAL;
			break;
		default :
			type = CAL_COMPONENT_NO_TYPE;
		}

		icalcomponent_free (icalcomp);
	}

	g_free (comp_str);

	return type;
}

/**
 * cal_backend_get_uids:
 * @backend: A calendar backend.
 * @type: Bitmask with types of objects to return.
 *
 * Builds a list of unique identifiers corresponding to calendar objects whose
 * type matches one of the types specified in the @type flags.
 *
 * Return value: A list of strings that are the sought UIDs.  The list should be
 * freed using the cal_obj_uid_list_free() function.
 **/
GList *
cal_backend_get_uids (CalBackend *backend, CalObjType type)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_uids != NULL);
	return (* CLASS (backend)->get_uids) (backend, type);
}


/**
 * cal_backend_get_objects_in_range:
 * @backend: A calendar backend.
 * @type: Bitmask with types of objects to return.
 * @start: Start time for query.
 * @end: End time for query.
 * 
 * Builds a list of unique identifiers corresponding to calendar objects of the
 * specified type that occur or recur within the specified time range.
 * 
 * Return value: A list of UID strings.  The list should be freed using the
 * cal_obj_uid_list_free() function.
 **/
GList *
cal_backend_get_objects_in_range (CalBackend *backend, CalObjType type,
				  time_t start, time_t end)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	g_assert (CLASS (backend)->get_objects_in_range != NULL);
	return (* CLASS (backend)->get_objects_in_range) (backend, type, start, end);
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
GList *
cal_backend_get_free_busy (CalBackend *backend, GList *users, time_t start, time_t end)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	g_assert (CLASS (backend)->get_free_busy != NULL);
	return (* CLASS (backend)->get_free_busy) (backend, users, start, end);
}

/**
 * cal_backend_get_changes:
 * @backend: A calendar backend
 * @type: Bitmask with types of objects to return.
 * @change_id: A unique uid for the callers change list
 * 
 * Builds a sequence of objects and the type of change that occurred on them since
 * the last time the give change_id was seen
 * 
 * Return value: A list of the objects that changed and the type of change
 **/
GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_get_changes (CalBackend *backend, CalObjType type, const char *change_id) 
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (change_id != NULL, NULL);

	g_assert (CLASS (backend)->get_changes != NULL);
	return (* CLASS (backend)->get_changes) (backend, type, change_id);
}

/**
 * cal_backend_get_alarms_in_range:
 * @backend: A calendar backend.
 * @start: Start time for query.
 * @end: End time for query.
 * @valid_range: Return value that says whether the range is valid or not.
 * 
 * Builds a sorted list of the alarms that trigger in the specified time range.
 * 
 * Return value: A sequence of component alarm instances structures, or NULL
 * if @valid_range returns FALSE.
 **/
GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
cal_backend_get_alarms_in_range (CalBackend *backend, time_t start, time_t end,
				 gboolean *valid_range)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (valid_range != NULL, NULL);

	g_assert (CLASS (backend)->get_alarms_in_range != NULL);

	if (!(start != -1 && end != -1 && start <= end)) {
		*valid_range = FALSE;
		return NULL;
	} else {
		*valid_range = TRUE;
		return (* CLASS (backend)->get_alarms_in_range) (backend, start, end);
	}
}

/**
 * cal_backend_get_alarms_for_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 * @start: Start time for query.
 * @end: End time for query.
 * @result: Return value for the result code for the operation.
 * 
 * Builds a sorted list of the alarms of the specified event that trigger in a
 * particular time range.
 * 
 * Return value: A structure of the component's alarm instances, or NULL if @result
 * returns something other than #CAL_BACKEND_GET_ALARMS_SUCCESS.
 **/
GNOME_Evolution_Calendar_CalComponentAlarms *
cal_backend_get_alarms_for_object (CalBackend *backend, const char *uid,
				   time_t start, time_t end,
				   CalBackendGetAlarmsForObjectResult *result)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (uid != NULL, NULL);
	g_return_val_if_fail (result != NULL, NULL);

	g_assert (CLASS (backend)->get_alarms_for_object != NULL);

	if (!(start != -1 && end != -1 && start <= end)) {
		*result = CAL_BACKEND_GET_ALARMS_INVALID_RANGE;
		return NULL;
	} else {
		gboolean object_found;
		GNOME_Evolution_Calendar_CalComponentAlarms *alarms;

		alarms = (* CLASS (backend)->get_alarms_for_object) (backend, uid, start, end,
								     &object_found);

		if (object_found)
			*result = CAL_BACKEND_GET_ALARMS_SUCCESS;
		else
			*result = CAL_BACKEND_GET_ALARMS_NOT_FOUND;

		return alarms;
	}
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
 * Return value: a #CalBackendResult value, which indicates the
 * result of the operation.
 **/
CalBackendResult
cal_backend_discard_alarm (CalBackend *backend, const char *uid, const char *auid)
{
	g_return_val_if_fail (backend != NULL, CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (uid != NULL, CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (auid != NULL, CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (CLASS (backend)->discard_alarm != NULL, CAL_BACKEND_RESULT_NOT_FOUND);

	return (* CLASS (backend)->discard_alarm) (backend, uid, auid);
}

/**
 * cal_backend_update_objects:
 * @backend: A calendar backend.
 * @calobj: String representation of the new calendar object(s).
 * 
 * Updates an object in a calendar backend.  It will replace any existing
 * object that has the same UID as the specified one.  The backend will in
 * turn notify all of its clients about the change.
 * 
 * Return value: a #CalBackendResult value, which indicates the
 * result of the operation.
 **/
CalBackendResult
cal_backend_update_objects (CalBackend *backend, const char *calobj, CalObjModType mod)
{
	g_return_val_if_fail (backend != NULL, CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (calobj != NULL, CAL_BACKEND_RESULT_NOT_FOUND);

	g_assert (CLASS (backend)->update_objects != NULL);
	return (* CLASS (backend)->update_objects) (backend, calobj, mod);
}

/**
 * cal_backend_remove_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the object to remove.
 * 
 * Removes an object in a calendar backend.  The backend will notify all of its
 * clients about the change.
 * 
 * Return value: a #CalBackendResult value, which indicates the
 * result of the operation.
 **/
CalBackendResult
cal_backend_remove_object (CalBackend *backend, const char *uid, CalObjModType mod)
{
	g_return_val_if_fail (backend != NULL, CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_RESULT_NOT_FOUND);
	g_return_val_if_fail (uid != NULL, CAL_BACKEND_RESULT_NOT_FOUND);

	g_assert (CLASS (backend)->remove_object != NULL);
	return (* CLASS (backend)->remove_object) (backend, uid, mod);
}

CalBackendSendResult
cal_backend_send_object (CalBackend *backend, const char *calobj, char **new_calobj,
			 GNOME_Evolution_Calendar_UserList **user_list, char error_msg[256])
{
	g_return_val_if_fail (backend != NULL, CAL_BACKEND_SEND_INVALID_OBJECT);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_SEND_INVALID_OBJECT);
	g_return_val_if_fail (calobj != NULL, CAL_BACKEND_SEND_INVALID_OBJECT);

	g_assert (CLASS (backend)->send_object != NULL);
	return (* CLASS (backend)->send_object) (backend, calobj, new_calobj, user_list, error_msg);
}

/**
 * cal_backend_last_client_gone:
 * @backend: A calendar backend.
 * 
 * Emits the "last_client_gone" signal of a calendar backend.  This function is
 * to be used only by backend implementations.
 **/
void
cal_backend_last_client_gone (CalBackend *backend)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_signal_emit (G_OBJECT (backend), cal_backend_signals[LAST_CLIENT_GONE], 0);
}

/**
 * cal_backend_opened:
 * @backend: A calendar backend.
 * @status: Open status code.
 * 
 * Emits the "opened" signal of a calendar backend.  This function is to be used
 * only by backend implementations.
 **/
void
cal_backend_opened (CalBackend *backend, CalBackendOpenStatus status)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_signal_emit (G_OBJECT (backend), cal_backend_signals[OPENED],
		       0, status);
}

/**
 * cal_backend_obj_updated:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the component that was updated.
 * 
 * Emits the "obj_updated" signal of a calendar backend.  This function is to be
 * used only by backend implementations.
 **/
void
cal_backend_obj_updated (CalBackend *backend, const char *uid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uid != NULL);

	g_signal_emit (G_OBJECT (backend), cal_backend_signals[OBJ_UPDATED],
		       0, uid);
}

/**
 * cal_backend_obj_removed:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the component that was removed.
 * 
 * Emits the "obj_removed" signal of a calendar backend.  This function is to be
 * used only by backend implementations.
 **/
void
cal_backend_obj_removed (CalBackend *backend, const char *uid)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uid != NULL);

	g_signal_emit (G_OBJECT (backend), cal_backend_signals[OBJ_REMOVED],
		       0, uid);
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
icaltimezone*
cal_backend_get_timezone (CalBackend *backend, const char *tzid)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (tzid != NULL, NULL);

	g_assert (CLASS (backend)->get_timezone != NULL);
	return (* CLASS (backend)->get_timezone) (backend, tzid);
}


/**
 * cal_backend_get_default_timezone:
 * @backend: A calendar backend.
 * 
 * Returns the default timezone for the calendar, which is used to resolve
 * DATE and floating DATE-TIME values.
 * 
 * Returns: The default icaltimezone* for the calendar.
 **/
icaltimezone*
cal_backend_get_default_timezone (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_default_timezone != NULL);
	return (* CLASS (backend)->get_default_timezone) (backend);
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
gboolean
cal_backend_set_default_timezone (CalBackend *backend, const char *tzid)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (tzid != NULL, FALSE);

	g_assert (CLASS (backend)->set_default_timezone != NULL);
	return (* CLASS (backend)->set_default_timezone) (backend, tzid);
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
 * cal_backend_notify_update:
 * @backend: A calendar backend.
 * @uid: UID of object that was updated.
 * 
 * Notifies each of the backend's listeners about an update to a
 * calendar object.
 **/
void
cal_backend_notify_update (CalBackend *backend, const char *uid)
{
	CalBackendPrivate *priv = backend->priv;
	GList *l;

	cal_backend_obj_updated (backend, uid);
	for (l = priv->clients; l; l = l->next)
		cal_notify_update (l->data, uid);
}

/**
 * cal_backend_notify_remove:
 * @backend: A calendar backend.
 * @uid: UID of object that was removed.
 * 
 * Notifies each of the backend's listeners about a calendar object
 * that was removed.
 **/
void
cal_backend_notify_remove (CalBackend *backend, const char *uid)
{
	CalBackendPrivate *priv = backend->priv;
	GList *l;

	cal_backend_obj_removed (backend, uid);
	for (l = priv->clients; l; l = l->next)
		cal_notify_remove (l->data, uid);
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

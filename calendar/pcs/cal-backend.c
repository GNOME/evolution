/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
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
#include <gtk/gtk.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/parserInternals.h>
#include <gnome-xml/xmlmemory.h>

#include "cal-backend.h"
#include "libversit/vcc.h"



/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static void cal_backend_class_init (CalBackendClass *class);
static void cal_backend_init (CalBackend *backend);
static gboolean cal_backend_log_sync (CalBackend *backend);
static GHashTable *cal_backend_get_log_entries (CalBackend *backend, 
						CalObjType type,
						time_t since);

static GtkObjectClass *parent_class;

static guint cal_backend_signals[LAST_SIGNAL];

#define CLASS(backend) (CAL_BACKEND_CLASS (GTK_OBJECT (backend)->klass))



/**
 * cal_backend_get_type:
 * @void:
 *
 * Registers the #CalBackend class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalBackend class.
 **/
GtkType
cal_backend_get_type (void)
{
	static GtkType cal_backend_type = 0;

	if (!cal_backend_type) {
		static const GtkTypeInfo cal_backend_info = {
			"CalBackend",
			sizeof (CalBackend),
			sizeof (CalBackendClass),
			(GtkClassInitFunc) cal_backend_class_init,
			(GtkObjectInitFunc) cal_backend_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_backend_type =
			gtk_type_unique (GTK_TYPE_OBJECT, &cal_backend_info);
	}

	return cal_backend_type;
}

/* Class initialization function for the calendar backend */
static void
cal_backend_class_init (CalBackendClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	cal_backend_signals[LAST_CLIENT_GONE] =
		gtk_signal_new ("last_client_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalBackendClass, last_client_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, cal_backend_signals, LAST_SIGNAL);
}

/* Per instance initialization function */
static void
cal_backend_init (CalBackend *backend)
{
	backend->uri = NULL;
	backend->entries = NULL;
}



/**
 * cal_backend_get_uri:
 * @backend: A calendar backend.
 *
 * Queries the URI of a calendar backend, which must already have a loaded
 * calendar.
 *
 * Return value: The URI where the calendar is stored.
 **/
GnomeVFSURI *
cal_backend_get_uri (CalBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	g_assert (CLASS (backend)->get_uri != NULL);
	return (* CLASS (backend)->get_uri) (backend);
}

static void
cal_backend_set_uri (CalBackend *backend, GnomeVFSURI *uri)
{
	if (backend->uri)
		gnome_vfs_uri_unref (backend->uri);

	if (backend->timer)
		gtk_timeout_remove (backend->timer);


	gnome_vfs_uri_ref (uri);
	backend->uri = uri;
	backend->timer = gtk_timeout_add (60000, 
					  (GtkFunction)cal_backend_log_sync, 
					  backend);
}

/**
 * cal_backend_add_cal:
 * @backend: A calendar backend.
 * @cal: A calendar client interface object.
 *
 * Adds a calendar client interface object to a calendar @backend.
 * The calendar backend must already have a loaded calendar.
 **/
void
cal_backend_add_cal (CalBackend *backend, Cal *cal)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	g_assert (CLASS (backend)->add_cal != NULL);
	(* CLASS (backend)->add_cal) (backend, cal);
}

/**
 * cal_backend_load:
 * @backend: A calendar backend.
 * @uri: URI that contains the calendar data.
 *
 * Loads a calendar backend with data from a calendar stored at the specified
 * URI.
 *
 * Return value: An operation status code.
 **/
CalBackendLoadStatus
cal_backend_load (CalBackend *backend, GnomeVFSURI *uri)
{
	CalBackendLoadStatus result;

	g_return_val_if_fail (backend != NULL, CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_LOAD_ERROR);

	g_assert (CLASS (backend)->load != NULL);
	result =  (* CLASS (backend)->load) (backend, uri);

	/* Remember the URI for saving the log file in the same dir and add
	 * a timeout handler so for saving pending entries sometimes */
	if (result == CAL_BACKEND_LOAD_SUCCESS)
		cal_backend_set_uri (backend, uri);
	
	return result;
}

/**
 * cal_backend_create:
 * @backend: A calendar backend.
 * @uri: URI that will contain the calendar data.
 *
 * Creates a new empty calendar in a calendar backend.
 **/
void
cal_backend_create (CalBackend *backend, GnomeVFSURI *uri)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (uri != NULL);

	g_assert (CLASS (backend)->create != NULL);
	(* CLASS (backend)->create) (backend, uri);

	/* Remember the URI for saving the log file in the same dir and add
	 * a timeout handler so for saving pending entries sometimes */
	cal_backend_set_uri (backend, uri);
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

/**
 * cal_backend_get_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 *
 * Queries a calendar backend for a calendar object based on its unique
 * identifier.
 *
 * Return value: The string representation of a complete calendar wrapping the
 * the sought object, or NULL if no object had the specified UID.  A complete
 * calendar is returned because you also need the timezone data.
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


static void
cal_backend_foreach_changed (gpointer key, gpointer value, gpointer data) 
{
	GList **list = data;
	
	*list = g_list_append (*list, value);
}

/**
 * cal_backend_get_changed_uids:
 * @backend: 
 * @type: 
 * @since: 
 * 
 * 
 * 
 * Return value: 
 **/
GList *
cal_backend_get_changed_uids (CalBackend *backend, CalObjType type, time_t since) 
{
	GHashTable *hash;
	GList *uids = NULL;
	
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	hash = cal_backend_get_log_entries (backend, type, since);

	if (hash)
		g_hash_table_foreach (hash, cal_backend_foreach_changed, &uids);
	
	return uids;
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
 * cal_backend_get_alarms_in_range:
 * @backend: A calendar backend.
 * @start: Start time for query.
 * @end: End time for query.
 * 
 * Builds a sorted list of the alarms that trigger in the specified time range.
 * 
 * Return value: A list of #CalAlarmInstance structures, sorted by trigger time.
 **/
GList *
cal_backend_get_alarms_in_range (CalBackend *backend, time_t start, time_t end)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	g_assert (CLASS (backend)->get_alarms_in_range != NULL);
	return (* CLASS (backend)->get_alarms_in_range) (backend, start, end);
}

/**
 * cal_backend_get_alarms_for_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier for a calendar object.
 * @start: Start time for query.
 * @end: End time for query.
 * @alarms: Return value for the list of alarm instances.
 * 
 * Builds a sorted list of the alarms of the specified event that trigger in a
 * particular time range.
 * 
 * Return value: TRUE on success, FALSE if the object was not found.
 **/
gboolean
cal_backend_get_alarms_for_object (CalBackend *backend, const char *uid,
				   time_t start, time_t end,
				   GList **alarms)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (start != -1 && end != -1, FALSE);
	g_return_val_if_fail (start <= end, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);

	g_assert (CLASS (backend)->get_alarms_for_object != NULL);
	return (* CLASS (backend)->get_alarms_for_object) (backend, uid, start, end, alarms);
}


char *cal_backend_get_uid_by_pilot_id (CalBackend *backend, unsigned long int pilot_id)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_assert (CLASS(backend)->get_uid_by_pilot_id != NULL);
	return (* CLASS(backend)->get_uid_by_pilot_id) (backend, pilot_id);
}


void cal_backend_update_pilot_id (CalBackend *backend, const char *uid,
				  unsigned long int pilot_id,
				  unsigned long int pilot_status)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_assert (CLASS(backend)->update_pilot_id != NULL);
	(* CLASS(backend)->update_pilot_id) (backend, uid,
					     pilot_id, pilot_status);
}

/* Internal logging stuff */
typedef enum {
	CAL_BACKEND_UPDATED,
	CAL_BACKEND_REMOVED
} CalBackendLogEntryType;

typedef struct {
	char *uid;
	CalObjType type;
	
	CalBackendLogEntryType event_type;

	time_t time_stamp;
} CalBackendLogEntry;

typedef struct {
	CalObjType type;
	time_t since;

	gboolean in_valid_timestamp;
	
	GHashTable *hash;
} CalBackendParseState;

static gchar *
cal_backend_log_name (GnomeVFSURI *uri)
{	
	const gchar *path;
	gchar *filename;
	
	path = gnome_vfs_uri_get_path (uri);
	filename = g_strdup_printf ("%s.log.xml", path);

	return filename;
}

static void
cal_backend_set_node_timet (xmlNodePtr node, const char *name, time_t t)
{
	char *tstring;
	
	tstring = g_strdup_printf ("%ld", t);
	xmlSetProp (node, name, tstring);
}

static void
cal_backend_log_entry (CalBackend *backend, const char *uid, 
		       CalBackendLogEntryType type)
{
	CalBackendLogEntry *entry = g_new0 (CalBackendLogEntry, 1);
	CalObjType cot;
	
	g_assert (CLASS (backend)->get_type_by_uid != NULL);
	cot = (* CLASS (backend)->get_type_by_uid) (backend, uid);	

	/* Only log todos and events */
	if (cot != CALOBJ_TYPE_EVENT && cot != CALOBJ_TYPE_TODO)
		return;

	entry = g_new0 (CalBackendLogEntry, 1);
	entry->uid = g_strdup (uid);
	entry->type = cot;
	entry->event_type = type;	
	entry->time_stamp = time (NULL);

	/* Append so they get stored in chronological order */
	backend->entries = g_slist_append (backend->entries, entry);
}

static gboolean
cal_backend_log_sync (CalBackend *backend)
{	
	xmlDocPtr doc;
	xmlNodePtr tnode;
	gchar *filename;
	GSList *l;
	int ret;
	time_t start_time = (time_t) - 1;

	g_return_val_if_fail (backend->uri != NULL, FALSE);
	
	if (backend->entries == NULL)
		return TRUE;
	
	filename = cal_backend_log_name (backend->uri);
	
	doc = xmlParseFile (filename);
	if (doc == NULL) {
		/* Create the document */
		doc = xmlNewDoc ("1.0");
		if (doc == NULL) {
			g_warning ("Log file could not be created\n");
			return FALSE;
		}
		
		
		doc->root = xmlNewDocNode(doc, NULL, "CalendarLog", NULL);
	}

	tnode = xmlNewChild (doc->root, NULL, "timestamp", NULL);
	for (l = backend->entries; l != NULL; l = l->next) {
		xmlNodePtr node;
		CalBackendLogEntry *entry;
		
		entry = (CalBackendLogEntry *)l->data;
		node = xmlNewChild (tnode, NULL, "status", NULL);

		xmlSetProp (node, "uid", entry->uid);
		
		switch (entry->type) {
		case CALOBJ_TYPE_EVENT:
			xmlSetProp (node, "type", "event");
			break;
		case CALOBJ_TYPE_TODO:
			xmlSetProp (node, "type", "todo");
			break;
		default:
		}

		switch (entry->event_type) {
		case (CAL_BACKEND_UPDATED):
			xmlSetProp (node, "operation", "updated");
			break;
		case (CAL_BACKEND_REMOVED):
			xmlSetProp (node, "operation", "removed");
			break;
		}

		if (start_time == (time_t) - 1 
		    || entry->time_stamp < start_time)
			start_time = entry->time_stamp;

		g_free (entry);
	}
	cal_backend_set_node_timet (tnode, "start", start_time);
	cal_backend_set_node_timet (tnode, "end", time (NULL));

	g_slist_free (backend->entries);
	backend->entries = NULL;
	
	/* Write the file */
	xmlSetDocCompressMode (doc, 0);
	ret = xmlSaveFile (filename, doc);
	if (ret < 0) {
		g_warning ("Log file could not be saved\n");
		return FALSE;
	}
	
	xmlFreeDoc (doc);

	g_free (filename);

	return TRUE;
}

static void
cal_backend_log_sax_start_element (CalBackendParseState *state, const CHAR *name, 
				   const CHAR **attrs)
{
	if (!strcmp (name, "timestamp")) {
		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "start")) {
				time_t start = (time_t)strtoul (*val, NULL, 0);
				
				if (start >= state->since) 
					state->in_valid_timestamp = TRUE;
				break;
			}	
			attrs = ++val;
		}		
	}

	if (!strcmp (name, "status")) {
		CalObjChange *coc = g_new0 (CalObjChange, 1);
		CalObjType cot = 0;

		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;

			
			val++;
			if (!strcmp (*attrs, "uid")) 
				coc->uid = g_strdup (*val);
			
			if (!strcmp (*attrs, "type")) {
				if (!strcmp (*val, "event"))
					cot = CALOBJ_TYPE_EVENT;
				else if (!strcmp (*val, "todo"))
					cot = CALOBJ_TYPE_TODO;
			}

			if (!strcmp (*attrs, "operation")) {
				if (!strcmp (*val, "updated"))
					coc->type = CALOBJ_UPDATED;
				else if (!strcmp (*val, "removed"))
					coc->type = CALOBJ_REMOVED;
			}
			
			attrs = ++val;
		}

		if (state->type == CALOBJ_TYPE_ANY || state->type == cot)
			g_hash_table_insert (state->hash, coc->uid, coc);
	}
}

static void
cal_backend_log_sax_end_element (CalBackendParseState *state, const CHAR *name)
{
	if (!strcmp (name, "timestamp")) {
		state->in_valid_timestamp = FALSE;
	}
}

static GHashTable *
cal_backend_get_log_entries (CalBackend *backend, CalObjType type, time_t since)
{
	xmlSAXHandler handler;
	CalBackendParseState state;
	GHashTable *hash;
	char *filename;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (backend->uri != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	if (!cal_backend_log_sync (backend))
		return NULL;

	memset (&handler, 0, sizeof (xmlSAXHandler));
	handler.startElement = (startElementSAXFunc)cal_backend_log_sax_start_element;
	handler.endElement = (endElementSAXFunc)cal_backend_log_sax_end_element;

	hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	state.type = type;
	state.since = since;
	state.in_valid_timestamp = FALSE;
	state.hash = hash;

	filename = cal_backend_log_name (backend->uri);	
	if (xmlSAXUserParseFile (&handler, &state, filename) < 0)
		return NULL;
	
	return hash;
}

/**
 * cal_backend_update_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the object to update.
 * @calobj: String representation of the new calendar object.
 * 
 * Updates an object in a calendar backend.  It will replace any existing
 * object that has the same UID as the specified one.  The backend will in
 * turn notify all of its clients about the change.
 * 
 * Return value: TRUE on success, FALSE on being passed an invalid object or one
 * with an unsupported type.
 **/
gboolean
cal_backend_update_object (CalBackend *backend, const char *uid, const char *calobj)
{
	gboolean result;
	
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	g_assert (CLASS (backend)->update_object != NULL);
	result =  (* CLASS (backend)->update_object) (backend, uid, calobj);

	if (result)
		cal_backend_log_entry (backend, uid, CAL_BACKEND_UPDATED);

	return result;
}

/**
 * cal_backend_remove_object:
 * @backend: A calendar backend.
 * @uid: Unique identifier of the object to remove.
 * 
 * Removes an object in a calendar backend.  The backend will notify all of its
 * clients about the change.
 * 
 * Return value: TRUE on success, FALSE on being passed an UID for an object
 * that does not exist in the backend.
 **/
gboolean
cal_backend_remove_object (CalBackend *backend, const char *uid)
{
	gboolean result;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	g_assert (CLASS (backend)->remove_object != NULL);
	result = (* CLASS (backend)->remove_object) (backend, uid);

	if (result)
		cal_backend_log_entry (backend, uid, CAL_BACKEND_REMOVED);

	return result;
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

	cal_backend_log_sync (backend);

	gtk_signal_emit (GTK_OBJECT (backend), cal_backend_signals[LAST_CLIENT_GONE]);
}





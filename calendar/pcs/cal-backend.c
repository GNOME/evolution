/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
 *          JP Rosevear <jpr@helixcode.com>
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

#include "e-util/e-dbhash.h"
#include "cal-backend.h"
#include "libversit/vcc.h"



/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static void cal_backend_class_init (CalBackendClass *class);
static void cal_backend_init (CalBackend *backend);
static void cal_backend_destroy (GtkObject *object);

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

	object_class->destroy = cal_backend_destroy;

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
}

static void
cal_backend_destroy (GtkObject *object)
{
	CalBackend *backend;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND (object));

	backend = CAL_BACKEND (object);

 	if (backend->uri)
		gnome_vfs_uri_unref (backend->uri);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

	gnome_vfs_uri_ref (uri);
	backend->uri = uri;
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

typedef struct 
{
	CalBackend *backend;
	GList *changes;
	GList *change_ids;
} CalBackendComputeChangesData;

static void
cal_backend_compute_changes_foreach_key (const char *key, gpointer data)
{
	CalBackendComputeChangesData *be_data = data;
	char *calobj = cal_backend_get_object (be_data->backend, key);
	
	if (calobj == NULL) {
		CalComponent *comp;
		GNOME_Evolution_Calendar_CalObjChange *coc;

		comp = cal_component_new ();
		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
		cal_component_set_uid (comp, key);

		coc = GNOME_Evolution_Calendar_CalObjChange__alloc ();
		coc->calobj =  CORBA_string_dup (cal_component_get_as_string (comp));
		coc->type = GNOME_Evolution_Calendar_DELETED;
		be_data->changes = g_list_prepend (be_data->changes, coc);
		be_data->change_ids = g_list_prepend (be_data->change_ids, (gpointer) key);
 	}
}

static GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_compute_changes (CalBackend *backend, CalObjType type, const char *change_id)
{
	char    *filename;
	EDbHash *ehash;
	CalBackendComputeChangesData be_data;
	GNOME_Evolution_Calendar_CalObjChangeSeq *seq;
	GList *uids, *changes = NULL, *change_ids = NULL;
	GList *i, *j;
	int n;
	
	/* Find the changed ids - FIX ME, path should not be hard coded */
	filename = g_strdup_printf ("%s/evolution/local/Calendar/%s.db", g_get_home_dir (), change_id);
	ehash = e_dbhash_new (filename);
	g_free (filename);
	
	uids = cal_backend_get_uids (backend, type);
	
	/* Calculate adds and modifies */
	for (i = uids; i != NULL; i = i->next) {
		GNOME_Evolution_Calendar_CalObjChange *coc;
		char *uid = i->data;
		char *calobj = cal_backend_get_object (backend, uid);

		g_assert (calobj != NULL);

		/* check what type of change has occurred, if any */
		switch (e_dbhash_compare (ehash, uid, calobj)) {
		case E_DBHASH_STATUS_SAME:
			break;
		case E_DBHASH_STATUS_NOT_FOUND:
			coc = GNOME_Evolution_Calendar_CalObjChange__alloc ();
			coc->calobj =  CORBA_string_dup (calobj);
			coc->type = GNOME_Evolution_Calendar_ADDED;
			changes = g_list_prepend (changes, coc);
			change_ids = g_list_prepend (change_ids, uid);
			break;
		case E_DBHASH_STATUS_DIFFERENT:
			coc = GNOME_Evolution_Calendar_CalObjChange__alloc ();
			coc->calobj =  CORBA_string_dup (calobj);
			coc->type = GNOME_Evolution_Calendar_ADDED;
			changes = g_list_append (changes, coc);
			change_ids = g_list_prepend (change_ids, uid);
			break;
		}
	}

	/* Calculate deletions */
	be_data.backend = backend;
	be_data.changes = changes;
	be_data.change_ids = change_ids;
   	e_dbhash_foreach_key (ehash, (EDbHashFunc)cal_backend_compute_changes_foreach_key, &be_data);
	changes = be_data.changes;
	change_ids = be_data.change_ids;
	
	/* Build the sequence and update the hash */
	n = g_list_length (changes);

	seq = GNOME_Evolution_Calendar_CalObjChangeSeq__alloc ();
	seq->_length = n;
	seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObjChange_allocbuf (n);
	CORBA_sequence_set_release (seq, TRUE);

	for (i = changes, j = change_ids, n = 0; i != NULL; i = i->next, j = j->next, n++) {
		GNOME_Evolution_Calendar_CalObjChange *coc = i->data;
		GNOME_Evolution_Calendar_CalObjChange *seq_coc;
		char *uid = j->data;

		/* sequence building */
		seq_coc = &seq->_buffer[n];
		seq_coc->calobj = CORBA_string_dup (coc->calobj);
		seq_coc->type = coc->type;

		/* hash updating */
		if (coc->type == GNOME_Evolution_Calendar_ADDED 
		    || coc->type == GNOME_Evolution_Calendar_MODIFIED) {
			e_dbhash_add (ehash, uid, coc->calobj);
		} else {
			e_dbhash_remove (ehash, uid);
		}		

		CORBA_free (coc);
	}	
  	e_dbhash_write (ehash);
  	e_dbhash_destroy (ehash);

	cal_obj_uid_list_free (uids);
	g_list_free (change_ids);
	g_list_free (changes);
	
	return seq;
}

/**
 * cal_backend_get_changes:
 * @backend: 
 * @type: 
 * @change_id: 
 * 
 * 
 * 
 * Return value: 
 **/
GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_get_changes (CalBackend *backend, CalObjType type, const char *change_id) 
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	return cal_backend_compute_changes (backend, type, change_id);
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
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	g_assert (CLASS (backend)->update_object != NULL);
	return (* CLASS (backend)->update_object) (backend, uid, calobj);
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
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);

	g_assert (CLASS (backend)->remove_object != NULL);
	return (* CLASS (backend)->remove_object) (backend, uid);
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

	gtk_signal_emit (GTK_OBJECT (backend), cal_backend_signals[LAST_CLIENT_GONE]);
}


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
#include <gtk/gtksignal.h>
#include <cal-util/calobj.h>
#include "cal-backend.h"
#include "libversit/vcc.h"



/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static void cal_backend_class_init (CalBackendClass *class);

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
			(GtkObjectInitFunc) NULL,
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
	g_return_val_if_fail (backend != NULL, CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_LOAD_ERROR);

	g_assert (CLASS (backend)->load != NULL);
	return (* CLASS (backend)->load) (backend, uri);
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
 * Return value: A list of strings that are the sought UIDs.
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
 * cal_backend_get_events_in_range:
 * @backend: A calendar backend.
 * @start: Start time for query.
 * @end: End time for query.
 *
 * Builds a sorted list of calendar event object instances that occur or recur
 * within the specified time range.  Each object instance contains the object
 * itself and the start/end times at which it occurs or recurs.
 *
 * Return value: A list of calendar event object instances, sorted by their
 * start times.
 **/
GList *
cal_backend_get_events_in_range (CalBackend *backend, time_t start, time_t end)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	g_assert (CLASS (backend)->get_events_in_range != NULL);
	return (* CLASS (backend)->get_events_in_range) (backend, start, end);
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
 * Return value: TRUE on success, FALSE on being passed an invalid object.
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

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
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
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/parserInternals.h>
#include <gnome-xml/xmlmemory.h>

#include "cal-backend.h"
#include "libversit/vcc.h"



/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	OPENED,
	OBJ_UPDATED,
	OBJ_REMOVED,
	LAST_SIGNAL
};

static void cal_backend_class_init (CalBackendClass *class);

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

	cal_backend_signals[LAST_CLIENT_GONE] =
		gtk_signal_new ("last_client_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalBackendClass, last_client_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	cal_backend_signals[OPENED] =
		gtk_signal_new ("opened",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalBackendClass, opened),
				gtk_marshal_NONE__ENUM,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_ENUM);
	cal_backend_signals[OBJ_UPDATED] =
		gtk_signal_new ("obj_updated",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalBackendClass, obj_updated),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	cal_backend_signals[OBJ_REMOVED] =
		gtk_signal_new ("obj_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalBackendClass, obj_removed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, cal_backend_signals, LAST_SIGNAL);

	class->last_client_gone = NULL;
	class->opened = NULL;
	class->obj_updated = NULL;
	class->obj_removed = NULL;

	class->get_uri = NULL;
	class->add_cal = NULL;
	class->open = NULL;
	class->get_n_objects = NULL;
	class->get_object = NULL;
	class->get_type_by_uid = NULL;
	class->get_uids = NULL;
	class->get_objects_in_range = NULL;
	class->get_changes = NULL;
	class->get_alarms_in_range = NULL;
	class->get_alarms_for_object = NULL;
	class->update_object = NULL;
	class->remove_object = NULL;
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
 * The calendar backend must already have an open calendar.
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
 * cal_backend_open:
 * @backend: A calendar backend.
 * @uri: URI that contains the calendar data.
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
cal_backend_open (CalBackend *backend, GnomeVFSURI *uri, gboolean only_if_exists)
{
	CalBackendOpenStatus result;

	g_return_val_if_fail (backend != NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_OPEN_ERROR);

	g_assert (CLASS (backend)->open != NULL);
	result = (* CLASS (backend)->open) (backend, uri, only_if_exists);

	return result;
}

/**
 * cal_backend_is_loaded:
 * @backend: A calendar backend.
 * 
 * Queries whether a calendar backend has been loaded yet.
 * 
 * Return value: TRUE if the backend has been loaded with data, FALSE otherwise.
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
cal_backend_get_alarms_in_range (CalBackend *backend, time_t start, time_t end, gboolean *valid_range)
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

	gtk_signal_emit (GTK_OBJECT (backend), cal_backend_signals[OPENED],
			 status);
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

	gtk_signal_emit (GTK_OBJECT (backend), cal_backend_signals[OBJ_UPDATED],
			 uid);
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

	gtk_signal_emit (GTK_OBJECT (backend), cal_backend_signals[OBJ_REMOVED],
			 uid);
}

/* Evolution calendar - iCalendar file backend
 *
 * Copyright (C) 2000-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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
#include <string.h>
#include <unistd.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include "e-util/e-xml-hash-utils.h"
#include "cal-util/cal-recur.h"
#include "cal-util/cal-util.h"
#include "cal-backend-file-events.h"
#include "cal-backend-util.h"
#include "cal-backend-object-sexp.h"



/* Placeholder for each component and its recurrences */
typedef struct {
	CalComponent *full_object;
	GHashTable *recurrences;
} CalBackendFileObject;

/* Private part of the CalBackendFile structure */
struct _CalBackendFilePrivate {
	/* URI where the calendar data is stored */
	char *uri;

	/* Filename in the dir */
	char *file_name;	
	gboolean read_only;

	/* Toplevel VCALENDAR component */
	icalcomponent *icalcomp;

	/* All the objects in the calendar, hashed by UID.  The
	 * hash key *is* the uid returned by cal_component_get_uid(); it is not
	 * copied, so don't free it when you remove an object from the hash
	 * table. Each item in the hash table is a CalBackendFileObject.
	 */
	GHashTable *comp_uid_hash;

	GList *comp;
	
	/* Config database handle for free/busy organizer information */
	EConfigListener *config_listener;
	
	/* Idle handler for saving the calendar when it is dirty */
	guint idle_id;

	/* The calendar's default timezone, used for resolving DATE and
	   floating DATE-TIME values. */
	icaltimezone *default_zone;

	/* The list of live queries */
	GList *queries;
};



static void cal_backend_file_dispose (GObject *object);
static void cal_backend_file_finalize (GObject *object);

static CalBackendSyncClass *parent_class;



/* g_hash_table_foreach() callback to destroy recurrences in the hash table */
static void
free_recurrence (gpointer key, gpointer value, gpointer data)
{
	char *rid = key;
	CalComponent *comp = value;

	g_free (rid);
	g_object_unref (comp);
}

/* g_hash_table_foreach() callback to destroy a CalBackendFileObject */
static void
free_object (gpointer key, gpointer value, gpointer data)
{
	CalBackendFileObject *obj_data = value;

	g_object_unref (obj_data->full_object);
	g_hash_table_foreach (obj_data->recurrences, (GHFunc) free_recurrence, NULL);
	g_hash_table_destroy (obj_data->recurrences);
}

/* Saves the calendar data */
static void
save (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;
	GnomeVFSURI *uri, *backup_uri;
	GnomeVFSHandle *handle = NULL;
	GnomeVFSResult result = GNOME_VFS_ERROR_BAD_FILE;
	GnomeVFSFileSize out;
	gchar *tmp, *backup_uristr;
	char *buf;
	
	priv = cbfile->priv;
	g_assert (priv->uri != NULL);
	g_assert (priv->icalcomp != NULL);

	uri = gnome_vfs_uri_new (priv->uri);
	if (!uri)
		goto error_malformed_uri;

	/* save calendar to backup file */
	tmp = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	if (!tmp) {
		gnome_vfs_uri_unref (uri);
		goto error_malformed_uri;
	}
		
	backup_uristr = g_strconcat (tmp, "~", NULL);
	backup_uri = gnome_vfs_uri_new (backup_uristr);

	g_free (tmp);
	g_free (backup_uristr);

	if (!backup_uri) {
		gnome_vfs_uri_unref (uri);
		goto error_malformed_uri;
	}
	
	result = gnome_vfs_create_uri (&handle, backup_uri,
                                       GNOME_VFS_OPEN_WRITE,
                                       FALSE, 0666);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (uri);
		gnome_vfs_uri_unref (backup_uri);
		goto error;
	}

	buf = icalcomponent_as_ical_string (priv->icalcomp);
	result = gnome_vfs_write (handle, buf, strlen (buf) * sizeof (char), &out);
	gnome_vfs_close (handle);
	if (result != GNOME_VFS_OK) {
		gnome_vfs_uri_unref (uri);
		gnome_vfs_uri_unref (backup_uri);
		goto error;
	}

	/* now copy the temporary file to the real file */
	result = gnome_vfs_move_uri (backup_uri, uri, TRUE);

	gnome_vfs_uri_unref (uri);
	gnome_vfs_uri_unref (backup_uri);
	if (result != GNOME_VFS_OK)
		goto error;

	return;

 error_malformed_uri:
	cal_backend_notify_error (CAL_BACKEND (cbfile),
				  _("Can't save calendar data: Malformed URI."));
	return;

 error:
	cal_backend_notify_error (CAL_BACKEND (cbfile), gnome_vfs_result_to_string (result));
	return;
}

/* Dispose handler for the file backend */
static void
cal_backend_file_dispose (GObject *object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (object);
	priv = cbfile->priv;

	/* Save if necessary */

	if (priv->idle_id != 0) {
		save (cbfile);
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	if (priv->comp_uid_hash) {
		g_hash_table_foreach (priv->comp_uid_hash, (GHFunc) free_object, NULL);
		g_hash_table_destroy (priv->comp_uid_hash);
		priv->comp_uid_hash = NULL;
	}

	g_list_free (priv->comp);
	priv->comp = NULL;

	if (priv->icalcomp) {
		icalcomponent_free (priv->icalcomp);
		priv->icalcomp = NULL;
	}

	if (priv->config_listener) {
		g_object_unref (priv->config_listener);
		priv->config_listener = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* Finalize handler for the file backend */
static void
cal_backend_file_finalize (GObject *object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND_FILE (object));

	cbfile = CAL_BACKEND_FILE (object);
	priv = cbfile->priv;

	/* Clean up */

	if (priv->uri) {
	        g_free (priv->uri);
		priv->uri = NULL;
	}

	g_free (priv);
	cbfile->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Looks up a component by its UID on the backend's component hash table */
static CalComponent *
lookup_component (CalBackendFile *cbfile, const char *uid)
{
	CalBackendFilePrivate *priv;
	CalBackendFileObject *obj_data;

	priv = cbfile->priv;

	obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
	return obj_data ? obj_data->full_object : NULL;
}



/* Calendar backend methods */

/* Is_read_only handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_is_read_only (CalBackendSync *backend, Cal *cal, gboolean *read_only)
{
	CalBackendFile *cbfile = backend;

	*read_only = cbfile->priv->read_only;
	
	return GNOME_Evolution_Calendar_Success;
}

/* Get_email_address handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_cal_address (CalBackendSync *backend, Cal *cal, char **address)
{
	/* A file backend has no particular email address associated
	 * with it (although that would be a useful feature some day).
	 */
	*address = NULL;

	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
cal_backend_file_get_ldap_attribute (CalBackendSync *backend, Cal *cal, char **attribute)
{
	*attribute = NULL;
	
	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
cal_backend_file_get_alarm_email_address (CalBackendSync *backend, Cal *cal, char **address)
{
 	/* A file backend has no particular email address associated
 	 * with it (although that would be a useful feature some day).
 	 */
	*address = NULL;
	
	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
cal_backend_file_get_static_capabilities (CalBackendSync *backend, Cal *cal, char **capabilities)
{
	*capabilities = CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS;
	
	return GNOME_Evolution_Calendar_Success;
}

/* function to resolve timezones */
static icaltimezone *
resolve_tzid (const char *tzid, gpointer user_data)
{
	icalcomponent *vcalendar_comp = user_data;

        if (!tzid || !tzid[0])
                return NULL;
        else if (!strcmp (tzid, "UTC"))
                return icaltimezone_get_utc_timezone ();

	return icalcomponent_get_timezone (vcalendar_comp, tzid);
}

/* Idle handler; we save the calendar since it is dirty */
static gboolean
save_idle (gpointer data)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (data);
	priv = cbfile->priv;

	g_assert (priv->icalcomp != NULL);

	save (cbfile);

	priv->idle_id = 0;
	return FALSE;
}

/* Marks the file backend as dirty and queues a save operation */
static void
mark_dirty (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;

	priv = cbfile->priv;

	if (priv->idle_id != 0)
		return;

	priv->idle_id = g_idle_add (save_idle, cbfile);
}

/* Checks if the specified component has a duplicated UID and if so changes it */
static void
check_dup_uid (CalBackendFile *cbfile, CalComponent *comp)
{
	CalBackendFilePrivate *priv;
	CalBackendFileObject *obj_data;
	const char *uid;
	char *new_uid;

	priv = cbfile->priv;

	cal_component_get_uid (comp, &uid);

	obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
	if (!obj_data)
		return; /* Everything is fine */

	g_message ("check_dup_uid(): Got object with duplicated UID `%s', changing it...", uid);

	new_uid = cal_component_gen_uid ();
	cal_component_set_uid (comp, new_uid);
	g_free (new_uid);

	/* FIXME: I think we need to reset the SEQUENCE property and reset the
	 * CREATED/DTSTAMP/LAST-MODIFIED.
	 */

	mark_dirty (cbfile);
}

static char *
get_rid_string (CalComponent *comp)
{
        CalComponentRange range;
        struct icaltimetype tt;
                                                                                   
        cal_component_get_recurid (comp, &range);
        if (!range.datetime.value)
                return "0";
        tt = *range.datetime.value;
        cal_component_free_range (&range);
                                                                                   
        return icaltime_is_valid_time (tt) && !icaltime_is_null_time (tt) ?
                icaltime_as_ical_string (tt) : "0";
}

/* Tries to add an icalcomponent to the file backend.  We only store the objects
 * of the types we support; all others just remain in the toplevel component so
 * that we don't lose them.
 */
static void
add_component (CalBackendFile *cbfile, CalComponent *comp, gboolean add_to_toplevel)
{
	CalBackendFilePrivate *priv;
	CalBackendFileObject *obj_data;
	const char *uid;
	GSList *categories;

	priv = cbfile->priv;

	if (cal_component_is_instance (comp)) { /* FIXME: more checks needed, to detect detached instances */
		char *rid;

		cal_component_get_uid (comp, &uid);

		obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
		if (!obj_data) {
			g_warning (G_STRLOC ": Got an instance of a non-existing component");
			return;
		}

		rid = get_rid_string (comp);
		if (g_hash_table_lookup (obj_data->recurrences, rid)) {
			g_warning (G_STRLOC ": Tried to adding an already existing recurrence");
			return;
		}

		g_hash_table_insert (obj_data->recurrences, g_strdup (rid), comp);
	} else {
		/* Ensure that the UID is unique; some broken implementations spit
		 * components with duplicated UIDs.
		 */
		check_dup_uid (cbfile, comp);
		cal_component_get_uid (comp, &uid);

		obj_data = g_new0 (CalBackendFileObject, 1);
		obj_data->full_object = comp;
		obj_data->recurrences = g_hash_table_new (g_str_hash, g_str_equal);

		g_hash_table_insert (priv->comp_uid_hash, (gpointer) uid, obj_data);
	}

	priv->comp = g_list_prepend (priv->comp, comp);

	/* Put the object in the toplevel component if required */

	if (add_to_toplevel) {
		icalcomponent *icalcomp;

		icalcomp = cal_component_get_icalcomponent (comp);
		g_assert (icalcomp != NULL);

		icalcomponent_add_component (priv->icalcomp, icalcomp);
	}

	/* Update the set of categories */
	cal_component_get_categories_list (comp, &categories);
	cal_backend_ref_categories (CAL_BACKEND (cbfile), categories);
	cal_component_free_categories_list (categories);
}

/* g_hash_table_foreach() callback to remove recurrences from the calendar */
static void
remove_recurrence_cb (gpointer key, gpointer value, gpointer data)
{
	GList *l;
	GSList *categories;
	icalcomponent *icalcomp;
	CalBackendFilePrivate *priv;
	char *rid = key;
	CalComponent *comp = value;
	CalBackendFile *cbfile = data;

	priv = cbfile->priv;

	/* remove the recurrence from the top-level calendar */
	icalcomp = cal_component_get_icalcomponent (comp);
	g_assert (icalcomp != NULL);

	icalcomponent_remove_component (priv->icalcomp, icalcomp);

	/* remove it from our mapping */
	l = g_list_find (priv->comp, comp);
	priv->comp = g_list_delete_link (priv->comp, l);

	/* update the set of categories */
	cal_component_get_categories_list (comp, &categories);
	cal_backend_unref_categories (CAL_BACKEND (cbfile), categories);
	cal_component_free_categories_list (categories);
}

/* Removes a component from the backend's hash and lists.  Does not perform
 * notification on the clients.  Also removes the component from the toplevel
 * icalcomponent.
 */
static void
remove_component (CalBackendFile *cbfile, CalComponent *comp)
{
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;
	const char *uid;
	GList *l;
	GSList *categories;
	CalBackendFileObject *obj_data;

	priv = cbfile->priv;

	/* Remove the icalcomp from the toplevel */

	icalcomp = cal_component_get_icalcomponent (comp);
	g_assert (icalcomp != NULL);

	icalcomponent_remove_component (priv->icalcomp, icalcomp);

	/* Remove it from our mapping */

	cal_component_get_uid (comp, &uid);
	obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
	if (!obj_data)
		return;

	g_hash_table_remove (priv->comp_uid_hash, uid);

	l = g_list_find (priv->comp, comp);
	g_assert (l != NULL);
	priv->comp = g_list_delete_link (priv->comp, l);

	/* remove the recurrences also */
	g_hash_table_foreach (obj_data->recurrences, (GHFunc) remove_recurrence_cb, cbfile);

	/* Update the set of categories */
	cal_component_get_categories_list (comp, &categories);
	cal_backend_unref_categories (CAL_BACKEND (cbfile), categories);
	cal_component_free_categories_list (categories);

	free_object (uid, obj_data, NULL);
}

/* Scans the toplevel VCALENDAR component and stores the objects it finds */
static void
scan_vcalendar (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;
	icalcompiter iter;

	priv = cbfile->priv;
	g_assert (priv->icalcomp != NULL);
	g_assert (priv->comp_uid_hash != NULL);

	for (iter = icalcomponent_begin_component (priv->icalcomp, ICAL_ANY_COMPONENT);
	     icalcompiter_deref (&iter) != NULL;
	     icalcompiter_next (&iter)) {
		icalcomponent *icalcomp;
		icalcomponent_kind kind;
		CalComponent *comp;

		icalcomp = icalcompiter_deref (&iter);
		
		kind = icalcomponent_isa (icalcomp);

		if (!(kind == ICAL_VEVENT_COMPONENT
		      || kind == ICAL_VTODO_COMPONENT
		      || kind == ICAL_VJOURNAL_COMPONENT))
			continue;

		comp = cal_component_new ();

		if (!cal_component_set_icalcomponent (comp, icalcomp))
			continue;

		add_component (cbfile, comp, FALSE);
	}
}

/* Parses an open iCalendar file and loads it into the backend */
static CalBackendSyncStatus
open_cal (CalBackendFile *cbfile, const char *uristr)
{
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;

	priv = cbfile->priv;

	icalcomp = cal_util_parse_ics_file (uristr);
	if (!icalcomp)
		return GNOME_Evolution_Calendar_OtherError;

	/* FIXME: should we try to demangle XROOT components and
	 * individual components as well?
	 */

	if (icalcomponent_isa (icalcomp) != ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_free (icalcomp);

		return GNOME_Evolution_Calendar_OtherError;
	}

	priv->icalcomp = icalcomp;

	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	scan_vcalendar (cbfile);

	priv->uri = g_strdup (uristr);

	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
create_cal (CalBackendFile *cbfile, const char *uristr)
{
	CalBackendFilePrivate *priv;

	priv = cbfile->priv;

	/* Create the new calendar information */
	priv->icalcomp = cal_util_new_top_level ();

	/* Create our internal data */
	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->uri = g_strdup (uristr);

	mark_dirty (cbfile);

	return GNOME_Evolution_Calendar_Success;
}

static char *
get_uri_string (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	const char *master_uri;
	char *full_uri, *str_uri;
	GnomeVFSURI *uri;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;
	
	master_uri = cal_backend_get_uri (backend);
	g_message (G_STRLOC ": Trying to open %s", master_uri);
	
	/* FIXME Check the error conditions a little more elegantly here */
	if (g_strrstr ("tasks.ics", master_uri) || g_strrstr ("calendar.ics", master_uri)) {
		g_warning (G_STRLOC ": Existing file name %s", master_uri);

		return NULL;
	}
	
	full_uri = g_strdup_printf ("%s%s%s", master_uri, G_DIR_SEPARATOR_S, priv->file_name);
	uri = gnome_vfs_uri_new (full_uri);
	g_free (full_uri);
	
	if (!uri)
		return NULL;

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));
	gnome_vfs_uri_unref (uri);

	if (!str_uri || !strlen (str_uri)) {
		g_free (str_uri);

		return NULL;
	}	

	return str_uri;
}

/* Open handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_open (CalBackendSync *backend, Cal *cal, gboolean only_if_exists)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	char *str_uri;
	CalBackendSyncStatus status;
	
	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	/* Claim a succesful open if we are already open */
	if (priv->uri && priv->comp_uid_hash)
		return GNOME_Evolution_Calendar_Success;
	
	str_uri = get_uri_string (CAL_BACKEND (backend));
	if (!str_uri)
		return GNOME_Evolution_Calendar_OtherError;
	
	if (access (str_uri, R_OK) == 0) {
		status = open_cal (cbfile, str_uri);
		if (access (str_uri, W_OK) != 0)
			priv->read_only = TRUE;
	} else {
		if (only_if_exists)
			status = GNOME_Evolution_Calendar_NoSuchCal;
		else
			status = create_cal (cbfile, str_uri);
	}

	g_free (str_uri);

	return status;
}

static CalBackendSyncStatus
cal_backend_file_remove (CalBackendSync *backend, Cal *cal)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	char *str_uri;
	
	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	str_uri = get_uri_string (CAL_BACKEND (backend));
	if (!str_uri)
		return GNOME_Evolution_Calendar_OtherError;

	if (access (str_uri, W_OK) != 0) {
		g_free (str_uri);

		return GNOME_Evolution_Calendar_PermissionDenied;
	}

	/* FIXME Remove backup file and whole directory too? */
	if (unlink (str_uri) != 0) {
		g_free (str_uri);

		return GNOME_Evolution_Calendar_OtherError;
	}
	
	g_free (str_uri);
	
	return GNOME_Evolution_Calendar_Success;
}

/* is_loaded handler for the file backend */
static gboolean
cal_backend_file_is_loaded (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	return (priv->icalcomp != NULL);
}

/* is_remote handler for the file backend */
static CalMode
cal_backend_file_get_mode (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	return CAL_MODE_LOCAL;	
}

/* Set_mode handler for the file backend */
static void
cal_backend_file_set_mode (CalBackend *backend, CalMode mode)
{
	cal_backend_notify_mode (backend,
				 GNOME_Evolution_Calendar_Listener_MODE_NOT_SUPPORTED,
				 GNOME_Evolution_Calendar_MODE_LOCAL);
	
}

static CalBackendSyncStatus
cal_backend_file_get_default_object (CalBackendSync *backend, Cal *cal, char **object)
{
 	CalComponent *comp;
 	
 	comp = cal_component_new ();

 	switch (cal_backend_get_kind (CAL_BACKEND (backend))) {
 	case ICAL_VEVENT_COMPONENT:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
 		break;
 	case ICAL_VTODO_COMPONENT:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
 		break;
 	case ICAL_VJOURNAL_COMPONENT:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_JOURNAL);
 		break;
 	default:
 		g_object_unref (comp);
		return GNOME_Evolution_Calendar_ObjectNotFound;
 	}
 	
 	*object = cal_component_get_as_string (comp);
 	g_object_unref (comp);
 
	return GNOME_Evolution_Calendar_Success;
}

/* Get_object_component handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_object (CalBackendSync *backend, Cal *cal, const char *uid, const char *rid, char **object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalBackendFileObject *obj_data;
	CalComponent *comp = NULL;
	gboolean free_comp = FALSE;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_InvalidObject);
	g_return_val_if_fail (uid != NULL, GNOME_Evolution_Calendar_ObjectNotFound);
	g_assert (priv->comp_uid_hash != NULL);

	obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
	if (!obj_data)
		return GNOME_Evolution_Calendar_ObjectNotFound;

	if (rid && *rid) {
		comp = g_hash_table_lookup (obj_data->recurrences, rid);
		if (!comp) {
			icalcomponent *icalcomp;
			struct icaltimetype itt;

			itt = icaltime_from_string (rid);
			icalcomp = cal_util_construct_instance (
				cal_component_get_icalcomponent (obj_data->full_object),
				itt);
			if (!icalcomp)
				return GNOME_Evolution_Calendar_ObjectNotFound;

			comp = cal_component_new ();
			free_comp = TRUE;
			cal_component_set_icalcomponent (comp, icalcomp);
		}
	} else
		comp = obj_data->full_object;
	
	if (!comp)
		return GNOME_Evolution_Calendar_ObjectNotFound;

	*object = cal_component_get_as_string (comp);

	if (free_comp)
		g_object_unref (comp);

	return GNOME_Evolution_Calendar_Success;
}

/* Get_timezone_object handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_timezone (CalBackendSync *backend, Cal *cal, const char *tzid, char **object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icaltimezone *zone;
	icalcomponent *icalcomp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (tzid != NULL, GNOME_Evolution_Calendar_ObjectNotFound);

	if (!strcmp (tzid, "UTC")) {
		zone = icaltimezone_get_utc_timezone ();
	} else {
		zone = icalcomponent_get_timezone (priv->icalcomp, tzid);
		if (!zone) {
			zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
			if (!zone)
				return GNOME_Evolution_Calendar_ObjectNotFound;
		}
	}
	
	icalcomp = icaltimezone_get_component (zone);
	if (!icalcomp)
		return GNOME_Evolution_Calendar_InvalidObject;

	*object = g_strdup (icalcomponent_as_ical_string (icalcomp));

	return GNOME_Evolution_Calendar_Success;
}

/* Add_timezone handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_add_timezone (CalBackendSync *backend, Cal *cal, const char *tzobj)
{
	icalcomponent *tz_comp;
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = (CalBackendFile *) backend;

	g_return_val_if_fail (IS_CAL_BACKEND_FILE (cbfile), GNOME_Evolution_Calendar_OtherError);
	g_return_val_if_fail (tzobj != NULL, GNOME_Evolution_Calendar_OtherError);

	priv = cbfile->priv;

	tz_comp = icalparser_parse_string (tzobj);
	if (!tz_comp)
		return GNOME_Evolution_Calendar_InvalidObject;

	if (icalcomponent_isa (tz_comp) == ICAL_VTIMEZONE_COMPONENT) {
		icaltimezone *zone;

		zone = icaltimezone_new ();
		icaltimezone_set_component (zone, tz_comp);
		if (!icalcomponent_get_timezone (priv->icalcomp,
						 icaltimezone_get_tzid (zone))) {
			icalcomponent_add_component (priv->icalcomp, tz_comp);
			mark_dirty (cbfile);
		}

		icaltimezone_free (zone, 1);
	}

	return GNOME_Evolution_Calendar_Success;
}


static CalBackendSyncStatus
cal_backend_file_set_default_timezone (CalBackendSync *backend, Cal *cal, const char *tzid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icaltimezone *zone;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);

	/* Look up the VTIMEZONE in our icalcomponent. */
	zone = icalcomponent_get_timezone (priv->icalcomp, tzid);
	if (!zone)
		return GNOME_Evolution_Calendar_ObjectNotFound;

	/* Set the default timezone to it. */
	priv->default_zone = zone;

	return GNOME_Evolution_Calendar_Success;
}

typedef struct {
	GList *obj_list;
	gboolean search_needed;
	const char *query;
	CalBackendObjectSExp *obj_sexp;
	CalBackend *backend;
	icaltimezone *default_zone;
} MatchObjectData;

static void
match_recurrence_sexp (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp = value;
	MatchObjectData *match_data = data;

	if ((!match_data->search_needed) ||
	    (cal_backend_object_sexp_match_comp (match_data->obj_sexp, comp, match_data->backend))) {
		match_data->obj_list = g_list_append (match_data->obj_list,
						      cal_component_get_as_string (comp));
	}
}

static void
match_object_sexp (gpointer key, gpointer value, gpointer data)
{
	CalBackendFileObject *obj_data = value;
	MatchObjectData *match_data = data;

	if ((!match_data->search_needed) ||
	    (cal_backend_object_sexp_match_comp (match_data->obj_sexp, obj_data->full_object, match_data->backend))) {
		match_data->obj_list = g_list_append (match_data->obj_list,
						      cal_component_get_as_string (obj_data->full_object));

		/* match also recurrences */
		g_hash_table_foreach (obj_data->recurrences,
				      (GHFunc) match_recurrence_sexp,
				      match_data);
	}
}

/* Get_objects_in_range handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_object_list (CalBackendSync *backend, Cal *cal, const char *sexp, GList **objects)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	MatchObjectData match_data;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_message (G_STRLOC ": Getting object list (%s)", sexp);

	match_data.search_needed = TRUE;
	match_data.query = sexp;
	match_data.obj_list = NULL;
	match_data.backend = CAL_BACKEND (backend);
	match_data.default_zone = priv->default_zone;

	if (!strcmp (sexp, "#t"))
		match_data.search_needed = FALSE;

	match_data.obj_sexp = cal_backend_object_sexp_new (sexp);
	if (!match_data.obj_sexp)
		return GNOME_Evolution_Calendar_InvalidQuery;

	g_hash_table_foreach (priv->comp_uid_hash, (GHFunc) match_object_sexp, &match_data);

	*objects = match_data.obj_list;
	
	return GNOME_Evolution_Calendar_Success;	
}

/* get_query handler for the file backend */
static void
cal_backend_file_start_query (CalBackend *backend, Query *query)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	MatchObjectData match_data;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_message (G_STRLOC ": Starting query (%s)", query_get_text (query));

	/* try to match all currently existing objects */
	match_data.search_needed = TRUE;
	match_data.query = query_get_text (query);
	match_data.obj_list = NULL;
	match_data.backend = backend;
	match_data.default_zone = priv->default_zone;

	if (!strcmp (match_data.query, "#t"))
		match_data.search_needed = FALSE;

	match_data.obj_sexp = query_get_object_sexp (query);
	if (!match_data.obj_sexp) {
		query_notify_query_done (query, GNOME_Evolution_Calendar_InvalidQuery);
		return;
	}

	g_hash_table_foreach (priv->comp_uid_hash, (GHFunc) match_object_sexp, &match_data);

	/* notify listeners of all objects */
	if (match_data.obj_list) {
		query_notify_objects_added (query, (const GList *) match_data.obj_list);

		/* free memory */
		g_list_foreach (match_data.obj_list, (GFunc) g_free, NULL);
		g_list_free (match_data.obj_list);
	}

	query_notify_query_done (query, GNOME_Evolution_Calendar_Success);
}

static gboolean
free_busy_instance (CalComponent *comp,
		    time_t        instance_start,
		    time_t        instance_end,
		    gpointer      data)
{
	icalcomponent *vfb = data;
	icalproperty *prop;
	icalparameter *param;
	struct icalperiodtype ipt;
	icaltimezone *utc_zone;

	utc_zone = icaltimezone_get_utc_timezone ();

	ipt.start = icaltime_from_timet_with_zone (instance_start, FALSE, utc_zone);
	ipt.end = icaltime_from_timet_with_zone (instance_end, FALSE, utc_zone);
	ipt.duration = icaldurationtype_null_duration ();
	
        /* add busy information to the vfb component */
	prop = icalproperty_new (ICAL_FREEBUSY_PROPERTY);
	icalproperty_set_freebusy (prop, ipt);
	
	param = icalparameter_new_fbtype (ICAL_FBTYPE_BUSY);
	icalproperty_add_parameter (prop, param);
	
	icalcomponent_add_property (vfb, prop);

	return TRUE;
}

static icalcomponent *
create_user_free_busy (CalBackendFile *cbfile, const char *address, const char *cn,
		       time_t start, time_t end)
{	
	CalBackendFilePrivate *priv;
	GList *l;
	icalcomponent *vfb;
	icaltimezone *utc_zone;
	CalBackendObjectSExp *obj_sexp;
	char *query;
	
	priv = cbfile->priv;

	/* create the (unique) VFREEBUSY object that we'll return */
	vfb = icalcomponent_new_vfreebusy ();
	if (address != NULL) {
		icalproperty *prop;
		icalparameter *param;
		
		prop = icalproperty_new_organizer (address);
		if (prop != NULL && cn != NULL) {
			param = icalparameter_new_cn (cn);
			icalproperty_add_parameter (prop, param);			
		}
		if (prop != NULL)
			icalcomponent_add_property (vfb, prop);		
	}
	utc_zone = icaltimezone_get_utc_timezone ();
	icalcomponent_set_dtstart (vfb, icaltime_from_timet_with_zone (start, FALSE, utc_zone));
	icalcomponent_set_dtend (vfb, icaltime_from_timet_with_zone (end, FALSE, utc_zone));

	/* add all objects in the given interval */
	query = g_strdup_printf ("occur-in-time-range? %lu %lu", start, end);
	obj_sexp = cal_backend_object_sexp_new (query);
	g_free (query);

	if (!obj_sexp)
		return vfb;

	for (l = priv->comp; l; l = l->next) {
		CalComponent *comp = l->data;
		icalcomponent *icalcomp, *vcalendar_comp;
		icalproperty *prop;
		
		icalcomp = cal_component_get_icalcomponent (comp);
		if (!icalcomp)
			continue;

		/* If the event is TRANSPARENT, skip it. */
		prop = icalcomponent_get_first_property (icalcomp,
							 ICAL_TRANSP_PROPERTY);
		if (prop) {
			icalproperty_transp transp_val = icalproperty_get_transp (prop);
			if (transp_val == ICAL_TRANSP_TRANSPARENT ||
			    transp_val == ICAL_TRANSP_TRANSPARENTNOCONFLICT)
				continue;
		}
	
		if (!cal_backend_object_sexp_match_comp (obj_sexp, l->data, CAL_BACKEND (cbfile)))
			continue;
		
		vcalendar_comp = icalcomponent_get_parent (icalcomp);
		cal_recur_generate_instances (comp, start, end,
					      free_busy_instance,
					      vfb,
					      resolve_tzid,
					      vcalendar_comp,
					      priv->default_zone);
	}

	return vfb;	
}

/* Get_free_busy handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_free_busy (CalBackendSync *backend, Cal *cal, GList *users,
				time_t start, time_t end, GList **freebusy)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	gchar *address, *name;	
	icalcomponent *vfb;
	char *calobj;
	GList *l;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (start != -1 && end != -1, GNOME_Evolution_Calendar_InvalidRange);
	g_return_val_if_fail (start <= end, GNOME_Evolution_Calendar_InvalidRange);

	*freebusy = NULL;
	
	if (users == NULL) {
		if (cal_backend_mail_account_get_default (priv->config_listener, &address, &name)) {
			vfb = create_user_free_busy (cbfile, address, name, start, end);
			calobj = icalcomponent_as_ical_string (vfb);
			*freebusy = g_list_append (*freebusy, g_strdup (calobj));
			icalcomponent_free (vfb);
			g_free (address);
			g_free (name);
		}		
	} else {
		for (l = users; l != NULL; l = l->next ) {
			address = l->data;			
			if (cal_backend_mail_account_is_valid (priv->config_listener, address, &name)) {
				vfb = create_user_free_busy (cbfile, address, name, start, end);
				calobj = icalcomponent_as_ical_string (vfb);
				*freebusy = g_list_append (*freebusy, g_strdup (calobj));
				icalcomponent_free (vfb);
				g_free (name);
			}
		}		
	}

	return GNOME_Evolution_Calendar_Success;
}

typedef struct 
{
	CalBackendFile *backend;
	CalObjType type;	
	GList *deletes;
	EXmlHash *ehash;
} CalBackendFileComputeChangesData;

static void
cal_backend_file_compute_changes_foreach_key (const char *key, gpointer data)
{
	CalBackendFileComputeChangesData *be_data = data;
	
	if (!lookup_component (be_data->backend, key)) {
		CalComponent *comp;

		comp = cal_component_new ();
		if (be_data->type == GNOME_Evolution_Calendar_TYPE_TODO)
			cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
		else
			cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

		cal_component_set_uid (comp, key);
		be_data->deletes = g_list_prepend (be_data->deletes, cal_component_get_as_string (comp));

		e_xmlhash_remove (be_data->ehash, key);
 	}
}

static CalBackendSyncStatus
cal_backend_file_compute_changes (CalBackendFile *cbfile, CalObjType type, const char *change_id,
				  GList **adds, GList **modifies, GList **deletes)
{
	CalBackendFilePrivate *priv;
	char    *filename;
	EXmlHash *ehash;
	CalBackendFileComputeChangesData be_data;
	GList *i;

	priv = cbfile->priv;

	/* FIXME Will this always work? */
	filename = g_strdup_printf ("%s/%s.db", priv->uri, change_id);
	ehash = e_xmlhash_new (filename);
	g_free (filename);
	
	/* Calculate adds and modifies */
	for (i = priv->comp; i != NULL; i = i->next) {
		const char *uid;
		char *calobj;

		cal_component_get_uid (i->data, &uid);
		calobj = cal_component_get_as_string (i->data);

		g_assert (calobj != NULL);

		/* check what type of change has occurred, if any */
		switch (e_xmlhash_compare (ehash, uid, calobj)) {
		case E_XMLHASH_STATUS_SAME:
			break;
		case E_XMLHASH_STATUS_NOT_FOUND:
			*adds = g_list_prepend (*adds, g_strdup (calobj));
			e_xmlhash_add (ehash, uid, calobj);
			break;
		case E_XMLHASH_STATUS_DIFFERENT:
			*modifies = g_list_prepend (*modifies, g_strdup (calobj));
			e_xmlhash_add (ehash, uid, calobj);
			break;
		}

		g_free (calobj);
	}

	/* Calculate deletions */
	be_data.backend = cbfile;
	be_data.type = type;	
	be_data.deletes = NULL;
	be_data.ehash = ehash;
   	e_xmlhash_foreach_key (ehash, (EXmlHashFunc)cal_backend_file_compute_changes_foreach_key, &be_data);

	*deletes = be_data.deletes;

	e_xmlhash_write (ehash);
  	e_xmlhash_destroy (ehash);
	
	return GNOME_Evolution_Calendar_Success;
}

/* Get_changes handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_get_changes (CalBackendSync *backend, Cal *cal, CalObjType type, const char *change_id,
			      GList **adds, GList **modifies, GList **deletes)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (change_id != NULL, GNOME_Evolution_Calendar_ObjectNotFound);

	return cal_backend_file_compute_changes (cbfile, type, change_id, adds, modifies, deletes);
}

/* Discard_alarm handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_discard_alarm (CalBackendSync *backend, Cal *cal, const char *uid, const char *auid)
{
	/* we just do nothing with the alarm */
	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
cal_backend_file_create_object (CalBackendSync *backend, Cal *cal, const char *calobj, char **uid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	CalComponent *comp;
	const char *comp_uid;
	struct icaltimetype current;
	
	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (calobj != NULL, GNOME_Evolution_Calendar_ObjectNotFound);

	icalcomp = icalparser_parse_string ((char *) calobj);
	if (!icalcomp)
		return GNOME_Evolution_Calendar_InvalidObject;

	/* FIXME Check kind with the parent */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VEVENT_COMPONENT && kind != ICAL_VTODO_COMPONENT) {
		icalcomponent_free (icalcomp);
		return GNOME_Evolution_Calendar_InvalidObject;
	}

	/* Get the UID */
	comp_uid = icalcomponent_get_uid (icalcomp);
	
	/* check the object is not in our cache */
	if (lookup_component (cbfile, comp_uid)) {
		icalcomponent_free (icalcomp);
		return GNOME_Evolution_Calendar_CardIdAlreadyExists;
	}

	/* Create the cal component */
	comp = cal_component_new ();
	cal_component_set_icalcomponent (comp, icalcomp);

	/* Set the created and last modified times on the component */
	current = icaltime_from_timet (time (NULL), 0);
	cal_component_set_created (comp, &current);
	cal_component_set_last_modified (comp, &current);

	/* Add the object */
	add_component (cbfile, comp, TRUE);

	/* Mark for saving */
	mark_dirty (cbfile);

	/* Return the UID */
	if (uid)
		*uid = g_strdup (comp_uid);

	return GNOME_Evolution_Calendar_Success;
}

static CalBackendSyncStatus
cal_backend_file_modify_object (CalBackendSync *backend, Cal *cal, const char *calobj, 
				CalObjModType mod, char **old_object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	const char *comp_uid;
	CalComponent *comp;
	CalBackendFileObject *obj_data;
	struct icaltimetype current;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;
		
	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (calobj != NULL, GNOME_Evolution_Calendar_ObjectNotFound);

	icalcomp = icalparser_parse_string ((char *) calobj);
	if (!icalcomp)
		return GNOME_Evolution_Calendar_InvalidObject;

	/* FIXME Check kind with the parent */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VEVENT_COMPONENT && kind != ICAL_VTODO_COMPONENT) {
		icalcomponent_free (icalcomp);
		return GNOME_Evolution_Calendar_InvalidObject;
	}

	/* Get the uid */
	comp_uid = icalcomponent_get_uid (icalcomp);

	/* Get the object from our cache */
	if (!(obj_data = g_hash_table_lookup (priv->comp_uid_hash, comp_uid))) {
		icalcomponent_free (icalcomp);
		return GNOME_Evolution_Calendar_ObjectNotFound;
	}

	/* Create the cal component */
	comp = cal_component_new ();
	cal_component_set_icalcomponent (comp, icalcomp);
	
	/* Set the last modified time on the component */
	current = icaltime_from_timet (time (NULL), 0);
	cal_component_set_last_modified (comp, &current);

	/* handle mod_type */
	if (cal_component_is_instance (comp) ||
	    mod != CALOBJ_MOD_ALL) {
		/* FIXME */
	} else {
		/* Remove the old version */
		remove_component (cbfile, obj_data->full_object);

		/* Add the object */
		add_component (cbfile, comp, TRUE);
	}

	mark_dirty (cbfile);

	if (old_object)
		*old_object = cal_component_get_as_string (comp);

	return GNOME_Evolution_Calendar_Success;
}

/* Remove_object handler for the file backend */
static CalBackendSyncStatus
cal_backend_file_remove_object (CalBackendSync *backend, Cal *cal,
				const char *uid, const char *rid,
				CalObjModType mod, char **object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalBackendFileObject *obj_data;
	CalComponent *comp;
	char *hash_rid;
	GSList *categories;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_NoSuchCal);
	g_return_val_if_fail (uid != NULL, GNOME_Evolution_Calendar_ObjectNotFound);

	obj_data = g_hash_table_lookup (priv->comp_uid_hash, uid);
	if (!obj_data)
		return GNOME_Evolution_Calendar_ObjectNotFound;

	if (rid && *rid) {
		if (g_hash_table_lookup_extended (obj_data->recurrences, rid,
						  &hash_rid, &comp)) {
			/* remove the component from our data */
			icalcomponent_remove_component (priv->icalcomp,
							cal_component_get_icalcomponent (comp));
			priv->comp = g_list_remove (priv->comp, comp);
			g_hash_table_remove (obj_data->recurrences, rid);

			/* update the set of categories */
			cal_component_get_categories_list (comp, &categories);
			cal_backend_unref_categories (CAL_BACKEND (cbfile), categories);
			cal_component_free_categories_list (categories);

			/* free memory */
			g_free (hash_rid);
			g_object_unref (comp);

			mark_dirty (cbfile);

			return GNOME_Evolution_Calendar_Success;
		}
	}

	comp = obj_data->full_object;

	if (mod != CALOBJ_MOD_ALL) {
		*object = cal_component_get_as_string (comp);
		remove_component (cbfile, comp);
	} else {
		/* remove the component from our data, temporarily */
		icalcomponent_remove_component (priv->icalcomp,
						cal_component_get_icalcomponent (comp));
		priv->comp = g_list_remove (priv->comp, comp);

		cal_util_remove_instances (cal_component_get_icalcomponent (comp),
					   icaltime_from_string (rid), mod);

		/* add the modified object to the beginning of the list, 
		   so that it's always before any detached instance we
		   might have */
		priv->comp = g_list_prepend (priv->comp, comp);
	}

	mark_dirty (cbfile);

	return GNOME_Evolution_Calendar_Success;
}

static gboolean
cancel_received_object (CalBackendFile *cbfile, icalcomponent *icalcomp)
{
	CalComponent *old_comp;

	/* Find the old version of the component. */
	old_comp = lookup_component (cbfile, icalcomponent_get_uid (icalcomp));
	if (!old_comp)
		return FALSE;

	/* And remove it */
	remove_component (cbfile, old_comp);

	return TRUE;
}

typedef struct {
	GHashTable *zones;
	
	gboolean found;
} CalBackendFileTzidData;

static void
check_tzids (icalparameter *param, void *data)
{
	CalBackendFileTzidData *tzdata = data;
	const char *tzid;
	
	tzid = icalparameter_get_tzid (param);
	if (!tzid || g_hash_table_lookup (tzdata->zones, tzid))
		tzdata->found = FALSE;
}

/* Update_objects handler for the file backend. */
static CalBackendSyncStatus
cal_backend_file_receive_objects (CalBackendSync *backend, Cal *cal, const char *calobj,
				  GList **created, GList **modified, GList **removed)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalcomponent *toplevel_comp, *icalcomp = NULL;
	icalcomponent_kind kind;
	icalproperty_method method;
	icalcomponent *subcomp;
	GList *comps, *l;
	CalBackendFileTzidData tzdata;
	CalBackendSyncStatus status = GNOME_Evolution_Calendar_Success;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, GNOME_Evolution_Calendar_InvalidObject);
	g_return_val_if_fail (calobj != NULL, GNOME_Evolution_Calendar_InvalidObject);

	/* Pull the component from the string and ensure that it is sane */
	toplevel_comp = icalparser_parse_string ((char *) calobj);
	if (!toplevel_comp)
		return GNOME_Evolution_Calendar_InvalidObject;

	kind = icalcomponent_isa (toplevel_comp);
	if (kind != ICAL_VCALENDAR_COMPONENT) {
		/* If its not a VCALENDAR, make it one to simplify below */
		icalcomp = toplevel_comp;
		toplevel_comp = cal_util_new_top_level ();
		icalcomponent_add_component (toplevel_comp, icalcomp);	
	}

	method = icalcomponent_get_method (toplevel_comp);

	*created = *modified = *removed = NULL;

	/* Build a list of timezones so we can make sure all the objects have valid info */
	tzdata.zones = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	subcomp = icalcomponent_get_first_component (toplevel_comp, ICAL_VTIMEZONE_COMPONENT);
	while (subcomp) {
		icaltimezone *zone;
		
		zone = icaltimezone_new ();
		if (icaltimezone_set_component (zone, subcomp))
			g_hash_table_insert (tzdata.zones, g_strdup (icaltimezone_get_tzid (zone)), NULL);
		
		subcomp = icalcomponent_get_next_component (toplevel_comp, ICAL_VTIMEZONE_COMPONENT);
	}	

	/* First we make sure all the components are usuable */
	comps = NULL;
	subcomp = icalcomponent_get_first_component (toplevel_comp, ICAL_ANY_COMPONENT);
	while (subcomp) {
		/* We ignore anything except VEVENT, VTODO and VJOURNAL
		   components. */
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);

		switch (child_kind) {
		case ICAL_VEVENT_COMPONENT:
		case ICAL_VTODO_COMPONENT:
		case ICAL_VJOURNAL_COMPONENT:
			tzdata.found = TRUE;
			icalcomponent_foreach_tzid (subcomp, check_tzids, &tzdata);
			
			if (!tzdata.found) {
				status = GNOME_Evolution_Calendar_InvalidObject;
				goto error;
			}

			if (!icalcomponent_get_uid (subcomp)) {
				status = GNOME_Evolution_Calendar_InvalidObject;
				goto error;
			}
		
			comps = g_list_prepend (comps, subcomp);
			break;
		default:
			/* Ignore it */
			break;
		}

		subcomp = icalcomponent_get_next_component (toplevel_comp, ICAL_ANY_COMPONENT);
	}

	/* Now we manipulate the components we care about */
	for (l = comps; l; l = l->next) {
		subcomp = l->data;
		
		switch (method) {
		case ICAL_METHOD_PUBLISH:
		case ICAL_METHOD_REQUEST:
			/* FIXME Need to see the new create/modify stuff before we set this up */
			break;			
		case ICAL_METHOD_REPLY:
			/* FIXME Update the status of the user, if we are the organizer */
			break;
		case ICAL_METHOD_ADD:
			/* FIXME This should be doable once all the recurid stuff is done */
			break;
		case ICAL_METHOD_COUNTER:
			status = GNOME_Evolution_Calendar_UnsupportedMethod;
			goto error;
			break;			
		case ICAL_METHOD_DECLINECOUNTER:			
			status = GNOME_Evolution_Calendar_UnsupportedMethod;
			goto error;
			break;
		case ICAL_METHOD_CANCEL:
			/* FIXME Do we need to remove the subcomp so it isn't merged? */
			if (cancel_received_object (cbfile, subcomp))
				*removed = g_list_prepend (*removed, g_strdup (icalcomponent_get_uid (subcomp)));
			break;
		default:
			status = GNOME_Evolution_Calendar_UnsupportedMethod;
			goto error;
		}
	}
	g_list_free (comps);
	
	/* Merge the iCalendar components with our existing VCALENDAR,
	   resolving any conflicting TZIDs. */
	icalcomponent_merge_component (priv->icalcomp, toplevel_comp);

	mark_dirty (cbfile);

 error:
	g_hash_table_destroy (tzdata.zones);
	
	return status;
}

static CalBackendSyncStatus
cal_backend_file_send_objects (CalBackendSync *backend, Cal *cal, const char *calobj)
{
	/* FIXME Put in a util routine to send stuff via email */
	
	return GNOME_Evolution_Calendar_Success;
}

static icaltimezone *
cal_backend_file_internal_get_default_timezone (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	return priv->default_zone;
}

static icaltimezone *
cal_backend_file_internal_get_timezone (CalBackend *backend, const char *tzid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icaltimezone *zone;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	if (!strcmp (tzid, "UTC"))
	        zone = icaltimezone_get_utc_timezone ();
	else {
		zone = icalcomponent_get_timezone (priv->icalcomp, tzid);
		if (!zone)
			zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	}

	return zone;
}

/* Object initialization function for the file backend */
static void
cal_backend_file_init (CalBackendFile *cbfile, CalBackendFileClass *class)
{
	CalBackendFilePrivate *priv;

	priv = g_new0 (CalBackendFilePrivate, 1);
	cbfile->priv = priv;

	priv->uri = NULL;
	priv->file_name = g_strdup ("calendar.ics");
	priv->read_only = FALSE;
	priv->icalcomp = NULL;
	priv->comp_uid_hash = NULL;
	priv->comp = NULL;

	/* The timezone defaults to UTC. */
	priv->default_zone = icaltimezone_get_utc_timezone ();

	priv->config_listener = e_config_listener_new ();
}

/* Class initialization function for the file backend */
static void
cal_backend_file_class_init (CalBackendFileClass *class)
{
	GObjectClass *object_class;
	CalBackendClass *backend_class;
	CalBackendSyncClass *sync_class;

	object_class = (GObjectClass *) class;
	backend_class = (CalBackendClass *) class;
	sync_class = (CalBackendSyncClass *) class;

	parent_class = (CalBackendSyncClass *) g_type_class_peek_parent (class);

	object_class->dispose = cal_backend_file_dispose;
	object_class->finalize = cal_backend_file_finalize;

	sync_class->is_read_only_sync = cal_backend_file_is_read_only;
	sync_class->get_cal_address_sync = cal_backend_file_get_cal_address;
 	sync_class->get_alarm_email_address_sync = cal_backend_file_get_alarm_email_address;
 	sync_class->get_ldap_attribute_sync = cal_backend_file_get_ldap_attribute;
 	sync_class->get_static_capabilities_sync = cal_backend_file_get_static_capabilities;
	sync_class->open_sync = cal_backend_file_open;
	sync_class->remove_sync = cal_backend_file_remove;
	sync_class->create_object_sync = cal_backend_file_create_object;
	sync_class->modify_object_sync = cal_backend_file_modify_object;
	sync_class->remove_object_sync = cal_backend_file_remove_object;
	sync_class->discard_alarm_sync = cal_backend_file_discard_alarm;
	sync_class->receive_objects_sync = cal_backend_file_receive_objects;
	sync_class->send_objects_sync = cal_backend_file_send_objects;
 	sync_class->get_default_object_sync = cal_backend_file_get_default_object;
	sync_class->get_object_sync = cal_backend_file_get_object;
	sync_class->get_object_list_sync = cal_backend_file_get_object_list;
	sync_class->get_timezone_sync = cal_backend_file_get_timezone;
	sync_class->add_timezone_sync = cal_backend_file_add_timezone;
	sync_class->set_default_timezone_sync = cal_backend_file_set_default_timezone;
	sync_class->get_freebusy_sync = cal_backend_file_get_free_busy;
	sync_class->get_changes_sync = cal_backend_file_get_changes;

	backend_class->is_loaded = cal_backend_file_is_loaded;
	backend_class->start_query = cal_backend_file_start_query;
	backend_class->get_mode = cal_backend_file_get_mode;
	backend_class->set_mode = cal_backend_file_set_mode;

	backend_class->internal_get_default_timezone = cal_backend_file_internal_get_default_timezone;
	backend_class->internal_get_timezone = cal_backend_file_internal_get_timezone;
}


/**
 * cal_backend_file_get_type:
 * @void: 
 * 
 * Registers the #CalBackendFile class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #CalBackendFile class.
 **/
GType
cal_backend_file_get_type (void)
{
	static GType cal_backend_file_type = 0;

	if (!cal_backend_file_type) {
		static GTypeInfo info = {
                        sizeof (CalBackendFileClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_backend_file_class_init,
                        NULL, NULL,
                        sizeof (CalBackendFile),
                        0,
                        (GInstanceInitFunc) cal_backend_file_init
                };
		cal_backend_file_type = g_type_register_static (CAL_TYPE_BACKEND_SYNC,
								"CalBackendFile", &info, 0);
	}

	return cal_backend_file_type;
}

void
cal_backend_file_set_file_name (CalBackendFile *cbfile, const char *file_name)
{
	CalBackendFilePrivate *priv;
	
	g_return_if_fail (cbfile != NULL);
	g_return_if_fail (IS_CAL_BACKEND_FILE (cbfile));
	g_return_if_fail (file_name != NULL);

	priv = cbfile->priv;
	
	if (priv->file_name)
		g_free (priv->file_name);
	
	priv->file_name = g_strdup (file_name);
}

const char *
cal_backend_file_get_file_name (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;

	g_return_val_if_fail (cbfile != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND_FILE (cbfile), NULL);

	priv = cbfile->priv;	

	return priv->file_name;
}

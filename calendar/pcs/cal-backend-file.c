/* Evolution calendar - iCalendar file backend
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
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
#include "e-util/e-dbhash.h"
#include "cal-util/cal-recur.h"
#include "cal-util/cal-util.h"
#include "cal-backend-file.h"
#include "cal-backend-util.h"



/* Private part of the CalBackendFile structure */
struct _CalBackendFilePrivate {
	/* URI where the calendar data is stored */
	char *uri;

	/* Toplevel VCALENDAR component */
	icalcomponent *icalcomp;

	/* All the CalComponent objects in the calendar, hashed by UID.  The
	 * hash key *is* the uid returned by cal_component_get_uid(); it is not
	 * copied, so don't free it when you remove an object from the hash
	 * table.
	 */
	GHashTable *comp_uid_hash;

	/* All event, to-do, and journal components in the calendar; they are
	 * here just for easy access (i.e. so that you don't have to iterate
	 * over the comp_uid_hash).  If you need *all* the components in the
	 * calendar, iterate over the hash instead.
	 */
	GList *events;
	GList *todos;
	GList *journals;

	/* Config database handle for free/busy organizer information */
	EConfigListener *config_listener;
	
	/* Idle handler for saving the calendar when it is dirty */
	guint idle_id;

	/* The calendar's default timezone, used for resolving DATE and
	   floating DATE-TIME values. */
	icaltimezone *default_zone;
};



static void cal_backend_file_class_init (CalBackendFileClass *class);
static void cal_backend_file_init (CalBackendFile *cbfile, CalBackendFileClass *class);
static void cal_backend_file_dispose (GObject *object);
static void cal_backend_file_finalize (GObject *object);

static const char *cal_backend_file_get_uri (CalBackend *backend);
static gboolean cal_backend_file_is_read_only (CalBackend *backend);
static const char *cal_backend_file_get_cal_address (CalBackend *backend);
static const char *cal_backend_file_get_alarm_email_address (CalBackend *backend);
static const char *cal_backend_file_get_ldap_attribute (CalBackend *backend);
static const char *cal_backend_file_get_static_capabilities (CalBackend *backend);
static CalBackendOpenStatus cal_backend_file_open (CalBackend *backend,
						   const char *uristr,
						   gboolean only_if_exists);
static gboolean cal_backend_file_is_loaded (CalBackend *backend);
static Query *cal_backend_file_get_query (CalBackend *backend,
					  GNOME_Evolution_Calendar_QueryListener ql,
					  const char *sexp);

static CalMode cal_backend_file_get_mode (CalBackend *backend);
static void cal_backend_file_set_mode (CalBackend *backend, CalMode mode);

static int cal_backend_file_get_n_objects (CalBackend *backend, CalObjType type);
static char *cal_backend_file_get_default_object (CalBackend *backend, CalObjType type);
static CalComponent *cal_backend_file_get_object_component (CalBackend *backend, const char *uid);
static char *cal_backend_file_get_timezone_object (CalBackend *backend, const char *tzid);
static GList *cal_backend_file_get_uids (CalBackend *backend, CalObjType type);
static GList *cal_backend_file_get_objects_in_range (CalBackend *backend, CalObjType type,
						     time_t start, time_t end);
static GList *cal_backend_file_get_free_busy (CalBackend *backend, GList *users, time_t start, time_t end);
static GNOME_Evolution_Calendar_CalObjChangeSeq *cal_backend_file_get_changes (
	CalBackend *backend, CalObjType type, const char *change_id);

static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *cal_backend_file_get_alarms_in_range (
	CalBackend *backend, time_t start, time_t end);

static GNOME_Evolution_Calendar_CalComponentAlarms *cal_backend_file_get_alarms_for_object (
	CalBackend *backend, const char *uid,
	time_t start, time_t end, gboolean *object_found);

static CalBackendResult cal_backend_file_discard_alarm (CalBackend *backend,
							const char *uid,
							const char *auid);

static CalBackendResult cal_backend_file_update_objects (CalBackend *backend,
							 const char *calobj,
							 CalObjModType mod);
static CalBackendResult cal_backend_file_remove_object (CalBackend *backend, const char *uid, CalObjModType mod);

static CalBackendSendResult cal_backend_file_send_object (CalBackend *backend, 
							  const char *calobj, gchar **new_calobj,
							  GNOME_Evolution_Calendar_UserList **user_list,
							  char error_msg[256]);

static icaltimezone* cal_backend_file_get_timezone (CalBackend *backend, const char *tzid);
static icaltimezone* cal_backend_file_get_default_timezone (CalBackend *backend);
static gboolean cal_backend_file_set_default_timezone (CalBackend *backend,
						       const char *tzid);

static CalBackendClass *parent_class;



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
		cal_backend_file_type = g_type_register_static (CAL_BACKEND_TYPE,
								"CalBackendFile", &info, 0);
	}

	return cal_backend_file_type;
}

/* Class initialization function for the file backend */
static void
cal_backend_file_class_init (CalBackendFileClass *class)
{
	GObjectClass *object_class;
	CalBackendClass *backend_class;

	object_class = (GObjectClass *) class;
	backend_class = (CalBackendClass *) class;

	parent_class = (CalBackendClass *) g_type_class_peek_parent (class);

	object_class->dispose = cal_backend_file_dispose;
	object_class->finalize = cal_backend_file_finalize;

	backend_class->get_uri = cal_backend_file_get_uri;
	backend_class->is_read_only = cal_backend_file_is_read_only;
	backend_class->get_cal_address = cal_backend_file_get_cal_address;
 	backend_class->get_alarm_email_address = cal_backend_file_get_alarm_email_address;
 	backend_class->get_ldap_attribute = cal_backend_file_get_ldap_attribute;
 	backend_class->get_static_capabilities = cal_backend_file_get_static_capabilities;
	backend_class->open = cal_backend_file_open;
	backend_class->is_loaded = cal_backend_file_is_loaded;
	backend_class->get_query = cal_backend_file_get_query;
	backend_class->get_mode = cal_backend_file_get_mode;
	backend_class->set_mode = cal_backend_file_set_mode;	
	backend_class->get_n_objects = cal_backend_file_get_n_objects;
 	backend_class->get_default_object = cal_backend_file_get_default_object;
	backend_class->get_object_component = cal_backend_file_get_object_component;
	backend_class->get_timezone_object = cal_backend_file_get_timezone_object;
	backend_class->get_uids = cal_backend_file_get_uids;
	backend_class->get_objects_in_range = cal_backend_file_get_objects_in_range;
	backend_class->get_free_busy = cal_backend_file_get_free_busy;
	backend_class->get_changes = cal_backend_file_get_changes;
	backend_class->get_alarms_in_range = cal_backend_file_get_alarms_in_range;
	backend_class->get_alarms_for_object = cal_backend_file_get_alarms_for_object;
	backend_class->discard_alarm = cal_backend_file_discard_alarm;
	backend_class->update_objects = cal_backend_file_update_objects;
	backend_class->remove_object = cal_backend_file_remove_object;
	backend_class->send_object = cal_backend_file_send_object;

	backend_class->get_timezone = cal_backend_file_get_timezone;
	backend_class->get_default_timezone = cal_backend_file_get_default_timezone;
	backend_class->set_default_timezone = cal_backend_file_set_default_timezone;
}

/* Object initialization function for the file backend */
static void
cal_backend_file_init (CalBackendFile *cbfile, CalBackendFileClass *class)
{
	CalBackendFilePrivate *priv;

	priv = g_new0 (CalBackendFilePrivate, 1);
	cbfile->priv = priv;

	priv->uri = NULL;
	priv->icalcomp = NULL;
	priv->comp_uid_hash = NULL;
	priv->events = NULL;
	priv->todos = NULL;
	priv->journals = NULL;

	/* The timezone defaults to UTC. */
	priv->default_zone = icaltimezone_get_utc_timezone ();

	priv->config_listener = e_config_listener_new ();
}

/* g_hash_table_foreach() callback to destroy a CalComponent */
static void
free_cal_component (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp;

	comp = CAL_COMPONENT (value);
	g_object_unref (comp);
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
		g_hash_table_foreach (priv->comp_uid_hash, 
				      free_cal_component, NULL);
		g_hash_table_destroy (priv->comp_uid_hash);
		priv->comp_uid_hash = NULL;
	}

	g_list_free (priv->events);
	g_list_free (priv->todos);
	g_list_free (priv->journals);
	priv->events = NULL;
	priv->todos = NULL;
	priv->journals = NULL;

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
	CalComponent *comp;

	priv = cbfile->priv;

	comp = g_hash_table_lookup (priv->comp_uid_hash, uid);

	return comp;
}



/* Calendar backend methods */

/* Get_uri handler for the file backend */
static const char *
cal_backend_file_get_uri (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_assert (priv->uri != NULL);

	return (const char *) priv->uri;
}

/* Is_read_only handler for the file backend */
static gboolean
cal_backend_file_is_read_only (CalBackend *backend)
{
	/* we just return FALSE, since all calendars are read-write */
	return FALSE;
}

/* Get_email_address handler for the file backend */
static const char *
cal_backend_file_get_cal_address (CalBackend *backend)
{
	/* A file backend has no particular email address associated
	 * with it (although that would be a useful feature some day).
	 */
	return NULL;
}

static const char *
cal_backend_file_get_ldap_attribute (CalBackend *backend)
{
	return NULL;
}

static const char *
cal_backend_file_get_alarm_email_address (CalBackend *backend)
{
 	/* A file backend has no particular email address associated
 	 * with it (although that would be a useful feature some day).
 	 */
	return NULL;
}

static const char *
cal_backend_file_get_static_capabilities (CalBackend *backend)
{
 	return CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS;
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
	CalComponent *old_comp;
	const char *uid;
	char *new_uid;

	priv = cbfile->priv;

	cal_component_get_uid (comp, &uid);

	old_comp = g_hash_table_lookup (priv->comp_uid_hash, uid);
	if (!old_comp)
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

/* Tries to add an icalcomponent to the file backend.  We only store the objects
 * of the types we support; all others just remain in the toplevel component so
 * that we don't lose them.
 */
static void
add_component (CalBackendFile *cbfile, CalComponent *comp, gboolean add_to_toplevel)
{
	CalBackendFilePrivate *priv;
	GList **list;
	const char *uid;
	GSList *categories;

	priv = cbfile->priv;

	switch (cal_component_get_vtype (comp)) {
	case CAL_COMPONENT_EVENT:
		list = &priv->events;
		break;

	case CAL_COMPONENT_TODO:
		list = &priv->todos;
		break;

	case CAL_COMPONENT_JOURNAL:
		list = &priv->journals;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	/* Ensure that the UID is unique; some broken implementations spit
	 * components with duplicated UIDs.
	 */
	check_dup_uid (cbfile, comp);
	cal_component_get_uid (comp, &uid);
	g_hash_table_insert (priv->comp_uid_hash, (char *)uid, comp);

	*list = g_list_prepend (*list, comp);

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
	GList **list, *l;
	GSList *categories;

	priv = cbfile->priv;

	/* Remove the icalcomp from the toplevel */

	icalcomp = cal_component_get_icalcomponent (comp);
	g_assert (icalcomp != NULL);

	icalcomponent_remove_component (priv->icalcomp, icalcomp);

	/* Remove it from our mapping */

	cal_component_get_uid (comp, &uid);
	g_hash_table_remove (priv->comp_uid_hash, uid);
	
	switch (cal_component_get_vtype (comp)) {
	case CAL_COMPONENT_EVENT:
		list = &priv->events;
		break;

	case CAL_COMPONENT_TODO:
		list = &priv->todos;
		break;

	case CAL_COMPONENT_JOURNAL:
		list = &priv->journals;
		break;

	default:
                /* Make the compiler shut up. */
	        list = NULL;
		g_assert_not_reached ();
	}

	l = g_list_find (*list, comp);
	g_assert (l != NULL);

	*list = g_list_remove_link (*list, l);
	g_list_free_1 (l);

	/* Update the set of categories */
	cal_component_get_categories_list (comp, &categories);
	cal_backend_unref_categories (CAL_BACKEND (cbfile), categories);
	cal_component_free_categories_list (categories);

	g_object_unref (comp);
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
static CalBackendOpenStatus
open_cal (CalBackendFile *cbfile, const char *uristr)
{
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;

	priv = cbfile->priv;

	icalcomp = cal_util_parse_ics_file (uristr);
	if (!icalcomp)
		return CAL_BACKEND_OPEN_ERROR;

	/* FIXME: should we try to demangle XROOT components and
	 * individual components as well?
	 */

	if (icalcomponent_isa (icalcomp) != ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_free (icalcomp);
		return CAL_BACKEND_OPEN_ERROR;
	}

	priv->icalcomp = icalcomp;

	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	scan_vcalendar (cbfile);

	priv->uri = g_strdup (uristr);

	return CAL_BACKEND_OPEN_SUCCESS;
}

static CalBackendOpenStatus
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

	return CAL_BACKEND_OPEN_SUCCESS;
}

/* Open handler for the file backend */
static CalBackendOpenStatus
cal_backend_file_open (CalBackend *backend, const char *uristr, gboolean only_if_exists)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	char *str_uri;
	GnomeVFSURI *uri;
	CalBackendOpenStatus status;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp == NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (uristr != NULL, CAL_BACKEND_OPEN_ERROR);

	g_assert (priv->uri == NULL);
	g_assert (priv->comp_uid_hash == NULL);

	uri = gnome_vfs_uri_new (uristr);
	if (!uri)
		return CAL_BACKEND_OPEN_ERROR;

	if (!uri->method_string || strcmp (uri->method_string, "file")) {
		gnome_vfs_uri_unref (uri);
		return CAL_BACKEND_OPEN_ERROR;
	}

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));
	if (!str_uri || !strlen (str_uri)) {
		g_free (str_uri);
		gnome_vfs_uri_unref (uri);
		return CAL_BACKEND_OPEN_ERROR;
	}

	if (access (str_uri, R_OK) == 0)
		status = open_cal (cbfile, str_uri);
	else {
		if (only_if_exists)
			status = CAL_BACKEND_OPEN_NOT_FOUND;
		else
			status = create_cal (cbfile, str_uri);
	}

	g_free (str_uri);
	gnome_vfs_uri_unref (uri);

	return status;
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

/* get_query handler for the file backend */
static Query *
cal_backend_file_get_query (CalBackend *backend,
			    GNOME_Evolution_Calendar_QueryListener ql,
			    const char *sexp)
{
	CalBackendFile *cbfile;

	cbfile = CAL_BACKEND_FILE (backend);

	return query_new (backend, ql, sexp);
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

/* Get_n_objects handler for the file backend */
static int
cal_backend_file_get_n_objects (CalBackend *backend, CalObjType type)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	int n;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, -1);

	n = 0;

	if (type & CALOBJ_TYPE_EVENT)
		n += g_list_length (priv->events);

	if (type & CALOBJ_TYPE_TODO)
		n += g_list_length (priv->todos);

	if (type & CALOBJ_TYPE_JOURNAL)
		n += g_list_length (priv->journals);

	return n;
}

static char *
cal_backend_file_get_default_object (CalBackend *backend, CalObjType type)
{
 	CalBackendFile *cbfile;
 	CalBackendFilePrivate *priv;
 	CalComponent *comp;
 	char *calobj;
 	
 	cbfile = CAL_BACKEND_FILE (backend);
 	priv = cbfile->priv;
 
 	comp = cal_component_new ();
 	
 	switch (type) {
 	case CALOBJ_TYPE_EVENT:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);
 		break;
 	case CALOBJ_TYPE_TODO:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
 		break;
 	case CALOBJ_TYPE_JOURNAL:
 		cal_component_set_new_vtype (comp, CAL_COMPONENT_JOURNAL);
 		break;
 	default:
 		g_object_unref (comp);
 		return NULL;
 	}
 	
 	calobj = cal_component_get_as_string (comp);
 	g_object_unref (comp);
 
 	return calobj;
}

/* Get_object_component handler for the file backend */
static CalComponent *
cal_backend_file_get_object_component (CalBackend *backend, const char *uid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (uid != NULL, NULL);

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_assert (priv->comp_uid_hash != NULL);

	return lookup_component (cbfile, uid);
}

/* Get_timezone_object handler for the file backend */
static char *
cal_backend_file_get_timezone_object (CalBackend *backend, const char *tzid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icaltimezone *zone;
	icalcomponent *icalcomp;
	char *ical_string;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (tzid != NULL, NULL);

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_assert (priv->comp_uid_hash != NULL);

	zone = icalcomponent_get_timezone (priv->icalcomp, tzid);
	if (!zone) {
		zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
		if (!zone)
			return NULL;
	}

	icalcomp = icaltimezone_get_component (zone);
	if (!icalcomp)
		return NULL;

	ical_string = icalcomponent_as_ical_string (icalcomp);
	/* We dup the string; libical owns that memory. */
	if (ical_string)
		return g_strdup (ical_string);
	else
		return NULL;
}

/* Builds a list of UIDs from a list of CalComponent objects */
static void
build_uids_list (GList **list, GList *components)
{
	GList *l;

	for (l = components; l; l = l->next) {
		CalComponent *comp;
		const char *uid;

		comp = CAL_COMPONENT (l->data);
		cal_component_get_uid (comp, &uid);
		*list = g_list_prepend (*list, g_strdup (uid));
	}
}

/* Get_uids handler for the file backend */
static GList *
cal_backend_file_get_uids (CalBackend *backend, CalObjType type)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	GList *list;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	list = NULL;

	if (type & CALOBJ_TYPE_EVENT)
		build_uids_list (&list, priv->events);

	if (type & CALOBJ_TYPE_TODO)
		build_uids_list (&list, priv->todos);

	if (type & CALOBJ_TYPE_JOURNAL)
		build_uids_list (&list, priv->journals);

	return list;
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

/* Callback used from cal_recur_generate_instances(); adds the component's UID
 * to our hash table.
 */
static gboolean
add_instance (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	GHashTable *uid_hash;
	const char *uid;
	const char *old_uid;

	uid_hash = data;

	/* We only care that the component's UID is listed in the hash table;
	 * that's why we only allow generation of one instance (i.e. return
	 * FALSE every time).
	 */

	cal_component_get_uid (comp, &uid);

	old_uid = g_hash_table_lookup (uid_hash, uid);
	if (old_uid)
		return FALSE;

	g_hash_table_insert (uid_hash, (char *) uid, NULL);
	return FALSE;
}

/* Populates a hash table with the UIDs of the components that occur or recur
 * within a specific time range.
 */
static void
get_instances_in_range (GHashTable *uid_hash, GList *components, time_t start, time_t end, icaltimezone *default_zone)
{
	GList *l;

	for (l = components; l; l = l->next) {
		CalComponent *comp;
		icalcomponent *icalcomp, *vcalendar_comp;

		comp = CAL_COMPONENT (l->data);

		/* Get the parent VCALENDAR component, so we can resolve
		   TZIDs. */
		icalcomp = cal_component_get_icalcomponent (comp);
		vcalendar_comp = icalcomponent_get_parent (icalcomp);
		g_assert (vcalendar_comp != NULL);

		cal_recur_generate_instances (comp, start, end, add_instance, uid_hash, resolve_tzid, vcalendar_comp, default_zone);
	}
}

/* Used from g_hash_table_foreach(), adds a UID from the hash table to our list */
static void
add_uid_to_list (gpointer key, gpointer value, gpointer data)
{
	GList **list;
	const char *uid;
	char *uid_copy;

	list = data;

	uid = key;
	uid_copy = g_strdup (uid);

	*list = g_list_prepend (*list, uid_copy);
}

/* Get_objects_in_range handler for the file backend */
static GList *
cal_backend_file_get_objects_in_range (CalBackend *backend, CalObjType type,
				       time_t start, time_t end)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	GList *event_list;
	GHashTable *uid_hash;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	uid_hash = g_hash_table_new (g_str_hash, g_str_equal);

	if (type & CALOBJ_TYPE_EVENT)
		get_instances_in_range (uid_hash, priv->events, start, end,
					priv->default_zone);

	if (type & CALOBJ_TYPE_TODO)
		get_instances_in_range (uid_hash, priv->todos, start, end,
					priv->default_zone);

	if (type & CALOBJ_TYPE_JOURNAL)
		get_instances_in_range (uid_hash, priv->journals, start, end,
					priv->default_zone);

	event_list = NULL;
	g_hash_table_foreach (uid_hash, add_uid_to_list, &event_list);
	g_hash_table_destroy (uid_hash);

	return event_list;
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
	GList *uids;
	GList *l;
	icalcomponent *vfb;
	icaltimezone *utc_zone;
	
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

	uids = cal_backend_get_objects_in_range (CAL_BACKEND (cbfile),
						 CALOBJ_TYPE_ANY, start, end);
	for (l = uids; l != NULL; l = l->next) {
		CalComponent *comp;
		icalcomponent *icalcomp, *vcalendar_comp;
		icalproperty *prop;
		char *uid = (char *) l->data;

		/* get the component from our internal list */
		comp = lookup_component (cbfile, uid);
		if (!comp)
			continue;

		icalcomp = cal_component_get_icalcomponent (comp);
		if (!icalcomp)
			continue;

		/* If the event is TRANSPARENT, skip it. */
		prop = icalcomponent_get_first_property (icalcomp,
							 ICAL_TRANSP_PROPERTY);
		if (prop) {
			const char *transp_val = icalproperty_get_transp (prop);
			if (transp_val
			    && !strcasecmp (transp_val, "TRANSPARENT"))
				continue;
		}

		vcalendar_comp = icalcomponent_get_parent (icalcomp);
		cal_recur_generate_instances (comp, start, end,
					      free_busy_instance,
					      vfb,
					      resolve_tzid,
					      vcalendar_comp,
					      priv->default_zone);
		
	}
	cal_obj_uid_list_free (uids);

	return vfb;	
}

/* Get_free_busy handler for the file backend */
static GList *
cal_backend_file_get_free_busy (CalBackend *backend, GList *users, time_t start, time_t end)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	gchar *address, *name;	
	icalcomponent *vfb;
	char *calobj;
	GList *obj_list = NULL;
	GList *l;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	if (users == NULL) {
		if (cal_backend_mail_account_get_default (priv->config_listener, &address, &name)) {
			vfb = create_user_free_busy (cbfile, address, name, start, end);
			calobj = icalcomponent_as_ical_string (vfb);
			obj_list = g_list_append (obj_list, g_strdup (calobj));
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
				obj_list = g_list_append (obj_list, g_strdup (calobj));
				icalcomponent_free (vfb);
				g_free (name);
			}
		}		
	}

	return obj_list;
}

typedef struct 
{
	CalBackend *backend;
	CalObjType type;	
	GList *changes;
	GList *change_ids;
} CalBackendFileComputeChangesData;

static void
cal_backend_file_compute_changes_foreach_key (const char *key, gpointer data)
{
	CalBackendFileComputeChangesData *be_data = data;
	char *calobj = cal_backend_get_object (be_data->backend, key);
	
	if (calobj == NULL) {
		CalComponent *comp;
		GNOME_Evolution_Calendar_CalObjChange *coc;
		char *calobj;

		comp = cal_component_new ();
		if (be_data->type == GNOME_Evolution_Calendar_TYPE_TODO)
			cal_component_set_new_vtype (comp, CAL_COMPONENT_TODO);
		else
			cal_component_set_new_vtype (comp, CAL_COMPONENT_EVENT);

		cal_component_set_uid (comp, key);
		calobj = cal_component_get_as_string (comp);

		coc = GNOME_Evolution_Calendar_CalObjChange__alloc ();
		coc->calobj =  CORBA_string_dup (calobj);
		coc->type = GNOME_Evolution_Calendar_DELETED;
		be_data->changes = g_list_prepend (be_data->changes, coc);
		be_data->change_ids = g_list_prepend (be_data->change_ids, g_strdup (key));

		g_free (calobj);
		g_object_unref (comp);
 	}
}

static GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_file_compute_changes (CalBackend *backend, CalObjType type, const char *change_id)
{
	char    *filename;
	EDbHash *ehash;
	CalBackendFileComputeChangesData be_data;
	GNOME_Evolution_Calendar_CalObjChangeSeq *seq;
	GList *uids, *changes = NULL, *change_ids = NULL;
	GList *i, *j;
	int n;
	
	/* Find the changed ids - FIX ME, path should not be hard coded */
	if (type == GNOME_Evolution_Calendar_TYPE_TODO)
		filename = g_strdup_printf ("%s/evolution/local/Tasks/%s.db", g_get_home_dir (), change_id);
	else 
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
			change_ids = g_list_prepend (change_ids, g_strdup (uid));
			break;
		case E_DBHASH_STATUS_DIFFERENT:
			coc = GNOME_Evolution_Calendar_CalObjChange__alloc ();
			coc->calobj =  CORBA_string_dup (calobj);
			coc->type = GNOME_Evolution_Calendar_MODIFIED;
			changes = g_list_prepend (changes, coc);
			change_ids = g_list_prepend (change_ids, g_strdup (uid));
			break;
		}
	}

	/* Calculate deletions */
	be_data.backend = backend;
	be_data.type = type;	
	be_data.changes = changes;
	be_data.change_ids = change_ids;
   	e_dbhash_foreach_key (ehash, (EDbHashFunc)cal_backend_file_compute_changes_foreach_key, &be_data);
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
		g_free (uid);
	}	
	e_dbhash_write (ehash);
  	e_dbhash_destroy (ehash);

	cal_obj_uid_list_free (uids);
	g_list_free (change_ids);
	g_list_free (changes);
	
	return seq;
}

/* Get_changes handler for the file backend */
static GNOME_Evolution_Calendar_CalObjChangeSeq *
cal_backend_file_get_changes (CalBackend *backend, CalObjType type, const char *change_id)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	return cal_backend_file_compute_changes (backend, type, change_id);
}

/* Get_alarms_in_range handler for the file backend */
static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
cal_backend_file_get_alarms_in_range (CalBackend *backend,
				      time_t start, time_t end)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	int n_comp_alarms;
	GSList *comp_alarms;
	GSList *l;
	int i;
	CalAlarmAction omit[] = {-1};
	
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	/* Per RFC 2445, only VEVENTs and VTODOs can have alarms */

	n_comp_alarms = 0;
	comp_alarms = NULL;

	n_comp_alarms += cal_util_generate_alarms_for_list (priv->events, start, end, omit,
							    &comp_alarms, resolve_tzid,
							    priv->icalcomp,
							    priv->default_zone);
	n_comp_alarms += cal_util_generate_alarms_for_list (priv->todos, start, end, omit,
							    &comp_alarms, resolve_tzid,
							    priv->icalcomp,
							    priv->default_zone);

	seq = GNOME_Evolution_Calendar_CalComponentAlarmsSeq__alloc ();
	CORBA_sequence_set_release (seq, TRUE);
	seq->_length = n_comp_alarms;
	seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalComponentAlarms_allocbuf (
		n_comp_alarms);

	for (l = comp_alarms, i = 0; l; l = l->next, i++) {
		CalComponentAlarms *alarms;
		char *comp_str;

		alarms = l->data;

		comp_str = cal_component_get_as_string (alarms->comp);
		seq->_buffer[i].calobj = CORBA_string_dup (comp_str);
		g_free (comp_str);

		cal_backend_util_fill_alarm_instances_seq (&seq->_buffer[i].alarms, alarms->alarms);

		cal_component_alarms_free (alarms);
	}

	g_slist_free (comp_alarms);

	return seq;
}

/* Get_alarms_for_object handler for the file backend */
static GNOME_Evolution_Calendar_CalComponentAlarms *
cal_backend_file_get_alarms_for_object (CalBackend *backend, const char *uid,
					time_t start, time_t end,
					gboolean *object_found)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;
	char *comp_str;
	GNOME_Evolution_Calendar_CalComponentAlarms *corba_alarms;
	CalComponentAlarms *alarms;
	CalAlarmAction omit[] = {-1};

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (uid != NULL, NULL);
	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);
	g_return_val_if_fail (object_found != NULL, NULL);

	comp = lookup_component (cbfile, uid);
	if (!comp) {
		*object_found = FALSE;
		return NULL;
	}

	*object_found = TRUE;

	comp_str = cal_component_get_as_string (comp);
	corba_alarms = GNOME_Evolution_Calendar_CalComponentAlarms__alloc ();

	corba_alarms->calobj = CORBA_string_dup (comp_str);
	g_free (comp_str);

	alarms = cal_util_generate_alarms_for_comp (comp, start, end, omit, resolve_tzid, priv->icalcomp, priv->default_zone);
	if (alarms) {
		cal_backend_util_fill_alarm_instances_seq (&corba_alarms->alarms, alarms->alarms);
		cal_component_alarms_free (alarms);
	} else
		cal_backend_util_fill_alarm_instances_seq (&corba_alarms->alarms, NULL);

	return corba_alarms;
}

/* Discard_alarm handler for the file backend */
static CalBackendResult
cal_backend_file_discard_alarm (CalBackend *backend, const char *uid, const char *auid)
{
	/* we just do nothing with the alarm */
	return CAL_BACKEND_RESULT_SUCCESS;
}

/* Creates a CalComponent for the given icalcomponent and adds it to our
   cache. Note that the icalcomponent is not added to the toplevel
   icalcomponent here. That needs to be done elsewhere. It returns the uid
   of the added component, or NULL if it failed. */
static const char*
cal_backend_file_update_object (CalBackendFile *cbfile,
				icalcomponent *icalcomp)
{
	CalComponent *old_comp;
	CalComponent *comp;
	const char *comp_uid;
	struct icaltimetype last_modified;

	/* Create a CalComponent wrapper for the icalcomponent. */
	comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		return NULL;
	}

	/* Get the UID, and check it isn't empty. */
	cal_component_get_uid (comp, &comp_uid);
	if (!comp_uid || !comp_uid[0]) {
		g_object_unref (comp);
		return NULL;
	}

	/* Set the LAST-MODIFIED time on the component */
	last_modified = icaltime_from_timet (time (NULL), 0);
	cal_component_set_last_modified (comp, &last_modified);
	
	/* Remove any old version of the component. */
	old_comp = lookup_component (cbfile, comp_uid);
	if (old_comp)
		remove_component (cbfile, old_comp);

	/* Now add the component to our local cache, but we pass FALSE as
	   the last argument, since the libical component is assumed to have
	   been added already. */
	add_component (cbfile, comp, FALSE);

	return comp_uid;
}

static const char*
cal_backend_file_cancel_object (CalBackendFile *cbfile,
				icalcomponent *icalcomp)
{
	CalComponent *old_comp;
	icalproperty *uid;
	const char *comp_uid;

	/* Get the UID, and check it isn't empty. */
	uid = icalcomponent_get_first_property (icalcomp, ICAL_UID_PROPERTY);
	if (!uid)
		return NULL;
	comp_uid = icalproperty_get_uid (uid);
	if (!comp_uid || !comp_uid[0])
		return NULL;

	/* Find the old version of the component. */
	old_comp = lookup_component (cbfile, comp_uid);
	if (!old_comp)
		return NULL;

	/* And remove it */
	remove_component (cbfile, old_comp);
	return comp_uid;
}

/* Update_objects handler for the file backend. */
static CalBackendResult
cal_backend_file_update_objects (CalBackend *backend, const char *calobj, CalObjModType mod)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalcomponent *toplevel_comp, *icalcomp = NULL;
	icalcomponent_kind kind;
	icalproperty_method method;
	icalcomponent *subcomp;
	CalBackendResult retval = CAL_BACKEND_RESULT_SUCCESS;
	GList *updated_uids = NULL, *removed_uids = NULL, *elem;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, CAL_BACKEND_RESULT_INVALID_OBJECT);

	g_return_val_if_fail (calobj != NULL, CAL_BACKEND_RESULT_INVALID_OBJECT);

	/* Pull the component from the string and ensure that it is sane */

	toplevel_comp = icalparser_parse_string ((char *) calobj);

	if (!toplevel_comp)
		return CAL_BACKEND_RESULT_INVALID_OBJECT;

	kind = icalcomponent_isa (toplevel_comp);

	if (kind == ICAL_VEVENT_COMPONENT
	    || kind == ICAL_VTODO_COMPONENT
	    || kind == ICAL_VJOURNAL_COMPONENT) {
		/* Create a temporary toplevel component and put the VEVENT
		   or VTODO in it, to simplify the code below. */
		icalcomp = toplevel_comp;
		toplevel_comp = cal_util_new_top_level ();
		icalcomponent_add_component (toplevel_comp, icalcomp);
	} else if (kind != ICAL_VCALENDAR_COMPONENT) {
		/* We don't support this type of component */
		icalcomponent_free (toplevel_comp);
		return CAL_BACKEND_RESULT_INVALID_OBJECT;
	}

	method = icalcomponent_get_method (toplevel_comp);

	/* Step throught the VEVENT/VTODOs being added, create CalComponents
	   for them, and add them to our cache. */
	subcomp = icalcomponent_get_first_component (toplevel_comp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		/* We ignore anything except VEVENT, VTODO and VJOURNAL
		   components. */
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		if (child_kind == ICAL_VEVENT_COMPONENT
		    || child_kind == ICAL_VTODO_COMPONENT
		    || child_kind == ICAL_VJOURNAL_COMPONENT) {
			const char *comp_uid;

			if (method == ICAL_METHOD_CANCEL) {
				comp_uid = cal_backend_file_cancel_object (cbfile, subcomp);
				if (comp_uid) {
					removed_uids = g_list_prepend (removed_uids,
								       g_strdup (comp_uid));
				} else
					retval = CAL_BACKEND_RESULT_NOT_FOUND;
			} else {
				comp_uid = cal_backend_file_update_object (cbfile, subcomp);
				if (comp_uid) {
					updated_uids = g_list_prepend (updated_uids,
								       g_strdup (comp_uid));
				} else
					retval = CAL_BACKEND_RESULT_INVALID_OBJECT;
			}
		}
		subcomp = icalcomponent_get_next_component (toplevel_comp,
							    ICAL_ANY_COMPONENT);
	}

	/* Merge the iCalendar components with our existing VCALENDAR,
	   resolving any conflicting TZIDs. */
	icalcomponent_merge_component (priv->icalcomp, toplevel_comp);

	mark_dirty (cbfile);

	/* Now emit notification signals for all of the added components.
	   We do this after adding them all to make sure the calendar is in a
	   stable state before emitting signals. */
	for (elem = updated_uids; elem; elem = elem->next) {
		char *comp_uid = elem->data;
		cal_backend_notify_update (backend, comp_uid);
		g_free (comp_uid);
	}
	g_list_free (updated_uids);

	for (elem = removed_uids; elem; elem = elem->next) {
		char *comp_uid = elem->data;
		cal_backend_notify_remove (backend, comp_uid);
		g_free (comp_uid);
	}
	g_list_free (removed_uids);

	return retval;
}


/* Remove_object handler for the file backend */
static CalBackendResult
cal_backend_file_remove_object (CalBackend *backend, const char *uid, CalObjModType mod)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, CAL_BACKEND_RESULT_INVALID_OBJECT);

	g_return_val_if_fail (uid != NULL, CAL_BACKEND_RESULT_NOT_FOUND);

	comp = lookup_component (cbfile, uid);
	if (!comp)
		return CAL_BACKEND_RESULT_NOT_FOUND;

	remove_component (cbfile, comp);

	mark_dirty (cbfile);

	cal_backend_notify_remove (backend, uid);

	return CAL_BACKEND_RESULT_SUCCESS;
}

static CalBackendSendResult
cal_backend_file_send_object (CalBackend *backend, const char *calobj, char **new_calobj,
			      GNOME_Evolution_Calendar_UserList **user_list, char error_msg[256])
{
	*new_calobj = g_strdup (calobj);
	
	*user_list = GNOME_Evolution_Calendar_UserList__alloc ();
	(*user_list)->_length = 0;

	return CAL_BACKEND_SEND_SUCCESS;
}

static icaltimezone*
cal_backend_file_get_timezone (CalBackend *backend, const char *tzid)
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


static icaltimezone*
cal_backend_file_get_default_timezone (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	return priv->default_zone;
}


static gboolean
cal_backend_file_set_default_timezone (CalBackend *backend,
				       const char *tzid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icaltimezone *zone;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	/* Look up the VTIMEZONE in our icalcomponent. */
	zone = icalcomponent_get_timezone (priv->icalcomp, tzid);
	if (!zone)
		return FALSE;

	/* Set the default timezone to it. */
	priv->default_zone = zone;

	return TRUE;
}


/* Evolution calendar - iCalendar file backend
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
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

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-util/e-dbhash.h"
#include "cal-util/cal-recur.h"
#include "cal-backend-file.h"



/* Private part of the CalBackendFile structure */
struct _CalBackendFilePrivate {
	/* URI where the calendar data is stored */
	GnomeVFSURI *uri;

	/* List of Cal objects with their listeners */
	GList *clients;

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

	/* Idle handler for saving the calendar when it is dirty */
	guint idle_id;
};



static void cal_backend_file_class_init (CalBackendFileClass *class);
static void cal_backend_file_init (CalBackendFile *cbfile);
static void cal_backend_file_destroy (GtkObject *object);

static GnomeVFSURI *cal_backend_file_get_uri (CalBackend *backend);
static void cal_backend_file_add_cal (CalBackend *backend, Cal *cal);
static CalBackendOpenStatus cal_backend_file_open (CalBackend *backend, GnomeVFSURI *uri,
						   gboolean only_if_exists);

static int cal_backend_file_get_n_objects (CalBackend *backend, CalObjType type);
static char *cal_backend_file_get_object (CalBackend *backend, const char *uid);
static CalObjType cal_backend_file_get_type_by_uid (CalBackend *backend, const char *uid);
static GList *cal_backend_file_get_uids (CalBackend *backend, CalObjType type);
static GList *cal_backend_file_get_objects_in_range (CalBackend *backend, CalObjType type,
						     time_t start, time_t end);
static GNOME_Evolution_Calendar_CalObjChangeSeq *cal_backend_file_get_changes (
	CalBackend *backend, CalObjType type, const char *change_id);

static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *cal_backend_file_get_alarms_in_range (
	CalBackend *backend, time_t start, time_t end);

static GNOME_Evolution_Calendar_CalComponentAlarms *cal_backend_file_get_alarms_for_object (
	CalBackend *backend, const char *uid,
	time_t start, time_t end, gboolean *object_found);

static gboolean cal_backend_file_update_object (CalBackend *backend, const char *uid,
					       const char *calobj);
static gboolean cal_backend_file_remove_object (CalBackend *backend, const char *uid);

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
GtkType
cal_backend_file_get_type (void)
{
	static GtkType cal_backend_file_type = 0;

	if (!cal_backend_file_type) {
		static const GtkTypeInfo cal_backend_file_info = {
			"CalBackendFile",
			sizeof (CalBackendFile),
			sizeof (CalBackendFileClass),
			(GtkClassInitFunc) cal_backend_file_class_init,
			(GtkObjectInitFunc) cal_backend_file_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_backend_file_type = gtk_type_unique (CAL_BACKEND_TYPE, &cal_backend_file_info);
	}

	return cal_backend_file_type;
}

/* Class initialization function for the file backend */
static void
cal_backend_file_class_init (CalBackendFileClass *class)
{
	GtkObjectClass *object_class;
	CalBackendClass *backend_class;

	object_class = (GtkObjectClass *) class;
	backend_class = (CalBackendClass *) class;

	parent_class = gtk_type_class (CAL_BACKEND_TYPE);

	object_class->destroy = cal_backend_file_destroy;

	backend_class->get_uri = cal_backend_file_get_uri;
	backend_class->add_cal = cal_backend_file_add_cal;
	backend_class->open = cal_backend_file_open;
	backend_class->get_n_objects = cal_backend_file_get_n_objects;
	backend_class->get_object = cal_backend_file_get_object;
	backend_class->get_type_by_uid = cal_backend_file_get_type_by_uid;
	backend_class->get_uids = cal_backend_file_get_uids;
	backend_class->get_objects_in_range = cal_backend_file_get_objects_in_range;
	backend_class->get_changes = cal_backend_file_get_changes;
	backend_class->get_alarms_in_range = cal_backend_file_get_alarms_in_range;
	backend_class->get_alarms_for_object = cal_backend_file_get_alarms_for_object;
	backend_class->update_object = cal_backend_file_update_object;
	backend_class->remove_object = cal_backend_file_remove_object;
}

/* Object initialization function for the file backend */
static void
cal_backend_file_init (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;

	priv = g_new0 (CalBackendFilePrivate, 1);
	cbfile->priv = priv;
}

/* g_hash_table_foreach() callback to destroy a CalComponent */
static void
free_cal_component (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp;

	comp = CAL_COMPONENT (value);
	gtk_object_unref (GTK_OBJECT (comp));
}

/* Saves the calendar data */
static void
save (CalBackendFile *cbfile)
{
	CalBackendFilePrivate *priv;
	GnomeVFSHandle *handle = NULL;
	GnomeVFSResult result;
	GnomeVFSFileSize out;
	gchar *tmp;
	char *buf;
	
	priv = cbfile->priv;
	g_assert (priv->uri != NULL);
	g_assert (priv->icalcomp != NULL);

	/* Make a backup copy of the file if it exists */
	tmp = gnome_vfs_uri_to_string (priv->uri, GNOME_VFS_URI_HIDE_NONE);
	if (tmp) {
		GnomeVFSURI *backup_uri;
		gchar *backup_uristr;
		
		backup_uristr = g_strconcat (tmp, "~", NULL);
		backup_uri = gnome_vfs_uri_new (backup_uristr);
		
		result = gnome_vfs_move_uri (priv->uri, backup_uri, TRUE);
		gnome_vfs_uri_unref (backup_uri);
		
		g_free (tmp);
		g_free (backup_uristr);
	}
	
	/* Now write the new file out */
	result = gnome_vfs_create_uri (&handle, priv->uri, 
				       GNOME_VFS_OPEN_WRITE,
				       FALSE, 0666);
	
	if (result != GNOME_VFS_OK)
		goto error;
	
	buf = icalcomponent_as_ical_string (priv->icalcomp);
	result = gnome_vfs_write (handle, buf, strlen (buf) * sizeof (char), &out);

	if (result != GNOME_VFS_OK)
		goto error;

	gnome_vfs_close (handle);

	return;
	
 error:
	g_warning ("Error writing calendar file.");
	return;
}

/* Destroy handler for the file backend */
static void
cal_backend_file_destroy (GtkObject *object)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND_FILE (object));

	cbfile = CAL_BACKEND_FILE (object);
	priv = cbfile->priv;

	g_assert (priv->clients == NULL);

	/* Save if necessary */

	if (priv->idle_id != 0) {
		save (cbfile);
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	/* Clean up */

	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
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

	g_free (priv);
	cbfile->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
static GnomeVFSURI *
cal_backend_file_get_uri (CalBackend *backend)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_assert (priv->uri != NULL);

	return priv->uri;
}

/* Callback used when a Cal is destroyed */
static void
cal_destroy_cb (GtkObject *object, gpointer data)
{
	Cal *cal;
	Cal *lcal;
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	GList *l;

	cal = CAL (object);

	cbfile = CAL_BACKEND_FILE (data);
	priv = cbfile->priv;

	/* Find the cal in the list of clients */

	for (l = priv->clients; l; l = l->next) {
		lcal = CAL (l->data);

		if (lcal == cal)
			break;
	}

	g_assert (l != NULL);

	/* Disconnect */

	priv->clients = g_list_remove_link (priv->clients, l);
	g_list_free_1 (l);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!priv->clients)
		cal_backend_last_client_gone (CAL_BACKEND (cbfile));
}

/* Add_cal handler for the file backend */
static void
cal_backend_file_add_cal (CalBackend *backend, Cal *cal)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_if_fail (priv->icalcomp != NULL);
	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	/* We do not keep a reference to the Cal since the calendar user agent
	 * owns it.
	 */

	gtk_signal_connect (GTK_OBJECT (cal), "destroy",
			    GTK_SIGNAL_FUNC (cal_destroy_cb),
			    backend);

	priv->clients = g_list_prepend (priv->clients, cal);
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

	gtk_object_unref (GTK_OBJECT (comp));
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

/* Callback used from icalparser_parse() */
static char *
get_line_fn (char *s, size_t size, void *data)
{
	FILE *file;

	file = data;
	return fgets (s, size, file);
}

/* Parses an open iCalendar file and returns a toplevel component with the contents */
static icalcomponent *
parse_file (FILE *file)
{
	icalparser *parser;
	icalcomponent *icalcomp;

	parser = icalparser_new ();
	icalparser_set_gen_data (parser, file);

	icalcomp = icalparser_parse (parser, get_line_fn);
	icalparser_free (parser);

	return icalcomp;
}

/* Parses an open iCalendar file and loads it into the backend */
static CalBackendOpenStatus
open_cal (CalBackendFile *cbfile, GnomeVFSURI *uri, FILE *file)
{
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;

	priv = cbfile->priv;

	icalcomp = parse_file (file);

	if (fclose (file) != 0) {
		if (icalcomp)
			icalcomponent_free (icalcomp);

		return CAL_BACKEND_OPEN_ERROR;
	}

	if (!icalcomp)
		return CAL_BACKEND_OPEN_ERROR;
		
	/* FIXME: should we try to demangle XROOT components and
	 * individual components as well?
	 */

	if (icalcomponent_isa (icalcomp) != ICAL_VCALENDAR_COMPONENT)
		return CAL_BACKEND_OPEN_ERROR;

	priv->icalcomp = icalcomp;

	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	scan_vcalendar (cbfile);

	gnome_vfs_uri_ref (uri);
	priv->uri = uri;

	return CAL_BACKEND_OPEN_SUCCESS;
}

static CalBackendOpenStatus
create_cal (CalBackendFile *cbfile, GnomeVFSURI *uri)
{
	CalBackendFilePrivate *priv;
	icalproperty *prop;

	priv = cbfile->priv;

	/* Create the new calendar information */

	priv->icalcomp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);

	/* RFC 2445, section 4.7.1 */
	prop = icalproperty_new_calscale ("GREGORIAN");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* RFC 2445, section 4.7.3 */
	prop = icalproperty_new_prodid ("-//Ximian//NONSGML Evolution Calendar//EN");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* RFC 2445, section 4.7.4.  This is the iCalendar spec version, *NOT*
	 * the product version!  Do not change this!
	 */
	prop = icalproperty_new_version ("2.0");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* Create our internal data */

	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);

	gnome_vfs_uri_ref (uri);
	priv->uri = uri;

	mark_dirty (cbfile);

	return CAL_BACKEND_OPEN_SUCCESS;
}

/* Open handler for the file backend */
static CalBackendOpenStatus
cal_backend_file_open (CalBackend *backend, GnomeVFSURI *uri, gboolean only_if_exists)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	char *str_uri;
	FILE *file;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp == NULL, CAL_BACKEND_OPEN_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_OPEN_ERROR);

	g_assert (priv->uri == NULL);
	g_assert (priv->comp_uid_hash == NULL);

	if (!gnome_vfs_uri_is_local (uri))
		return CAL_BACKEND_OPEN_ERROR;

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	/* Load! */
	file = fopen (str_uri, "r");
	g_free (str_uri);

	if (file)
		return open_cal (cbfile, uri, file);
	else {
		if (only_if_exists)
			return CAL_BACKEND_OPEN_NOT_FOUND;

		return create_cal (cbfile, uri);
	}
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

/* Get_object handler for the file backend */
static char *
cal_backend_file_get_object (CalBackend *backend, const char *uid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (uid != NULL, NULL);

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);
	g_assert (priv->comp_uid_hash != NULL);

	comp = lookup_component (cbfile, uid);

	if (!comp)
		return NULL;

	return cal_component_get_as_string (comp);
}

static CalObjType
cal_backend_file_get_type_by_uid (CalBackend *backend, const char *uid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;
	CalComponentVType type;
	
	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	comp = lookup_component (cbfile, uid);
	if (!comp)
		return CAL_COMPONENT_NO_TYPE;
	
	type = cal_component_get_vtype (comp);
	switch (type) {
	case CAL_COMPONENT_EVENT:
		return CALOBJ_TYPE_EVENT;
	case CAL_COMPONENT_TODO:
		return CALOBJ_TYPE_TODO;
	case CAL_COMPONENT_JOURNAL:
		return CALOBJ_TYPE_JOURNAL;
	default:
		return CAL_COMPONENT_NO_TYPE;
	}
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
get_instances_in_range (GHashTable *uid_hash, GList *components, time_t start, time_t end)
{
	GList *l;

	for (l = components; l; l = l->next) {
		CalComponent *comp;

		comp = CAL_COMPONENT (l->data);
		cal_recur_generate_instances (comp, start, end, add_instance, uid_hash);
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
		get_instances_in_range (uid_hash, priv->events, start, end);

	if (type & CALOBJ_TYPE_TODO)
		get_instances_in_range (uid_hash, priv->todos, start, end);

	if (type & CALOBJ_TYPE_JOURNAL)
		get_instances_in_range (uid_hash, priv->journals, start, end);

	event_list = NULL;
	g_hash_table_foreach (uid_hash, add_uid_to_list, &event_list);
	g_hash_table_destroy (uid_hash);

	return event_list;
}


typedef struct 
{
	CalBackend *backend;
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

/* Computes the range of time in which recurrences should be generated for a
 * component in order to compute alarm trigger times.
 */
static void
compute_alarm_range (CalComponent *comp, GList *alarm_uids, time_t start, time_t end,
		     time_t *alarm_start, time_t *alarm_end)
{
	GList *l;

	*alarm_start = start;
	*alarm_end = end;

	for (l = alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		struct icaldurationtype *dur;
		time_t dur_time;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		switch (trigger.type) {
		case CAL_ALARM_TRIGGER_NONE:
		case CAL_ALARM_TRIGGER_ABSOLUTE:
			continue;

		case CAL_ALARM_TRIGGER_RELATIVE_START:
		case CAL_ALARM_TRIGGER_RELATIVE_END:
			dur = &trigger.u.rel_duration;
  			dur_time = icaldurationtype_as_int (*dur);

			if (dur->is_neg)
				*alarm_end = MAX (*alarm_end, end + dur_time);
			else
				*alarm_start = MIN (*alarm_start, start - dur_time);

			break;

		default:
			g_assert_not_reached ();
		}
	}

	g_assert (*alarm_start <= *alarm_end);
}

/* Closure data to generate alarm occurrences */
struct alarm_occurrence_data {
	/* These are the info we have */
	GList *alarm_uids;
	time_t start;
	time_t end;

	/* This is what we compute */
	GSList *triggers;
	int n_triggers;
};

/* Callback used from cal_recur_generate_instances(); generates triggers for all
 * of a component's RELATIVE alarms.
 */
static gboolean
add_alarm_occurrences_cb (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	struct alarm_occurrence_data *aod;
	GList *l;

	aod = data;

	for (l = aod->alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		struct icaldurationtype *dur;
		time_t dur_time;
		time_t occur_time, trigger_time;
		CalAlarmInstance *instance;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		if (trigger.type != CAL_ALARM_TRIGGER_RELATIVE_START
		    && trigger.type != CAL_ALARM_TRIGGER_RELATIVE_END)
			continue;

		dur = &trigger.u.rel_duration;
  		dur_time = icaldurationtype_as_int (*dur);

		if (trigger.type == CAL_ALARM_TRIGGER_RELATIVE_START)
			occur_time = start;
		else
			occur_time = end;

		if (dur->is_neg)
			trigger_time = occur_time - dur_time;
		else
			trigger_time = occur_time + dur_time;

		if (trigger_time < aod->start || trigger_time >= aod->end)
			continue;

		instance = g_new (CalAlarmInstance, 1);
		instance->auid = auid;
		instance->trigger = trigger_time;
		instance->occur = occur_time;

		aod->triggers = g_slist_prepend (aod->triggers, instance);
		aod->n_triggers++;
	}

	return TRUE;
}

/* Generates the absolute triggers for a component */
static void
generate_absolute_triggers (CalComponent *comp, struct alarm_occurrence_data *aod)
{
	GList *l;

	for (l = aod->alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		time_t abs_time;
		CalAlarmInstance *instance;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		if (trigger.type != CAL_ALARM_TRIGGER_ABSOLUTE)
			continue;

		abs_time = icaltime_as_timet (trigger.u.abs_time);

		if (abs_time < aod->start || abs_time >= aod->end)
			continue;

		instance = g_new (CalAlarmInstance, 1);
		instance->auid = auid;
		instance->trigger = abs_time;
		instance->occur = abs_time; /* No particular occurrence, so just use the same time */

		aod->triggers = g_slist_prepend (aod->triggers, instance);
		aod->n_triggers++;
	}
}

/* Compares two alarm instances; called from g_slist_sort() */
static gint
compare_alarm_instance (gconstpointer a, gconstpointer b)
{
	const CalAlarmInstance *aia, *aib;

	aia = a;
	aib = b;

	if (aia->trigger < aib->trigger)
		return -1;
	else if (aia->trigger > aib->trigger)
		return 1;
	else
		return 0;
}

/* Generates alarm instances for a calendar component.  Returns the instances
 * structure, or NULL if no alarm instances occurred in the specified time
 * range.
 */
static CalComponentAlarms *
generate_alarms_for_comp (CalComponent *comp, time_t start, time_t end)
{
	GList *alarm_uids;
	time_t alarm_start, alarm_end;
	struct alarm_occurrence_data aod;
	CalComponentAlarms *alarms;

	if (!cal_component_has_alarms (comp))
		return NULL;

	alarm_uids = cal_component_get_alarm_uids (comp);
	compute_alarm_range (comp, alarm_uids, start, end, &alarm_start, &alarm_end);

	aod.alarm_uids = alarm_uids;
	aod.start = start;
	aod.end = end;
	aod.triggers = NULL;
	aod.n_triggers = 0;
	cal_recur_generate_instances (comp, alarm_start, alarm_end, add_alarm_occurrences_cb, &aod);

	/* We add the ABSOLUTE triggers separately */
	generate_absolute_triggers (comp, &aod);

	if (aod.n_triggers == 0)
		return NULL;

	/* Create the component alarm instances structure */

	alarms = g_new (CalComponentAlarms, 1);
	alarms->comp = comp;
	gtk_object_ref (GTK_OBJECT (alarms->comp));
	alarms->alarms = g_slist_sort (aod.triggers, compare_alarm_instance);

	return alarms;
}

/* Iterates through all the components in the comps list and generates alarm
 * instances for them; putting them in the comp_alarms list.  Returns the number
 * of elements it added to that list.
 */
static int
generate_alarms_for_list (GList *comps, time_t start, time_t end, GSList **comp_alarms)
{
	GList *l;
	int n;

	n = 0;

	for (l = comps; l; l = l->next) {
		CalComponent *comp;
		CalComponentAlarms *alarms;

		comp = CAL_COMPONENT (l->data);
		alarms = generate_alarms_for_comp (comp, start, end);

		if (alarms) {
			*comp_alarms = g_slist_prepend (*comp_alarms, alarms);
			n++;
		}
	}

	return n;
}

/* Fills a CORBA sequence of alarm instances */
static void
fill_alarm_instances_seq (GNOME_Evolution_Calendar_CalAlarmInstanceSeq *seq, GSList *alarms)
{
	int n_alarms;
	GSList *l;
	int i;

	n_alarms = g_slist_length (alarms);

	CORBA_sequence_set_release (seq, TRUE);
	seq->_length = n_alarms;
	seq->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalAlarmInstance_allocbuf (n_alarms);

	for (l = alarms, i = 0; l; l = l->next, i++) {
		CalAlarmInstance *instance;
		GNOME_Evolution_Calendar_CalAlarmInstance *corba_instance;

		instance = l->data;
		corba_instance = seq->_buffer + i;

		corba_instance->auid = CORBA_string_dup (instance->auid);
		corba_instance->trigger = (long) instance->trigger;
		corba_instance->occur = (long) instance->occur;
	}
}

/* Get_alarms_in_range handler for the file backend */
static GNOME_Evolution_Calendar_CalComponentAlarmsSeq *
cal_backend_file_get_alarms_in_range (CalBackend *backend, time_t start, time_t end)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	int n_comp_alarms;
	GSList *comp_alarms;
	GSList *l;
	int i;
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *seq;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	/* Per RFC 2445, only VEVENTs and VTODOs can have alarms */

	n_comp_alarms = 0;
	comp_alarms = NULL;

	n_comp_alarms += generate_alarms_for_list (priv->events, start, end, &comp_alarms);
	n_comp_alarms += generate_alarms_for_list (priv->todos, start, end, &comp_alarms);

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

		fill_alarm_instances_seq (&seq->_buffer[i].alarms, alarms->alarms);

		cal_component_alarms_free (alarms);
	}

	g_slist_free (comp_alarms);

	return seq;
}

/* Get_alarms_for_object handler for the file backend */
static GNOME_Evolution_Calendar_CalComponentAlarms *
cal_backend_file_get_alarms_for_object (CalBackend *backend, const char *uid,
					time_t start, time_t end, gboolean *object_found)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;
	char *comp_str;
	GNOME_Evolution_Calendar_CalComponentAlarms *corba_alarms;
	CalComponentAlarms *alarms;

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

	alarms = generate_alarms_for_comp (comp, start, end);
	if (alarms) {
		fill_alarm_instances_seq (&corba_alarms->alarms, alarms->alarms);
		cal_component_alarms_free (alarms);
	} else
		fill_alarm_instances_seq (&corba_alarms->alarms, NULL);

	return corba_alarms;
}

/* Notifies a backend's clients that an object was updated */
static void
notify_update (CalBackendFile *cbfile, const char *uid)
{
	CalBackendFilePrivate *priv;
	GList *l;

	priv = cbfile->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_update (cal, uid);
	}
}

/* Notifies a backend's clients that an object was removed */
static void
notify_remove (CalBackendFile *cbfile, const char *uid)
{
	CalBackendFilePrivate *priv;
	GList *l;

	priv = cbfile->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_remove (cal, uid);
	}
}

/* Update_object handler for the file backend */
static gboolean
cal_backend_file_update_object (CalBackend *backend, const char *uid, const char *calobj)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	CalComponent *old_comp;
	CalComponent *comp;
	const char *comp_uid;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	/* Pull the component from the string and ensure that it is sane */

	icalcomp = icalparser_parse_string ((char *) calobj);

	if (!icalcomp)
		return FALSE;

	kind = icalcomponent_isa (icalcomp);

	if (!(kind == ICAL_VEVENT_COMPONENT
	      || kind == ICAL_VTODO_COMPONENT
	      || kind == ICAL_VJOURNAL_COMPONENT)) {
		/* We don't support this type of component */
		icalcomponent_free (icalcomp);
		return FALSE;
	}

	comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (comp, icalcomp)) {
		gtk_object_unref (GTK_OBJECT (comp));
		icalcomponent_free (icalcomp);
		return FALSE;
	}

	/* Check the UID for sanity's sake */

	cal_component_get_uid (comp, &comp_uid);

	if (strcmp (uid, comp_uid) != 0) {
		gtk_object_unref (GTK_OBJECT (comp));
		return FALSE;
	}

	/* Update the component */

	old_comp = lookup_component (cbfile, uid);

	if (old_comp)
		remove_component (cbfile, old_comp);

	add_component (cbfile, comp, TRUE);

	mark_dirty (cbfile);

	/* FIXME: do the notification asynchronously */
	notify_update (cbfile, comp_uid);

	return TRUE;
}

/* Remove_object handler for the file backend */
static gboolean
cal_backend_file_remove_object (CalBackend *backend, const char *uid)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	CalComponent *comp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);

	comp = lookup_component (cbfile, uid);
	if (!comp)
		return FALSE;

	remove_component (cbfile, comp);
	mark_dirty (cbfile);

	/* FIXME: do the notification asynchronously */
	notify_remove (cbfile, uid);

	return TRUE;
}


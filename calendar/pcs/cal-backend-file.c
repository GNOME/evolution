/* Evolution calendar - iCalendar file backend
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
static CalBackendLoadStatus cal_backend_file_load (CalBackend *backend, GnomeVFSURI *uri);
static void cal_backend_file_create (CalBackend *backend, GnomeVFSURI *uri);

static int cal_backend_file_get_n_objects (CalBackend *backend, CalObjType type);
static char *cal_backend_file_get_object (CalBackend *backend, const char *uid);
static GList *cal_backend_file_get_uids (CalBackend *backend, CalObjType type);
static GList *cal_backend_file_get_objects_in_range (CalBackend *backend, CalObjType type,
						     time_t start, time_t end);
static GList *cal_backend_file_get_alarms_in_range (CalBackend *backend, time_t start, time_t end);
static gboolean cal_backend_file_get_alarms_for_object (CalBackend *backend, const char *uid,
							time_t start, time_t end,
							GList **alarms);
static gboolean cal_backend_file_update_object (CalBackend *backend, const char *uid,
					       const char *calobj);
static gboolean cal_backend_file_remove_object (CalBackend *backend, const char *uid);
static char *cal_backend_file_get_uid_by_pilot_id (CalBackend *backend, unsigned long int pilot_id);
static void cal_backend_file_update_pilot_id (CalBackend *backend,
					      const char *uid,
					      unsigned long int pilot_id,
					      unsigned long int pilot_status);


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
	backend_class->load = cal_backend_file_load;
	backend_class->create = cal_backend_file_create;
	backend_class->get_n_objects = cal_backend_file_get_n_objects;
	backend_class->get_object = cal_backend_file_get_object;
	backend_class->get_uids = cal_backend_file_get_uids;
	backend_class->get_objects_in_range = cal_backend_file_get_objects_in_range;
	backend_class->get_alarms_in_range = cal_backend_file_get_alarms_in_range;
	backend_class->get_alarms_for_object = cal_backend_file_get_alarms_for_object;
	backend_class->update_object = cal_backend_file_update_object;
	backend_class->remove_object = cal_backend_file_remove_object;
	backend_class->get_uid_by_pilot_id = cal_backend_file_get_uid_by_pilot_id;
	backend_class->update_pilot_id = cal_backend_file_update_pilot_id;
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
	GnomeVFSURI *backup_uri;
	char *buf;
	
	priv = cbfile->priv;
	g_assert (priv->uri != NULL);
	g_assert (priv->icalcomp != NULL);

	/* Make a backup copy of the file */
	backup_uri = gnome_vfs_uri_append_file_name (priv->uri, "~");
	result = gnome_vfs_move_uri (priv->uri, backup_uri, TRUE);
	gnome_vfs_uri_unref (backup_uri);

//	if (result != GNOME_VFS_OK)
//		goto error;
	
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
		g_hash_table_foreach (priv->comp_uid_hash, free_cal_component, NULL);
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
add_component (CalBackendFile *cbfile, CalComponent *comp)
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

	g_hash_table_insert (priv->comp_uid_hash, (char *) uid, comp);
	*list = g_list_prepend (*list, comp);
}

/* Removes a component from the backend's hash and lists.  Does not perform
 * notification on the clients.
 */
static void
remove_component (CalBackendFile *cbfile, CalComponent *comp)
{
	CalBackendFilePrivate *priv;
	const char *uid;
	GList **list, *l;

	priv = cbfile->priv;

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
	icalcomponent *icalcomp;

	priv = cbfile->priv;
	g_assert (priv->icalcomp != NULL);
	g_assert (priv->comp_uid_hash != NULL);

	for (icalcomp = icalcomponent_get_first_component (priv->icalcomp, ICAL_ANY_COMPONENT);
	     icalcomp;
	     icalcomp = icalcomponent_get_next_component (priv->icalcomp, ICAL_ANY_COMPONENT)) {
		icalcomponent_kind kind;
		CalComponent *comp;

		kind = icalcomponent_isa (icalcomp);

		if (!(kind == ICAL_VEVENT_COMPONENT
		      || kind == ICAL_VTODO_COMPONENT
		      || kind == ICAL_VJOURNAL_COMPONENT))
			continue;

		comp = cal_component_new ();

		if (!cal_component_set_icalcomponent (comp, icalcomp))
			continue;

		add_component (cbfile, comp);
	}
}

/* Callback used from icalparser_parse() */
static char *
get_line (char *s, size_t size, void *data)
{
	FILE *file;

	file = data;
	return fgets (s, size, file);
}

/* Load handler for the file backend */
static CalBackendLoadStatus
cal_backend_file_load (CalBackend *backend, GnomeVFSURI *uri)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	char *str_uri;
	FILE *file;
	icalparser *parser;
	icalcomponent *icalcomp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp == NULL, CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_LOAD_ERROR);

	/* FIXME: this looks rather bad; maybe we should check for local files
	 * and fail if they are remote.
	 */

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	/* Load! */

	file = fopen (str_uri, "r");
	g_free (str_uri);

	if (!file)
		return CAL_BACKEND_LOAD_ERROR;

	parser = icalparser_new ();
	icalparser_set_gen_data (parser, file);

	icalcomp = icalparser_parse (parser, get_line);
	icalparser_free (parser);

	if (fclose (file) != 0)
		return CAL_BACKEND_LOAD_ERROR;

	if (!icalcomp)
		return CAL_BACKEND_LOAD_ERROR;

	/* FIXME: should we try to demangle XROOT components and individual
         * components as well?
	 */

	if (icalcomponent_isa (icalcomp) != ICAL_VCALENDAR_COMPONENT)
		return CAL_BACKEND_LOAD_ERROR;

	priv->icalcomp = icalcomp;

	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
	scan_vcalendar (cbfile);

	/* Clean up */

	gnome_vfs_uri_ref (uri);
	priv->uri = uri;

	return CAL_BACKEND_LOAD_SUCCESS;
}

/* Create handler for the file backend */
static void
cal_backend_file_create (CalBackend *backend, GnomeVFSURI *uri)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;
	icalproperty *prop;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_if_fail (priv->icalcomp == NULL);
	g_return_if_fail (uri != NULL);

	/* Create the new calendar information */

	g_assert (priv->icalcomp == NULL);
	priv->icalcomp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);

	/* RFC 2445, section 4.7.1 */
	prop = icalproperty_new_calscale ("GREGORIAN");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* RFC 2445, section 4.7.3 */
	prop = icalproperty_new_prodid ("-//Helix Code//NONSGML Evolution Calendar//EN");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* RFC 2445, section 4.7.4 */
	prop = icalproperty_new_version ("2.0");
	icalcomponent_add_property (priv->icalcomp, prop);

	/* Create our internal data */

	g_assert (priv->comp_uid_hash == NULL);
	priv->comp_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);

	/* Done */

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;

	mark_dirty (cbfile);
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

/* Get_alarms_in_range handler for the file backend */
static GList *
cal_backend_file_get_alarms_in_range (CalBackend *backend, time_t start, time_t end)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	/* FIXME: have to deal with an unknown number of alarms; we can't just
	 * do the same thing as in cal-backend-imc.
	 */
	return NULL;
}

/* Get_alarms_for_object handler for the file backend */
static gboolean
cal_backend_file_get_alarms_for_object (CalBackend *backend, const char *uid,
					time_t start, time_t end,
					GList **alarms)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (start != -1 && end != -1, FALSE);
	g_return_val_if_fail (start <= end, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);

	/* FIXME */

	*alarms = NULL;
	return FALSE;
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
	icalcomponent *icalcomp, *icalcomp2;
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

	if (old_comp) {	
		/* Remove it from our calendar component */
		icalcomp2 = cal_component_get_icalcomponent (old_comp);
		icalcomponent_remove_component (priv->icalcomp, icalcomp2);

		remove_component (cbfile, old_comp);
	}
	
	/* Add it to our calendar component */
	icalcomp2 = cal_component_get_icalcomponent (comp);
	icalcomponent_add_component (priv->icalcomp, icalcomp2);

	add_component (cbfile, comp);
#if 0
	/* FIXME */
	new_ico->pilot_status = ICAL_PILOT_SYNC_MOD;
#endif

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
	icalcomponent *icalcomp;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);

	comp = lookup_component (cbfile, uid);
	if (!comp)
		return FALSE;

	/* Remove it from our calendar component */
	icalcomp = cal_component_get_icalcomponent (comp);
	icalcomponent_remove_component (priv->icalcomp, icalcomp);

	remove_component (cbfile, comp);

	mark_dirty (cbfile);

	/* FIXME: do the notification asynchronously */
	notify_remove (cbfile, uid);

	return TRUE;
}

/* Get_uid_by_pilot_id handler for the file backend */
static char *
cal_backend_file_get_uid_by_pilot_id (CalBackend *backend, unsigned long int pilot_id)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	/* FIXME */
	return NULL;
}

/* Update_pilot_id handler for the file backend */
static void
cal_backend_file_update_pilot_id (CalBackend *backend,
				  const char *uid,
				  unsigned long int pilot_id,
				  unsigned long int pilot_status)
{
	CalBackendFile *cbfile;
	CalBackendFilePrivate *priv;

	cbfile = CAL_BACKEND_FILE (backend);
	priv = cbfile->priv;

	g_return_if_fail (priv->icalcomp != NULL);

	g_return_if_fail (uid != NULL);

	/* FIXME */
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar backend
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
#include "icalendar.h"




/* Private part of the CalBackend structure */
typedef struct {
	/* URI where the calendar data is stored */
	GnomeVFSURI *uri;

        /* format of this calendar (ical or vcal) */
	CalendarFormat format;

	/* List of Cal objects with their listeners */
	GList *clients;

	/* All the iCalObject structures in the calendar, hashed by UID.  The
	 * hash key *is* icalobj->uid; it is not copied, so don't free it when
	 * you remove an object from the hash table.
	 */
	GHashTable *object_hash;

	/* All events, TODOs, and journals in the calendar */
	GList *events;
	GList *todos;
	GList *journals;

	/* Whether a calendar has been loaded */
	guint loaded : 1;

	/* Do we need to sync to permanent storage? */
	gboolean dirty : 1;
} CalBackendPrivate;



static void cal_backend_class_init (CalBackendClass *class);
static void cal_backend_init (CalBackend *backend);
static void cal_backend_destroy (GtkObject *object);

static GtkObjectClass *parent_class;



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
}

/* Object initialization function for the calendar backend */
static void
cal_backend_init (CalBackend *backend)
{
	CalBackendPrivate *priv;

	priv = g_new0 (CalBackendPrivate, 1);
	backend->priv = priv;

	/* FIXME can be CAL_VCAL or CAL_ICAL */
	priv->format = CAL_VCAL;
}

static void save_to_vcal (CalBackend *backend, char *fname)
{
	FILE *fp;
	CalBackendPrivate *priv = backend->priv;
	VObject *vcal;
	GList *l;

	if (g_file_exists (fname)){
		char *backup_name = g_strconcat (fname, "~", NULL);

		if (g_file_exists (backup_name)){
			unlink (backup_name);
		}
		rename (fname, backup_name);
		g_free (backup_name);
	}

	vcal = newVObject (VCCalProp);
	addPropValue (vcal, VCProdIdProp,
		      "-//GNOME//NONSGML GnomeCalendar//EN");
	addPropValue (vcal, VCVersionProp, VERSION);

	for (l = priv->events; l; l = l->next) {
		iCalObject *ical = l->data;
		VObject *vobject = ical_object_to_vobject (ical);
		addVObjectProp (vcal, vobject);
	}
	for (l = priv->todos; l; l = l->next) {
		iCalObject *ical = l->data;
		VObject *vobject = ical_object_to_vobject (ical);
		addVObjectProp (vcal, vobject);
	}
	for (l = priv->journals; l; l = l->next) {
		iCalObject *ical = l->data;
		VObject *vobject = ical_object_to_vobject (ical);
		addVObjectProp (vcal, vobject);
	}

	fp = fopen(fname,"w");
	if (fp) {
		writeVObject(fp, vcal);
		fclose(fp);
	}
	cleanStrTbl ();
}


/* Saves a calendar */
static void
save (CalBackend *backend)
{
	char *str_uri;
	CalBackendPrivate *priv = backend->priv;

	str_uri = gnome_vfs_uri_to_string (priv->uri,
				   (GNOME_VFS_URI_HIDE_USER_NAME
				    | GNOME_VFS_URI_HIDE_PASSWORD
				    | GNOME_VFS_URI_HIDE_HOST_NAME
				    | GNOME_VFS_URI_HIDE_HOST_PORT
				    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	if (! priv->dirty){
		return;
	}

	switch (priv->format) {
	case CAL_VCAL:
		save_to_vcal (backend, str_uri);
		break;
	case CAL_ICAL:
		/*icalendar_calendar_save (backend, str_uri);*/
		/* FIX ME */
		break;
	default:
		/* FIX ME log */
	        break;
	}

	printf ("cal-backend: '%s' saved\n", str_uri);
}


/* g_hash_table_foreach() callback to destroy an iCalObject */
static void
free_ical_object (gpointer key, gpointer value, gpointer data)
{
	iCalObject *ico;

	ico = value;
	ical_object_destroy (ico);
}

/* Destroys a backend's data */
static void
destroy (CalBackend *backend)
{
	CalBackendPrivate *priv;

	priv = backend->priv;

	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
	}

	g_assert (priv->clients == NULL);

	if (priv->object_hash) {
		g_hash_table_foreach (priv->object_hash, free_ical_object, NULL);
		g_hash_table_destroy (priv->object_hash);
		priv->object_hash = NULL;
	}

	g_list_free (priv->events);
	g_list_free (priv->todos);
	g_list_free (priv->journals);

	priv->events = NULL;
	priv->todos = NULL;
	priv->journals = NULL;

	priv->loaded = FALSE;
}

/* Destroy handler for the calendar backend */
static void
cal_backend_destroy (GtkObject *object)
{
	CalBackend *backend;
	CalBackendPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND (object));

	backend = CAL_BACKEND (object);
	priv = backend->priv;

	if (priv->loaded)
		save (backend);

	destroy (backend);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* iCalObject manipulation functions */

/* Looks up an object by its UID in the backend's object hash table */
static iCalObject *
lookup_object (CalBackend *backend, const char *uid)
{
	CalBackendPrivate *priv;
	iCalObject *ico;

	priv = backend->priv;
	ico = g_hash_table_lookup (priv->object_hash, uid);

	return ico;
}

/* Ensures that an iCalObject has a unique identifier.  If it doesn't have one,
 * it will create one for it.  Returns whether an UID was created or not.
 */
static gboolean
ensure_uid (iCalObject *ico)
{
	char *buf;
	gulong str_time;
	static guint seqno = 0;

	if (ico->uid){
		return FALSE;
	}

	str_time = (gulong) time (NULL);

	/* Is this good enough? */

	buf = g_strdup_printf ("Evolution-Tlacuache-%d-%ld-%u",
			       (int) getpid(), str_time, seqno++);
	ico->uid = buf;
	return TRUE;
}

/* Adds an object to the calendar backend.  Does *not* perform notification to
 * calendar clients.
 */
static void
add_object (CalBackend *backend, iCalObject *ico)
{
	CalBackendPrivate *priv;

	g_assert (ico != NULL);
	priv = backend->priv;

#if 0
	/* FIXME: gnomecal old code */
	ico->new = 0;
#endif

	if (ensure_uid (ico))
		/* FIXME: mark the calendar as dirty so that we can re-save it
		 * with the object's new UID.
		 */
		;

	g_hash_table_insert (priv->object_hash, ico->uid, ico);

	priv->dirty = TRUE;

	switch (ico->type) {
	case ICAL_EVENT:
		priv->events = g_list_prepend (priv->events, ico);
#if 0
		/* FIXME: gnomecal old code */
		ical_object_try_alarms (ico);
#  ifdef DEBUGGING_MAIL_ALARM
		ico->malarm.trigger = 0;
		calendar_notify (0, ico);
#  endif
#endif
		break;

	case ICAL_TODO:
		priv->todos = g_list_prepend (priv->todos, ico);
		break;

	case ICAL_JOURNAL:
		priv->journals = g_list_prepend (priv->journals, ico);
		break;

	default:
		g_assert_not_reached ();
	}

#if 0
	/* FIXME: gnomecal old code */
	ico->last_mod = time (NULL);
#endif

	/*save (backend);*/
}

/* Removes an object from the backend's hash and lists.  Does not perform
 * notification on the clients.
 */
static void
remove_object (CalBackend *backend, iCalObject *ico)
{
	CalBackendPrivate *priv;
	GList **list, *l;

	priv = backend->priv;

	g_assert (ico->uid != NULL);
	g_hash_table_remove (priv->object_hash, ico->uid);

	priv->dirty = TRUE;

	switch (ico->type) {
	case ICAL_EVENT:
		list = &priv->events;
		break;

	case ICAL_TODO:
		list = &priv->todos;
		break;

	case ICAL_JOURNAL:
		list = &priv->journals;
		break;

	default:
		list = NULL;
	}

	if (!list){
		return;
	}

	l = g_list_find (*list, ico);
	g_assert (l != NULL);

	*list = g_list_remove_link (*list, l);
	g_list_free_1 (l);

	ical_object_destroy (ico);
	save (backend);
}

/* Load a calendar from a VObject */
static void
load_from_vobject (CalBackend *backend, VObject *vobject)
{
	CalBackendPrivate *priv;
	VObjectIterator i;

	priv = backend->priv;

	g_assert (!priv->loaded);
	g_assert (priv->object_hash == NULL);
	priv->object_hash = g_hash_table_new (g_str_hash, g_str_equal);

	initPropIterator (&i, vobject);

	while (moreIteration (&i)) {
		VObject *this;
		iCalObject *ical;
		const char *object_name;

		this = nextVObject (&i);
		object_name = vObjectName (this);
#if 0
		/* FIXME?  What is this used for in gnomecal? */
		if (strcmp (object_name, VCDCreatedProp) == 0) {
			cal->created = time_from_isodate (str_val (this));
			continue;
		}
#endif
		if (strcmp (object_name, VCLocationProp) == 0)
			continue; /* FIXME: imlement */

		if (strcmp (object_name, VCProdIdProp) == 0)
			continue; /* FIXME: implement */

		if (strcmp (object_name, VCVersionProp) == 0)
			continue; /* FIXME: implement */

		if (strcmp (object_name, VCTimeZoneProp) == 0)
			continue; /* FIXME: implement */

		ical = ical_object_create_from_vobject (this, object_name);

		/* FIXME: some broken files may have duplicated UIDs.  This is
		 * Bad(tm).  Deal with it by creating new UIDs for them and
		 * spitting some messages to the console.
		 */

		if (ical)
			add_object (backend, ical);
	}
}



/**
 * cal_backend_new:
 * @void:
 *
 * Creates a new empty calendar backend.  A calendar must then be loaded or
 * created before the backend can be used.
 *
 * Return value: A newly-created calendar backend.
 **/
CalBackend *
cal_backend_new (void)
{
	return CAL_BACKEND (gtk_type_new (CAL_BACKEND_TYPE));
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
	CalBackendPrivate *priv;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, NULL);
	g_assert (priv->uri != NULL);

	return priv->uri;
}

/* Callback used when a Cal is destroyed */
static void
cal_destroy_cb (GtkObject *object, gpointer data)
{
	Cal *cal;
	Cal *lcal;
	CalBackend *backend;
	CalBackendPrivate *priv;
	GList *l;

	cal = CAL (object);

	backend = CAL_BACKEND (data);
	priv = backend->priv;

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

	/* When all clients go away, the backend can go away, too.  Commit
         * suicide here.
	 */

	if (!priv->clients)
		gtk_object_unref (GTK_OBJECT (backend));
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
	CalBackendPrivate *priv;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	priv = backend->priv;
	g_return_if_fail (priv->loaded);

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


static icalcomponent* 
icalendar_parse_file (char* fname)
{
	FILE* fp;
	icalcomponent* comp = NULL;
	gchar* str;
	struct stat st;
	int n;

	fp = fopen (fname, "r");
	if (!fp) {
		g_warning ("Cannot open open calendar file.");
		return NULL;
	}
	
	stat (fname, &st);
	
	str = g_malloc (st.st_size + 2);
	
	n = fread ((gchar*) str, 1, st.st_size, fp);
	if (n != st.st_size) {
		g_warning ("Read error.");
	}
	str[n] = '\0';

	fclose (fp);
	
	comp = icalparser_parse_string (str);
	g_free (str);

	return comp;
}


static void
icalendar_calendar_load (CalBackend * cal, char* fname)
{
	icalcomponent *comp;
	icalcomponent *subcomp;
	iCalObject    *ical;

	comp = icalendar_parse_file (fname);
	subcomp = icalcomponent_get_first_component (comp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		ical = ical_object_create_from_icalcomponent (subcomp);
		if (ical->type != ICAL_EVENT && 
		    ical->type != ICAL_TODO  &&
		    ical->type != ICAL_JOURNAL) {
		       g_warning ("Skipping unsupported iCalendar component.");
		} else
			add_object (cal, ical);
		subcomp = icalcomponent_get_next_component (comp,
							   ICAL_ANY_COMPONENT);
	}
}


/*
ics is to be used to designate a file containing (an arbitrary set of)
calendaring and scheduling information.

ifb is to be used to designate a file containing free or busy time
information.

anything else is assumed to be a vcal file.
*/

static CalendarFormat
cal_get_type_from_filename (char *str_uri)
{
	int len;

	if (str_uri == NULL){
		return CAL_VCAL;
	}

	len = strlen (str_uri);
	if (len < 5){
		return CAL_VCAL;
	}

	if (str_uri[ len-4 ] == '.' &&
	    str_uri[ len-3 ] == 'i' &&
	    str_uri[ len-2 ] == 'c' &&
	    str_uri[ len-1 ] == 's'){
		return CAL_ICAL;
	}

	if (str_uri[ len-4 ] == '.' &&
	    str_uri[ len-3 ] == 'i' &&
	    str_uri[ len-2 ] == 'f' &&
	    str_uri[ len-1 ] == 'b'){
		return CAL_ICAL;
	}

	return CAL_VCAL;
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
	CalBackendPrivate *priv;
	VObject *vobject;
	char *str_uri;

	g_return_val_if_fail (backend != NULL, CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (IS_CAL_BACKEND (backend),CAL_BACKEND_LOAD_ERROR);

	priv = backend->priv;
	g_return_val_if_fail (!priv->loaded, CAL_BACKEND_LOAD_ERROR);

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


	/* look at the extension on the filename and decide
	   if this is a ical or vcal file */

	priv->format = cal_get_type_from_filename (str_uri);

	/* load */

	switch (priv->format) {
	case CAL_VCAL:
	        vobject = Parse_MIME_FromFileName (str_uri);
	
		if (!vobject){
			return CAL_BACKEND_LOAD_ERROR;
		}
	
		load_from_vobject (backend, vobject);
		cleanVObject (vobject);
		cleanStrTbl ();
		break;
	case CAL_ICAL:
		icalendar_calendar_load (backend, str_uri);
		break;
	default:
	        return CAL_BACKEND_LOAD_ERROR;
	}

	g_free (str_uri);

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;
	priv->loaded = TRUE;

	return CAL_BACKEND_LOAD_SUCCESS;
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
	CalBackendPrivate *priv;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	priv = backend->priv;
	g_return_if_fail (!priv->loaded);

	g_return_if_fail (uri != NULL);

	/* Create the new calendar information */

	g_assert (priv->object_hash == NULL);
	priv->object_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->dirty = TRUE;

	/* Done */

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;
	priv->loaded = TRUE;

	save (backend);
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
	CalBackendPrivate *priv;
	iCalObject *ico;
	char *buf;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, NULL);

	g_return_val_if_fail (uid != NULL, NULL);

	g_assert (priv->object_hash != NULL);

	ico = lookup_object (backend, uid);

	if (!ico){
		return NULL;
	}

	buf = ical_object_to_string (ico);

	return buf;
}

struct get_uids_closure {
	CalObjType type;
	GList *uid_list;
};

/* Builds a list of UIDs for objects that match the sought type.  Called from
 * g_hash_table_foreach().
 */
static void
build_uids_list (gpointer key, gpointer value, gpointer data)
{
	iCalObject *ico;
	struct get_uids_closure *c;
	gboolean store;

	ico = value;
	c = data;

	store = FALSE;

	if (c->type & CALOBJ_TYPE_ANY)
		store = TRUE;
	else if (ico->type == ICAL_EVENT)
		store = (c->type & CALOBJ_TYPE_EVENT) ? TRUE : FALSE;
	else if (ico->type == ICAL_TODO)
		store = (c->type & CALOBJ_TYPE_TODO) ? TRUE : FALSE;
	else if (ico->type == ICAL_JOURNAL)
		store = (c->type & CALOBJ_TYPE_JOURNAL) ? TRUE : FALSE;
	else
		store = (c->type & CALOBJ_TYPE_OTHER) ? TRUE : FALSE;

	if (store)
		c->uid_list = g_list_prepend (c->uid_list, g_strdup (ico->uid));
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
	CalBackendPrivate *priv;
	struct get_uids_closure c;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, NULL);

	/* We go through the hash table instead of the lists of particular
	 * object types so that we can pick up CALOBJ_TYPE_OTHER objects.
	 */

	c.type = type;
	c.uid_list = NULL;
	g_hash_table_foreach (priv->object_hash, build_uids_list, &c);

	return c.uid_list;
}

struct build_event_list_closure {
	CalBackend *backend;
	GList *event_list;
};

/* Builds a sorted list of event object instances.  Used as a callback from
 * ical_object_generate_events().
 */
static int
build_event_list (iCalObject *ico, time_t start, time_t end, void *data)
{
	CalObjInstance *icoi;
	struct build_event_list_closure *c;

	c = data;

	icoi = g_new (CalObjInstance, 1);

	g_assert (ico->uid != NULL);
	icoi->uid = g_strdup (ico->uid);
	icoi->start = start;
	icoi->end = end;

	c->event_list = g_list_prepend (c->event_list, icoi);

	return TRUE;
}

/* Compares two CalObjInstance structures by their start times.  Called from
 * g_list_sort().
 */
static gint
compare_instance_func (gconstpointer a, gconstpointer b)
{
	const CalObjInstance *ca, *cb;
	time_t diff;

	ca = a;
	cb = b;

	diff = ca->start - cb->start;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
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
	CalBackendPrivate *priv;
	struct build_event_list_closure c;
	GList *l;

	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	c.backend = backend;
	c.event_list = NULL;

	for (l = priv->events; l; l = l->next) {
		iCalObject *ico;

		ico = l->data;
		ical_object_generate_events (ico, start, end,
					     build_event_list, &c);
	}

	c.event_list = g_list_sort (c.event_list, compare_instance_func);

	return c.event_list;
}

/* Notifies a backend's clients that an object was updated */
static void
notify_update (CalBackend *backend, const char *uid)
{
	CalBackendPrivate *priv;
	GList *l;

	priv = backend->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_update (cal, uid);
	}
}

/* Notifies a backend's clients that an object was removed */
static void
notify_remove (CalBackend *backend, const char *uid)
{
	CalBackendPrivate *priv;
	GList *l;

	priv = backend->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_remove (cal, uid);
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
 * Return value: TRUE on success, FALSE on being passed an invalid object.
 **/
gboolean
cal_backend_update_object (CalBackend *backend, const char *uid,
			   const char *calobj)
{
	CalBackendPrivate *priv;
	iCalObject *ico, *new_ico;
	CalObjFindStatus status;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, FALSE);
	
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	/* Pull the object from the string */

	status = ical_object_find_in_string (uid, calobj, &new_ico);

	if (status != CAL_OBJ_FIND_SUCCESS){
		return FALSE;
	}

	/* Update the object */

	ico = lookup_object (backend, uid);

	if (ico)
		remove_object (backend, ico);

	add_object (backend, new_ico);
	save (backend);

	/* FIXME: do the notification asynchronously */

	notify_update (backend, new_ico->uid);

	return TRUE;
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
	CalBackendPrivate *priv;
	iCalObject *ico;

	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), FALSE);

	priv = backend->priv;
	g_return_val_if_fail (priv->loaded, FALSE);
	
	g_return_val_if_fail (uid != NULL, FALSE);

	ico = lookup_object (backend, uid);
	if (!ico){
		return FALSE;
	}

	remove_object (backend, ico);

	priv->dirty = TRUE;
	save (backend);

	/* FIXME: do the notification asynchronously */
	notify_remove (backend, uid);

	return TRUE;
}

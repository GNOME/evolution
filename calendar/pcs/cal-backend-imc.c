/* Evolution calendar - Internet Mail Consortium formats backend
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@helixcode.com>
 *          Seth Alves <alves@helixcode.com>
 *          Miguel de Icaza <miguel@helixcode.com>
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
#include "cal-backend-imc.h"
#include "cal-util/icalendar.h"



/* Supported calendar formats from the IMC */
typedef enum {
	CAL_FORMAT_UNKNOWN,
	CAL_FORMAT_VCALENDAR,
	CAL_FORMAT_ICALENDAR
} CalendarFormat;

/* Private part of the CalBackendIMC structure */
typedef struct {
	/* URI where the calendar data is stored */
	GnomeVFSURI *uri;

        /* Format of this calendar (iCalendar or vCalendar) */
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
} IMCPrivate;



static void cal_backend_imc_class_init (CalBackendIMCClass *class);
static void cal_backend_imc_init (CalBackendIMC *bimc);
static void cal_backend_imc_destroy (GtkObject *object);

static GnomeVFSURI *cal_backend_imc_get_uri (CalBackend *backend);
static void cal_backend_imc_add_cal (CalBackend *backend, Cal *cal);
static CalBackendLoadStatus cal_backend_imc_load (CalBackend *backend, GnomeVFSURI *uri);
static void cal_backend_imc_create (CalBackend *backend, GnomeVFSURI *uri);

static int cal_backend_imc_get_n_objects (CalBackend *backend, CalObjType type);
static char *cal_backend_imc_get_object (CalBackend *backend, const char *uid);
static GList *cal_backend_imc_get_uids (CalBackend *backend, CalObjType type);
static GList *cal_backend_imc_get_events_in_range (CalBackend *backend, time_t start, time_t end);
static GList *cal_backend_imc_get_alarms_in_range (CalBackend *backend, time_t start, time_t end);
static gboolean cal_backend_imc_get_alarms_for_object (CalBackend *backend, const char *uid,
						       time_t start, time_t end,
						       GList **alarms);
static gboolean cal_backend_imc_update_object (CalBackend *backend, const char *uid,
					       const char *calobj);
static gboolean cal_backend_imc_remove_object (CalBackend *backend, const char *uid);

static CalBackendClass *parent_class;



/**
 * cal_backend_imc_get_type:
 * @void:
 *
 * Registers the #CalBackendIMC class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalBackendIMC class.
 **/
GtkType
cal_backend_imc_get_type (void)
{
	static GtkType cal_backend_imc_type = 0;

	if (!cal_backend_imc_type) {
		static const GtkTypeInfo cal_backend_imc_info = {
			"CalBackendIMC",
			sizeof (CalBackendIMC),
			sizeof (CalBackendIMCClass),
			(GtkClassInitFunc) cal_backend_imc_class_init,
			(GtkObjectInitFunc) cal_backend_imc_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_backend_imc_type = gtk_type_unique (CAL_BACKEND_TYPE, &cal_backend_imc_info);
	}

	return cal_backend_imc_type;
}

/* Class initialization function for the IMC backend */
static void
cal_backend_imc_class_init (CalBackendIMCClass *class)
{
	GtkObjectClass *object_class;
	CalBackendClass *backend_class;

	object_class = (GtkObjectClass *) class;
	backend_class = (CalBackendClass *) class;

	parent_class = gtk_type_class (CAL_BACKEND_TYPE);

	backend_class->get_uri = cal_backend_imc_get_uri;
	backend_class->add_cal = cal_backend_imc_add_cal;
	backend_class->load = cal_backend_imc_load;
	backend_class->create = cal_backend_imc_create;
	backend_class->get_n_objects = cal_backend_imc_get_n_objects;
	backend_class->get_object = cal_backend_imc_get_object;
	backend_class->get_uids = cal_backend_imc_get_uids;
	backend_class->get_events_in_range = cal_backend_imc_get_events_in_range;
	backend_class->get_alarms_in_range = cal_backend_imc_get_alarms_in_range;
	backend_class->get_alarms_for_object = cal_backend_imc_get_alarms_for_object;
	backend_class->update_object = cal_backend_imc_update_object;
	backend_class->remove_object = cal_backend_imc_remove_object;

	object_class->destroy = cal_backend_imc_destroy;
}

/* Object initialization function for the IMC backend */
static void
cal_backend_imc_init (CalBackendIMC *cbimc)
{
	IMCPrivate *priv;

	priv = g_new0 (IMCPrivate, 1);
	cbimc->priv = priv;

	priv->format = CAL_FORMAT_UNKNOWN;
}

static void
save_to_vcal (CalBackendIMC *cbimc, char *fname)
{
	FILE *fp;
	IMCPrivate *priv;
	VObject *vcal;
	GList *l;

	priv = cbimc->priv;

	if (g_file_exists (fname)) {
		char *backup_name = g_strconcat (fname, "~", NULL);

		/* FIXME: do error checking on system calls!!!! */

		if (g_file_exists (backup_name))
			unlink (backup_name);

		rename (fname, backup_name);
		g_free (backup_name);
	}

	vcal = newVObject (VCCalProp);
	addPropValue (vcal, VCProdIdProp,
		      "-//Helix Code//NONSGML Evolution Calendar//EN");

	/* Per the vCalendar spec, this must be "1.0" */
	addPropValue (vcal, VCVersionProp, "1.0");

	/* FIXME: this should really iterate over the object hash table instead
	 * of the lists; that way we won't lose objects if they are of a type
	 * that we don't support but are in the calendar anyways.
	 */

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
	cleanVObject (vcal);
	cleanStrTbl ();
}

/* Saves a calendar */
static void
save (CalBackendIMC *cbimc)
{
	char *str_uri;
	IMCPrivate *priv = cbimc->priv;

	str_uri = gnome_vfs_uri_to_string (priv->uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	if (!priv->dirty)
		return;

	switch (priv->format) {
	case CAL_FORMAT_VCALENDAR:
		save_to_vcal (cbimc, str_uri);
		break;

	case CAL_FORMAT_ICALENDAR:
		/*icalendar_calendar_save (cbimc, str_uri);*/
		/* FIX ME */
		break;

	default:
		g_message ("save(): Attempt to save a calendar with an unknown format!");
	        break;
	}

	printf ("cal-backend-imc: '%s' saved\n", str_uri);

	g_free (str_uri);
}

/* g_hash_table_foreach() callback to destroy an iCalObject */
static void
free_ical_object (gpointer key, gpointer value, gpointer data)
{
	iCalObject *ico;

	ico = value;
	ical_object_unref (ico);
}

/* Destroys an IMC backend's data */
static void
destroy (CalBackendIMC *cbimc)
{
	IMCPrivate *priv;

	priv = cbimc->priv;

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
	priv->format = CAL_FORMAT_UNKNOWN;
}

/* Destroy handler for the IMC backend */
static void
cal_backend_imc_destroy (GtkObject *object)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND_IMC (object));

	cbimc = CAL_BACKEND_IMC (object);
	priv = cbimc->priv;

	/*
	if (priv->loaded)
		save (cbimc);
	*/

	destroy (cbimc);

	g_free (priv);
	cbimc->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* iCalObject manipulation functions */

/* Looks up an object by its UID in the backend's object hash table */
static iCalObject *
lookup_object (CalBackendIMC *cbimc, const char *uid)
{
	IMCPrivate *priv;
	iCalObject *ico;

	priv = cbimc->priv;
	ico = g_hash_table_lookup (priv->object_hash, uid);

	return ico;
}

/* Ensures that an iCalObject has a unique identifier.  If it doesn't have one,
 * it will create one for it.
 */
static void
ensure_uid (iCalObject *ico)
{
	char *buf;
	gulong str_time;
	static guint seqno = 0;

	if (ico->uid)
		return;

	str_time = (gulong) time (NULL);

	/* Is this good enough? */

	buf = g_strdup_printf ("Evolution-Calendar-%d-%ld-%u",
			       (int) getpid(), str_time, seqno++);
	ico->uid = buf;
}

/* Adds an object to the calendar backend.  Does *not* perform notification to
 * calendar clients.
 */
static void
add_object (CalBackendIMC *cbimc, iCalObject *ico)
{
	IMCPrivate *priv;

	g_assert (ico != NULL);

	priv = cbimc->priv;

#if 0
	/* FIXME: gnomecal old code */
	ico->new = 0;
#endif

	ensure_uid (ico);
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
}

/* Removes an object from the backend's hash and lists.  Does not perform
 * notification on the clients.
 */
static void
remove_object (CalBackendIMC *cbimc, iCalObject *ico)
{
	IMCPrivate *priv;
	GList **list, *l;

	priv = cbimc->priv;

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
                /* Make the compiler shut up. */
	        list = NULL;
		g_assert_not_reached ();
	}

	l = g_list_find (*list, ico);
	g_assert (l != NULL);

	*list = g_list_remove_link (*list, l);
	g_list_free_1 (l);

	ical_object_unref (ico);
}

/* Load a calendar from a VObject */
static void
load_from_vobject (CalBackendIMC *cbimc, VObject *vobject)
{
	IMCPrivate *priv;
	VObjectIterator i;

	priv = cbimc->priv;

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

		/* FIXME: some broken files (ahem, old KOrganizer files) may
		 * have duplicated UIDs.  This is Bad(tm).  Deal with it by
		 * creating new UIDs for them and spitting some messages to the
		 * console.
		 */

		if (ical)
			add_object (cbimc, ical);
	}
}



/* Calendar backend methods */

/* Get_uri handler for the IMC backend */
static GnomeVFSURI *
cal_backend_imc_get_uri (CalBackend *backend)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

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
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	GList *l;

	cal = CAL (object);

	cbimc = CAL_BACKEND_IMC (data);
	priv = cbimc->priv;

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
		cal_backend_last_client_gone (CAL_BACKEND (cbimc));
}

/* Add_cal handler for the IMC backend */
static void
cal_backend_imc_add_cal (CalBackend *backend, Cal *cal)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

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

static icalcomponent *
icalendar_parse_file (char *fname)
{
	FILE *fp;
	icalcomponent *comp = NULL;
	char *str;
	struct stat st;
	int n;

	fp = fopen (fname, "r");
	if (!fp) {
		/* FIXME: remove message */
		g_message ("icalendar_parse_file(): Cannot open open calendar file.");
		return NULL;
	}

	stat (fname, &st);

	str = g_malloc (st.st_size + 2);

	n = fread (str, 1, st.st_size, fp);
	if (n != st.st_size) {
		/* FIXME: remove message, return error code instead */
		g_message ("icalendar_parse_file(): Read error.");
	}
	str[n] = '\0';

	fclose (fp);

	comp = icalparser_parse_string (str);
	g_free (str);

	return comp;
}

static void
icalendar_calendar_load (CalBackendIMC *cbimc, char *fname)
{
	IMCPrivate *priv;
	icalcomponent *comp;
	icalcomponent *subcomp;
	iCalObject    *ical;

	priv = cbimc->priv;

	g_assert (!priv->loaded);
	g_assert (priv->object_hash == NULL);

	priv->object_hash = g_hash_table_new (g_str_hash, g_str_equal);

	comp = icalendar_parse_file (fname);
	subcomp = icalcomponent_get_first_component (comp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		ical = ical_object_create_from_icalcomponent (subcomp);
		if (ical->type != ICAL_EVENT &&
		    ical->type != ICAL_TODO  &&
		    ical->type != ICAL_JOURNAL) {
		       g_message ("icalendar_calendar_load(): Skipping unsupported "
				  "iCalendar component.");
		} else
			add_object (cbimc, ical);

		subcomp = icalcomponent_get_next_component (comp,
							   ICAL_ANY_COMPONENT);
	}
}

/* ics is to be used to designate a file containing (an arbitrary set of)
 * calendaring and scheduling information.
 *
 * ifb is to be used to designate a file containing free or busy time
 * information.
 *
 * anything else is assumed to be a vcal file.
 *
 * FIXME: should we return UNKNOWN at some point?
 */
static CalendarFormat
cal_get_type_from_filename (char *str_uri)
{
	int len;

	if (str_uri == NULL)
		return CAL_FORMAT_VCALENDAR;

	len = strlen (str_uri);
	if (len < 4)
		return CAL_FORMAT_VCALENDAR;

	if (str_uri[len - 4] == '.' && str_uri[len - 3] == 'i' &&
	    str_uri[len - 2] == 'c' && str_uri[len - 1] == 's')
		return CAL_FORMAT_ICALENDAR;

	if (str_uri[len - 4] == '.' && str_uri[len - 3] == 'i' &&
	    str_uri[len - 2] == 'f' && str_uri[len - 1] == 'b')
		return CAL_FORMAT_ICALENDAR;

	if (str_uri[len - 4] == '.' && str_uri[len - 3] == 'i' &&
	    str_uri[len - 2] == 'c' && str_uri[len - 1] == 's')
		return CAL_FORMAT_ICALENDAR;

	if (len < 5)
		return CAL_FORMAT_VCALENDAR;

	if (str_uri[len - 5] == '.' && str_uri[len - 4] == 'i' &&
	    str_uri[len - 3] == 'c' && str_uri[len - 2] == 'a' &&
	    str_uri[len - 1] == 'l')
		return CAL_FORMAT_ICALENDAR;

	return CAL_FORMAT_VCALENDAR;
}

/* Load handler for the IMC backend */
static CalBackendLoadStatus
cal_backend_imc_load (CalBackend *backend, GnomeVFSURI *uri)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	VObject *vobject;
	char *str_uri;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

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

	/* look at the extension on the filename and decide if this is a
	 * iCalendar or vCalendar file.
	 */
	priv->format = cal_get_type_from_filename (str_uri);

	/* load */

	switch (priv->format) {
	case CAL_FORMAT_VCALENDAR:
	        vobject = Parse_MIME_FromFileName (str_uri);

		if (!vobject){
			g_free (str_uri);
			return CAL_BACKEND_LOAD_ERROR;
		}

		load_from_vobject (cbimc, vobject);
		cleanVObject (vobject);
		cleanStrTbl ();
		break;

	case CAL_FORMAT_ICALENDAR:
		icalendar_calendar_load (cbimc, str_uri);
		break;

	default:
		g_free (str_uri);
	        return CAL_BACKEND_LOAD_ERROR;
	}

	g_free (str_uri);

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;
	priv->loaded = TRUE;

	return CAL_BACKEND_LOAD_SUCCESS;
}

/* Create handler for the IMC backend */
static void
cal_backend_imc_create (CalBackend *backend, GnomeVFSURI *uri)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	char *str_uri;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_if_fail (!priv->loaded);
	g_return_if_fail (uri != NULL);

	/* Create the new calendar information */

	g_assert (priv->object_hash == NULL);
	priv->object_hash = g_hash_table_new (g_str_hash, g_str_equal);

	priv->dirty = TRUE;

	/* Done */

	/* FIXME: this looks rather bad; maybe we should check for local files
	 * and fail if they are remote.
	 */

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	/* look at the extension on the filename and decide if this is a
	 * iCalendar or vCalendar file.
	 */
	priv->format = cal_get_type_from_filename (str_uri);

	g_free (str_uri);

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;
	priv->loaded = TRUE;

	save (cbimc);
}

struct get_n_objects_closure {
	CalObjType type;
	int n;
};

/* Counts the number of objects of the specified type.  Called from
 * g_hash_table_foreach().
 */
static void
count_objects (gpointer key, gpointer value, gpointer data)
{
	iCalObject *ico;
	struct get_n_objects_closure *c;
	gboolean store;

	ico = value;
	c = data;

	store = FALSE;

	if (ico->type == ICAL_EVENT)
		store = (c->type & CALOBJ_TYPE_EVENT) != 0;
	else if (ico->type == ICAL_TODO)
		store = (c->type & CALOBJ_TYPE_TODO) != 0;
	else if (ico->type == ICAL_JOURNAL)
		store = (c->type & CALOBJ_TYPE_JOURNAL) != 0;
	else
		store = (c->type & CALOBJ_TYPE_OTHER) != 0;

	if (store)
		c->n++;
}

/* Get_n_objects handler for the IMC backend */
static int
cal_backend_imc_get_n_objects (CalBackend *backend, CalObjType type)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	struct get_n_objects_closure c;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, -1);

	c.type = type;
	c.n = 0;

	g_hash_table_foreach (priv->object_hash, count_objects, &c);
}

/* Get_object handler for the IMC backend */
static char *
cal_backend_imc_get_object (CalBackend *backend, const char *uid)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	iCalObject *ico;
	char *buf;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (uid != NULL, NULL);

	g_return_val_if_fail (priv->loaded, NULL);
	g_assert (priv->object_hash != NULL);

	ico = lookup_object (cbimc, uid);

	if (!ico)
		return NULL;

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

	if (ico->type == ICAL_EVENT)
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

/* Get_uids handler for the IMC backend */
static GList *
cal_backend_imc_get_uids (CalBackend *backend, CalObjType type)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	struct get_uids_closure c;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, NULL);

	/* We go through the hash table instead of the lists of particular
	 * object types so that we can pick up CALOBJ_TYPE_OTHER objects.
	 */
	c.type = type;
	c.uid_list = NULL;
	g_hash_table_foreach (priv->object_hash, build_uids_list, &c);

	return c.uid_list;
}

/* Allocates and fills in a new CalObjInstance structure */
static CalObjInstance *
build_cal_obj_instance (iCalObject *ico, time_t start, time_t end)
{
	CalObjInstance *icoi;

	g_assert (ico->uid != NULL);

	icoi = g_new (CalObjInstance, 1);
	icoi->uid = g_strdup (ico->uid);
	icoi->start = start;
	icoi->end = end;

	return icoi;
}

struct build_event_list_closure {
	CalBackendIMC *cbimc;
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

	icoi = build_cal_obj_instance (ico, start, end);
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

/* Get_events_in_range handler for the IMC backend */
static GList *
cal_backend_imc_get_events_in_range (CalBackend *backend, time_t start, time_t end)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	struct build_event_list_closure c;
	GList *l;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	c.cbimc = cbimc;
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

struct build_alarm_list_closure {
	time_t start;
	time_t end;
	GList *alarms;
};

/* Computes the offset in minutes from an alarm trigger to the actual event */
static int
compute_alarm_offset (CalendarAlarm *a)
{
	int ofs;

	if (!a->enabled)
		return -1;

	switch (a->units) {
	case ALARM_MINUTES:
		ofs = a->count * 60;
		break;

	case ALARM_HOURS:
		ofs = a->count * 3600;
		break;

	case ALARM_DAYS:
		ofs = a->count * 24 * 3600;
		break;

	default:
		ofs = -1;
		g_assert_not_reached ();
	}

	return ofs;
}

/* Allocates and fills in a new CalAlarmInstance structure */
static CalAlarmInstance *
build_cal_alarm_instance (iCalObject *ico, enum AlarmType type, time_t trigger, time_t occur)
{
	CalAlarmInstance *ai;

	g_assert (ico->uid != NULL);

	ai = g_new (CalAlarmInstance, 1);
	ai->uid = g_strdup (ico->uid);
	ai->type = type;
	ai->trigger = trigger;
	ai->occur = occur;

	return ai;
}

/* Adds the specified alarm to the list if its trigger time falls within the
 * requested range.
 */
static void
try_add_alarm (time_t occur_start, iCalObject *ico, CalendarAlarm *alarm,
	       struct build_alarm_list_closure *c)
{
	int ofs;
	time_t trigger;
	CalAlarmInstance *ai;

	if (!alarm->enabled)
		return;

	ofs = compute_alarm_offset (alarm);
	g_assert (ofs != -1);

	trigger = occur_start - ofs;

	if (trigger < c->start || trigger > c->end)
		return;

	ai = build_cal_alarm_instance (ico, alarm->type, trigger, occur_start);
	c->alarms = g_list_prepend (c->alarms, ai);
}

/* Builds a list of alarm instances.  Used as a callback from
 * ical_object_generate_events().
 */
static int
build_alarm_list (iCalObject *ico, time_t start, time_t end, void *data)
{
	struct build_alarm_list_closure *c;

	c = data;

	try_add_alarm (start, ico, &ico->dalarm, c);
	try_add_alarm (start, ico, &ico->aalarm, c);
	try_add_alarm (start, ico, &ico->palarm, c);
	try_add_alarm (start, ico, &ico->malarm, c);

	return TRUE;
}

/* Adds all the alarm triggers that occur within the specified time range */
static GList *
add_alarms_for_object (GList *alarms, iCalObject *ico, time_t start, time_t end)
{
	struct build_alarm_list_closure c;
	int dofs, aofs, pofs, mofs;
	int max_ofs;

	dofs = compute_alarm_offset (&ico->dalarm);
	aofs = compute_alarm_offset (&ico->aalarm);
	pofs = compute_alarm_offset (&ico->palarm);
	mofs = compute_alarm_offset (&ico->malarm);

	max_ofs = MAX (dofs, MAX (aofs, MAX (pofs, mofs)));
	if (max_ofs == -1)
		return alarms;

	c.start = start;
	c.end = end;
	c.alarms = alarms;

	ical_object_generate_events (ico, start, end, build_alarm_list, &c);
	return c.alarms;
}

/* Get_alarms_in_range handler for the IMC backend */
static GList *
cal_backend_imc_get_alarms_in_range (CalBackend *backend, time_t start, time_t end)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	GList *l;
	GList *alarms;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, NULL);

	g_return_val_if_fail (start != -1 && end != -1, NULL);
	g_return_val_if_fail (start <= end, NULL);

	/* Only VEVENT and VTODO components can have alarms */

	alarms = NULL;

	for (l = priv->events; l; l = l->next)
		alarms = add_alarms_for_object (alarms, (iCalObject *) l->data, start, end);

	for (l = priv->todos; l; l = l->next)
		alarms = add_alarms_for_object (alarms, (iCalObject *) l->data, start, end);

	alarms = g_list_sort (alarms, compare_instance_func);
	return alarms;
}

/* Get_alarms_for_object handler for the IMC backend */
static gboolean
cal_backend_imc_get_alarms_for_object (CalBackend *backend, const char *uid,
				       time_t start, time_t end,
				       GList **alarms)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	iCalObject *ico;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (start != -1 && end != -1, FALSE);
	g_return_val_if_fail (start <= end, FALSE);
	g_return_val_if_fail (alarms != NULL, FALSE);

	*alarms = NULL;

	ico = lookup_object (cbimc, uid);
	if (!ico)
		return FALSE;

	/* Only VEVENT and VTODO components can have alarms */

	if (ico->type != ICAL_EVENT && ico->type != ICAL_TODO)
		return TRUE;

	*alarms = add_alarms_for_object (*alarms, ico, start, end);
	*alarms = g_list_sort (*alarms, compare_instance_func);

	return TRUE;
}

/* Notifies a backend's clients that an object was updated */
static void
notify_update (CalBackendIMC *cbimc, const char *uid)
{
	IMCPrivate *priv;
	GList *l;

	priv = cbimc->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_update (cal, uid);
	}
}

/* Notifies a backend's clients that an object was removed */
static void
notify_remove (CalBackendIMC *cbimc, const char *uid)
{
	IMCPrivate *priv;
	GList *l;

	priv = cbimc->priv;

	for (l = priv->clients; l; l = l->next) {
		Cal *cal;

		cal = CAL (l->data);
		cal_notify_remove (cal, uid);
	}
}

/* Update_object handler for the IMC backend */
static gboolean
cal_backend_imc_update_object (CalBackend *backend, const char *uid, const char *calobj)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	iCalObject *ico, *new_ico;
	CalObjFindStatus status;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (calobj != NULL, FALSE);

	/* Pull the object from the string */

	status = ical_object_find_in_string (uid, calobj, &new_ico);

	if (status != CAL_OBJ_FIND_SUCCESS)
		return FALSE;

	/* Update the object */

	ico = lookup_object (cbimc, uid);

	if (ico)
		remove_object (cbimc, ico);

	add_object (cbimc, new_ico);
	save (cbimc);

	/* FIXME: do the notification asynchronously */

	notify_update (cbimc, new_ico->uid);

	return TRUE;
}

/* Remove_object handler for the IMC backend */
static gboolean
cal_backend_imc_remove_object (CalBackend *backend, const char *uid)
{
	CalBackendIMC *cbimc;
	IMCPrivate *priv;
	iCalObject *ico;

	cbimc = CAL_BACKEND_IMC (backend);
	priv = cbimc->priv;

	g_return_val_if_fail (priv->loaded, FALSE);

	g_return_val_if_fail (uid != NULL, FALSE);

	ico = lookup_object (cbimc, uid);
	if (!ico)
		return FALSE;

	remove_object (cbimc, ico);

	priv->dirty = TRUE;
	save (cbimc);

	/* FIXME: do the notification asynchronously */
	notify_remove (cbimc, uid);

	return TRUE;
}

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
#include "cal-backend.h"
#include "calobj.h"
#include "../libversit/vcc.h"



/* Private part of the CalBackend structure */
typedef struct {
	/* URI where the calendar data is stored */
	GnomeVFSURI *uri;

	/* List of Cal client interface objects, each with its listener */
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

		cal_backend_type = gtk_type_unique (GTK_TYPE_OBJECT, &cal_backend_info);
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

	/* FIXME: free stuff */

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* iCalObject manipulation functions */

/* Ensures that an iCalObject has a unique identifier.  If it doesn't have one,
 * it will create one for it.  Returns whether an UID was created or not.
 */
static gboolean
ensure_uid (iCalObject *ico)
{
	char *buf;
	gulong str_time;
	static guint seqno = 0;

	if (ico->uid)
		return FALSE;

	str_time = (gulong) time (NULL);

	/* Is this good enough? */

	buf = g_strdup_printf ("Evolution-Tlacuache-%d-%ld-%u", (int) getpid(), str_time, seqno++);
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

/**
 * cal_backend_add_cal:
 * @backend: A calendar backend.
 * @cal: A calendar client interface object.
 *
 * Adds a calendar client interface object to a calendar @backend.  The calendar
 * backend must already have a loaded calendar.
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

	gtk_object_ref (GTK_OBJECT (cal));
	priv->clients = g_list_prepend (priv->clients, cal);
}

/**
 * cal_backend_remove_cal:
 * @backend: A calendar backend.
 * @cal: A calendar client interface object.
 * 
 * Removes a calendar client interface object from a calendar backend.  The
 * calendar backend must already have a loaded calendar.
 **/
void
cal_backend_remove_cal (CalBackend *backend, Cal *cal)
{
	CalBackendPrivate *priv;
	GList *l;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (IS_CAL_BACKEND (backend));

	priv = backend->priv;
	g_return_if_fail (priv->loaded);

	g_return_if_fail (cal != NULL);
	g_return_if_fail (IS_CAL (cal));

	l = g_list_find (priv->clients, cal);
	if (!l)
		return;

	gtk_object_unref (GTK_OBJECT (cal));
	priv->clients = g_list_remove_link (priv->clients, l);
	g_list_free_1 (l);
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
	g_return_val_if_fail (IS_CAL_BACKEND (backend), CAL_BACKEND_LOAD_ERROR);
	g_return_val_if_fail (uri != NULL, CAL_BACKEND_LOAD_ERROR);

	priv = backend->priv;
	g_return_val_if_fail (!priv->loaded, CAL_BACKEND_LOAD_ERROR);

	/* FIXME: this looks rather bad; maybe we should check for local files
	 * and fail if they are remote.
	 */

	str_uri = gnome_vfs_uri_to_string (uri,
					   (GNOME_VFS_URI_HIDE_USER_NAME
					    | GNOME_VFS_URI_HIDE_PASSWORD
					    | GNOME_VFS_URI_HIDE_HOST_NAME
					    | GNOME_VFS_URI_HIDE_HOST_PORT
					    | GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD));

	vobject = Parse_MIME_FromFileName (str_uri);
	g_free (str_uri);

	if (!vobject)
		return CAL_BACKEND_LOAD_ERROR;

	load_from_vobject (backend, vobject);
	cleanVObject (vobject);
	cleanStrTbl ();

	gnome_vfs_uri_ref (uri);

	priv->uri = uri;
	priv->loaded = TRUE;
	return CAL_BACKEND_LOAD_SUCCESS;
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

	ico = g_hash_table_lookup (priv->objec_hash, uid);

	if (!ico)
		return NULL;

	/* FIXME */
}

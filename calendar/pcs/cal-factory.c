/* Evolution calendar factory
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
#include <ctype.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <liboaf/liboaf.h>
#include "cal.h"
#include "cal-backend.h"
#include "cal-factory.h"
#include "job.h"

#define PARENT_TYPE         BONOBO_X_OBJECT_TYPE
#define CAL_FACTORY_OAF_ID "OAFIID:GNOME_Evolution_Wombat_CalendarFactory"

static BonoboXObjectClass *parent_class;

/* Private part of the CalFactory structure */
struct _CalFactoryPrivate {
	/* Hash table from URI method strings to GtkType * for backend class types */
	GHashTable *methods;

	/* Hash table from GnomeVFSURI structures to CalBackend objects */
	GHashTable *backends;

	/* Whether we have been registered with OAF yet */
	guint registered : 1;
};

/* Signal IDs */
enum SIGNALS {
	LAST_CALENDAR_GONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Frees a method/GtkType * pair from the methods hash table */
static void
free_method (gpointer key, gpointer value, gpointer data)
{
	char *method;
	GtkType *type;

	method = key;
	type = value;

	g_free (method);
	g_free (type);
}

/* Frees a uri/backend pair from the backends hash table */
static void
free_backend (gpointer key, gpointer value, gpointer data)
{
	GnomeVFSURI *uri;
	CalBackend *backend;

	uri = key;
	backend = value;

	gnome_vfs_uri_unref (uri);
	gtk_object_unref (GTK_OBJECT (backend));
}

/* Opening calendars */

/* Looks up a calendar backend in a factory's hash table of uri->cal */
static CalBackend *
lookup_backend (CalFactory *factory, GnomeVFSURI *uri)
{
	CalFactoryPrivate *priv;
	CalBackend *backend;

	priv = factory->priv;

	backend = g_hash_table_lookup (priv->backends, uri);
	return backend;
}

/* Callback used when a backend loses its last connected client */
static void
backend_last_client_gone_cb (CalBackend *backend, gpointer data)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;
	GnomeVFSURI *uri;
	gpointer orig_key;
	gboolean result;
	GnomeVFSURI *orig_uri;

	fprintf (stderr, "backend_last_client_gone_cb() called!\n");

	factory = CAL_FACTORY (data);
	priv = factory->priv;

	/* Remove the backend from the hash table */

	uri = cal_backend_get_uri (backend);
	g_assert (uri != NULL);

	result = g_hash_table_lookup_extended (priv->backends, uri, &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uri = orig_key;

	g_hash_table_remove (priv->backends, orig_uri);
	gnome_vfs_uri_unref (orig_uri);

	gtk_object_unref (GTK_OBJECT (backend));

	/* Notify upstream if there are no more backends */

	if (g_hash_table_size (priv->backends) == 0)
		gtk_signal_emit (GTK_OBJECT (factory), signals[LAST_CALENDAR_GONE]);
}

/* Adds a backend to the calendar factory's hash table */
static void
add_backend (CalFactory *factory, GnomeVFSURI *uri, CalBackend *backend)
{
	CalFactoryPrivate *priv;

	priv = factory->priv;

	gnome_vfs_uri_ref (uri);
	g_hash_table_insert (priv->backends, uri, backend);

	gtk_signal_connect (GTK_OBJECT (backend), "last_client_gone",
			    GTK_SIGNAL_FUNC (backend_last_client_gone_cb),
			    factory);
}

/* Tries to launch a backend for the method of the specified URI.  If there is
 * no such method registered in the factory, it sends the listener the
 * MethodNotSupported error code.
 */
static CalBackend *
launch_backend_for_uri (CalFactory *factory, GnomeVFSURI *uri, GNOME_Evolution_Calendar_Listener listener)
{
	CalFactoryPrivate *priv;
	char *method;
	GtkType *type;
	CalBackend *backend;

	priv = factory->priv;

	/* FIXME: add an accessor function to gnome-vfs */
	method = uri->method_string;

	type = g_hash_table_lookup (priv->methods, method);

	if (!type) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_Listener_notifyCalOpened (
			listener,
			GNOME_Evolution_Calendar_Listener_METHOD_NOT_SUPPORTED,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("launch_backend_for_uri(): could not notify the listener");

		CORBA_exception_free (&ev);
		return NULL;
	}

	backend = gtk_type_new (*type);
	if (!backend)
		g_message ("launch_backend_for_uri(): could not launch the backend");

	return backend;
}

/* Opens a calendar backend and puts it in the factory's backend hash table */
static CalBackend *
open_backend (CalFactory *factory, GnomeVFSURI *uri, gboolean only_if_exists,
	      GNOME_Evolution_Calendar_Listener listener)
{
	CalFactoryPrivate *priv;
	CalBackend *backend;
	CalBackendOpenStatus status;
	CORBA_Environment ev;

	priv = factory->priv;

	backend = launch_backend_for_uri (factory, uri, listener);
	if (!backend)
		return NULL;

	status = cal_backend_open (backend, uri, only_if_exists);

	switch (status) {
	case CAL_BACKEND_OPEN_SUCCESS:
		add_backend (factory, uri, backend);
		return backend;

	case CAL_BACKEND_OPEN_ERROR:
		gtk_object_unref (GTK_OBJECT (backend));

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_Listener_notifyCalOpened (
			listener,
			GNOME_Evolution_Calendar_Listener_ERROR,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("open_backend(): could not notify the listener");

		CORBA_exception_free (&ev);
		return NULL;

	case CAL_BACKEND_OPEN_NOT_FOUND:
		gtk_object_unref (GTK_OBJECT (backend));

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_Listener_notifyCalOpened (
			listener,
			GNOME_Evolution_Calendar_Listener_NOT_FOUND,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("open_backend(): could not notify the listener");

		CORBA_exception_free (&ev);
		return NULL;

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Adds a listener to a calendar backend by creating a calendar client interface
 * object.
 */
static void
add_calendar_client (CalFactory *factory, CalBackend *backend, GNOME_Evolution_Calendar_Listener listener)
{
	Cal *cal;
	CORBA_Environment ev;

	cal = cal_new (backend, listener);
	if (!cal) {
		g_message ("add_calendar_client(): could not create the calendar client interface");

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_Listener_notifyCalOpened (
			listener,
			GNOME_Evolution_Calendar_Listener_ERROR,
			CORBA_OBJECT_NIL,
			&ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("add_calendar_client(): could not notify the listener");

		CORBA_exception_free (&ev);
		return;
	}

	cal_backend_add_cal (backend, cal);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_Listener_notifyCalOpened (
		listener,
		GNOME_Evolution_Calendar_Listener_SUCCESS,
		BONOBO_OBJREF (cal),
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("add_calendar_client(): could not notify the listener");
		bonobo_object_unref (BONOBO_OBJECT (cal));
	}

	CORBA_exception_free (&ev);
}

/* Job data */
typedef struct {
	CalFactory *factory;
	char *uri;
	gboolean only_if_exists;
	GNOME_Evolution_Calendar_Listener listener;
} OpenJobData;

/* Job handler for the open calendar command */
static void
open_fn (gpointer data)
{
	OpenJobData *jd;
	CalFactory *factory;
	GnomeVFSURI *uri;
	gboolean only_if_exists;
	GNOME_Evolution_Calendar_Listener listener;
	CalBackend *backend;
	CORBA_Environment ev;

	jd = data;
	g_assert (jd->uri != NULL);

	/* Check the URI */

	uri = gnome_vfs_uri_new (jd->uri);
	g_free (jd->uri);

	only_if_exists = jd->only_if_exists;
	factory = jd->factory;
	listener = jd->listener;
	g_free (jd);

	if (!uri) {
		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_Listener_notifyCalOpened (
			listener,
			GNOME_Evolution_Calendar_Listener_ERROR,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("open_fn(): Could not notify the listener!");

		CORBA_exception_free (&ev);
		goto out;
	}

	/* Look up the backend and create it if needed */

	backend = lookup_backend (factory, uri);

	if (!backend)
		backend = open_backend (factory, uri, only_if_exists, listener);

	gnome_vfs_uri_unref (uri);

	if (backend)
		add_calendar_client (factory, backend, listener);

 out:

	CORBA_exception_init (&ev);
	CORBA_Object_release (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("open_fn(): could not release the listener");

	CORBA_exception_free (&ev);
}



static void
impl_CalFactory_open (PortableServer_Servant servant,
		      const CORBA_char *uri,
		      CORBA_boolean only_if_exists,
		      GNOME_Evolution_Calendar_Listener listener,
		      CORBA_Environment *ev)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;
	CORBA_Environment ev2;
	gboolean result;
	OpenJobData *jd;
	GNOME_Evolution_Calendar_Listener listener_copy;

	factory = CAL_FACTORY (bonobo_object_from_servant (servant));
	priv = factory->priv;

	CORBA_exception_init (&ev2);
	result = CORBA_Object_is_nil (listener, &ev2);

	if (ev2._major != CORBA_NO_EXCEPTION || result) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_CalFactory_NilListener,
				     NULL);

		CORBA_exception_free (&ev2);
		return;
	}
	CORBA_exception_free (&ev2);

	CORBA_exception_init (&ev2);
	listener_copy = CORBA_Object_duplicate (listener, &ev2);

	if (ev2._major != CORBA_NO_EXCEPTION) {
		g_message ("CalFactory_open(): could not duplicate the listener");
		CORBA_exception_free (&ev2);
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_CalFactory_NilListener,
				     NULL);
		return;
	}

	CORBA_exception_free (&ev2);

	jd = g_new (OpenJobData, 1);
	jd->factory = factory;
	jd->uri = g_strdup (uri);
	jd->only_if_exists = only_if_exists;
	jd->listener = listener_copy;

	job_add (open_fn, jd);
}



/**
 * cal_factory_new:
 * @void:
 *
 * Creates a new #CalFactory object.
 *
 * Return value: A newly-created #CalFactory, or NULL if its corresponding CORBA
 * object could not be created.
 **/
CalFactory *
cal_factory_new (void)
{
	CalFactory *factory;

	factory = gtk_type_new (CAL_FACTORY_TYPE);

	return factory;
}

/* Destroy handler for the calendar */
static void
cal_factory_destroy (GtkObject *object)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_FACTORY (object));

	factory = CAL_FACTORY (object);
	priv = factory->priv;

	g_hash_table_foreach (priv->methods, free_method, NULL);
	g_hash_table_destroy (priv->methods);
	priv->methods = NULL;

	/* Should we assert that there are no more backends? */

	g_hash_table_foreach (priv->backends, free_backend, NULL);
	g_hash_table_destroy (priv->backends);
	priv->backends = NULL;

	if (priv->registered) {
		oaf_active_server_unregister (CAL_FACTORY_OAF_ID, BONOBO_OBJREF (factory));
		priv->registered = FALSE;
	}

	g_free (priv);
	factory->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Class initialization function for the calendar factory */
static void
cal_factory_class_init (CalFactoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	POA_GNOME_Evolution_Calendar_CalFactory__epv *epv = &klass->epv;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	signals[LAST_CALENDAR_GONE] =
		gtk_signal_new ("last_calendar_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalFactoryClass, last_calendar_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	/* Class method overrides */
	object_class->destroy = cal_factory_destroy;

	/* Epv methods */
	epv->open = impl_CalFactory_open;
}

/* Object initialization function for the calendar factory */
static void
cal_factory_init (CalFactory *factory)
{
	CalFactoryPrivate *priv;

	priv = g_new0 (CalFactoryPrivate, 1);
	factory->priv = priv;

	priv->methods = g_hash_table_new (g_str_hash, g_str_equal);
	priv->backends = g_hash_table_new (gnome_vfs_uri_hash, gnome_vfs_uri_hequal);
	priv->registered = FALSE;
}

BONOBO_X_TYPE_FUNC_FULL (CalFactory,
			 GNOME_Evolution_Calendar_CalFactory,
			 PARENT_TYPE,
			 cal_factory);

/* Returns the lowercase version of a string */
static char *
str_tolower (const char *s)
{
	char *str;
	unsigned char *p;

	str = g_strdup (s);
	for (p = str; *p; p++)
		if (isalpha (*p))
			*p = tolower (*p);

	return str;
}

/**
 * cal_factory_oaf_register:
 * @factory: A calendar factory.
 * 
 * Registers a calendar factory with the OAF object activation daemon.  This
 * function must be called before any clients can activate the factory.
 * 
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
cal_factory_oaf_register (CalFactory *factory)
{
	CalFactoryPrivate *priv;
	OAF_RegistrationResult result;

	g_return_val_if_fail (factory != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_FACTORY (factory), FALSE);

	priv = factory->priv;

	g_return_val_if_fail (!priv->registered, FALSE);

	result = oaf_active_server_register (CAL_FACTORY_OAF_ID, BONOBO_OBJREF (factory));

	switch (result) {
	case OAF_REG_SUCCESS:
		priv->registered = TRUE;
		return TRUE;

	case OAF_REG_NOT_LISTED:
		g_message ("cal_factory_oaf_register(): Cannot register the calendar factory: "
			   "not listed");
		break;

	case OAF_REG_ALREADY_ACTIVE:
		g_message ("cal_factory_oaf_register(): Cannot register the calendar factory: "
			   "already active");
		break;

	case OAF_REG_ERROR:
	default:
		g_message ("cal_factory_oaf_register(): Cannot register the calendar factory: "
			   "generic error");
		break;
	}

	return FALSE;
}

/**
 * cal_factory_register_method:
 * @factory: A calendar factory.
 * @method: Method for the URI, i.e. "http", "file", etc.
 * @backend_type: Class type of the backend to create for this @method.
 * 
 * Registers the type of a #CalBackend subclass that will be used to handle URIs
 * with a particular method.  When the factory is asked to open a particular
 * URI, it will look in its list of registered methods and create a backend of
 * the appropriate type.
 **/
void
cal_factory_register_method (CalFactory *factory, const char *method, GtkType backend_type)
{
	CalFactoryPrivate *priv;
	GtkType *type;
	char *method_str;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (IS_CAL_FACTORY (factory));
	g_return_if_fail (method != NULL);
	g_return_if_fail (backend_type != 0);
	g_return_if_fail (gtk_type_is_a (backend_type, CAL_BACKEND_TYPE));

	priv = factory->priv;

	method_str = str_tolower (method);

	type = g_hash_table_lookup (priv->methods, method_str);
	if (type) {
		g_message ("cal_factory_register_method(): Method `%s' already registered!",
			   method_str);
		g_free (method_str);
		return;
	}

	type = g_new (GtkType, 1);
	*type = backend_type;

	g_hash_table_insert (priv->methods, method_str, type);
}

/**
 * cal_factory_get_n_backends:
 * @factory: A calendar factory.
 * 
 * Queries the number of running calendar backends in a calendar factory.
 * 
 * Return value: Number of running backends.
 **/
int
cal_factory_get_n_backends (CalFactory *factory)
{
	CalFactoryPrivate *priv;

	g_return_val_if_fail (factory != NULL, -1);
	g_return_val_if_fail (IS_CAL_FACTORY (factory), -1);

	priv = factory->priv;
	return g_hash_table_size (priv->backends);
}

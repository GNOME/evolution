/* Evolution calendar factory
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
#include <ctype.h>
#include <gtk/gtksignal.h>
#include "cal.h"
#include "cal-backend.h"
#include "cal-factory.h"
#include "job.h"



/* Private part of the CalFactory structure */
typedef struct {
	/* Hash table from URI method strings to GtkType * for backend class types */
	GHashTable *methods;

	/* Hash table from GnomeVFSURI structures to CalBackend objects */
	GHashTable *backends;
} CalFactoryPrivate;



/* Signal IDs */
enum {
	LAST_CALENDAR_GONE,
	LAST_SIGNAL
};

static void cal_factory_class_init (CalFactoryClass *class);
static void cal_factory_init (CalFactory *factory);
static void cal_factory_destroy (GtkObject *object);

static POA_Evolution_Calendar_CalFactory__vepv cal_factory_vepv;

static BonoboObjectClass *parent_class;

static guint cal_factory_signals[LAST_SIGNAL];



/**
 * cal_factory_get_type:
 * @void:
 *
 * Registers the #CalFactory class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalFactory class.
 **/
GtkType
cal_factory_get_type (void)
{
	static GtkType cal_factory_type = 0;

	if (!cal_factory_type) {
		static const GtkTypeInfo cal_factory_info = {
			"CalFactory",
			sizeof (CalFactory),
			sizeof (CalFactoryClass),
			(GtkClassInitFunc) cal_factory_class_init,
			(GtkObjectInitFunc) cal_factory_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_factory_type = gtk_type_unique (bonobo_object_get_type (), &cal_factory_info);
	}

	return cal_factory_type;
}

/* CORBA class initialization function for the calendar factory */
static void
init_cal_factory_corba_class (void)
{
	cal_factory_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	cal_factory_vepv.Evolution_Calendar_CalFactory_epv = cal_factory_get_epv ();
}

/* Class initialization function for the calendar factory */
static void
cal_factory_class_init (CalFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	cal_factory_signals[LAST_CALENDAR_GONE] =
		gtk_signal_new ("last_calendar_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalFactoryClass, last_calendar_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, cal_factory_signals, LAST_SIGNAL);

	object_class->destroy = cal_factory_destroy;

	init_cal_factory_corba_class ();
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
}

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

	g_free (priv);
	factory->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Loading and creating calendars */

/* Job data */
typedef struct {
	CalFactory *factory;
	char *uri;
	Evolution_Calendar_Listener listener;
} LoadCreateJobData;

/* Queues a load or create request */
static void
queue_load_create_job (CalFactory *factory, const char *uri, Evolution_Calendar_Listener listener,
		       JobFunc func)
{
	LoadCreateJobData *jd;
	CORBA_Environment ev;
	Evolution_Calendar_Listener listener_copy;
	gboolean result;

	CORBA_exception_init (&ev);
	result = CORBA_Object_is_nil (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("queue_load_create_job(): could not see if the listener was NIL");
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);

	if (result) {
		g_message ("queue_load_create_job(): cannot operate on a NIL listener!");
		return;
	}

	listener_copy = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("queue_load_create_job(): could not duplicate the listener");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	jd = g_new (LoadCreateJobData, 1);
	jd->factory = factory;
	jd->uri = g_strdup (uri);
	jd->listener = listener_copy;

	job_add (func, jd);
}

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
		gtk_signal_emit (GTK_OBJECT (factory), cal_factory_signals[LAST_CALENDAR_GONE]);
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
launch_backend_for_uri (CalFactory *factory, GnomeVFSURI *uri, Evolution_Calendar_Listener listener)
{
	CalFactoryPrivate *priv;
	char *method;
	GtkType *type;
	CalBackend *backend;

	priv = factory->priv;

	/* FIXME: add an accessor function to gnome-vfs */
	method = uri->method_string;

	type = g_hash_table_lookup (priv->methods, method);
	g_free (method);

	if (!type) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		Evolution_Calendar_Listener_cal_loaded (
			listener,
			Evolution_Calendar_Listener_METHOD_NOT_SUPPORTED,
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

/* Loads a calendar backend and puts it in the factory's backend hash table */
static CalBackend *
load_backend (CalFactory *factory, GnomeVFSURI *uri, Evolution_Calendar_Listener listener)
{
	CalFactoryPrivate *priv;
	CalBackend *backend;
	CalBackendLoadStatus status;
	CORBA_Environment ev;

	priv = factory->priv;

	backend = launch_backend_for_uri (factory, uri, listener);
	if (!backend)
		return NULL;

	status = cal_backend_load (backend, uri);

	switch (status) {
	case CAL_BACKEND_LOAD_SUCCESS:
		add_backend (factory, uri, backend);
		return backend;

	case CAL_BACKEND_LOAD_ERROR:
		gtk_object_unref (GTK_OBJECT (backend));

		CORBA_exception_init (&ev);
		Evolution_Calendar_Listener_cal_loaded (listener,
							Evolution_Calendar_Listener_ERROR,
							CORBA_OBJECT_NIL,
							&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("load_backend(): could not notify the listener");

		CORBA_exception_free (&ev);
		return NULL;

	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/* Creates a calendar backend and puts it in the factory's backend hash table */
static CalBackend *
create_backend (CalFactory *factory, GnomeVFSURI *uri, Evolution_Calendar_Listener listener)
{
	CalFactoryPrivate *priv;
	CalBackend *backend;

	priv = factory->priv;

	backend = launch_backend_for_uri (factory, uri, listener);
	if (!backend)
		return NULL;

	cal_backend_create (backend, uri);

	/* FIXME: add error reporting to cal_backend_create() */
#if 0
	{
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		Evolution_Calendar_Listener_cal_loaded (listener,
							Evolution_Calendar_Listener_ERROR,
							CORBA_OBJECT_NIL,
							&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("create_fn(): could not notify the listener");

		CORBA_exception_free (&ev);
	}
#endif

	add_backend (factory, uri, backend);

	return backend;
}

/* Adds a listener to a calendar backend by creating a calendar client interface
 * object.
 */
static void
add_calendar_client (CalFactory *factory, CalBackend *backend, Evolution_Calendar_Listener listener)
{
	Cal *cal;
	CORBA_Environment ev;

	cal = cal_new (backend, listener);
	if (!cal) {
		g_message ("add_calendar_client(): could not create the calendar client interface");

		CORBA_exception_init (&ev);
		Evolution_Calendar_Listener_cal_loaded (listener,
							Evolution_Calendar_Listener_ERROR,
							CORBA_OBJECT_NIL,
							&ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("add_calendar_client(): could not notify the listener");

		CORBA_exception_free (&ev);
		return;
	}

	cal_backend_add_cal (backend, cal);

	CORBA_exception_init (&ev);
	Evolution_Calendar_Listener_cal_loaded (listener,
						Evolution_Calendar_Listener_SUCCESS,
						bonobo_object_corba_objref (BONOBO_OBJECT (cal)),
						&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("add_calendar_client(): could not notify the listener");
		bonobo_object_unref (BONOBO_OBJECT (cal));
	}

	CORBA_exception_free (&ev);
}

/* Job handler for the load calendar command */
static void
load_fn (gpointer data)
{
	LoadCreateJobData *jd;
	CalFactory *factory;
	GnomeVFSURI *uri;
	Evolution_Calendar_Listener listener;
	CalBackend *backend;
	CORBA_Environment ev;

	jd = data;

	/* Look up the calendar */

	uri = gnome_vfs_uri_new (jd->uri);
	g_free (jd->uri);

	factory = jd->factory;
	listener = jd->listener;
	g_free (jd);

	/* Look up the backend and create it if needed */

	backend = lookup_backend (factory, uri);

	if (!backend)
		backend = load_backend (factory, uri, listener);

	if (!backend)
		goto out;

	add_calendar_client (factory, backend, listener);

 out:

	gnome_vfs_uri_unref (uri);

	CORBA_exception_init (&ev);
	CORBA_Object_release (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("load_fn(): could not release the listener");

	CORBA_exception_free (&ev);
}

/* Job handler for the create calendar command */
static void
create_fn (gpointer data)
{
	LoadCreateJobData *jd;
	CalFactory *factory;
	GnomeVFSURI *uri;
	Evolution_Calendar_Listener listener;
	CalBackend *backend;
	CORBA_Environment ev;

	jd = data;
	factory = jd->factory;

	uri = gnome_vfs_uri_new (jd->uri);
	g_free (jd->uri);

	factory = jd->factory;
	listener = jd->listener;
	g_free (jd);

	/* Check that the backend is not in use */

	backend = lookup_backend (factory, uri);

	if (backend) {
		CORBA_exception_init (&ev);
		Evolution_Calendar_Listener_cal_loaded (listener,
							Evolution_Calendar_Listener_IN_USE,
							CORBA_OBJECT_NIL,
							&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("create_fn(): could not notify the listener");

		CORBA_exception_free (&ev);
		goto out;
	}

	/* Create the backend */

	backend = create_backend (factory, uri, listener);

	if (!backend)
		goto out;

	add_calendar_client (factory, backend, listener);

 out:

	gnome_vfs_uri_unref (uri);

	CORBA_exception_init (&ev);
	CORBA_Object_release (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("create_fn(): could not release the listener");

	CORBA_exception_free (&ev);
}



/* CORBA servant implementation */

/* CalFactory::load method */
static void
CalFactory_load (PortableServer_Servant servant,
		 const CORBA_char *uri,
		 Evolution_Calendar_Listener listener,
		 CORBA_Environment *ev)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;
	CORBA_Environment ev2;
	gboolean result;

	factory = CAL_FACTORY (bonobo_object_from_servant (servant));
	priv = factory->priv;

	CORBA_exception_init (&ev2);
	result = CORBA_Object_is_nil (listener, &ev2);

	if (ev2._major != CORBA_NO_EXCEPTION || result) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_Calendar_CalFactory_NilListener,
				     NULL);

		CORBA_exception_free (&ev2);
		return;
	}
	CORBA_exception_free (&ev2);

	queue_load_create_job (factory, uri, listener, load_fn);
}

/* CalFactory::create method */
static void
CalFactory_create (PortableServer_Servant servant,
		   const CORBA_char *uri,
		   Evolution_Calendar_Listener listener,
		   CORBA_Environment *ev)
{
	CalFactory *factory;
	CalFactoryPrivate *priv;

	factory = CAL_FACTORY (bonobo_object_from_servant (servant));
	priv = factory->priv;

	queue_load_create_job (factory, uri, listener, create_fn);
}

/**
 * cal_factory_get_epv:
 * @void:
 *
 * Creates an EPV for the CalFactory CORBA class.
 *
 * Return value: A newly-allocated EPV.
 **/
POA_Evolution_Calendar_CalFactory__epv *
cal_factory_get_epv (void)
{
	POA_Evolution_Calendar_CalFactory__epv *epv;

	epv = g_new0 (POA_Evolution_Calendar_CalFactory__epv, 1);
	epv->load = CalFactory_load;
	epv->create = CalFactory_create;

	return epv;
}



/**
 * cal_factory_construct:
 * @factory: A calendar factory.
 * @corba_factory: CORBA object for the calendar factory.
 *
 * Constructs a calendar factory by binding the corresponding CORBA object to
 * it.
 *
 * Return value: The same object as the @factory argument.
 **/
CalFactory *
cal_factory_construct (CalFactory *factory, Evolution_Calendar_CalFactory corba_factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (IS_CAL_FACTORY (factory), NULL);

	bonobo_object_construct (BONOBO_OBJECT (factory), corba_factory);
	return factory;
}

/**
 * cal_factory_corba_object_create:
 * @object: #BonoboObject that will wrap the CORBA object.
 *
 * Creates and activates the CORBA object that is wrapped by the specified
 * calendar factory @object.
 *
 * Return value: An activated object reference or #CORBA_OBJECT_NIL in case of
 * failure.
 **/
Evolution_Calendar_CalFactory
cal_factory_corba_object_create (BonoboObject *object)
{
	POA_Evolution_Calendar_CalFactory *servant;
	CORBA_Environment ev;

	g_return_val_if_fail (object != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (IS_CAL_FACTORY (object), CORBA_OBJECT_NIL);

	servant = (POA_Evolution_Calendar_CalFactory *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &cal_factory_vepv;

	CORBA_exception_init (&ev);
	POA_Evolution_Calendar_CalFactory__init ((PortableServer_Servant) servant, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_factory_corba_object_create(): could not init the servant");
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Evolution_Calendar_CalFactory) bonobo_object_activate_servant (object, servant);
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
	CORBA_Environment ev;
	Evolution_Calendar_CalFactory corba_factory;
	gboolean retval;

	factory = gtk_type_new (CAL_FACTORY_TYPE);

	corba_factory = cal_factory_corba_object_create (BONOBO_OBJECT (factory));

	CORBA_exception_init (&ev);
	retval = CORBA_Object_is_nil (corba_factory, &ev);

	if (ev._major != CORBA_NO_EXCEPTION || retval) {
		g_message ("cal_factory_new(): could not create the CORBA factory");
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	return cal_factory_construct (factory, corba_factory);
}

/* Returns the lowercase version of a string */
static char *
str_tolower (const char *s)
{
	char *str;
	char *p;

	str = g_strdup (s);
	for (p = str; *p; p++)
		if (isalpha (*p))
			*p = tolower (*p);

	return str;
}

/**
 * cal_factory_register_method:
 * @factory: A calendar factory.
 * @method: Method for the URI, i.e. "http", "file", etc.
 * @backend_type: Class type of the backend to create for this @method.
 * 
 * Registers the type of a #CalBackend subclass that will be used to handle URIs
 * with a particular method.  When the factory is asked to load a particular
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

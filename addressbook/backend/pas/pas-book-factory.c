/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <ctype.h>
#include <gnome.h>

#include "addressbook.h"
#include "pas-book-factory.h"

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#define PAS_BOOK_FACTORY_OAF_ID "OAFIID:evolution:addressbook-server:0fbc844d-c721-4615-98d0-d67eacf42d80"
#else
#include <libgnorba/gnorba.h>
#define PAS_BOOK_FACTORY_GOAD_ID "evolution:addressbook-server"
#endif

static BonoboObjectClass          *pas_book_factory_parent_class;
POA_Evolution_BookFactory__vepv   pas_book_factory_vepv;

typedef struct {
	char                              *uri;
	Evolution_BookListener             listener;
} PASBookFactoryQueuedRequest;

struct _PASBookFactoryPrivate {
	gint        idle_id;
	GHashTable *backends;
	GHashTable *active_server_map;
	GList      *queued_requests;
};

/* Signal IDs */
enum {
	LAST_BOOK_GONE,
	LAST_SIGNAL
};

static guint factory_signals[LAST_SIGNAL];

static char *
pas_book_factory_canonicalize_uri (const char *uri)
{
	/* FIXME: What do I do here? */

	return g_strdup (uri);
}

static char *
pas_book_factory_extract_proto_from_uri (const char *uri)
{
	char *proto;
	char *p;

	p = strchr (uri, ':');

	if (p == NULL)
		return NULL;

	proto = g_malloc0 (p - uri + 1);

	strncpy (proto, uri, p - uri);

	return proto;
}

/**
 * pas_book_factory_register_backend:
 * @factory:
 * @proto:
 * @backend:
 */
void
pas_book_factory_register_backend (PASBookFactory      *factory,
				   const char          *proto,
				   PASBackendFactoryFn  backend)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (PAS_IS_BOOK_FACTORY (factory));
	g_return_if_fail (proto != NULL);
	g_return_if_fail (backend != NULL);

	if (g_hash_table_lookup (factory->priv->backends, proto) != NULL) {
		g_warning ("pas_book_factory_register_backend: "
			   "Proto \"%s\" already registered!\n", proto);
	}

	g_hash_table_insert (factory->priv->backends,
			     g_strdup (proto), backend);
}

/**
 * pas_book_factory_get_n_backends:
 * @factory: An addressbook factory.
 * 
 * Queries the number of running addressbook backends in an addressbook factory.
 * 
 * Return value: Number of running backends.
 **/
int
pas_book_factory_get_n_backends (PASBookFactory *factory)
{
	g_return_val_if_fail (factory != NULL, -1);
	g_return_val_if_fail (PAS_IS_BOOK_FACTORY (factory), -1);

	return g_hash_table_size (factory->priv->active_server_map);
}

/* Callback used when a backend loses its last connected client */
static void
backend_last_client_gone_cb (PASBackend *backend, gpointer data)
{
	PASBookFactory *factory;
	const char *uri;
	gpointer orig_key;
	gboolean result;
	char *orig_uri;

	factory = PAS_BOOK_FACTORY (data);

	/* Remove the backend from the active server map */

	uri = pas_backend_get_uri (backend);
	g_assert (uri != NULL);

	result = g_hash_table_lookup_extended (factory->priv->active_server_map, uri,
					       &orig_key, NULL);
	g_assert (result != FALSE);

	orig_uri = orig_key;

	g_hash_table_remove (factory->priv->active_server_map, orig_uri);
	g_free (orig_uri);

	gtk_object_unref (GTK_OBJECT (backend));

	/* Notify upstream if there are no more backends */

	if (g_hash_table_size (factory->priv->active_server_map) == 0)
		gtk_signal_emit (GTK_OBJECT (factory), factory_signals[LAST_BOOK_GONE]);
}

static PASBackendFactoryFn
pas_book_factory_lookup_backend_factory (PASBookFactory *factory,
					 const char     *uri)
{
	PASBackendFactoryFn  backend_fn;
	char                *proto;
	char                *canonical_uri;

	g_assert (factory != NULL);
	g_assert (PAS_IS_BOOK_FACTORY (factory));
	g_assert (uri != NULL);

	canonical_uri = pas_book_factory_canonicalize_uri (uri);
	if (canonical_uri == NULL)
		return NULL;

	proto = pas_book_factory_extract_proto_from_uri (canonical_uri);
	if (proto == NULL) {
		g_free (canonical_uri);
		return NULL;
	}

	backend_fn = g_hash_table_lookup (factory->priv->backends, proto);

	g_free (proto); 
	g_free (canonical_uri);

	return backend_fn;
}

static PASBackend *
pas_book_factory_launch_backend (PASBookFactory              *factory,
				 Evolution_BookListener       listener,
				 const char                  *uri)
{
	PASBackendFactoryFn  backend_factory;
	PASBackend          *backend;

	backend_factory = pas_book_factory_lookup_backend_factory (
		factory, uri);

	if (!backend_factory) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		Evolution_BookListener_respond_open_book (
			listener,
			Evolution_BookListener_ProtocolNotSupported,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_factory_launch_backend(): could not notify "
				   "the listener");

		CORBA_exception_free (&ev);
		return NULL;
	}

	backend = (* backend_factory) ();
	if (!backend) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		Evolution_BookListener_respond_open_book (
			listener,
			Evolution_BookListener_OtherError,
			CORBA_OBJECT_NIL,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_factory_launch_backend(): could not notify "
				   "the listener");

		CORBA_exception_free (&ev);
		return NULL;
	}

	g_hash_table_insert (factory->priv->active_server_map,
			     g_strdup (uri),
			     backend);

	gtk_signal_connect (GTK_OBJECT (backend), "last_client_gone",
			    backend_last_client_gone_cb,
			    factory);

	return backend;
}

static void
pas_book_factory_process_request (PASBookFactory              *factory,
				  PASBookFactoryQueuedRequest *request)
{
	PASBackend *backend;
	char *uri;
	Evolution_BookListener listener;
	CORBA_Environment ev;

	uri = request->uri;
	listener = request->listener;
	g_free (request);

	/* Look up the backend and create one if needed */

	backend = g_hash_table_lookup (factory->priv->active_server_map, uri);

	if (!backend) {
		backend = pas_book_factory_launch_backend (factory, listener, uri);
		if (!backend)
			goto out;

		if (!pas_backend_add_client (backend, listener))
			goto out;

		pas_backend_load_uri (backend, uri);

		goto out;
	}

	pas_backend_add_client (backend, listener);

 out:
	g_free (uri);
#if 0
	CORBA_exception_init (&ev);
	CORBA_Object_release (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("pas_book_factory_process_request(): could not release the listener");

	CORBA_exception_free (&ev);
#endif
}

static gboolean
pas_book_factory_process_queue (PASBookFactory *factory)
{
	/* Process pending Book-creation requests. */
	if (factory->priv->queued_requests != NULL) {
		PASBookFactoryQueuedRequest  *request;
		GList *l;

		l = factory->priv->queued_requests;
		request = l->data;

		pas_book_factory_process_request (factory, request);

		factory->priv->queued_requests = g_list_remove_link (
			factory->priv->queued_requests, l);
		g_list_free_1 (l);
	}

	if (factory->priv->queued_requests == NULL) {

		factory->priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
pas_book_factory_queue_request (PASBookFactory               *factory,
				const char                   *uri,
				const Evolution_BookListener  listener)
{
	PASBookFactoryQueuedRequest *request;
	Evolution_BookListener       listener_copy;
	CORBA_Environment            ev;

	CORBA_exception_init (&ev);

	listener_copy = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("PASBookFactory: Could not duplicate BookListener!\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	request           = g_new0 (PASBookFactoryQueuedRequest, 1);
	request->listener = listener_copy;
	request->uri      = g_strdup (uri);

	factory->priv->queued_requests =
		g_list_prepend (factory->priv->queued_requests, request);

	if (! factory->priv->idle_id) {
		factory->priv->idle_id =
			g_idle_add ((GSourceFunc) pas_book_factory_process_queue, factory);
	}
}


static void
impl_Evolution_BookFactory_open_book (PortableServer_Servant        servant,
				      const CORBA_char             *uri,
				      const Evolution_BookListener  listener,
				      CORBA_Environment            *ev)
{
	PASBookFactory      *factory =
		PAS_BOOK_FACTORY (bonobo_object_from_servant (servant));
	PASBackendFactoryFn  backend_factory;

	backend_factory = pas_book_factory_lookup_backend_factory (factory, uri);

	if (backend_factory == NULL) {
		g_warning ("PASBookFactory: No backend found for uri: %s\n", uri);

		CORBA_exception_set (
			ev, CORBA_USER_EXCEPTION,
			ex_Evolution_BookFactory_ProtocolNotSupported, NULL);

		return;
	}

	pas_book_factory_queue_request (factory, uri, listener);
}

static gboolean
pas_book_factory_construct (PASBookFactory *factory)
{
	POA_Evolution_BookFactory  *servant;
	CORBA_Environment           ev;
	CORBA_Object                obj;

	g_assert (factory != NULL);
	g_assert (PAS_IS_BOOK_FACTORY (factory));

	servant = (POA_Evolution_BookFactory *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &pas_book_factory_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_BookFactory__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);

		return FALSE;
	}

	CORBA_exception_free (&ev);

	obj = bonobo_object_activate_servant (BONOBO_OBJECT (factory), servant);
	if (obj == CORBA_OBJECT_NIL) {
		g_free (servant);

		return FALSE;
	}

	bonobo_object_construct (BONOBO_OBJECT (factory), obj);

	return TRUE;
}

/**
 * pas_book_factory_new:
 */
PASBookFactory *
pas_book_factory_new (void)
{
	PASBookFactory *factory;

	factory = gtk_type_new (pas_book_factory_get_type ());

	if (! pas_book_factory_construct (factory)) {
		g_warning ("pas_book_factory_new: Could not construct PASBookFactory!\n");
		gtk_object_unref (GTK_OBJECT (factory));

		return NULL;
	}

	return factory;
}

#ifdef USING_OAF

static gboolean
register_factory (CORBA_Object obj)
{
	OAF_RegistrationResult result;

	puts ("about to register addressbook");

	result = oaf_active_server_register (PAS_BOOK_FACTORY_OAF_ID, obj);

	switch (result) {
	case OAF_REG_SUCCESS:
		return TRUE;
	case OAF_REG_NOT_LISTED:
		g_message ("Error registering the PAS factory: not listed");
		return FALSE;
	case OAF_REG_ALREADY_ACTIVE:
		g_message ("Error registering the PAS factory: already active");
		return FALSE;
	case OAF_REG_ERROR:
	default:
		g_message ("Error registering the PAS factory: generic error");
		return FALSE;
	}
}

#else

static gboolean
register_factory (CORBA_Object obj)
{
	CORBA_Environment  ev;
	int ret;

	CORBA_exception_init (&ev);
	ret = goad_server_register (NULL, obj, PAS_BOOK_FACTORY_GOAD_ID, "server", &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("pas_book_factory_activate: Exception "
			   "registering PASBookFactory!\n");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	switch (ret) {
	case 0:
		return TRUE;
	case -2:
		g_message ("pas_book_factory_activate: Another "
			   "PASBookFactory is already running.\n");
		return FALSE;
	case -1:
	default:
		g_message ("pas_book_factory_activate: Error "
			   "registering PASBookFactory!\n");
		return FALSE;
	}
}

#endif

/**
 * pas_book_factory_activate:
 */
void
pas_book_factory_activate (PASBookFactory *factory)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (PAS_IS_BOOK_FACTORY (factory));

	register_factory (bonobo_object_corba_objref (BONOBO_OBJECT (factory)));
}

static void
pas_book_factory_init (PASBookFactory *factory)
{
	factory->priv = g_new0 (PASBookFactoryPrivate, 1);

	factory->priv->active_server_map = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->backends          = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->queued_requests   = NULL;
}

static void
free_active_server_map_entry (gpointer key, gpointer value, gpointer data)
{
	char *uri;
	PASBackend *backend;

	uri = key;
	g_free (uri);

	backend = PAS_BACKEND (value);
	gtk_object_unref (GTK_OBJECT (backend));
}

static void
remove_backends_entry (gpointer key, gpointer value, gpointer data)
{
	char *uri;

	uri = key;
	g_free (uri);
}

static void
pas_book_factory_destroy (GtkObject *object)
{
	PASBookFactory *factory = PAS_BOOK_FACTORY (object);
	GList          *l;

	for (l = factory->priv->queued_requests; l != NULL; l = l->next) {
		PASBookFactoryQueuedRequest *request = l->data;
		CORBA_Environment ev;

		g_free (request->uri);

		CORBA_exception_init (&ev);
		CORBA_Object_release (request->listener, &ev);
		CORBA_exception_free (&ev);

		g_free (request);
	}
	g_list_free (factory->priv->queued_requests);
	factory->priv->queued_requests = NULL;

	g_hash_table_foreach (factory->priv->active_server_map,
			      free_active_server_map_entry,
			      NULL);
	g_hash_table_destroy (factory->priv->active_server_map);
	factory->priv->active_server_map = NULL;

	g_hash_table_foreach (factory->priv->backends,
			      remove_backends_entry,
			      NULL);
	g_hash_table_destroy (factory->priv->backends);
	factory->priv->backends = NULL;
	
	g_free (factory->priv);

	GTK_OBJECT_CLASS (pas_book_factory_parent_class)->destroy (object);
}

static POA_Evolution_BookFactory__epv *
pas_book_factory_get_epv (void)
{
	POA_Evolution_BookFactory__epv *epv;

	epv = g_new0 (POA_Evolution_BookFactory__epv, 1);

	epv->open_book = impl_Evolution_BookFactory_open_book;

	return epv;
	
}

static void
pas_book_factory_corba_class_init (void)
{
	pas_book_factory_vepv.Bonobo_Unknown_epv         = bonobo_object_get_epv ();
	pas_book_factory_vepv.Evolution_BookFactory_epv = pas_book_factory_get_epv ();
}

static void
pas_book_factory_class_init (PASBookFactoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	pas_book_factory_parent_class = gtk_type_class (bonobo_object_get_type ());

	factory_signals[LAST_BOOK_GONE] =
		gtk_signal_new ("last_book_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (PASBookFactoryClass, last_book_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, factory_signals, LAST_SIGNAL);

	object_class->destroy = pas_book_factory_destroy;

	pas_book_factory_corba_class_init ();
}

/**
 * pas_book_factory_get_type:
 */
GtkType
pas_book_factory_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBookFactory",
			sizeof (PASBookFactory),
			sizeof (PASBookFactoryClass),
			(GtkClassInitFunc)  pas_book_factory_class_init,
			(GtkObjectInitFunc) pas_book_factory_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

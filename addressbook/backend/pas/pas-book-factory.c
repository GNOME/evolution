/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <ctype.h>
#include <libgnorba/gnorba.h>
#include <addressbook.h>
#include <pas-book-factory.h>

#define PAS_BOOK_FACTORY_GOAD_ID "evolution:addressbook-server"

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

static PASBackendFactoryFn
pas_book_factory_lookup_backend_factory (PASBookFactory *factory,
					 const char     *uri)
{
	PASBackendFactoryFn  backend;
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

	backend = g_hash_table_lookup (factory->priv->backends, proto);

	g_free (proto); 
	g_free (canonical_uri);

	return backend;
}

static PASBackend *
pas_book_factory_launch_backend (PASBookFactory              *factory,
				 PASBookFactoryQueuedRequest *request)
{
	PASBackendFactoryFn  backend_factory;
	PASBackend          *backend;

	backend_factory = pas_book_factory_lookup_backend_factory (
		factory, request->uri);
	g_assert (backend_factory != NULL);

	backend = (backend_factory) ();
	g_assert (backend != NULL);

	g_hash_table_insert (factory->priv->active_server_map,
			     g_strdup (request->uri),
			     backend);

	return backend;
}

static void
pas_book_factory_process_request (PASBookFactory              *factory,
				  PASBookFactoryQueuedRequest *request)
{
	PASBackend *backend;

	request = factory->priv->queued_requests->data;

	backend = g_hash_table_lookup (factory->priv->active_server_map, request->uri);

	if (backend == NULL) {

		backend = pas_book_factory_launch_backend (factory, request);
		pas_backend_add_client (backend, request->listener);
		pas_backend_load_uri (backend, request->uri);
		g_free (request->uri);

		return;
	}

	g_free (request->uri);

	pas_backend_add_client (backend, request->listener);
}

static gboolean
pas_book_factory_process_queue (PASBookFactory *factory)
{
	/* Process pending Book-creation requests. */
	if (factory->priv->queued_requests != NULL) {
		PASBookFactoryQueuedRequest  *request;

		request = factory->priv->queued_requests->data;

		pas_book_factory_process_request (factory, request);

		factory->priv->queued_requests = g_list_remove (
			factory->priv->queued_requests, request);

		g_free (request);
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

/**
 * pas_book_factory_activate:
 */
void
pas_book_factory_activate (PASBookFactory *factory)
{
	CORBA_Environment  ev;
	int                ret;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (PAS_IS_BOOK_FACTORY (factory));

	CORBA_exception_init (&ev);

	ret = goad_server_register (
		NULL,
		bonobo_object_corba_objref (BONOBO_OBJECT (factory)),
		PAS_BOOK_FACTORY_GOAD_ID, "server",
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("pas_book_factory_activate: Exception "
			   "registering PASBookFactory!\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	if (ret == -1) {
		g_message ("pas_book_factory_activate: Error "
			   "registering PASBookFactory!\n");
		return;
	}

	if (ret == -2) {
		g_message ("pas_book_factory_activate: Another "
			   "PASBookFactory is already running.\n");
		return;
		
	}

	return;
}


static void
pas_book_factory_init (PASBookFactory *factory)
{
	factory->priv = g_new0 (PASBookFactoryPrivate, 1);

	factory->priv->active_server_map = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->backends          = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->queued_requests   = NULL;
}

static gboolean
pas_book_factory_remove_asm_entry (gpointer key, gpointer value,
				   gpointer data)
{
	CORBA_Environment ev;

	g_free (key);

	CORBA_exception_init (&ev);
	CORBA_Object_release ((CORBA_Object) value, &ev);
	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
pas_book_factory_remove_backend_entry (gpointer key, gpointer value,
				       gpointer data)
{
	g_free (key);
	return TRUE;
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

	g_hash_table_foreach_remove (factory->priv->active_server_map,
				     pas_book_factory_remove_asm_entry,
				     NULL);
	g_hash_table_destroy (factory->priv->active_server_map);

	g_hash_table_foreach_remove (factory->priv->backends,
				     pas_book_factory_remove_backend_entry,
				     NULL);
	g_hash_table_destroy (factory->priv->backends);
	
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

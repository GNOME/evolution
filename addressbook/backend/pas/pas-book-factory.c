/*
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <addressbook.h>
#include <pas-book-factory.h>

static GnomeObjectClass          *pas_book_factory_parent_class;
POA_Evolution_BookFactory__vepv   pas_book_factory_vepv;

typedef struct {
	Evolution_BookListener             listener;
	Evolution_BookListener_CallStatus  status;
} PASBookFactoryQueuedResponse;

typedef struct {
	char                              *uri;
	Evolution_BookListener             listener;
} PASBookFactoryQueuedRequest;

struct _PASBookFactoryPrivate {
	GHashTable *backends;
	GHashTable *active_server_map;
	GList      *queued_responses;
	GList      *queued_requests;
};

static char *
pas_book_factory_canonicalize_uri (const char *uri)
{
	char *canon;
	char *p;

	/* FIXME: What do I do here? */

	canon = g_strdup (uri);

	for (p = canon; *p != '\0'; p ++)
		*p = toupper (*p);

	return canon;
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
pas_book_factory_register_backend (PASBookFactory               *factory,
				   const char                   *proto,
				   PASBookFactoryBackendFactory  backend)
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

static PASBookFactoryBackendFactory
pas_book_factory_lookup_backend_factory (PASBookFactory *factory,
					 const char     *uri)
{
	PASBookFactoryBackendFactory  backend;
	char                         *proto;
	char                         *canonical_uri;

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

static void
pas_book_factory_process_request (PASBookFactory              *factory,
				  PASBookFactoryQueuedRequest *request)
{
	request = factory->priv->queued_requests->data;

	/*
	 * Check to see if there is already a running backend for this
	 * URI.
	 */
	

	backend = pas_book_factory_lookup_backend_factory (
		factory, request->uri);
	g_assert (backend != NULL);

	(backend) (factory, request->uri, request->listener);
}


static void
pas_book_factory_process_response (PASBookFactory               *factory,
				   PASBookFactoryQueuedResponse *response)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Evolution_BookListener_respond_open_book (
		response->listener, response->status,
		CORBA_OBJECT_NIL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("PASBookFactory: Exception while sending "
			   "response to BookListener!\n");

		CORBA_exception_free (&ev);
		CORBA_exception_init (&ev);
	}

	CORBA_Object_release (response->listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("PASBookFactory: Exception releasing "
			   "BookListener!\n");
	}

	CORBA_exception_free (&ev);
}


static gboolean
pas_book_factory_process_queues (PASBookFactory *factory)
{
	/* Process pending Book-creation requests. */
	while (factory->priv->queued_requests != NULL) {
		PASBookFactoryQueuedRequest  *request;

		pas_book_factory_process_request (factory, request);

		factory->priv->queued_requests = g_list_remove (
			factory->priv->queued_requests, request);

		g_free (request);
	}

	/* Flush the outgoing error queue. */
	while (factory->priv->queued_responses != NULL) {
		PASBookFactoryQueuedResponse *response;

		response = factory->priv->queued_responses->data;

		pas_book_factory_process_response (factory, response);
		factory->priv->queued_responses = g_list_remove (
			factory->priv->queued_responses, response);

		g_free (response);
	}

	return TRUE;
}

static void
pas_book_factory_queue_response (PASBookFactory                    *factory,
				 const Evolution_BookListener       listener,
				 Evolution_BookListener_CallStatus  status)
{
	PASBookFactoryQueuedResponse *response;
	Evolution_BookListener        listener_copy;
	CORBA_Environment             ev;

	CORBA_exception_init (&ev);

	listener_copy = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("PASBookFactory: Could not duplicate BookListener!\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	response           = g_new0 (PASBookFactoryQueuedResponse, 1);
	response->listener = listener_copy;
	response->status   = status;

	factory->priv->queued_responses =
		g_list_prepend (factory->priv->queued_responses, response);
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
}

static void
impl_Evolution_BookFactory_open_book (PortableServer_Servant        servant,
				      const CORBA_char             *uri,
				      const Evolution_BookListener  listener,
				      CORBA_Environment            *ev)
{
	PASBookFactory               *factory =
		PAS_BOOK_FACTORY (gnome_object_from_servant (servant));

	PASBookFactoryBackendFactory  backend;

	backend = pas_book_factory_lookup_backend_factory (factory, uri);

	if (backend == NULL) {
		g_warning ("PASBookFactory: No backend found for uri: %s\n", uri);

		pas_book_factory_queue_response (
			factory, listener,
			Evolution_BookListener_ProtocolNotSupported);

		return;
	}

	pas_book_factory_queue_request (factory, uri, listener);
}

static PASBookFactory *
pas_book_factory_construct (PASBookFactory *factory)
{
	POA_Evolution_BookFactory  *servant;
	CORBA_Environment           ev;
	CORBA_Object                obj;

	g_assert (factory != NULL);
	g_assert (PAS_IS_BOOK_FACTORY (factory));

	servant = (POA_Evolution_BookFactory *) g_new0 (GnomeObjectServant, 1);
	servant->vepv = &pas_book_factory_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_BookFactory__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);

		return NULL;
	}

	CORBA_exception_free (&ev);

	obj = gnome_object_activate_servant (GNOME_OBJECT (factory), servant);
	if (obj == CORBA_OBJECT_NIL) {
		g_free (servant);

		return NULL;
	}

	gnome_object_construct (GNOME_OBJECT (factory), obj);

	return factory;
}

/**
 * pas_book_factory_new:
 */
PASBookFactory *
pas_book_factory_new (void)
{
	PASBookFactory *factory;
	PASBookFactory *retval;

	factory = gtk_type_new (PAS_BOOK_FACTORY_TYPE);

	retval = pas_book_factory_construct (factory);

	if (retval == NULL) {
		g_warning ("pas_book_factoy_new: Could not construct PASBookFactory!\n");
		gtk_object_unref (GTK_OBJECT (factory));

		return NULL;
	}

	return retval;
}

static void
pas_book_factory_init (PASBookFactory *factory)
{
	factory->priv = g_new0 (PASBookFactoryPrivate, 1);

	factory->priv->active_server_map = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->backends          = g_hash_table_new (g_str_hash, g_str_equal);
	factory->priv->queued_requests   = NULL;
	factory->priv->queued_responses  = NULL;

	g_idle_add ((GSourceFunc) pas_book_factory_process_queues, factory);
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

	for (l = factory->priv->queued_responses; l != NULL; l = l->next) {
		PASBookFactoryQueuedResponse *response = l->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (response->listener, &ev);
		CORBA_exception_free (&ev);

		g_free (response);
	}
	g_list_free (factory->priv->queued_responses);
	factory->priv->queued_responses = NULL;

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
	pas_book_factory_vepv.GNOME_Unknown_epv         = gnome_object_get_epv ();
	pas_book_factory_vepv.Evolution_BookFactory_epv = pas_book_factory_get_epv ();
}

static void
pas_book_factory_class_init (PASBookFactoryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	pas_book_factory_parent_class = gtk_type_class (gnome_object_get_type ());

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

		type = gtk_type_unique (gnome_object_get_type (), &info);
	}

	return type;
}

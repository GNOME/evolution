/*
 * Copyright 2000, Helix Code, Inc.
 */

#include <gtk/gtkobject.h>
#include <pas-backend.h>

typedef struct {
	Evolution_BookListener listener;
} PASClient;

struct _PASBackendPrivate {
	gboolean  book_loaded;
	GList    *clients;
	GList    *response_queue;
};

PASBackend *
pas_backend_new (void)
{
	PASBackend *backend;

	backend = gtk_type_new (pas_backend_get_type ());

	return backend;
}

void
pas_backend_load_uri (PASBackend             *backend,
		      char                   *uri)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (uri != NULL);
}

/**
 * pas_backend_add_client:
 * @backend:
 * @listener:
 */
void
pas_backend_add_client (PASBackend             *backend,
			Evolution_BookListener  listener)
{
	PASClient *client;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	client = g_new0 (PASClient, 1);

	client->listener = listener;

	if (backend->priv->book_loaded) {
		
	}
}


void
pas_backend_remove_client (PASBackend             *backend,
			   Evolution_BookListener  listener)
{
}

/* Synchronous operations. */
char *
pas_backend_get_vcard (PASBackend             *backend,
		       PASBook                *book,
		       char                   *id)
{
}

/* Asynchronous operations. */

/**
 * pas_backend_queue_remove_card:
 */
void
pas_backend_queue_create_card (PASBackend             *backend,
			       PASBook                *book,
			       char                   *vcard)
{
}

/**
 * pas_backend_queue_remove_card:
 */
void
pas_backend_queue_remove_card (PASBackend             *backend,
			       PASBook                *book,
			       char                   *id)
{
}

/**
 * pas_backend_queue_modify_card:
 */
void
pas_backend_queue_modify_card (PASBackend             *backend,
			       PASBook                *book,
			       char                   *id,
			       char                   *vcard)
{
}

static void
pas_backend_init (PASBackend *backend)
{
	PASBackendPrivate *priv;
	
	priv              = g_new0 (PASBackendPrivate, 1);
	priv->book_loaded = FALSE;
	priv->clients     = NULL;
}

static void
pas_backend_class_init (PASBackendClass *klass)
{
}

/**
 * pas_backend_get_type:
 */
GtkType
pas_backend_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBackend",
			sizeof (PASBackend),
			sizeof (PASBackendClass),
			(GtkClassInitFunc)  pas_backend_class_init,
			(GtkObjectInitFunc) pas_backend_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}

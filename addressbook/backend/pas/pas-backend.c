/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include "pas-backend.h"
#include "pas-marshal.h"

/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static guint pas_backend_signals[LAST_SIGNAL];


gboolean
pas_backend_construct (PASBackend *backend)
{
	return TRUE;
}

GNOME_Evolution_Addressbook_BookListener_CallStatus
pas_backend_load_uri (PASBackend             *backend,
		      const char             *uri)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->load_uri != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->load_uri) (backend, uri);
}

/**
 * pas_backend_get_uri:
 * @backend: An addressbook backend.
 * 
 * Queries the URI that an addressbook backend is serving.
 * 
 * Return value: URI for the backend.
 **/
const char *
pas_backend_get_uri (PASBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_uri != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_uri) (backend);
}

/**
 * pas_backend_add_client:
 * @backend: An addressbook backend.
 * @listener: Listener for notification to the client.
 *
 * Adds a client to an addressbook backend.
 *
 * Return value: TRUE on success, FALSE on failure to add the client.
 */
gboolean
pas_backend_add_client (PASBackend             *backend,
			GNOME_Evolution_Addressbook_BookListener  listener)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	g_return_val_if_fail (listener != CORBA_OBJECT_NIL, FALSE);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->add_client != NULL);

	return PAS_BACKEND_GET_CLASS (backend)->add_client (backend, listener);
}

void
pas_backend_remove_client (PASBackend *backend,
			   PASBook    *book)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (book    != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));
	
	g_assert (PAS_BACKEND_GET_CLASS (backend)->remove_client != NULL);

	PAS_BACKEND_GET_CLASS (backend)->remove_client (backend, book);
}

char *
pas_backend_get_static_capabilities (PASBackend *backend)
{
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), NULL);
	
	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_static_capabilities != NULL);

	return PAS_BACKEND_GET_CLASS (backend)->get_static_capabilities (backend);
}

/**
 * pas_backend_last_client_gone:
 * @backend: An addressbook backend.
 * 
 * Emits the "last_client_gone" signal for the specified backend.  Should
 * only be called from backend implementations if the backend really does
 * not have any more clients.
 **/
void
pas_backend_last_client_gone (PASBackend *backend)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));

	g_signal_emit (backend, pas_backend_signals[LAST_CLIENT_GONE], 0);	
}

static void
pas_backend_init (PASBackend *backend)
{
}

static void
pas_backend_class_init (PASBackendClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	klass->add_client = NULL;
	klass->remove_client = NULL;
	klass->get_static_capabilities = NULL;

	pas_backend_signals[LAST_CLIENT_GONE] =
		g_signal_new ("last_client_gone",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PASBackendClass, last_client_gone),
			      NULL, NULL,
			      pas_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

/**
 * pas_backend_get_type:
 */
GType
pas_backend_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (PASBackendClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  pas_backend_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PASBackend),
			0,    /* n_preallocs */
			(GInstanceInitFunc) pas_backend_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "PASBackend", &info, 0);
	}

	return type;
}

/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>
#include "pas-backend.h"

#define CLASS(o) PAS_BACKEND_CLASS (GTK_OBJECT (o)->klass)

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

gboolean
pas_backend_load_uri (PASBackend             *backend,
		      const char             *uri)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	g_assert (CLASS (backend)->load_uri != NULL);

	return (* CLASS (backend)->load_uri) (backend, uri);
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

	g_assert (CLASS (backend)->get_uri != NULL);

	return (* CLASS (backend)->get_uri) (backend);
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
			Evolution_BookListener  listener)
{
	g_return_val_if_fail (backend != NULL, FALSE);
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	g_return_val_if_fail (listener != CORBA_OBJECT_NIL, FALSE);

	g_assert (CLASS (backend)->add_client != NULL);

	return CLASS (backend)->add_client (backend, listener);
}

void
pas_backend_remove_client (PASBackend *backend,
			   PASBook    *book)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (book    != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));
	
	g_assert (CLASS (backend)->remove_client != NULL);

	CLASS (backend)->remove_client (backend, book);
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

	gtk_signal_emit (GTK_OBJECT (backend), pas_backend_signals[LAST_CLIENT_GONE]);	
}

static void
pas_backend_init (PASBackend *backend)
{
}

static void
pas_backend_class_init (PASBackendClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	pas_backend_signals[LAST_CLIENT_GONE] =
		gtk_signal_new ("last_client_gone",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (PASBackendClass, last_client_gone),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, pas_backend_signals, LAST_SIGNAL);
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

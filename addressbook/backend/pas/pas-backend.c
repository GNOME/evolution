/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <gtk/gtkobject.h>
#include <pas-backend.h>

#define CLASS(o) PAS_BACKEND_CLASS (GTK_OBJECT (o)->klass)

gboolean
pas_backend_construct (PASBackend *backend)
{
	return TRUE;
}

void
pas_backend_load_uri (PASBackend             *backend,
		      const char             *uri)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (uri != NULL);

	g_assert (CLASS (backend)->load_uri != NULL);

	CLASS (backend)->load_uri (backend, uri);
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
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (listener != CORBA_OBJECT_NIL);

	g_assert (CLASS (backend)->add_client != NULL);

	CLASS (backend)->add_client (backend, listener);
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

static void
pas_backend_init (PASBackend *backend)
{
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

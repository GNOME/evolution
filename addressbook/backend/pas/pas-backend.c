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

struct _PASBackendPrivate {
	GList *clients;
	gboolean loaded, writable;
};

/* Signal IDs */
enum {
	LAST_CLIENT_GONE,
	LAST_SIGNAL
};

static guint pas_backend_signals[LAST_SIGNAL];

static GObjectClass *parent_class;

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
	g_return_val_if_fail (backend->priv->loaded == FALSE, FALSE);

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
	g_return_val_if_fail (backend->priv->loaded, NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_uri != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_uri) (backend);
}


void
pas_backend_create_card (PASBackend *backend,
			 PASBook    *book,
			 PASCreateCardRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->vcard != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->create_card != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->create_card) (backend, book, req);
}

void
pas_backend_remove_card (PASBackend *backend,
			 PASBook *book,
			 PASRemoveCardRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->id != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->remove_card != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->remove_card) (backend, book, req);
}

void
pas_backend_modify_card (PASBackend *backend,
			 PASBook *book,
			 PASModifyCardRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->vcard != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->modify_card != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->modify_card) (backend, book, req);
}

void
pas_backend_check_connection (PASBackend *backend,
			      PASBook *book,
			      PASCheckConnectionRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->check_connection != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->check_connection) (backend, book, req);
}

void
pas_backend_get_vcard (PASBackend *backend,
		       PASBook *book,
		       PASGetVCardRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->id != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_vcard != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_vcard) (backend, book, req);
}

void
pas_backend_get_cursor (PASBackend *backend,
			PASBook *book,
			PASGetCursorRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->search != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_cursor != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_cursor) (backend, book, req);
}

void
pas_backend_get_book_view (PASBackend *backend,
			   PASBook *book,
			   PASGetBookViewRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->search != NULL && req->listener != CORBA_OBJECT_NIL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_book_view != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_book_view) (backend, book, req);
}

void
pas_backend_get_completion_view (PASBackend *backend,
				 PASBook *book,
				 PASGetCompletionViewRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->search != NULL && req->listener != CORBA_OBJECT_NIL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_completion_view != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_completion_view) (backend, book, req);
}

void
pas_backend_get_changes (PASBackend *backend,
			 PASBook *book,
			 PASGetChangesRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL && req->change_id != NULL && req->listener != CORBA_OBJECT_NIL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_changes != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_changes) (backend, book, req);
}

void
pas_backend_authenticate_user (PASBackend *backend,
			       PASBook *book,
			       PASAuthenticateUserRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->authenticate_user != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->authenticate_user) (backend, book, req);
}

void
pas_backend_get_supported_fields (PASBackend *backend,
				  PASBook *book,
				  PASGetSupportedFieldsRequest *req)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (PAS_IS_BOOK (book));
	g_return_if_fail (req != NULL);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->get_supported_fields != NULL);

	return (* PAS_BACKEND_GET_CLASS (backend)->get_supported_fields) (backend, book, req);
}

static void
process_client_requests (PASBook *book, gpointer user_data)
{
	PASBackend *backend;
	PASRequest *req;

	backend = PAS_BACKEND (user_data);

	req = pas_book_pop_request (book);
	if (req == NULL)
		return;

	switch (req->op) {
	case CreateCard:
		pas_backend_create_card (backend, book, &req->create);
		break;

	case RemoveCard:
		pas_backend_remove_card (backend, book, &req->remove);
		break;

	case ModifyCard:
		pas_backend_modify_card (backend, book, &req->modify);
		break;

	case CheckConnection:
		pas_backend_check_connection (backend, book, &req->check_connection);
		break;

	case GetVCard:
		pas_backend_get_vcard (backend, book, &req->get_vcard);
		break;

	case GetCursor:
		pas_backend_get_cursor (backend, book, &req->get_cursor);
		break;

	case GetBookView:
		pas_backend_get_book_view (backend, book, &req->get_book_view);
		break;

	case GetCompletionView:
		pas_backend_get_completion_view (backend, book, &req->get_completion_view);
		break;

	case GetChanges:
		pas_backend_get_changes (backend, book, &req->get_changes);
		break;

	case AuthenticateUser:
		pas_backend_authenticate_user (backend, book, &req->auth_user);
		break;

	case GetSupportedFields:
		pas_backend_get_supported_fields (backend, book, &req->get_supported_fields);
		break;
	}

	pas_book_free_request (req);
}

static void
book_destroy_cb (gpointer data, GObject *where_book_was)
{
	PASBackend *backend = PAS_BACKEND (data);

	pas_backend_remove_client (backend, (PASBook *)where_book_was);
}

static void
last_client_gone (PASBackend *backend)
{
	g_signal_emit (backend, pas_backend_signals[LAST_CLIENT_GONE], 0);
}

static gboolean
add_client (PASBackend                               *backend,
	    GNOME_Evolution_Addressbook_BookListener  listener)
{
	PASBook *book;

	book = pas_book_new (backend, listener);
	if (!book) {
		if (!backend->priv->clients)
			last_client_gone (backend);

		return FALSE;
	}

	g_object_weak_ref (G_OBJECT (book), book_destroy_cb, backend);

	g_signal_connect (book, "requests_queued",
			  G_CALLBACK (process_client_requests), backend);

	backend->priv->clients = g_list_prepend (backend->priv->clients, book);

	if (backend->priv->loaded) {
		pas_book_respond_open (
			book, GNOME_Evolution_Addressbook_BookListener_Success);
	} else {
		pas_book_respond_open (
			book, GNOME_Evolution_Addressbook_BookListener_OtherError);
	}

	pas_book_report_writable (book, backend->priv->writable);

	bonobo_object_unref (BONOBO_OBJECT (book));

	return TRUE;
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
pas_backend_add_client (PASBackend                               *backend,
			GNOME_Evolution_Addressbook_BookListener  listener)
{
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	g_return_val_if_fail (listener != CORBA_OBJECT_NIL, FALSE);

	g_assert (PAS_BACKEND_GET_CLASS (backend)->add_client != NULL);

	return PAS_BACKEND_GET_CLASS (backend)->add_client (backend, listener);
}

static void
remove_client (PASBackend *backend,
	       PASBook    *book)
{
	/* Disconnect */
	backend->priv->clients = g_list_remove (backend->priv->clients, book);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!backend->priv->clients)
		last_client_gone (backend);
}

void
pas_backend_remove_client (PASBackend *backend,
			   PASBook    *book)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
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

gboolean
pas_backend_is_loaded (PASBackend *backend)
{
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);

	return backend->priv->loaded;
}

void
pas_backend_set_is_loaded (PASBackend *backend, gboolean is_loaded)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));

	backend->priv->loaded = is_loaded;
}

gboolean
pas_backend_is_writable (PASBackend *backend)
{
	g_return_val_if_fail (PAS_IS_BACKEND (backend), FALSE);
	
	return backend->priv->writable;
}

void
pas_backend_set_is_writable (PASBackend *backend, gboolean is_writable)
{
	g_return_if_fail (PAS_IS_BACKEND (backend));
	
	backend->priv->writable = is_writable;
}

static void
pas_backend_init (PASBackend *backend)
{
	PASBackendPrivate *priv;

	priv          = g_new0 (PASBackendPrivate, 1);
	priv->clients = NULL;

	backend->priv = priv;
}

static void
pas_backend_dispose (GObject *object)
{
	PASBackend *backend;

	backend = PAS_BACKEND (object);

	if (backend->priv) {
		g_list_free (backend->priv->clients);
		g_free (backend->priv);

		backend->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
pas_backend_class_init (PASBackendClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass *) klass;

	klass->add_client = add_client;
	klass->remove_client = remove_client;

	object_class->dispose = pas_backend_dispose;

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

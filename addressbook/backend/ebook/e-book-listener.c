/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Exports the BookListener interface.  Maintains a queue of messages
 * which come in on the interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-book-listener.h"

static EBookStatus e_book_listener_convert_status (Evolution_BookListener_CallStatus status);

enum {
	RESPONSES_QUEUED,
	LAST_SIGNAL
};

static guint e_book_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *e_book_listener_parent_class;
POA_Evolution_BookListener__vepv  e_book_listener_vepv;

struct _EBookListenerPrivate {
	GList *response_queue;
	gint   idle_id;
};

static gboolean
e_book_listener_check_queue (EBookListener *listener)
{
	if (listener->priv->response_queue != NULL) {
		gtk_signal_emit (GTK_OBJECT (listener),
				 e_book_listener_signals [RESPONSES_QUEUED]);
	}

	if (listener->priv->response_queue == NULL) {
		listener->priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
e_book_listener_queue_response (EBookListener         *listener,
				EBookListenerResponse *response)
{
	listener->priv->response_queue =
		g_list_append (listener->priv->response_queue,
			       response);

	if (listener->priv->idle_id == 0) {
		listener->priv->idle_id = g_idle_add (
			(GSourceFunc) e_book_listener_check_queue, listener);
	}
}

/* Add, Remove, Modify */
static void
e_book_listener_queue_generic_response (EBookListener          *listener,
					EBookListenerOperation  op,
					EBookStatus             status)
{
	EBookListenerResponse *resp;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = op;
	resp->status = status;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_open_response (EBookListener  *listener,
				     EBookStatus     status,
				     Evolution_Book  book)
{
	EBookListenerResponse *resp;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = OpenBookResponse;
	resp->status = status;
	resp->book   = book;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_open_progress (EBookListener *listener,
				     const char    *msg,
				     short          percent)
{
	EBookListenerResponse *resp;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op      = OpenProgressEvent;
	resp->msg     = g_strdup (msg);
	resp->percent = percent;

	e_book_listener_queue_response (listener, resp);
}


static void
e_book_listener_queue_create_card_response (EBookListener *listener,
					    EBookStatus    status,
					    const char    *id)
{
	EBookListenerResponse *resp;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = CreateCardResponse;
	resp->status = status;
	resp->id     = g_strdup (id);

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_cursor_response (EBookListener        *listener,
					   EBookStatus           status,
					   Evolution_CardCursor  cursor)
{
	EBookListenerResponse *resp;
	
	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = GetCursorResponse;
	resp->status = status;
	resp->cursor = cursor;
	
	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_view_response (EBookListener        *listener,
					 EBookStatus           status,
					 Evolution_BookView    book_view)
{
	EBookListenerResponse *resp;
	
	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = GetBookViewResponse;
	resp->status    = status;
	resp->book_view = book_view;
	
	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_link_status (EBookListener *listener,
				   gboolean       connected)
{
	EBookListenerResponse *resp;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = LinkStatusEvent;
	resp->connected = connected;

	e_book_listener_queue_response (listener, resp);
}

static void
impl_BookListener_respond_create_card (PortableServer_Servant                   servant,
				       const Evolution_BookListener_CallStatus  status,
				       const Evolution_CardId                   id,
				       CORBA_Environment                       *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_create_card_response (
		listener,
		e_book_listener_convert_status (status),
		id);
}

static void
impl_BookListener_respond_remove_card (PortableServer_Servant servant,
				       const Evolution_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_generic_response (
		listener, RemoveCardResponse,
		e_book_listener_convert_status (status));
}

static void
impl_BookListener_respond_modify_card (PortableServer_Servant servant,
				       const Evolution_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_generic_response (
		listener, ModifyCardResponse,
		e_book_listener_convert_status (status));
}

static void
impl_BookListener_respond_get_cursor (PortableServer_Servant servant,
				      const Evolution_BookListener_CallStatus status,
				      const Evolution_CardCursor cursor,
				      CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	Evolution_CardCursor  cursor_copy;

	cursor_copy = CORBA_Object_duplicate (cursor, ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("EBookListener: Exception while duplicating CardCursor!\n");
		return;
	}

	e_book_listener_queue_get_cursor_response (
		listener,
		e_book_listener_convert_status (status),
		cursor_copy);
}

static void
impl_BookListener_respond_get_view (PortableServer_Servant servant,
				    const Evolution_BookListener_CallStatus status,
				    const Evolution_BookView book_view,
				    CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	Evolution_BookView    book_view_copy;

	book_view_copy = CORBA_Object_duplicate (book_view, ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("EBookListener: Exception while duplicating BookView.\n");
		return;
	}

	e_book_listener_queue_get_view_response (
		listener,
		e_book_listener_convert_status (status),
		book_view_copy);
}

static void
impl_BookListener_respond_open_book (PortableServer_Servant servant,
				     const Evolution_BookListener_CallStatus status,
				     const Evolution_Book book,
				     CORBA_Environment *ev)
{
	EBookListener  *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	Evolution_Book  book_copy;

	book_copy = CORBA_Object_duplicate (book, ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("EBookListener: Exception while duplicating Book!\n");
		return;
	}

	e_book_listener_queue_open_response (
		listener,
		e_book_listener_convert_status (status),
		book_copy);
}

static void
impl_BookListener_report_open_book_progress (PortableServer_Servant servant,
					     const CORBA_char *status_message,
					     const CORBA_short percent,
					     CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_open_progress (
		listener, status_message, percent);
}

static void
impl_BookListener_report_connection_status (PortableServer_Servant servant,
					    const CORBA_boolean connected,
					    CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_link_status (
		listener, connected);
}

/**
 * e_book_listener_check_pending:
 * @listener: the #EBookListener 
 *
 * Returns: the number of items on the response queue,
 * or -1 if the @listener is isn't an #EBookListener.
 */
int
e_book_listener_check_pending (EBookListener *listener)
{
	g_return_val_if_fail (listener != NULL,              -1);
	g_return_val_if_fail (E_IS_BOOK_LISTENER (listener), -1);

	return g_list_length (listener->priv->response_queue);
}

/**
 * e_book_listener_pop_response:
 * @listener: the #EBookListener for which a request is to be popped
 *
 * Returns: an #EBookListenerResponse if there are responses on the
 * queue to be returned; %NULL if there aren't, or if the @listener
 * isn't an EBookListener.
 */
EBookListenerResponse *
e_book_listener_pop_response (EBookListener *listener)
{
	EBookListenerResponse *resp;
	GList                 *popped;

	g_return_val_if_fail (listener != NULL,              NULL);
	g_return_val_if_fail (E_IS_BOOK_LISTENER (listener), NULL);

	if (listener->priv->response_queue == NULL)
		return NULL;

	resp = listener->priv->response_queue->data;

	popped = listener->priv->response_queue;
	listener->priv->response_queue =
		g_list_remove_link (listener->priv->response_queue,
				    listener->priv->response_queue);
	g_list_free_1 (popped);

	return resp;
}

static EBookStatus
e_book_listener_convert_status (const Evolution_BookListener_CallStatus status)
{
	switch (status) {
	case Evolution_BookListener_Success:
		return E_BOOK_STATUS_SUCCESS;
	case Evolution_BookListener_RepositoryOffline:
		return E_BOOK_STATUS_REPOSITORY_OFFLINE;
	case Evolution_BookListener_PermissionDenied:
		return E_BOOK_STATUS_PERMISSION_DENIED;
	case Evolution_BookListener_CardNotFound:
		return E_BOOK_STATUS_CARD_NOT_FOUND;
	case Evolution_BookListener_ProtocolNotSupported:
		return E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED;
	case Evolution_BookListener_OtherError:
		return E_BOOK_STATUS_OTHER_ERROR;
	default:
		g_warning ("e_book_listener_convert_status: Unknown status "
			   "from card server: %d\n", (int) status);
		return E_BOOK_STATUS_UNKNOWN;

	}
}

static EBookListener *
e_book_listener_construct (EBookListener *listener)
{
	POA_Evolution_BookListener *servant;
	CORBA_Environment           ev;
	CORBA_Object                obj;

	g_assert (listener != NULL);
	g_assert (E_IS_BOOK_LISTENER (listener));

	servant = (POA_Evolution_BookListener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &e_book_listener_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_BookListener__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);

		return NULL;
	}

	CORBA_exception_free (&ev);

	obj = bonobo_object_activate_servant (BONOBO_OBJECT (listener), servant);
	if (obj == CORBA_OBJECT_NIL) {
		g_free (servant);

		return NULL;
	}

	bonobo_object_construct (BONOBO_OBJECT (listener), obj);

	return listener;
}

/**
 * e_book_listener_new:
 * @book: the #EBook for which the listener is to be bound
 *
 * Creates and returns a new #EBookListener for the book.
 *
 * Returns: a new #EBookListener
 */
EBookListener *
e_book_listener_new ()
{
	EBookListener *listener;
	EBookListener *retval;

	listener = gtk_type_new (E_BOOK_LISTENER_TYPE);

	retval = e_book_listener_construct (listener);

	if (retval == NULL) {
		g_warning ("e_book_listener_new: Error constructing "
			   "EBookListener!\n");
		gtk_object_unref (GTK_OBJECT (listener));
		return NULL;
	}

	return retval;
}

static void
e_book_listener_init (EBookListener *listener)
{
	listener->priv = g_new0 (EBookListenerPrivate, 1);
}

static void
e_book_listener_destroy (GtkObject *object)
{
	EBookListener     *listener = E_BOOK_LISTENER (object);
	GList             *l;

	for (l = listener->priv->response_queue; l != NULL; l = l->next) {
		EBookListenerResponse *resp = l->data;

		g_free (resp->msg);
		g_free (resp->id);

		if (resp->book != CORBA_OBJECT_NIL) {
			CORBA_Environment ev;

			CORBA_exception_init (&ev);

			CORBA_Object_release (resp->book, &ev);

			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("e_book_listener_destroy: "
					   "Exception destroying book "
					   "in response queue!\n");
			}
			
			CORBA_exception_free (&ev);
		}

		if (resp->cursor != CORBA_OBJECT_NIL) {
			CORBA_Environment ev;

			CORBA_exception_init (&ev);

			CORBA_Object_release (resp->cursor, &ev);

			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("e_book_listener_destroy: "
					   "Exception destroying cursor "
					   "in response queue!\n");
			}
			
			CORBA_exception_free (&ev);
		}

		if (resp->book_view != CORBA_OBJECT_NIL) {
			CORBA_Environment ev;

			CORBA_exception_init (&ev);

			CORBA_Object_release (resp->book_view, &ev);

			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("e_book_listener_destroy: "
					   "Exception destroying book_view "
					   "in response queue!\n");
			}
			
			CORBA_exception_free (&ev);
		}

		g_free (resp);
	}
	g_list_free (listener->priv->response_queue);

	g_free (listener->priv);
	
	GTK_OBJECT_CLASS (e_book_listener_parent_class)->destroy (object);
}

POA_Evolution_BookListener__epv *
e_book_listener_get_epv (void)
{
	POA_Evolution_BookListener__epv *epv;

	epv = g_new0 (POA_Evolution_BookListener__epv, 1);

	epv->report_open_book_progress = impl_BookListener_report_open_book_progress;
	epv->respond_open_book         = impl_BookListener_respond_open_book;

	epv->respond_create_card       = impl_BookListener_respond_create_card;
	epv->respond_remove_card       = impl_BookListener_respond_remove_card;
	epv->respond_modify_card       = impl_BookListener_respond_modify_card;

	epv->respond_get_cursor        = impl_BookListener_respond_get_cursor;
	epv->respond_get_view          = impl_BookListener_respond_get_view;

	epv->report_connection_status  = impl_BookListener_report_connection_status;

	return epv;
}

static void
e_book_listener_corba_class_init (void)
{
	e_book_listener_vepv.Bonobo_Unknown_epv          = bonobo_object_get_epv ();
	e_book_listener_vepv.Evolution_BookListener_epv = e_book_listener_get_epv ();
}

static void
e_book_listener_class_init (EBookListenerClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	e_book_listener_parent_class = gtk_type_class (bonobo_object_get_type ());

	e_book_listener_signals [RESPONSES_QUEUED] =
		gtk_signal_new ("responses_queued",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookListenerClass, responses_queued),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_book_listener_signals, LAST_SIGNAL);

	object_class->destroy = e_book_listener_destroy;

	e_book_listener_corba_class_init ();
}

/**
 * e_book_listener_get_type:
 */
GtkType
e_book_listener_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"EBookListener",
			sizeof (EBookListener),
			sizeof (EBookListenerClass),
			(GtkClassInitFunc)  e_book_listener_class_init,
			(GtkObjectInitFunc) e_book_listener_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Exports the BookListener interface.  Maintains a queue of messages
 * which come in on the interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include <bonobo/bonobo-main.h>
#include "e-book-listener.h"
#include "e-book-marshal.h"

static EBookStatus e_book_listener_convert_status (GNOME_Evolution_Addressbook_BookListener_CallStatus status);

enum {
	RESPONSES_QUEUED,
	LAST_SIGNAL
};

static guint e_book_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *parent_class;
POA_GNOME_Evolution_Addressbook_BookListener__vepv  e_book_listener_vepv;

struct _EBookListenerPrivate {
	EBookListenerServant *servant;
	GNOME_Evolution_Addressbook_BookListener corba_objref;

	GList   *response_queue;
	gint     timeout_id;

	guint timeout_lock : 1;
	guint stopped      : 1;
};

static void
response_free (EBookListenerResponse *resp)
{
	if (resp == NULL)
		return;

	g_free (resp->msg);
	g_free (resp->id);

	if (resp->book != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		
		CORBA_exception_init (&ev);
		
		bonobo_object_release_unref (resp->book, &ev);
		
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
		
		bonobo_object_release_unref (resp->cursor, &ev);

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
		
		bonobo_object_release_unref (resp->book_view, &ev);
		
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_book_listener_destroy: "
				   "Exception destroying book_view "
				   "in response queue!\n");
		}
		
		CORBA_exception_free (&ev);
	}
	
	g_free (resp);
}

static gboolean
e_book_listener_check_queue (EBookListener *listener)
{
	if (listener->priv->timeout_lock)
		return TRUE;

	listener->priv->timeout_lock = TRUE;

	if (listener->priv->response_queue != NULL && !listener->priv->stopped) {
		g_signal_emit (listener, e_book_listener_signals [RESPONSES_QUEUED], 0);
	}

	if (listener->priv->response_queue == NULL || listener->priv->stopped) {
		listener->priv->timeout_id = 0;
		listener->priv->timeout_lock = FALSE;
		bonobo_object_unref (BONOBO_OBJECT (listener)); /* release the timeout's reference */
		return FALSE;
	}

	listener->priv->timeout_lock = FALSE;
	return TRUE;
}

static void
e_book_listener_queue_response (EBookListener         *listener,
				EBookListenerResponse *response)
{
	if (response == NULL)
		return;

	if (listener->priv->stopped) {
		response_free (response);
		return;
	}

	listener->priv->response_queue = g_list_append (listener->priv->response_queue, response);

	if (listener->priv->timeout_id == 0) {

		/* 20 == an arbitrary small integer */
		listener->priv->timeout_id = g_timeout_add (20, (GSourceFunc) e_book_listener_check_queue, listener);

		/* Hold a reference on behalf of the timeout */
		bonobo_object_ref (BONOBO_OBJECT (listener));
		
	}
}

/* Add, Remove, Modify */
static void
e_book_listener_queue_generic_response (EBookListener          *listener,
					EBookListenerOperation  op,
					EBookStatus             status)
{
	EBookListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = op;
	resp->status = status;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_open_response (EBookListener  *listener,
				     EBookStatus     status,
				     GNOME_Evolution_Addressbook_Book  book)
{
	EBookListenerResponse *resp;

	if (listener->priv->stopped)
		return;

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

	if (listener->priv->stopped)
		return;

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

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = CreateCardResponse;
	resp->status = status;
	resp->id     = g_strdup (id);

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_vcard_response (EBookListener        *listener,
					  EBookStatus           status,
					  const char           *vcard)
{
	EBookListenerResponse *resp;
	
	if (listener->priv->stopped)
		return;

	resp         = g_new0 (EBookListenerResponse, 1);

	resp->op     = GetCardResponse;
	resp->status = status;
	resp->vcard  = g_strdup (vcard);

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_cursor_response (EBookListener        *listener,
					   EBookStatus           status,
					   GNOME_Evolution_Addressbook_CardCursor  cursor)
{
	EBookListenerResponse *resp;
	
	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = GetCursorResponse;
	resp->status = status;
	resp->cursor = cursor;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_view_response (EBookListener        *listener,
					 EBookStatus           status,
					 GNOME_Evolution_Addressbook_BookView    book_view)
{
	EBookListenerResponse *resp;
	
	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = GetBookViewResponse;
	resp->status    = status;
	resp->book_view = book_view;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_changes_response (EBookListener        *listener,
					    EBookStatus           status,
					    GNOME_Evolution_Addressbook_BookView book_view)
{
	EBookListenerResponse *resp;
	
	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = GetChangesResponse;
	resp->status    = status;
	resp->book_view = book_view;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_link_status (EBookListener *listener,
				   gboolean       connected)
{
	EBookListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = LinkStatusEvent;
	resp->connected = connected;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_writable_status (EBookListener *listener,
				       gboolean       writable)
{
	EBookListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op        = WritableStatusEvent;
	resp->writable  = writable;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_authentication_response (EBookListener *listener,
					       EBookStatus    status)
{
	EBookListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = AuthenticationResponse;
	resp->status = status;

	e_book_listener_queue_response (listener, resp);
}

static void
e_book_listener_queue_get_supported_fields_response (EBookListener *listener,
						     EBookStatus status,
						     const GNOME_Evolution_Addressbook_stringlist *fields)
{
	EBookListenerResponse *resp;
	int i;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookListenerResponse, 1);

	resp->op     = GetSupportedFieldsResponse;
	resp->status = status;
	resp->fields = e_list_new ((EListCopyFunc)g_strdup, (EListFreeFunc)g_free, NULL);

	for (i = 0; i < fields->_length; i ++) {
		e_list_append (resp->fields, fields->_buffer[i]);
	}

	e_book_listener_queue_response (listener, resp);
}

static void
impl_BookListener_respond_create_card (PortableServer_Servant                   servant,
				       const GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
				       const CORBA_char* id,
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
				       const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_generic_response (
		listener, RemoveCardResponse,
		e_book_listener_convert_status (status));
}

static void
impl_BookListener_respond_modify_card (PortableServer_Servant servant,
				       const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_generic_response (
		listener, ModifyCardResponse,
		e_book_listener_convert_status (status));
}

static void
impl_BookListener_respond_get_vcard (PortableServer_Servant servant,
				     const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				     const CORBA_char* card,
				     CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_get_vcard_response (
		listener,
		e_book_listener_convert_status (status),
		g_strdup (card));
}

static void
impl_BookListener_respond_get_cursor (PortableServer_Servant servant,
				      const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				      const GNOME_Evolution_Addressbook_CardCursor cursor,
				      CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	GNOME_Evolution_Addressbook_CardCursor  cursor_copy;

	cursor_copy = bonobo_object_dup_ref (cursor, ev);

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
				    const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				    const GNOME_Evolution_Addressbook_BookView book_view,
				    CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	GNOME_Evolution_Addressbook_BookView    book_view_copy;

	book_view_copy = bonobo_object_dup_ref (book_view, ev);

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
impl_BookListener_respond_get_changes (PortableServer_Servant servant,
				       const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				       const GNOME_Evolution_Addressbook_BookView book_view,
				       CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	GNOME_Evolution_Addressbook_BookView    book_view_copy;

	book_view_copy = bonobo_object_dup_ref (book_view, ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("EBookListener: Exception while duplicating BookView.\n");
		return;
	}

	e_book_listener_queue_get_changes_response (
		listener,
		e_book_listener_convert_status (status),
		book_view_copy);
}

static void
impl_BookListener_respond_open_book (PortableServer_Servant servant,
				     const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
				     const GNOME_Evolution_Addressbook_Book book,
				     CORBA_Environment *ev)
{
	EBookListener  *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));
	GNOME_Evolution_Addressbook_Book  book_copy;

	book_copy = bonobo_object_dup_ref (book, ev);

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
impl_BookListener_respond_authentication_result (PortableServer_Servant servant,
						 const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
						 CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_authentication_response (
				       listener, status);
}

static void
impl_BookListener_response_get_supported_fields (PortableServer_Servant servant,
						 const GNOME_Evolution_Addressbook_BookListener_CallStatus status,
						 const GNOME_Evolution_Addressbook_stringlist *fields,
						 CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_get_supported_fields_response (
				     listener, status, fields);
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

static void
impl_BookListener_report_writable (PortableServer_Servant servant,
				   const CORBA_boolean writable,
				   CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object_from_servant (servant));

	e_book_listener_queue_writable_status (listener, writable);
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
e_book_listener_convert_status (const GNOME_Evolution_Addressbook_BookListener_CallStatus status)
{
	switch (status) {
	case GNOME_Evolution_Addressbook_BookListener_Success:
		return E_BOOK_STATUS_SUCCESS;
	case GNOME_Evolution_Addressbook_BookListener_RepositoryOffline:
		return E_BOOK_STATUS_REPOSITORY_OFFLINE;
	case GNOME_Evolution_Addressbook_BookListener_PermissionDenied:
		return E_BOOK_STATUS_PERMISSION_DENIED;
	case GNOME_Evolution_Addressbook_BookListener_CardNotFound:
		return E_BOOK_STATUS_CARD_NOT_FOUND;
	case GNOME_Evolution_Addressbook_BookListener_CardIdAlreadyExists:
		return E_BOOK_STATUS_CARD_ID_ALREADY_EXISTS;
	case GNOME_Evolution_Addressbook_BookListener_ProtocolNotSupported:
		return E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED;
	case GNOME_Evolution_Addressbook_BookListener_AuthenticationFailed:
		return E_BOOK_STATUS_AUTHENTICATION_FAILED;
	case GNOME_Evolution_Addressbook_BookListener_AuthenticationRequired:
		return E_BOOK_STATUS_AUTHENTICATION_REQUIRED;
	case GNOME_Evolution_Addressbook_BookListener_TLSNotAvailable:
		return E_BOOK_STATUS_TLS_NOT_AVAILABLE;
	case GNOME_Evolution_Addressbook_BookListener_NoSuchBook:
		return E_BOOK_STATUS_NO_SUCH_BOOK;
	case GNOME_Evolution_Addressbook_BookListener_OtherError:
		return E_BOOK_STATUS_OTHER_ERROR;
	default:
		g_warning ("e_book_listener_convert_status: Unknown status "
			   "from card server: %d\n", (int) status);
		return E_BOOK_STATUS_UNKNOWN;

	}
}

void
e_book_listener_construct (EBookListener *listener,
			   GNOME_Evolution_Addressbook_BookListener corba_objref)
{
	EBookListenerPrivate *priv;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (corba_objref != CORBA_OBJECT_NIL);

	priv = listener->priv;

	g_return_if_fail (priv->corba_objref == CORBA_OBJECT_NIL);

	priv->corba_objref = corba_objref;
}

static EBookListenerServant *
create_servant (EBookListener *listener)
{
	EBookListenerServant *servant;
	POA_GNOME_Evolution_Addressbook_BookListener *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (EBookListenerServant, 1);
	corba_servant = (POA_GNOME_Evolution_Addressbook_BookListener *) servant;

	corba_servant->vepv = &e_book_listener_vepv;
	POA_GNOME_Evolution_Addressbook_BookListener__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->object = listener;

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_BookListener
activate_servant (EBookListener *listener,
		  POA_GNOME_Evolution_Addressbook_BookListener *servant)
{
	GNOME_Evolution_Addressbook_BookListener corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), servant, &ev));

	corba_object = PortableServer_POA_servant_to_reference (bonobo_poa(), servant, &ev);

	if (ev._major == CORBA_NO_EXCEPTION && ! CORBA_Object_is_nil (corba_object, &ev)) {
		CORBA_exception_free (&ev);
		return corba_object;
	}

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
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
	EBookListenerPrivate *priv;
	GNOME_Evolution_Addressbook_BookListener corba_objref;

	listener = g_object_new (E_TYPE_BOOK_LISTENER, NULL);
	priv = listener->priv;

	priv->servant = create_servant (listener);
	corba_objref = activate_servant (listener, (POA_GNOME_Evolution_Addressbook_BookListener *) priv->servant);

	e_book_listener_construct (listener, corba_objref);

	return listener;
}

static void
e_book_listener_init (EBookListener *listener)
{
	listener->priv = g_new0 (EBookListenerPrivate, 1);
	listener->priv->corba_objref   = CORBA_OBJECT_NIL;
}

void
e_book_listener_stop (EBookListener *listener)
{
	g_return_if_fail (E_IS_BOOK_LISTENER (listener));

	listener->priv->stopped = TRUE;
}

static void
e_book_listener_dispose (GObject *object)
{
	EBookListener *listener = E_BOOK_LISTENER (object);
	GList *l;

	/* Remove our response queue handler: In theory, this can never happen since we
	 always hold a reference to the listener while the timeout is running. */
	if (listener->priv->timeout_id) {
		g_source_remove (listener->priv->timeout_id);
	}

	/* Clean up anything still sitting in response_queue */
	for (l = listener->priv->response_queue; l != NULL; l = l->next) {
		EBookListenerResponse *resp = l->data;

		response_free (resp);
	}
	g_list_free (listener->priv->response_queue);

	g_free (listener->priv);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
corba_class_init (EBookListenerClass *klass)
{
	POA_GNOME_Evolution_Addressbook_BookListener__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_BookListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;


	epv = &klass->epv;

	epv->notifyOpenBookProgress     = impl_BookListener_report_open_book_progress;
	epv->notifyBookOpened           = impl_BookListener_respond_open_book;
	epv->notifyCardCreated          = impl_BookListener_respond_create_card;
	epv->notifyCardRemoved          = impl_BookListener_respond_remove_card;
	epv->notifyCardModified         = impl_BookListener_respond_modify_card;
	epv->notifyAuthenticationResult = impl_BookListener_respond_authentication_result;
	epv->notifySupportedFields      = impl_BookListener_response_get_supported_fields;
	epv->notifyCardRequested        = impl_BookListener_respond_get_vcard;
	epv->notifyCursorRequested      = impl_BookListener_respond_get_cursor;
	epv->notifyViewRequested        = impl_BookListener_respond_get_view;
	epv->notifyChangesRequested     = impl_BookListener_respond_get_changes;
	epv->notifyConnectionStatus     = impl_BookListener_report_connection_status;
	epv->notifyWritable             = impl_BookListener_report_writable;

	vepv = &e_book_listener_vepv;
	vepv->_base_epv                                    = base_epv;
	vepv->GNOME_Evolution_Addressbook_BookListener_epv = epv;
}

static void
e_book_listener_class_init (EBookListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	e_book_listener_signals [RESPONSES_QUEUED] =
		g_signal_new ("responses_queued",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookListenerClass, responses_queued),
			      NULL, NULL,
			      e_book_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	object_class->dispose = e_book_listener_dispose;

	corba_class_init (klass);
}

BONOBO_TYPE_FUNC_FULL (
		       EBookListener,
		       GNOME_Evolution_Addressbook_BookListener,
		       BONOBO_TYPE_OBJECT,
		       e_book_listener);

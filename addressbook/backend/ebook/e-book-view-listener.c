/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Exports the BookViewListener interface.  Maintains a queue of messages
 * which come in on the interface.
 *
 * Author:
 *   Nat Friedman (nat@ximian.com)
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include <bonobo/bonobo-main.h>
#include "e-book-view-listener.h"
#include "e-book-view.h"
#include "e-card.h"
#include "e-book-marshal.h"

static EBookViewStatus e_book_view_listener_convert_status (GNOME_Evolution_Addressbook_BookViewListener_CallStatus status);

enum {
	RESPONSES_QUEUED,
	LAST_SIGNAL
};

static guint e_book_view_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *parent_class;
POA_GNOME_Evolution_Addressbook_BookViewListener__vepv  e_book_view_listener_vepv;

struct _EBookViewListenerPrivate {
	EBookViewListenerServant *servant;
	GNOME_Evolution_Addressbook_BookViewListener corba_objref;

	GList   *response_queue;
	gint     timeout_id;

	guint timeout_lock : 1;
	guint stopped      : 1;
};

static gboolean
e_book_view_listener_check_queue (EBookViewListener *listener)
{
	if (listener->priv->timeout_lock)
		return TRUE;

	listener->priv->timeout_lock = TRUE;

	if (listener->priv->response_queue != NULL && !listener->priv->stopped) {
		g_signal_emit (listener, e_book_view_listener_signals [RESPONSES_QUEUED], 0);
	}

	if (listener->priv->response_queue == NULL || listener->priv->stopped) {
		listener->priv->timeout_id = 0;
		listener->priv->timeout_lock = FALSE;
		bonobo_object_unref (BONOBO_OBJECT (listener));
		return FALSE;
	}

	listener->priv->timeout_lock = FALSE;
	return TRUE;
}

static void
e_book_view_listener_queue_response (EBookViewListener         *listener,
				     EBookViewListenerResponse *response)
{
	if (response == NULL)
		return;

	if (listener->priv->stopped) {
		/* Free response and return */
		g_free (response->id);
		g_list_foreach (response->cards, (GFunc) g_object_unref, NULL);
		g_list_free (response->cards);
		g_free (response->message);
		g_free (response);
		return;
	}

	/* a slight optimization for huge ldap queries.  if there's an
	   existing Add response on the end of the queue, and we're an
	   Add response, we just glom the two lists of cards
	   together */
	if (response->op == CardAddedEvent) {
		GList *last = g_list_last (listener->priv->response_queue);
		EBookViewListenerResponse *last_resp = NULL;

		if (last) last_resp = last->data;

		if (last_resp && last_resp->op == CardAddedEvent ) {
			response->cards = g_list_concat (last_resp->cards, response->cards);
			g_free (response);
			/* there should already be a timeout since the
			   queue isn't empty, so we'll just return
			   here */
			return;
		}
		else
			listener->priv->response_queue = g_list_append (last, response);
	}
	else
		listener->priv->response_queue = g_list_append (listener->priv->response_queue, response);

	if (listener->priv->timeout_id == 0) {

		/* Here, 20 == an arbitrary small number */		
		listener->priv->timeout_id = g_timeout_add (20, (GSourceFunc) e_book_view_listener_check_queue, listener);

		/* Hold a reference to the listener on behalf of the timeout */
		bonobo_object_ref (BONOBO_OBJECT (listener));
	}
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_status_event (EBookViewListener          *listener,
					 EBookViewListenerOperation  op,
					 EBookViewStatus             status)
{
	EBookViewListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = status;
	resp->id        = NULL;
	resp->cards     = NULL;
	resp->message   = NULL;

	e_book_view_listener_queue_response (listener, resp);
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_id_event (EBookViewListener          *listener,
				     EBookViewListenerOperation  op,
				     const char             *id)
{
	EBookViewListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = E_BOOK_VIEW_STATUS_SUCCESS;
	resp->id        = g_strdup (id);
	resp->cards     = NULL;
	resp->message   = NULL;

	e_book_view_listener_queue_response (listener, resp);
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_sequence_event (EBookViewListener          *listener,
					   EBookViewListenerOperation  op,
					   const GNOME_Evolution_Addressbook_VCardList  *cards)
{
	EBookViewListenerResponse *resp;
	int i;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = E_BOOK_VIEW_STATUS_SUCCESS;
	resp->id        = NULL;
	resp->cards     = NULL;
	resp->message   = NULL;
	
	for ( i = 0; i < cards->_length; i++ ) {
		resp->cards = g_list_append(resp->cards, e_card_new(cards->_buffer[i]));
	}

	e_book_view_listener_queue_response (listener, resp);
}

/* Status Message */
static void
e_book_view_listener_queue_message_event (EBookViewListener          *listener,
					  EBookViewListenerOperation  op,
					  const char                 *message)
{
	EBookViewListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = E_BOOK_VIEW_STATUS_SUCCESS;
	resp->id        = NULL;
	resp->cards     = NULL;
	resp->message   = g_strdup(message);

	e_book_view_listener_queue_response (listener, resp);
}

static void
impl_BookViewListener_notify_card_added (PortableServer_Servant servant,
					 const GNOME_Evolution_Addressbook_VCardList *cards,
					 CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_sequence_event (
		listener, CardAddedEvent, cards);
}

static void
impl_BookViewListener_notify_card_removed (PortableServer_Servant servant,
					   const CORBA_char* id,
					   CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_id_event (
		listener, CardRemovedEvent, (const char *) id);
}

static void
impl_BookViewListener_notify_card_changed (PortableServer_Servant servant,
					   const GNOME_Evolution_Addressbook_VCardList *cards,
					   CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_sequence_event (
		listener, CardModifiedEvent, cards);
}

static void
impl_BookViewListener_notify_sequence_complete (PortableServer_Servant servant,
						const GNOME_Evolution_Addressbook_BookViewListener_CallStatus status,
						CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_status_event (listener, SequenceCompleteEvent,
						 e_book_view_listener_convert_status (status));
}

static void
impl_BookViewListener_notify_status_message (PortableServer_Servant  servant,
					     const char             *message,
					     CORBA_Environment      *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_message_event (listener, StatusMessageEvent, message);
}

/**
 * e_book_view_listener_check_pending:
 * @listener: the #EBookViewListener 
 *
 * Returns: the number of items on the response queue,
 * or -1 if the @listener is isn't an #EBookViewListener.
 */
int
e_book_view_listener_check_pending (EBookViewListener *listener)
{
	g_return_val_if_fail (listener != NULL,              -1);
	g_return_val_if_fail (E_IS_BOOK_VIEW_LISTENER (listener), -1);

	return g_list_length (listener->priv->response_queue);
}

/**
 * e_book_view_listener_pop_response:
 * @listener: the #EBookViewListener for which a request is to be popped
 *
 * Returns: an #EBookViewListenerResponse if there are responses on the
 * queue to be returned; %NULL if there aren't, or if the @listener
 * isn't an EBookViewListener.
 */
EBookViewListenerResponse *
e_book_view_listener_pop_response (EBookViewListener *listener)
{
	EBookViewListenerResponse *resp;
	GList                 *popped;

	g_return_val_if_fail (listener != NULL,              NULL);
	g_return_val_if_fail (E_IS_BOOK_VIEW_LISTENER (listener), NULL);

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

static EBookViewStatus
e_book_view_listener_convert_status (const GNOME_Evolution_Addressbook_BookViewListener_CallStatus status)
{
	switch (status) {
	case GNOME_Evolution_Addressbook_BookViewListener_Success:
		return E_BOOK_VIEW_STATUS_SUCCESS;
	case GNOME_Evolution_Addressbook_BookViewListener_SearchTimeLimitExceeded:
		return E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED;
	case GNOME_Evolution_Addressbook_BookViewListener_SearchSizeLimitExceeded:
		return E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED;
	case GNOME_Evolution_Addressbook_BookViewListener_InvalidQuery:
		return E_BOOK_VIEW_STATUS_INVALID_QUERY;
	case GNOME_Evolution_Addressbook_BookViewListener_QueryRefused:
		return E_BOOK_VIEW_STATUS_QUERY_REFUSED;
	case GNOME_Evolution_Addressbook_BookViewListener_OtherError:
		return E_BOOK_VIEW_STATUS_OTHER_ERROR;
	default:
		g_warning ("e_book_view_listener_convert_status: Unknown status "
			   "from card server: %d\n", (int) status);
		return E_BOOK_VIEW_STATUS_UNKNOWN;

	}
}

void
e_book_view_listener_construct      (EBookViewListener *listener,
				     GNOME_Evolution_Addressbook_BookViewListener corba_objref)
{
	EBookViewListenerPrivate *priv;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (corba_objref != CORBA_OBJECT_NIL);

	priv = listener->priv;

	g_return_if_fail (priv->corba_objref == CORBA_OBJECT_NIL);

	priv->corba_objref = corba_objref;
}

static EBookViewListenerServant *
create_servant (EBookViewListener *listener)
{
	EBookViewListenerServant *servant;
	POA_GNOME_Evolution_Addressbook_BookViewListener *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (EBookViewListenerServant, 1);
	corba_servant = (POA_GNOME_Evolution_Addressbook_BookViewListener *) servant;

	corba_servant->vepv = &e_book_view_listener_vepv;
	POA_GNOME_Evolution_Addressbook_BookViewListener__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->object = listener;

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_BookViewListener
activate_servant (EBookViewListener *listener,
		  POA_GNOME_Evolution_Addressbook_BookViewListener *servant)
{
	GNOME_Evolution_Addressbook_BookViewListener corba_object;
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
 * e_book_view_listener_new:
 * @book: the #EBookView for which the listener is to be bound
 *
 * Creates and returns a new #EBookViewListener for the book.
 *
 * Returns: a new #EBookViewListener
 */
EBookViewListener *
e_book_view_listener_new ()
{
	EBookViewListener *listener;
	EBookViewListenerPrivate *priv;
	GNOME_Evolution_Addressbook_BookViewListener corba_objref;

	listener = g_object_new (E_TYPE_BOOK_VIEW_LISTENER, NULL);
	priv = listener->priv;

	priv->servant = create_servant (listener);
	corba_objref = activate_servant (listener, (POA_GNOME_Evolution_Addressbook_BookViewListener *) priv->servant);

	e_book_view_listener_construct (listener, corba_objref);

	return listener;
}

static void
e_book_view_listener_init (EBookViewListener *listener)
{
	listener->priv                 = g_new0 (EBookViewListenerPrivate, 1);
	listener->priv->corba_objref   = CORBA_OBJECT_NIL;
	listener->priv->response_queue = NULL;
	listener->priv->timeout_id     = 0;
	listener->priv->timeout_lock   = FALSE;
	listener->priv->stopped        = FALSE;
}

void
e_book_view_listener_stop (EBookViewListener *listener)
{
	g_return_if_fail (E_IS_BOOK_VIEW_LISTENER (listener));
	listener->priv->stopped = TRUE;
}

static void
e_book_view_listener_dispose (GObject *object)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (object);
	GList *l;
	
	/* Remove our response queue handler: In theory, this can never happen since we
	 always hold a reference to the listener while the timeout is running. */
	if (listener->priv->timeout_id) {
		g_source_remove (listener->priv->timeout_id);
	}

	/* Clear out the queue */
	for (l = listener->priv->response_queue; l != NULL; l = l->next) {
		EBookViewListenerResponse *resp = l->data;

		g_free(resp->id);

		g_list_foreach(resp->cards, (GFunc) g_object_unref, NULL);
		g_list_free(resp->cards);
		resp->cards = NULL;

		g_free (resp->message);
		resp->message = NULL;

		g_free (resp);
	}
	g_list_free (listener->priv->response_queue);
	
	g_free (listener->priv);
	listener->priv = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
corba_class_init (void)
{
	POA_GNOME_Evolution_Addressbook_BookViewListener__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_BookViewListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;


	epv = g_new0 (POA_GNOME_Evolution_Addressbook_BookViewListener__epv, 1);
	epv->notifyCardChanged      = impl_BookViewListener_notify_card_changed;
	epv->notifyCardRemoved      = impl_BookViewListener_notify_card_removed;
	epv->notifyCardAdded        = impl_BookViewListener_notify_card_added;
	epv->notifySequenceComplete = impl_BookViewListener_notify_sequence_complete;
	epv->notifyStatusMessage    = impl_BookViewListener_notify_status_message;

	vepv = &e_book_view_listener_vepv;
	vepv->_base_epv                                    = base_epv;
	vepv->GNOME_Evolution_Addressbook_BookViewListener_epv = epv;
}

static void
e_book_view_listener_class_init (EBookViewListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	e_book_view_listener_signals [RESPONSES_QUEUED] =
		g_signal_new ("responses_queued",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookViewListenerClass, responses_queued),
			      NULL, NULL,
			      e_book_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	object_class->dispose = e_book_view_listener_dispose;

	corba_class_init ();
}

/**
 * e_book_view_listener_get_type:
 */
GType
e_book_view_listener_get_type (void)
{
	static GType type = 0;

	if (! type) {
		GTypeInfo info = {
			sizeof (EBookViewListenerClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)  e_book_view_listener_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EBookViewListener),
			0,    /* n_preallocs */
			(GInstanceInitFunc) e_book_view_listener_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "EBookViewListener", &info, 0);
	}

	return type;
}

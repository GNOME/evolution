/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Exports the BookViewListener interface.  Maintains a queue of messages
 * which come in on the interface.
 *
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "e-book-view-listener.h"
#include "e-book-view.h"
#include "e-card.h"

enum {
	RESPONSES_QUEUED,
	LAST_SIGNAL
};

static guint e_book_view_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *e_book_view_listener_parent_class;
POA_GNOME_Evolution_Addressbook_BookViewListener__vepv  e_book_view_listener_vepv;

struct _EBookViewListenerPrivate {
	GList *response_queue;
	gint   idle_id;

	guint stopped : 1;
};

static gboolean
e_book_view_listener_check_queue (EBookViewListener *listener)
{
	static gint thrash = 0;
	gint queue_len;

	if (listener->priv->stopped) {
		listener->priv->idle_id = 0;
		return TRUE;
	}

	queue_len = g_list_length (listener->priv->response_queue);

	bonobo_object_ref (BONOBO_OBJECT (listener));
	if (listener->priv->response_queue != NULL) {
		gtk_signal_emit (GTK_OBJECT (listener),
				 e_book_view_listener_signals [RESPONSES_QUEUED]);
	}

	/* This means we didn't make any progress in dealing with what is on our
	   response queue. */
	if (queue_len == g_list_length (listener->priv->response_queue))
		++thrash;
	else
		thrash = 0;

	if (thrash > 20) {
		g_error ("e_book_view_listener_check_queue thrashing!");
		thrash = 0;
		listener->priv->idle_id = 0;
		return FALSE;
	}

	if (listener->priv->response_queue == NULL) {
		listener->priv->idle_id = 0;
		bonobo_object_unref (BONOBO_OBJECT (listener));
		return FALSE;
	}
	bonobo_object_unref (BONOBO_OBJECT (listener));

	return TRUE;
}

static void
e_book_view_listener_queue_response (EBookViewListener         *listener,
				     EBookViewListenerResponse *response)
{
	listener->priv->response_queue =
		g_list_append (listener->priv->response_queue,
			       response);

	if (listener->priv->idle_id == 0) {
		listener->priv->idle_id = g_idle_add (
			(GSourceFunc) e_book_view_listener_check_queue, listener);
	}
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_empty_event (EBookViewListener          *listener,
					EBookViewListenerOperation  op)
{
	EBookViewListenerResponse *resp;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
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
					   const GNOME_Evolution_Addressbook_CardId id,
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
						CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object_from_servant (servant));

	e_book_view_listener_queue_empty_event (listener, SequenceCompleteEvent);
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

static EBookViewListener *
e_book_view_listener_construct (EBookViewListener *listener)
{
	POA_GNOME_Evolution_Addressbook_BookViewListener *servant;
	CORBA_Environment           ev;
	CORBA_Object                obj;

	g_assert (listener != NULL);
	g_assert (E_IS_BOOK_VIEW_LISTENER (listener));

	servant = (POA_GNOME_Evolution_Addressbook_BookViewListener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &e_book_view_listener_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Addressbook_BookViewListener__init ((PortableServer_Servant) servant, &ev);
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
	EBookViewListener *retval;

	listener = gtk_type_new (E_BOOK_VIEW_LISTENER_TYPE);

	retval = e_book_view_listener_construct (listener);

	if (retval == NULL) {
		g_warning ("e_book_view_listener_new: Error constructing "
			   "EBookViewListener!\n");
		gtk_object_unref (GTK_OBJECT (listener));
		return NULL;
	}

	return retval;
}

static void
e_book_view_listener_init (EBookViewListener *listener)
{
	listener->priv                 = g_new0 (EBookViewListenerPrivate, 1);
	listener->priv->response_queue = NULL;
	listener->priv->idle_id        = 0;
	listener->priv->stopped        = FALSE;
}

void
e_book_view_listener_stop (EBookViewListener *listener)
{
	GList *l;

	if (listener->priv->stopped)
		return;

	if (listener->priv->idle_id) {
		g_source_remove(listener->priv->idle_id);
		listener->priv->idle_id = 0;
	}

	for (l = listener->priv->response_queue; l != NULL; l = l->next) {
		EBookViewListenerResponse *resp = l->data;

		g_free(resp->id);

		g_list_foreach(resp->cards, (GFunc) gtk_object_unref, NULL);
		g_list_free(resp->cards);

		g_free (resp->message);

		g_free (resp);
	}
	g_list_free (listener->priv->response_queue);
	listener->priv->response_queue = NULL;

	listener->priv->stopped = TRUE;
}

static void
e_book_view_listener_destroy (GtkObject *object)
{
	EBookViewListener     *listener = E_BOOK_VIEW_LISTENER (object);

	e_book_view_listener_stop (listener);
	g_free (listener->priv);
	
	GTK_OBJECT_CLASS (e_book_view_listener_parent_class)->destroy (object);
}

POA_GNOME_Evolution_Addressbook_BookViewListener__epv *
e_book_view_listener_get_epv (void)
{
	POA_GNOME_Evolution_Addressbook_BookViewListener__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Addressbook_BookViewListener__epv, 1);

	epv->notifyCardChanged      = impl_BookViewListener_notify_card_changed;
	epv->notifyCardRemoved      = impl_BookViewListener_notify_card_removed;
	epv->notifyCardAdded        = impl_BookViewListener_notify_card_added;
	epv->notifySequenceComplete = impl_BookViewListener_notify_sequence_complete;
	epv->notifyStatusMessage    = impl_BookViewListener_notify_status_message;

	return epv;
}

static void
e_book_view_listener_corba_class_init (void)
{
	e_book_view_listener_vepv.Bonobo_Unknown_epv          = bonobo_object_get_epv ();
	e_book_view_listener_vepv.GNOME_Evolution_Addressbook_BookViewListener_epv = e_book_view_listener_get_epv ();
}

static void
e_book_view_listener_class_init (EBookViewListenerClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	e_book_view_listener_parent_class = gtk_type_class (bonobo_object_get_type ());

	e_book_view_listener_signals [RESPONSES_QUEUED] =
		gtk_signal_new ("responses_queued",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EBookViewListenerClass, responses_queued),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_book_view_listener_signals, LAST_SIGNAL);

	object_class->destroy = e_book_view_listener_destroy;

	e_book_view_listener_corba_class_init ();
}

/**
 * e_book_view_listener_get_type:
 */
GtkType
e_book_view_listener_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"EBookViewListener",
			sizeof (EBookViewListener),
			sizeof (EBookViewListenerClass),
			(GtkClassInitFunc)  e_book_view_listener_class_init,
			(GtkObjectInitFunc) e_book_view_listener_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}

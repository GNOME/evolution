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
#include "e-contact.h"
#include "e-book-marshal.h"

static EBookViewStatus e_book_view_listener_convert_status (GNOME_Evolution_Addressbook_CallStatus status);

enum {
	RESPONSE,
	LAST_SIGNAL
};

static guint e_book_view_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *parent_class;

struct _EBookViewListenerPrivate {
	guint stopped      : 1;
};

static void
e_book_view_listener_queue_response (EBookViewListener         *listener,
				     EBookViewListenerResponse *response)
{
	if (response == NULL)
		return;

	if (listener->priv->stopped) {
		/* Free response and return */
		g_list_foreach (response->ids, (GFunc)g_free, NULL);
		g_list_free (response->ids);
		g_list_foreach (response->contacts, (GFunc) g_object_unref, NULL);
		g_list_free (response->contacts);
		g_free (response->message);
		g_free (response);
		return;
	}

	g_signal_emit (listener, e_book_view_listener_signals [RESPONSE], 0, response);
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

	e_book_view_listener_queue_response (listener, resp);
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_idlist_event (EBookViewListener          *listener,
					 EBookViewListenerOperation  op,
					 const GNOME_Evolution_Addressbook_ContactIdList *ids)
{
	EBookViewListenerResponse *resp;
	int i;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = E_BOOK_VIEW_STATUS_OK;

	for (i = 0; i < ids->_length; i ++) {
		resp->ids = g_list_prepend (resp->ids, g_strdup (ids->_buffer[i]));
	}

	e_book_view_listener_queue_response (listener, resp);
}

/* Add, Remove, Modify */
static void
e_book_view_listener_queue_sequence_event (EBookViewListener          *listener,
					   EBookViewListenerOperation  op,
					   const GNOME_Evolution_Addressbook_VCardList  *vcards)
{
	EBookViewListenerResponse *resp;
	int i;

	if (listener->priv->stopped)
		return;

	resp = g_new0 (EBookViewListenerResponse, 1);

	resp->op        = op;
	resp->status    = E_BOOK_VIEW_STATUS_OK;
	
	for ( i = 0; i < vcards->_length; i++ ) {
		resp->contacts = g_list_append(resp->contacts, e_contact_new_from_vcard (vcards->_buffer[i]));
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
	resp->status    = E_BOOK_VIEW_STATUS_OK;
	resp->message   = g_strdup(message);

	e_book_view_listener_queue_response (listener, resp);
}

static void
impl_BookViewListener_notify_contacts_added (PortableServer_Servant servant,
					     const GNOME_Evolution_Addressbook_VCardList *vcards,
					     CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object (servant));

	printf ("impl_BookViewListener_notify_contacts_added\n");

	e_book_view_listener_queue_sequence_event (
		listener, ContactsAddedEvent, vcards);
}

static void
impl_BookViewListener_notify_contacts_removed (PortableServer_Servant servant,
					       const GNOME_Evolution_Addressbook_ContactIdList *ids,
					       CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object (servant));

	printf ("impl_BookViewListener_notify_contacts_removed\n");

	e_book_view_listener_queue_idlist_event (listener, ContactsRemovedEvent, ids);
}

static void
impl_BookViewListener_notify_contacts_changed (PortableServer_Servant servant,
					       const GNOME_Evolution_Addressbook_VCardList *vcards,
					       CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object (servant));

	printf ("impl_BookViewListener_notify_contacts_changed\n");

	e_book_view_listener_queue_sequence_event (
		listener, ContactsModifiedEvent, vcards);
}

static void
impl_BookViewListener_notify_sequence_complete (PortableServer_Servant servant,
						const GNOME_Evolution_Addressbook_CallStatus status,
						CORBA_Environment *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object (servant));

	printf ("impl_BookViewListener_notify_sequence_complete\n");

	e_book_view_listener_queue_status_event (listener, SequenceCompleteEvent,
						 e_book_view_listener_convert_status (status));
}

static void
impl_BookViewListener_notify_progress (PortableServer_Servant  servant,
				       const char             *message,
				       const CORBA_short       percent,
				       CORBA_Environment      *ev)
{
	EBookViewListener *listener = E_BOOK_VIEW_LISTENER (bonobo_object (servant));

	printf ("impl_BookViewListener_notify_progress\n");

	e_book_view_listener_queue_message_event (listener, StatusMessageEvent, message);
}

static EBookViewStatus
e_book_view_listener_convert_status (const GNOME_Evolution_Addressbook_CallStatus status)
{
	switch (status) {
	case GNOME_Evolution_Addressbook_Success:
		return E_BOOK_VIEW_STATUS_OK;
	case GNOME_Evolution_Addressbook_SearchTimeLimitExceeded:
		return E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED;
	case GNOME_Evolution_Addressbook_SearchSizeLimitExceeded:
		return E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED;
	case GNOME_Evolution_Addressbook_InvalidQuery:
		return E_BOOK_VIEW_ERROR_INVALID_QUERY;
	case GNOME_Evolution_Addressbook_QueryRefused:
		return E_BOOK_VIEW_ERROR_QUERY_REFUSED;
	case GNOME_Evolution_Addressbook_OtherError:
	default:
		return E_BOOK_VIEW_ERROR_OTHER_ERROR;
	}
}

static void
e_book_view_listener_construct      (EBookViewListener *listener)
{
	/* nothing needed here */
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

	listener = g_object_new (E_TYPE_BOOK_VIEW_LISTENER,
				 "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_ALL_AT_IDLE, NULL),
				 NULL);

	e_book_view_listener_construct (listener);

	return listener;
}

static void
e_book_view_listener_init (EBookViewListener *listener)
{
	listener->priv                 = g_new0 (EBookViewListenerPrivate, 1);
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

	if (listener->priv) {
		g_free (listener->priv);
		listener->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_book_view_listener_class_init (EBookViewListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Evolution_Addressbook_BookViewListener__epv *epv;

	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	e_book_view_listener_signals [RESPONSE] =
		g_signal_new ("response",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookViewListenerClass, response),
			      NULL, NULL,
			      e_book_marshal_NONE__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	object_class->dispose = e_book_view_listener_dispose;

	epv = &klass->epv;
	epv->notifyContactsChanged  = impl_BookViewListener_notify_contacts_changed;
	epv->notifyContactsRemoved  = impl_BookViewListener_notify_contacts_removed;
	epv->notifyContactsAdded    = impl_BookViewListener_notify_contacts_added;
	epv->notifySequenceComplete = impl_BookViewListener_notify_sequence_complete;
	epv->notifyProgress         = impl_BookViewListener_notify_progress;
}

BONOBO_TYPE_FUNC_FULL (
		       EBookViewListener,
		       GNOME_Evolution_Addressbook_BookViewListener,
		       BONOBO_TYPE_OBJECT,
		       e_book_view_listener);

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-book.c
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include "pas-book.h"

static BonoboObjectClass *pas_book_parent_class;
POA_Evolution_Book__vepv pas_book_vepv;

enum {
	REQUESTS_QUEUED,
	LAST_SIGNAL
};

static guint pas_book_signals [LAST_SIGNAL];

struct _PASBookPrivate {
	PASBackend             *backend;
	Evolution_BookListener  listener;
	PASBookGetVCardFn       get_vcard;
	PASBookCanWriteFn       can_write;
	PASBookCanWriteCardFn   can_write_card;

	GList                  *request_queue;
	gint                    idle_id;
};

static gboolean
pas_book_check_queue (PASBook *book)
{
	if (book->priv->request_queue != NULL) {
		gtk_signal_emit (GTK_OBJECT (book),
				 pas_book_signals [REQUESTS_QUEUED]);
	}

	if (book->priv->request_queue == NULL) {
		book->priv->idle_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
pas_book_queue_request (PASBook *book, PASRequest *req)
{
	book->priv->request_queue =
		g_list_append (book->priv->request_queue, req);

	if (book->priv->idle_id == 0) {
		book->priv->idle_id = g_idle_add ((GSourceFunc) pas_book_check_queue, book);
	}
}

static void
pas_book_queue_create_card (PASBook *book, const char *vcard)
{
	PASRequest *req;

	req        = g_new0 (PASRequest, 1);
	req->op    = CreateCard;
	req->vcard = g_strdup (vcard);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_remove_card (PASBook *book, const char *id)
{
	PASRequest *req;

	req     = g_new0 (PASRequest, 1);
	req->op = RemoveCard;
	req->id = g_strdup (id);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_modify_card (PASBook *book, const char *vcard)
{
	PASRequest *req;

	req        = g_new0 (PASRequest, 1);
	req->op    = ModifyCard;
	req->vcard = g_strdup (vcard);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_cursor (PASBook *book, const char *search)
{
	PASRequest *req;

	req         = g_new0 (PASRequest, 1);
	req->op     = GetCursor;
	req->search = g_strdup(search);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_book_view (PASBook *book, const Evolution_BookViewListener listener, const char *search)
{
	PASRequest *req;
	CORBA_Environment ev;

	req           = g_new0 (PASRequest, 1);
	req->op       = GetBookView;
	req->search   = g_strdup(search);
	
	CORBA_exception_init (&ev);

	req->listener = CORBA_Object_duplicate(listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_queue_get_book_view: Exception "
			   "duplicating BookViewListener!\n");
	}

	CORBA_exception_free (&ev);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_check_connection (PASBook *book)
{
	PASRequest *req;

	req        = g_new0 (PASRequest, 1);
	req->op    = CheckConnection;

	pas_book_queue_request (book, req);
}

static CORBA_char *
impl_Evolution_Book_get_vcard (PortableServer_Servant servant,
			       const Evolution_CardId id,
			       CORBA_Environment *ev)
{
	PASBook    *book = PAS_BOOK (bonobo_object_from_servant (servant));
	char       *vcard;
	CORBA_char *retval;

	vcard = (book->priv->get_vcard) (book, (const char *) id);
	retval = CORBA_string_dup (vcard);
	g_free (vcard);

	return retval;
}

static CORBA_boolean
impl_Evolution_Book_can_write (PortableServer_Servant servant,
			       CORBA_Environment *ev)
{
	PASBook    *book = PAS_BOOK (bonobo_object_from_servant (servant));
	CORBA_boolean retval;

	retval = (book->priv->can_write) (book);

	return retval;
}

static CORBA_boolean
impl_Evolution_Book_can_write_card (PortableServer_Servant servant,
				    const Evolution_CardId id,
				    CORBA_Environment *ev)
{
	PASBook    *book = PAS_BOOK (bonobo_object_from_servant (servant));
	CORBA_boolean retval;

	retval = (book->priv->can_write_card) (book, (const char *) id);

	return retval;
}

static void
impl_Evolution_Book_create_card (PortableServer_Servant servant,
				 const Evolution_VCard vcard,
				 CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_create_card (book, (const char *) vcard);
}

static void
impl_Evolution_Book_remove_card (PortableServer_Servant servant,
				 const Evolution_CardId id,
				 CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_remove_card (book, (const char *) id);
}

static void
impl_Evolution_Book_modify_card (PortableServer_Servant servant,
				 const Evolution_VCard vcard,
				 CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_modify_card (book, (const char *) vcard);
}

static void
impl_Evolution_Book_get_cursor (PortableServer_Servant servant,
				const CORBA_char *search,
				CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_cursor (book, search);
}

static void
impl_Evolution_Book_get_book_view (PortableServer_Servant servant,
				   const Evolution_BookViewListener listener,
				   const CORBA_char *search,
				   CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_book_view (book, listener, search);
}

static void
impl_Evolution_Book_check_connection (PortableServer_Servant servant,
				      CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_check_connection (book);
}

/**
 * pas_book_get_backend:
 */
PASBackend *
pas_book_get_backend (PASBook *book)
{
	g_return_val_if_fail (book != NULL,       NULL);
	g_return_val_if_fail (PAS_IS_BOOK (book), NULL);

	return book->priv->backend;
}

/**
 * pas_book_get_listener:
 */
Evolution_BookListener
pas_book_get_listener (PASBook *book)
{
	g_return_val_if_fail (book != NULL,       CORBA_OBJECT_NIL);
	g_return_val_if_fail (PAS_IS_BOOK (book), CORBA_OBJECT_NIL);

	return book->priv->listener;
}

/**
 * pas_book_check_pending
 */
gint
pas_book_check_pending (PASBook *book)
{
	g_return_val_if_fail (book != NULL,       -1);
	g_return_val_if_fail (PAS_IS_BOOK (book), -1);

	return g_list_length (book->priv->request_queue);
}

/**
 * pas_book_pop_request:
 */
PASRequest *
pas_book_pop_request (PASBook *book)
{
	GList      *popped;
	PASRequest *req;

	g_return_val_if_fail (book != NULL,       NULL);
	g_return_val_if_fail (PAS_IS_BOOK (book), NULL);

	if (book->priv->request_queue == NULL)
		return NULL;

	req = book->priv->request_queue->data;

	popped = book->priv->request_queue;
	book->priv->request_queue =
		g_list_remove_link (book->priv->request_queue, popped);

	g_list_free_1 (popped);

	return req;
}

/**
 * pas_book_respond_open:
 */
void
pas_book_respond_open (PASBook                           *book,
		       Evolution_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (status == Evolution_BookListener_Success) {
		Evolution_BookListener_respond_open_book (
			book->priv->listener, status,
			bonobo_object_corba_objref (BONOBO_OBJECT (book)),
			&ev);
	} else {
		Evolution_BookListener_respond_open_book (
			book->priv->listener, status,
			CORBA_OBJECT_NIL, &ev);
	}
	

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_open: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_create:
 */
void
pas_book_respond_create (PASBook                           *book,
			 Evolution_BookListener_CallStatus  status,
			 const char                        *id)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Evolution_BookListener_respond_create_card (
		book->priv->listener, status, (char *)id, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_create: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_remove:
 */
void
pas_book_respond_remove (PASBook                           *book,
			 Evolution_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Evolution_BookListener_respond_remove_card (
		book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_remove: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_modify:
 */
void
pas_book_respond_modify (PASBook                           *book,
			 Evolution_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Evolution_BookListener_respond_modify_card (
		book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_modify: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_cursor:
 */
void
pas_book_respond_get_cursor (PASBook                           *book,
			     Evolution_BookListener_CallStatus  status,
			     PASCardCursor                     *cursor)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(cursor));

	Evolution_BookListener_respond_get_cursor (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_cursor: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_cursor:
 */
void
pas_book_respond_get_book_view (PASBook                           *book,
				Evolution_BookListener_CallStatus  status,
				PASBookView                       *book_view)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(book_view));

	Evolution_BookListener_respond_get_view (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_book_view: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_report_connection:
 */
void
pas_book_report_connection (PASBook  *book,
			    gboolean  connected)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Evolution_BookListener_report_connection_status (
		book->priv->listener, (CORBA_boolean) connected, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_report_connection: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

static gboolean
pas_book_construct (PASBook                *book,
		    PASBackend             *backend,
		    Evolution_BookListener  listener,
		    PASBookGetVCardFn       get_vcard,
		    PASBookCanWriteFn       can_write,
		    PASBookCanWriteCardFn   can_write_card)
{
	POA_Evolution_Book *servant;
	CORBA_Environment   ev;
	CORBA_Object        obj;

	g_assert (book      != NULL);
	g_assert (PAS_IS_BOOK (book));
	g_assert (listener  != CORBA_OBJECT_NIL);
	g_assert (get_vcard != NULL);
	g_assert (can_write != NULL);
	g_assert (can_write_card != NULL);

	servant = (POA_Evolution_Book *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &pas_book_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Book__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);

		return FALSE;
	}

	CORBA_exception_free (&ev);

	obj = bonobo_object_activate_servant (BONOBO_OBJECT (book), servant);
	if (obj == CORBA_OBJECT_NIL) {
		g_free (servant);

		return FALSE;
	}

	bonobo_object_construct (BONOBO_OBJECT (book), obj);

	CORBA_exception_init (&ev);
	book->priv->listener = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("pas_book_construct(): could not duplicate the listener");

	CORBA_exception_free (&ev);

	book->priv->listener  = listener;
	book->priv->get_vcard = get_vcard;
	book->priv->can_write = can_write;
	book->priv->can_write_card = can_write_card;
	book->priv->backend   = backend;

	return TRUE;
}

/**
 * pas_book_new:
 */
PASBook *
pas_book_new (PASBackend             *backend,
	      Evolution_BookListener  listener,
	      PASBookGetVCardFn       get_vcard,
	      PASBookCanWriteFn       can_write,
	      PASBookCanWriteCardFn   can_write_card)
{
	PASBook *book;

	g_return_val_if_fail (listener  != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (get_vcard != NULL,             NULL);

	book = gtk_type_new (pas_book_get_type ());

	if (! pas_book_construct (book, backend, listener,
				  get_vcard, can_write, can_write_card)) {
		gtk_object_unref (GTK_OBJECT (book));

		return NULL;
	}

	return book;
}

static void
pas_book_destroy (GtkObject *object)
{
	PASBook *book = PAS_BOOK (object);
	GList   *l;
	CORBA_Environment ev;

	for (l = book->priv->request_queue; l != NULL; l = l->next) {
		PASRequest *req = l->data;

		g_free (req->id);
		g_free (req->vcard);
		g_free (req);
	}
	g_list_free (book->priv->request_queue);

	CORBA_exception_init (&ev);
	CORBA_Object_release (book->priv->listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("pas_book_construct(): could not release the listener");

	CORBA_exception_free (&ev);

	g_free (book->priv);

	GTK_OBJECT_CLASS (pas_book_parent_class)->destroy (object);	
}

static POA_Evolution_Book__epv *
pas_book_get_epv (void)
{
	POA_Evolution_Book__epv *epv;

	epv = g_new0 (POA_Evolution_Book__epv, 1);

	epv->get_vcard        = impl_Evolution_Book_get_vcard;
	epv->can_write        = impl_Evolution_Book_can_write;
	epv->can_write_card   = impl_Evolution_Book_can_write_card;
	epv->create_card      = impl_Evolution_Book_create_card;
	epv->remove_card      = impl_Evolution_Book_remove_card;
	epv->modify_card      = impl_Evolution_Book_modify_card;
	epv->check_connection = impl_Evolution_Book_check_connection;
	epv->get_cursor       = impl_Evolution_Book_get_cursor;
	epv->get_book_view    = impl_Evolution_Book_get_book_view;

	return epv;
	
}

static void
pas_book_corba_class_init (void)
{
	pas_book_vepv.Bonobo_Unknown_epv  = bonobo_object_get_epv ();
	pas_book_vepv.Evolution_Book_epv = pas_book_get_epv ();
}

static void
pas_book_class_init (PASBookClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	pas_book_parent_class = gtk_type_class (bonobo_object_get_type ());

	pas_book_signals [REQUESTS_QUEUED] =
		gtk_signal_new ("requests_queued",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (PASBookClass, requests_queued),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, pas_book_signals, LAST_SIGNAL);

	object_class->destroy = pas_book_destroy;

	pas_book_corba_class_init ();
}

static void
pas_book_init (PASBook *book)
{
	book->priv                = g_new0 (PASBookPrivate, 1);
	book->priv->idle_id       = 0;
	book->priv->request_queue = NULL;
}

/**
 * pas_book_get_type:
 */
GtkType
pas_book_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBook",
			sizeof (PASBook),
			sizeof (PASBookClass),
			(GtkClassInitFunc)  pas_book_class_init,
			(GtkObjectInitFunc) pas_book_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
	}

	return type;
}


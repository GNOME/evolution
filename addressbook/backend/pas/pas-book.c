/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-book.c
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include <bonobo/bonobo-main.h>
#include "e-util/e-list.h"
#include "pas-backend.h"
#include "pas-marshal.h"

static BonoboObjectClass *pas_book_parent_class;
POA_GNOME_Evolution_Addressbook_Book__vepv pas_book_vepv;

enum {
	REQUESTS_QUEUED,
	LAST_SIGNAL
};

static guint pas_book_signals [LAST_SIGNAL];

struct _PASBookPrivate {
	PASBookServant *servant;
	GNOME_Evolution_Addressbook_Book   corba_objref;

	PASBackend             *backend;
	GNOME_Evolution_Addressbook_BookListener  listener;

	GList                  *request_queue;
	gint                    timeout_id;
	
	guint                   timeout_lock : 1;
};

static gboolean
pas_book_check_queue (PASBook *book)
{
	if (book->priv->timeout_lock)
		return TRUE;

	book->priv->timeout_lock = TRUE;

	if (book->priv->request_queue != NULL) {
		g_signal_emit (book, pas_book_signals [REQUESTS_QUEUED], 0);
	}

	if (book->priv->request_queue == NULL) {
		book->priv->timeout_id = 0;
		book->priv->timeout_lock = FALSE;
		bonobo_object_unref (BONOBO_OBJECT (book));
		return FALSE;
	}

	book->priv->timeout_lock = FALSE;

	return TRUE;
}

static void
pas_book_queue_request (PASBook *book, PASRequest *req)
{
	book->priv->request_queue =
		g_list_append (book->priv->request_queue, req);

	if (book->priv->timeout_id == 0) {
		bonobo_object_ref (BONOBO_OBJECT (book));
		book->priv->timeout_id = g_timeout_add (20, (GSourceFunc) pas_book_check_queue, book);
	}
}

static void
pas_book_queue_create_card (PASBook *book, const char *vcard)
{
	PASRequest *req;

	req        = g_new0 (PASRequest, 1);
	req->op    = CreateCard;
	req->create.vcard = g_strdup (vcard);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_remove_card (PASBook *book, const char *id)
{
	PASRequest *req;

	req     = g_new0 (PASRequest, 1);
	req->op = RemoveCard;
	req->remove.id = g_strdup (id);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_modify_card (PASBook *book, const char *vcard)
{
	PASRequest *req;

	req        = g_new0 (PASRequest, 1);
	req->op    = ModifyCard;
	req->modify.vcard = g_strdup (vcard);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_cursor (PASBook *book, const char *search)
{
	PASRequest *req;

	req         = g_new0 (PASRequest, 1);
	req->op     = GetCursor;
	req->get_cursor.search = g_strdup(search);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_vcard (PASBook *book, const char *id)
{
	PASRequest *req;

	req     = g_new0 (PASRequest, 1);
	req->op = GetVCard;
	req->get_vcard.id = g_strdup(id);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_authenticate_user (PASBook *book,
				  const char *user, const char *passwd, const char *auth_method)
{
	PASRequest *req;

	req         = g_new0 (PASRequest, 1);
	req->op     = AuthenticateUser;
	req->auth_user.user   = g_strdup(user);
	req->auth_user.passwd = g_strdup(passwd);
	req->auth_user.auth_method = g_strdup(auth_method);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_supported_fields (PASBook *book)
{
	PASRequest *req;

	req     = g_new0 (PASRequest, 1);
	req->op = GetSupportedFields;

	pas_book_queue_request (book, req);
}


static void
pas_book_queue_get_book_view (PASBook *book, const GNOME_Evolution_Addressbook_BookViewListener listener, const char *search)
{
	PASRequest *req;
	CORBA_Environment ev;

	req           = g_new0 (PASRequest, 1);
	req->op       = GetBookView;
	req->get_book_view.search   = g_strdup(search);
	
	CORBA_exception_init (&ev);

	req->get_book_view.listener = bonobo_object_dup_ref(listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_queue_get_book_view: Exception "
			   "duplicating BookViewListener!\n");
	}

	CORBA_exception_free (&ev);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_completion_view (PASBook *book, const GNOME_Evolution_Addressbook_BookViewListener listener, const char *search)
{
	PASRequest *req;
	CORBA_Environment ev;

	req                       = g_new0 (PASRequest, 1);
	req->op                   = GetCompletionView;
	req->get_book_view.search = g_strdup(search);
	
	CORBA_exception_init (&ev);

	req->get_book_view.listener = bonobo_object_dup_ref(listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_queue_get_completion_view: Exception "
			   "duplicating BookViewListener!\n");
	}

	CORBA_exception_free (&ev);

	pas_book_queue_request (book, req);
}

static void
pas_book_queue_get_changes (PASBook *book, const GNOME_Evolution_Addressbook_BookViewListener listener, const char *change_id)
{
	PASRequest *req;
	CORBA_Environment ev;

	req           = g_new0 (PASRequest, 1);
	req->op       = GetChanges;
	req->get_changes.change_id= g_strdup(change_id);
	
	CORBA_exception_init (&ev);

	req->get_changes.listener = bonobo_object_dup_ref(listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_queue_get_changes: Exception "
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

static void
impl_GNOME_Evolution_Addressbook_Book_getVCard (PortableServer_Servant servant,
						const CORBA_char *id,
						CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_vcard (book, id);
}

static void
impl_GNOME_Evolution_Addressbook_Book_authenticateUser (PortableServer_Servant servant,
							const char* user,
							const char* passwd,
							const char* auth_method,
							CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_authenticate_user (book, user, passwd, auth_method);
}

static void
impl_GNOME_Evolution_Addressbook_Book_addCard (PortableServer_Servant servant,
					       const CORBA_char *vcard,
					       CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_create_card (book, (const char *) vcard);
}

static void
impl_GNOME_Evolution_Addressbook_Book_removeCard (PortableServer_Servant servant,
						  const CORBA_char *id,
						  CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_remove_card (book, (const char *) id);
}

static void
impl_GNOME_Evolution_Addressbook_Book_modifyCard (PortableServer_Servant servant,
						  const CORBA_char *vcard,
						  CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_modify_card (book, (const char *) vcard);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getCursor (PortableServer_Servant servant,
				const CORBA_char *search,
				CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_cursor (book, search);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getBookView (PortableServer_Servant servant,
				   const GNOME_Evolution_Addressbook_BookViewListener listener,
				   const CORBA_char *search,
				   CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_book_view (book, listener, search);
}


static void
impl_GNOME_Evolution_Addressbook_Book_getCompletionView (PortableServer_Servant servant,
				   const GNOME_Evolution_Addressbook_BookViewListener listener,
				   const CORBA_char *search,
				   CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_completion_view (book, listener, search);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getChanges (PortableServer_Servant servant,
				 const GNOME_Evolution_Addressbook_BookViewListener listener,
				 const CORBA_char *change_id,
				 CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_changes (book, listener, change_id);
}

static void
impl_GNOME_Evolution_Addressbook_Book_checkConnection (PortableServer_Servant servant,
				      CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_check_connection (book);
}

static char *
impl_GNOME_Evolution_Addressbook_Book_getStaticCapabilities (PortableServer_Servant servant,
					     CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));
	char *temp;
	char *ret_val;

	temp = pas_backend_get_static_capabilities (book->priv->backend);
	ret_val = CORBA_string_dup(temp);
	g_free(temp);
	return ret_val;
}

static void
impl_GNOME_Evolution_Addressbook_Book_getSupportedFields (PortableServer_Servant servant,
							  CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object_from_servant (servant));

	pas_book_queue_get_supported_fields (book);
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
GNOME_Evolution_Addressbook_BookListener
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
		       GNOME_Evolution_Addressbook_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (status == GNOME_Evolution_Addressbook_BookListener_Success) {
		GNOME_Evolution_Addressbook_BookListener_notifyBookOpened (
			book->priv->listener, status,
			bonobo_object_corba_objref (BONOBO_OBJECT (book)),
			&ev);
	} else {
		GNOME_Evolution_Addressbook_BookListener_notifyBookOpened (
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
			 GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
			 const char                        *id)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyCardCreated (
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
			 GNOME_Evolution_Addressbook_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyCardRemoved (
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
			 GNOME_Evolution_Addressbook_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyCardModified (
		book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_modify: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_authenticate_user:
 */
void
pas_book_respond_authenticate_user (PASBook                           *book,
				    GNOME_Evolution_Addressbook_BookListener_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyAuthenticationResult (
		book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_authenticate_user: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

void
pas_book_respond_get_supported_fields (PASBook *book,
				       GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
				       EList   *fields)
{
	CORBA_Environment ev;
	GNOME_Evolution_Addressbook_stringlist stringlist;
	int num_fields;
	EIterator *iter;
	int i;

	CORBA_exception_init (&ev);

	num_fields = e_list_length (fields);

	stringlist._buffer = CORBA_sequence_CORBA_string_allocbuf (num_fields);
	stringlist._maximum = num_fields;
	stringlist._length = num_fields;

	iter = e_list_get_iterator (fields);

	for (i = 0; e_iterator_is_valid (iter); e_iterator_next (iter), i ++) {
		stringlist._buffer[i] = CORBA_string_dup (e_iterator_get(iter));
	}

	g_object_unref (fields);

	GNOME_Evolution_Addressbook_BookListener_notifySupportedFields (
			book->priv->listener, status,
			&stringlist,
			&ev);

	CORBA_exception_free (&ev);

	CORBA_free(stringlist._buffer);
}

/**
 * pas_book_respond_get_cursor:
 */
void
pas_book_respond_get_cursor (PASBook                           *book,
			     GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
			     PASCardCursor                     *cursor)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(cursor));

	GNOME_Evolution_Addressbook_BookListener_notifyCursorRequested (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_cursor: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_book_view:
 */
void
pas_book_respond_get_book_view (PASBook                           *book,
				GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
				PASBookView                       *book_view)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(book_view));

	GNOME_Evolution_Addressbook_BookListener_notifyViewRequested (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_book_view: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_book_view:
 */
void
pas_book_respond_get_completion_view (PASBook                           *book,
				      GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
				      PASBookView                       *completion_view)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(completion_view));

	GNOME_Evolution_Addressbook_BookListener_notifyViewRequested (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_completion_view: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_changes:
 */
void
pas_book_respond_get_vcard (PASBook                           *book,
			    GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
			    char                              *vcard)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyCardRequested (
		book->priv->listener, status, vcard, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_card: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_changes:
 */
void
pas_book_respond_get_changes (PASBook                           *book,
			      GNOME_Evolution_Addressbook_BookListener_CallStatus  status,
			      PASBookView                       *book_view)
{
	CORBA_Environment ev;
	CORBA_Object      object;

	CORBA_exception_init (&ev);
	
	object = bonobo_object_corba_objref(BONOBO_OBJECT(book_view));

	GNOME_Evolution_Addressbook_BookListener_notifyChangesRequested (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_changes: Exception "
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

	GNOME_Evolution_Addressbook_BookListener_notifyConnectionStatus (
		book->priv->listener, (CORBA_boolean) connected, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_report_connection: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_report_writable:
 */
void
pas_book_report_writable (PASBook                           *book,
			  gboolean                           writable)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyWritable (
		book->priv->listener, (CORBA_boolean) writable, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_report_writable: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

void
pas_book_construct (PASBook                *book,
		    GNOME_Evolution_Addressbook_Book corba_objref,
		    PASBackend             *backend,
		    GNOME_Evolution_Addressbook_BookListener  listener)
{
	PASBookPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (book != NULL);
	g_return_if_fail (corba_objref != CORBA_OBJECT_NIL);

	priv = book->priv;

	g_return_if_fail (priv->corba_objref == CORBA_OBJECT_NIL);

	priv->corba_objref = corba_objref;
	priv->backend   = backend;

	CORBA_exception_init (&ev);
	book->priv->listener = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("pas_book_construct(): could not duplicate the listener");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	priv->listener  = listener;
}

static PASBookServant *
create_servant (PASBook *book)
{
	PASBookServant *servant;
	POA_GNOME_Evolution_Addressbook_Book *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (PASBookServant, 1);
	corba_servant = (POA_GNOME_Evolution_Addressbook_Book *) servant;

	corba_servant->vepv = &pas_book_vepv;
	POA_GNOME_Evolution_Addressbook_Book__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->object = book;

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_Addressbook_Book
activate_servant (PASBook *book,
		  POA_GNOME_Evolution_Addressbook_Book *servant)
{
	GNOME_Evolution_Addressbook_Book corba_object;
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
 * pas_book_new:
 */
PASBook *
pas_book_new (PASBackend             *backend,
	      GNOME_Evolution_Addressbook_BookListener  listener)
{
	PASBook *book;
	PASBookPrivate *priv;
	GNOME_Evolution_Addressbook_Book corba_objref;

	book = g_object_new (PAS_TYPE_BOOK, NULL);
	priv = book->priv;

	priv->servant = create_servant (book);
	corba_objref = activate_servant (book, (POA_GNOME_Evolution_Addressbook_Book*)priv->servant);
	
	pas_book_construct (book, corba_objref, backend, listener);

	return book;
}

void
pas_book_free_request (PASRequest *req)
{
	CORBA_Environment ev;
	switch (req->op) {
	case CreateCard:
		g_free (req->create.id);
		g_free (req->create.vcard);
		break;
	case RemoveCard:
		g_free (req->remove.id);
		break;
	case ModifyCard:
		g_free (req->modify.vcard);
		break;
	case GetVCard:
		g_free (req->get_vcard.id);
		break;
	case GetCursor:
		g_free (req->get_cursor.search);
		break;
	case GetBookView:
		g_free (req->get_book_view.search);
		CORBA_exception_init (&ev);
		bonobo_object_release_unref (req->get_book_view.listener, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_free_request(GetBookView): could not release the listener");
	
		CORBA_exception_free (&ev);
		break;
	case GetCompletionView:
		g_free (req->get_completion_view.search);
		CORBA_exception_init (&ev);
		bonobo_object_release_unref (req->get_completion_view.listener, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_free_request(GetCompletionView): could not release the listener");
	
		CORBA_exception_free (&ev);
		break;
	case GetChanges:
		g_free (req->get_changes.change_id);
		CORBA_exception_init (&ev);
		bonobo_object_release_unref (req->get_changes.listener, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_free_request(GetChanges): could not release the listener");

		CORBA_exception_free (&ev);
		break;
	case CheckConnection:
		/* nothing to free */
		break;
	case AuthenticateUser:
		g_free (req->auth_user.user);
		g_free (req->auth_user.passwd);
		g_free (req->auth_user.auth_method);
		break;
	case GetSupportedFields:
		/* nothing to free */
		break;
	}

	g_free (req);
}

static void
pas_book_dispose (GObject *object)
{
	PASBook *book = PAS_BOOK (object);
	GList   *l;
	CORBA_Environment ev;

	for (l = book->priv->request_queue; l != NULL; l = l->next) {
		pas_book_free_request ((PASRequest *)l->data);
	}
	g_list_free (book->priv->request_queue);

	/* We should never ever have timeout_id == 0 when we get destroyed, unless there
	   is some sort of reference counting bug.  Still, we do this to try to avoid
	   horrible crashes in those situations. */
	if (book->priv->timeout_id) {
		g_warning ("PASBook destroyed with non-zero timeout_id.  This shouldn't happen.");
		g_source_remove (book->priv->timeout_id);
		book->priv->timeout_id = 0;
	}

	CORBA_exception_init (&ev);
	CORBA_Object_release (book->priv->listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("pas_book_construct(): could not release the listener");

	CORBA_exception_free (&ev);

	g_free (book->priv);
	book->priv = NULL;

	G_OBJECT_CLASS (pas_book_parent_class)->dispose (object);	
}

static void
corba_class_init (PASBookClass *klass)
{
	POA_GNOME_Evolution_Addressbook_Book__vepv *vepv;
	POA_GNOME_Evolution_Addressbook_Book__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;


	epv = &klass->epv;

	epv->getVCard              = impl_GNOME_Evolution_Addressbook_Book_getVCard;
	epv->authenticateUser      = impl_GNOME_Evolution_Addressbook_Book_authenticateUser;
	epv->addCard               = impl_GNOME_Evolution_Addressbook_Book_addCard;
	epv->removeCard            = impl_GNOME_Evolution_Addressbook_Book_removeCard;
	epv->modifyCard            = impl_GNOME_Evolution_Addressbook_Book_modifyCard;
	epv->checkConnection       = impl_GNOME_Evolution_Addressbook_Book_checkConnection;
	epv->getStaticCapabilities = impl_GNOME_Evolution_Addressbook_Book_getStaticCapabilities;
	epv->getSupportedFields    = impl_GNOME_Evolution_Addressbook_Book_getSupportedFields;
	epv->getCursor             = impl_GNOME_Evolution_Addressbook_Book_getCursor;
	epv->getBookView           = impl_GNOME_Evolution_Addressbook_Book_getBookView;
	epv->getCompletionView     = impl_GNOME_Evolution_Addressbook_Book_getCompletionView;
	epv->getChanges            = impl_GNOME_Evolution_Addressbook_Book_getChanges;

	vepv = &pas_book_vepv;
	vepv->_base_epv                            = base_epv;
	vepv->GNOME_Evolution_Addressbook_Book_epv = epv;
}

static void
pas_book_class_init (PASBookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	pas_book_parent_class = g_type_class_peek_parent (klass);

	pas_book_signals [REQUESTS_QUEUED] =
		g_signal_new ("requests_queued",
			      G_OBJECT_CLASS_TYPE(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (PASBookClass, requests_queued),
			      NULL, NULL,
			      pas_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	object_class->dispose = pas_book_dispose;

	corba_class_init (klass);
}

static void
pas_book_init (PASBook *book)
{
	book->priv                = g_new0 (PASBookPrivate, 1);
	book->priv->timeout_id    = 0;
	book->priv->request_queue = NULL;
	book->priv->timeout_id    = 0;
	book->priv->timeout_lock  = FALSE;
}

BONOBO_TYPE_FUNC_FULL (
		       PASBook,
		       GNOME_Evolution_Addressbook_Book,
		       BONOBO_TYPE_OBJECT,
		       pas_book);

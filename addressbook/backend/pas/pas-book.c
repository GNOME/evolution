/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * pas-book.c
 *
 * Copyright 2000, Ximian, Inc.
 */

#include <config.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-arg.h>
#include "e-util/e-list.h"
#include "ebook/e-contact.h"
#include "pas-book-view.h"
#include "pas-backend.h"
#include "pas-backend-card-sexp.h"
#include "pas-marshal.h"

static BonoboObjectClass *pas_book_parent_class;
POA_GNOME_Evolution_Addressbook_Book__vepv pas_book_vepv;

struct _PASBookPrivate {
	PASBackend                               *backend;
	GNOME_Evolution_Addressbook_BookListener  listener;
	char                                     *uri;
};

static void
impl_GNOME_Evolution_Addressbook_Book_open (PortableServer_Servant servant,
					    const CORBA_boolean only_if_exists,
					    CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	printf ("impl_GNOME_Evolution_Addressbook_Book_open\n");

	pas_backend_open (pas_book_get_backend (book), book, only_if_exists);
}

static void
impl_GNOME_Evolution_Addressbook_Book_remove (PortableServer_Servant servant,
					      CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	printf ("impl_GNOME_Evolution_Addressbook_Book_remove\n");

	pas_backend_remove (pas_book_get_backend (book), book);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getContact (PortableServer_Servant servant,
						  const CORBA_char *id,
						  CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	printf ("impl_GNOME_Evolution_Addressbook_Book_getContact\n");

	pas_backend_get_contact (pas_book_get_backend (book), book, id);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getContactList (PortableServer_Servant servant,
						      const CORBA_char *query,
						      CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	printf ("impl_GNOME_Evolution_Addressbook_Book_getContactList\n");

	pas_backend_get_contact_list (pas_book_get_backend (book), book, query);
}

static void
impl_GNOME_Evolution_Addressbook_Book_authenticateUser (PortableServer_Servant servant,
							const char* user,
							const char* passwd,
							const char* auth_method,
							CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_authenticate_user (pas_book_get_backend (book), book,
				       user, passwd, auth_method);
}

static void
impl_GNOME_Evolution_Addressbook_Book_addContact (PortableServer_Servant servant,
						  const CORBA_char *vcard,
						  CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_create_contact (pas_book_get_backend (book), book, vcard);
}

static void
impl_GNOME_Evolution_Addressbook_Book_removeContacts (PortableServer_Servant servant,
						      const GNOME_Evolution_Addressbook_ContactIdList *ids,
						      CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));
	int i;
	GList *id_list = NULL;
	
	for (i = 0; i < ids->_length; i ++)
		id_list = g_list_append (id_list, ids->_buffer[i]);

	pas_backend_remove_contacts (pas_book_get_backend (book), book, id_list);

	g_list_free (id_list);
}

static void
impl_GNOME_Evolution_Addressbook_Book_modifyContact (PortableServer_Servant servant,
						     const CORBA_char *vcard,
						     CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_modify_contact (pas_book_get_backend (book), book, vcard);
}

static void
view_listener_died_cb (gpointer cnx, gpointer user_data)
{
	PASBookView *book_view = PAS_BOOK_VIEW (user_data);

	pas_backend_stop_book_view (pas_book_view_get_backend (book_view), book_view);
	pas_backend_remove_book_view (pas_book_view_get_backend (book_view), book_view);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getBookView (PortableServer_Servant servant,
				   const GNOME_Evolution_Addressbook_BookViewListener listener,
				   const CORBA_char *search,
				   const GNOME_Evolution_Addressbook_stringlist* requested_fields,
				   const CORBA_long max_results,
				   CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));
	PASBackend *backend = pas_book_get_backend (book);
	PASBackendCardSExp *card_sexp;
	PASBookView *view;

	g_warning ("impl_GNOME_Evolution_Addressbook_Book_getBookView (%s)\n", search);

	/* we handle this entirely here, since it doesn't require any
	   backend involvement now that we have pas_book_view_start to
	   actually kick off the search. */

	card_sexp = pas_backend_card_sexp_new (search);
	if (!card_sexp) {
		pas_book_respond_get_book_view (book, GNOME_Evolution_Addressbook_InvalidQuery, NULL);
		return;
	}

	view = pas_book_view_new (backend, listener, search, card_sexp);

	if (!view) {
		g_object_unref (card_sexp);
		pas_book_respond_get_book_view (book, GNOME_Evolution_Addressbook_OtherError, NULL);
		return;
	}

	ORBit_small_listen_for_broken (pas_book_view_get_listener (view), G_CALLBACK (view_listener_died_cb), view);

	pas_backend_add_book_view (backend, view);

	pas_book_respond_get_book_view (book, GNOME_Evolution_Addressbook_Success, view);

	g_object_unref (view);
}


static void
impl_GNOME_Evolution_Addressbook_Book_getChanges (PortableServer_Servant servant,
				 const CORBA_char *change_id,
				 CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_get_changes (pas_book_get_backend (book), book, change_id);
}

static char *
impl_GNOME_Evolution_Addressbook_Book_getStaticCapabilities (PortableServer_Servant servant,
					     CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));
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
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_get_supported_fields (pas_book_get_backend (book), book);
}

static void
impl_GNOME_Evolution_Addressbook_Book_getSupportedAuthMethods (PortableServer_Servant servant,
							       CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	pas_backend_get_supported_auth_methods (pas_book_get_backend (book), book);
}

static GNOME_Evolution_Addressbook_CallStatus
impl_GNOME_Evolution_Addressbook_Book_cancelOperation (PortableServer_Servant servant,
						       CORBA_Environment *ev)
{
	PASBook *book = PAS_BOOK (bonobo_object (servant));

	return pas_backend_cancel_operation (pas_book_get_backend (book), book);
}

/**
 * pas_book_get_backend:
 */
PASBackend *
pas_book_get_backend (PASBook *book)
{
	g_return_val_if_fail (book && PAS_IS_BOOK (book), NULL);

	return book->priv->backend;
}

GNOME_Evolution_Addressbook_BookListener
pas_book_get_listener (PASBook *book)
{
	g_return_val_if_fail (book && PAS_IS_BOOK (book), CORBA_OBJECT_NIL);

	return book->priv->listener;
}

const char*
pas_book_get_uri (PASBook *book)
{
	return book->priv->uri;
}

/**
 * pas_book_respond_open:
 */
void
pas_book_respond_open (PASBook                           *book,
		       GNOME_Evolution_Addressbook_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Addressbook_BookListener_notifyBookOpened (book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_open: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_remove:
 */
void
pas_book_respond_remove (PASBook                           *book,
			 GNOME_Evolution_Addressbook_CallStatus  status)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Addressbook_BookListener_notifyBookRemoved (book->priv->listener, status, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_remove: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_create:
 */
void
pas_book_respond_create (PASBook                                *book,
			 GNOME_Evolution_Addressbook_CallStatus  status,
			 EContact                               *contact)
{
	CORBA_Environment ev;

	if (status == GNOME_Evolution_Addressbook_Success) {
		pas_backend_notify_update (book->priv->backend, contact);
		pas_backend_notify_complete (book->priv->backend);
	}

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyContactCreated (
		book->priv->listener, status,
		e_contact_get_const (contact, E_CONTACT_UID), &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_create: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_remove_contacts:
 */
void
pas_book_respond_remove_contacts (PASBook                                *book,
				  GNOME_Evolution_Addressbook_CallStatus  status,
				  GList                                  *ids)
{
	CORBA_Environment ev;
	GList *i;

	CORBA_exception_init (&ev);

	if (ids) {
		for (i = ids; i; i = i->next)
			pas_backend_notify_remove (book->priv->backend, i->data);
		pas_backend_notify_complete (book->priv->backend);
	}

	GNOME_Evolution_Addressbook_BookListener_notifyContactsRemoved (
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
pas_book_respond_modify (PASBook                                *book,
			 GNOME_Evolution_Addressbook_CallStatus  status,
			 EContact                               *contact)
{
	CORBA_Environment ev;

	if (status == GNOME_Evolution_Addressbook_Success) {
		pas_backend_notify_update (book->priv->backend, contact);
		pas_backend_notify_complete (book->priv->backend);
	}

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyContactModified (
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
				    GNOME_Evolution_Addressbook_CallStatus  status)
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
				       GNOME_Evolution_Addressbook_CallStatus  status,
				       GList   *fields)
{
	CORBA_Environment ev;
	GNOME_Evolution_Addressbook_stringlist stringlist;
	int num_fields;
	int i;
	GList *iter;

	CORBA_exception_init (&ev);

	num_fields = g_list_length (fields);

	stringlist._buffer = CORBA_sequence_CORBA_string_allocbuf (num_fields);
	stringlist._maximum = num_fields;
	stringlist._length = num_fields;

	for (i = 0, iter = fields; iter; iter = iter->next, i ++) {
		stringlist._buffer[i] = CORBA_string_dup ((char*)iter->data);
	}

	printf ("calling GNOME_Evolution_Addressbook_BookListener_notifySupportedFields\n");

	GNOME_Evolution_Addressbook_BookListener_notifySupportedFields (
			book->priv->listener, status,
			&stringlist,
			&ev);

	CORBA_exception_free (&ev);

	CORBA_free(stringlist._buffer);
}

void
pas_book_respond_get_supported_auth_methods (PASBook *book,
					     GNOME_Evolution_Addressbook_CallStatus  status,
					     GList   *auth_methods)
{
	CORBA_Environment ev;
	GNOME_Evolution_Addressbook_stringlist stringlist;
	int num_auth_methods;
	GList *iter;
	int i;

	CORBA_exception_init (&ev);

	num_auth_methods = g_list_length (auth_methods);

	stringlist._buffer = CORBA_sequence_CORBA_string_allocbuf (num_auth_methods);
	stringlist._maximum = num_auth_methods;
	stringlist._length = num_auth_methods;

	for (i = 0, iter = auth_methods; iter; iter = iter->next, i ++) {
		stringlist._buffer[i] = CORBA_string_dup ((char*)iter->data);
	}

	GNOME_Evolution_Addressbook_BookListener_notifySupportedAuthMethods (
			book->priv->listener, status,
			&stringlist,
			&ev);

	CORBA_exception_free (&ev);

	CORBA_free(stringlist._buffer);
}

static void
view_destroy(gpointer data, GObject *where_object_was)
{
	PASBook           *book = (PASBook *)data;
	EIterator         *iterator;
	gboolean success = FALSE;
	EList *views = pas_backend_get_book_views (book->priv->backend);

	for (iterator = e_list_get_iterator(views);
	     e_iterator_is_valid(iterator);
	     e_iterator_next(iterator)) {
		const PASBookView *view = e_iterator_get(iterator);
		if (view == (PASBookView*)where_object_was) {
			e_iterator_delete(iterator);
			success = TRUE;
			break;
		}
	}
	if (!success)
		g_warning ("Failed to remove from book_views list");
	g_object_unref(iterator);
	g_object_unref(views);
}

/**
 * pas_book_respond_get_book_view:
 */
void
pas_book_respond_get_book_view (PASBook                           *book,
				GNOME_Evolution_Addressbook_CallStatus  status,
				PASBookView                       *book_view)
{
	CORBA_Environment ev;
	CORBA_Object      object = CORBA_OBJECT_NIL;

	printf ("pas_book_respond_get_book_view\n");

	CORBA_exception_init (&ev);

	if (book_view) {
		object = bonobo_object_corba_objref(BONOBO_OBJECT(book_view));

		g_object_weak_ref (G_OBJECT (book_view), view_destroy, book);
	}

	GNOME_Evolution_Addressbook_BookListener_notifyViewRequested (
		book->priv->listener, status, object, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("pas_book_respond_get_book_view: Exception "
			   "responding to BookListener!\n");
	}

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_contact:
 */
void
pas_book_respond_get_contact (PASBook                           *book,
			      GNOME_Evolution_Addressbook_CallStatus  status,
			      char                              *vcard)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_BookListener_notifyContactRequested (book->priv->listener,
									 status,
									 vcard,
									 &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("could not notify listener of get-contact response");

	CORBA_exception_free (&ev);
}

/**
 * pas_book_respond_get_contact_list:
 */
void
pas_book_respond_get_contact_list (PASBook                           *book,
				   GNOME_Evolution_Addressbook_CallStatus  status,
				   GList                             *card_list)
{
	CORBA_Environment ev;
	GNOME_Evolution_Addressbook_stringlist stringlist;
	int num_cards;
	int i;
	GList *l;

	CORBA_exception_init (&ev);

	num_cards = g_list_length (card_list);

	stringlist._buffer = CORBA_sequence_CORBA_string_allocbuf (num_cards);
	stringlist._maximum = num_cards;
	stringlist._length = num_cards;

	for (i = 0, l = card_list; l; l = l->next, i ++)
		stringlist._buffer[i] = CORBA_string_dup (l->data);

	g_list_foreach (card_list, (GFunc)g_free, NULL);
	g_list_free (card_list);


	GNOME_Evolution_Addressbook_BookListener_notifyContactListRequested (book->priv->listener,
									     status,
									     &stringlist,
									     &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("could not notify listener of get-contact-list response");

	CORBA_exception_free (&ev);

	CORBA_free(stringlist._buffer);
}

/**
 * pas_book_respond_get_changes:
 */
void
pas_book_respond_get_changes (PASBook                                *book,
			      GNOME_Evolution_Addressbook_CallStatus  status,
			      GList                                  *changes)
{
	CORBA_Environment ev;
	GNOME_Evolution_Addressbook_BookChangeList changelist;
	int num_changes;
	int i;
	GList *l;

	CORBA_exception_init (&ev);

	num_changes = g_list_length (changes);

	changelist._buffer = CORBA_sequence_GNOME_Evolution_Addressbook_BookChangeItem_allocbuf (num_changes);
	changelist._maximum = num_changes;
	changelist._length = num_changes;

	for (i = 0, l = changes; l; l = l->next, i ++) {
		GNOME_Evolution_Addressbook_BookChangeItem *change = (GNOME_Evolution_Addressbook_BookChangeItem*)l->data;
		changelist._buffer[i] = *change;
		switch (change->_d) {
		case GNOME_Evolution_Addressbook_ContactAdded:
			changelist._buffer[i]._u.add_vcard = CORBA_string_dup (change->_u.add_vcard);
			break;
		case GNOME_Evolution_Addressbook_ContactModified:
			changelist._buffer[i]._u.mod_vcard = CORBA_string_dup (change->_u.mod_vcard);
			break;
		case GNOME_Evolution_Addressbook_ContactDeleted:
			changelist._buffer[i]._u.del_id = CORBA_string_dup (change->_u.del_id);
			break;
		}
	}

	g_list_foreach (changes, (GFunc)CORBA_free, NULL);
	g_list_free (changes);

	GNOME_Evolution_Addressbook_BookListener_notifyChangesRequested (book->priv->listener,
									 status,
									 &changelist,
									 &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("could not notify listener of get-changes response");

	CORBA_exception_free (&ev);

	CORBA_free(changelist._buffer);
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

static void
pas_book_construct (PASBook                *book,
		    PASBackend             *backend,
		    const char             *uri,
		    GNOME_Evolution_Addressbook_BookListener listener)
{
	PASBookPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (book != NULL);

	priv = book->priv;

	CORBA_exception_init (&ev);
	book->priv->listener = CORBA_Object_duplicate (listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("pas_book_construct(): could not duplicate the listener");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	priv->backend   = backend;
	priv->uri       = g_strdup (uri);

}

/**
 * pas_book_new:
 */
PASBook *
pas_book_new (PASBackend                               *backend,
	      const char                               *uri,
	      GNOME_Evolution_Addressbook_BookListener  listener)
{
	PASBook *book;
	char *caps = pas_backend_get_static_capabilities (backend);

	book = g_object_new (PAS_TYPE_BOOK,
			     "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL),
			     NULL);

	pas_book_construct (book, backend, uri, listener);

	g_free (caps);

	return book;
}

static void
pas_book_dispose (GObject *object)
{
	PASBook *book = PAS_BOOK (object);

	if (book->priv) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (book->priv->listener, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("pas_book_construct(): could not release the listener");

		CORBA_exception_free (&ev);

		g_free (book->priv->uri);
		g_free (book->priv);
		book->priv = NULL;
	}

	if (G_OBJECT_CLASS (pas_book_parent_class)->dispose)
		G_OBJECT_CLASS (pas_book_parent_class)->dispose (object);
}

static void
pas_book_class_init (PASBookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Evolution_Addressbook_Book__epv *epv;

	pas_book_parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = pas_book_dispose;

	epv = &klass->epv;

	epv->open                    = impl_GNOME_Evolution_Addressbook_Book_open;
	epv->remove                  = impl_GNOME_Evolution_Addressbook_Book_remove;
	epv->getContact              = impl_GNOME_Evolution_Addressbook_Book_getContact;
	epv->getContactList          = impl_GNOME_Evolution_Addressbook_Book_getContactList;
	epv->authenticateUser        = impl_GNOME_Evolution_Addressbook_Book_authenticateUser;
	epv->addContact              = impl_GNOME_Evolution_Addressbook_Book_addContact;
	epv->removeContacts          = impl_GNOME_Evolution_Addressbook_Book_removeContacts;
	epv->modifyContact           = impl_GNOME_Evolution_Addressbook_Book_modifyContact;
	epv->getStaticCapabilities   = impl_GNOME_Evolution_Addressbook_Book_getStaticCapabilities;
	epv->getSupportedFields      = impl_GNOME_Evolution_Addressbook_Book_getSupportedFields;
	epv->getSupportedAuthMethods = impl_GNOME_Evolution_Addressbook_Book_getSupportedAuthMethods;
	epv->getBookView             = impl_GNOME_Evolution_Addressbook_Book_getBookView;
	epv->getChanges              = impl_GNOME_Evolution_Addressbook_Book_getChanges;
	epv->cancelOperation         = impl_GNOME_Evolution_Addressbook_Book_cancelOperation;
}

static void
pas_book_init (PASBook *book)
{
	book->priv                = g_new0 (PASBookPrivate, 1);
}

BONOBO_TYPE_FUNC_FULL (
		       PASBook,
		       GNOME_Evolution_Addressbook_Book,
		       BONOBO_TYPE_OBJECT,
		       pas_book);

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
#include "e-contact.h"
#include "e-book-listener.h"
#include "e-book-marshal.h"

static EBookStatus e_book_listener_convert_status (GNOME_Evolution_Addressbook_CallStatus status);

enum {
	RESPONSE,
	LAST_SIGNAL
};

static guint e_book_listener_signals [LAST_SIGNAL];

static BonoboObjectClass          *parent_class;

struct _EBookListenerPrivate {
	guint stopped      : 1;
};

static EBookStatus
e_book_listener_convert_status (const GNOME_Evolution_Addressbook_CallStatus status)
{
	switch (status) {
	case GNOME_Evolution_Addressbook_Success:
		return E_BOOK_ERROR_OK;
	case GNOME_Evolution_Addressbook_RepositoryOffline:
		return E_BOOK_ERROR_REPOSITORY_OFFLINE;
	case GNOME_Evolution_Addressbook_PermissionDenied:
		return E_BOOK_ERROR_PERMISSION_DENIED;
	case GNOME_Evolution_Addressbook_ContactNotFound:
		return E_BOOK_ERROR_CONTACT_NOT_FOUND;
	case GNOME_Evolution_Addressbook_ContactIdAlreadyExists:
		return E_BOOK_ERROR_CONTACT_ID_ALREADY_EXISTS;
	case GNOME_Evolution_Addressbook_AuthenticationFailed:
		return E_BOOK_ERROR_AUTHENTICATION_FAILED;
	case GNOME_Evolution_Addressbook_AuthenticationRequired:
		return E_BOOK_ERROR_AUTHENTICATION_REQUIRED;
	case GNOME_Evolution_Addressbook_TLSNotAvailable:
		return E_BOOK_ERROR_TLS_NOT_AVAILABLE;
	case GNOME_Evolution_Addressbook_NoSuchBook:
		return E_BOOK_ERROR_NO_SUCH_BOOK;
	case GNOME_Evolution_Addressbook_OtherError:
	default:
		return E_BOOK_ERROR_OTHER_ERROR;
	}
}

static void
impl_BookListener_respond_create_contact (PortableServer_Servant                                    servant,
					  const GNOME_Evolution_Addressbook_CallStatus              status,
					  const CORBA_char*                                         id,
					  CORBA_Environment                                        *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = CreateContactResponse;
	response.status   = e_book_listener_convert_status (status);
	response.id       = g_strdup (id);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);

	g_free (response.id);
}

static void
impl_BookListener_respond_remove_contacts (PortableServer_Servant servant,
					   const GNOME_Evolution_Addressbook_CallStatus status,
					   CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = RemoveContactResponse;
	response.status   = e_book_listener_convert_status (status);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_modify_contact (PortableServer_Servant servant,
					  const GNOME_Evolution_Addressbook_CallStatus status,
					  CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = ModifyContactResponse;
	response.status   = e_book_listener_convert_status (status);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_get_contact (PortableServer_Servant servant,
				       const GNOME_Evolution_Addressbook_CallStatus status,
				       const CORBA_char* card,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = GetContactResponse;
	response.status   = e_book_listener_convert_status (status);
	response.vcard    = g_strdup (card);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);

	g_free (response.vcard);
}

static void
impl_BookListener_respond_get_contact_list (PortableServer_Servant servant,
					    const GNOME_Evolution_Addressbook_CallStatus status,
					    const GNOME_Evolution_Addressbook_stringlist *cards,
					    CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;
	int i;

	if (listener->priv->stopped)
		return;

	response.op     = GetContactListResponse;
	response.status = e_book_listener_convert_status (status);
	response.list   = NULL;

	for (i = 0; i < cards->_length; i ++) {
		response.list = g_list_prepend (response.list, e_contact_new_from_vcard (cards->_buffer[i]));
	}

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);

	/* XXX free response.list? */
}

static void
impl_BookListener_respond_get_view (PortableServer_Servant servant,
				    const GNOME_Evolution_Addressbook_CallStatus status,
				    const GNOME_Evolution_Addressbook_BookView book_view,
				    CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	printf ("impl_BookListener_respond_get_view\n");

	if (listener->priv->stopped)
		return;

	response.op        = GetBookViewResponse;
	response.status    = e_book_listener_convert_status (status);
	response.book_view = bonobo_object_dup_ref (book_view, ev);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_get_changes (PortableServer_Servant servant,
				       const GNOME_Evolution_Addressbook_CallStatus status,
				       const GNOME_Evolution_Addressbook_BookChangeList *changes,
				       CORBA_Environment *ev)
{
	EBookListener        *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;
	int i;

	response.op     = GetChangesResponse;
	response.status = e_book_listener_convert_status (status);
	response.list   = NULL;

	for (i = 0; i < changes->_length; i ++) {
		EBookChange *change = g_new (EBookChange, 1);
		GNOME_Evolution_Addressbook_BookChangeItem corba_change = changes->_buffer[i];

		switch (corba_change._d) {
		case GNOME_Evolution_Addressbook_ContactAdded:
			change->change_type = E_BOOK_CHANGE_CARD_ADDED;
			change->vcard = g_strdup (corba_change._u.add_vcard);
			break;
		case GNOME_Evolution_Addressbook_ContactDeleted:
			change->change_type = E_BOOK_CHANGE_CARD_DELETED;
			change->id = g_strdup (corba_change._u.del_id);
			break;
		case GNOME_Evolution_Addressbook_ContactModified:
			change->change_type = E_BOOK_CHANGE_CARD_MODIFIED;
			change->vcard = g_strdup (corba_change._u.mod_vcard);
			break;
		}

		response.list = g_list_prepend (response.list, change);
	}

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_open_book (PortableServer_Servant servant,
				     const GNOME_Evolution_Addressbook_CallStatus status,
				     CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = OpenBookResponse;
	response.status   = e_book_listener_convert_status (status);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_remove_book (PortableServer_Servant servant,
				       const GNOME_Evolution_Addressbook_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = RemoveBookResponse;
	response.status   = e_book_listener_convert_status (status);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_authentication_result (PortableServer_Servant servant,
						 const GNOME_Evolution_Addressbook_CallStatus status,
						 CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = AuthenticationResponse;
	response.status   = e_book_listener_convert_status (status);

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_get_supported_fields (PortableServer_Servant servant,
						const GNOME_Evolution_Addressbook_CallStatus status,
						const GNOME_Evolution_Addressbook_stringlist *fields,
						CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;
	int i;

	response.op     = GetSupportedFieldsResponse;
	response.status = e_book_listener_convert_status (status);
	response.list   = NULL;

	for (i = 0; i < fields->_length; i ++)
		response.list = g_list_prepend (response.list, g_strdup (fields->_buffer[i]));

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_get_supported_auth_methods (PortableServer_Servant servant,
						      const GNOME_Evolution_Addressbook_CallStatus status,
						      const GNOME_Evolution_Addressbook_stringlist *auth_methods,
						      CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;
	int i;

	response.op     = GetSupportedAuthMethodsResponse;
	response.status = e_book_listener_convert_status (status);
	response.list   = NULL;

	for (i = 0; i < auth_methods->_length; i ++)
		response.list = g_list_prepend (response.list, g_strdup (auth_methods->_buffer[i]));

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_report_writable (PortableServer_Servant servant,
				   const CORBA_boolean writable,
				   CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (bonobo_object (servant));
	EBookListenerResponse response;

	response.op       = WritableStatusEvent;
	response.writable = writable;

	g_signal_emit (listener, e_book_listener_signals [RESPONSE], 0, &response);
}

static void
impl_BookListener_respond_progress (PortableServer_Servant servant,
				    const CORBA_char * message,
				    const CORBA_short percent,
				    CORBA_Environment *ev)
{
}

static void
e_book_listener_construct (EBookListener *listener)
{
	/* nothing to do here */
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

	listener = g_object_new (E_TYPE_BOOK_LISTENER,
				 "poa", bonobo_poa_get_threaded (ORBIT_THREAD_HINT_ALL_AT_IDLE, NULL),
				 NULL);

	e_book_listener_construct (listener);

	return listener;
}

static void
e_book_listener_init (EBookListener *listener)
{
	listener->priv = g_new0 (EBookListenerPrivate, 1);
}

void
e_book_listener_stop (EBookListener *listener)
{
	g_return_if_fail (E_IS_BOOK_LISTENER (listener));

	listener->priv->stopped = TRUE;
}

static void
e_book_listener_class_init (EBookListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Evolution_Addressbook_BookListener__epv *epv;

	parent_class = g_type_class_ref (BONOBO_TYPE_OBJECT);

	e_book_listener_signals [RESPONSE] =
		g_signal_new ("response",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EBookListenerClass, response),
			      NULL, NULL,
			      e_book_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	epv = &klass->epv;
	epv->notifyProgress             = impl_BookListener_respond_progress;
	epv->notifyBookOpened           = impl_BookListener_respond_open_book;
	epv->notifyBookRemoved          = impl_BookListener_respond_remove_book;
	epv->notifyContactCreated       = impl_BookListener_respond_create_contact;
	epv->notifyContactsRemoved      = impl_BookListener_respond_remove_contacts;
	epv->notifyContactModified      = impl_BookListener_respond_modify_contact;
	epv->notifyAuthenticationResult = impl_BookListener_respond_authentication_result;
	epv->notifySupportedFields      = impl_BookListener_respond_get_supported_fields;
	epv->notifySupportedAuthMethods = impl_BookListener_respond_get_supported_auth_methods;
	epv->notifyContactRequested     = impl_BookListener_respond_get_contact;
	epv->notifyContactListRequested = impl_BookListener_respond_get_contact_list;
	epv->notifyViewRequested        = impl_BookListener_respond_get_view;
	epv->notifyChangesRequested     = impl_BookListener_respond_get_changes;
	epv->notifyWritable             = impl_BookListener_report_writable;
}

BONOBO_TYPE_FUNC_FULL (
		       EBookListener,
		       GNOME_Evolution_Addressbook_BookListener,
		       BONOBO_TYPE_OBJECT,
		       e_book_listener);

/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <e-book-listener.h>

static GnomeObjectClass          *e_book_listener_parent_class;
POA_Evolution_BookListener__vepv  e_book_listener_vepv;

static EBookStatus
e_book_listener_convert_status (const Evolution_BookListener_CallStatus status)
{
	switch (status) {

	case Evolution_BookListener_RepositoryOffline:
		return E_BOOK_STATUS_REPOSITORY_OFFLINE;
	case Evolution_BookListener_PermissionDenied:
		return E_BOOK_STATUS_PERMISSION_DENIED;
	case Evolution_BookListener_CardNotFound:
		return E_BOOK_STATUS_CARD_NOT_FOUND;
	default:
		g_warning ("e_book_listener_convert_status: Unknown status "
			   "from card server: %d\n", (int) status);
		return E_BOOK_STATUS_UNKNOWN;

	}
}

static EBookListener *
e_book_listener_construct (EBookListener *listener, EBook *book)
{
	POA_Evolution_BookListener *servant;
	CORBA_Environment           ev;
	CORBA_Object                obj;

	g_assert (listener != NULL);
	g_assert (E_IS_BOOK_LISTENER (listener));
	g_assert (book != NULL);
	g_assert (E_IS_BOOK (book));

	listener->book = book;

	servant = (POA_Evolution_BookListener *) g_new0 (GnomeObjectServant, 1);
	servant->vepv = &e_book_listener_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_BookListener__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);

		return NULL;
	}

	CORBA_exception_free (&ev);

	obj = gnome_object_activate_servant (GNOME_OBJECT (listener), servant);
	if (obj == CORBA_OBJECT_NIL) {
		g_free (servant);

		return NULL;
	}

	gnome_object_construct (GNOME_OBJECT (listener), obj);

	return listener;
}

/**
 * e_book_listener_new:
 */
EBookListener *
e_book_listener_new (EBook *book)
{
	EBookListener *listener;
	EBookListener *retval;

	g_return_val_if_fail (book != NULL,     NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	listener = gtk_type_new (E_BOOK_LISTENER_TYPE);

	retval = e_book_listener_construct (listener, book);

	if (retval == NULL) {
		g_warning ("e_book_listener_new: Error constructing "
			   "EBookListener!\n");
		gtk_object_unref (GTK_OBJECT (listener));
		return NULL;
	}

	return retval;
}

static void
impl_BookListener_respond_create_card (PortableServer_Servant servant,
				       const Evolution_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->create_response == NULL)
		return;

	(listener->create_response) (listener->book,
				     e_book_listener_convert_status (status),
				     listener->closure);
}

static void
impl_BookListener_respond_remove_card (PortableServer_Servant servant,
				       const Evolution_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->remove_response == NULL)
		return;

	(listener->remove_response) (listener->book,
				     e_book_listener_convert_status (status),
				     listener->closure);
}

static void
impl_BookListener_respond_modify_card (PortableServer_Servant servant,
				       const Evolution_BookListener_CallStatus status,
				       CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->modify_response == NULL)
		return;

	(listener->modify_response) (listener->book,
				     e_book_listener_convert_status (status),
				     listener->closure);
}

static void
impl_BookListener_report_open_book_progress (PortableServer_Servant servant,
					     const CORBA_char *status_message,
					     const CORBA_short percent,
					     CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->open_progress == NULL)
		return;

	(listener->open_progress) (listener->book,
				   status_message,
				   percent,
 				   listener->closure);
}

static void
impl_BookListener_respond_open_book (PortableServer_Servant servant,
				     const Evolution_BookListener_CallStatus status,
				     const Evolution_Book book,
				     CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->open_response == NULL)
		return;

	(listener->open_response) (listener->book,
				   e_book_listener_convert_status (status),
				   book,
				   listener->closure);
}


static void
impl_BookListener_report_connection_status (PortableServer_Servant servant,
					    const CORBA_boolean connected,
					    CORBA_Environment *ev)
{
	EBookListener *listener = E_BOOK_LISTENER (gnome_object_from_servant (servant));

	if (listener->connect_status == NULL)
		return;

	(listener->connect_status) (listener->book, connected, listener->closure);
}


static void
e_book_listener_init (EBook *listener)
{
}

static void
e_book_listener_destroy (GtkObject *object)
{
	EBookListener     *listener = E_BOOK_LISTENER (object);
	CORBA_Environment  ev;

	CORBA_exception_init (&ev);

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

	epv->report_connection_status  = impl_BookListener_report_connection_status;

	return epv;
}

static void
e_book_listener_corba_class_init (void)
{
	e_book_listener_vepv.GNOME_Unknown_epv          = gnome_object_get_epv ();
	e_book_listener_vepv.Evolution_BookListener_epv = e_book_listener_get_epv ();
}

static void
e_book_listener_class_init (EBookListenerClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	e_book_listener_parent_class = gtk_type_class (gnome_object_get_type ());

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

		type = gtk_type_unique (gnome_object_get_type (), &info);
	}

	return type;
}

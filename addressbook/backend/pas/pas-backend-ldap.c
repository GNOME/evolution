/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include "config.h"  
#include <gtk/gtksignal.h>
#include <fcntl.h>
#include <time.h>
#include <lber.h>
#include <ldap.h>

#include "pas-backend-ldap.h"
#include "pas-book.h"
#include "pas-card-cursor.h"

static PASBackendClass *pas_backend_ldap_parent_class;
typedef struct _PASBackendLDAPCursorPrivate PASBackendLDAPCursorPrivate;

struct _PASBackendLDAPPrivate {
	gboolean connected;
	GList    *clients;
	LDAP     *ldap;
};

struct _PASBackendLDAPCursorPrivate {
	PASBackend *backend;
	PASBook    *book;

	GList      *elements;
	int        num_elements;
};

static long
get_length(PASCardCursor *cursor, gpointer data)
{
	PASBackendLDAPCursorPrivate *cursor_data = (PASBackendLDAPCursorPrivate *) data;

	return cursor_data->num_elements;
}

static char *
get_nth(PASCardCursor *cursor, long n, gpointer data)
{
	return g_strdup("");
}

static void
cursor_destroy(GtkObject *object, gpointer data)
{
	CORBA_Environment ev;
	Evolution_Book corba_book;
	PASBackendLDAPCursorPrivate *cursor_data = (PASBackendLDAPCursorPrivate *) data;

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(cursor_data->book));

	CORBA_exception_init(&ev);

	Evolution_Book_unref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("cursor_destroy: Exception unreffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);

	/* free the ldap specific cursor information */


	g_free(cursor_data);
}

static char *
pas_backend_ldap_create_unique_id (char *vcard)
{
	/* use a 32 counter and the 32 bit timestamp to make an id.
	   it's doubtful 2^32 id's will be created in a second, so we
	   should be okay. */
	static guint c = 0;
	return g_strdup_printf ("pas-id-%08lX%08X", time(NULL), c++);
}

static void
pas_backend_ldap_process_create_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAP           *ldap = bl->priv->ldap;
	int            ldap_error;
	char           *id;

	id = pas_backend_ldap_create_unique_id (req->vcard);

	/* XXX use ldap_add_s */

	if (LDAP_SUCCESS == ldap_error) {
		pas_book_notify_add(book, id);

		pas_book_respond_create (
				 book,
				 Evolution_BookListener_Success,
				 id);
	}
	else {
		/* XXX need a different call status for this case, i
                   think */
		pas_book_respond_create (
				 book,
				 Evolution_BookListener_CardNotFound,
				 "");
	}

	g_free (id);
	g_free (req->vcard);
}

static void
pas_backend_ldap_process_remove_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAP           *ldap = bl->priv->ldap;
	int            ldap_error;

	/* XXX use ldap_delete_s */

	if (LDAP_SUCCESS == ldap_error) {
		pas_book_notify_remove (book, req->id);

		pas_book_respond_remove (
				  book,
				  Evolution_BookListener_Success);
	}
	else {
		pas_book_respond_remove (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	g_free (req->id);
}

static void
pas_backend_ldap_build_all_cards_list(PASBackend *backend,
				      PASBackendLDAPCursorPrivate *cursor_data)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAP           *ldap = bl->priv->ldap;
	int            ldap_error;
	LDAPMessage    *res, *e;


	if (ldap_search_s (ldap, NULL, LDAP_SCOPE_ONELEVEL,
			   "(objectclass=*)",
			   NULL, 0, &res) == -1) {
		ldap_perror (ldap, "ldap_search");
	}

	cursor_data->elements = NULL;

	cursor_data->num_elements = ldap_count_entries (ldap, res);

	e = ldap_first_entry(ldap, res);

	while (NULL != e) {

		/* for now just make a list of the dn's */
#if 0
		for ( a = ldap_first_attribute( ldap, e, &ber ); a != NULL;
		      a = ldap_next_attribute( ldap, e, ber ) ) {
		}
#else
		cursor_data->elements = g_list_prepend(cursor_data->elements,
						       g_strdup(ldap_get_dn(ldap, e)));
#endif

		e = ldap_next_entry(ldap, e);
	}

	ldap_msgfree(res);
}

static void
pas_backend_ldap_process_modify_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAP           *ldap = bl->priv->ldap;
	int            ldap_error;

	/* XXX use ldap_modify_s */

	if (LDAP_SUCCESS == ldap_error) {

		pas_book_notify_change (book, req->id);

		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_Success);
	}
	else {
		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	g_free (req->vcard);
}

static void
pas_backend_ldap_process_get_all_cards (PASBackend *backend,
					PASBook    *book,
					PASRequest *req)
{
	CORBA_Environment ev;
	PASBackendLDAPCursorPrivate *cursor_data;
	int            ldap_error = 0;
	PASCardCursor *cursor;
	Evolution_Book corba_book;

	cursor_data = g_new(PASBackendLDAPCursorPrivate, 1);
	cursor_data->backend = backend;
	cursor_data->book = book;

	pas_backend_ldap_build_all_cards_list(backend, cursor_data);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	Evolution_Book_ref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("pas_backend_file_process_get_all_cards: Exception reffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);
	
	cursor = pas_card_cursor_new(get_length,
				     get_nth,
				     cursor_data);

	gtk_signal_connect(GTK_OBJECT(cursor), "destroy",
			   GTK_SIGNAL_FUNC(cursor_destroy), cursor_data);
	
	pas_book_respond_get_cursor (
		book,
		(ldap_error == 0 
		 ? Evolution_BookListener_Success 
		 : Evolution_BookListener_CardNotFound),
		cursor);
}

static void
pas_backend_ldap_process_check_connection (PASBackend *backend,
					   PASBook    *book,
					   PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);

	pas_book_report_connection (book, bl->priv->connected);
}

static void
pas_backend_ldap_process_client_requests (PASBook *book)
{
	PASBackend *backend;
	PASRequest *req;

	backend = pas_book_get_backend (book);

	req = pas_book_pop_request (book);
	if (req == NULL)
		return;

	switch (req->op) {
	case CreateCard:
		pas_backend_ldap_process_create_card (backend, book, req);
		break;

	case RemoveCard:
		pas_backend_ldap_process_remove_card (backend, book, req);
		break;

	case ModifyCard:
		pas_backend_ldap_process_modify_card (backend, book, req);
		break;

	case CheckConnection:
		pas_backend_ldap_process_check_connection (backend, book, req);
		break;
		
	case GetAllCards:
		pas_backend_ldap_process_get_all_cards (backend, book, req);
		break;
	}

	g_free (req);
}

static void
pas_backend_ldap_book_destroy_cb (PASBook *book)
{
	PASBackendLDAP *backend;

	backend = PAS_BACKEND_LDAP (pas_book_get_backend (book));

	pas_backend_remove_client (PAS_BACKEND (backend), book);
}

static char *
pas_backend_ldap_get_vcard (PASBook *book, const char *id)
{
	PASBackendLDAP *bl;
	LDAP           *ldap;
	int            ldap_error;

	bl = PAS_BACKEND_LDAP (pas_book_get_backend (book));
	ldap = bl->priv->ldap;

	/* XXX use ldap_search */

	if (LDAP_SUCCESS == ldap_error) {
		/* success */
		return g_strdup ("");
	}
	else {
		return g_strdup ("");
	}
}

static void
pas_backend_ldap_load_uri (PASBackend             *backend,
			   const char             *uri)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAPURLDesc    *lud;
	int ldap_error;

	g_assert (bl->priv->connected == FALSE);

#if 0
	ldap_error = ldap_url_parse (uri, &lud);
	if (LDAP_SUCCESS == ldap_error) {
		bl->priv->ldap = ldap_open (lud->lud_host, lud->lud_port);
		if (NULL != bl->priv->ldap)
			bl->priv->connected = TRUE;
		else
			g_warning ("pas_backend_ldap_load_uri failed for '%s' (error %s)\n",
				   uri, ldap_err2string(ldap_error));

		ldap_free_urldesc(lud);
	}
	else {
		g_warning ("pas_backend_ldap_load_uri failed for '%s' (error %s)\n",
			   uri, ldap_err2string(ldap_error));
	}
#else
	bl->priv->ldap = ldap_init ("ldap.bigfoot.com", 389);
	if (NULL != bl->priv->ldap)
		bl->priv->connected = TRUE;
	else
		g_warning ("pas_backend_ldap_load_uri failed for '%s' (error %s)\n",
			   uri, ldap_err2string(ldap_error));

	ldap_bind_s(bl->priv->ldap, NULL /*binddn*/, NULL /*passwd*/, LDAP_AUTH_SIMPLE);

#endif
}

static void
pas_backend_ldap_add_client (PASBackend             *backend,
			     Evolution_BookListener  listener)
{
	PASBackendLDAP *bl;
	PASBook        *book;

	g_assert (backend != NULL);
	g_assert (PAS_IS_BACKEND_LDAP (backend));

	bl = PAS_BACKEND_LDAP (backend);

	book = pas_book_new (
		backend, listener,
		pas_backend_ldap_get_vcard);

	g_assert (book != NULL);

	gtk_signal_connect (GTK_OBJECT (book), "destroy",
		    pas_backend_ldap_book_destroy_cb, NULL);

	gtk_signal_connect (GTK_OBJECT (book), "requests_queued",
		    pas_backend_ldap_process_client_requests, NULL);

	bl->priv->clients = g_list_prepend (
		bl->priv->clients, book);

	if (bl->priv->connected) {
		pas_book_respond_open (
			book, Evolution_BookListener_Success);
	} else {
		/* Open the book. */
		pas_book_respond_open (
			book, Evolution_BookListener_Success);
	}
}

static void
pas_backend_ldap_remove_client (PASBackend             *backend,
				PASBook                *book)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (book != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));

	g_warning ("pas_backend_ldap_remove_client: Unimplemented!\n");
}

static gboolean
pas_backend_ldap_construct (PASBackendLDAP *backend)
{
	g_assert (backend != NULL);
	g_assert (PAS_IS_BACKEND_LDAP (backend));

	if (! pas_backend_construct (PAS_BACKEND (backend)))
		return FALSE;

	return TRUE;
}

/**
 * pas_backend_ldap_new:
 */
PASBackend *
pas_backend_ldap_new (void)
{
	PASBackendLDAP *backend;

	backend = gtk_type_new (pas_backend_ldap_get_type ());

	if (! pas_backend_ldap_construct (backend)) {
		gtk_object_unref (GTK_OBJECT (backend));

		return NULL;
	}

	return PAS_BACKEND (backend);
}

static void
pas_backend_ldap_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (pas_backend_ldap_parent_class)->destroy (object);	
}

static void
pas_backend_ldap_class_init (PASBackendLDAPClass *klass)
{
	GtkObjectClass  *object_class = (GtkObjectClass *) klass;
	PASBackendClass *parent_class;

	pas_backend_ldap_parent_class = gtk_type_class (pas_backend_get_type ());

	parent_class = PAS_BACKEND_CLASS (klass);

	/* Set the virtual methods. */
	parent_class->load_uri      = pas_backend_ldap_load_uri;
	parent_class->add_client    = pas_backend_ldap_add_client;
	parent_class->remove_client = pas_backend_ldap_remove_client;

	object_class->destroy = pas_backend_ldap_destroy;
}

static void
pas_backend_ldap_init (PASBackendLDAP *backend)
{
	PASBackendLDAPPrivate *priv;

	priv          = g_new0 (PASBackendLDAPPrivate, 1);
	priv->connected  = FALSE;
	priv->clients = NULL;

	backend->priv = priv;
}

/**
 * pas_backend_ldap_get_type:
 */
GtkType
pas_backend_ldap_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBackendLDAP",
			sizeof (PASBackendLDAP),
			sizeof (PASBackendLDAPClass),
			(GtkClassInitFunc)  pas_backend_ldap_class_init,
			(GtkObjectInitFunc) pas_backend_ldap_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (pas_backend_get_type (), &info);
	}

	return type;
}

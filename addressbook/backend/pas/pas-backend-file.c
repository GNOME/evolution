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
#ifdef HAVE_DB_185_H
#include <db_185.h>
#else
#include <db.h>
#endif

#include <pas-backend-file.h>
#include <pas-book.h>
#include <pas-card-cursor.h>
#include <e-card.h>

#define PAS_BACKEND_FILE_VERSION_NAME "PAS-DB-VERSION"
#define PAS_BACKEND_FILE_VERSION "0.1"

static PASBackendClass *pas_backend_file_parent_class;
typedef struct _PASBackendFileCursorPrivate PASBackendFileCursorPrivate;
typedef struct _PASBackendFileBookView PASBackendFileBookView;

struct _PASBackendFilePrivate {
	GList    *clients;
	gboolean  loaded;
	DB       *file_db;
	GList    *book_views;
};

struct _PASBackendFileCursorPrivate {
	PASBackend *backend;
	PASBook    *book;

	GList      *elements;
	guint32    num_elements;
};

struct _PASBackendFileBookView {
	PASBookView *book_view;
	gchar       *search;
};

static long
get_length(PASCardCursor *cursor, gpointer data)
{
	PASBackendFileCursorPrivate *cursor_data = (PASBackendFileCursorPrivate *) data;

	return cursor_data->num_elements;
}

static char *
get_nth(PASCardCursor *cursor, long n, gpointer data)
{
	PASBackendFileCursorPrivate *cursor_data = (PASBackendFileCursorPrivate *) data;
	GList *nth_item = g_list_nth(cursor_data->elements, n);

	return g_strdup((char*)nth_item->data);
}

static void
cursor_destroy(GtkObject *object, gpointer data)
{
	CORBA_Environment ev;
	Evolution_Book corba_book;
	PASBackendFileCursorPrivate *cursor_data = (PASBackendFileCursorPrivate *) data;

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(cursor_data->book));

	CORBA_exception_init(&ev);

	Evolution_Book_unref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("cursor_destroy: Exception unreffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);

	g_list_foreach(cursor_data->elements, (GFunc)g_free, NULL);
	g_list_free (cursor_data->elements);

	g_free(cursor_data);
}

static void
view_destroy(GtkObject *object, gpointer data)
{
	CORBA_Environment ev;
	Evolution_Book    corba_book;
	PASBook           *book = (PASBook *)data;
	PASBackendFile    *bf;
	GList             *list;

	bf = PAS_BACKEND_FILE(pas_book_get_backend(book));
	for (list = bf->priv->book_views; list; list = g_list_next(list)) {
		PASBackendFileBookView *view = list->data;
		if (view->book_view == PAS_BOOK_VIEW(object)) {
			g_free (view->search);
			g_free (view);
			bf->priv->book_views = g_list_remove_link(bf->priv->book_views, list);
			g_list_free_1(list);
			break;
		}
	}

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	Evolution_Book_unref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("view_destroy: Exception unreffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);
}

static void
string_to_dbt(const char *str, DBT *dbt)
{
	dbt->data = (void*)str;
	dbt->size = strlen (str);
}

static char *
pas_backend_file_create_unique_id (char *vcard)
{
	/* use a 32 counter and the 32 bit timestamp to make an id.
	   it's doubtful 2^32 id's will be created in a second, so we
	   should be okay. */
	static guint c = 0;
	return g_strdup_printf ("pas-id-%08lX%08X", time(NULL), c++);
}

static void
pas_backend_file_process_create_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt, vcard_dbt;
	int            db_error;
	char           *id;
	GList         *list;
	ECard         *card;
	char          *vcard;

	id = pas_backend_file_create_unique_id (req->vcard);

	string_to_dbt (id, &id_dbt);
	
	card = e_card_new(req->vcard);
	e_card_set_id(card, id);
	vcard = e_card_get_vcard(card);
	gtk_object_unref(GTK_OBJECT(card));
	card = NULL;

	string_to_dbt (vcard, &vcard_dbt);

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {
		for (list = bf->priv->book_views; list; list = g_list_next(list)) {
			PASBackendFileBookView *view = list->data;
			/* if (card matches view->search) */
			pas_book_view_notify_add_1 (view->book_view, req->vcard);
		}

		pas_book_respond_create (
				 book,
				 Evolution_BookListener_Success,
				 id);

		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");
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
	g_free (vcard);
	g_free (req->vcard);
}

static void
pas_backend_file_process_remove_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt;
	int            db_error;
	GList         *list;

	string_to_dbt (req->id, &id_dbt);

	db_error = db->del (db, &id_dbt, 0);

	if (0 == db_error) {
		for (list = bf->priv->book_views; list; list = g_list_next(list)) {
			PASBackendFileBookView *view = list->data;
			/* if (card matches view->search) */
			pas_book_view_notify_remove (view->book_view, req->id);
		}

		pas_book_respond_remove (
				  book,
				  Evolution_BookListener_Success);

		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");
	}
	else {
		pas_book_respond_remove (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	g_free (req->id);
}

static void
pas_backend_file_process_modify_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt, vcard_dbt;
	int            db_error;
	GList         *list;
	ECard         *card;
	char          *id;

	card = e_card_new(req->vcard);
	id = e_card_get_id(card);

	string_to_dbt (id, &id_dbt);
	string_to_dbt (req->vcard, &vcard_dbt);	

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {
		for (list = bf->priv->book_views; list; list = g_list_next(list)) {
			PASBackendFileBookView *view = list->data;
			/* if (card matches view->search) */
			pas_book_view_notify_change_1 (view->book_view, req->vcard);
			/* else if (card changes to match view->search )
			   pas_book_view_notify_add_1 (view->book_view, req->vcard);
			   else if (card changes to not match view->search )
			   pas_book_view_notify_remove (view->book_view, id);
			*/
		}

		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_Success);

		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");
	}
	else {
		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	gtk_object_unref(GTK_OBJECT(card));
	g_free (req->vcard);
}

static void
pas_backend_file_build_all_cards_list(PASBackend *backend,
				      PASBackendFileCursorPrivate *cursor_data)
{
	  PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	  DB             *db = bf->priv->file_db;
	  int            db_error;
	  DBT  id_dbt, vcard_dbt;
  
	  cursor_data->elements = NULL;
	  
	  db_error = db->seq(db, &id_dbt, &vcard_dbt, R_FIRST);

	  while (db_error == 0) {

		  /* don't include the version in the list of cards */
		  if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME)
		      || strncmp (id_dbt.data, PAS_BACKEND_FILE_VERSION_NAME, id_dbt.size)) {

			  cursor_data->elements = g_list_append(cursor_data->elements,
								g_strndup(vcard_dbt.data,
									  vcard_dbt.size));
		  }

		  db_error = db->seq(db, &id_dbt, &vcard_dbt, R_NEXT);

	  }

	  if (db_error == -1) {
		  g_warning ("pas_backend_file_build_all_cards_list: error building list\n");
	  }
	  else {
		  cursor_data->num_elements = g_list_length (cursor_data->elements);
	  }
}

static void
pas_backend_file_process_get_cursor (PASBackend *backend,
				     PASBook    *book,
				     PASRequest *req)
{
	/*
	  PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	  DB             *db = bf->priv->file_db;
	  DBT            id_dbt, vcard_dbt;
	*/
	CORBA_Environment ev;
	int            db_error = 0;
	PASBackendFileCursorPrivate *cursor_data;
	PASCardCursor *cursor;
	Evolution_Book corba_book;

	cursor_data = g_new(PASBackendFileCursorPrivate, 1);
	cursor_data->backend = backend;
	cursor_data->book = book;

	pas_backend_file_build_all_cards_list(backend, cursor_data);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	Evolution_Book_ref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("pas_backend_file_process_get_cursor: Exception reffing "
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
		(db_error == 0 
		 ? Evolution_BookListener_Success 
		 : Evolution_BookListener_CardNotFound),
		cursor);
}

static void
pas_backend_file_process_get_book_view (PASBackend *backend,
					PASBook    *book,
					PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt, vcard_dbt;
	CORBA_Environment ev;
	int               db_error = 0;
	PASBookView       *book_view;
	Evolution_Book    corba_book;
	GList             *cards = NULL;
	PASBackendFileBookView *view;
	

	g_return_if_fail (req->listener != NULL);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	Evolution_Book_ref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("pas_backend_file_process_get_book_view: Exception reffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);

	book_view = pas_book_view_new (req->listener);

	gtk_signal_connect(GTK_OBJECT(book_view), "destroy",
			   GTK_SIGNAL_FUNC(view_destroy), book);

	pas_book_respond_get_book_view (book,
		   (db_error == 0 
		    ? Evolution_BookListener_Success 
		    : Evolution_BookListener_CardNotFound /* XXX */),
		   book_view);

	/*
	** no reason to not iterate through the file now and notify
	** the listener of all the cards.
	*/

	db_error = db->seq(db, &id_dbt, &vcard_dbt, R_FIRST);

	while (db_error == 0) {

		/* don't include the version in the list of cards */
		if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME)
		    || strncmp (id_dbt.data, PAS_BACKEND_FILE_VERSION_NAME, id_dbt.size)) {
			
			cards = g_list_append(cards,
					      g_strndup(vcard_dbt.data,
							vcard_dbt.size));
		}
		
		db_error = db->seq(db, &id_dbt, &vcard_dbt, R_NEXT);
	}

	if (db_error == -1) {
		g_warning ("pas_backend_file_process_get_book_view: error building list\n");
	}
	else {
		pas_book_view_notify_add (book_view, cards);
	}

	/*
	** It's fine to do this now since the data has been handed off.
	*/
	g_list_foreach (cards, (GFunc)g_free, NULL);
	g_list_free (cards);

	view = g_new(PASBackendFileBookView, 1);
	view->book_view = book_view;
	view->search = g_strdup(req->search);
	bf->priv->book_views = g_list_prepend(bf->priv->book_views, view);
}

static void
pas_backend_file_process_check_connection (PASBackend *backend,
					   PASBook    *book,
					   PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);

	pas_book_report_connection (book, bf->priv->file_db != NULL);
}

static void
pas_backend_file_process_client_requests (PASBook *book)
{
	PASBackend *backend;
	PASRequest *req;

	backend = pas_book_get_backend (book);

	req = pas_book_pop_request (book);
	if (req == NULL)
		return;

	switch (req->op) {
	case CreateCard:
		pas_backend_file_process_create_card (backend, book, req);
		break;

	case RemoveCard:
		pas_backend_file_process_remove_card (backend, book, req);
		break;

	case ModifyCard:
		pas_backend_file_process_modify_card (backend, book, req);
		break;

	case CheckConnection:
		pas_backend_file_process_check_connection (backend, book, req);
		break;
		
	case GetCursor:
		pas_backend_file_process_get_cursor (backend, book, req);
		break;
		
	case GetBookView:
		pas_backend_file_process_get_book_view (backend, book, req);
		break;
	}

	g_free (req);
}

static void
pas_backend_file_book_destroy_cb (PASBook *book)
{
	PASBackendFile *backend;

	backend = PAS_BACKEND_FILE (pas_book_get_backend (book));

	pas_backend_remove_client (PAS_BACKEND (backend), book);
}

static char *
pas_backend_file_get_vcard (PASBook *book, const char *id)
{
	PASBackendFile *bf;
	DBT            id_dbt, vcard_dbt;
	DB             *db;
	int            db_error;

	bf = PAS_BACKEND_FILE (pas_book_get_backend (book));
	db = bf->priv->file_db;

	string_to_dbt (id, &id_dbt);

	db_error = db->get (db, &id_dbt, &vcard_dbt, 0);
	if (db_error == 0) {
		/* success */
		return g_strndup (vcard_dbt.data, vcard_dbt.size);
	}
	else if (db_error == 1) {
		/* key was not in file */
		return g_strdup (""); /* XXX */
	}
	else /* if (db_error < 0)*/ {
		/* error */
		return g_strdup (""); /* XXX */
	}
}

static char *
pas_backend_file_extract_path_from_uri (const char *uri)
{
	g_assert (strncasecmp (uri, "file:", 5) == 0);

	return g_strdup (uri + 5);
}

static gboolean
pas_backend_file_upgrade_db (PASBackendFile *bf, char *old_version)
{
	if (!strcmp (old_version, "0.0")) {
		/* 0.0 is the same as 0.1, we just need to add the version */
		DB  *db = bf->priv->file_db;
		DBT version_name_dbt, version_dbt;
		int db_error;

		string_to_dbt (PAS_BACKEND_FILE_VERSION_NAME, &version_name_dbt);
		string_to_dbt (PAS_BACKEND_FILE_VERSION, &version_dbt);

		db_error = db->put (db, &version_name_dbt, &version_dbt, 0);
		if (db_error == 0)
			return TRUE;
		else
			return FALSE;
	}
	else {
		g_warning ("unsupported version '%s' found in PAS backend file\n",
			   old_version);
		return FALSE;
	}
}

static gboolean
pas_backend_file_maybe_upgrade_db (PASBackendFile *bf)
{
	DB   *db = bf->priv->file_db;
	DBT  version_name_dbt, version_dbt;
	int  db_error;
	char *version;
	gboolean ret_val = TRUE;

	string_to_dbt (PAS_BACKEND_FILE_VERSION_NAME, &version_name_dbt);

	db_error = db->get (db, &version_name_dbt, &version_dbt, 0);
	if (db_error == 0) {
		/* success */
		version = g_strndup (version_dbt.data, version_dbt.size);
	}
	else {
		/* key was not in file */
		version = g_strdup ("0.0");
	}

	if (strcmp (version, PAS_BACKEND_FILE_VERSION))
		ret_val = pas_backend_file_upgrade_db (bf, version);

	g_free (version);

	return ret_val;
}

static void
pas_backend_file_load_uri (PASBackend             *backend,
			   const char             *uri)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	char           *filename;

	g_assert (bf->priv->loaded == FALSE);

	filename = pas_backend_file_extract_path_from_uri (uri);

	bf->priv->file_db = dbopen (filename, O_RDWR | O_CREAT, 0666, DB_HASH, NULL);

	if (bf->priv->file_db != NULL) {
		if (pas_backend_file_maybe_upgrade_db (bf))
			bf->priv->loaded = TRUE;
		/* XXX what if we fail to upgrade it? */
	}
	else
		g_warning ("pas_backend_file_load_uri failed for '%s'\n", filename);

	g_free (filename);
}

static void
pas_backend_file_add_client (PASBackend             *backend,
			     Evolution_BookListener  listener)
{
	PASBackendFile *bf;
	PASBook        *book;

	g_assert (backend != NULL);
	g_assert (PAS_IS_BACKEND_FILE (backend));

	bf = PAS_BACKEND_FILE (backend);

	book = pas_book_new (
		backend, listener,
		pas_backend_file_get_vcard);

	g_assert (book != NULL);

	gtk_signal_connect (GTK_OBJECT (book), "destroy",
		    pas_backend_file_book_destroy_cb, NULL);

	gtk_signal_connect (GTK_OBJECT (book), "requests_queued",
		    pas_backend_file_process_client_requests, NULL);

	bf->priv->clients = g_list_prepend (
		bf->priv->clients, book);

	if (bf->priv->loaded) {
		pas_book_respond_open (
			book, Evolution_BookListener_Success);
	} else {
		/* Open the book. */
		pas_book_respond_open (
			book, Evolution_BookListener_Success);
	}
}

static void
pas_backend_file_remove_client (PASBackend             *backend,
				PASBook                *book)
{
	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND (backend));
	g_return_if_fail (book != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));

	g_warning ("pas_backend_file_remove_client: Unimplemented!\n");
}

static gboolean
pas_backend_file_construct (PASBackendFile *backend)
{
	g_assert (backend != NULL);
	g_assert (PAS_IS_BACKEND_FILE (backend));

	if (! pas_backend_construct (PAS_BACKEND (backend)))
		return FALSE;

	return TRUE;
}

/**
 * pas_backend_file_new:
 */
PASBackend *
pas_backend_file_new (void)
{
	PASBackendFile *backend;

	backend = gtk_type_new (pas_backend_file_get_type ());

	if (! pas_backend_file_construct (backend)) {
		gtk_object_unref (GTK_OBJECT (backend));

		return NULL;
	}

	return PAS_BACKEND (backend);
}

static void
pas_backend_file_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (pas_backend_file_parent_class)->destroy (object);	
}

static void
pas_backend_file_class_init (PASBackendFileClass *klass)
{
	GtkObjectClass  *object_class = (GtkObjectClass *) klass;
	PASBackendClass *parent_class;

	pas_backend_file_parent_class = gtk_type_class (pas_backend_get_type ());

	parent_class = PAS_BACKEND_CLASS (klass);

	/* Set the virtual methods. */
	parent_class->load_uri      = pas_backend_file_load_uri;
	parent_class->add_client    = pas_backend_file_add_client;
	parent_class->remove_client = pas_backend_file_remove_client;

	object_class->destroy = pas_backend_file_destroy;
}

static void
pas_backend_file_init (PASBackendFile *backend)
{
	PASBackendFilePrivate *priv;

	priv             = g_new0 (PASBackendFilePrivate, 1);
	priv->loaded     = FALSE;
	priv->clients    = NULL;
	priv->book_views = NULL;

	backend->priv = priv;
}

/**
 * pas_backend_file_get_type:
 */
GtkType
pas_backend_file_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBackendFile",
			sizeof (PASBackendFile),
			sizeof (PASBackendFileClass),
			(GtkClassInitFunc)  pas_backend_file_class_init,
			(GtkObjectInitFunc) pas_backend_file_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (pas_backend_get_type (), &info);
	}

	return type;
}

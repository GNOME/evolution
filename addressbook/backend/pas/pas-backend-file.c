/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */
  
#include <gtk/gtksignal.h>
#include <fcntl.h>
#include <db.h>

#include <pas-backend-file.h>
#include <pas-book.h>

static PASBackendClass *pas_backend_file_parent_class;

struct _PASBackendFilePrivate {
	GList    *clients;
	gboolean  loaded;
	DB       *file_db;
};

static char *
pas_backend_file_create_unique_id (char *vcard)
{
	return g_strdup ("foo");  /* XXX create unique id here */
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

	id = pas_backend_file_create_unique (req->vcard);

	id_dbt.data = id;
	id_dbt.size = strlen(id_dbt.data);

	vcard_dbt.data = (void*)req->vcard;
	vcard_dbt.size = strlen(req->vcard);

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {
		pas_book_notify_add(book, id);

		pas_book_respond_create (
				 book,
				 Evolution_BookListener_Success);

		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");
	}
	else {
		/* XXX need a different call status for this case, i
                   think */
		pas_book_respond_create (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	g_free (id);
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

	id_dbt.data = (void*)req->id;
	id_dbt.size = strlen(req->id);

	db_error = db->del (db, &id_dbt, 0);

	if (0 == db_error) {
		pas_book_notify_remove (book, req->id);

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

	id_dbt.data = (void*)req->id;
	id_dbt.size = strlen(req->id);

	vcard_dbt.data = (void*)req->vcard;
	vcard_dbt.size = strlen(req->vcard);

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {

		pas_book_notify_change (book, req->id);

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

	g_free (req->vcard);
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

	id_dbt.data = (void*)id;
	id_dbt.size = strlen(id);

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

	return g_strdup ("blah blah blah");
}

static char *
pas_backend_file_extract_path_from_uri (const char *uri)
{
	g_assert (strncasecmp (uri, "file:", 5) == 0);

	return g_strdup (uri + 5);
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

	if (bf->priv->file_db != NULL)
		bf->priv->loaded = TRUE;
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

	priv          = g_new0 (PASBackendFilePrivate, 1);
	priv->loaded  = FALSE;
	priv->clients = NULL;

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

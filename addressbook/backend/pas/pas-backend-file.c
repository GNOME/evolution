/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include "config.h"  
#include <gtk/gtksignal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#ifdef HAVE_DB_185_H
#include <db_185.h>
#else
#include <db.h>
#endif

#include "pas-backend-file.h"
#include "pas-book.h"
#include "pas-card-cursor.h"
#include <ebook/e-card.h>
#include <e-util/e-sexp.h>

#define PAS_BACKEND_FILE_VERSION_NAME "PAS-DB-VERSION"
#define PAS_BACKEND_FILE_VERSION "0.1"

static PASBackendClass *pas_backend_file_parent_class;
typedef struct _PASBackendFileCursorPrivate PASBackendFileCursorPrivate;
typedef struct _PASBackendFileBookView PASBackendFileBookView;
typedef struct _PASBackendFileSearchContext PASBackendFileSearchContext;

struct _PASBackendFilePrivate {
	GList    *clients;
	gboolean  loaded;
	char     *uri;
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
	PASBookView                 *book_view;
	gchar                       *search;
	ESExp                       *search_sexp;
	PASBackendFileSearchContext *search_context;
};

struct _PASBackendFileSearchContext {
	ECard *ecard;
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
			gtk_object_unref((GtkObject *)view->search_sexp);
			g_free (view->search_context);
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
	dbt->size = strlen (str) + 1;
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

static gboolean
compare_email (ECard *ecard, const char *str,
	       char *(*compare)(const char*, const char*))
{
	ECardList *prop_list;
	ECardIterator *iter;
	gboolean truth = FALSE;

	gtk_object_get(GTK_OBJECT(ecard),
		       "email", &prop_list, NULL);

	iter = e_card_list_get_iterator(prop_list);

	while (e_card_iterator_is_valid(iter)) {
		
		if (compare((char*)e_card_iterator_get(iter), str)) {
			truth = TRUE;
			break;
		}
		else {
			e_card_iterator_next(iter);
		}
	}

	gtk_object_unref (GTK_OBJECT(iter));

	return truth;
}

static gboolean
compare_phone (ECard *ecard, const char *str,
	       char *(*compare)(const char*, const char*))
{
	ECardList *prop_list;
	ECardIterator *iter;
	gboolean truth = FALSE;

	gtk_object_get(GTK_OBJECT(ecard),
		       "phone", &prop_list, NULL);
				
	iter = e_card_list_get_iterator(prop_list);

	while (e_card_iterator_is_valid(iter)) {
		ECardPhone *phone = (ECardPhone*)e_card_iterator_get(iter);
		if (compare(phone->number, str)) {
			truth = TRUE;
			break;
		}
		else {
			e_card_iterator_next(iter);
		}
	}

	gtk_object_unref (GTK_OBJECT(iter));

	return truth;
}

static gboolean
compare_address (ECard *ecard, const char *str,
		 char *(*compare)(const char*, const char*))
{
	g_warning("address searching not implemented\n");
	return FALSE;
}

static struct prop_info {
	const char *query_prop;
	const char *ecard_prop;
#define PROP_TYPE_NORMAL   0x01
#define PROP_TYPE_LIST     0x02
#define PROP_TYPE_LISTITEM 0x03
	int prop_type;
	gboolean (*list_compare)(ECard *ecard, const char *str,
				 char *(*compare)(const char*, const char*));

} prop_info_table[] = {
	/* query prop,  ecard prop,   type,              list compare function */
	{ "file_as",    "file_as",    PROP_TYPE_NORMAL,  NULL },
	{ "full_name",  "full_name",  PROP_TYPE_NORMAL,  NULL },
	{ "url",        "url",        PROP_TYPE_NORMAL,  NULL },
	{ "mailer",     "mailer",     PROP_TYPE_NORMAL,  NULL },
	{ "org",        "org",        PROP_TYPE_NORMAL,  NULL },
	{ "org_unit",   "org_unit",   PROP_TYPE_NORMAL,  NULL },
	{ "office",     "office",     PROP_TYPE_NORMAL,  NULL },
	{ "title",      "title",      PROP_TYPE_NORMAL,  NULL },
	{ "role",       "role",       PROP_TYPE_NORMAL,  NULL },
	{ "manager",    "manager",    PROP_TYPE_NORMAL,  NULL },
	{ "assistant",  "assistant",  PROP_TYPE_NORMAL,  NULL },
	{ "nickname",   "nickname",   PROP_TYPE_NORMAL,  NULL },
	{ "spouse",     "spouse",     PROP_TYPE_NORMAL,  NULL },
	{ "email",      "email",      PROP_TYPE_LIST,    compare_email },
	{ "phone",      "phone",      PROP_TYPE_LIST,    compare_phone },
	{ "address",    "address",    PROP_TYPE_LIST,    compare_address },
	{ "note",       "note",       PROP_TYPE_NORMAL,  NULL },
};
static int num_prop_infos = sizeof(prop_info_table) / sizeof(prop_info_table[0]);

static ESExpResult *
entry_compare(PASBackendFileSearchContext *ctx, struct _ESExp *f,
	      int argc, struct _ESExpResult **argv,
	      char *(*compare)(const char*, const char*))
{
	ESExpResult *r;
	int truth = FALSE;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname;
		struct prop_info *info = NULL;
		int i;

		propname = argv[0]->value.string;

		for (i = 0; i < num_prop_infos; i ++) {
			if (!strcmp (prop_info_table[i].query_prop, propname)) {
				info = &prop_info_table[i];
				break;
			}
		}

		if (info) {
			if (info->prop_type == PROP_TYPE_NORMAL) {
				char *prop = NULL;
				/* searches where the query's property
                                   maps directly to an ecard property */

				gtk_object_get(GTK_OBJECT(ctx->ecard),
					       info->ecard_prop, &prop, NULL);

				if (prop && compare(prop, argv[1]->value.string)) {
					truth = TRUE;
				}
				if ((!prop) && compare("", argv[1]->value.string)) {
					truth = TRUE;
				}
			}
			else if (info->prop_type == PROP_TYPE_LIST) {
				/* the special searches that match any of the list elements */
				truth = info->list_compare (ctx->ecard, argv[1]->value.string, compare);
			}
		}
		
	}
	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = truth;

	return r;
}

static ESExpResult *
func_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendFileSearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, strstr);
}

static char *
is_helper (const char *s1, const char *s2)
{
	if (!strcmp(s1, s2))
		return (char*)s1;
	else
		return NULL;
}

static ESExpResult *
func_is(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendFileSearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, is_helper);
}

static char *
endswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = strstr(s1, s2))
	    && (strlen(p) == strlen(s2)))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_endswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendFileSearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, endswith_helper);
}

static char *
beginswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = strstr(s1, s2))
	    && (p == s1))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_beginswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	PASBackendFileSearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, beginswith_helper);
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
};

static gboolean
vcard_matches_search (PASBackendFileBookView *view, char *vcard_string)
{
	ESExpResult *r;
	gboolean retval;

	view->search_context->ecard = e_card_new (vcard_string);

	/* if it's not a valid vcard why is it in our db? :) */
	if (!view->search_context->ecard)
		return FALSE;

	r = e_sexp_eval(view->search_sexp);

	retval = (r && r->type == ESEXP_RES_BOOL && r->value.bool);

	gtk_object_unref(GTK_OBJECT(view->search_context->ecard));

	e_sexp_result_free(r);

	return retval;
}

static void
pas_backend_file_search (PASBackendFile  	*bf,
			 PASBook         	*book,
			 PASBackendFileBookView *view)
{
	int     db_error = 0;
	GList   *cards = NULL;
	DB      *db = bf->priv->file_db;
	DBT     id_dbt, vcard_dbt;
	int i;

	if (!bf->priv->loaded)
		return;

	view->search_sexp = e_sexp_new();
	view->search_context = g_new0(PASBackendFileSearchContext, 1);

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(view->search_sexp, 0, symbols[i].name,
					     (ESExpIFunc *)symbols[i].func, view->search_context);
		} else {
			e_sexp_add_function(view->search_sexp, 0, symbols[i].name,
					    symbols[i].func, view->search_context);
		}
	}

	e_sexp_input_text(view->search_sexp, view->search, strlen(view->search));
	e_sexp_parse(view->search_sexp);

	db_error = db->seq(db, &id_dbt, &vcard_dbt, R_FIRST);

	while (db_error == 0) {

		/* don't include the version in the list of cards */
		if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME) + 1
		    || strcmp (id_dbt.data, PAS_BACKEND_FILE_VERSION_NAME)) {
			char *vcard_string = vcard_dbt.data;

			/* check if the vcard matches the search sexp */
			if (vcard_matches_search (view, vcard_string)) {
				cards = g_list_append (cards, strdup(vcard_string));
			}
		}
		
		db_error = db->seq(db, &id_dbt, &vcard_dbt, R_NEXT);
	}

	if (db_error == -1) {
		g_warning ("pas_backend_file_search: error building list\n");
	}
	else {
		pas_book_view_notify_add (view->book_view, cards);
		pas_book_view_notify_complete (view->book_view);
	}

	/*
	** It's fine to do this now since the data has been handed off.
	*/
	g_list_foreach (cards, (GFunc)g_free, NULL);
	g_list_free (cards);
}

static char *
do_create(PASBackend *backend,
	  char       *vcard_req,
	  char      **vcard_ptr)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt, vcard_dbt;
	int            db_error;
	char           *id;
	ECard          *card;
	char           *vcard;
	char           *ret_val;

	id = pas_backend_file_create_unique_id (vcard_req);

	string_to_dbt (id, &id_dbt);
	
	card = e_card_new(vcard_req);
	e_card_set_id(card, id);
	vcard = e_card_get_vcard(card);

	string_to_dbt (vcard, &vcard_dbt);

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {
		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");
		ret_val = id;

	}
	else {
		ret_val = NULL;
	}

	gtk_object_unref(GTK_OBJECT(card));
	card = NULL;

	if (vcard_ptr && ret_val)
		*vcard_ptr = vcard;
	else
		g_free (vcard);

	return ret_val;
}

static void
pas_backend_file_process_create_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	char *id;
	char *vcard;
	GList          *list;
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);

	id = do_create(backend, req->vcard, &vcard);
	if (id) {
		for (list = bf->priv->book_views; list; list = g_list_next(list)) {
			PASBackendFileBookView *view = list->data;
			if (vcard_matches_search (view, vcard)) {
				pas_book_view_notify_add_1 (view->book_view, vcard);
				pas_book_view_notify_complete (view->book_view);
			}
		}

		pas_book_respond_create (
				 book,
				 Evolution_BookListener_Success,
				 id);
		g_free(vcard);
		g_free(id);
	}
	else {
		/* XXX need a different call status for this case, i
                   think */
		pas_book_respond_create (
				 book,
				 Evolution_BookListener_CardNotFound,
				 "");
	}

	g_free(req->vcard);
}

static void
pas_backend_file_process_remove_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	DB             *db = bf->priv->file_db;
	DBT            id_dbt, vcard_dbt;
	int            db_error;
	GList         *list;
	char          *vcard_string;

	string_to_dbt (req->id, &id_dbt);

	db_error = db->get (db, &id_dbt, &vcard_dbt, 0);
	if (0 != db_error) {
		pas_book_respond_remove (
				 book,
				 Evolution_BookListener_CardNotFound);
		g_free (req->id);
		return;
	}
	
	db_error = db->del (db, &id_dbt, 0);
	if (0 != db_error) {
		pas_book_respond_remove (
				 book,
				 Evolution_BookListener_CardNotFound);
		g_free (req->id);
		return;
	}

	db_error = db->sync (db, 0);
	if (db_error != 0)
		g_warning ("db->sync failed.\n");


	vcard_string = vcard_dbt.data;
	for (list = bf->priv->book_views; list; list = g_list_next(list)) {
		PASBackendFileBookView *view = list->data;
		if (vcard_matches_search (view, vcard_string)) {
			pas_book_view_notify_remove (view->book_view, req->id);
			pas_book_view_notify_complete (view->book_view);
		}
	}
	
	pas_book_respond_remove (
				 book,
				 Evolution_BookListener_Success);
	
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
	char          *old_vcard_string;

	/* create a new ecard from the request data */
	card = e_card_new(req->vcard);
	id = e_card_get_id(card);

	string_to_dbt (id, &id_dbt);	

	/* get the old ecard - the one that's presently in the db */
	db_error = db->get (db, &id_dbt, &vcard_dbt, 0);
	if (0 != db_error) {
		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_CardNotFound);
		g_free (req->id);
		return;
	}
	old_vcard_string = g_strdup(vcard_dbt.data);

	string_to_dbt (req->vcard, &vcard_dbt);	

	db_error = db->put (db, &id_dbt, &vcard_dbt, 0);

	if (0 == db_error) {
		db_error = db->sync (db, 0);
		if (db_error != 0)
			g_warning ("db->sync failed.\n");

		for (list = bf->priv->book_views; list; list = g_list_next(list)) {
			PASBackendFileBookView *view = list->data;
			gboolean old_match, new_match;

			old_match = vcard_matches_search (view, old_vcard_string);
			new_match = vcard_matches_search (view, req->vcard);
			if (old_match && new_match)
				pas_book_view_notify_change_1 (view->book_view, req->vcard);
			else if (new_match)
				pas_book_view_notify_add_1 (view->book_view, req->vcard);
			else /* if (old_match) */
				pas_book_view_notify_remove (view->book_view, id);
			pas_book_view_notify_complete (view->book_view);
		}

		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_Success);
	}
	else {
		pas_book_respond_modify (
				 book,
				 Evolution_BookListener_CardNotFound);
	}

	g_free(old_vcard_string);

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
		  if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME + 1)
		      || strcmp (id_dbt.data, PAS_BACKEND_FILE_VERSION_NAME)) {

			  cursor_data->elements = g_list_append(cursor_data->elements,
								g_strdup(vcard_dbt.data));

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
	CORBA_Environment ev;
	PASBookView       *book_view;
	Evolution_Book    corba_book;
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
		   (book_view != NULL
		    ? Evolution_BookListener_Success 
		    : Evolution_BookListener_CardNotFound /* XXX */),
		   book_view);

	view = g_new(PASBackendFileBookView, 1);
	view->book_view = book_view;
	view->search = g_strdup(req->search);

	bf->priv->book_views = g_list_prepend(bf->priv->book_views, view);

	pas_backend_file_search (bf, book, view);

	g_free(req->search);
}

static void
pas_backend_file_process_check_connection (PASBackend *backend,
					   PASBook    *book,
					   PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);

	pas_book_report_connection (book, bf->priv->file_db != NULL);
}

static char *
pas_backend_file_extract_path_from_uri (const char *uri)
{
	g_assert (strncasecmp (uri, "file:", 5) == 0);

	return g_strdup (uri + 5);
}

static gboolean
can_write (PASBackend *backend)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	char *path = pas_backend_file_extract_path_from_uri (bf->priv->uri);
	gboolean retval;

	retval = (access (path, W_OK) != -1);

	g_free (path);

	return retval;
}

static gboolean
pas_backend_file_can_write (PASBook *book)
{
	PASBackend* backend = pas_book_get_backend (book);

	return can_write(backend);
}

static gboolean
pas_backend_file_can_write_card (PASBook *book,
				 const char *id)
{
	PASBackend* backend = pas_book_get_backend (book);

	return can_write(backend);
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
pas_backend_file_book_destroy_cb (PASBook *book, gpointer data)
{
	PASBackendFile *backend;

	backend = PAS_BACKEND_FILE (data);

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
		return g_strdup (vcard_dbt.data);
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
		version = g_strdup (version_dbt.data);
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

#define INITIAL_VCARD "BEGIN:VCARD\n\
X-EVOLUTION-FILE-AS:Helix Code, Inc.\n\
LABEL;WORK;QUOTED-PRINTABLE:101 Rogers St. Ste. 214=0ACambridge, MA 02142=0AUSA\n\
TEL;WORK;VOICE:(617) 679-1984\n\
TEL;WORK;FAX:(617) 679-1949\n\
EMAIL;INTERNET:hello@helixcode.com\n\
URL:http://www.helixcode.com/\n\
ORG:Helix Code, Inc.;\n\
NOTE:Welcome to the Helix Code Addressbook.\n\
END:VCARD"

static gboolean
pas_backend_file_load_uri (PASBackend             *backend,
			   const char             *uri)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	char           *filename;

	g_assert (bf->priv->loaded == FALSE);

	filename = pas_backend_file_extract_path_from_uri (uri);

	bf->priv->file_db = dbopen (filename, O_RDWR, 0666, DB_HASH, NULL);
	if (bf->priv->file_db == NULL) {
		bf->priv->file_db = dbopen (filename, O_RDWR | O_CREAT, 0666, DB_HASH, NULL);

		if (bf->priv->file_db) {
			char *id;
			id = do_create(backend, INITIAL_VCARD, NULL);
			g_free (id);
		}
	}

	g_free (filename);

	if (bf->priv->file_db != NULL) {
		if (pas_backend_file_maybe_upgrade_db (bf))
			bf->priv->loaded = TRUE;
		/* XXX what if we fail to upgrade it? */
		
		g_free(bf->priv->uri);
		bf->priv->uri = g_strdup (uri);
	} else
		return FALSE;

	return TRUE;
}

/* Get_uri handler for the addressbook file backend */
static const char *
pas_backend_file_get_uri (PASBackend *backend)
{
	PASBackendFile *bf;

	bf = PAS_BACKEND_FILE (backend);

	g_return_val_if_fail (bf->priv->loaded, NULL);
	g_assert (bf->priv->uri != NULL);

	return bf->priv->uri;
}

static gboolean
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
		pas_backend_file_get_vcard,
		pas_backend_file_can_write,
		pas_backend_file_can_write_card);

	if (!book) {
		if (!bf->priv->clients)
			pas_backend_last_client_gone (backend);

		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (book), "destroy",
			    pas_backend_file_book_destroy_cb, backend);

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

	return TRUE;
}

static void
pas_backend_file_remove_client (PASBackend             *backend,
				PASBook                *book)
{
	PASBackendFile *bf;
	GList *l;
	PASBook *lbook;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND_FILE (backend));
	g_return_if_fail (book != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));

	bf = PAS_BACKEND_FILE (backend);

	/* Find the book in the list of clients */

	for (l = bf->priv->clients; l; l = l->next) {
		lbook = PAS_BOOK (l->data);

		if (lbook == book)
			break;
	}

	g_assert (l != NULL);

	/* Disconnect */

	bf->priv->clients = g_list_remove_link (bf->priv->clients, l);
	g_list_free_1 (l);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!bf->priv->clients)
		pas_backend_last_client_gone (backend);
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
	PASBackendFile *bf;

	bf = PAS_BACKEND_FILE (object);

	g_free (bf->priv->uri);

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
	parent_class->get_uri       = pas_backend_file_get_uri;
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
	priv->uri        = NULL;

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

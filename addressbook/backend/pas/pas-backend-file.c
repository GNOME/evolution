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
#ifdef HAVE_DB1_DB_H
#include <db1/db.h>
#else
#include <db.h>
#endif
#endif

#include "pas-backend-file.h"
#include "pas-book.h"
#include "pas-card-cursor.h"
#include <ebook/e-card-simple.h>
#include <e-util/e-sexp.h>
#include <e-util/e-dbhash.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#define PAS_BACKEND_FILE_VERSION_NAME "PAS-DB-VERSION"
#define PAS_BACKEND_FILE_VERSION "0.1"

static PASBackendClass *pas_backend_file_parent_class;
typedef struct _PASBackendFileCursorPrivate PASBackendFileCursorPrivate;
typedef struct _PASBackendFileBookView PASBackendFileBookView;
typedef struct _PASBackendFileSearchContext PASBackendFileSearchContext;
typedef struct _PasBackendFileChangeContext PASBackendFileChangeContext;

struct _PASBackendFilePrivate {
	GList    *clients;
	gboolean  loaded;
	char     *uri;
	DB       *file_db;
	EList    *book_views;
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
	gchar                       *change_id;
	PASBackendFileChangeContext *change_context;
};

struct _PASBackendFileSearchContext {
	ECardSimple *card;
};

struct _PasBackendFileChangeContext {
	DB *db;

	GList *add_cards;
	GList *add_ids;
	GList *mod_cards;
	GList *mod_ids;
	GList *del_cards;
	GList *del_ids;
};

static PASBackendFileBookView *
pas_backend_file_book_view_copy(const PASBackendFileBookView *book_view, void *closure)
{
	PASBackendFileBookView *new_book_view;
	new_book_view = g_new (PASBackendFileBookView, 1);
	new_book_view->book_view = book_view->book_view;

	new_book_view->search = g_strdup(book_view->search);
	new_book_view->search_sexp = book_view->search_sexp;
	if (new_book_view->search_sexp)
		gtk_object_ref(GTK_OBJECT(new_book_view->search_sexp));
	if (book_view->search_context) {		
		new_book_view->search_context = g_new(PASBackendFileSearchContext, 1);
		new_book_view->search_context->card = book_view->search_context->card;
	} else
		new_book_view->search_context = NULL;
	
	new_book_view->change_id = g_strdup(book_view->change_id);
	if (book_view->change_context) {
		new_book_view->change_context = g_new(PASBackendFileChangeContext, 1);
		new_book_view->change_context->db = book_view->change_context->db;
		new_book_view->change_context->add_cards = book_view->change_context->add_cards;
		new_book_view->change_context->add_ids = book_view->change_context->add_ids;
		new_book_view->change_context->mod_cards = book_view->change_context->mod_cards;
		new_book_view->change_context->mod_ids = book_view->change_context->mod_ids;
		new_book_view->change_context->del_cards = book_view->change_context->del_cards;
		new_book_view->change_context->del_ids = book_view->change_context->del_ids;
	} else
		new_book_view->change_context = NULL;
	
	return new_book_view;
}

static void
pas_backend_file_book_view_free(PASBackendFileBookView *book_view, void *closure)
{
	g_free(book_view->search);
	if (book_view->search_sexp)
		gtk_object_unref(GTK_OBJECT(book_view->search_sexp));
	g_free(book_view->search_context);

	g_free(book_view->change_id);
	if (book_view->change_context) {
		g_list_foreach (book_view->change_context->add_cards, (GFunc)g_free, NULL);
		g_list_foreach (book_view->change_context->add_ids, (GFunc)g_free, NULL);
		g_list_foreach (book_view->change_context->mod_cards, (GFunc)g_free, NULL);
		g_list_foreach (book_view->change_context->mod_ids, (GFunc)g_free, NULL);
		g_list_foreach (book_view->change_context->del_cards, (GFunc)g_free, NULL);
		g_list_foreach (book_view->change_context->del_ids, (GFunc)g_free, NULL);
		g_list_free (book_view->change_context->add_cards);
		g_list_free (book_view->change_context->add_ids);
		g_list_free (book_view->change_context->mod_cards);
		g_list_free (book_view->change_context->mod_ids);
		g_list_free (book_view->change_context->del_cards);
		g_list_free (book_view->change_context->del_ids);
	}
	g_free(book_view->change_context);

	g_free(book_view);
}

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
	GNOME_Evolution_Addressbook_Book corba_book;
	PASBackendFileCursorPrivate *cursor_data = (PASBackendFileCursorPrivate *) data;

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(cursor_data->book));

	CORBA_exception_init(&ev);

	GNOME_Evolution_Addressbook_Book_unref(corba_book, &ev);
	
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
	GNOME_Evolution_Addressbook_Book    corba_book;
	PASBook           *book = (PASBook *)data;
	PASBackendFile    *bf;
	EIterator         *iterator;

	bf = PAS_BACKEND_FILE(pas_book_get_backend(book));
	for (iterator = e_list_get_iterator(bf->priv->book_views); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const PASBackendFileBookView *view = e_iterator_get(iterator);
		if (view->book_view == PAS_BOOK_VIEW(object)) {
			e_iterator_delete(iterator);
			break;
		}
	}
	gtk_object_unref(GTK_OBJECT(iterator));

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	GNOME_Evolution_Addressbook_Book_unref(corba_book, &ev);
	
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
compare_email (ECardSimple *card, const char *str,
	       char *(*compare)(const char*, const char*))
{
	int i;

	for (i = E_CARD_SIMPLE_EMAIL_ID_EMAIL; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
		const char *email = e_card_simple_get_email (card, i);

		if (email && compare(email, str))
			return TRUE;
	}

	return FALSE;
}

static gboolean
compare_phone (ECardSimple *card, const char *str,
	       char *(*compare)(const char*, const char*))
{
	int i;

	for (i = E_CARD_SIMPLE_PHONE_ID_ASSISTANT; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
		const ECardPhone *phone = e_card_simple_get_phone (card, i);

		if (phone && compare(phone->number, str))
			return TRUE;
	}

	return FALSE;
}

static gboolean
compare_address (ECardSimple *card, const char *str,
		 char *(*compare)(const char*, const char*))
{
	g_warning("address searching not implemented\n");
	return FALSE;
}

static struct prop_info {
	ECardSimpleField field_id;
	const char *query_prop;
	const char *ecard_prop;
#define PROP_TYPE_NORMAL   0x01
#define PROP_TYPE_LIST     0x02
#define PROP_TYPE_LISTITEM 0x03
	int prop_type;
	gboolean (*list_compare)(ECardSimple *ecard, const char *str,
				 char *(*compare)(const char*, const char*));

} prop_info_table[] = {
#define NORMAL_PROP(f,q,e) {f, q, e, PROP_TYPE_NORMAL, NULL}
#define LIST_PROP(q,e,c) {0, q, e, PROP_TYPE_LIST, c}

	/* query prop,  ecard prop,   type,              list compare function */
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_FILE_AS, "file_as", "file_as" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_FULL_NAME, "full_name",  "full_name" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_URL, "url", "url" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_MAILER, "mailer", "mailer"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ORG, "org", "org"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ORG_UNIT, "org_unit", "org_unit"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_OFFICE, "office", "office"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_TITLE, "title", "title"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ROLE, "role", "role"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_MANAGER, "manager", "manager"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ASSISTANT, "assistant", "assistant"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_NICKNAME, "nickname", "nickname"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_SPOUSE, "spouse", "spouse" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_NOTE, "note", "note"),
	LIST_PROP ( "email", "email", compare_email ),
	LIST_PROP ( "phone", "phone", compare_phone ),
	LIST_PROP ( "address", "address", compare_address ),
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
		gboolean any_field;

		propname = argv[0]->value.string;

		any_field = !strcmp(propname, "x-evolution-any-field");
		for (i = 0; i < num_prop_infos; i ++) {
			if (any_field
			    || !strcmp (prop_info_table[i].query_prop, propname)) {
				info = &prop_info_table[i];
				
				if (info->prop_type == PROP_TYPE_NORMAL) {
					char *prop = NULL;
					/* searches where the query's property
					   maps directly to an ecard property */
					
					prop = e_card_simple_get (ctx->card, info->field_id);

					if (prop && compare(prop, argv[1]->value.string)) {
						truth = TRUE;
					}
					if ((!prop) && compare("", argv[1]->value.string)) {
						truth = TRUE;
					}
				}
				else if (info->prop_type == PROP_TYPE_LIST) {
				/* the special searches that match any of the list elements */
					truth = info->list_compare (ctx->card, argv[1]->value.string, compare);
				}

				/* if we're looking at all fields and find a match,
				   or if we're just looking at this one field,
				   break. */
				if ((any_field && truth)
				    || !any_field)
					break;
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

	return entry_compare (ctx, f, argc, argv, (char *(*)(const char*, const char*)) e_utf8_strstrcase);
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
vcard_matches_search (const PASBackendFileBookView *view, char *vcard_string)
{
	ESExpResult *r;
	gboolean retval;
	ECard *card;

	card = e_card_new (vcard_string);
	view->search_context->card = e_card_simple_new (card);
	gtk_object_unref(GTK_OBJECT(card));

	/* if it's not a valid vcard why is it in our db? :) */
	if (!view->search_context->card)
		return FALSE;

	r = e_sexp_eval(view->search_sexp);

	retval = (r && r->type == ESEXP_RES_BOOL && r->value.bool);


	gtk_object_unref(GTK_OBJECT(view->search_context->card));

	e_sexp_result_free(r);

	return retval;
}

static void
pas_backend_file_search (PASBackendFile  	      *bf,
			 PASBook         	      *book,
			 const PASBackendFileBookView *cnstview)
{
	int     db_error = 0;
	GList   *cards = NULL;
	DB      *db = bf->priv->file_db;
	DBT     id_dbt, vcard_dbt;
	int i;
	PASBackendFileBookView *view = (PASBackendFileBookView *)cnstview;

	if (!bf->priv->loaded)
		return;

	if (view->search_sexp)
		gtk_object_unref(GTK_OBJECT(view->search_sexp));
	view->search_sexp = e_sexp_new();

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

static void
pas_backend_file_changes_foreach_key (const char *key, gpointer user_data)
{
	PASBackendFileChangeContext *ctx = user_data;
	DB      *db = ctx->db;
	DBT     id_dbt, vcard_dbt;
	int     db_error = 0;
	
	string_to_dbt (key, &id_dbt);
	db_error = db->get (db, &id_dbt, &vcard_dbt, 0);
	
	if (db_error == 1) {
		ECard *ecard;
		char *id = id_dbt.data;
		
		ecard = e_card_new ("");
		e_card_set_id (ecard, id);
		
		ctx->del_cards = g_list_append (ctx->del_cards, e_card_get_vcard (ecard));
		ctx->del_ids = g_list_append (ctx->del_ids, strdup(id));
	}
}

static void
pas_backend_file_changes (PASBackendFile  	      *bf,
			  PASBook         	      *book,
			  const PASBackendFileBookView *cnstview)
{
	int     db_error = 0;
	DBT     id_dbt, vcard_dbt;
	char    *filename;
	EDbHash *ehash;
	GList *i, *v;
	DB      *db = bf->priv->file_db;
	PASBackendFileBookView *view = (PASBackendFileBookView *)cnstview;
	PASBackendFileChangeContext *ctx = cnstview->change_context;

	if (!bf->priv->loaded)
		return;

	/* Find the changed ids - FIX ME, patch should not be hard coded */
	filename = g_strdup_printf ("%s/evolution/local/Contacts/%s.db", g_get_home_dir (), view->change_id);
	ehash = e_dbhash_new (filename);
	g_free (filename);
	
	db_error = db->seq(db, &id_dbt, &vcard_dbt, R_FIRST);

	while (db_error == 0) {

		/* don't include the version in the list of cards */
		if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME) + 1
		    || strcmp (id_dbt.data, PAS_BACKEND_FILE_VERSION_NAME)) {
			char *id = id_dbt.data;
			char *vcard_string = vcard_dbt.data;

			/* check what type of change has occurred, if any */
			switch (e_dbhash_compare (ehash, id, vcard_string)) {
			case E_DBHASH_STATUS_SAME:
				break;
			case E_DBHASH_STATUS_NOT_FOUND:
				ctx->add_cards = g_list_append (ctx->add_cards, strdup(vcard_string));
				ctx->add_ids = g_list_append (ctx->add_ids, strdup(id));
				break;
			case E_DBHASH_STATUS_DIFFERENT:
				ctx->mod_cards = g_list_append (ctx->mod_cards, strdup(vcard_string));
				ctx->mod_ids = g_list_append (ctx->mod_ids, strdup(id));
				break;
			}
		}
		
		db_error = db->seq(db, &id_dbt, &vcard_dbt, R_NEXT);
	}

   	e_dbhash_foreach_key (ehash, (EDbHashFunc)pas_backend_file_changes_foreach_key, view->change_context);

	/* Update the hash */
	for (i = ctx->add_ids, v = ctx->add_cards; i != NULL; i = i->next, v = v->next){
		char *id = i->data;
		char *vcard = v->data;
		e_dbhash_add (ehash, id, vcard);
	}	
	for (i = ctx->mod_ids, v = ctx->mod_cards; i != NULL; i = i->next, v = v->next){
		char *id = i->data;
		char *vcard = v->data;
		e_dbhash_add (ehash, id, vcard);
	}	
	for (i = ctx->del_ids; i != NULL; i = i->next){
		char *id = i->data;
		e_dbhash_remove (ehash, id);
	}	

  	e_dbhash_write (ehash);
  	e_dbhash_destroy (ehash);

	/* Send the changes */
	if (db_error == -1) {
		g_warning ("pas_backend_file_changes: error building list\n");
	} else {
  		if (ctx->add_cards != NULL)
  			pas_book_view_notify_add (view->book_view, ctx->add_cards);
		
		if (ctx->mod_cards != NULL)
			pas_book_view_notify_change (view->book_view, ctx->mod_cards);

		for (v = ctx->del_cards; v != NULL; v = v->next){
			char *vcard = v->data;
			pas_book_view_notify_remove (view->book_view, vcard);
		}
		
		pas_book_view_notify_complete (view->book_view);
	}
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
	EIterator *iterator;
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);

	id = do_create(backend, req->vcard, &vcard);
	if (id) {
		for (iterator = e_list_get_iterator(bf->priv->book_views); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const PASBackendFileBookView *view = e_iterator_get(iterator);
			if (vcard_matches_search (view, vcard)) {
				pas_book_view_notify_add_1 (view->book_view, vcard);
				pas_book_view_notify_complete (view->book_view);
			}
		}
		gtk_object_unref(GTK_OBJECT(iterator));
		
		pas_book_respond_create (
			book,
			GNOME_Evolution_Addressbook_BookListener_Success,
			id);
		g_free(vcard);
		g_free(id);
	}
	else {
		/* XXX need a different call status for this case, i
                   think */
		pas_book_respond_create (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_CardNotFound,
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
	EIterator     *iterator;
	char          *vcard_string;

	string_to_dbt (req->id, &id_dbt);

	db_error = db->get (db, &id_dbt, &vcard_dbt, 0);
	if (0 != db_error) {
		pas_book_respond_remove (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_CardNotFound);
		g_free (req->id);
		return;
	}
	
	db_error = db->del (db, &id_dbt, 0);
	if (0 != db_error) {
		pas_book_respond_remove (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_CardNotFound);
		g_free (req->id);
		return;
	}

	db_error = db->sync (db, 0);
	if (db_error != 0)
		g_warning ("db->sync failed.\n");


	vcard_string = vcard_dbt.data;
	for (iterator = e_list_get_iterator (bf->priv->book_views); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const PASBackendFileBookView *view = e_iterator_get(iterator);
		if (vcard_matches_search (view, vcard_string)) {
			pas_book_view_notify_remove (view->book_view, req->id);
			pas_book_view_notify_complete (view->book_view);
		}
	}
	gtk_object_unref(GTK_OBJECT(iterator));
	
	pas_book_respond_remove (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_Success);
	
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
	EIterator     *iterator;
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
				 GNOME_Evolution_Addressbook_BookListener_CardNotFound);
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

		for (iterator = e_list_get_iterator(bf->priv->book_views); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			CORBA_Environment ev;
			const PASBackendFileBookView *view = e_iterator_get(iterator);
			gboolean old_match, new_match;

			CORBA_exception_init(&ev);

			bonobo_object_dup_ref(bonobo_object_corba_objref(BONOBO_OBJECT(view->book_view)), &ev);

			old_match = vcard_matches_search (view, old_vcard_string);
			new_match = vcard_matches_search (view, req->vcard);
			if (old_match && new_match)
				pas_book_view_notify_change_1 (view->book_view, req->vcard);
			else if (new_match)
				pas_book_view_notify_add_1 (view->book_view, req->vcard);
			else /* if (old_match) */
				pas_book_view_notify_remove (view->book_view, id);
			pas_book_view_notify_complete (view->book_view);

			bonobo_object_release_unref(bonobo_object_corba_objref(BONOBO_OBJECT(view->book_view)), &ev);
		}
		gtk_object_unref(GTK_OBJECT(iterator));

		pas_book_respond_modify (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_Success);
	}
	else {
		pas_book_respond_modify (
				 book,
				 GNOME_Evolution_Addressbook_BookListener_CardNotFound);
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
		  if (id_dbt.size != strlen(PAS_BACKEND_FILE_VERSION_NAME) + 1
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
	GNOME_Evolution_Addressbook_Book corba_book;

	cursor_data = g_new(PASBackendFileCursorPrivate, 1);
	cursor_data->backend = backend;
	cursor_data->book = book;

	pas_backend_file_build_all_cards_list(backend, cursor_data);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	GNOME_Evolution_Addressbook_Book_ref(corba_book, &ev);
	
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
		 ? GNOME_Evolution_Addressbook_BookListener_Success 
		 : GNOME_Evolution_Addressbook_BookListener_CardNotFound),
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
	GNOME_Evolution_Addressbook_Book    corba_book;
	PASBackendFileBookView view;
	PASBackendFileSearchContext ctx;
	EIterator *iterator;

	g_return_if_fail (req->listener != NULL);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	GNOME_Evolution_Addressbook_Book_ref(corba_book, &ev);
	
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
		    ? GNOME_Evolution_Addressbook_BookListener_Success 
		    : GNOME_Evolution_Addressbook_BookListener_CardNotFound /* XXX */),
		   book_view);

	view.book_view = book_view;
	view.search = req->search;
	view.search_sexp = NULL;
	view.search_context = &ctx;
	view.change_context = NULL;
	ctx.card = NULL;

	e_list_append(bf->priv->book_views, &view);

	iterator = e_list_get_iterator(bf->priv->book_views);
	e_iterator_last(iterator);
	pas_backend_file_search (bf, book, e_iterator_get(iterator));
	gtk_object_unref(GTK_OBJECT(iterator));

	g_free(req->search);
}

static void
pas_backend_file_process_get_changes (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	PASBackendFile *bf = PAS_BACKEND_FILE (backend);
	CORBA_Environment ev;
	PASBookView       *book_view;
	GNOME_Evolution_Addressbook_Book    corba_book;
	PASBackendFileBookView view;
	PASBackendFileChangeContext ctx;
	EIterator *iterator;

	g_return_if_fail (req->listener != NULL);

	corba_book = bonobo_object_corba_objref(BONOBO_OBJECT(book));

	CORBA_exception_init(&ev);

	GNOME_Evolution_Addressbook_Book_ref(corba_book, &ev);
	
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning("pas_backend_file_process_get_book_view: Exception reffing "
			  "corba book.\n");
	}

	CORBA_exception_free(&ev);

	book_view = pas_book_view_new (req->listener);

	gtk_signal_connect(GTK_OBJECT(book_view), "destroy",
			   GTK_SIGNAL_FUNC(view_destroy), book);

	pas_book_respond_get_changes (book,
		   (book_view != NULL
		    ? GNOME_Evolution_Addressbook_BookListener_Success 
		    : GNOME_Evolution_Addressbook_BookListener_CardNotFound /* XXX */),
		   book_view);

	view.book_view = book_view;
	view.change_id = req->change_id;
	view.change_context = &ctx;
	ctx.db = bf->priv->file_db;
	ctx.add_cards = NULL;
	ctx.add_ids = NULL;
	ctx.mod_cards = NULL;
	ctx.mod_ids = NULL;
	ctx.del_cards = NULL;
	ctx.del_ids = NULL;
	view.search = NULL;
	view.search_sexp = NULL;
	view.search_context = NULL;

	e_list_append(bf->priv->book_views, &view);

	iterator = e_list_get_iterator(bf->priv->book_views);
	e_iterator_last(iterator);
	pas_backend_file_changes (bf, book, e_iterator_get(iterator));
	gtk_object_unref(GTK_OBJECT(iterator));

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

	case GetChanges:
		pas_backend_file_process_get_changes (backend, book, req);
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
			     GNOME_Evolution_Addressbook_BookListener  listener)
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
			book, GNOME_Evolution_Addressbook_BookListener_Success);
	} else {
		/* Open the book. */
		pas_book_respond_open (
			book, GNOME_Evolution_Addressbook_BookListener_Success);
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

static char *
pas_backend_file_get_static_capabilities (PASBackend             *backend)
{
	return g_strdup("local");
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

	gtk_object_unref(GTK_OBJECT(bf->priv->book_views));
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
	parent_class->load_uri                = pas_backend_file_load_uri;
	parent_class->get_uri                 = pas_backend_file_get_uri;
	parent_class->add_client              = pas_backend_file_add_client;
	parent_class->remove_client           = pas_backend_file_remove_client;
	parent_class->get_static_capabilities = pas_backend_file_get_static_capabilities;

	object_class->destroy = pas_backend_file_destroy;
}

static void
pas_backend_file_init (PASBackendFile *backend)
{
	PASBackendFilePrivate *priv;

	priv             = g_new0 (PASBackendFilePrivate, 1);
	priv->loaded     = FALSE;
	priv->clients    = NULL;
	priv->book_views = e_list_new((EListCopyFunc) pas_backend_file_book_view_copy, (EListFreeFunc) pas_backend_file_book_view_free, NULL);
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

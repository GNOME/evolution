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

#include <e-util/e-sexp.h>
#include <ebook/e-card-simple.h>

#define LDAP_MAX_SEARCH_RESPONSES 500

static gchar *map_e_card_prop_to_ldap(gchar *e_card_prop);

static PASBackendClass *pas_backend_ldap_parent_class;
typedef struct _PASBackendLDAPCursorPrivate PASBackendLDAPCursorPrivate;
typedef struct _PASBackendLDAPBookView PASBackendLDAPBookView;

struct _PASBackendLDAPPrivate {
	char     *uri;
	gboolean connected;
	GList    *clients;
	LDAP     *ldap;
	gchar    *ldap_host;
	gchar    *ldap_rootdn;
	int      ldap_port;
	GList    *book_views;
};

struct _PASBackendLDAPCursorPrivate {
	PASBackend *backend;
	PASBook    *book;

	GList      *elements;
	long       num_elements;
};

struct _PASBackendLDAPBookView {
	PASBookView           *book_view;
	PASBackendLDAPPrivate *blpriv;
	gchar                 *search;
	int                   search_idle;
	int                   search_msgid;
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

static void
view_destroy(GtkObject *object, gpointer data)
{
	CORBA_Environment ev;
	Evolution_Book    corba_book;
	PASBook           *book = (PASBook *)data;
	PASBackendLDAP    *bl;
	GList             *list;

	bl = PAS_BACKEND_LDAP(pas_book_get_backend(book));
	for (list = bl->priv->book_views; list; list = g_list_next(list)) {
		PASBackendLDAPBookView *view = list->data;
		if (view->book_view == PAS_BOOK_VIEW(object)) {
			g_free (view->search);
			g_free (view);
			if (view->search_idle != 0)
				g_source_remove(view->search_idle);
			bl->priv->book_views = g_list_remove_link(bl->priv->book_views, list);
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
pas_backend_ldap_ensure_connected (PASBackendLDAP *bl)
{
	LDAP           *ldap = bl->priv->ldap;

	/* the connection has gone down, or wasn't ever opened */
	if (ldap == NULL ||
	    (ldap_simple_bind_s(ldap, NULL /*binddn*/, NULL /*passwd*/) != LDAP_SUCCESS)) {

		/* close connection first if it's open first */
		if (ldap)
			ldap_unbind (ldap);

		bl->priv->ldap = ldap_open (bl->priv->ldap_host, bl->priv->ldap_port);
		if (NULL != bl->priv->ldap) {
			ldap_simple_bind_s(bl->priv->ldap,
					   NULL /*binddn*/, NULL /*passwd*/);
			bl->priv->connected = TRUE;
		}
		else
			g_warning ("pas_backend_ldap_ensure_connected failed for "
				   "'ldap://%s:%d/%s' (error %s)\n",
				   bl->priv->ldap_host,
				   bl->priv->ldap_port,
				   bl->priv->ldap_rootdn ? bl->priv->ldap_rootdn : "",
				   
				   ldap_err2string(ldap->ld_errno));

	}
}

static void
pas_backend_ldap_process_create_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	g_warning ("pas_backend_ldap_process_create_card not implemented\n");

	pas_book_respond_create (
				 book,
				 Evolution_BookListener_CardNotFound,
				 "");

	g_free (req->vcard);
}

static void
pas_backend_ldap_process_remove_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	g_warning ("pas_backend_ldap_process_remove_card not implemented\n");

	pas_book_respond_remove (
				 book,
				 Evolution_BookListener_CardNotFound);

	g_free (req->id);
}

static void
pas_backend_ldap_build_all_cards_list(PASBackend *backend,
				      PASBackendLDAPCursorPrivate *cursor_data)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAP           *ldap;
	int            ldap_error;
	LDAPMessage    *res, *e;

	pas_backend_ldap_ensure_connected(bl);

	ldap = bl->priv->ldap;

	if (ldap) {
		ldap->ld_sizelimit = LDAP_MAX_SEARCH_RESPONSES;
		ldap->ld_deref = LDAP_DEREF_ALWAYS;

		if ((ldap_error = ldap_search_s (ldap,
						 bl->priv->ldap_rootdn,
						 LDAP_SCOPE_ONELEVEL,
						 "(objectclass=*)",
						 NULL, 0, &res)) == -1) {
			g_warning ("ldap error '%s' in "
				   "pas_backend_ldap_build_all_cards_list\n",
				   ldap_err2string(ldap_error));
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
}

static void
pas_backend_ldap_process_modify_card (PASBackend *backend,
				      PASBook    *book,
				      PASRequest *req)
{
	g_warning ("pas_backend_ldap_process_modify_card not implemented\n");

	pas_book_respond_modify (
				 book,
				 Evolution_BookListener_CardNotFound);
	g_free (req->vcard);
}

static void
pas_backend_ldap_process_get_cursor (PASBackend *backend,
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
		(ldap_error == 0 
		 ? Evolution_BookListener_Success 
		 : Evolution_BookListener_CardNotFound),
		cursor);
}

static ESExpResult *
func_and(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	char ** strings;

	if (argc > 0) {
		int i;

		strings = g_new(char*, argc+3);
		strings[0] = g_strdup ("(&");
		strings[argc+3 - 2] = g_strdup (")");
		strings[argc+3 - 1] = NULL;
		
		for (i = 0; i < argc; i ++) {
			GList *list_head = *list;
			strings[argc - i] = (*list)->data;
			*list = g_list_remove_link(*list, *list);
			g_list_free_1(list_head);
		}

		*list = g_list_prepend(*list, g_strjoinv(" ", strings));

		for (i = 0 ; i < argc + 2; i ++)
			g_free (strings[i]);

		g_free (strings);
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_or(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	char ** strings;

	if (argc > 0) {
		int i;

		strings = g_new(char*, argc+3);
		strings[0] = g_strdup ("(|");
		strings[argc+3 - 2] = g_strdup (")");
		strings[argc+3 - 1] = NULL;
		for (i = 0; i < argc; i ++) {
			GList *list_head = *list;
			strings[argc - i] = (*list)->data;
			*list = g_list_remove_link(*list, *list);
			g_list_free_1(list_head);
		}

		*list = g_list_prepend(*list, g_strjoinv(" ", strings));

		for (i = 0 ; i < argc + 2; i ++)
			g_free (strings[i]);

		g_free (strings);
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_not(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	/* just replace the head of the list with the NOT of it. */
	if (argc > 0) {
		char *term = (*list)->data;
		(*list)->data = g_strdup_printf("(!%s)", term);
		g_free (term);
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *ldap_attr = map_e_card_prop_to_ldap(propname);
		gboolean one_star = FALSE;

		if (strlen(str) == 0)
			one_star = TRUE;

		if (ldap_attr)
			*list = g_list_prepend(*list,
					       g_strdup_printf("(%s=*%s%s)",
							       ldap_attr,
							       str,
							       one_star ? "" : "*"));
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_is(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *ldap_attr = map_e_card_prop_to_ldap(propname);

		if (ldap_attr)
			*list = g_list_prepend(*list,
					       g_strdup_printf("(%s=%s)",
							       ldap_attr, str));
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_beginswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *ldap_attr = map_e_card_prop_to_ldap(propname);
		gboolean one_star = FALSE;

		if (strlen(str) == 0)
			one_star = TRUE;

		if (ldap_attr)
			*list = g_list_prepend(*list,
					       g_strdup_printf("(%s=%s*)",
							       ldap_attr,
							       str));
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_endswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		char *ldap_attr = map_e_card_prop_to_ldap(propname);
		gboolean one_star = FALSE;

		if (strlen(str) == 0)
			one_star = TRUE;

		if (ldap_attr)
			*list = g_list_prepend(*list,
					       g_strdup_printf("(%s=*%s)",
							       ldap_attr,
							       str));
	}

	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "and", func_and, 0 },
	{ "or", func_or, 0 },
	{ "not", func_not, 0 },
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
};

static gchar *
pas_backend_ldap_build_query (gchar *query)
{
	ESExp *sexp;
	ESExpResult *r;
	gchar *retval;
	GList *list = NULL;
	int i;

	sexp = e_sexp_new();

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(sexp, 0, symbols[i].name,
					     (ESExpIFunc *)symbols[i].func, &list);
		} else {
			e_sexp_add_function(sexp, 0, symbols[i].name,
					    symbols[i].func, &list);
		}
	}

	e_sexp_input_text(sexp, query, strlen(query));
	e_sexp_parse(sexp);

	r = e_sexp_eval(sexp);

	gtk_object_unref(GTK_OBJECT(sexp));
	e_sexp_result_free(r);

	if (list->next) {
		g_warning ("conversion to ldap query string failed");
		retval = NULL;
		g_list_foreach (list, (GFunc)g_free, NULL);
	}
	else {
		retval = list->data;
	}

	g_list_free (list);
	return retval;
}

static void
construct_email_list(ECardSimple *card, const char *prop, char **values)
{
	int i;

	for (i = 0; values[i] && i < 3; i ++) {
		e_card_simple_set_email (card, i, values[i]);
	}
}

struct prop_info {
	ECardSimpleField field_id;
	char *query_prop;
	char *ldap_attr;
#define PROP_TYPE_NORMAL   0x01
#define PROP_TYPE_LIST     0x02
#define PROP_TYPE_LISTITEM 0x03
	int prop_type;
	void (*construct_list_func)(ECardSimple *card, const char *prop, char **values);
} prop_info_table[] = {
	/* field_id,                     query prop,   ldap attr,         type,           list construct function */
	{ E_CARD_SIMPLE_FIELD_FULL_NAME, "full_name",  "cn",              PROP_TYPE_NORMAL,  NULL },
	{ E_CARD_SIMPLE_FIELD_TITLE,     "title",      "title",           PROP_TYPE_NORMAL,  NULL },
	{ E_CARD_SIMPLE_FIELD_ORG_UNIT,  "org",        "o",               PROP_TYPE_NORMAL,  NULL },
	{ E_CARD_SIMPLE_FIELD_PHONE_PRIMARY, "phone",  "telephonenumber", PROP_TYPE_NORMAL,    NULL },
	{ 0 /* unused */,                "email",      "mail",            PROP_TYPE_LIST,    construct_email_list },
};

static int num_prop_infos = sizeof(prop_info_table) / sizeof(prop_info_table[0]);

static gchar *
map_e_card_prop_to_ldap(gchar *e_card_prop)
{
	int i;

	for (i = 0; i < num_prop_infos; i ++)
		if (!strcmp (e_card_prop, prop_info_table[i].query_prop))
			return prop_info_table[i].ldap_attr;

	return NULL;
}

static gboolean
poll_ldap (PASBackendLDAPBookView *view)
{
	LDAP           *ldap;
	int            rc;
	LDAPMessage    *res, *e;
	GList   *cards = NULL;

	printf ("polling ldap server\n");

	ldap = view->blpriv->ldap;
		
	if ((rc = ldap_result (ldap, view->search_msgid, 0, NULL, &res))
	    != LDAP_RES_SEARCH_ENTRY) {
		view->search_idle = 0;
		return FALSE;
	}
		
	e = ldap_first_entry(ldap, res);

	while (NULL != e) {
		ECard *ecard = E_CARD(gtk_type_new(e_card_get_type()));
		ECardSimple *card = e_card_simple_new (ecard);
		char *dn = ldap_get_dn(ldap, e);
		char *attr;
		BerElement *ber = NULL;

		e_card_simple_set_id (card, dn);

		for (attr = ldap_first_attribute (ldap, e, &ber); attr;
		     attr = ldap_next_attribute (ldap, e, ber)) {
			int i;
			struct prop_info *info = NULL;

			for (i = 0; i < num_prop_infos; i ++)
				if (!strcmp (attr, prop_info_table[i].ldap_attr))
					info = &prop_info_table[i];

			if (info) {
				char **values;
				values = ldap_get_values (ldap, e, attr);

				if (info->prop_type == PROP_TYPE_NORMAL) {
					/* if it's a normal property just set the string */
					e_card_simple_set (card, info->field_id, values[0]);

				}
				else if (info->prop_type == PROP_TYPE_LIST) {
					/* if it's a list call the construction function,
					   which calls gtk_object_set to set the property */
					info->construct_list_func(card,
								  info->query_prop,
								  values);
				}

				ldap_value_free (values);
			}
		}

		/* if ldap->ld_errno == LDAP_DECODING_ERROR there was an
		   error decoding an attribute, and we shouldn't free ber,
		   since the ldap library already did it. */
		if (ldap->ld_errno != LDAP_DECODING_ERROR && ber)
			ber_free (ber, 0);

		e_card_simple_sync_card (card);
		cards = g_list_append (cards, e_card_simple_get_vcard (card));

		gtk_object_unref (GTK_OBJECT(card));

		e = ldap_next_entry(ldap, e);
	}

	if (cards) {
		pas_book_view_notify_add (view->book_view, cards);
			
		g_list_foreach (cards, (GFunc)g_free, NULL);
		g_list_free (cards);
		cards = NULL;
	}

	ldap_msgfree(res);

	return TRUE;
}

static void
pas_backend_ldap_search (PASBackendLDAP  	*bl,
			 PASBook         	*book,
			 PASBackendLDAPBookView *view)
{
	char *ldap_query = pas_backend_ldap_build_query(view->search);

	if (ldap_query != NULL) {
		LDAP           *ldap;

		pas_backend_ldap_ensure_connected(bl);

		ldap = bl->priv->ldap;

		if (ldap) {
			ldap->ld_sizelimit = LDAP_MAX_SEARCH_RESPONSES;
			ldap->ld_deref = LDAP_DEREF_ALWAYS;

			if ((view->search_msgid = ldap_search (ldap,
							       bl->priv->ldap_rootdn,
							       LDAP_SCOPE_ONELEVEL,
							       ldap_query,
							       NULL, 0)) == -1) {
				g_warning ("ldap error '%s' in pas_backend_ldap_search\n", ldap_err2string(ldap->ld_errno));
			}
			else {
				view->search_idle = g_idle_add((GSourceFunc)poll_ldap, view);
			}
		}
	}
}

static void
pas_backend_ldap_process_get_book_view (PASBackend *backend,
					PASBook    *book,
					PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	CORBA_Environment ev;
	Evolution_Book    corba_book;
	PASBookView       *book_view;
	PASBackendLDAPBookView *view;

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

	view = g_new(PASBackendLDAPBookView, 1);
	view->book_view = book_view;
	view->search = g_strdup(req->search);
	view->blpriv = bl->priv;

	bl->priv->book_views = g_list_prepend(bl->priv->book_views, view);

	pas_backend_ldap_search (bl, book, view);

}

static void
pas_backend_ldap_process_check_connection (PASBackend *backend,
					   PASBook    *book,
					   PASRequest *req)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);

	pas_book_report_connection (book, bl->priv->connected);
}

static gboolean
pas_backend_ldap_can_write (PASBook *book)
{
	return FALSE; /* XXX */
}

static gboolean
pas_backend_ldap_can_write_card (PASBook *book,
				 const char *id)
{
	return FALSE; /* XXX */
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

	case GetCursor:
		pas_backend_ldap_process_get_cursor (backend, book, req);
		break;

	case GetBookView:
		pas_backend_ldap_process_get_book_view (backend, book, req);
		break;
	}

	g_free (req);
}

static void
pas_backend_ldap_book_destroy_cb (PASBook *book, gpointer data)
{
	PASBackendLDAP *backend;

	backend = PAS_BACKEND_LDAP (data);

	pas_backend_remove_client (PAS_BACKEND (backend), book);
}

static char *
pas_backend_ldap_get_vcard (PASBook *book, const char *id)
{
	PASBackendLDAP *bl;
	LDAP           *ldap;
	int            ldap_error = LDAP_SUCCESS; /* XXX */

	bl = PAS_BACKEND_LDAP (pas_book_get_backend (book));
	ldap = bl->priv->ldap;

	/* XXX use ldap_search */

	if (ldap_error == LDAP_SUCCESS) {
		/* success */
		return g_strdup ("");
	}
	else {
		return g_strdup ("");
	}
}

static gboolean
pas_backend_ldap_load_uri (PASBackend             *backend,
			   const char             *uri)
{
	PASBackendLDAP *bl = PAS_BACKEND_LDAP (backend);
	LDAPURLDesc    *lud;
	int ldap_error;

	g_assert (bl->priv->connected == FALSE);

	ldap_error = ldap_url_parse ((char*)uri, &lud);
	if (ldap_error == LDAP_SUCCESS) {
		g_free(bl->priv->uri);
		bl->priv->uri = g_strdup (uri);
		bl->priv->ldap_host = g_strdup(lud->lud_host);
		bl->priv->ldap_port = lud->lud_port;
		/* if a port wasn't specified, default to 389 */
		if (bl->priv->ldap_port == 0)
			bl->priv->ldap_port = 389;
		bl->priv->ldap_rootdn = g_strdup(lud->lud_dn);

		ldap_free_urldesc(lud);

		pas_backend_ldap_ensure_connected(bl);
		return TRUE;
	} else
		return FALSE;
}

/* Get_uri handler for the addressbook LDAP backend */
static const char *
pas_backend_ldap_get_uri (PASBackend *backend)
{
	PASBackendLDAP *bl;

	bl = PAS_BACKEND_LDAP (backend);
	return bl->priv->uri;
}

static gboolean
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
		pas_backend_ldap_get_vcard,
		pas_backend_ldap_can_write,
		pas_backend_ldap_can_write_card);

	if (!book) {
		if (!bl->priv->clients)
			pas_backend_last_client_gone (backend);

		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (book), "destroy",
		    pas_backend_ldap_book_destroy_cb, backend);

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

	return TRUE;
}

static void
pas_backend_ldap_remove_client (PASBackend             *backend,
				PASBook                *book)
{
	PASBackendLDAP *bl;
	GList *l;
	PASBook *lbook;

	g_return_if_fail (backend != NULL);
	g_return_if_fail (PAS_IS_BACKEND_LDAP (backend));
	g_return_if_fail (book != NULL);
	g_return_if_fail (PAS_IS_BOOK (book));

	bl = PAS_BACKEND_LDAP (backend);

	/* Find the book in the list of clients */

	for (l = bl->priv->clients; l; l = l->next) {
		lbook = PAS_BOOK (l->data);

		if (lbook == book)
			break;
	}

	g_assert (l != NULL);

	/* Disconnect */

	bl->priv->clients = g_list_remove_link (bl->priv->clients, l);
	g_list_free_1 (l);

	/* When all clients go away, notify the parent factory about it so that
	 * it may decide whether to kill the backend or not.
	 */
	if (!bl->priv->clients)
		pas_backend_last_client_gone (backend);
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

	backend->priv->ldap = NULL;

	return PAS_BACKEND (backend);
}

static void
pas_backend_ldap_destroy (GtkObject *object)
{
	PASBackendLDAP *bl;

	bl = PAS_BACKEND_LDAP (object);

	g_free (bl->priv->uri);

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
	parent_class->get_uri       = pas_backend_ldap_get_uri;
	parent_class->add_client    = pas_backend_ldap_add_client;
	parent_class->remove_client = pas_backend_ldap_remove_client;

	object_class->destroy = pas_backend_ldap_destroy;
}

static void
pas_backend_ldap_init (PASBackendLDAP *backend)
{
	PASBackendLDAPPrivate *priv;

	priv            = g_new0 (PASBackendLDAPPrivate, 1);
	priv->connected = FALSE;
	priv->clients   = NULL;
	priv->uri       = NULL;

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

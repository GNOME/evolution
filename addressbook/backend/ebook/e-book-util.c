/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-book-util.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <config.h>
#include "e-book-util.h"

#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <e-util/e-config-listener.h>
#include "e-card-compare.h"

typedef struct _CommonBookInfo CommonBookInfo;
struct _CommonBookInfo {
	EBookCommonCallback cb;
	gpointer closure;
};

char *
e_book_expand_uri (const char *uri)
{
	if (!strncmp (uri, "file:", 5)) {
		int length = strlen (uri);
		int offset = 5;

		if (!strncmp (uri, "file://", 7))
			offset = 7;

		if (length < 3 || strcmp (uri + length - 3, ".db")) {
			/* we assume it's a dir and glom addressbook.db onto the end. */

			char *ret_val;
			char *file_name;

			file_name = g_build_filename(uri + offset, "addressbook.db", NULL);
			ret_val = g_strdup_printf("file://%s", file_name);
			g_free(file_name);
			return ret_val; 
		}
	}

	return g_strdup (uri);
}

static void
got_uri_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	CommonBookInfo *info = (CommonBookInfo *) closure;

	if (status == E_BOOK_STATUS_SUCCESS) {
		info->cb (book, info->closure);
	} else {
		if (book)
			g_object_unref (book);
		info->cb (NULL, info->closure);
	}
	g_free (info);
}

void
e_book_load_address_book_by_uri (EBook *book, const char *uri, EBookCallback open_response, gpointer closure)
{
	char *real_uri;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (open_response != NULL);

	real_uri = e_book_expand_uri (uri);

	e_book_load_uri (book, real_uri, open_response, closure);

	g_free (real_uri);
}

void
e_book_use_address_book_by_uri (const char *uri, EBookCommonCallback cb, gpointer closure)
{
	EBook *book;
	CommonBookInfo *info;

	g_return_if_fail (cb != NULL);

	info = g_new0 (CommonBookInfo, 1);
	info->cb = cb;
	info->closure = closure;

	book = e_book_new ();
	e_book_load_address_book_by_uri (book, uri, got_uri_book_cb, info);
}

EConfigListener *
e_book_get_config_database ()
{
	static EConfigListener *config_db;

	if (config_db == NULL)
		config_db = e_config_listener_new ();

	return config_db;
}

static EBook *common_default_book = NULL;

static void
got_default_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	CommonBookInfo *info = (CommonBookInfo *) closure;

	if (status == E_BOOK_STATUS_SUCCESS) {

		/* We try not to leak in a race condition where the
		   default book got loaded twice. */

		if (common_default_book) {
			g_object_unref (book);
			book = common_default_book;
		}
		
		info->cb (book, info->closure);

		if (common_default_book == NULL) {
			common_default_book = book;
		}
		
	} else {
		if (book)
			g_object_unref (book);
		info->cb (NULL, info->closure);

	}
	g_free (info);
}

void
e_book_use_default_book (EBookCommonCallback cb, gpointer closure)
{
	EBook *book;
	CommonBookInfo *info;

	g_return_if_fail (cb != NULL);

	if (common_default_book != NULL) {
		cb (common_default_book, closure);
		return;
	}

	info = g_new0 (CommonBookInfo, 1);
	info->cb = cb;
	info->closure = closure;

	book = e_book_new ();
	e_book_load_default_book (book, got_default_book_cb, info);
}

static char *default_book_uri;

static void
set_default_book_uri_local (void)
{
	char *filename;

	filename = g_build_filename (g_get_home_dir(),
				     "evolution/local/Contacts/addressbook.db",
				     NULL);
	default_book_uri = g_strdup_printf ("file://%s", filename);
	g_free (filename);
}

static void
set_default_book_uri (char *val)
{
	if (default_book_uri)
		g_free (default_book_uri);

	if (val) {
		default_book_uri = e_book_expand_uri (val);
		g_free (val);
	}
	else {
		set_default_book_uri_local ();
	}
}

#define DEFAULT_CONTACTS_URI_PATH "/apps/evolution/shell/default_folders/contacts_uri"
static void
default_folder_listener (EConfigListener *cl, const char *key, gpointer data)
{
	char *val;

	if (strcmp (key, DEFAULT_CONTACTS_URI_PATH))
		return;

	val = e_config_listener_get_string (cl, DEFAULT_CONTACTS_URI_PATH);

	set_default_book_uri (val);
}

static void
set_default_book_uri_from_config_db (void)
{
	char *val;
	EConfigListener* config_db;

	config_db = e_book_get_config_database ();
	val = e_config_listener_get_string_with_default (config_db, DEFAULT_CONTACTS_URI_PATH, NULL, NULL);

	g_signal_connect (config_db,
			  "key_changed",
			  G_CALLBACK (default_folder_listener), NULL);

	set_default_book_uri (val);
}

typedef struct {
	gpointer closure;
	EBookCallback open_response;
} DefaultBookClosure;

static void
e_book_default_book_open (EBook *book, EBookStatus status, gpointer closure)
{
	DefaultBookClosure *default_book_closure = closure;
	gpointer user_closure = default_book_closure->closure;
	EBookCallback user_response = default_book_closure->open_response;

	g_free (default_book_closure);

	/* If there's a transient error, report it to the caller, but
	 * if the old default folder has disappeared, fall back to
	 * the local contacts folder instead.
	 */
	if (status == E_BOOK_STATUS_PROTOCOL_NOT_SUPPORTED ||
	    status == E_BOOK_STATUS_NO_SUCH_BOOK) {
		set_default_book_uri_local ();
		e_book_load_default_book (book, user_response, user_closure);
	} else {
		user_response (book, status, user_closure);
	}
}

void
e_book_load_default_book (EBook *book, EBookCallback open_response, gpointer closure)
{
	const char *uri;
	DefaultBookClosure *default_book_closure;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (open_response != NULL);

	uri = e_book_get_default_book_uri ();

	default_book_closure = g_new (DefaultBookClosure, 1);

	default_book_closure->closure = closure;
	default_book_closure->open_response = open_response;

	e_book_load_uri (book, uri,
			 e_book_default_book_open, default_book_closure);

}

const char *
e_book_get_default_book_uri ()
{
	if (!default_book_uri)
		set_default_book_uri_from_config_db ();

	return default_book_uri;
}

/*
 *
 * Simple Query Stuff
 *
 */

typedef struct _SimpleQueryInfo SimpleQueryInfo;
struct _SimpleQueryInfo {
	guint tag;
	EBook *book;
	gchar *query;
	EBookSimpleQueryCallback cb;
	gpointer closure;
	EBookView *view;
	guint add_tag;
	guint seq_complete_tag;
	GList *cards;
	gboolean cancelled;
};

static void
book_add_simple_query (EBook *book, SimpleQueryInfo *info)
{
	GList *pending = g_object_get_data (G_OBJECT(book), "sq_pending");
	pending = g_list_prepend (pending, info);
	g_object_set_data (G_OBJECT (book), "sq_pending", pending);
}

static SimpleQueryInfo *
book_lookup_simple_query (EBook *book, guint tag)
{
	GList *pending = g_object_get_data (G_OBJECT (book), "sq_pending");
	while (pending) {
		SimpleQueryInfo *sq = pending->data;
		if (sq->tag == tag)
			return sq;
		pending = g_list_next (pending);
	}
	return NULL;
}

static void
book_remove_simple_query (EBook *book, SimpleQueryInfo *info)
{
	GList *pending = g_object_get_data (G_OBJECT (book), "sq_pending");
	GList *i;

	for (i=pending; i != NULL; i = g_list_next (i)) {
		if (i->data == info) {
			pending = g_list_remove_link (pending, i);
			g_list_free_1 (i);
			break;
		}
	}
	g_object_set_data (G_OBJECT (book), "sq_pending", pending);
}

static guint
book_issue_tag (EBook *book)
{
	gpointer ptr = g_object_get_data (G_OBJECT (book), "sq_tag");
	guint tag = GPOINTER_TO_UINT (ptr);
	if (tag == 0)
		tag = 1;
	g_object_set_data (G_OBJECT (book), "sq_tag", GUINT_TO_POINTER (tag+1));
	return tag;
}

static SimpleQueryInfo *
simple_query_new (EBook *book, const char *query, EBookSimpleQueryCallback cb, gpointer closure)
{
	SimpleQueryInfo *sq = g_new0 (SimpleQueryInfo, 1);

	sq->tag = book_issue_tag (book);
	sq->book = book;
	g_object_ref (book);
	sq->query = g_strdup (query);
	sq->cb = cb;
	sq->closure = closure;
	sq->cancelled = FALSE;

	/* Automatically add ourselves to the EBook's pending list. */
	book_add_simple_query (book, sq);

	return sq;
}

static void
simple_query_disconnect (SimpleQueryInfo *sq)
{
	if (sq->add_tag) {
		g_signal_handler_disconnect (sq->view, sq->add_tag);
		sq->add_tag = 0;
	}

	if (sq->seq_complete_tag) {
		g_signal_handler_disconnect (sq->view, sq->seq_complete_tag);
		sq->seq_complete_tag = 0;
	}

	if (sq->view) {
		g_object_unref (sq->view);
		sq->view = NULL;
	}
}

static void
simple_query_free (SimpleQueryInfo *sq)
{
	simple_query_disconnect (sq);

	/* Remove ourselves from the EBook's pending list. */
	book_remove_simple_query (sq->book, sq);

	g_free (sq->query);

	if (sq->book)
		g_object_unref (sq->book);

	g_list_foreach (sq->cards, (GFunc) g_object_unref, NULL);
	g_list_free (sq->cards);

	g_free (sq);
}

static void
simple_query_card_added_cb (EBookView *view, const GList *cards, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	if (sq->cancelled)
		return;

	sq->cards = g_list_concat (sq->cards, g_list_copy ((GList *) cards));
	g_list_foreach ((GList *) cards, (GFunc) g_object_ref, NULL);
}

static void
simple_query_sequence_complete_cb (EBookView *view, EBookViewStatus status, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	/* Disconnect signals, so that we don't pick up any changes to the book that occur
	   in our callback */
	simple_query_disconnect (sq);
	if (! sq->cancelled)
		sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS, sq->cards, sq->closure);
	simple_query_free (sq);
}

static void
simple_query_book_view_cb (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	if (sq->cancelled) {
		simple_query_free (sq);
		return;
	}

	if (status != E_BOOK_STATUS_SUCCESS) {
		simple_query_disconnect (sq);
		sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_OTHER_ERROR, NULL, sq->closure);
		simple_query_free (sq);
		return;
	}

	sq->view = book_view;
	g_object_ref (book_view);

	sq->add_tag = g_signal_connect (sq->view, "card_added",
					G_CALLBACK (simple_query_card_added_cb), sq);
	sq->seq_complete_tag = g_signal_connect (sq->view, "sequence_complete",
						 G_CALLBACK (simple_query_sequence_complete_cb), sq);
}

guint
e_book_simple_query (EBook *book, const char *query, EBookSimpleQueryCallback cb, gpointer closure)
{
	SimpleQueryInfo *sq;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (query, 0);
	g_return_val_if_fail (cb, 0);

	sq = simple_query_new (book, query, cb, closure);
	e_book_get_book_view (book, (gchar *) query, simple_query_book_view_cb, sq);

	return sq->tag;
}

void
e_book_simple_query_cancel (EBook *book, guint tag)
{
	SimpleQueryInfo *sq;

	g_return_if_fail (book && E_IS_BOOK (book));

	sq = book_lookup_simple_query (book, tag);

	if (sq) {
		sq->cancelled = TRUE;
		sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_CANCELLED, NULL, sq->closure);
	} else {
		g_warning ("Simple query tag %d is unknown", tag);
	}
}

/*
 *
 * Specialized Queries
 *
 */

typedef struct _NameEmailQueryInfo NameEmailQueryInfo;
struct _NameEmailQueryInfo {
	gchar *name;
	gchar *email;
	EBookSimpleQueryCallback cb;
	gpointer closure;
};

static void
name_email_query_info_free (NameEmailQueryInfo *info)
{
	if (info) {
		g_free (info->name);
		g_free (info->email);
		g_free (info);
	}
}

static void
name_and_email_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	NameEmailQueryInfo *info = closure;
	GList *filtered_cards = NULL;

	while (cards) {
		ECard *card = E_CARD (cards->data);
		if ((info->name == NULL || e_card_compare_name_to_string (card, info->name) >= E_CARD_MATCH_VAGUE)
		    && (info->email == NULL || e_card_email_match_string (card, info->email))) {
			filtered_cards = g_list_append (filtered_cards, card);
		}
		cards = g_list_next (cards);
	}

	info->cb (book, status, filtered_cards, info->closure);

	g_list_free (filtered_cards);

	name_email_query_info_free (info);
}

guint
e_book_name_and_email_query (EBook *book,
			     const gchar *name,
			     const gchar *email,
			     EBookSimpleQueryCallback cb,
			     gpointer closure)
{
	NameEmailQueryInfo *info;
	gchar *email_query=NULL, *name_query=NULL, *query;
	guint tag;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (cb != NULL, 0);

	if (name && !*name)
		name = NULL;
	if (email && !*email)
		email = NULL;

	if (name == NULL && email == NULL)
		return 0;

	/* Build our e-mail query.
	 * We only query against the username part of the address, to avoid not matching
	 * fred@foo.com and fred@mail.foo.com.  While their may be namespace collisions
	 * in the usernames of everyone out there, it shouldn't be that bad.  (Famous last words.)
	 */
	if (email) {
		const gchar *t = email;
		while (*t && *t != '@')
			++t;
		if (*t == '@') {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%.*s@\")", t-email, email);

		} else {
			email_query = g_strdup_printf ("(beginswith \"email\" \"%s\")", email);
		}
	}

	/* Build our name query.
	 * We only do name-query stuff if we don't have an e-mail address.  Our basic assumption
	 * is that the username part of the email is good enough to keep the amount of stuff returned
	 * in the query relatively small.
	 */
	if (name && !email) {
		gchar *name_cpy = g_strdup (name), *qjoined;
		gchar **namev;
		gint i, count=0;

		g_strstrip (name_cpy);
		namev = g_strsplit (name_cpy, " ", 0);
		for (i=0; namev[i]; ++i) {
			if (*namev[i]) {
				char *str = namev[i];

				namev[i] = g_strdup_printf ("(contains \"file_as\" \"%s\")", namev[i]);
				++count;

				g_free (str);
			}
		}

		qjoined = g_strjoinv (" ", namev);
		if (count > 1) {
			name_query = g_strdup_printf ("(or %s)", qjoined);
		} else {
			name_query = qjoined;
			qjoined = NULL;
		}
		
		g_free (name_cpy);
		g_strfreev (namev);
		g_free (qjoined);
	}

	/* Assemble our e-mail & name queries */
	if (email_query && name_query) {
		query = g_strdup_printf ("(and %s %s)", email_query, name_query);
	} else if (email_query) {
		query = email_query;
		email_query = NULL;
	} else if (name_query) {
		query = name_query;
		name_query = NULL;
	} else
		return 0;

	info = g_new0 (NameEmailQueryInfo, 1);
	info->name = g_strdup (name);
	info->email = g_strdup (email);
	info->cb = cb;
	info->closure = closure;

	tag = e_book_simple_query (book, query, name_and_email_cb, info);

	g_free (email_query);
	g_free (name_query);
	g_free (query);

	return tag;
}

/*
 * Simple nickname query
 */

typedef struct _NicknameQueryInfo NicknameQueryInfo;
struct _NicknameQueryInfo {
	gchar *nickname;
	EBookSimpleQueryCallback cb;
	gpointer closure;
};

static void
nickname_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	NicknameQueryInfo *info = closure;

	if (info->cb)
		info->cb (book, status, cards, info->closure);

	g_free (info->nickname);
	g_free (info);
}

guint
e_book_nickname_query (EBook *book,
		       const char *nickname,
		       EBookSimpleQueryCallback cb,
		       gpointer closure)
{
	NicknameQueryInfo *info;
	gchar *query;
	guint retval;

	g_return_val_if_fail (E_IS_BOOK (book), 0);
	g_return_val_if_fail (nickname != NULL, 0);

	/* The empty-string case shouldn't generate a warning. */
	if (! *nickname)
		return 0;

	info = g_new0 (NicknameQueryInfo, 1);
	info->nickname = g_strdup (nickname);
	info->cb = cb;
	info->closure = closure;

	query = g_strdup_printf ("(is \"nickname\" \"%s\")", info->nickname);

	retval = e_book_simple_query (book, query, nickname_cb, info);

	g_free (query);

	return retval;
}

/*
 *  Convenience routine to check for addresses in the local address book.
 */

typedef struct _HaveAddressInfo HaveAddressInfo;
struct _HaveAddressInfo {
	gchar *email;
	EBookHaveAddressCallback cb;
	gpointer closure;
};

static void
have_address_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;
	
	info->cb (book, 
		  info->email,
		  cards && (status == E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS) ? E_CARD (cards->data) : NULL,
		  info->closure);

	g_free (info->email);
	g_free (info);
}

static void
have_address_book_open_cb (EBook *book, gpointer closure)
{
	HaveAddressInfo *info = (HaveAddressInfo *) closure;

	if (book) {

		e_book_name_and_email_query (book, NULL, info->email, have_address_query_cb, info);

	} else {

		info->cb (NULL, info->email, NULL, info->closure);

		g_free (info->email);
		g_free (info);

	}
}

void
e_book_query_address_default (const gchar *email,
			      EBookHaveAddressCallback cb,
			      gpointer closure)
{
	HaveAddressInfo *info;

	g_return_if_fail (email != NULL);
	g_return_if_fail (cb != NULL);

	info = g_new0 (HaveAddressInfo, 1);
	info->email = g_strdup (email);
	info->cb = cb;
	info->closure = closure;

	e_book_use_default_book (have_address_book_open_cb, info);
}

/* bad place for this i know. */
int
e_utf8_casefold_collate_len (const gchar *str1, const gchar *str2, int len)
{
	gchar *s1 = g_utf8_casefold(str1, len);
	gchar *s2 = g_utf8_casefold(str2, len);
	int rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

int
e_utf8_casefold_collate (const gchar *str1, const gchar *str2)
{
	return e_utf8_casefold_collate_len (str1, str2, -1);
}


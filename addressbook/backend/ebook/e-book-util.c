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
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include "e-book-util.h"

gboolean
e_book_load_local_address_book (EBook *book, EBookCallback open_response, gpointer closure)
{
	gchar *filename;
	gchar *uri;
	gboolean rv;

	g_return_val_if_fail (book != NULL,          FALSE);
	g_return_val_if_fail (E_IS_BOOK (book),      FALSE);
	g_return_val_if_fail (open_response != NULL, FALSE);

	filename = gnome_util_prepend_user_home ("evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf ("file://%s", filename);

	rv = e_book_load_uri (book, uri, open_response, closure);

	g_free (filename);
	g_free (uri);

	return rv;
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
};

static void
book_add_simple_query (EBook *book, SimpleQueryInfo *info)
{
	GList *pending = gtk_object_get_data (GTK_OBJECT (book), "sq_pending");
	pending = g_list_prepend (pending, info);
	gtk_object_set_data (GTK_OBJECT (book), "sq_pending", pending);
}

static SimpleQueryInfo *
book_lookup_simple_query (EBook *book, guint tag)
{
	GList *pending = gtk_object_get_data (GTK_OBJECT (book), "sq_pending");
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
	GList *pending = gtk_object_get_data (GTK_OBJECT (book), "sq_pending");
	GList *i;

	for (i=pending; i != NULL; i = g_list_next (i)) {
		if (i->data == info) {
			pending = g_list_remove_link (pending, i);
			g_list_free_1 (i);
			break;
		}
	}
	gtk_object_set_data (GTK_OBJECT (book), "sq_pending", pending);
}

static guint
book_issue_tag (EBook *book)
{
	gpointer ptr = gtk_object_get_data (GTK_OBJECT (book), "sq_tag");
	guint tag = GPOINTER_TO_UINT (ptr);
	if (tag == 0)
		tag = 1;
	gtk_object_set_data (GTK_OBJECT (book), "sq_tag", GUINT_TO_POINTER (tag+1));
	return tag;
}

#ifdef USE_WORKAROUND
static GList *WORKAROUND_sq_queue = NULL;
static gboolean WORKAROUND_running_query = FALSE;
#endif

static SimpleQueryInfo *
simple_query_new (EBook *book, const char *query, EBookSimpleQueryCallback cb, gpointer closure)
{
	SimpleQueryInfo *sq = g_new0 (SimpleQueryInfo, 1);

	sq->tag = book_issue_tag (book);
	sq->book = book;
	gtk_object_ref (GTK_OBJECT (book));
	sq->query = g_strdup_printf (query);
	sq->cb = cb;
	sq->closure = closure;

	/* Automatically add ourselves to the EBook's pending list. */
	book_add_simple_query (book, sq);

#ifdef USE_WORKAROUND
	/* Add ourselves to the queue. */
	WORKAROUND_sq_queue = g_list_append (WORKAROUND_sq_queue, sq);
#endif

	return sq;
}

static void
simple_query_free (SimpleQueryInfo *sq)
{
	/* Remove ourselves from the EBook's pending list. */
	book_remove_simple_query (sq->book, sq);

#ifdef USE_WORKAROUND
	/* If we are still in the queue, remove ourselves. */
	for (i = WORKAROUND_sq_queue; i != NULL; i = g_list_next (i)) {
		if (i->data == sq) {
			WORKAROUND_sq_queue = g_list_remove_link (WORKAROUND_sq_queue, i);
			g_list_free_1 (i);
			break;
		} 
	}
#endif
	
	g_free (sq->query);

	if (sq->add_tag)
		gtk_signal_disconnect (GTK_OBJECT (sq->view), sq->add_tag);
	if (sq->seq_complete_tag)
		gtk_signal_disconnect (GTK_OBJECT (sq->view), sq->seq_complete_tag);

#ifdef USE_WORKAROUND
	if (sq->view)
		WORKAROUND_running_query = FALSE;
#endif

	if (sq->view)
		gtk_object_unref (GTK_OBJECT (sq->view));

	if (sq->book)
		gtk_object_unref (GTK_OBJECT (sq->book));

	g_list_foreach (sq->cards, (GFunc) gtk_object_unref, NULL);
	g_list_free (sq->cards);

	g_free (sq);
}

static void
simple_query_card_added_cb (EBookView *view, const GList *cards, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	sq->cards = g_list_concat (sq->cards, g_list_copy ((GList *) cards));
	g_list_foreach ((GList *) cards, (GFunc) gtk_object_ref, NULL);
}

static void
simple_query_sequence_complete_cb (EBookView *view, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS, sq->cards, sq->closure);
	simple_query_free (sq);
}

static void
simple_query_book_view_cb (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	SimpleQueryInfo *sq = closure;

	if (status != E_BOOK_STATUS_SUCCESS) {
		sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_OTHER_ERROR, NULL, sq->closure);
		simple_query_free (sq);
		return;
	}

	sq->view = book_view;
	gtk_object_ref (GTK_OBJECT (book_view));

	sq->add_tag = gtk_signal_connect (GTK_OBJECT (sq->view),
					  "card_added",
					  GTK_SIGNAL_FUNC (simple_query_card_added_cb),
					  sq);
	sq->seq_complete_tag = gtk_signal_connect (GTK_OBJECT (sq->view),
						   "sequence_complete",
						   GTK_SIGNAL_FUNC (simple_query_sequence_complete_cb),
						   sq);
}

#ifdef USE_WORKAROUND
static gint
WORKAROUND_try_queue (gpointer foo)
{
	if (WORKAROUND_sq_queue) {
		SimpleQueryInfo *sq;
		GList *i;

		if (WORKAROUND_running_query) 
			return TRUE;

		WORKAROUND_running_query = TRUE;
		sq = WORKAROUND_sq_queue->data;

		i = WORKAROUND_sq_queue;
		WORKAROUND_sq_queue = g_list_remove_link (WORKAROUND_sq_queue, WORKAROUND_sq_queue);
		g_list_free_1 (i);

		e_book_get_book_view (sq->book, sq->query, simple_query_book_view_cb, sq);
	}

	return FALSE;
}
#endif

guint
e_book_simple_query (EBook *book, const char *query, EBookSimpleQueryCallback cb, gpointer closure)
{
	SimpleQueryInfo *sq;

	g_return_val_if_fail (book && E_IS_BOOK (book), 0);
	g_return_val_if_fail (query, 0);
	g_return_val_if_fail (cb, 0);

	sq = simple_query_new (book, query, cb, closure);
#ifdef USE_WORKAROUND
	gtk_timeout_add (50, WORKAROUND_try_queue, NULL);
#else
	e_book_get_book_view (book, (gchar *) query, simple_query_book_view_cb, sq);
#endif

	return sq->tag;
}

void
e_book_simple_query_cancel (EBook *book, guint tag)
{
	SimpleQueryInfo *sq;

	g_return_if_fail (book && E_IS_BOOK (book));

	sq = book_lookup_simple_query (book, tag);

	if (sq) {
		sq->cb (sq->book, E_BOOK_SIMPLE_QUERY_STATUS_CANCELLED, NULL, sq->closure);
		simple_query_free (sq);
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
		if ((info->name == NULL || e_card_name_match_string (card->name, info->name))
		    && (info->email == NULL || e_card_email_match_string (card, info->email)))
			filtered_cards = g_list_append (filtered_cards, card);
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
		const gchar *t=email;
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
		namev = g_strsplit (" ", name_cpy, 0);
		for (i=0; namev[i]; ++i) {
			if (*namev[i]) {
				namev[i] = g_strdup_printf ("(contains \"file_as\" \"%s\")", namev[i]);
				++count;
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
		for (i=0; namev[i]; ++i)
			if (*namev[i])
				g_free (namev[i]);
		g_free (namev);
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

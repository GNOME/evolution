/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-select-names-completion.c
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

#include <gal/unicode/gunicode.h>

#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/backend/ebook/e-destination.h>
#include <addressbook/backend/ebook/e-card-simple.h>
#include "e-select-names-completion.h"

struct _ESelectNamesCompletionPrivate {

	ESelectNamesModel *model;

	EBook *book;
	gboolean book_ready;
	gboolean cancelled;

	guint book_view_tag;
	EBookView *book_view;

	gchar *waiting_query;
	gint waiting_pos, waiting_limit;
	gchar *query_text;

	gchar *cached_query_text;
	GList *cached_cards;

	gboolean primary_only;
};

static void e_select_names_completion_class_init (ESelectNamesCompletionClass *);
static void e_select_names_completion_init (ESelectNamesCompletion *);
static void e_select_names_completion_destroy (GtkObject *object);

static void e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data);
static void e_select_names_completion_card_added_cb    (EBookView *, const GList *cards, gpointer user_data);
static void e_select_names_completion_seq_complete_cb  (EBookView *, gpointer user_data);

static void e_select_names_completion_do_query (ESelectNamesCompletion *, const gchar *query_text, gint pos, gint limit);

static void e_select_names_completion_begin  (ECompletion *, const gchar *txt, gint pos, gint limit);
static void e_select_names_completion_end    (ECompletion *);
static void e_select_names_completion_cancel (ECompletion *);

static GtkObjectClass *parent_class;

static FILE *out;

/*
 *
 * Query builders
 *
 */

typedef gchar *(*BookQuerySExp) (ESelectNamesCompletion *);
typedef ECompletionMatch *(*BookQueryMatchTester) (ESelectNamesCompletion *, EDestination *);

static void
our_match_destroy (ECompletionMatch *match)
{
	gtk_object_unref (GTK_OBJECT (match->user_data));
}

static ECompletionMatch *
make_match (EDestination *dest, const gchar *menu_form, double score)
{
	ECompletionMatch *match = g_new0 (ECompletionMatch, 1);
	e_completion_match_construct (match);

	e_completion_match_set_text (match, e_destination_get_name (dest), menu_form);
	match->score = score;
	match->sort_minor = e_destination_get_email_num (dest);

	match->user_data = dest;
	gtk_object_ref (GTK_OBJECT (dest));

	match->destroy = our_match_destroy;

	return match;
}

static void
emailify_match (ECompletionMatch *match)
{
	EDestination *dest = E_DESTINATION (match->user_data);
	ECard *card = e_destination_get_card (dest);
	const gchar *email = e_destination_get_email (dest);
	const gchar *menu_txt = e_completion_match_get_menu_text (match);
	
	if (card && card->email && e_list_length (card->email) > 1) {
		
		if (email && strstr (menu_txt, email) == NULL) {
			gchar *tmp = g_strdup_printf ("%s <%s>", menu_txt, email);
			e_completion_match_set_text (match,
						     e_completion_match_get_match_text (match),
						     tmp);
			g_free (tmp);
		}

		match->sort_minor = e_destination_get_email_num (dest);
	}
}

/*
 * Nickname query
 */

static gchar *
sexp_nickname (ESelectNamesCompletion *comp)
{
	return g_strdup_printf ("(beginswith \"nickname\" \"%s\")", comp->priv->query_text);

}

static ECompletionMatch *
match_nickname (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *match = NULL;
	gint len = strlen (comp->priv->query_text);
	ECard *card = e_destination_get_card (dest);
	double score;

	if (card->nickname
	    && !g_utf8_strncasecmp (comp->priv->query_text, card->nickname, len)) {
		ECompletionMatch *match = g_new0 (ECompletionMatch, 1);
		gchar *name = e_card_name_to_string (card->name);
		gchar *str;
		
		score = len * 10; /* nickname gives 10 points per matching character */
		str = g_strdup_printf ("(%s) %s", card->nickname, name);

		match = make_match (dest, str, score);
		g_free (name);
		g_free (str);
	}

	return match;
}

/*
 * E-Mail Query
 */

static gchar *
sexp_email (ESelectNamesCompletion *comp)
{
	return g_strdup_printf ("(beginswith \"email\" \"%s\")", comp->priv->query_text);
}

static ECompletionMatch *
match_email (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *match;
	gint len = strlen (comp->priv->query_text);
	ECard *card = e_destination_get_card (dest);
	const gchar *email = e_destination_get_email (dest);
	double score;
	
	if (email && !g_utf8_strncasecmp (comp->priv->query_text, email, len)) {

		gchar *name, *str;
		
		score = len * 2; /* 2 points for each matching character */

		name = e_card_name_to_string (card->name);
		if (name && *name)
			str = g_strdup_printf ("<%s> %s", email, name);
		else
			str = g_strdup (email);

		match = make_match (dest, str, score);

		g_free (name);
		g_free (str);

		return match;
	}

	return NULL;
}

/*
 * Name Query
 */

static gchar *
sexp_name (ESelectNamesCompletion *comp)
{
	if (comp && comp->priv->query_text && *comp->priv->query_text) {
		gchar *cpy = g_strdup (comp->priv->query_text);
		gchar **strv;
		gchar *query;
		gint i, count=0;

		strv = g_strsplit (cpy, " ", 0);
		for (i=0; strv[i]; ++i) {
			++count;
			g_strstrip (strv[i]);
			strv[i] = g_strdup_printf ("(contains \"full_name\" \"%s\")", strv[i]);
		}

		if (count == 1) {
			query = strv[0];
			strv[0] = NULL;
		} else {
			gchar *joined = g_strjoinv (" ", strv);
			query = g_strdup_printf ("(and %s)", joined);
			g_free (joined);
		}

		for (i=0; strv[i]; ++i)
			g_free (strv[i]);
		g_free (cpy);
		g_free (strv);


		return query;
	}

	return NULL;
}

enum {
	MATCHED_NOTHING = 0,
	MATCHED_GIVEN_NAME  = 1<<0,
	MATCHED_ADDITIONAL_NAME = 1<<1,
	MATCHED_FAMILY_NAME   = 1<<2
};

/*
  Match text against every substring in fragment that follows whitespace.
  This allows the fragment "de Icaza" to match against txt "ica".
*/
static gboolean
match_name_fragment (const gchar *fragment, const gchar *txt)
{
	gint len = strlen (txt);

	while (*fragment) {
		if (!g_utf8_strncasecmp (fragment, txt, len))
			return TRUE;

		while (*fragment && !isspace ((gint) *fragment))
			++fragment;
		while (*fragment && isspace ((gint) *fragment))
			++fragment;
	}

	return FALSE;
}

static ECompletionMatch *
match_name (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *final_match = NULL;
	gchar *menu_text = NULL;
	ECard *card;
	gchar *cpy, **strv;
	const gchar *email;
	gint len, i, match_len = 0;
	gint match = 0, first_match = 0;
	double score = 0;
	gboolean have_given, have_additional, have_family;

	card = e_destination_get_card (dest);
	
	if (card->name == NULL)
		return NULL;

	cpy = g_strdup (comp->priv->query_text);
	strv = g_strsplit (cpy, " ", 0);
	
	for (i=0; strv[i] && !(match & MATCHED_NOTHING); ++i) {
		gint this_match = 0;

		g_strstrip (strv[i]);
		len = strlen (strv[i]);

		if (card->name->given
		    && *card->name->given
		    && !(match & MATCHED_GIVEN_NAME)
		    && match_name_fragment (card->name->given, strv[i])) {

			this_match = MATCHED_GIVEN_NAME;

		}
		else if (card->name->additional
			 && *card->name->additional
			 && !(match & MATCHED_ADDITIONAL_NAME)
			 && match_name_fragment (card->name->additional, strv[i])) {

			this_match = MATCHED_ADDITIONAL_NAME;

		} else if (card->name->family
			   && *card->name->family
			   && !(match & MATCHED_FAMILY_NAME)
			   && match_name_fragment (card->name->family, strv[i])) {
			
			this_match = MATCHED_FAMILY_NAME;
		}


		if (this_match != MATCHED_NOTHING) {
			match_len += len;
			match |= this_match;
			if (first_match == 0)
				first_match = this_match;
		} else {
			match = first_match = 0;
			break;
		}

	}
	
	score = match_len * 3; /* three points per match character */

	if (card->nickname) {
		/* We massively boost the score if the nickname exists and is the same as one of the "real" names.  This keeps the
		   nickname from matching ahead of the real name for this card. */
		len = strlen (card->nickname);
		if ((card->name->given && !g_utf8_strncasecmp (card->name->given, card->nickname, MIN (strlen (card->name->given), len)))
		    || (card->name->family && !g_utf8_strncasecmp (card->name->family, card->nickname, MIN (strlen (card->name->family), len)))
		    || (card->name->additional && !g_utf8_strncasecmp (card->name->additional, card->nickname, MIN (strlen (card->name->additional), len))))
			score *= 100;
	}

#if 0
	/* This leads to some pretty counter-intuitive results, so I'm disabling it. */
	email = e_destination_get_email (dest);
	if (email) {
		/* Do the same for the email address. */
		gchar *at = strchr (email, '@');
		len = at ? at-email : strlen (email);
		if ((card->name->given && !g_utf8_strncasecmp (card->name->given, email, MIN (strlen (card->name->given), len)))
		    || (card->name->family && !g_utf8_strncasecmp (card->name->family, email, MIN (strlen (card->name->family), len)))
		    || (card->name->additional && !g_utf8_strncasecmp (card->name->additional, email, MIN (strlen (card->name->additional), len))))
			score *= 100;
	}
#endif

	have_given       = card->name->given && *card->name->given;
	have_additional  = card->name->additional && *card->name->additional;
	have_family      = card->name->family && *card->name->family;

	if (first_match == MATCHED_GIVEN_NAME) {

		if (have_family)
			menu_text = g_strdup_printf ("%s %s", card->name->given, card->name->family);
		else
			menu_text = g_strdup_printf (card->name->given);

	} else if (first_match == MATCHED_ADDITIONAL_NAME) {

		if (have_family) {
			
			menu_text = g_strdup_printf ("%s, %s%s%s",
						     card->name->family,
						     have_given ? card->name->given : "",
						     have_given ? " " : "",
						     card->name->additional);

		} else {

			menu_text = g_strdup_printf ("%s%s%s",
						     have_given ? card->name->given : "",
						     have_given ?  " " : "",
						     card->name->additional);

		}

	} else if (first_match == MATCHED_FAMILY_NAME) {

		if (have_given)
			menu_text = g_strdup_printf ("%s, %s %s",
						     card->name->family,
						     card->name->given,
						     have_additional ? card->name->additional : "");
		else
			menu_text = g_strdup_printf (card->name->family);
	}

	if (menu_text) {
		final_match = make_match (dest, menu_text, score);
		g_free (menu_text);
	}
	
	return final_match;
}

/*
 * Initials Query
 */

static gchar *
sexp_initials (ESelectNamesCompletion *comp)
{
	return NULL;
}

static ECompletionMatch *
match_initials (ESelectNamesCompletion *comp, EDestination *dest)
{
	return NULL;
}


typedef struct _BookQuery BookQuery;
struct _BookQuery {
	gboolean primary;
	BookQuerySExp builder;
	BookQueryMatchTester tester;
};

static BookQuery book_queries[] = {
	{ TRUE,  sexp_nickname, match_nickname},
	{ TRUE,  sexp_email,    match_email },
	{ TRUE,  sexp_name,     match_name },
	{ FALSE, sexp_initials, match_initials }
};
static gint book_query_count = sizeof (book_queries) / sizeof (BookQuery);

/*
 * Build up a big compound sexp corresponding to all of our queries.
 */
static gchar *
book_query_sexp (ESelectNamesCompletion *comp)
{
	gint i, j, count = 0;
	gchar **queryv, *query;

	g_return_val_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp), NULL);

	if (! (comp->priv->query_text && *comp->priv->query_text))
		return NULL;

	if (comp->priv->primary_only) {
		for (i=0; i<book_query_count; ++i)
			if (book_queries[i].primary)
				++count;
	} else {
		count = book_query_count;
	}

	queryv = g_new0 (gchar *, count+1);
	for (i=0, j=0; i<count; ++i) {
		queryv[j] = book_queries[i].builder (comp);
		if (queryv[j])
			++j;
	}

	if (j == 0) {
		query = NULL;
	} else if (j == 1) {
		query = queryv[0];
		queryv[0] = NULL;
	} else {
		gchar *tmp = g_strjoinv (" ", queryv);
		query = g_strdup_printf ("(or %s)", tmp);
		g_free (tmp);
	}

	for (i=0; i<count; ++i)
		g_free (queryv[i]);
	g_free (queryv);

	return query;
}

/*
 * Sweep across all of our query rules and find the best score/match
 * string that applies to a given destination.
 */
static ECompletionMatch *
book_query_score (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *best_match = NULL;
	gint i;

	g_return_val_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp), NULL);
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	for (i=0; i<book_query_count; ++i) {

		ECompletionMatch *this_match = NULL;

		if (book_queries[i].primary || !comp->priv->primary_only) {
			if (book_queries[i].tester && e_destination_get_card (dest))
				this_match = book_queries[i].tester (comp, dest);

			if (this_match) {
				if (best_match == NULL || this_match->score > best_match->score) {
					e_completion_match_unref (best_match);
					best_match = this_match;
				} else {
					e_completion_match_unref (this_match);
				}
			}
		}
	}

	if (best_match)
		emailify_match (best_match);

	return best_match;
}

static void
book_query_process_card_list (ESelectNamesCompletion *comp, const GList *cards)
{
	while (cards) {
		ECard *card = E_CARD (cards->data);

		if (card->email) {
			gint i;
			for (i=0; i<e_list_length (card->email); ++i) {
				EDestination *dest = e_destination_new ();
				const gchar *email;
				ECompletionMatch *match;
				
				e_destination_set_card (dest, card, i);
				email = e_destination_get_email (dest);

				if (email && *email) {
				
					match = book_query_score (comp, dest);
					if (match && match->score > 0) {
						e_completion_found_match (E_COMPLETION (comp), match);
					} else {
						e_completion_match_unref (match);
					}

					gtk_object_unref (GTK_OBJECT (dest));
				}
			}
		}
		
		cards = g_list_next (cards);
	}
}

#if 0
static gchar *
initials_query_match_cb (QueryInfo *qi, ECard *card, double *score)
{
	gint len;
	gchar f='\0', m='\0', l='\0'; /* initials */
	gchar cf, cm, cl;

	len = strlen (qi->comp->priv->query_text);
	
	if (len == 2) {
		
		f = qi->comp->priv->query_text[0];
		m = '\0';
		l = qi->comp->priv->query_text[1];

	} else if (len == 3) {

		f = qi->comp->priv->query_text[0];
		m = qi->comp->priv->query_text[1];
		l = qi->comp->priv->query_text[2];

	} else {
		return NULL;
	}

	cf = card->name->given ? *card->name->given : '\0';
	cm = card->name->additional ? *card->name->additional : '\0';
	cl = card->name->family ? *card->name->family : '\0';

	if (f && isupper ((gint) f))
		f = tolower ((gint) f);
	if (m && isupper ((gint) m))
		m = tolower ((gint) m);
	if (l && isupper ((gint) l))
		l = tolower ((gint) l);

	if (cf && isupper ((gint) cf))
		cf = tolower ((gint) cf);
	if (cm && isupper ((gint) cm))
		cm = tolower ((gint) cm);
	if (cl && isupper ((gint) cl))
		cl = tolower ((gint) cl);

	if ((f == '\0' || (f == cf)) && (m == '\0' || (m == cm)) && (l == '\0' || (l == cl))) {
		if (score)
			*score = 3;
		if (m)
			return g_strdup_printf ("%s %s %s", card->name->given, card->name->additional, card->name->family);
		else
			return g_strdup_printf ("%s %s", card->name->given, card->name->family);
	}

	return NULL;
}

static gboolean
start_initials_query (ESelectNamesCompletion *comp)
{
	gint len;
	gchar *query;

	if (comp && comp->priv->query_text && *(comp->priv->query_text)) {
		
		len = strlen (comp->priv->query_text);
		if (len < 2 || len > 3)
			return FALSE;

		query = g_strdup_printf ("(contains \"x-evolution-any-field\" \"%c\")", *(comp->priv->query_text));
		query_info_start (comp, comp->priv->query_text, query, initials_query_match_cb);
		g_free (query);
		return TRUE;
	}

	return FALSE;
}
#endif


/*
 *
 * ESelectNamesCompletion code
 *
 */


GtkType
e_select_names_completion_get_type (void)
{
	static GtkType select_names_complete_type = 0;
  
	if (!select_names_complete_type) {
		GtkTypeInfo select_names_complete_info = {
			"ESelectNamesCompletion",
			sizeof (ESelectNamesCompletion),
			sizeof (ESelectNamesCompletionClass),
			(GtkClassInitFunc) e_select_names_completion_class_init,
			(GtkObjectInitFunc) e_select_names_completion_init,
			NULL, NULL, /* reserved */
			(GtkClassInitFunc) NULL
		};

		select_names_complete_type = gtk_type_unique (e_completion_get_type (), &select_names_complete_info);
	}

	return select_names_complete_type;
}

static void
e_select_names_completion_class_init (ESelectNamesCompletionClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	ECompletionClass *completion_class = E_COMPLETION_CLASS (klass);

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (e_completion_get_type ()));

	object_class->destroy = e_select_names_completion_destroy;

	completion_class->begin_completion = e_select_names_completion_begin;
	completion_class->end_completion = e_select_names_completion_end;
	completion_class->cancel_completion = e_select_names_completion_cancel;

	if (getenv ("EVO_DEBUG_SELECT_NAMES_COMPLETION")) {
		out = fopen ("/tmp/evo-debug-select-names-completion", "w");
		if (out)
			setvbuf (out, NULL, _IONBF, 0);
	}
}

static void
e_select_names_completion_init (ESelectNamesCompletion *comp)
{
	comp->priv = g_new0 (struct _ESelectNamesCompletionPrivate, 1);
}

static void
e_select_names_completion_destroy (GtkObject *object)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (object);

	if (comp->priv->model)
		gtk_object_unref (GTK_OBJECT (comp->priv->model));

	if (comp->priv->book)
		gtk_object_unref (GTK_OBJECT (comp->priv->book));

	if (comp->priv->book_view)
		gtk_object_unref (GTK_OBJECT (comp->priv->book_view));
	
	g_free (comp->priv->waiting_query);
	g_free (comp->priv->query_text);

	g_free (comp->priv->cached_query_text);
	g_list_foreach (comp->priv->cached_cards, (GFunc)gtk_object_unref, NULL);
	g_list_free (comp->priv->cached_cards);

	g_free (comp->priv);

	if (parent_class->destroy)
		parent_class->destroy (object);
}


/*
 *
 *  EBook/EBookView Callbacks & Query Stuff
 *
 */

static gchar *
clean_query_text (const gchar *s)
{
	gchar *q = g_new (gchar, strlen(s)+1), *t;

	t = q;
	while (*s) {
		if (*s != ',' && *s != '"') {
			*t = *s;
			++t;
		}
		++s;
	}
	*t = '\0';

	return q;
}

static void
e_select_names_completion_clear_cache (ESelectNamesCompletion *comp)
{
	g_free (comp->priv->cached_query_text);
	comp->priv->cached_query_text = NULL;

	g_list_foreach (comp->priv->cached_cards, (GFunc)gtk_object_unref, NULL);
	g_list_free (comp->priv->cached_cards);
	comp->priv->cached_cards = NULL;
}

static void
e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data)
{
	ESelectNamesCompletion *comp;

	if (status != E_BOOK_STATUS_SUCCESS)
		return;

	comp = E_SELECT_NAMES_COMPLETION (user_data);

	comp->priv->cancelled = FALSE;
	
	comp->priv->book_view_tag = 0;
	
	comp->priv->book_view = view;
	gtk_object_ref (GTK_OBJECT (view));

	gtk_signal_connect (GTK_OBJECT (view),
			    "card_added",
			    GTK_SIGNAL_FUNC (e_select_names_completion_card_added_cb),
			    comp);
	gtk_signal_connect (GTK_OBJECT (view),
			    "sequence_complete",
			    GTK_SIGNAL_FUNC (e_select_names_completion_seq_complete_cb),
			    comp);
}

static void
e_select_names_completion_card_added_cb (EBookView *book_view, const GList *cards, gpointer user_data)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (user_data);

	if (! comp->priv->cancelled)
		book_query_process_card_list (comp, cards);

	/* Save the list of matching cards. */
	while (cards) {
		comp->priv->cached_cards = g_list_prepend (comp->priv->cached_cards, cards->data);
		gtk_object_ref (GTK_OBJECT (cards->data));
		cards = g_list_next (cards);
	}
}

static void
e_select_names_completion_seq_complete_cb (EBookView *book_view, gpointer user_data)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (user_data);

	gtk_object_unref (GTK_OBJECT (comp->priv->book_view));

	comp->priv->book_view = NULL;

	g_free (comp->priv->query_text);
	comp->priv->query_text = NULL;

	if (out)
		fprintf (out, "ending search ");
	if (out && !e_completion_searching (E_COMPLETION (comp)))
		fprintf (out, "while not searching!");
	if (out)
		fprintf (out, "\n");
	e_completion_end_search (E_COMPLETION (comp)); /* That's all folks! */

	/* Need to launch a new completion if another one is pending. */
	if (comp->priv->waiting_query) {
		gchar *s = comp->priv->waiting_query;
		comp->priv->waiting_query = NULL;
		e_completion_begin_search (E_COMPLETION (comp), s, comp->priv->waiting_pos, comp->priv->waiting_limit);
		g_free (s);
	}
}

static void
e_select_names_completion_stop_query (ESelectNamesCompletion *comp)
{
	g_return_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp));

	if (out)
		fprintf (out, "stopping query\n");

	if (comp->priv->waiting_query) {
		if (out)
			fprintf (out, "stopped waiting query\n");
		g_free (comp->priv->waiting_query);
		comp->priv->waiting_query = NULL;
	}

	g_free (comp->priv->query_text);
	comp->priv->query_text = NULL;

	if (comp->priv->book_view_tag) {
		if (out)
			fprintf (out, "cancelled book view creation\n");
		e_book_cancel (comp->priv->book, comp->priv->book_view_tag);
		comp->priv->book_view_tag = 0;
	}

	if (comp->priv->book_view) {
		if (out)
			fprintf (out, "unrefed book view\n");
		gtk_object_unref (GTK_OBJECT (comp->priv->book_view));
		comp->priv->book_view = NULL;
	}

	/* Clear the cache, which may contain partial results. */
	e_select_names_completion_clear_cache (comp);

}

static void
e_select_names_completion_start_query (ESelectNamesCompletion *comp, const gchar *query_text)
{
	g_return_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp));
	g_return_if_fail (query_text);

	e_select_names_completion_stop_query (comp);  /* Stop any prior queries. */

	if (comp->priv->book_ready) {
		gchar *sexp;
	
		g_free (comp->priv->query_text);
		comp->priv->query_text = g_strdup (query_text);

		g_free (comp->priv->cached_query_text);
		comp->priv->cached_query_text = g_strdup (query_text);

		sexp = book_query_sexp (comp);
		if (sexp && *sexp) {

			if (out)
				fprintf (out, "\n\n**** starting query: \"%s\"\n", comp->priv->query_text);

			comp->priv->book_view_tag = e_book_get_book_view (comp->priv->book, sexp, 
									  e_select_names_completion_got_book_view_cb, comp);

			if (! comp->priv->book_view_tag)
				g_warning ("Exception calling e_book_get_book_view");

		} else {
			g_free (comp->priv->query_text);
		}
		g_free (sexp);

	} else {

		comp->priv->waiting_query = g_strdup (query_text);

	}
}

static void
e_select_names_completion_do_query (ESelectNamesCompletion *comp, const gchar *query_text, gint pos, gint limit)
{
	gchar *clean;
	gboolean query_is_still_running, can_reuse_cached_cards;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	query_is_still_running = comp->priv->book_view_tag || comp->priv->book_view;
	clean = clean_query_text (query_text);

	if (out) {
		fprintf (out, "do_query: %s => %s\n", query_text, clean);
		if (query_is_still_running)
			fprintf (out, "a query is still running!\n");
	}
	if (comp->priv->cached_query_text && out)
		fprintf (out, "cached: %s\n", comp->priv->cached_query_text);

	can_reuse_cached_cards = (comp->priv->cached_query_text
				  && (strlen (comp->priv->cached_query_text) <= strlen (clean))
				  && !g_utf8_strncasecmp (comp->priv->cached_query_text, clean, strlen (comp->priv->cached_query_text)));


	if (can_reuse_cached_cards) {

		if (out)
			fprintf (out, "can reuse cached card!\n");

		if (query_is_still_running) {
			g_free (comp->priv->waiting_query);
			comp->priv->waiting_query = clean;
			comp->priv->waiting_pos = pos;
			comp->priv->waiting_limit = limit;
			if (out)
				fprintf (out, "waiting for running query to complete: %s\n", comp->priv->waiting_query);
			return;
		}

		g_free (comp->priv->query_text);
		comp->priv->query_text = clean;
		if (out)
			fprintf (out, "using existing query info: %s (vs %s)\n", comp->priv->query_text, comp->priv->cached_query_text);
		comp->priv->cancelled = FALSE;
		book_query_process_card_list (comp, comp->priv->cached_cards);
		e_completion_end_search (E_COMPLETION (comp));
		return;
	}
	
	e_select_names_completion_start_query (comp, clean);
	g_free (clean);
}


/*
 *
 *  Completion Search Override - a Framework for Christian-Resurrection-Holiday Edible-Chicken-Embryos
 *
 */

typedef struct _SearchOverride SearchOverride;
struct _SearchOverride {
	const gchar *trigger;
	const gchar *text[4];
};
static SearchOverride override[] = { 
	{ "why?", { "\"I must create a system, or be enslaved by another man's.\"",
		    "            -- Wiliam Blake, \"Jerusalem\"",
		    NULL } },
	{ NULL, { NULL } } };

static gboolean
search_override_check (SearchOverride *over, const gchar *text)
{
	if (over == NULL || text == NULL)
		return FALSE;

	return !g_utf8_strcasecmp (over->trigger, text);
}


/*
 *
 *  Completion Callbacks
 *
 */

static void
e_select_names_completion_begin (ECompletion *comp, const gchar *text, gint pos, gint limit)
{
	ESelectNamesCompletion *selcomp = E_SELECT_NAMES_COMPLETION (comp);
	const gchar *str;
	gint index, j;
	
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));
	g_return_if_fail (text != NULL);
	
	if (out) {
		fprintf (out, "\n\n**** requesting completion\n");
		fprintf (out, "text=\"%s\" pos=%d limit=%d\n", text, pos, limit);
	}

	e_select_names_model_text_pos (selcomp->priv->model, pos, &index, NULL, NULL);
	str = index >= 0 ? e_select_names_model_get_string (selcomp->priv->model, index) : NULL;

	if (out)
		fprintf (out, "index=%d str=\"%s\"\n", index, str);

	if (str == NULL || *str == '\0') {
		if (out)
			fprintf (out, "aborting empty query\n");
		e_completion_end_search (comp);
		return;
	}

	for (j=0; override[j].trigger; ++j) {
		if (search_override_check (&(override[j]), str)) {
			gint k;

			for (k=0; override[j].text[k]; ++k) {
				ECompletionMatch *match = g_new (ECompletionMatch, 1);
				e_completion_match_construct (match);
				e_completion_match_set_text (match, text, override[j].text[k]);
				match->score = 1;
				e_completion_found_match (comp, match);
			}

			e_completion_end_search (comp);
			return;
		}
	}

	e_select_names_completion_do_query (selcomp, str, pos, limit);
}

static void
e_select_names_completion_end (ECompletion *comp)
{
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_COMPLETION (comp));

	if (out)
		fprintf (out, "completion ended\n");
}

static void
e_select_names_completion_cancel (ECompletion *comp)
{
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_COMPLETION (comp));

	E_SELECT_NAMES_COMPLETION (comp)->priv->cancelled = TRUE;
	
	if (out)
		fprintf (out, "completion cancelled\n");
}

static void
e_select_names_completion_book_ready (EBook *book, EBookStatus status, ESelectNamesCompletion *comp)
{
	comp->priv->book_ready = TRUE;

	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	/* If waiting_query is non-NULL, someone tried to start a query before the book was ready.
	   Now that it is, get started. */
	if (comp->priv->waiting_query) {
		e_select_names_completion_start_query (comp, comp->priv->waiting_query);
		g_free (comp->priv->waiting_query);
		comp->priv->waiting_query = NULL;
	}

	gtk_object_unref (GTK_OBJECT (comp)); /* post-async unref */
}


/*
 *
 *  Our Pseudo-Constructor
 *
 */

ECompletion *
e_select_names_completion_new (EBook *book, ESelectNamesModel *model)
{
	ESelectNamesCompletion *comp;

	g_return_val_if_fail (book == NULL || E_IS_BOOK (book), NULL);
	g_return_val_if_fail (model, NULL);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), NULL);

	comp = (ESelectNamesCompletion *) gtk_type_new (e_select_names_completion_get_type ());

	if (book == NULL) {

		comp->priv->book = e_book_new ();
		gtk_object_ref (GTK_OBJECT (comp->priv->book));
		gtk_object_sink (GTK_OBJECT (comp->priv->book));

		comp->priv->book_ready = FALSE;
		gtk_object_ref (GTK_OBJECT (comp)); /* ref ourself before our async call */
		e_book_load_local_address_book (comp->priv->book, (EBookCallback) e_select_names_completion_book_ready, comp);

	} else {
		comp->priv->book = book;
		gtk_object_ref (GTK_OBJECT (comp->priv->book));
		comp->priv->book_ready = TRUE;
	}
		
	comp->priv->model = model;
	gtk_object_ref (GTK_OBJECT (model));

	return E_COMPLETION (comp);
}


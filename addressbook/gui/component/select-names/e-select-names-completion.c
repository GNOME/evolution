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
#include "e-select-names-completion.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <gtk/gtksignal.h>
#include <libgnome/gnome-util.h>

#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/backend/ebook/e-destination.h>
#include <addressbook/backend/ebook/e-card-simple.h>
#include <addressbook/backend/ebook/e-card-compare.h>

#define MINIMUM_QUERY_LENGTH 3

typedef struct {
	EBook *book;
	guint book_view_tag;
	EBookView *book_view;
	ESelectNamesCompletion *comp;
	guint card_added_tag;
	guint seq_complete_tag;
	gboolean sequence_complete_received;
} ESelectNamesCompletionBookData;

struct _ESelectNamesCompletionPrivate {

	ESelectNamesTextModel *text_model;

	GList *book_data;
	gint books_not_ready;
	gint pending_completion_seq;

	gchar *waiting_query;
	gint waiting_pos, waiting_limit;
	gchar *query_text;

	gchar *cached_query_text;
	GList *cached_cards;
	gboolean cache_complete;

	gboolean match_contact_lists;
	gboolean primary_only;

	gboolean can_fail_due_to_too_many_hits; /* like LDAP, for example... */
};

static void e_select_names_completion_class_init (ESelectNamesCompletionClass *);
static void e_select_names_completion_init (ESelectNamesCompletion *);
static void e_select_names_completion_destroy (GtkObject *object);

static void e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data);
static void e_select_names_completion_card_added_cb    (EBookView *, const GList *cards, gpointer user_data);
static void e_select_names_completion_seq_complete_cb  (EBookView *, EBookViewStatus status, gpointer user_data);

static void e_select_names_completion_do_query (ESelectNamesCompletion *, const gchar *query_text, gint pos, gint limit);

static void e_select_names_completion_handle_request  (ECompletion *, const gchar *txt, gint pos, gint limit);
static void e_select_names_completion_end    (ECompletion *);

static GtkObjectClass *parent_class;

static FILE *out;

/*
 *
 * Query builders
 *
 */

typedef gchar *(*BookQuerySExp) (ESelectNamesCompletion *);
typedef ECompletionMatch *(*BookQueryMatchTester) (ESelectNamesCompletion *, EDestination *);

static int
utf8_casefold_collate_len (const gchar *str1, const gchar *str2, int len)
{
	gchar *s1 = g_utf8_casefold(str1, len);
	gchar *s2 = g_utf8_casefold(str2, len);
	int rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

static int
utf8_casefold_collate (const gchar *str1, const gchar *str2)
{
	return utf8_casefold_collate_len (str1, str2, -1);
}

static void
our_match_destroy (ECompletionMatch *match)
{
	g_object_unref (match->user_data);
}

static ECompletionMatch *
make_match (EDestination *dest, const gchar *menu_form, double score)
{
	ECompletionMatch *match;
	ECard *card = e_destination_get_card (dest);

	match = e_completion_match_new (e_destination_get_name (dest), menu_form, score);

	e_completion_match_set_text (match, e_destination_get_name (dest), menu_form);

	/* Reject any match that has null text fields. */
	if (! (e_completion_match_get_match_text (match) && e_completion_match_get_menu_text (match))) {
		g_object_unref (match);
		return NULL;
	}

	/* Since we sort low to high, we negate so that larger use scores will come first */
	match->sort_major = card ? -floor (e_card_get_use_score (card)) : 0;

	match->sort_minor = e_destination_get_email_num (dest);

	match->user_data = dest;
	g_object_ref (dest);

	match->destroy = our_match_destroy;

	return match;
}

/*
 * Nickname query
 */

static gchar *
sexp_nickname (ESelectNamesCompletion *comp)
{
	gchar *query = g_strdup_printf ("(beginswith \"nickname\" \"%s\")", comp->priv->query_text);

	return query;
}

static ECompletionMatch *
match_nickname (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *match = NULL;
	gint len;
	ECard *card = e_destination_get_card (dest);
	double score;

	if (card->nickname == NULL)
		return NULL;

	len = g_utf8_strlen (comp->priv->query_text, -1);
	if (card->nickname && !utf8_casefold_collate_len (comp->priv->query_text, card->nickname, len)) {
		const gchar *name;
		gchar *str;

		score = len * 2; /* nickname gives 2 points per matching character */

		if (len == g_utf8_strlen (card->nickname, -1)) /* boost score on an exact match */
		    score *= 10;

		name = e_destination_get_name (dest);
		if (name && *name)
			str = g_strdup_printf ("'%s' %s <%s>", card->nickname, name, e_destination_get_email (dest));
		else
			str = g_strdup_printf ("'%s' <%s>", card->nickname, e_destination_get_email (dest));

		match = make_match (dest, str, score);
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
	const gchar *name = e_destination_get_name (dest);
	const gchar *email = e_destination_get_email (dest);
	double score;
	
	if (email
	    && !utf8_casefold_collate_len (comp->priv->query_text, email, len)
	    && !e_destination_is_evolution_list (dest)) {
		
		gchar *str;
		
		score = len * 2; /* 2 points for each matching character */

		if (name && *name)
			str = g_strdup_printf ("<%s> %s", email, name);
		else
			str = g_strdup (email);

		match = make_match (dest, str, score);

		g_free (str);

		return match;
	}

	return NULL;
}

/*
 * Name Query
 */

static gchar *
name_style_query (ESelectNamesCompletion *comp, const gchar *field)
{
	if (comp && comp->priv->query_text && *comp->priv->query_text) {
		gchar *cpy = g_strdup (comp->priv->query_text), *c;
		gchar **strv;
		gchar *query;
		gint i, count=0;

		for (c = cpy; *c; ++c) {
			if (*c == ',')
				*c = ' ';
		}

		strv = g_strsplit (cpy, " ", 0);
		for (i=0; strv[i]; ++i) {
			gchar *old;
			++count;
			g_strstrip (strv[i]);
			old = strv[i];
			strv[i] = g_strdup_printf ("(beginswith \"%s\" \"%s\")", field, old);
			g_free (old);
		}

		if (count == 1) {
			query = strv[0];
			strv[0] = NULL;
		} else {
			gchar *joined = g_strjoinv (" ", strv);
			query = g_strdup_printf ("(and %s)", joined);
			g_free (joined);
		}

		g_free (cpy);
		g_strfreev (strv);

		return query;
	}

	return NULL;
}

static gchar *
sexp_name (ESelectNamesCompletion *comp)
{
	return name_style_query (comp, "full_name");
}

static ECompletionMatch *
match_name (ESelectNamesCompletion *comp, EDestination *dest)
{
	ECompletionMatch *final_match = NULL;
	gchar *menu_text = NULL;
	ECard *card;
	const gchar *email;
	gint match_len = 0;
	ECardMatchType match;
	ECardMatchPart first_match;
	double score = 0;
	gboolean have_given, have_additional, have_family;

	card = e_destination_get_card (dest);
	
	if (card->name == NULL)
		return NULL;

	email = e_destination_get_email (dest);

	match = e_card_compare_name_to_string_full (card, comp->priv->query_text, TRUE /* yes, allow partial matches */,
						    NULL, &first_match, &match_len);

	if (match <= E_CARD_MATCH_NONE)
		return NULL;

	score = match_len * 3; /* three points per match character */

#if 0
	if (card->nickname) {
		/* We massively boost the score if the nickname exists and is the same as one of the "real" names.  This keeps the
		   nickname from matching ahead of the real name for this card. */
		len = strlen (card->nickname);
		if ((card->name->given && !utf8_casefold_collate_len (card->name->given, card->nickname, MIN (strlen (card->name->given), len)))
		    || (card->name->family && !utf8_casefold_collate_len (card->name->family, card->nickname, MIN (strlen (card->name->family), len)))
		    || (card->name->additional && !utf8_casefold_collate_len (card->name->additional, card->nickname, MIN (strlen (card->name->additional), len))))
			score *= 100;
	}
#endif

	have_given       = card->name->given && *card->name->given;
	have_additional  = card->name->additional && *card->name->additional;
	have_family      = card->name->family && *card->name->family;

	if (e_card_evolution_list (card)) {

		menu_text = e_card_name_to_string (card->name);

	} else if (first_match == E_CARD_MATCH_PART_GIVEN_NAME) {

		if (have_family)
			menu_text = g_strdup_printf ("%s %s <%s>", card->name->given, card->name->family, email);
		else
			menu_text = g_strdup_printf ("%s <%s>", card->name->given, email);

	} else if (first_match == E_CARD_MATCH_PART_ADDITIONAL_NAME) {

		if (have_given) {

			menu_text = g_strdup_printf ("%s%s%s, %s <%s>",
						     card->name->additional,
						     have_family ? " " : "",
						     have_family ? card->name->family : "",
						     card->name->given,
						     email);
		} else {

			menu_text = g_strdup_printf ("%s%s%s <%s>",
						     card->name->additional,
						     have_family ? " " : "",
						     have_family ? card->name->family : "",
						     email);
		}

	} else if (first_match == E_CARD_MATCH_PART_FAMILY_NAME) { 

		if (have_given)
			menu_text = g_strdup_printf ("%s, %s%s%s <%s>",
						     card->name->family,
						     card->name->given,
						     have_additional ? " " : "",
						     have_additional ? card->name->additional : "",
						     email);
		else
			menu_text = g_strdup_printf ("%s <%s>", card->name->family, email);

	} else { /* something funny happened */

		menu_text = g_strdup_printf ("<%s> ???", email);

	}

	if (menu_text) {
		g_strstrip (menu_text);
		final_match = make_match (dest, menu_text, score);
		g_free (menu_text);
	}
	
	return final_match;
}

/*
 * File As Query
 */

static gchar *
sexp_file_as (ESelectNamesCompletion *comp)
{
	return name_style_query (comp, "file_as");
}

static ECompletionMatch *
match_file_as (ESelectNamesCompletion *comp, EDestination *dest)
{
	const gchar *name;
	const gchar *email;
	gchar *cpy, **strv, *menu_text;
	gint i, len;
	double score = 0.00001;
	ECompletionMatch *match;

	name = e_destination_get_name (dest);
	email = e_destination_get_email (dest);

	if (!(name && *name))
		return NULL;

	cpy = g_strdup (comp->priv->query_text);
	strv = g_strsplit (cpy, " ", 0);

	for (i=0; strv[i] && score > 0; ++i) {
		len = g_utf8_strlen (strv[i], -1);
		if (!utf8_casefold_collate_len (name, strv[i], len))
			score += len; /* one point per character of the match */
		else
			score = 0;
	}
	
	g_free (cpy);
	g_strfreev (strv);

	if (score <= 0)
		return NULL;
	
	menu_text = g_strdup_printf ("%s <%s>", name, email);
	g_strstrip (menu_text);
	match = make_match (dest, menu_text, score);
	g_free (menu_text);

	return match;
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
	{ TRUE,  sexp_file_as,  match_file_as },
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

	g_return_val_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp), NULL);
	g_return_val_if_fail (E_IS_DESTINATION (dest), NULL);

	if (! (comp->priv->query_text && *comp->priv->query_text))
		return NULL;
	
	for (i=0; i<book_query_count; ++i) {

		ECompletionMatch *this_match = NULL;

		if (book_queries[i].primary || !comp->priv->primary_only) {
			if (book_queries[i].tester && e_destination_get_card (dest)) {
				this_match = book_queries[i].tester (comp, dest);
			}

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

	return best_match;
}

static void
book_query_process_card_list (ESelectNamesCompletion *comp, const GList *cards)
{
	while (cards) {
		ECard *card = E_CARD (cards->data);

		if (e_card_evolution_list (card)) {

			if (comp->priv->match_contact_lists) {

				EDestination *dest = e_destination_new ();
				ECompletionMatch *match;
				e_destination_set_card (dest, card, 0);
				match = book_query_score (comp, dest);
				if (match && match->score > 0) {
					e_completion_found_match (E_COMPLETION (comp), match);
				} else {
					e_completion_match_unref (match);
				}
				g_object_unref (dest);

			}

		} else if (card->email) {
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
				}

				g_object_unref (dest);
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

	completion_class->request_completion = e_select_names_completion_handle_request;
	completion_class->end_completion = e_select_names_completion_end;

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
	comp->priv->match_contact_lists = TRUE;
}

static void
e_select_names_completion_clear_book_data (ESelectNamesCompletion *comp)
{
	GList *l;

	for (l = comp->priv->book_data; l; l = l->next) {
		ESelectNamesCompletionBookData *book_data = l->data;

		if (book_data->card_added_tag) {
			gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->card_added_tag);
			book_data->card_added_tag = 0;
		}

		if (book_data->seq_complete_tag) {
			gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->seq_complete_tag);
			book_data->seq_complete_tag = 0;
		}

		g_object_unref (book_data->book);

		if (book_data->book_view) {
			e_book_view_stop (book_data->book_view);
			g_object_unref (book_data->book_view);
		}

		g_free (book_data);
	}
	g_list_free (comp->priv->book_data);
	comp->priv->book_data = NULL;
}

static void
e_select_names_completion_destroy (GtkObject *object)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (object);

	if (comp->priv->text_model)
		g_object_unref (comp->priv->text_model);

	e_select_names_completion_clear_book_data (comp);

	g_free (comp->priv->waiting_query);
	g_free (comp->priv->query_text);

	g_free (comp->priv->cached_query_text);
	g_list_foreach (comp->priv->cached_cards, (GFunc)g_object_unref, NULL);
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

	g_strstrip (q);

	return q;
}

static void
e_select_names_completion_clear_cache (ESelectNamesCompletion *comp)
{
	g_free (comp->priv->cached_query_text);
	comp->priv->cached_query_text = NULL;

	g_list_foreach (comp->priv->cached_cards, (GFunc)g_object_unref, NULL);
	g_list_free (comp->priv->cached_cards);
	comp->priv->cached_cards = NULL;
}

static void
e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data)
{
	ESelectNamesCompletion *comp;
	ESelectNamesCompletionBookData *book_data;

	if (status != E_BOOK_STATUS_SUCCESS)
		return;

	book_data = (ESelectNamesCompletionBookData*)user_data;
	comp = book_data->comp;

	book_data->book_view_tag = 0;

	if (book_data->card_added_tag) {
		gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->card_added_tag);
		book_data->card_added_tag = 0;
	}
	if (book_data->seq_complete_tag) {
		gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->seq_complete_tag);
		book_data->seq_complete_tag = 0;
	}

	g_object_ref (view);
	if (book_data->book_view) {
		e_book_view_stop (book_data->book_view);
		g_object_unref (book_data->book_view);
	}
	book_data->book_view = view;

	book_data->card_added_tag = 
		g_signal_connect (view,
				    "card_added",
				    G_CALLBACK (e_select_names_completion_card_added_cb),
				    book_data);

	book_data->seq_complete_tag =
		g_signal_connect (view,
				    "sequence_complete",
				    G_CALLBACK (e_select_names_completion_seq_complete_cb),
				    book_data);
	book_data->sequence_complete_received = FALSE;
	comp->priv->pending_completion_seq++;
}

static void
e_select_names_completion_card_added_cb (EBookView *book_view, const GList *cards, gpointer user_data)
{
	ESelectNamesCompletionBookData *book_data = user_data;
	ESelectNamesCompletion *comp = book_data->comp;

	if (e_completion_searching (E_COMPLETION (comp))) {
		book_query_process_card_list (comp, cards);

		/* Save the list of matching cards. */
		while (cards) {
			comp->priv->cached_cards = g_list_prepend (comp->priv->cached_cards, cards->data);
			g_object_ref (cards->data);
			cards = g_list_next (cards);
		}
	}
}

static void
e_select_names_completion_seq_complete_cb (EBookView *book_view, EBookViewStatus status, gpointer user_data)
{
	ESelectNamesCompletionBookData *book_data = user_data;
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION(book_data->comp);

	/*
	 * We aren't searching, but the addressbook has changed -- clear our card cache so that
	 * future completion requests will take the changes into account.
	 */
	if (! e_completion_searching (E_COMPLETION (comp))) {
		e_select_names_completion_clear_cache (comp);
		return;
	}

	if (!book_data->sequence_complete_received) {
		book_data->sequence_complete_received = TRUE;
		comp->priv->pending_completion_seq --;
		if (comp->priv->pending_completion_seq > 0)
			return;
	}

	if (comp->priv->cached_query_text
	    && !comp->priv->cache_complete
	    && !strcmp (comp->priv->cached_query_text, comp->priv->query_text))
		comp->priv->cache_complete = TRUE;

	g_free (comp->priv->query_text);
	comp->priv->query_text = NULL;

	if (out)
		fprintf (out, "ending search\n");

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
	GList *l;

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

	for (l = comp->priv->book_data; l; l = l->next) {
		ESelectNamesCompletionBookData *book_data = l->data;
		if (book_data->book_view_tag) {
			if (out)
				fprintf (out, "cancelled book view creation\n");
			e_book_cancel (book_data->book, book_data->book_view_tag);
			book_data->book_view_tag = 0;
		}
		if (book_data->book_view) {
			if (out)
				fprintf (out, "disconnecting book view signals\n");

			if (book_data->card_added_tag) {
				gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->card_added_tag);
				book_data->card_added_tag = 0;
			}
			if (book_data->seq_complete_tag) {
				gtk_signal_disconnect (GTK_OBJECT (book_data->book_view), book_data->seq_complete_tag);
				book_data->seq_complete_tag = 0;
			}
	
			if (out)
				fprintf (out, "unrefed book view\n");

			e_book_view_stop (book_data->book_view);
			g_object_unref (book_data->book_view);
			book_data->book_view = NULL;
		}
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

	if (comp->priv->books_not_ready == 0) {
		gchar *sexp;
	
		if (strlen (query_text) < MINIMUM_QUERY_LENGTH)
			return;


		g_free (comp->priv->query_text);
		comp->priv->query_text = g_strdup (query_text);

		g_free (comp->priv->cached_query_text);
		comp->priv->cached_query_text = g_strdup (query_text);
		comp->priv->cache_complete = FALSE;

		sexp = book_query_sexp (comp);
		if (sexp && *sexp) {
			GList *l;

			if (out)
				fprintf (out, "\n\n**** starting query: \"%s\"\n", comp->priv->query_text);

			for (l = comp->priv->book_data; l; l = l->next) {
				ESelectNamesCompletionBookData *book_data = l->data;
				book_data->book_view_tag = e_book_get_completion_view (book_data->book,
										       sexp, 
										       e_select_names_completion_got_book_view_cb, book_data);
				if (! book_data->book_view_tag)
					g_warning ("Exception calling e_book_get_completion_view");
			}

		} else {
			g_free (comp->priv->query_text);
			comp->priv->query_text = NULL;
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
	GList *l;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	clean = clean_query_text (query_text);
	if (! (clean && *clean)) {
		g_free (clean);
		e_completion_end_search (E_COMPLETION (comp));
		return;
	}

	query_is_still_running = FALSE;
	for (l = comp->priv->book_data; l; l = l->next) {
		ESelectNamesCompletionBookData *book_data = l->data;
		query_is_still_running = book_data->book_view_tag;
		if (query_is_still_running)
			break;
	}

	if (out) {
		fprintf (out, "do_query: %s => %s\n", query_text, clean);
		if (query_is_still_running)
			fprintf (out, "a query is still running!\n");
	}
	if (comp->priv->cached_query_text && out)
		fprintf (out, "cached: %s\n", comp->priv->cached_query_text);

	can_reuse_cached_cards = (comp->priv->cached_query_text
				  && comp->priv->cache_complete
				  && (!comp->priv->can_fail_due_to_too_many_hits || comp->priv->cached_cards != NULL)
				  && (strlen (comp->priv->cached_query_text) <= strlen (clean))
				  && !utf8_casefold_collate_len (comp->priv->cached_query_text, clean, strlen (comp->priv->cached_query_text)));


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
		book_query_process_card_list (comp, comp->priv->cached_cards);
		e_completion_end_search (E_COMPLETION (comp));
		return;
	}
	
	e_select_names_completion_start_query (comp, clean);
	g_free (clean);
}


/*
 *
 *  Completion Search Override - a Framework for Christian-Resurrection-Holiday Edible-Chicken-Ova
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
	{ "easter-egg?", { "What were you expecting, a flight simulator?", NULL } },
	{ NULL, { NULL } } };

static gboolean
search_override_check (SearchOverride *over, const gchar *text)
{
	/* The g_utf8_validate is needed because as of 2001-06-11,
	 * EText doesn't translate from locale->UTF8 when you paste
	 * into it.
	 */
	if (over == NULL || text == NULL || !g_utf8_validate (text, -1, NULL))
		return FALSE;

	return !utf8_casefold_collate (over->trigger, text);
}


/*
 *
 *  Completion Callbacks
 *
 */

static void
e_select_names_completion_handle_request (ECompletion *comp, const gchar *text, gint pos, gint limit)
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

	e_select_names_model_text_pos (selcomp->priv->text_model->source,
				       selcomp->priv->text_model->seplen,
				       pos, &index, NULL, NULL);
	str = index >= 0 ? e_select_names_model_get_string (selcomp->priv->text_model->source, index) : NULL;

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
				match->score = 1 / (double) (k + 1);
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
check_capabilities (ESelectNamesCompletion *comp, EBook *book)
{
	gchar *cap = e_book_get_static_capabilities (book);
	comp->priv->can_fail_due_to_too_many_hits = !strcmp (cap, "net");
	if (comp->priv->can_fail_due_to_too_many_hits) {
		g_message ("using LDAP source for completion!");
	}
	g_free (cap);
}

#if 0
static void
e_select_names_completion_book_ready (EBook *book, EBookStatus status, ESelectNamesCompletion *comp)
{
	comp->priv->books_not_ready--;

	g_return_if_fail (E_IS_BOOK (book));
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	check_capabilities (comp, book);

	/* If waiting_query is non-NULL, someone tried to start a query before the book was ready.
	   Now that it is, get started. */
	if (comp->priv->books_not_ready == 0 && comp->priv->waiting_query) {
		e_select_names_completion_start_query (comp, comp->priv->waiting_query);
		g_free (comp->priv->waiting_query);
		comp->priv->waiting_query = NULL;
	}

	g_object_unref (comp); /* post-async unref */
}
#endif


/*
 *
 *  Our Pseudo-Constructor
 *
 */

ECompletion *
e_select_names_completion_new (ESelectNamesTextModel *text_model)
{
	ESelectNamesCompletion *comp;

	g_return_val_if_fail (E_IS_SELECT_NAMES_TEXT_MODEL (text_model), NULL);

	comp = (ESelectNamesCompletion *) gtk_type_new (e_select_names_completion_get_type ());

	comp->priv->text_model = text_model;
	g_object_ref (text_model);

	return E_COMPLETION (comp);
}

void
e_select_names_completion_add_book (ESelectNamesCompletion *comp, EBook *book)
{
	ESelectNamesCompletionBookData *book_data;

	g_return_if_fail (book != NULL);

	book_data = g_new0 (ESelectNamesCompletionBookData, 1);
	book_data->book = book;
	book_data->comp = comp;
	check_capabilities (comp, book);
	g_object_ref (book_data->book);
	comp->priv->book_data = g_list_append (comp->priv->book_data, book_data);

	/* if the user is typing as we're adding books, restart the
	   query after the new book has been added */
	if (comp->priv->query_text && *comp->priv->query_text) {
		char *query_text = g_strdup (comp->priv->query_text);
		e_select_names_completion_stop_query (comp);
		e_select_names_completion_start_query (comp, query_text);
		g_free (query_text);
	}
}

void
e_select_names_completion_clear_books (ESelectNamesCompletion *comp)
{
	e_select_names_completion_stop_query (comp);
	e_select_names_completion_clear_book_data (comp);
}

gboolean
e_select_names_completion_get_match_contact_lists (ESelectNamesCompletion *comp)
{
	g_return_val_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp), FALSE);
	return comp->priv->match_contact_lists;
}


void
e_select_names_completion_set_match_contact_lists (ESelectNamesCompletion *comp, gboolean x)
{
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));
	comp->priv->match_contact_lists = x;
}


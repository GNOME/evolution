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

#include <libebook/e-contact.h>
#include <addressbook/util/eab-book-util.h>
#include <libebook/e-destination.h>
#include <addressbook/gui/merging/eab-contact-compare.h>

#include <libedataserver/e-sexp.h>

typedef struct {
	EBook *book;
	guint book_view_tag;
	EBookView *book_view;
	ESelectNamesCompletion *comp;
	guint contacts_added_tag;
	guint seq_complete_tag;
	gboolean sequence_complete_received;

	gchar *cached_query_text;
	GList *cached_cards;
	gboolean cache_complete;

} ESelectNamesCompletionBookData;

struct _ESelectNamesCompletionPrivate {

	ESelectNamesTextModel *text_model;

	GList *book_data;
	gint books_not_ready;
	gint pending_completion_seq;

	gchar *waiting_query;
	gint waiting_pos, waiting_limit;
	gchar *query_text;

	gboolean match_contact_lists;

	gint minimum_query_length;
};

static void e_select_names_completion_class_init (ESelectNamesCompletionClass *);
static void e_select_names_completion_init (ESelectNamesCompletion *);
static void e_select_names_completion_dispose (GObject *object);

static void e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data);
static void e_select_names_completion_contacts_added_cb    (EBookView *, const GList *cards, gpointer user_data);
static void e_select_names_completion_seq_complete_cb  (EBookView *, EBookViewStatus status, gpointer user_data);

static void e_select_names_completion_do_query (ESelectNamesCompletion *, const gchar *query_text, gint pos, gint limit);

static void e_select_names_completion_handle_request  (ECompletion *, const gchar *txt, gint pos, gint limit);
static void e_select_names_completion_end    (ECompletion *);

static GObjectClass *parent_class;

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
#if notyet
	EContact *contact = e_destination_get_contact (dest);
#endif

	match = e_completion_match_new (e_destination_get_name (dest), menu_form, score);

	e_completion_match_set_text (match, e_destination_get_name (dest), menu_form);

	/* Reject any match that has null text fields. */
	if (! (e_completion_match_get_match_text (match) && e_completion_match_get_menu_text (match))) {
		g_object_unref (match);
		return NULL;
	}

#if notyet
	/* XXX toshok - EContact doesn't have the use_score stuff */
	/* Since we sort low to high, we negate so that larger use scores will come first */
	match->sort_major = contact ? -floor (e_contact_get_use_score (contact)) : 0;
#else
	match->sort_major = 0;
#endif

	match->sort_minor = e_destination_get_email_num (dest);

	match->user_data = g_object_ref (dest);

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
	EContact *contact = e_destination_get_contact (dest);
	double score;
	const char *nickname;

	nickname = e_contact_get_const (contact, E_CONTACT_NICKNAME);
	if (nickname == NULL)
		return NULL;

	len = g_utf8_strlen (comp->priv->query_text, -1);
	if (nickname && !utf8_casefold_collate_len (comp->priv->query_text, nickname, len)) {
		const gchar *name;
		gchar *str;

		score = len * 2; /* nickname gives 2 points per matching character */

		if (len == g_utf8_strlen (nickname, -1)) /* boost score on an exact match */
		    score *= 10;

		name = e_destination_get_name (dest);
		if (name && *name)
			str = g_strdup_printf ("'%s' %s <%s>", nickname, name, e_destination_get_email (dest));
		else
			str = g_strdup_printf ("'%s' <%s>", nickname, e_destination_get_email (dest));

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
		gint i;
		GString *out = g_string_new("");

		for (c = cpy; *c; ++c) {
			if (*c == ',')
				*c = ' ';
		}

		strv = g_strsplit (cpy, " ", 0);
		if (strv[0] && strv[1])
			g_string_append(out, "(and ");
		for (i=0; strv[i]; ++i) {
			if (i==0)
				g_string_append(out, "(beginswith ");
			else
				g_string_append(out, " (beginswith ");
			e_sexp_encode_string(out, field);
			g_strstrip(strv[i]);
			e_sexp_encode_string(out, strv[i]);
			g_string_append(out, ")");
		}
		if (strv[0] && strv[1])
			g_string_append(out, ")");

		query = out->str;
		g_string_free(out, FALSE);

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
	EContact *contact;
	const gchar *email;
	gint match_len = 0;
	EABContactMatchType match;
	EABContactMatchPart first_match;
	double score = 0;
	gboolean have_given, have_additional, have_family;
	EContactName *contact_name;

	contact = e_destination_get_contact (dest);

	contact_name = e_contact_get (contact, E_CONTACT_NAME);
	if (!contact_name)
		return NULL;

	email = e_destination_get_email (dest);

	match = eab_contact_compare_name_to_string_full (contact, comp->priv->query_text, TRUE /* yes, allow partial matches */,
							 NULL, &first_match, &match_len);

	if (match <= EAB_CONTACT_MATCH_NONE) {
		e_contact_name_free (contact_name);
		return NULL;
	}

	score = match_len * 3; /* three points per match character */

	have_given       = contact_name->given && *contact_name->given;
	have_additional  = contact_name->additional && *contact_name->additional;
	have_family      = contact_name->family && *contact_name->family;

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {

		menu_text = e_contact_name_to_string (contact_name);

	} else if (first_match == EAB_CONTACT_MATCH_PART_GIVEN_NAME) {

		if (have_family)
			menu_text = g_strdup_printf ("%s %s <%s>", contact_name->given, contact_name->family, email);
		else
			menu_text = g_strdup_printf ("%s <%s>", contact_name->given, email);

	} else if (first_match == EAB_CONTACT_MATCH_PART_ADDITIONAL_NAME) {

		if (have_given) {

			menu_text = g_strdup_printf ("%s%s%s, %s <%s>",
						     contact_name->additional,
						     have_family ? " " : "",
						     have_family ? contact_name->family : "",
						     contact_name->given,
						     email);
		} else {

			menu_text = g_strdup_printf ("%s%s%s <%s>",
						     contact_name->additional,
						     have_family ? " " : "",
						     have_family ? contact_name->family : "",
						     email);
		}

	} else if (first_match == EAB_CONTACT_MATCH_PART_FAMILY_NAME) { 

		if (have_given)
			menu_text = g_strdup_printf ("%s, %s%s%s <%s>",
						     contact_name->family,
						     contact_name->given,
						     have_additional ? " " : "",
						     have_additional ? contact_name->additional : "",
						     email);
		else
			menu_text = g_strdup_printf ("%s <%s>", contact_name->family, email);

	} else { /* something funny happened */

		menu_text = g_strdup_printf ("<%s> ???", email);

	}

	if (menu_text) {
		g_strstrip (menu_text);
		final_match = make_match (dest, menu_text, score);
		g_free (menu_text);
	}
	
	e_contact_name_free (contact_name);

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

typedef struct _BookQuery BookQuery;
struct _BookQuery {
	BookQuerySExp builder;
	BookQueryMatchTester tester;
};

static BookQuery book_queries[] = {
	{ sexp_nickname, match_nickname},
	{ sexp_email,    match_email },
	{ sexp_name,     match_name },
	{ sexp_file_as,  match_file_as },
};
static gint book_query_count = sizeof (book_queries) / sizeof (BookQuery);

/*
 * Build up a big compound sexp corresponding to all of our queries.
 */
static EBookQuery*
book_query_sexp (ESelectNamesCompletion *comp)
{
	gint i, j;
	gchar **queryv;
	EBookQuery *query;

	g_return_val_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp), NULL);

	if (! (comp->priv->query_text && *comp->priv->query_text))
		return NULL;

	queryv = g_new0 (gchar *, book_query_count+1);
	for (i=0, j=0; i<book_query_count; ++i) {
		queryv[j] = book_queries[i].builder (comp);
		if (queryv[j])
			++j;
	}

	if (j == 0) {
		query = NULL;
	} else if (j == 1) {
		query = e_book_query_from_string (queryv[0]);
		queryv[0] = NULL;
	} else {
		gchar *tmp, *tmp2;
		tmp = g_strjoinv (" ", queryv);
		tmp2 = g_strdup_printf ("(or %s)", tmp);
		query = e_book_query_from_string (tmp2);
		g_free (tmp);
		g_free (tmp2);
	}

	for (i=0; i<book_query_count; ++i)
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

		if (book_queries[i].tester && e_destination_get_contact (dest)) {
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

	return best_match;
}

static void
book_query_process_card_list (ESelectNamesCompletion *comp, const GList *contacts)
{
	while (contacts) {
		EContact *contact = E_CONTACT (contacts->data);

		if (e_contact_get (contact, E_CONTACT_IS_LIST)) {

			if (comp->priv->match_contact_lists) {

				EDestination *dest = e_destination_new ();
				ECompletionMatch *match;
				e_destination_set_contact (dest, contact, 0);
				match = book_query_score (comp, dest);
				if (match && match->score > 0) {
					e_completion_found_match (E_COMPLETION (comp), match);
				} else {
					e_completion_match_unref (match);
				}
				g_object_unref (dest);

			}

		}
		else {
			GList *email = e_contact_get (contact, E_CONTACT_EMAIL);
			if (email) {
				GList *iter;
				gint i;
				for (i=0, iter = email; iter; ++i, iter = iter->next) {
					EDestination *dest = e_destination_new ();
					gchar *e;
					ECompletionMatch *match;
				
					e_destination_set_contact (dest, contact, i);
					e = iter->data;

					if (e && *e) {
				
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
			g_list_foreach (email, (GFunc)g_free, NULL);
			g_list_free (email);
		}
		
		contacts = contacts->next;
	}
}

/*
 *
 * ESelectNamesCompletion code
 *
 */


GType
e_select_names_completion_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesCompletionClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_completion_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNamesCompletion),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_completion_init,
		};

		type = g_type_register_static (e_completion_get_type (), "ESelectNamesCompletion", &info, 0);
	}

	return type;
}

static void
e_select_names_completion_class_init (ESelectNamesCompletionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECompletionClass *completion_class = E_COMPLETION_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_completion_dispose;

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

		if (book_data->contacts_added_tag) {
			g_signal_handler_disconnect (book_data->book_view, book_data->contacts_added_tag);
			book_data->contacts_added_tag = 0;
		}

		if (book_data->seq_complete_tag) {
			g_signal_handler_disconnect (book_data->book_view, book_data->seq_complete_tag);
			book_data->seq_complete_tag = 0;
		}

		g_object_unref (book_data->book);

		if (book_data->book_view) {
			e_book_view_stop (book_data->book_view);
			g_object_unref (book_data->book_view);
		}

		g_free (book_data->cached_query_text);
		g_list_foreach (book_data->cached_cards, (GFunc)g_object_unref, NULL);
		g_list_free (book_data->cached_cards);

		g_free (book_data);
	}
	g_list_free (comp->priv->book_data);
	comp->priv->book_data = NULL;
}

static void
e_select_names_completion_dispose (GObject *object)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (object);

	if (comp->priv) {
		if (comp->priv->text_model)
			g_object_unref (comp->priv->text_model);

		e_select_names_completion_clear_book_data (comp);

		g_free (comp->priv->waiting_query);
		g_free (comp->priv->query_text);

		g_free (comp->priv);
		comp->priv = NULL;
	}

	if (parent_class->dispose)
		parent_class->dispose (object);
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
e_select_names_completion_clear_cache (ESelectNamesCompletionBookData *book_data)
{
	if (out)
		fprintf (out, "** clearing cache on book %s\n", e_book_get_uri (book_data->book));

	g_free (book_data->cached_query_text);
	g_list_foreach (book_data->cached_cards, (GFunc)g_object_unref, NULL);
	g_list_free (book_data->cached_cards);

	book_data->cached_query_text = NULL;
	book_data->cached_cards = NULL;
}

static void
e_select_names_completion_done (ESelectNamesCompletion *comp)
{
	g_free (comp->priv->query_text);
	comp->priv->query_text = NULL;

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
e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data)
{
	ESelectNamesCompletion *comp;
	ESelectNamesCompletionBookData *book_data;

	book_data = (ESelectNamesCompletionBookData*)user_data;
	comp = book_data->comp;

	if (status != E_BOOK_ERROR_OK) {
		comp->priv->pending_completion_seq--;
		if (!comp->priv->pending_completion_seq)
			e_select_names_completion_done (comp);
		return;
	}

	book_data->book_view_tag = 0;

	if (book_data->contacts_added_tag) {
		g_signal_handler_disconnect (book_data->book_view, book_data->contacts_added_tag);
		book_data->contacts_added_tag = 0;
	}
	if (book_data->seq_complete_tag) {
		g_signal_handler_disconnect (book_data->book_view, book_data->seq_complete_tag);
		book_data->seq_complete_tag = 0;
	}

	g_object_ref (view);
	if (book_data->book_view) {
		e_book_view_stop (book_data->book_view);
		g_object_unref (book_data->book_view);
	}
	book_data->book_view = view;

	book_data->contacts_added_tag = 
		g_signal_connect (view,
				  "contacts_added",
				  G_CALLBACK (e_select_names_completion_contacts_added_cb),
				  book_data);

	book_data->seq_complete_tag =
		g_signal_connect (view,
				  "sequence_complete",
				  G_CALLBACK (e_select_names_completion_seq_complete_cb),
				  book_data);

	book_data->sequence_complete_received = FALSE;

	e_book_view_start (view);
}

static void
e_select_names_completion_contacts_added_cb (EBookView *book_view, const GList *cards, gpointer user_data)
{
	ESelectNamesCompletionBookData *book_data = user_data;
	ESelectNamesCompletion *comp = book_data->comp;

	if (e_completion_searching (E_COMPLETION (comp))) {
		book_query_process_card_list (comp, cards);

		/* Save the list of matching cards. */
		while (cards) {
			book_data->cached_cards = g_list_prepend (book_data->cached_cards, g_object_ref (cards->data));
			cards = g_list_next (cards);
		}
	}
}

static void
e_select_names_completion_seq_complete_cb (EBookView *book_view, EBookViewStatus status, gpointer user_data)
{
	ESelectNamesCompletionBookData *book_data = user_data;
	ESelectNamesCompletion *comp = book_data->comp;

	if (out)
		fprintf (out, "** got sequence_complete (status = %d) on book %s\n", status, e_book_get_uri (book_data->book));

	/*
	 * We aren't searching, but the addressbook has changed -- clear our card cache so that
	 * future completion requests will take the changes into account.
	 */
	if (! e_completion_searching (E_COMPLETION (comp))) {
		if (out)
			fprintf (out, "\t we weren't searching, clearing the cache\n");
		e_select_names_completion_clear_cache (book_data);
		return;
	}

	if (book_data->cached_query_text
	    && status == E_BOOK_ERROR_OK
	    && !book_data->cache_complete
	    && !strcmp (book_data->cached_query_text, comp->priv->query_text))
		book_data->cache_complete = TRUE;

	if (out)
		fprintf (out, "\tending search, book_data->cache_complete == %d, cached_cards = %p\n",
			 book_data->cache_complete,
			 book_data->cached_cards);

	if (!book_data->sequence_complete_received) {
		book_data->sequence_complete_received = TRUE;

		if (book_data->contacts_added_tag) {
			g_signal_handler_disconnect (book_data->book_view, book_data->contacts_added_tag);
			book_data->contacts_added_tag = 0;
		}
		if (book_data->seq_complete_tag) {
			g_signal_handler_disconnect (book_data->book_view, book_data->seq_complete_tag);
			book_data->seq_complete_tag = 0;
		}

		if (out)
			fprintf (out, "\t %d remaining book view's\n", comp->priv->pending_completion_seq - 1);

		comp->priv->pending_completion_seq --;
		if (comp->priv->pending_completion_seq > 0)
			return;
	}

	e_select_names_completion_done (comp);
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
			e_book_cancel (book_data->book, NULL);
			book_data->book_view_tag = 0;
		}
		if (book_data->book_view) {
			if (book_data->contacts_added_tag) {
				g_signal_handler_disconnect (book_data->book_view, book_data->contacts_added_tag);
				book_data->contacts_added_tag = 0;
			}
			if (book_data->seq_complete_tag) {
				g_signal_handler_disconnect (book_data->book_view, book_data->seq_complete_tag);
				book_data->seq_complete_tag = 0;
			}

			e_book_view_stop (book_data->book_view);
			g_object_unref (book_data->book_view);
			book_data->book_view = NULL;
		}
	}

	comp->priv->pending_completion_seq = 0;
}

static void
e_select_names_completion_start_query (ESelectNamesCompletion *comp, const gchar *query_text)
{
	g_return_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp));
	g_return_if_fail (query_text);

	e_select_names_completion_stop_query (comp);  /* Stop any prior queries. */

	if (comp->priv->books_not_ready == 0) {
		EBookQuery *query;
	
		if (strlen (query_text) < comp->priv->minimum_query_length) {
			e_completion_end_search (E_COMPLETION (comp));
			return;
		}

		g_free (comp->priv->query_text);
		comp->priv->query_text = g_strdup (query_text);

		query = book_query_sexp (comp);
		if (query) {
			GList *l;

			if (out)
				fprintf (out, "\n\n**** starting query: \"%s\"\n", comp->priv->query_text);

			for (l = comp->priv->book_data; l; l = l->next) {
				ESelectNamesCompletionBookData *book_data = l->data;
				gboolean can_reuse_cached_cards;

				if (out) {
					fprintf (out,
						 "book == %s[\n"
						 "\tbook_data->cached_query_text == `%s'\n"
						 "\tbook_data->cache_complete == %d\n"
						 "\tbook_data->cached_cards == %p\n",
						 e_book_get_uri (book_data->book),
						 book_data->cached_query_text,
						 book_data->cache_complete,
						 book_data->cached_cards);
				}

				/* for lack of a better place, we invalidate the cache here if we
				   notice that the text is different. */
				if (book_data->cached_query_text
				    && (strlen (book_data->cached_query_text) > strlen (query_text)
					|| utf8_casefold_collate_len (book_data->cached_query_text, query_text,
								      strlen (book_data->cached_query_text))))
					book_data->cache_complete = FALSE;

				can_reuse_cached_cards = (book_data->cached_query_text
							  && book_data->cache_complete
							  && book_data->cached_cards != NULL);

				if (can_reuse_cached_cards) {

					if (out)
						fprintf (out, "\t*** can reuse cached cards (%d cards cached)!\n", g_list_length (book_data->cached_cards));

					if (out)
						fprintf (out, "\tusing existing query info: %s (vs %s)\n", query_text, book_data->cached_query_text);
					book_query_process_card_list (comp, book_data->cached_cards);
				}
				else {
					e_select_names_completion_clear_cache (book_data);
					book_data->cached_query_text = g_strdup (query_text);

					book_data->book_view_tag = e_book_async_get_book_view (book_data->book,
											       query, 
											       NULL, -1,
											       e_select_names_completion_got_book_view_cb, book_data);
					comp->priv->pending_completion_seq++;
				}

				if (out)
					fprintf (out, "]\n");
			}

			/* if we looped through all the books
			   and were able to complete based
			   solely on our cached cards, signal
			   that the search is over. */
			if (!comp->priv->pending_completion_seq)
				e_select_names_completion_done (E_SELECT_NAMES_COMPLETION (comp));

			e_book_query_unref (query);
		} else {
			g_free (comp->priv->query_text);
			comp->priv->query_text = NULL;
		}
	} else {
		g_free (comp->priv->waiting_query);
		comp->priv->waiting_query = g_strdup (query_text);
	}
}

static void
e_select_names_completion_do_query (ESelectNamesCompletion *comp, const gchar *query_text, gint pos, gint limit)
{
	gchar *clean;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	clean = clean_query_text (query_text);
	if (! (clean && *clean)) {
		g_free (clean);
		e_completion_end_search (E_COMPLETION (comp));
		return;
	}

	if (out)
		fprintf (out, "do_query: %s => %s\n", query_text, clean);

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

	comp = g_object_new (E_TYPE_SELECT_NAMES_COMPLETION, NULL);

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
	g_object_ref (book_data->book);
	comp->priv->book_data = g_list_append (comp->priv->book_data, book_data);

	/* XXX toshok - this doesn't work properly.  need to rethink this next bit. */
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

void
e_select_names_completion_set_minimum_query_length (ESelectNamesCompletion *comp, int query_length)
{
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));
	comp->priv->minimum_query_length = query_length;
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


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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gnome.h>
#include <addressbook/backend/ebook/e-destination.h>
#include "e-select-names-completion.h"

struct _ESelectNamesCompletionPrivate {

	ESelectNamesModel *model;

	EBook *book;
	gboolean book_ready;
	gboolean cancelled;

	EBookView *book_view;
	guint card_added_id;
	guint seq_complete_id;

	gchar *query_text;
	gchar *pending_query_text;

	gboolean primary_only;
};

static void e_select_names_completion_class_init (ESelectNamesCompletionClass *);
static void e_select_names_completion_init (ESelectNamesCompletion *);
static void e_select_names_completion_destroy (GtkObject *object);

static void e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data);
static void e_select_names_completion_card_added_cb    (EBookView *, const GList *cards, gpointer user_data);
static void e_select_names_completion_seq_complete_cb  (EBookView *, gpointer user_data);

static void e_select_names_completion_do_query (ESelectNamesCompletion *);

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
typedef gchar *(*BookQueryMatchTester) (ESelectNamesCompletion *, EDestination *, double *score);

/*
 * Nickname query
 */

static gchar *
sexp_nickname (ESelectNamesCompletion *comp)
{
	return g_strdup_printf ("(beginswith \"nickname\" \"%s\")", comp->priv->query_text);

}

static gchar *
match_nickname (ESelectNamesCompletion *comp, EDestination *dest, double *score)
{
	gint len = strlen (comp->priv->query_text);
	ECard *card = e_destination_get_card (dest);

	if (card->nickname
	    && !g_strncasecmp (comp->priv->query_text, card->nickname, len)) {
		
		*score = len * 10; /* nickname gives 10 points per matching character */
		
		return g_strdup_printf ("(%s) %s %s", card->nickname, card->name->given, card->name->family);
		
	}

	return NULL;
}

/*
 * E-Mail Query
 */

static gchar *
sexp_email (ESelectNamesCompletion *comp)
{
	return g_strdup_printf ("(beginswith \"email\" \"%s\")", comp->priv->query_text);
}

static gchar *
match_email (ESelectNamesCompletion *comp, EDestination *dest, double *score)
{
	gint len = strlen (comp->priv->query_text);
	ECard *card = e_destination_get_card (dest);
	const gchar *email = e_destination_get_email (dest);
	
	if (email && !g_strncasecmp (comp->priv->query_text, email, len)) {
		*score = len * 2; /* 2 points for each matching character */
		return g_strdup_printf ("<%s> %s %s", email, card->name->given, card->name->family);
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

static gchar *
match_name (ESelectNamesCompletion *comp, EDestination *dest, double *score)
{
	ECard *card;
	gchar *cpy, **strv;
	gint len, i, match_len = 0;
	gint match = 0, first_match = 0;

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
		    && !(match & MATCHED_GIVEN_NAME)
		    && !g_strncasecmp (card->name->given, strv[i], len)) {

			this_match = MATCHED_GIVEN_NAME;

		}
		else if (card->name->additional
			   && !(match & MATCHED_ADDITIONAL_NAME)
			   && !g_strncasecmp (card->name->additional, strv[i], len)) {

			this_match = MATCHED_ADDITIONAL_NAME;

		} else if (card->name->family
			   && !(match & MATCHED_FAMILY_NAME)
			   && !g_strncasecmp (card->name->family, strv[i], len)) {
			
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
	
	*score = match_len * 3; /* three points per match character */

	if (card->nickname) {
		/* We massively boost the score if the nickname exists and is the same as one of the "real" names.  This keeps the
		   nickname from matching ahead of the real name for this card. */
		len = strlen (card->nickname);
		if ((card->name->given && !g_strncasecmp (card->name->given, card->nickname, MIN (strlen (card->name->given), len)))
		    || (card->name->family && !g_strncasecmp (card->name->family, card->nickname, MIN (strlen (card->name->family), len)))
		    || (card->name->additional && !g_strncasecmp (card->name->additional, card->nickname, MIN (strlen (card->name->additional), len))))
			*score *= 100;
	}

	if (first_match == MATCHED_GIVEN_NAME)
		return g_strdup_printf ("%s %s", card->name->given, card->name->family);
	else if (first_match == MATCHED_ADDITIONAL_NAME)
		return g_strdup_printf ("%s, %s %s", card->name->family, card->name->given, card->name->additional);
	else if (first_match == MATCHED_FAMILY_NAME)
		return g_strdup_printf ("%s, %s", card->name->family, card->name->given);
	
	return NULL;
}

/*
 * Initials Query
 */

static gchar *
sexp_initials (ESelectNamesCompletion *comp)
{
	return NULL;
}

static gchar *
match_initials (ESelectNamesCompletion *comp, EDestination *dest, double *score)
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
static gchar *
book_query_score (ESelectNamesCompletion *comp, EDestination *dest, double *score)
{
	double best_score = -1;
	gchar *best_string = NULL;
	gint i;

	g_return_val_if_fail (score, NULL);
	*score = -1;

	g_return_val_if_fail (comp && E_IS_SELECT_NAMES_COMPLETION (comp), NULL);
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	for (i=0; i<book_query_count; ++i) {
		double this_score = -1;
		gchar *this_string = NULL;

		if (book_queries[i].primary || !comp->priv->primary_only) {
			if (book_queries[i].tester && e_destination_get_card (dest))
				this_string = book_queries[i].tester (comp, dest, &this_score);
			if (this_string) {
				if (this_score > best_score) {
					g_free (best_string);
					best_string = this_string;
					best_score = this_score;
				} else {
					g_free (this_string);
				}
			}
		}
	}

	/* If this destination corresponds to a card w/ multiple addresses, and if the
	   address isn't already in the string, append it. */
	if (best_string) {
		ECard *card = e_destination_get_card (dest);
		if (e_list_length (card->email) > 1) {
			const gchar *email = e_destination_get_email (dest);
			if (email && strstr (best_string, email) == NULL) {
				gchar *tmp = g_strdup_printf ("%s <%s>", best_string, email);
				g_free (best_string);
				best_string = tmp;
			}
		}
	}

	*score = best_score;
	return best_string;
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

	if (comp->priv->book_view) {
		gtk_signal_disconnect (GTK_OBJECT (comp->priv->book_view), comp->priv->card_added_id);
		gtk_signal_disconnect (GTK_OBJECT (comp->priv->book_view), comp->priv->seq_complete_id);
		gtk_object_unref (GTK_OBJECT (comp->priv->book_view));
	}

	g_free (comp->priv->query_text);
	g_free (comp->priv->pending_query_text);

	g_free (comp->priv);

	if (parent_class->destroy)
		parent_class->destroy (object);
}

/*
 *
 *  EBook/EBookView Callbacks
 *
 */

static void
e_select_names_completion_got_book_view_cb (EBook *book, EBookStatus status, EBookView *view, gpointer user_data)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (user_data);

	comp->priv->book_view = view;
	gtk_object_ref (GTK_OBJECT (view));


	comp->priv->card_added_id   = gtk_signal_connect (GTK_OBJECT (view),
							  "card_added",
							  GTK_SIGNAL_FUNC (e_select_names_completion_card_added_cb),
							  comp);
	comp->priv->seq_complete_id = gtk_signal_connect (GTK_OBJECT (view),
							  "sequence_complete",
							  GTK_SIGNAL_FUNC (e_select_names_completion_seq_complete_cb),
							  comp);
}

static void
e_select_names_completion_card_added_cb (EBookView *book_view, const GList *cards, gpointer user_data)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (user_data);
	
	if (comp->priv->cancelled)
		return;

	while (cards) {
		ECard *card = E_CARD (cards->data);

		if (card->email) {
			gint i;
			for (i=0; i<e_list_length (card->email); ++i) {
				EDestination *dest = e_destination_new ();
				gchar *match_text;
				double score = -1;
				
				e_destination_set_card (dest, card, i);
				
				match_text = book_query_score (comp, dest, &score);
				if (match_text && score > 0) {
					
					e_completion_found_match_full (E_COMPLETION (comp), match_text, score, dest,
								       (GtkDestroyNotify) gtk_object_unref);
				} else {
					gtk_object_unref (GTK_OBJECT (dest));
				}
				g_free (match_text);
			}
		}
			
		cards = g_list_next (cards);
	}
}

static void
e_select_names_completion_seq_complete_cb (EBookView *book_view, gpointer user_data)
{
	ESelectNamesCompletion *comp = E_SELECT_NAMES_COMPLETION (user_data);

	gtk_signal_disconnect (GTK_OBJECT (comp->priv->book_view), comp->priv->card_added_id);
	gtk_signal_disconnect (GTK_OBJECT (comp->priv->book_view), comp->priv->seq_complete_id);
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
	e_select_names_completion_do_query (comp);
}

/*
 *
 *  Completion Callbacks
 *
 */

static void
e_select_names_completion_do_query (ESelectNamesCompletion *comp)
{
	gchar *sexp;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_SELECT_NAMES_COMPLETION (comp));

	/* Wait until we are ready... */
	if (! comp->priv->book_ready) 
		return;

	if (comp->priv->query_text) 
		return;

	if (comp->priv->pending_query_text == NULL) 
		return;


	comp->priv->query_text = comp->priv->pending_query_text;
	comp->priv->pending_query_text = NULL;

	sexp = book_query_sexp (comp);

	if (sexp == NULL || *sexp == '\0') {
		g_free (sexp);
		g_free (comp->priv->query_text);
		comp->priv->query_text = NULL;
		return;
	}

	comp->priv->cancelled = FALSE;

	if (out)
		fprintf (out, "\n\n**** starting query: \"%s\"\n", comp->priv->query_text);

	if (! e_book_get_book_view (comp->priv->book, sexp, e_select_names_completion_got_book_view_cb, comp)) {
		g_warning ( "exception getting book view");
		return;
	}
	g_free (sexp);
	
}

typedef struct _SearchOverride SearchOverride;
struct _SearchOverride {
	const gchar *trigger;
	const gchar *text[4];
};
static SearchOverride override[] = { 
	{ "easter egg", { "This is the sample", "Easter Egg text for", "Evolution.", NULL } },
	{ NULL, { NULL } } };

static gboolean
search_override_check (SearchOverride *over, const gchar *text)
{
	if (over == NULL || text == NULL)
		return FALSE;

	return !g_strcasecmp (over->trigger, text);
}

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

			for (k=0; override[j].text[k]; ++k)
				e_completion_found_match (comp, override[j].text[k]);
			
			if (out)
				fprintf (out, "aborting on override \"%s\"\n", override[j].trigger);
			e_completion_end_search (comp);
			return;
		}
	}
		    
	
	g_free (selcomp->priv->pending_query_text);
	selcomp->priv->pending_query_text = g_strdup (str);

	e_select_names_completion_do_query (selcomp);
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

	/* If query_text is non-NULL, someone tried to start a query before the book was ready.
	   Now that it is, get started. */
	if (comp->priv->query_text != NULL)
		e_select_names_completion_do_query (comp);

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
		gchar *filename, *uri;

		comp->priv->book = e_book_new ();
		gtk_object_ref (GTK_OBJECT (comp->priv->book));
		gtk_object_sink (GTK_OBJECT (comp->priv->book));

		filename = gnome_util_prepend_user_home ("evolution/local/Contacts/addressbook.db");
		uri = g_strdup_printf ("file://%s", filename);
		
		comp->priv->book_ready = FALSE;
		gtk_object_ref (GTK_OBJECT (comp)); /* ref ourself before our async call */
		e_book_load_uri (comp->priv->book, uri, (EBookCallback) e_select_names_completion_book_ready, comp);
		
		g_free (filename);
		g_free (uri);
	} else {
		
		comp->priv->book = book;
		gtk_object_ref (GTK_OBJECT (comp->priv->book));
		comp->priv->book_ready = TRUE;
	}
		
	comp->priv->model = model;
	gtk_object_ref (GTK_OBJECT (model));

	return E_COMPLETION (comp);
}


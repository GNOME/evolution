/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-searching-tokenizer.c
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
#include <string.h>
#include <ctype.h>
#include <gal/unicode/gunicode.h>
#include "e-searching-tokenizer.h"

enum {
	EST_MATCH_SIGNAL,
	EST_LAST_SIGNAL
};
guint e_searching_tokenizer_signals[EST_LAST_SIGNAL] = { 0 };

#define START_MAGIC "<\n>S<\n>"
#define END_MAGIC   "<\n>E<\n>"

static void     e_searching_tokenizer_begin      (HTMLTokenizer *, gchar *);
static void     e_searching_tokenizer_end        (HTMLTokenizer *);
static gchar   *e_searching_tokenizer_peek_token (HTMLTokenizer *);
static gchar   *e_searching_tokenizer_next_token (HTMLTokenizer *);
static gboolean e_searching_tokenizer_has_more   (HTMLTokenizer *);

static HTMLTokenizer *e_searching_tokenizer_clone (HTMLTokenizer *);

static const gchar *ignored_tags[] = { "b", "i", NULL };
static const gchar *space_tags[] = { "br", NULL };

GtkObjectClass *parent_class = NULL;

static FILE *out = NULL;

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

typedef enum {
	MATCH_FAILED = 0,
	MATCH_COMPLETE,
	MATCH_START,
	MATCH_CONTINUES,
	MATCH_END
} MatchInfo;

typedef struct _SearchInfo SearchInfo;
struct _SearchInfo {
	gchar *search;
	gchar *current;
	gboolean case_sensitive;
};

struct _ESearchingTokenizerPrivate {
	gint match_count;
	SearchInfo *search;
	GList *pending;
	GList *trash;
	gchar **match_begin;
	gchar **match_end;

	gchar *pending_search_string;
	gboolean pending_search_string_clear;
	gboolean pending_case_sensitivity;
};

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static SearchInfo *
search_info_new (void)
{
	SearchInfo *si;

	si = g_new0 (SearchInfo, 1);
	si->case_sensitive = FALSE;

	return si;
}

static void
search_info_free (SearchInfo *si)
{
	if (si) {
		g_free (si->search);
		g_free (si);
	}
}

static SearchInfo *
search_info_clone (SearchInfo *si)
{
	SearchInfo *new_si = NULL;

	if (si) {
		new_si                 = search_info_new ();
		new_si->search         = g_strdup (si->search);
		new_si->case_sensitive = si->case_sensitive;
	}

	return new_si;
}

static void
search_info_set_string (SearchInfo *si, const gchar *str)
{
	g_return_if_fail (si);
	g_return_if_fail (str);

	g_free (si->search);
	si->search = g_strdup (str);
	si->current = NULL;
}

static void
search_info_reset (SearchInfo *si)
{
	g_return_if_fail (si);
	si->current = NULL;
}

static const gchar *
find_whole (SearchInfo *si, const gchar *haystack, const gchar *needle)
{
	const gchar *h, *n;

	g_return_val_if_fail (si, NULL);
	g_return_val_if_fail (haystack && needle, NULL);
	g_return_val_if_fail (g_utf8_validate (haystack, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (needle, -1, NULL), NULL);

	while (*haystack) {
		h = haystack;
		n = needle;
		while (*h && *n) {
			gunichar c1 = g_utf8_get_char (h);
			gunichar c2 = g_utf8_get_char (n);

			if (!si->case_sensitive) {
				c1 = g_unichar_tolower (c1);
				c2 = g_unichar_tolower (c2);
			}

			if (c1 != c2)
				break;
			
			h = g_utf8_next_char (h);
			n = g_utf8_next_char (n);
		}
		if (*n == '\0')
			return haystack;
		if (*h == '\0')
			return NULL;
		haystack = g_utf8_next_char (haystack);
	}

	return NULL;
}

/* This is a really stupid implementation of this function. */
static const gchar *
find_head (SearchInfo *si, const gchar *haystack, const gchar *needle)
{
	const gchar *h, *n;

	g_return_val_if_fail (si, NULL);
	g_return_val_if_fail (haystack && needle, NULL);
	g_return_val_if_fail (g_utf8_validate (haystack, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (needle, -1, NULL), NULL);

	while (*haystack) {
		h = haystack;
		n = needle;
		while (*h && *n) {
			gunichar c1 = g_utf8_get_char (h);
			gunichar c2 = g_utf8_get_char (n);

			if (!si->case_sensitive) {
				c1 = g_unichar_tolower (c1);
				c2 = g_unichar_tolower (c2);
			}

			if (c1 != c2)
				break;

			h = g_utf8_next_char (h);
			n = g_utf8_next_char (n);
		}
		if (*h == '\0')
			return haystack;
		haystack = g_utf8_next_char (haystack);
	}

	return NULL;
}

static const gchar *
find_partial (SearchInfo *si, const gchar *haystack, const gchar *needle)
{
	g_return_val_if_fail (si, NULL);
	g_return_val_if_fail (haystack && needle, NULL);
	g_return_val_if_fail (g_utf8_validate (haystack, -1, NULL), NULL);
	g_return_val_if_fail (g_utf8_validate (needle, -1, NULL), NULL);
	
	while (*needle) {
		gunichar c1 = g_utf8_get_char (haystack);
		gunichar c2 = g_utf8_get_char (needle);

		if (!si->case_sensitive) {
			c1 = g_unichar_tolower (c1);
			c2 = g_unichar_tolower (c2);
		}

		if (c1 != c2)
			return NULL;

		needle = g_utf8_next_char (needle);
		haystack = g_utf8_next_char (haystack);
	}
	return haystack;
}

static gboolean
tag_match (const gchar *token, const gchar *tag)
{
	token += 2; /* Skip past TAG_ESCAPE and < */
	if (*token == '/')
		++token;
	while (*token && *tag) {
		gunichar c1 = g_unichar_tolower (g_utf8_get_char (token));
		gunichar c2 = g_unichar_tolower (g_utf8_get_char (tag));
		if (c1 != c2)
			return FALSE;
		token = g_utf8_next_char (token);
		tag = g_utf8_next_char (tag);
	}
	return (*tag == '\0' && *token == '>');
}

static MatchInfo
search_info_compare (SearchInfo *si, const gchar *token, gint *start_pos, gint *end_pos)
{
	gboolean token_is_tag;
	const gchar *s;
	gint i;
	
	g_return_val_if_fail (si != NULL, MATCH_FAILED);
	g_return_val_if_fail (token != NULL, MATCH_FAILED);
	g_return_val_if_fail (start_pos != NULL, MATCH_FAILED);
	g_return_val_if_fail (end_pos != NULL, MATCH_FAILED);

	token_is_tag = (*token == TAG_ESCAPE);

	/* Try to start a new match. */
	if (si->current == NULL) {

		/* A match can never start on a token. */
		if (token_is_tag)
			return MATCH_FAILED;
		
		/* Check to see if the search string is entirely embedded within the token. */
		s = find_whole (si, token, si->search);
		if (s) {
			*start_pos = s - token;
			*end_pos = *start_pos + g_utf8_strlen (si->search, -1);

			return MATCH_COMPLETE;
		}

		/* Check to see if the beginning of the search string lies in this token. */
		s = find_head (si, token, si->search);
		if (s) {
			*start_pos = s - token;
			si->current = si->search;
			while (*s) {
				s = g_utf8_next_char (s);
				si->current = g_utf8_next_char (si->current);
			}

			return MATCH_START;
		}
		
		return MATCH_FAILED;
	}

	/* Try to continue a previously-started match. */
	
	/* Deal with tags that we encounter mid-match. */
	if (token_is_tag) {

		/* "Ignored tags" will never mess up a match. */
		for (i=0; ignored_tags[i]; ++i) {
			if (tag_match (token, ignored_tags[i]))
				return MATCH_CONTINUES;
		}
		
		/* "Space tags" only match whitespace in our ongoing match. */
		if (g_unichar_isspace (g_utf8_get_char (si->current))) {
			for (i=0; space_tags[i]; ++i) {
				if (tag_match (token, space_tags[i])) {
					si->current = g_utf8_next_char (si->current);
					return MATCH_CONTINUES;
				}
			}
		}

		/* All other tags derail our match. */
		return MATCH_FAILED;
	}



	s = find_partial (si, token, si->current);
	if (s) {
		if (start_pos)
			*start_pos = 0;
		if (end_pos)
			*end_pos = s - token;
		return MATCH_END;
	}

	s = find_partial (si, si->current, token);
	if (s) {
		si->current = (gchar *) s;
		return MATCH_CONTINUES;
	}
	
	return MATCH_FAILED;
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static void
e_searching_tokenizer_cleanup (ESearchingTokenizer *st)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));

	if (st->priv->trash) {
		g_list_foreach (st->priv->trash, (GFunc) g_free, NULL);
		g_list_free (st->priv->trash);
		st->priv->trash = NULL;
	}

	if (st->priv->pending) {
		//g_list_foreach (st->priv->pending, (GFunc) g_free, NULL);
		//g_list_free (st->priv->pending);
		st->priv->pending = NULL;
	}
}

static void
e_searching_tokenizer_destroy (GtkObject *obj)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (obj);

	e_searching_tokenizer_cleanup (st);

	search_info_free (st->priv->search);

	g_strfreev (st->priv->match_begin);
	g_strfreev (st->priv->match_end);

	g_free (st->priv);
	st->priv = NULL;

	if (parent_class->destroy)
		parent_class->destroy (obj);
}

static void
e_searching_tokenizer_class_init (ESearchingTokenizerClass *klass)
{
	GtkObjectClass *obj_class = (GtkObjectClass *) klass;
	HTMLTokenizerClass *tok_class = HTML_TOKENIZER_CLASS (klass);

	e_searching_tokenizer_signals[EST_MATCH_SIGNAL] =
		gtk_signal_new ("match",
				GTK_RUN_LAST,
				obj_class->type,
				GTK_SIGNAL_OFFSET (ESearchingTokenizerClass, match),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,
				0);
	gtk_object_class_add_signals (obj_class, e_searching_tokenizer_signals, EST_LAST_SIGNAL);

	obj_class->destroy = e_searching_tokenizer_destroy;

	tok_class->begin      = e_searching_tokenizer_begin;
	tok_class->end        = e_searching_tokenizer_end;

	tok_class->peek_token = e_searching_tokenizer_peek_token;
	tok_class->next_token = e_searching_tokenizer_next_token;
	tok_class->has_more   = e_searching_tokenizer_has_more;
	tok_class->clone      = e_searching_tokenizer_clone;
	
	parent_class = gtk_type_class (HTML_TYPE_TOKENIZER);
}

static void
e_searching_tokenizer_init (ESearchingTokenizer *st)
{
	st->priv = g_new0 (struct _ESearchingTokenizerPrivate, 1);

	/* For now, hard coded match markup. */
	st->priv->match_begin = g_new0 (gchar *, 2);
	st->priv->match_begin[0] = g_strdup_printf ("%c<font color=red size=+1>", TAG_ESCAPE);

	st->priv->match_end = g_new0 (gchar *, 2);
	st->priv->match_end[0] = g_strdup_printf ("%c</font>", TAG_ESCAPE);
}

GtkType
e_searching_tokenizer_get_type (void)
{
	static GtkType e_searching_tokenizer_type = 0;
	if (! e_searching_tokenizer_type) {
		static GtkTypeInfo e_searching_tokenizer_info = {
			"ESearchingTokenizer",
			sizeof (ESearchingTokenizer),
			sizeof (ESearchingTokenizerClass),
			(GtkClassInitFunc) e_searching_tokenizer_class_init,
			(GtkObjectInitFunc) e_searching_tokenizer_init,
			NULL, NULL,
			(GtkClassInitFunc) NULL
		};
		e_searching_tokenizer_type = gtk_type_unique (HTML_TYPE_TOKENIZER,
							     &e_searching_tokenizer_info);
	}
	return e_searching_tokenizer_type;
}

HTMLTokenizer *
e_searching_tokenizer_new (void)
{
	if (out == NULL) {
		out = fopen ("/tmp/tokbarn", "w");
		setvbuf (out, NULL, _IONBF, 0);
		fprintf (out, "New!\n");
	};
	
	return (HTMLTokenizer *) gtk_type_new (E_TYPE_SEARCHING_TOKENIZER);
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static GList *
g_list_remove_head (GList *x)
{
	GList *repl = NULL;
	if (x) {
		repl = g_list_remove_link (x, x);
		g_list_free_1 (x);
	}
	return repl;
}

/* I can't believe that there isn't a better way to do this. */
static GList *
g_list_insert_before (GList *list, GList *llink, gpointer data)
{
	gint pos = g_list_position (list, llink);
	return g_list_insert (list, data, pos);
}

static gchar *
pop_pending (ESearchingTokenizer *st)
{
	gchar *token = NULL;
	if (st->priv->pending) {
		token = (gchar *) st->priv->pending->data;
		st->priv->trash = g_list_prepend (st->priv->trash, token);
		st->priv->pending = g_list_remove_head (st->priv->pending);
	}
	return token;
}

static inline void
add_pending (ESearchingTokenizer *st, gchar *tok)
{
	st->priv->pending = g_list_append (st->priv->pending, tok);
}

static void
add_pending_match_begin (ESearchingTokenizer *st)
{
	gint i;
	for (i=0; st->priv->match_begin[i]; ++i)
		add_pending (st, g_strdup (st->priv->match_begin[i]));
}

static void
add_pending_match_end (ESearchingTokenizer *st)
{
	gint i;
	for (i=0; st->priv->match_end[i]; ++i)
		add_pending (st, g_strdup (st->priv->match_end[i]));
}

static void
add_to_trash (ESearchingTokenizer *st, gchar *txt)
{
	st->priv->trash = g_list_prepend (st->priv->trash, txt);
}

static gchar *
get_next_token (ESearchingTokenizer *st)
{
	HTMLTokenizer *ht = HTML_TOKENIZER (st);
	HTMLTokenizerClass *klass = HTML_TOKENIZER_CLASS (parent_class);
	
	return klass->has_more (ht) ? klass->next_token (ht) : NULL;
}

/*
 * Move the matched part of the queue into pending, replacing the start and end placeholders by
 * the appropriate tokens.
 */
static GList *
queue_matched (ESearchingTokenizer *st, GList *q)
{
	GList *qh = q;
	gboolean post_start = FALSE;

	while (q != NULL) {
		GList *q_next = g_list_next (q);
		if (!strcmp ((gchar *) q->data, START_MAGIC)) {
			add_pending_match_begin (st);
			post_start = TRUE;
		} else if (!strcmp ((gchar *) q->data, END_MAGIC)) {
			add_pending_match_end (st);
			q_next = NULL;
		} else {
			gboolean is_tag = *((gchar *)q->data) == TAG_ESCAPE;
			if (is_tag && post_start)
				add_pending_match_end (st);
			add_pending (st, g_strdup ((gchar *) q->data));
			if (is_tag && post_start)
				add_pending_match_begin (st);
		}
		qh = g_list_remove_link (qh, q);
		g_list_free_1 (q);
		q = q_next;
	}

	return qh;
}

/*
 * Strip the start and end placeholders out of the queue.
 */
static GList *
queue_match_failed (ESearchingTokenizer *st, GList *q)
{
	GList *qh = q;

	/* If we do find the START_MAGIC token in the queue, we want
	   to drop everything up to and including the token immediately
	   following START_MAGIC. */
	while (q != NULL && strcmp ((gchar *) q->data, START_MAGIC))
		q = g_list_next (q);
	if (q) {
		q = g_list_next (q);
		/* If there is no token following START_MAGIC, something is
		   very wrong. */
		if (q == NULL) {
			g_assert_not_reached ();
		}
	}

	/* Otherwise we just want to just drop the the first token. */
	if (q == NULL)
		q = qh;

	/* Now move everything up to and including q to pending. */
	while (qh && qh != q) {
		if (strcmp ((gchar *) qh->data, START_MAGIC))
			add_pending (st, g_strdup (qh->data));
		qh = g_list_remove_head (qh);
	}
	if (qh == q) {
		if (strcmp ((gchar *) qh->data, START_MAGIC))
			add_pending (st, g_strdup (qh->data));
		qh = g_list_remove_head (qh);
	}

	return qh;
}

static void
matched (ESearchingTokenizer *st)
{
	++st->priv->match_count;
	gtk_signal_emit (GTK_OBJECT (st), e_searching_tokenizer_signals[EST_MATCH_SIGNAL]);
}

static void
get_pending_tokens (ESearchingTokenizer *st)
{
	GList *queue = NULL;
	gchar *token = NULL;
	MatchInfo result;
	gint start_pos, end_pos;
	GList *start_after = NULL;

	/* Get an initial token into the queue. */
	token = get_next_token (st);
	if (token) {
		queue = g_list_append (queue, token);
	}

	while (queue) {
		GList *q;
		gboolean finished = FALSE;
		search_info_reset (st->priv->search);

		if (start_after) {
			q = g_list_next (start_after);
			start_after = NULL;
		} else {
			q = queue;
		}
		
		while (q) {
			GList *q_next = g_list_next (q);
			token = (gchar *) q->data;
			
			result = search_info_compare (st->priv->search, token, &start_pos, &end_pos);

			switch (result) {

			case MATCH_FAILED:

				queue = queue_match_failed (st, queue);

				finished = TRUE;
				break;

			case MATCH_COMPLETE:

				if (start_pos != 0)
					add_pending (st, g_strndup (token, start_pos));
				add_pending_match_begin (st);
				add_pending (st, g_strndup (token+start_pos, end_pos-start_pos));
				add_pending_match_end (st);
				if (*(token+end_pos)) {
					queue->data = g_strdup (token+end_pos);
					add_to_trash (st, (gchar *) queue->data);
				} else {
					queue = g_list_remove_head (queue);
				}

				matched (st);

				finished = TRUE;
				break;

			case MATCH_START: {
				
				gchar *s1 = g_strndup (token, start_pos);
				gchar *s2 = g_strdup (START_MAGIC);
				gchar *s3 = g_strdup (token+start_pos);
				
				queue = g_list_insert_before (queue, q, s1);
				queue = g_list_insert_before (queue, q, s2);
				queue = g_list_insert_before (queue, q, s3);

				add_to_trash (st, s1);
				add_to_trash (st, s2);
				add_to_trash (st, s3);

				queue = g_list_remove_link (queue, q);

				finished = FALSE;
				break;
			}

			case MATCH_CONTINUES:
				/* Do nothing... */
				finished = FALSE;
				break;
				
			case MATCH_END: {
				gchar *s1 = g_strndup (token, end_pos);
				gchar *s2 = g_strdup (END_MAGIC);
				gchar *s3 = g_strdup (token+end_pos);

				queue = g_list_insert_before (queue, q, s1);
				queue = g_list_insert_before (queue, q, s2);
				queue = g_list_insert_before (queue, q, s3);

				add_to_trash (st, s1);
				add_to_trash (st, s2);
				add_to_trash (st, s3);

				queue = g_list_remove_link (queue, q);
				queue = queue_matched (st, queue);

				matched (st);

				finished = TRUE;
				break;
			}
				
			default:
				g_assert_not_reached ();
			}

			/* If we reach the end of the queue but we aren't finished, try to pull in another
			   token and stick it onto the end. */
			if (q_next == NULL && !finished) {
				gchar *next_token = get_next_token (st);
				if (next_token) {
					queue = g_list_append (queue, next_token);
					q_next = g_list_last (queue);
				}
			}
			q = finished ? NULL : q_next;
		
		} /* while (q) */

		if (!finished && queue) { /* ...we add the token at the head of the queue to pending and try again. */
			add_pending (st, g_strdup ((gchar *) queue->data));
			queue = g_list_remove_head (queue);
		}
		
	} /* while (queue) */
}

/** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/

static void
e_searching_tokenizer_begin (HTMLTokenizer *t, gchar *content_type)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (t);

	if (st->priv->pending_search_string) {

		if (st->priv->search == NULL)
			st->priv->search = search_info_new ();

		search_info_set_string (st->priv->search, st->priv->pending_search_string);
		g_free (st->priv->pending_search_string);
		st->priv->pending_search_string = NULL;

	} else if (st->priv->pending_search_string_clear) {

		search_info_free (st->priv->search);
		st->priv->search = NULL;
		st->priv->pending_search_string_clear = FALSE;
	}

	if (st->priv->search) {
		st->priv->search->case_sensitive = st->priv->pending_case_sensitivity;
	}
	
	e_searching_tokenizer_cleanup (st);
	search_info_reset (st->priv->search);
	st->priv->match_count = 0;

	HTML_TOKENIZER_CLASS (parent_class)->begin (t, content_type);
}

static void
e_searching_tokenizer_end (HTMLTokenizer *t)
{
	e_searching_tokenizer_cleanup (E_SEARCHING_TOKENIZER (t));

	HTML_TOKENIZER_CLASS (parent_class)->end (t);
}

static gchar *
e_searching_tokenizer_peek_token (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);

	/* If no search is active, just use the default method. */
	if (st->priv->search == NULL)
		return HTML_TOKENIZER_CLASS (parent_class)->peek_token (tok);

	if (st->priv->pending == NULL)
		get_pending_tokens (st);
	return st->priv->pending ? (gchar *) st->priv->pending->data : NULL;
}

static gchar *
e_searching_tokenizer_next_token (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);

	/* If no search is active, just use the default method. */
	if (st->priv->search == NULL)
		return HTML_TOKENIZER_CLASS (parent_class)->next_token (tok);

	if (st->priv->pending == NULL)
		get_pending_tokens (st);
	return pop_pending (st);
}

static gboolean
e_searching_tokenizer_has_more (HTMLTokenizer *tok)
{
	ESearchingTokenizer *st = E_SEARCHING_TOKENIZER (tok);

	/* If no search is active, pending will always be NULL and thus
	   we'll always fall back to using the default method. */

	return st->priv->pending || HTML_TOKENIZER_CLASS (parent_class)->has_more (tok);
}

static HTMLTokenizer *
e_searching_tokenizer_clone (HTMLTokenizer *tok)
{
	ESearchingTokenizer *orig_st = E_SEARCHING_TOKENIZER (tok);
	ESearchingTokenizer *new_st = E_SEARCHING_TOKENIZER (e_searching_tokenizer_new ());

	if (new_st->priv->search) {
		search_info_free (new_st->priv->search);
	}

	new_st->priv->search = search_info_clone (orig_st->priv->search);

	gtk_signal_connect_object (GTK_OBJECT (new_st),
				   "match",
				   GTK_SIGNAL_FUNC (matched),
				   GTK_OBJECT (orig_st));

	return HTML_TOKENIZER (new_st);
}
/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

static gboolean
only_whitespace (const gchar *p)
{
	gunichar c;
	g_return_val_if_fail (p, FALSE);

	while (*p && g_unichar_validate (c = g_utf8_get_char (p))) {
		if (!g_unichar_isspace (c))
			return FALSE;
		p = g_utf8_next_char (p);
	}
	return TRUE;
}

void
e_searching_tokenizer_set_search_string (ESearchingTokenizer *st, const gchar *search_str)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));

	if (search_str == NULL
	    || !g_utf8_validate (search_str, -1, NULL)
	    || only_whitespace (search_str)) {

		st->priv->pending_search_string_clear = TRUE;

	} else {

		st->priv->pending_search_string_clear = FALSE;
		st->priv->pending_search_string = g_strdup (search_str);

	}
}

void
e_searching_tokenizer_set_case_sensitivity (ESearchingTokenizer *st, gboolean is_case_sensitive)
{
	g_return_if_fail (st && E_IS_SEARCHING_TOKENIZER (st));

	st->priv->pending_case_sensitivity = is_case_sensitive;
}

gint
e_searching_tokenizer_match_count (ESearchingTokenizer *st)
{
	g_return_val_if_fail (st && E_IS_SEARCHING_TOKENIZER (st), -1);

	return st->priv->match_count;
}




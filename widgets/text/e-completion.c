/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion.c - A base class for text completion.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 * Adapted by Jon Trowbridge <trow@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gal/util/e-util.h>
#include "e-completion.h"
#include "gal/util/e-util.h"

enum {
	E_COMPLETION_REQUEST_COMPLETION,
	E_COMPLETION_BEGIN_COMPLETION,
	E_COMPLETION_COMPLETION,
	E_COMPLETION_RESTART_COMPLETION,
	E_COMPLETION_CANCEL_COMPLETION,
	E_COMPLETION_END_COMPLETION,
	E_COMPLETION_CLEAR_COMPLETION,
	E_COMPLETION_LOST_COMPLETION,
	E_COMPLETION_LAST_SIGNAL
};

static guint e_completion_signals[E_COMPLETION_LAST_SIGNAL] = { 0 };

struct _ECompletionPrivate {
	gboolean searching;
	gboolean done_search;
	gboolean refining;
	gchar *search_text;
	GPtrArray *matches;
	gint match_count;
	gint pos;
	gint limit;
	double min_score, max_score;
	gint refinement_count;
	GList *search_stack;
};

typedef struct {
	gchar *text;
	gint pos;
} ECompletionSearch;

static void e_completion_class_init (ECompletionClass *klass);
static void e_completion_init       (ECompletion *complete);
static void e_completion_destroy    (GtkObject *object);

static void     e_completion_add_match          (ECompletion *complete, ECompletionMatch *);
static void     e_completion_clear_search_stack (ECompletion *complete);
static void     e_completion_clear_matches      (ECompletion *complete);
static gboolean e_completion_sort               (ECompletion *complete);
static void     e_completion_restart            (ECompletion *complete);

static GtkObjectClass *parent_class;



GtkType
e_completion_get_type (void)
{
	static GtkType complete_type = 0;
  
	if (!complete_type) {
		GtkTypeInfo complete_info = {
			"ECompletion",
			sizeof (ECompletion),
			sizeof (ECompletionClass),
			(GtkClassInitFunc) e_completion_class_init,
			(GtkObjectInitFunc) e_completion_init,
			NULL, NULL, /* reserved */
			(GtkClassInitFunc) NULL
		};

		complete_type = gtk_type_unique (gtk_object_get_type (), &complete_info);
	}

	return complete_type;
}

static void
e_completion_class_init (ECompletionClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (gtk_object_get_type ()));

	e_completion_signals[E_COMPLETION_REQUEST_COMPLETION] =
		gtk_signal_new ("request_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, request_completion),
				e_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);

	e_completion_signals[E_COMPLETION_BEGIN_COMPLETION] =
		gtk_signal_new ("begin_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, begin_completion),
				e_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);

	e_completion_signals[E_COMPLETION_COMPLETION] =
		gtk_signal_new ("completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, completion),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_completion_signals[E_COMPLETION_RESTART_COMPLETION] =
		gtk_signal_new ("restart_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, restart_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_CANCEL_COMPLETION] =
		gtk_signal_new ("cancel_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, cancel_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_END_COMPLETION] =
		gtk_signal_new ("end_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, end_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_CLEAR_COMPLETION] = 
		gtk_signal_new ("clear_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, clear_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_LOST_COMPLETION] =
		gtk_signal_new ("lost_completion",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (ECompletionClass, lost_completion),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, e_completion_signals, E_COMPLETION_LAST_SIGNAL);

	object_class->destroy = e_completion_destroy;
}

static void
e_completion_init (ECompletion *complete)
{
	complete->priv = g_new0 (struct _ECompletionPrivate, 1);
	complete->priv->matches = g_ptr_array_new ();
}

static void
e_completion_destroy (GtkObject *object)
{
	ECompletion *complete = E_COMPLETION (object);

	if (complete->priv) {
		g_free (complete->priv->search_text);
		complete->priv->search_text = NULL;

		e_completion_clear_matches (complete);
		e_completion_clear_search_stack (complete);

		g_ptr_array_free (complete->priv->matches, TRUE);
		complete->priv->matches = NULL;

		g_free (complete->priv);
		complete->priv = NULL;
	}

	if (parent_class->destroy)
		(parent_class->destroy) (object);
}

static void
e_completion_add_match (ECompletion *complete, ECompletionMatch *match)
{
	g_return_if_fail (complete && E_IS_COMPLETION (complete));
	g_return_if_fail (match != NULL);

	g_ptr_array_add (complete->priv->matches, match);

	if (complete->priv->matches->len == 1) {

		complete->priv->min_score = complete->priv->max_score = match->score;
		
	} else {

		complete->priv->min_score = MIN (complete->priv->min_score, match->score);
		complete->priv->max_score = MAX (complete->priv->max_score, match->score);

	}
}

static void
e_completion_clear_matches (ECompletion *complete)
{
	ECompletionMatch *match;
	GPtrArray *m;
	int i;

	g_return_if_fail (E_IS_COMPLETION (complete));

	m = complete->priv->matches;
	for (i = 0; i < m->len; i++) {
		match = g_ptr_array_index (m, i);
		e_completion_match_unref (match);
	}
	g_ptr_array_set_size (m, 0);

	complete->priv->min_score = 0;
	complete->priv->max_score = 0;
}

void
e_completion_clear (ECompletion *complete)
{
	g_return_if_fail (E_IS_COMPLETION (complete));

	/* FIXME: do we really want _clear and _clear_matches() ? */

	/* I think yes, because it is convenient to be able to clear our match cache
	   without emitting a "clear_completion" signal. -JT */

	e_completion_clear_matches (complete);
	e_completion_clear_search_stack (complete);
	complete->priv->refinement_count = 0;
	complete->priv->match_count = 0;
	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_CLEAR_COMPLETION]);
}

static void
e_completion_push_search (ECompletion *complete, const gchar *text, gint pos)
{
	ECompletionSearch *search;

	g_return_if_fail (E_IS_COMPLETION (complete));

	search = g_new (ECompletionSearch, 1);
	search->text = complete->priv->search_text;
	search->pos  = complete->priv->pos;
	complete->priv->search_stack = g_list_prepend (complete->priv->search_stack, search);

	complete->priv->search_text = g_strdup (text);
	complete->priv->pos = pos;
}

static void
e_completion_pop_search (ECompletion *complete)
{
	ECompletionSearch *search;
	GList *old_link = complete->priv->search_stack;

	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (complete->priv->search_stack != NULL);

	g_free (complete->priv->search_text);

	search = complete->priv->search_stack->data;
	complete->priv->search_text = search->text;
	complete->priv->pos = search->pos;

	g_free (search);
	complete->priv->search_stack = g_list_remove_link (complete->priv->search_stack,
							   complete->priv->search_stack);
	g_list_free_1 (old_link);
}

static void
e_completion_clear_search_stack (ECompletion *complete)
{
	GList *iter;
	
	g_return_if_fail (E_IS_COMPLETION (complete));

	for (iter = complete->priv->search_stack; iter != NULL; iter = g_list_next (iter)) {
		ECompletionSearch *search = iter->data;
		g_free (search->text);
		g_free (search);
	}
	g_list_free (complete->priv->search_stack);
	complete->priv->search_stack = NULL;
}

static void
e_completion_refine_search (ECompletion *comp, const gchar *text, gint pos, ECompletionRefineFn refine_fn)
{
	GPtrArray *m;
	gint i;
	
	comp->priv->refining = TRUE;

	e_completion_push_search (comp, text, pos);

	gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_BEGIN_COMPLETION], text, pos, comp->priv->limit);

	comp->priv->match_count = 0;

	comp->priv->searching = TRUE;

	m = comp->priv->matches;
	for (i = 0; i < m->len; ++i) {
		ECompletionMatch *match = g_ptr_array_index (m, i);
		if (comp->priv->refinement_count == match->hit_count
		    && refine_fn (comp, match, text, pos)) {
			++match->hit_count;
			gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_COMPLETION], match);
			++comp->priv->match_count;
		}
	}

	++comp->priv->refinement_count;

	gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_END_COMPLETION]);

	comp->priv->searching = FALSE;
	comp->priv->refining  = FALSE;
}

static void
e_completion_unrefine_search (ECompletion *comp)
{
	GPtrArray *m;
	gint i;

	comp->priv->refining = TRUE;

	e_completion_pop_search (comp);

	gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_BEGIN_COMPLETION], comp->priv->search_text, comp->priv->pos, comp->priv->limit);

	comp->priv->match_count = 0;
	--comp->priv->refinement_count;

	comp->priv->searching = TRUE;

	m = comp->priv->matches;
	for (i = 0; i < m->len; ++i) {
		ECompletionMatch *match = g_ptr_array_index (m, i);
		if (comp->priv->refinement_count <= match->hit_count) {
			match->hit_count = comp->priv->refinement_count;
			gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_COMPLETION], match);
			++comp->priv->match_count;
		}
	}

	gtk_signal_emit (GTK_OBJECT (comp), e_completion_signals[E_COMPLETION_END_COMPLETION]);

	comp->priv->searching = FALSE;
	comp->priv->refining  = FALSE;
}

void
e_completion_begin_search (ECompletion *complete, const gchar *text, gint pos, gint limit)
{
	ECompletionClass *klass;
	ECompletionRefineFn refine_fn;

	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (text != NULL);

	klass = E_COMPLETION_CLASS (GTK_OBJECT_GET_CLASS (complete));

	if (!complete->priv->searching && complete->priv->done_search) {

		/* If the search we are requesting is the same as what we had before our last refinement,
		   treat the request as an unrefine. */
		if (complete->priv->search_stack != NULL) {
			ECompletionSearch *search = complete->priv->search_stack->data;
			if ((klass->ignore_pos_on_auto_unrefine || search->pos == pos)
			    && !strcmp (search->text, text)) {
				e_completion_unrefine_search (complete);
				return;
			}
		}

		if (klass->auto_refine 
		    && (refine_fn = klass->auto_refine (complete,
							complete->priv->search_text, complete->priv->pos,
							text, pos))) {
			e_completion_refine_search (complete, text, pos, refine_fn);
			return;
		}

	}

	/* Stop any prior search. */
	if (complete->priv->searching)
		e_completion_cancel_search (complete);

	e_completion_clear_search_stack (complete);

	g_free (complete->priv->search_text);
	complete->priv->search_text = g_strdup (text);

	complete->priv->pos = pos;
	complete->priv->searching = TRUE;
	complete->priv->done_search = FALSE;

	e_completion_clear_matches (complete);

	complete->priv->limit = limit > 0 ? limit : G_MAXINT;
	complete->priv->refinement_count = 0;

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_BEGIN_COMPLETION], text, pos, limit);
	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_REQUEST_COMPLETION], text, pos, limit);
}

void
e_completion_cancel_search (ECompletion *complete)
{
	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));

	/* If there is no search to cancel, just silently return. */
	if (!complete->priv->searching)
		return;

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_CANCEL_COMPLETION]);

	complete->priv->searching = FALSE;
}

gboolean
e_completion_searching (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPLETION (complete), FALSE);

	return complete->priv->searching;
}

gboolean
e_completion_refining (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPLETION (complete), FALSE);

	return complete->priv->refining;
}

const gchar *
e_completion_search_text (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPLETION (complete), NULL);

	return complete->priv->search_text;
}

gint
e_completion_search_text_pos (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, -1);
	g_return_val_if_fail (E_IS_COMPLETION (complete), -1);

	return complete->priv->pos;
}

gint
e_completion_match_count (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, 0);
	g_return_val_if_fail (E_IS_COMPLETION (complete), 0);

	return complete->priv->refinement_count > 0 ? complete->priv->match_count : complete->priv->matches->len;
}

void
e_completion_foreach_match (ECompletion *complete, ECompletionMatchFn fn, gpointer closure)
{
	GPtrArray *m;
	int i;

	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));

	if (fn == NULL)
		return;

	m = complete->priv->matches;
	for (i = 0; i < m->len; i++) {
		ECompletionMatch *match = g_ptr_array_index (m, i);
		if (match->hit_count == complete->priv->refinement_count) {
			fn (match, closure);
		}
	}
}

ECompletion *
e_completion_new (void)
{
	return E_COMPLETION (gtk_type_new (e_completion_get_type ()));
}

static gboolean
e_completion_sort (ECompletion *complete)
{
	GPtrArray *m;
	int i;
	GList *sort_list = NULL, *j;
	gboolean diff;

	m = complete->priv->matches;

	for (i = 0; i < m->len; i++)
		sort_list = g_list_append (sort_list, 
					   g_ptr_array_index (m, i));
	
	sort_list = g_list_sort (sort_list, (GCompareFunc) e_completion_match_compare_alpha);

	diff = FALSE;

	for (i=0, j=sort_list; i < m->len; i++, j = g_list_next (j)) {
		if (g_ptr_array_index (m, i) == j->data)
			continue;

		diff = TRUE;
		g_ptr_array_index (m, i) = j->data;
	}

	g_list_free (sort_list);

	return diff;
}

/* Emit a restart signal and re-declare our matches, up to the limit. */
static void
e_completion_restart (ECompletion *complete)
{
	GPtrArray *m;
	gint i, count;

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_RESTART_COMPLETION]);
	
	m = complete->priv->matches;
	for (i = count = 0; 
	     i < m->len && count < complete->priv->limit; 
	     i++, count++) {
		gtk_signal_emit (GTK_OBJECT (complete),
				 e_completion_signals[E_COMPLETION_COMPLETION], 
				 g_ptr_array_index (m, i));
	}
}

void
e_completion_found_match (ECompletion *complete, ECompletionMatch *match)
{
	g_return_if_fail (complete);
	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (match != NULL);

	if (! complete->priv->searching) {
		g_warning ("e_completion_found_match(...,\"%s\",...) called outside of a search", match->match_text);
		return;
	}

	/* For now, do nothing when we hit the limit --- just don't
	 * announce the incoming matches. */
	if (complete->priv->matches->len >= complete->priv->limit) {
		e_completion_match_unref (match);
		return;
	}

	e_completion_add_match (complete, match);

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_COMPLETION], match);
}

/* to optimize this, make the match a hash table */
void
e_completion_lost_match (ECompletion *complete, ECompletionMatch *match)
{
	gboolean removed;

	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (match != NULL);

	/* FIXME: remove fast */
	removed = g_ptr_array_remove (complete->priv->matches,
				      match);

	/* maybe just return here? */
	g_return_if_fail (removed);

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_LOST_COMPLETION], match);

	e_completion_match_unref (match);
}

void
e_completion_end_search (ECompletion *complete)
{
	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (complete->priv->searching);

	/* our table model should be sorted by a non-visible column of
	 * doubles (the score) rather than whatever we are doing 
	 */
	/* If sorting by score accomplishes anything, issue a restart right before we end. */
	if (e_completion_sort (complete))
		e_completion_restart (complete);

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_END_COMPLETION]);

	complete->priv->searching = FALSE;
	complete->priv->done_search = TRUE;
}


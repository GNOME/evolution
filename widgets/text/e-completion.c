/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-completion.c - A base class for text completion.
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Adapted by Jon Trowbridge <trow@ximian.com>
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
#include "e-completion.h"
#include "gal/util/e-util.h"
#include "gal/util/e-marshal.h"

enum {
	COMPLETION_STARTED,
	COMPLETION_FOUND,
	COMPLETION_CANCELED,
	COMPLETION_FINISHED,
	LAST_SIGNAL
};

static guint e_completion_signals[LAST_SIGNAL] = { 0 };

struct _ECompletionPrivate {
	gboolean searching;
	gboolean done_search;
	gchar *search_text;
	GPtrArray *matches;
	gint pos;
	gint limit;
	double min_score, max_score;
};

static void e_completion_class_init (ECompletionClass *klass);
static void e_completion_init       (ECompletion *complete);
static void e_completion_dispose    (GObject *object);

static void     e_completion_add_match          (ECompletion *complete, ECompletionMatch *);
static void     e_completion_clear_matches      (ECompletion *complete);
#if 0
static gboolean e_completion_sort               (ECompletion *complete);
#endif

#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class;



E_MAKE_TYPE (e_completion,
	     "ECompletion",
	     ECompletion,
	     e_completion_class_init,
	     e_completion_init,
	     PARENT_TYPE)

static void
e_completion_class_init (ECompletionClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	e_completion_signals[COMPLETION_STARTED] =
		g_signal_new ("completion_started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionClass, completion_started),
			      NULL, NULL,
			      e_marshal_NONE__POINTER_INT_INT,
			      G_TYPE_NONE, 3,
			      G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT);

	e_completion_signals[COMPLETION_FOUND] =
		g_signal_new ("completion_found",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionClass, completion_found),
			      NULL, NULL,
			      e_marshal_NONE__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_completion_signals[COMPLETION_FINISHED] =
		g_signal_new ("completion_finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECompletionClass, completion_finished),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	object_class->dispose = e_completion_dispose;
}

static void
e_completion_init (ECompletion *complete)
{
	complete->priv = g_new0 (struct _ECompletionPrivate, 1);
	complete->priv->matches = g_ptr_array_new ();
}

static void
e_completion_dispose (GObject *object)
{
	ECompletion *complete = E_COMPLETION (object);

	if (complete->priv) {
		g_free (complete->priv->search_text);
		complete->priv->search_text = NULL;

		e_completion_clear_matches (complete);

		g_ptr_array_free (complete->priv->matches, TRUE);
		complete->priv->matches = NULL;

		g_free (complete->priv);
		complete->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(G_OBJECT_CLASS (parent_class)->dispose) (object);
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
e_completion_begin_search (ECompletion *complete, const gchar *text, gint pos, gint limit)
{
	ECompletionClass *klass;

	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (text != NULL);

	klass = E_COMPLETION_CLASS (GTK_OBJECT_GET_CLASS (complete));

	g_free (complete->priv->search_text);
	complete->priv->search_text = g_strdup (text);

	complete->priv->pos = pos;
	complete->priv->searching = TRUE;
	complete->priv->done_search = FALSE;

	e_completion_clear_matches (complete);

	complete->priv->limit = limit > 0 ? limit : G_MAXINT;

	g_signal_emit (complete, e_completion_signals[COMPLETION_STARTED], 0, text, pos, limit);
	if (klass->request_completion)
		klass->request_completion (complete, text, pos, limit);
}

gboolean
e_completion_searching (ECompletion *complete)
{
	g_return_val_if_fail (complete != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMPLETION (complete), FALSE);

	return complete->priv->searching;
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

	return complete->priv->matches->len;
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
		fn (match, closure);
	}
}

ECompletion *
e_completion_new (void)
{
	return E_COMPLETION (g_object_new (E_COMPLETION_TYPE, NULL));
}

#if 0
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
#endif

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

	g_signal_emit (complete, e_completion_signals[COMPLETION_FOUND], 0, match);
}

void
e_completion_end_search (ECompletion *comp)
{
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_COMPLETION (comp));
	g_return_if_fail (comp->priv->searching);

	if (E_COMPLETION_CLASS (GTK_OBJECT_GET_CLASS (comp))->end_completion) {
		E_COMPLETION_CLASS (GTK_OBJECT_GET_CLASS (comp))->end_completion (comp);
	}
	g_signal_emit (comp, e_completion_signals[COMPLETION_FINISHED], 0);

	comp->priv->searching = FALSE;
	comp->priv->done_search = TRUE;
}


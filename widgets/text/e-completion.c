/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* ECompletion - A base class for text completion.
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Author: Miguel de Icaza <miguel@ximian.com>
 * Adapted by Jon Trowbridge <trow@ximian.com>
 *
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
 * USA
 */


#include <config.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "e-completion.h"

enum {
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
	gchar *search_text;
	GPtrArray *matches;
	gint pos;
	gint limit;
	double min_score, max_score;
};

static void e_completion_class_init (ECompletionClass *klass);
static void e_completion_init       (ECompletion *complete);
static void e_completion_destroy    (GtkObject *object);

static void     e_completion_add_match     (ECompletion *complete, ECompletionMatch *);
static void     e_completion_clear_matches (ECompletion *complete);
static gboolean e_completion_sort          (ECompletion *complete);
static void     e_completion_restart       (ECompletion *complete);

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

	e_completion_signals[E_COMPLETION_BEGIN_COMPLETION] =
		gtk_signal_new ("begin_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, begin_completion),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);

	e_completion_signals[E_COMPLETION_COMPLETION] =
		gtk_signal_new ("completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, completion),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	e_completion_signals[E_COMPLETION_RESTART_COMPLETION] =
		gtk_signal_new ("restart_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, restart_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_CANCEL_COMPLETION] =
		gtk_signal_new ("cancel_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, cancel_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_END_COMPLETION] =
		gtk_signal_new ("end_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, end_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_CLEAR_COMPLETION] = 
		gtk_signal_new ("clear_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, clear_completion),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_completion_signals[E_COMPLETION_LOST_COMPLETION] =
		gtk_signal_new ("lost_completion",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ECompletionClass, lost_completion),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_completion_signals, E_COMPLETION_LAST_SIGNAL);

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

	g_free (complete->priv->search_text);
	complete->priv->search_text = NULL;

	e_completion_clear_matches (complete);

	g_ptr_array_free (complete->priv->matches, TRUE);
	complete->priv->matches = NULL;

	g_free (complete->priv);
	complete->priv = NULL;

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
	e_completion_clear_matches (complete);
	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_CLEAR_COMPLETION]);
}

void
e_completion_begin_search (ECompletion *complete, const gchar *text, gint pos, gint limit)
{
	g_return_if_fail (complete != NULL);
	g_return_if_fail (E_IS_COMPLETION (complete));
	g_return_if_fail (text != NULL);

	/* Stop any prior search. */
	if (complete->priv->searching)
		e_completion_cancel_search (complete);

	g_free (complete->priv->search_text);
	complete->priv->search_text = g_strdup (text);

	complete->priv->pos = pos;
	complete->priv->searching = TRUE;

	e_completion_clear_matches (complete);

	complete->priv->limit = limit > 0 ? limit : G_MAXINT;

	gtk_signal_emit (GTK_OBJECT (complete), e_completion_signals[E_COMPLETION_BEGIN_COMPLETION], text, pos, limit);
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
	for (i = 0; i < m->len; i++)
		fn (g_ptr_array_index (m, i), closure);
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
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-completion-match.c
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
#include <gal/unicode/gunicode.h>
#include <gal/widgets/e-unicode.h>
#include "e-completion-match.h"

static void
e_completion_match_destroy (ECompletionMatch *match)
{
	if (match) {
		g_free (match->match_text);
		g_free (match->menu_text);
		if (match->destroy)
			match->destroy (match);
		g_free (match);
	}
}

void
e_completion_match_construct (ECompletionMatch *match)
{
	g_return_if_fail (match != NULL);
	
	match->match_text = NULL;
	match->menu_text = NULL;
	match->score = 0;
	match->sort_major = 0;
	match->sort_minor = 0;
	match->user_data = NULL;
	match->ref = 1;
	match->destroy = NULL;
}

void
e_completion_match_ref (ECompletionMatch *match)
{
	g_return_if_fail (match != NULL);
	g_return_if_fail (match->ref > 0);

	++match->ref;
}

void
e_completion_match_unref (ECompletionMatch *match)
{
	g_return_if_fail (match != NULL);
	g_return_if_fail (match->ref > 0);

	--match->ref;
	if (match->ref == 0) {
		e_completion_match_destroy (match);
	}
}

void
e_completion_match_set_text (ECompletionMatch *match,
			     const gchar *match_text,
			     const gchar *menu_text)
{
	g_return_if_fail (match != NULL);

	g_free (match->match_text);
	g_free (match->menu_text);

	if (match_text == NULL) {
		match_text = "Unknown_Match";
	} else if (! g_utf8_validate (match_text, 0, NULL)) {
		match_text = "Invalid_UTF8";
	}

	if (menu_text == NULL) {
		menu_text = match_text;
	} else if (! g_utf8_validate (menu_text, 0, NULL)) {
		menu_text = "Invalid_UTF8";
	}

	match->match_text = g_strdup (match_text);
	match->menu_text  = g_strdup (menu_text);
}

const gchar *
e_completion_match_get_match_text (ECompletionMatch *match)
{
	return match ? match->match_text : "NULL_Match";
}

const gchar *
e_completion_match_get_menu_text (ECompletionMatch *match)
{
	return match ? match->menu_text : "NULL_Match";
}

gint
e_completion_match_compare (const ECompletionMatch *a, const ECompletionMatch *b)
{
	gint rv;

	/* Deal with NULL arguments. */
	if (!(a || b)) {
		if (!(a && b))
			return 0;
		return a ? -1 : 1;
	}

	/* Sort the scores high->low. */
	if ( (rv = (b->score > a->score) - (a->score > b->score)) )
		return rv;

	if ( (rv = (b->sort_major < a->sort_major) - (a->sort_major < b->sort_major)) )
		return rv;

	if ( (rv = (b->sort_minor < a->sort_minor) - (a->sort_minor < b->sort_minor)) )
		return rv;

	return 0;
}

gint
e_completion_match_compare_alpha (const ECompletionMatch *a, const ECompletionMatch *b)
{
	gint rv, rv2;

	/* Deal with NULL arguments. */
	if (!(a || b)) {
		if (!(a && b))
			return 0;
		return a ? -1 : 1;
	}

	/* Sort the scores high->low. */
	if ( (rv = (b->score > a->score) - (a->score > b->score)) )
		return rv;

	/* When the match text is the same, we use the major and minor fields */
	rv2 = strcmp (a->match_text, b->match_text);
	if (!rv2) {
		if ( (rv = (b->sort_major < a->sort_major) - (a->sort_major < b->sort_major)) )
			return rv;

		if ( (rv = (b->sort_minor < a->sort_minor) - (a->sort_minor < b->sort_minor)) )
			return rv;
	}

	return strcmp (a->menu_text, b->menu_text);
}

ECompletionMatch *
e_completion_match_new (const gchar *match_text, const gchar *menu_text, double score)
{
	ECompletionMatch *match = g_new0 (ECompletionMatch, 1);

	e_completion_match_construct (match);
	e_completion_match_set_text (match, match_text, menu_text);
	match->score = score;

	return match;
}

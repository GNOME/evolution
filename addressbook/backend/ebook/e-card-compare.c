/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-card-compare.c
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
#include <gal/unicode/gunicode.h>
#include "e-book-util.h"
#include "e-card-compare.h"

/* This is an "optimistic" combiner: the best of the two outcomes is
   selected. */
static ECardMatchType
combine_comparisons (ECardMatchType prev,
		     ECardMatchType new_info)
{
	if (new_info == E_CARD_MATCH_NOT_APPLICABLE)
		return prev;
	return (ECardMatchType) MAX ((gint) prev, (gint) new_info);
}


/*** Name comparisons ***/

/* This *so* doesn't belong here... at least not implemented in a
   sucky way like this.  But by getting it in here now, I can fix it
   up w/o adding a new feature when we are in feature freeze. :-) */

/* This is very Anglocentric. */
static gchar *name_synonyms[][2] = {
	{ "jon", "john" },   /* Ah, the hacker's perogative */
	{ "joseph", "joe" },
	{ "robert", "bob" },
	{ "richard", "dick" },
	{ "william", "bill" },
	{ "anthony", "tony" },
	{ "michael", "mike" },
	{ "eric", "erik" },
	{ "elizabeth", "liz" },
	{ "jeff", "geoff" },
	{ "jeff", "geoffrey" },
	{ "jim", "james" },
	{ "abigal", "abby" },
	{ "amanda", "amy" },
	{ "amanda", "manda" },
	{ "jennifer", "jenny" },
	{ "rebecca", "becca" },
	{ "rebecca", "becky" },
	{ "anderson", "andersen" },
	/* We could go on and on... */
	{ NULL, NULL }
};
	
static gboolean
name_fragment_match (const gchar *a, const gchar *b)
{
	gint i, len_a, len_b;

	/* This will cause "Chris" and "Christopher" to match. */
	len_a = g_utf8_strlen (a, -1);
	len_b = g_utf8_strlen (b, -1);
	if (!g_utf8_strncasecmp (a, b, MIN (len_a, len_b)))
		return TRUE;

	/* Check for nicknames.  Yes, the linear search blows. */
	for (i=0; name_synonyms[i][0]; ++i) {

		if (!g_utf8_strcasecmp (name_synonyms[i][0], a)
		    && !g_utf8_strcasecmp (name_synonyms[i][1], b))
			return TRUE;
		
		if (!g_utf8_strcasecmp (name_synonyms[i][0], b)
		    && !g_utf8_strcasecmp (name_synonyms[i][1], a))
			return TRUE;
	}

	return FALSE;
}

ECardMatchType
e_card_compare_name (ECard *card1, ECard *card2)
{
	ECardName *a, *b;
	gint matches=0, possible=0;

	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	a = card1->name;
	b = card2->name;

	if (a == NULL || b == NULL)
		return E_CARD_MATCH_NOT_APPLICABLE;

	if (a->given && b->given) {
		++possible;
		if (name_fragment_match (a->given, b->given))
			++matches;
	}

	if (a->additional && b->additional) {
		++possible;
		if (name_fragment_match (a->additional, b->additional))
			++matches;
	}

	if (a->family && b->family) {
		++possible;
		if (name_fragment_match (a->family, b->family))
			++matches;
	}

	/* Now look at the # of matches and try to intelligently map
	   an E_CARD_MATCH_* type to it. */

	if (possible == 0)
		return E_CARD_MATCH_NOT_APPLICABLE;
	if (matches == 0)
		return E_CARD_MATCH_NONE;

	if (matches == possible) {
		return possible > 1 ? E_CARD_MATCH_EXACT : E_CARD_MATCH_PARTIAL;
	} else if (matches == possible-1)
		return E_CARD_MATCH_PARTIAL;
	else
		return E_CARD_MATCH_VAGUE;
}


/*** Nickname Comparisons ***/

ECardMatchType
e_card_compare_nickname (ECard *card1, ECard *card2)
{
	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	return E_CARD_MATCH_NOT_APPLICABLE;
}



/*** E-mail Comparisons ***/

static gboolean
match_email_username (const gchar *addr1, const gchar *addr2)
{
	gint c1, c2;
	if (addr1 == NULL || addr2 == NULL)
		return FALSE;

	while (*addr1 && *addr2 && *addr1 != '@' && *addr2 != '@') {
		c1 = isupper (*addr1) ? tolower (*addr1) : *addr1;
		c2 = isupper (*addr2) ? tolower (*addr2) : *addr2;
		if (c1 != c2)
			return FALSE;
		++addr1;
		++addr2;
	}

	return *addr1 == *addr2;
}

static gboolean
match_email_hostname (const gchar *addr1, const gchar *addr2)
{
	gint c1, c2;
	gboolean seen_at1, seen_at2;
	if (addr1 == NULL || addr2 == NULL)
		return FALSE;

	/* Walk to the end of each string. */
	seen_at1 = FALSE;
	if (*addr1) {
		while (*addr1) {
			if (*addr1 == '@')
				seen_at1 = TRUE;
			++addr1;
		}
		--addr1;
	}

	seen_at2 = FALSE;
	if (*addr2) {
		while (*addr2) {
			if (*addr2 == '@')
				seen_at2 = TRUE;
			++addr2;
		}
		--addr2;
	}

	if (!seen_at1 && !seen_at2)
		return TRUE;
	if (!seen_at1 || !seen_at2)
		return FALSE;

	while (*addr1 != '@' && *addr2 != '@') {
		c1 = isupper (*addr1) ? tolower (*addr1) : *addr1;
		c2 = isupper (*addr2) ? tolower (*addr2) : *addr2;
		if (c1 != c2)
			return FALSE;
		--addr1;
		--addr2;
	}

	/* This will match bob@foo.ximian.com and bob@ximian.com */
	return *addr1 == '.' || *addr2 == '.';
}

static ECardMatchType
compare_email_addresses (const gchar *addr1, const gchar *addr2)
{
	if (addr1 == NULL || addr2 == NULL)
		return E_CARD_MATCH_NOT_APPLICABLE;

	if (match_email_username (addr1, addr2)) 
		return match_email_hostname (addr1, addr2) ? E_CARD_MATCH_EXACT : E_CARD_MATCH_PARTIAL;

	return E_CARD_MATCH_NONE;
}

ECardMatchType
e_card_compare_email (ECard *card1, ECard *card2)
{
	EIterator *i1, *i2;
	ECardMatchType match = E_CARD_MATCH_NOT_APPLICABLE;

	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	if (card1->email == NULL || card2->email == NULL)
		return E_CARD_MATCH_NOT_APPLICABLE;

	i1 = e_list_get_iterator (card1->email);
	i2 = e_list_get_iterator (card2->email);

	/* Do pairwise-comparisons on all of the e-mail addresses.  If
	   we find an exact match, there is no reason to keep
	   checking. */
	e_iterator_reset (i1);
	while (e_iterator_is_valid (i1) && match != E_CARD_MATCH_EXACT) {
		const gchar *addr1 = (const gchar *) e_iterator_get (i1);

		e_iterator_reset (i2);
		while (e_iterator_is_valid (i2) && match != E_CARD_MATCH_EXACT) {
			const gchar *addr2 = (const gchar *) e_iterator_get (i2);

			match = combine_comparisons (match, compare_email_addresses (addr1, addr2));
			
			e_iterator_next (i2);
		}

		e_iterator_next (i1);
	}

	gtk_object_unref (GTK_OBJECT (i1));
	gtk_object_unref (GTK_OBJECT (i2));

	return match;
}

ECardMatchType
e_card_compare_address (ECard *card1, ECard *card2)
{
	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	/* Unimplemented */

	return E_CARD_MATCH_NOT_APPLICABLE;
}

ECardMatchType
e_card_compare_telephone (ECard *card1, ECard *card2)
{
	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	/* Unimplemented */

	return E_CARD_MATCH_NOT_APPLICABLE;
}

ECardMatchType
e_card_compare (ECard *card1, ECard *card2)
{
	ECardMatchType result;

	g_return_val_if_fail (card1 && E_IS_CARD (card1), E_CARD_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (card2 && E_IS_CARD (card2), E_CARD_MATCH_NOT_APPLICABLE);

	result = E_CARD_MATCH_NONE;
	result = combine_comparisons (result, e_card_compare_name      (card1, card2));
	result = combine_comparisons (result, e_card_compare_nickname  (card1, card2));
	result = combine_comparisons (result, e_card_compare_email     (card1, card2));
	result = combine_comparisons (result, e_card_compare_address   (card1, card2));
	result = combine_comparisons (result, e_card_compare_telephone (card1, card2));

	return result;
}

typedef struct _MatchSearchInfo MatchSearchInfo;
struct _MatchSearchInfo {
	ECard *card;
	GList *avoid;
	ECardMatchQueryCallback cb;
	gpointer closure;
};

static void
match_search_info_free (MatchSearchInfo *info)
{
	if (info) {
		gtk_object_unref (GTK_OBJECT (info->card));
		g_free (info);
	}
}

static void
simple_query_cb (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	MatchSearchInfo *info = (MatchSearchInfo *) closure;
	ECardMatchType best_match = E_CARD_MATCH_NONE;
	ECard *best_card = NULL;
	const GList *i;

	if (status != E_BOOK_SIMPLE_QUERY_STATUS_SUCCESS) {
		info->cb (info->card, NULL, E_CARD_MATCH_NONE, info->closure);
		match_search_info_free (info);
		return;
	}

	for (i = cards; i != NULL; i = g_list_next (i)) {
		ECard *this_card = E_CARD (i->data);
		ECardMatchType this_match = e_card_compare (info->card, this_card);
		if ((gint)this_match > (gint)best_match) {
			best_match = this_match;
			best_card  = this_card;
		}
	}

	info->cb (info->card, best_card, best_match, info->closure);
	match_search_info_free (info);
}

#define MAX_QUERY_PARTS 10
static void
use_common_book_cb (EBook *book, gpointer closure)
{
	MatchSearchInfo *info = (MatchSearchInfo *) closure;
	ECard *card = info->card;
	gchar *query_parts[MAX_QUERY_PARTS];
	gint p=0;
	gchar *query, *qj;
	int i;

	if (book == NULL) {
		info->cb (info->card, NULL, E_CARD_MATCH_NONE, info->closure);
		match_search_info_free (info);
		return;
	}

	if (card->nickname)
		query_parts[p++] = g_strdup_printf ("(beginswith \"nickname\" \"%s\")", card->nickname);


	if (card->name->given && strlen (card->name->given) > 1)
		query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", card->name->given);

	if (card->name->additional && strlen (card->name->additional) > 1)
		query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", card->name->additional);

	if (card->name->family && strlen (card->name->family) > 1)
		query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", card->name->family);

		
	if (card->email) {
		EIterator *iter = e_list_get_iterator (card->email);
		while (e_iterator_is_valid (iter) && p < MAX_QUERY_PARTS) {
			gchar *addr = g_strdup (e_iterator_get (iter));
			if (addr) {
				gchar *s = addr;
				while (*s) {
					if (*s == '@') {
						*s = '\0';
						break;
					}
					++s;
				}
				query_parts[p++] = g_strdup_printf ("(beginswith \"email\" \"%s\")", addr);
				g_free (addr);
			}
			e_iterator_next (iter);
		}
	}

	
	
	/* Build up our full query from the parts. */
	query_parts[p] = NULL;
	qj = g_strjoinv (" ", query_parts);
	for(i = 0; query_parts[i] != NULL; i++)
		g_free(query_parts[i]);
	if (p > 0) {
		query = g_strdup_printf ("(or %s)", qj);
		g_free (qj);
	} else {
		query = qj;
	}

	if (info->avoid) {
		GList *iterator;
		p = 0;
		query_parts[p++] = query;
		for (iterator = info->avoid; iterator; iterator = iterator->next) {
			query_parts[p++] = g_strdup_printf("(not (is \"id\" \"%s\"))", e_card_get_id (iterator->data));
		}
		query_parts[p] = 0;
		qj = g_strjoinv (" ", query_parts);
		for(i = 0; query_parts[i] != NULL; i++)
			g_free(query_parts[i]);
		query = g_strdup_printf ("(and %s)", qj);
		g_list_foreach (info->avoid, (GFunc) gtk_object_unref, NULL);
		g_list_free (info->avoid);
		info->avoid = NULL;
	}

	e_book_simple_query (book, query, simple_query_cb, info);

	g_free (query);
}

void
e_card_locate_match (ECard *card, ECardMatchQueryCallback cb, gpointer closure)
{
	MatchSearchInfo *info;

	g_return_if_fail (card && E_IS_CARD (card));
	g_return_if_fail (cb != NULL);

	info = g_new (MatchSearchInfo, 1);
	info->card = card;
	gtk_object_ref (GTK_OBJECT (card));
	info->cb = cb;
	info->closure = closure;
	info->avoid = NULL;

	e_book_use_local_address_book (use_common_book_cb, info);
}

/**
 * e_card_locate_match_full:
 * @book: The book to look in.  If this is NULL, use the main local
 * addressbook.
 * @card: The card to compare to.
 * @avoid: A list of cards to not match.  These will not show up in the search.
 * @cb: The function to call.
 * @closure: The closure to add to the call.
 * 
 * Look for the best match and return it using the ECardMatchQueryCallback.
 **/
void
e_card_locate_match_full (EBook *book, ECard *card, GList *avoid, ECardMatchQueryCallback cb, gpointer closure)
{
	MatchSearchInfo *info;

	g_return_if_fail (card && E_IS_CARD (card));
	g_return_if_fail (cb != NULL);

	info = g_new (MatchSearchInfo, 1);
	info->card = card;
	gtk_object_ref (GTK_OBJECT (card));
	info->cb = cb;
	info->closure = closure;
	info->avoid = g_list_copy (avoid);
	g_list_foreach (info->avoid, (GFunc) gtk_object_ref, NULL);

	if (book)
		use_common_book_cb (book, info);
	else
		e_book_use_local_address_book (use_common_book_cb, info);
}


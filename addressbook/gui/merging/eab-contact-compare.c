/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * eab-contact-compare.c
 *
 * Copyright (C) 2001, 2002, 2003 Ximian, Inc.
 *
 * Authors: Jon Trowbridge <trow@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
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
#include <ctype.h>
#include <string.h>
#include "util/eab-book-util.h"
#include "../component/addressbook.h"
#include "eab-contact-compare.h"

/* This is an "optimistic" combiner: the best of the two outcomes is
   selected. */
static EABContactMatchType
combine_comparisons (EABContactMatchType prev,
		     EABContactMatchType new_info)
{
	if (new_info == EAB_CONTACT_MATCH_NOT_APPLICABLE)
		return prev;
	return (EABContactMatchType) MAX ((gint) prev, (gint) new_info);
}


/*** Name comparisons ***/

/* This *so* doesn't belong here... at least not implemented in a
   sucky way like this.  But it can be fixed later. */

/* This is very Anglocentric. */
static gchar *name_synonyms[][2] = {
	{ "jon", "john" },   /* Ah, the hacker's perogative */
	{ "joseph", "joe" },
	{ "robert", "bob" },
	{ "gene", "jean" },
	{ "jesse", "jessie" },
	{ "ian", "iain" },
	{ "richard", "dick" },
	{ "william", "bill" },
	{ "william", "will" },
	{ "anthony", "tony" },
	{ "michael", "mike" },
	{ "eric", "erik" },
	{ "elizabeth", "liz" },
	{ "jeff", "geoff" },
	{ "jeff", "geoffrey" },
	{ "tom", "thomas" },
	{ "dave", "david" },
	{ "jim", "james" },
	{ "abigal", "abby" },
	{ "amanda", "amy" },
	{ "amanda", "manda" },
	{ "jennifer", "jenny" },
	{ "christopher", "chris" },
	{ "rebecca", "becca" },
	{ "rebecca", "becky" },
	{ "anderson", "andersen" },
	{ "johnson", "johnsen" },
	/* We could go on and on... */
	/* We should add soundex here. */
	{ NULL, NULL }
};

static gboolean
name_fragment_match (const gchar *a, const gchar *b, gboolean strict)
{
	gint len;

	if (!(a && b && *a && *b))
		return FALSE;

	/* If we are in 'strict' mode, b must match the beginning of a.
	   So "Robert", "Rob" would match, but "Robert", "Robbie" wouldn't.

	   If strict is FALSE, it is sufficient for the strings to share
	   some leading characters.  In this case, "Robert" and "Robbie"
	   would match, as would "Dave" and "Dan". */
	
	if (strict) {
		len = g_utf8_strlen (b, -1);
	} else {
		len = MIN (g_utf8_strlen (a, -1), g_utf8_strlen (b, -1));
	}

	return !e_utf8_casefold_collate_len (a, b, len);
}

static gboolean
name_fragment_match_with_synonyms (const gchar *a, const gchar *b, gboolean strict)
{
	gint i;

	if (!(a && b && *a && *b))
		return FALSE;

	if (name_fragment_match (a, b, strict))
		return TRUE;

	/* Check for nicknames.  Yes, the linear search blows. */
	for (i=0; name_synonyms[i][0]; ++i) {

		if (!e_utf8_casefold_collate (name_synonyms[i][0], a)
		    && !e_utf8_casefold_collate (name_synonyms[i][1], b))
			return TRUE;
		
		if (!e_utf8_casefold_collate (name_synonyms[i][0], b)
		    && !e_utf8_casefold_collate (name_synonyms[i][1], a))
			return TRUE;
	}

	return FALSE;
}

EABContactMatchType
eab_contact_compare_name_to_string (EContact *contact, const gchar *str)
{
	return eab_contact_compare_name_to_string_full (contact, str, FALSE, NULL, NULL, NULL);
}

EABContactMatchType
eab_contact_compare_name_to_string_full (EContact *contact, const gchar *str, gboolean allow_partial_matches,
					 gint *matched_parts_out, EABContactMatchPart *first_matched_part_out, gint *matched_character_count_out)
{
	gchar **namev, **givenv = NULL, **addv = NULL, **familyv = NULL;

	gint matched_parts = EAB_CONTACT_MATCH_PART_NONE;
	EABContactMatchPart first_matched_part = EAB_CONTACT_MATCH_PART_NONE;
	EABContactMatchPart this_part_match = EAB_CONTACT_MATCH_PART_NOT_APPLICABLE;
	EABContactMatchType match_type;
	EContactName *contact_name;

	gint match_count = 0, matched_character_count = 0, fragment_count;
	gint i, j;
	gchar *str_cpy, *s;

	g_return_val_if_fail (E_IS_CONTACT (contact), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	if (!e_contact_get_const (contact, E_CONTACT_FULL_NAME))
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;
	if (str == NULL)
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;

	str_cpy = s = g_strdup (str);
	while (*s) {
		if (*s == ',' || *s == '"')
			*s = ' ';
		++s;
	}
	namev   = g_strsplit (str_cpy, " ", 0);
	g_free (str_cpy);

	contact_name = e_contact_get (contact, E_CONTACT_NAME);

	if (contact_name->given)
		givenv = g_strsplit (contact_name->given, " ", 0);
	if (contact_name->additional)
		addv = g_strsplit (contact_name->additional, " ", 0);
	if (contact_name->family)
		familyv = g_strsplit (contact_name->family, " ", 0);

	e_contact_name_free (contact_name);

	fragment_count = 0;
	for (i = 0; givenv && givenv[i]; ++i)
		++fragment_count;
	for (i = 0; addv && addv[i]; ++i)
		++fragment_count;
	for (i = 0; familyv && familyv[i]; ++i)
		++fragment_count;
	
	for (i = 0; namev[i] && this_part_match != EAB_CONTACT_MATCH_PART_NONE; ++i) {

		if (*namev[i]) {

			this_part_match = EAB_CONTACT_MATCH_PART_NONE;

			/* When we are allowing partials, we are strict about the matches we allow.
			   Does this make sense?  Not really, but it does the right thing for the purposes
			   of completion. */

			if (givenv && this_part_match == EAB_CONTACT_MATCH_PART_NONE) {
				for (j = 0; givenv[j]; ++j) {
					if (name_fragment_match_with_synonyms (givenv[j], namev[i], allow_partial_matches)) {

						this_part_match = EAB_CONTACT_MATCH_PART_GIVEN_NAME;

						/* We remove a piece of a name once it has been matched against, so
						   that "john john" won't match "john doe". */
						g_free (givenv[j]);
						givenv[j] = g_strdup ("");
						break;
					}
				}
			}

			if (addv && this_part_match == EAB_CONTACT_MATCH_PART_NONE) {
				for (j = 0; addv[j]; ++j) {
					if (name_fragment_match_with_synonyms (addv[j], namev[i], allow_partial_matches)) {
						
						this_part_match = EAB_CONTACT_MATCH_PART_ADDITIONAL_NAME;

						g_free (addv[j]);
						addv[j] = g_strdup ("");
						break;
					}
				}
			}

			if (familyv && this_part_match == EAB_CONTACT_MATCH_PART_NONE) {
				for (j = 0; familyv[j]; ++j) {
					if (allow_partial_matches ? name_fragment_match_with_synonyms (familyv[j], namev[i], allow_partial_matches)
					    : !e_utf8_casefold_collate (familyv[j], namev[i])) {

						this_part_match = EAB_CONTACT_MATCH_PART_FAMILY_NAME;

						g_free (familyv[j]);
						familyv[j] = g_strdup ("");
						break;
					}
				}
			}

			if (this_part_match != EAB_CONTACT_MATCH_PART_NONE) {
				++match_count;
				matched_character_count += g_utf8_strlen (namev[i], -1);
				matched_parts |= this_part_match;
				if (first_matched_part == EAB_CONTACT_MATCH_PART_NONE)
					first_matched_part = this_part_match;
			}
		}
	}

	match_type = EAB_CONTACT_MATCH_NONE;

	if (this_part_match != EAB_CONTACT_MATCH_PART_NONE) {

		if (match_count > 0)
			match_type = EAB_CONTACT_MATCH_VAGUE;
		
		if (fragment_count == match_count) {

			match_type = EAB_CONTACT_MATCH_EXACT;

		} else if (fragment_count == match_count + 1) {

			match_type = EAB_CONTACT_MATCH_PARTIAL;

		}
	}

	if (matched_parts_out)
		*matched_parts_out = matched_parts;
	if (first_matched_part_out)
		*first_matched_part_out = first_matched_part;
	if (matched_character_count_out)
		*matched_character_count_out = matched_character_count;

	g_strfreev (namev);
	g_strfreev (givenv);
	g_strfreev (addv);
	g_strfreev (familyv);

	return match_type;
}

EABContactMatchType
eab_contact_compare_file_as (EContact *contact1, EContact *contact2)
{
	EABContactMatchType match_type;
	gchar *a, *b;

	g_return_val_if_fail (E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	a = e_contact_get (contact1, E_CONTACT_FILE_AS);
	b = e_contact_get (contact2, E_CONTACT_FILE_AS);

	if (a == NULL || b == NULL) {
		g_free (a);
		g_free (b);
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;
	}

	if (!strcmp (a, b))
		match_type = EAB_CONTACT_MATCH_EXACT;
	else if (g_utf8_validate (a, -1, NULL) && g_utf8_validate (b, -1, NULL) &&
		 !g_utf8_collate (a, b))
		match_type = EAB_CONTACT_MATCH_PARTIAL;
	else
		match_type = EAB_CONTACT_MATCH_NONE;

	g_free (a);
	g_free (b);
	return match_type;
}

EABContactMatchType
eab_contact_compare_name (EContact *contact1, EContact *contact2)
{
	EContactName *a, *b;
	gint matches=0, possible=0;
	gboolean given_match = FALSE, additional_match = FALSE, family_match = FALSE;

	g_return_val_if_fail (E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	a = e_contact_get (contact1, E_CONTACT_NAME);
	b = e_contact_get (contact2, E_CONTACT_NAME);

	if (a == NULL || b == NULL) {
		g_free (a);
		g_free (b);
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;
	}

	if (a->given && b->given && *a->given && *b->given) {
		++possible;
		if (name_fragment_match_with_synonyms (a->given, b->given, FALSE /* both inputs are complete */)) {
			++matches;
			given_match = TRUE;
		}
	}

	if (a->additional && b->additional && *a->additional && *b->additional) {
		++possible;
		if (name_fragment_match_with_synonyms (a->additional, b->additional, FALSE /* both inputs are complete */)) {
			++matches;
			additional_match = TRUE;
		}
	}

	if (a->family && b->family && *a->family && *b->family) {
		++possible;
		/* We don't allow "loose matching" (i.e. John vs. Jon) on family names */
		if (! e_utf8_casefold_collate (a->family, b->family)) {
			++matches;
			family_match = TRUE;
		}
	}

	e_contact_name_free (a);
	e_contact_name_free (b);

	/* Now look at the # of matches and try to intelligently map
	   an EAB_CONTACT_MATCH_* type to it.  Special consideration is given
	   to family-name matches. */

	if (possible == 0)
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;

	if (possible == 1)
		return family_match ? EAB_CONTACT_MATCH_VAGUE : EAB_CONTACT_MATCH_NONE;

	if (possible == matches)
		return family_match ? EAB_CONTACT_MATCH_EXACT : EAB_CONTACT_MATCH_PARTIAL;

	if (possible == matches+1)
		return family_match ? EAB_CONTACT_MATCH_VAGUE : EAB_CONTACT_MATCH_NONE;

	return EAB_CONTACT_MATCH_NONE;
}


/*** Nickname Comparisons ***/

EABContactMatchType
eab_contact_compare_nickname (EContact *contact1, EContact *contact2)
{
	g_return_val_if_fail (contact1 && E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (contact2 && E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	return EAB_CONTACT_MATCH_NOT_APPLICABLE;
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

static EABContactMatchType
compare_email_addresses (const gchar *addr1, const gchar *addr2)
{
	if (addr1 == NULL || *addr1 == 0 ||
	    addr2 == NULL || *addr2 == 0)
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;

	if (match_email_username (addr1, addr2)) 
		return match_email_hostname (addr1, addr2) ? EAB_CONTACT_MATCH_EXACT : EAB_CONTACT_MATCH_VAGUE;

	return EAB_CONTACT_MATCH_NONE;
}

EABContactMatchType
eab_contact_compare_email (EContact *contact1, EContact *contact2)
{
	EABContactMatchType match = EAB_CONTACT_MATCH_NOT_APPLICABLE;
	GList *contact1_email, *contact2_email;
	GList *i1, *i2;

	g_return_val_if_fail (contact1 && E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (contact2 && E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	contact1_email = e_contact_get (contact1, E_CONTACT_EMAIL);
	contact2_email = e_contact_get (contact2, E_CONTACT_EMAIL);

	if (contact1_email == NULL || contact2_email == NULL) {
		g_list_foreach (contact1_email, (GFunc)g_free, NULL);
		g_list_free (contact1_email);

		g_list_foreach (contact2_email, (GFunc)g_free, NULL);
		g_list_free (contact2_email);
		return EAB_CONTACT_MATCH_NOT_APPLICABLE;
	}

	i1 = contact1_email;

	/* Do pairwise-comparisons on all of the e-mail addresses.  If
	   we find an exact match, there is no reason to keep
	   checking. */
	while (i1 && match != EAB_CONTACT_MATCH_EXACT) {
		char *addr1 = (char *) i1->data;

		i2 = contact2_email;
		while (i2 && match != EAB_CONTACT_MATCH_EXACT) {
			char *addr2 = (char *) i2->data;

			match = combine_comparisons (match, compare_email_addresses (addr1, addr2));
			
			i2 = i2->next;
		}

		i1 = i1->next;
	}

	g_list_foreach (contact1_email, (GFunc)g_free, NULL);
	g_list_free (contact1_email);

	g_list_foreach (contact2_email, (GFunc)g_free, NULL);
	g_list_free (contact2_email);

	return match;
}

EABContactMatchType
eab_contact_compare_address (EContact *contact1, EContact *contact2)
{
	g_return_val_if_fail (contact1 && E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (contact2 && E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	/* Unimplemented */

	return EAB_CONTACT_MATCH_NOT_APPLICABLE;
}

EABContactMatchType
eab_contact_compare_telephone (EContact *contact1, EContact *contact2)
{
	g_return_val_if_fail (contact1 && E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (contact2 && E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	/* Unimplemented */

	return EAB_CONTACT_MATCH_NOT_APPLICABLE;
}

EABContactMatchType
eab_contact_compare (EContact *contact1, EContact *contact2)
{
	EABContactMatchType result;

	g_return_val_if_fail (contact1 && E_IS_CONTACT (contact1), EAB_CONTACT_MATCH_NOT_APPLICABLE);
	g_return_val_if_fail (contact2 && E_IS_CONTACT (contact2), EAB_CONTACT_MATCH_NOT_APPLICABLE);

	result = EAB_CONTACT_MATCH_NONE;
	result = combine_comparisons (result, eab_contact_compare_name      (contact1, contact2));
	result = combine_comparisons (result, eab_contact_compare_nickname  (contact1, contact2));
	result = combine_comparisons (result, eab_contact_compare_email     (contact1, contact2));
	result = combine_comparisons (result, eab_contact_compare_address   (contact1, contact2));
	result = combine_comparisons (result, eab_contact_compare_telephone (contact1, contact2));
	result = combine_comparisons (result, eab_contact_compare_file_as   (contact1, contact2));

	return result;
}

typedef struct _MatchSearchInfo MatchSearchInfo;
struct _MatchSearchInfo {
	EContact *contact;
	GList *avoid;
	EABContactMatchQueryCallback cb;
	gpointer closure;
};

static void
match_search_info_free (MatchSearchInfo *info)
{
	if (info) {
		g_object_unref (info->contact);

		/* This should already have been deallocated, but just in case... */
		if (info->avoid) {
			g_list_foreach (info->avoid, (GFunc) g_object_unref, NULL);
			g_list_free (info->avoid);
			info->avoid = NULL;
		}

		g_free (info);
	}
}

static void
query_cb (EBook *book, EBookStatus status, GList *contacts, gpointer closure)
{
	/* XXX we need to free contacts */
	MatchSearchInfo *info = (MatchSearchInfo *) closure;
	EABContactMatchType best_match = EAB_CONTACT_MATCH_NONE;
	EContact *best_contact = NULL;
	GList *remaining_contacts = NULL;
	const GList *i;

	if (status != E_BOOK_ERROR_OK) {
		info->cb (info->contact, NULL, EAB_CONTACT_MATCH_NONE, info->closure);
		match_search_info_free (info);
		return;
	}

	/* remove the contacts we're to avoid from the list, if they're present */
	for (i = contacts; i != NULL; i = g_list_next (i)) {
		EContact *this_contact = E_CONTACT (i->data);
		const gchar *this_uid;
		GList *iterator;
		gboolean avoid = FALSE;

		this_uid = e_contact_get_const (this_contact, E_CONTACT_UID);
		if (!this_uid)
			continue;

		for (iterator = info->avoid; iterator; iterator = iterator->next) {
			const gchar *avoid_uid;

			avoid_uid = e_contact_get_const (iterator->data, E_CONTACT_UID);
			if (!avoid_uid)
				continue;

			if (!strcmp (avoid_uid, this_uid)) {
				avoid = TRUE;
				break;
			}
		}
		if (!avoid)
			remaining_contacts = g_list_prepend (remaining_contacts, this_contact);
	}

	remaining_contacts = g_list_reverse (remaining_contacts);

	for (i = remaining_contacts; i != NULL; i = g_list_next (i)) {
		EContact *this_contact = E_CONTACT (i->data);
		EABContactMatchType this_match = eab_contact_compare (info->contact, this_contact);
		if ((gint)this_match > (gint)best_match) {
			best_match = this_match;
			best_contact  = this_contact;
		}
	}

	g_list_free (remaining_contacts);

	info->cb (info->contact, best_contact, best_match, info->closure);
	match_search_info_free (info);
}

#define MAX_QUERY_PARTS 10
static void
use_common_book_cb (EBook *book, gpointer closure)
{
	MatchSearchInfo *info = (MatchSearchInfo *) closure;
	EContact *contact = info->contact;
	EContactName *contact_name;
	GList *contact_email;
	gchar *query_parts[MAX_QUERY_PARTS];
	gint p=0;
	gchar *contact_file_as, *qj;
	EBookQuery *query = NULL;
	int i;

	if (book == NULL) {
		info->cb (info->contact, NULL, EAB_CONTACT_MATCH_NONE, info->closure);
		match_search_info_free (info);
		return;
	}

	contact_file_as = e_contact_get (contact, E_CONTACT_FILE_AS);
	if (contact_file_as) {
		query_parts [p++] = g_strdup_printf ("(contains \"file_as\" \"%s\")", contact_file_as);
		g_free (contact_file_as);
	}

	contact_name = e_contact_get (contact, E_CONTACT_NAME);
	if (contact_name) {
		if (contact_name->given && *contact_name->given)
			query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", contact_name->given);

		if (contact_name->additional && *contact_name->additional)
			query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", contact_name->additional);

		if (contact_name->family && *contact_name->family)
			query_parts[p++] = g_strdup_printf ("(contains \"full_name\" \"%s\")", contact_name->family);

		e_contact_name_free (contact_name);
	}

	contact_email = e_contact_get (contact, E_CONTACT_EMAIL);
	if (contact_email) {
		GList *iter;
		for (iter = contact_email; iter && p < MAX_QUERY_PARTS; iter = iter->next) {
			gchar *addr = g_strdup (iter->data);
			if (addr && *addr) {
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
		}
	}
	g_list_foreach (contact_email, (GFunc)g_free, NULL);
	g_list_free (contact_email);
	
	
	/* Build up our full query from the parts. */
	query_parts[p] = NULL;
	qj = g_strjoinv (" ", query_parts);
	for(i = 0; query_parts[i] != NULL; i++)
		g_free(query_parts[i]);
	if (p > 1) {
		char *s;
		s = g_strdup_printf ("(or %s)", qj);
		query = e_book_query_from_string (s);
		g_free (s);
	}
	else if (p == 1) {
		query = e_book_query_from_string (qj);
	}
	else {
		query = NULL;
	}

	if (query)
		e_book_async_get_contacts (book, query, query_cb, info);
	else
		query_cb (book, E_BOOK_ERROR_OK, NULL, info);

	g_free (qj);
	if (query)
		e_book_query_unref (query);
}

void
eab_contact_locate_match (EContact *contact, EABContactMatchQueryCallback cb, gpointer closure)
{
	MatchSearchInfo *info;

	g_return_if_fail (contact && E_IS_CONTACT (contact));
	g_return_if_fail (cb != NULL);

	info = g_new (MatchSearchInfo, 1);
	info->contact = contact;
	g_object_ref (contact);
	info->cb = cb;
	info->closure = closure;
	info->avoid = NULL;

	addressbook_load_default_book ((EBookCallback) use_common_book_cb, info);
}

/**
 * e_contact_locate_match_full:
 * @book: The book to look in.  If this is NULL, use the default
 * addressbook.
 * @contact: The contact to compare to.
 * @avoid: A list of contacts to not match.  These will not show up in the search.
 * @cb: The function to call.
 * @closure: The closure to add to the call.
 * 
 * Look for the best match and return it using the EABContactMatchQueryCallback.
 **/
void
eab_contact_locate_match_full (EBook *book, EContact *contact, GList *avoid, EABContactMatchQueryCallback cb, gpointer closure)
{
	MatchSearchInfo *info;

	g_return_if_fail (contact && E_IS_CONTACT (contact));
	g_return_if_fail (cb != NULL);

	info = g_new (MatchSearchInfo, 1);
	info->contact = contact;
	g_object_ref (contact);
	info->cb = cb;
	info->closure = closure;
	info->avoid = g_list_copy (avoid);
	g_list_foreach (info->avoid, (GFunc) g_object_ref, NULL);

	if (book)
		use_common_book_cb (book, info);
	else
		addressbook_load_default_book ((EBookCallback) use_common_book_cb, info);
}


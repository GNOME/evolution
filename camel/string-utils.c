/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* string-util : utilities for gchar* strings  */

/* 
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
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
#include "string-utils.h"
#include "string.h"

gboolean
string_equal_for_glist (gconstpointer v, gconstpointer v2)
{
  return (!strcmp ( ((const gchar *)v), ((const gchar*)v2))) == 0;
}

/* utility func : frees a gchar element in a GList */
static void 
__string_list_free_string (gpointer data, gpointer user_data)
{
	gchar *string = (gchar *)data;
	g_free (string);
}

void 
string_list_free (GList *string_list)
{
	if (string_list == NULL) return; 

	g_list_foreach (string_list, __string_list_free_string, NULL);
	g_list_free (string_list);
}

GList *
string_split (const gchar *string, char sep, const gchar *trim_chars, StringTrimOption trim_options)
{
	GList *result = NULL;
	gint first, last, pos;
	gchar *new_string;

	g_assert (string);
	
	first = 0;
	last = strlen(string) - 1;
	
	/* strip leading and trailing separators */
	while ( (first<=last) && (string[first]==sep) )
		first++;
	while ( (first<=last) && (string[last]==sep) )
		last--;

	
	while (first<=last)  {
		pos = first;
		/* find next separator */
		while ((pos<=last) && (string[pos]!=sep)) pos++;
		if (first != pos) {
			new_string = g_strndup (string+first, pos-first);
			/* could do trimming in line to speed up this code */
			if (trim_chars) string_trim (new_string, trim_chars, trim_options);
			result = g_list_append (result, new_string);
		}	
		first = pos + 1;
	}

	return result;
}

void 
string_trim (gchar *string, const gchar *trim_chars, StringTrimOption options)
{
	gint first_ok;
	gint last_ok;
	guint length;

	g_return_if_fail (string);
	length = strlen (string);
	if (length==0)
		return;
	
	first_ok = 0;
	last_ok = length - 1;

	if (options & STRING_TRIM_STRIP_LEADING)
		while  ( (first_ok <= last_ok) && (strchr (trim_chars, string[first_ok])!=NULL) )
			first_ok++;
	
	if (options & STRING_TRIM_STRIP_TRAILING)
		while  ( (first_ok <= last_ok) && (strchr (trim_chars, string[last_ok])!=NULL) )
			last_ok--;
	
	if (first_ok > 0)
		memmove (string, string+first_ok, last_ok - first_ok + 1);
	string[last_ok - first_ok +1] = '\0';
	
}


/**
 * remove_suffix: remove a suffix from a string
 * @s: the string to remove the suffix from. 
 * @suffix: the suffix to remove
 * @suffix_found : suffix found flag
 *
 * Remove a suffix from a string. If the 
 * string ends with the full suffix, a copy 
 * of the string without the suffix is returned and
 * @suffix_found is set to %TRUE. 
 * Otherwise, NULL is returned and
 * @suffix_found is set to %FALSE. 
 * 
 * Return value: an allocated copy of the string without the suffix or NULL if the suffix was not found.
 **/
gchar *
string_prefix (const gchar *s, const gchar *suffix, gboolean *suffix_found)
{
	guint s_len, suf_len;
	guint suffix_pos;
	char *result_string;

	g_assert (s);
	g_assert (suffix);
	g_assert (suffix_found);

	s_len = strlen (s);
	suf_len = strlen (suffix);

	/* if the string is shorter than the suffix, do nothing */
	if (s_len < suf_len) {
		*suffix_found = FALSE;
		return NULL;
	}
	
	/* theoretical position of the prefix */
	suffix_pos = s_len - suf_len;

	/* compare the right hand side of the string with the suffix */
	if (!strncmp (s+suffix_pos, suffix, suf_len)) {

		/* if the suffix matches, check that there are 
		   characters before */
		if (suffix_pos == 0) {
			result_string = NULL;
			*suffix_found = TRUE;
		} else { 
			result_string = g_strndup (s, suffix_pos);
			*suffix_found = TRUE;
		}

	} else { 
		result_string = NULL;
		*suffix_found = FALSE;
	}

	return result_string;
}

gchar *
strstrcase (const gchar *haystack, const gchar *needle)
{
	/* find the needle in the haystack neglecting case */
	gchar *ptr;
	guint len;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	len = strlen (needle);
	if (len > strlen (haystack))
		return NULL;

	for (ptr = (gchar *) haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!g_strncasecmp (ptr, needle, len))
			return ptr;

	return NULL;
}


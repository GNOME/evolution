/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gstring-util : utilities for gstring object  */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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
#include "gstring-util.h"
#include <string.h>

/**
 * g_string_equals : test if two string are equal
 *
 * @string1 : first string
 * @string2 : second string
 *
 * @Return Value : true if the strings equal, false otherwise
 **/
gboolean
g_string_equals (GString *string1, GString *string2)
{
	g_assert (string1);
	g_assert (string2);
	return !strcmp (string1->str, string2->str);
}




/**
 * g_string_clone : clone a GString
 *
 * @string : the string to clone
 *
 * @Return Value : the clone ...
 **/
GString *
g_string_clone (GString *string)
{
	return g_string_new (string->str);
}

/**
 * g_string_append_g_string : append a GString to another  GString
 *
 * @dest_string : string which will be appended
 * @other_string : string to append
 *
 **/
void 
g_string_append_g_string(GString *dest_string, GString *other_string)
{
	g_assert(other_string);
	g_assert(dest_string);

	if (other_string->len)
		g_string_append(dest_string, other_string->str);
}

/**
 * g_string_equal_for_hash: test equality of two GStrings for hash tables
 * @v: string 1
 * @v2: string 2
 * 
 * 
 * 
 * Return value: 
 **/
gint
g_string_equal_for_hash (gconstpointer v, gconstpointer v2)
{
  return strcmp ( ((const GString*)v)->str, ((const GString*)v2)->str) == 0;
}

gint
g_string_equal_for_glist (gconstpointer v, gconstpointer v2)
{
  return !strcmp ( ((const GString*)v)->str, ((const GString*)v2)->str) == 0;
}


/**
 * g_string_hash: computes a hash value for a Gstring
 * @v: Gstring object
 * 
 * 
 * 
 * Return value: 
 **/
guint 
g_string_hash (gconstpointer v)
{
	return g_str_hash(((const GString*)v)->str);
}




/* utility func : frees a GString element in a GList */
static void 
__g_string_list_free_string (gpointer data, gpointer user_data)
{
	GString *string = (GString *)data;
	g_string_free(string, TRUE);
}


void 
g_string_list_free (GList *string_list)
{
	g_list_foreach(string_list, __g_string_list_free_string, NULL);
	g_list_free(string_list);
}






GList *
g_string_split (GString *string, char sep, gchar *trim_chars, GStringTrimOption trim_options)
{
	GList *result = NULL;
	gint first, last, pos;
	gchar *str;
	gchar *new_str;
	GString *new_gstring;

	g_assert (string);
	str = string->str;
	if (!str) return NULL;

	first = 0;
	last = strlen(str) - 1;
	
	/* strip leading and trailing separators */
	while ( (first<=last) && (str[first]==sep) )
		first++;
	while ( (first<=last) && (str[last]==sep) )
		last--;

	
	while (first<=last)  {
		pos = first;
		/* find next separator */
		while ((pos<=last) && (str[pos]!=sep)) pos++;
		if (first != pos) {
			new_str = g_strndup (str+first, pos-first);
			new_gstring = g_string_new (new_str);
			g_free (new_str);
			/* could do trimming in line to speed up this code */
			if (trim_chars) g_string_trim (new_gstring, trim_chars, trim_options);
			result = g_list_append (result, new_gstring);
		}	
		first = pos + 1;
	}

	return result;
}


void 
g_string_trim (GString *string, gchar *chars, GStringTrimOption options)
{
	gint first_ok;
	gint last_ok;
	guint length;
	gchar *str;

	if ((!string) || (!string->str))
		return; 
	str = string->str;
	length = strlen (str);
	if (!length)
		return;

	first_ok = 0;
	last_ok = length - 1;
	
	if (options & GSTRING_TRIM_STRIP_LEADING)
		while  ( (first_ok <= last_ok) && (strchr (chars, str[first_ok])) )
			first_ok++;

	if (options & GSTRING_TRIM_STRIP_TRAILING)
		while  ( (first_ok <= last_ok) && (strchr (chars, str[last_ok])) )
			last_ok++;

	if (first_ok > 0)
		g_string_erase (string, 0, first_ok);

	if (last_ok < length-1)
		g_string_truncate (string, last_ok - first_ok +1);
	
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* string-util : utilities for gchar* strings  */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-log.h"
#include "string.h"



gboolean
string_equal_for_glist (gconstpointer v, gconstpointer v2)
{
  return (!strcmp ( ((const gchar *)v), ((const gchar*)v2))) == 0;
}

/**
 * string_dichotomy:
 * @sep : separator
 * @prefix: pointer to be field by the prefix object
 *   the prefix is not returned when the given pointer is NULL
 * @suffix: pointer to be field by the suffix object
 *   the suffix is not returned when the given pointer is NULL
 *
 * Return the strings before and/or after 
 * the last occurence of the specified separator
 *
 * This routine returns the string before and/or after
 * a character given as an argument. 
 * if the separator is the last character, prefix and/or
 * suffix is set to NULL and result is set to 'l'
 * if the separator is not in the list, prefix and/or
 * suffix is set to NULL and result is set to 'n'
 * When the operation succedeed, the return value is 'o'
 *
 * @Return Value : result of the operation ('o', 'l' or 'n')
 *
 **/
gchar
string_dichotomy (const gchar *string, gchar sep, gchar **prefix, gchar **suffix,
		    StringDichotomyOption options)
{
	gint sep_pos, first, last, len;
	
	g_assert (string);
	CAMEL_LOG_FULL_DEBUG (\
		  "string_dichotomy:: string=\"%s\"\n\tseparator=\"%c\" \n\tprefix=%p \n\tsuffix=%p \n\toptions=%ld\n",\
		  string, sep, prefix, suffix, options);
	len = strlen (string);
	if (!len) {
		if (prefix)
			*prefix=NULL;
		if (suffix)
			*suffix=NULL;
		CAMEL_LOG_FULL_DEBUG ("string_dichotomy:: input string is empty\n");
		return 'n';
	}
	first = 0;
	last = len-1;
	
	if ( (options & STRING_DICHOTOMY_STRIP_LEADING ) && (string[first] == sep) )
	    do {first++;} while ( (first<len) && (string[first] == sep) );
	
	if (options & STRING_DICHOTOMY_STRIP_TRAILING )
		while ((string[last] == sep) && (last>first))
			last--;
	
	if (first==last) {
		if (prefix) *prefix=NULL;
		if (suffix) *suffix=NULL;
		CAMEL_LOG_FULL_DEBUG ("string_dichotomy: after stripping, string is empty\n");
		return 'n';
	}
	
	if (options & STRING_DICHOTOMY_RIGHT_DIR) {
		sep_pos = last;
		while ((sep_pos>=first) && (string[sep_pos]!=sep)) {
			sep_pos--;
		}
	} else {
		sep_pos = first;
		while ((sep_pos<=last) && (string[sep_pos]!=sep)) {
			sep_pos++;
		}
		
	}
	
	if ( (sep_pos<first) || (sep_pos>last) ) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator not found\n");
			return 'n';
		}
	
	/* if we have stripped trailing separators, we should */
	/* never enter here */
	if (sep_pos==last) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator is last character\n");
			return 'l';
		}
	/* if we have stripped leading separators, we should */
	/* never enter here */
	if (sep_pos==first)
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator is first character\n");
			return 'l';
		}
	CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator found at :%d\n", sep_pos);
	if (prefix) { /* return the prefix */
		*prefix = g_strndup (string+first,sep_pos-first);
		CAMEL_LOG_FULL_DEBUG ( "string_dichotomy:: prefix:\"%s\"\n", *prefix);
	}
	if (suffix) { /* return the suffix */
		*suffix = g_strndup (string+sep_pos+1, last-sep_pos);
		CAMEL_LOG_FULL_DEBUG ( "string_dichotomy:: suffix:\"%s\"\n", *suffix);
	}
	 
	return 'o';
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

	
	CAMEL_LOG_FULL_DEBUG ("string_split:: trim options: %d\n", trim_options);

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

	CAMEL_LOG_FULL_DEBUG ("string-utils:: Entering string_trim::\n");
	CAMEL_LOG_FULL_DEBUG ("string_trim:: trim_chars:\"%s\"", trim_chars);
	CAMEL_LOG_FULL_DEBUG ("string_trim:: trim_options:%d\n", options);

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
	CAMEL_LOG_FULL_DEBUG ("string_trim::\n\t\"%s\":first ok:%d last_ok:%d\n",
		   string, first_ok, last_ok);
	
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

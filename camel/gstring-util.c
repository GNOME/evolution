/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gstring-util : utilities for gstring object  */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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
#include "camel-log.h"


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
	g_assert(string1);
	g_assert(string2);
	return !strcmp(string1->str, string2->str);
}




/**
 * g_string_clone : clone a GString
 *
 * @string : the string to clone
 *
 * @Return Value : the clone ...
 **/
GString *
g_string_clone(GString *string)
{
	return g_string_new( g_strdup(string->str) );
}




/**
 * g_string_dichotomy:
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
g_string_dichotomy (GString *string, gchar sep, GString **prefix, GString **suffix,
		    GStringDichotomyOption options)
{
	gchar *str, *tmp;
	gint pos, len, first;
	
	CAMEL_LOG_FULL_DEBUG (\
		  "Entering string_dichotomy: \n\tseparator=%c \n\tprefix=%p \n\tsuffix=%p \n\toptions=%ld\n",\
		  sep, prefix, suffix, options);
	g_assert( tmp=string->str );
	len = strlen(tmp);
	if (!len) {
		if (prefix)
			*prefix=NULL;
		if (suffix)
			*suffix=NULL;
		CAMEL_LOG_FULL_DEBUG ("string_dichotomy: string is empty\n");
		return 'n';
	}
	first = 0;
	
	if ( (options & GSTRING_DICHOTOMY_STRIP_LEADING ) && (tmp[first] == sep) )
	    do {first++;} while ( (first<len) && (tmp[first] == sep) );
	
	if (options & GSTRING_DICHOTOMY_STRIP_TRAILING )
		while (tmp[len-1] == sep)
			len--;
	
	if (first==len) {
		if (prefix) *prefix=NULL;
		if (suffix) *suffix=NULL;
		CAMEL_LOG_FULL_DEBUG ("string_dichotomy: after stripping, string is empty\n");
		return 'n';
	}
	
	if (options & GSTRING_DICHOTOMY_RIGHT_DIR) {
		pos = len;
		
		do {
			pos--;
		} while ((pos>=first) && (tmp[pos]!=sep));
	} else {
		pos = first;
		do {
			pos++;
		} while ((pos<len) && (tmp[pos]!=sep));
		
	}
	
	if ( (pos<first) || (pos>=len) ) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator not found\n");
			return 'n';
		}
	
	/* if we have stripped trailing separators, we should */
	/* never enter here */
	if (pos==len-1) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator is last character\n");
			return 'l';
		}
	/* if we have stripped leading separators, we should */
	/* never enter here */
	if (pos==first)
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG_FULL_DEBUG ("string_dichotomy: separator is first character\n");
			return 'l';
		}
	
	if (prefix) /* return the prefix */
	{
		str = g_strndup(tmp,pos);
		*prefix = g_string_new(str);
		g_free(str);
	}
	if (suffix) /* return the suffix */
		{
			str = g_strdup(tmp+pos+1);
			*suffix = g_string_new(str);
			g_free(str);
	}
	
	return 'o';
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
	g_assert(other_string->str);

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
g_string_equal_for_hash (gconstpointer v, gconstpointer v2)
{
  return strcmp ( ((const GString*)v)->str, ((const GString*)v2)->str) == 0;
}

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

	
	CAMEL_LOG_FULL_DEBUG ("g_string_split:: trim options: %d\n", trim_options);

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

	CAMEL_LOG_FULL_DEBUG ("**\nentering g_string_trim::\n");

	if ((!string) || (!string->str))
		return; 
	str = string->str;
	length = strlen (str);
	if (!length)
		return;

	first_ok = 0;
	last_ok = length - 1;
	
	CAMEL_LOG_FULL_DEBUG ("g_string_trim:: trim_options:%d\n", options);
	if (options & GSTRING_TRIM_STRIP_LEADING)
		while  ( (first_ok <= last_ok) && (strchr (chars, str[first_ok])) )
			first_ok++;

	if (options & GSTRING_TRIM_STRIP_TRAILING)
		while  ( (first_ok <= last_ok) && (strchr (chars, str[last_ok])) )
			last_ok++;
	CAMEL_LOG_FULL_DEBUG ("g_string_trim::\n\t\"%s\":first ok:%d last_ok:%d\n",
		   string->str, first_ok, last_ok);

	if (first_ok > 0)
		g_string_erase (string, 0, first_ok);

	if (last_ok < length-1)
		g_string_truncate (string, last_ok - first_ok +1);
	
}

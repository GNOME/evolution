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
g_string_equals(GString *string1, GString *string2)
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
 * right_dichotomy : return the strings before and/or after 
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
 * @sep : separator
 * @prefix: pointer to be field by the prefix object
 *   the prefix is not returned when the given pointer is NULL
 * @suffix: pointer to be field by the suffix object
 *   the suffix is not returned when the given pointer is NULL
 *
 * @Return Value : result of the operation ('o', 'l' or 'n')
 *
 **/
gchar
g_string_right_dichotomy( GString *string, gchar sep, GString **prefix, GString **suffix, DichotomyOption options)
{
	gchar *str, *tmp;
	gint pos, len, first;
	
	CAMEL_LOG(FULL_DEBUG,\
		  "Entering rightDichotomy: \n\tseparator=%c \n\tprefix=%p \n\tsuffix=%p \n\toptions=%ld\n",\
		  sep, prefix, suffix, options);
	g_assert( tmp=string->str );
	len = strlen(tmp);
	if (!len) {
		if (prefix) *prefix=NULL;
		if (suffix) *suffix=NULL;
		CAMEL_LOG(FULL_DEBUG,"rightDichotomy: string is empty\n");
		return 'n';
	}
	first = 0;
	
	if ( (options & STRIP_LEADING ) && (tmp[first] == sep) )
	    do {first++;} while ( (first<len) && (tmp[first] == sep) );
	
	if (options & STRIP_TRAILING )
		while (tmp[len-1] == sep)
			len--;
	
	if (first==len) {
		if (prefix) *prefix=NULL;
		if (suffix) *suffix=NULL;
		CAMEL_LOG(FULL_DEBUG,"rightDichotomy: after stripping, string is empty\n");
		return 'n';
	}
	
	pos = len;
	
	do {
		pos--;
	} while ((pos>=first) && (tmp[pos]!=sep));
	
	
	if (pos<first) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG(FULL_DEBUG,"rightDichotomy: separator not found\n");
			return 'n';
		}
	
	/* if we have stripped trailongs separators, we should */
	/* never enter here */
	if (pos==len-1) 
		{
			if (suffix) *suffix=NULL;
			if (prefix) *prefix=NULL;
			CAMEL_LOG(FULL_DEBUG,"rightDichotomy: separator is last character\n");
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

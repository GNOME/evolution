/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-string-utils.h"


int
camel_strcase_equal (gconstpointer a, gconstpointer b)
{
	return (g_ascii_strcasecmp ((const char *) a, (const char *) b) == 0);
}

guint
camel_strcase_hash (gconstpointer v)
{
	const char *p = (char *) v;
	guint h = 0, g;
	
	for ( ; *p != '\0'; p++) {
		h = (h << 4) + g_ascii_toupper (*p);
		if ((g = h & 0xf0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	
	return h;
}


static void
free_string (gpointer string, gpointer user_data)
{
	g_free (string);
}

void
camel_string_list_free (GList *string_list)
{
	if (string_list == NULL)
		return; 
	
	g_list_foreach (string_list, free_string, NULL);
	g_list_free (string_list);
}

char *
camel_strstrcase (const char *haystack, const char *needle)
{
	/* find the needle in the haystack neglecting case */
	const char *ptr;
	guint len;
	
	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);
	
	len = strlen (needle);
	if (len > strlen (haystack))
		return NULL;
	
	if (len == 0)
		return (char *) haystack;
	
	for (ptr = haystack; *(ptr + len - 1) != '\0'; ptr++)
		if (!strncasecmp (ptr, needle, len))
			return (char *) ptr;
	
	return NULL;
}


const char *
camel_strdown (char *str)
{
	register char *s = str;
	
	while (*s) {
		if (*s >= 'A' && *s <= 'Z')
			*s += 0x20;
		s++;
	}
	
	return str;
}

/**
 * camel_tolower:
 * @c: 
 * 
 * ASCII to-lower function.
 * 
 * Return value: 
 **/
char camel_tolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c |= 0x20;

	return c;
}

/**
 * camel_toupper:
 * @c: 
 * 
 * ASCII to-upper function.
 * 
 * Return value: 
 **/
char camel_toupper(char c)
{
	if (c >= 'a' && c <= 'z')
		c &= ~0x20;

	return c;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* generic utilities for hash tables */

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

#include <ctype.h>
#include "glib.h"
#include "hash-table-utils.h"


/* 
 * free a (key/value) hash table pair.
 * to be called in a g_hash_table_foreach()
 * before g_hash_table_destroy().
 */
void
g_hash_table_generic_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}



/***/
/* use these two funcs for case insensitive hash table */

gint 
g_strcase_equal (gconstpointer a, gconstpointer b)
{
	return (g_strcasecmp ((gchar *)a, (gchar *)b) == 0);
}


/* modified g_str_hash from glib/gstring.c
   because it would have been too slow to
   us g_strdown() on the string */
/* a char* hash function from ASU */
guint
g_strcase_hash (gconstpointer v)
{
	const char *s = (char*)v;
	const char *p;
	guint h=0, g;
	
	for(p = s; *p != '\0'; p += 1) {
		h = ( h << 4 ) + toupper(*p);
		if ( ( g = h & 0xf0000000 ) ) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
  }

  return h /* % M */;
}



/***/

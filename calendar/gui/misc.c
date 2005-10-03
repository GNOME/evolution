/* Evolution calendar - Miscellaneous utility functions
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <libedataserver/e-url.h>
#include "misc.h"



/**
 * string_is_empty:
 * @value: A string.
 * 
 * Returns whether a string is NULL, the empty string, or completely made up of
 * whitespace characters.
 * 
 * Return value: TRUE if the string is empty, FALSE otherwise.
 **/
gboolean
string_is_empty (const char *value)
{
	const char *p;
	gboolean empty;

	empty = TRUE;

	if (value) {
		p = value;
		while (*p) {
			if (!isspace ((unsigned char) *p)) {
				empty = FALSE;
				break;
			}
			p++;
		}
	}
	return empty;

}

/**
 * get_uri_without_password
 */
char *
get_uri_without_password (const char *full_uri)
{
	EUri *uri;
	char *uristr;

	uri = e_uri_new (full_uri);
	if (!uri)
		return NULL;

	uristr = e_uri_to_string (uri, FALSE);
	e_uri_free (uri);

	return uristr;
 }

gint
get_position_in_array (GPtrArray *objects, gpointer item)
{
	gint i;

	for (i = 0; i < objects->len; i++) {
		if (g_ptr_array_index (objects, i) == item)
			return i;
	}

	return -1;
}

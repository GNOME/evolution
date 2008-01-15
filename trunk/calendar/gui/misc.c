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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <time.h>
#include <libedataserver/e-url.h>
#include <glib/gi18n.h>

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

char *
calculate_time (time_t start, time_t end)
{
	time_t difference = end - start;
	char *str;
	int   hours, minutes;
	char *times[4];
	char *joined;
	int   i;

        i = 0;
	if (difference >= 3600) {
		hours = difference / 3600;
		difference %= 3600;

		times[i++] = g_strdup_printf (ngettext("%d hour", "%d hours", hours), hours);
	}
	if (difference >= 60) {
		minutes = difference / 60;
		difference %= 60;

		times[i++] = g_strdup_printf (ngettext("%d minute", "%d minutes", minutes), minutes);
	}
	if (i == 0 || difference != 0) {
		/* TRANSLATORS: here, "second" is the time division (like "minute"), not the ordinal number (like "third") */
		times[i++] = g_strdup_printf (ngettext("%d second", "%d seconds", difference), (int)difference);
	}

	times[i] = NULL;
	joined = g_strjoinv (" ", times);
	str = g_strconcat ("(", joined, ")", NULL);
	while (i > 0)
		g_free (times[--i]);
	g_free (joined);

	return str;
}

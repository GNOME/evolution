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
#include <time.h>
#include <libedataserver/e-url.h>
#include "e-util/e-i18n.h"

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
	
	if (difference < 60) {/* Can't be zero */ 
		str = g_strdup_printf (_("(%d seconds)"), difference);
	} else if (difference > 60 && difference < 3600) { /* It will be x minutes y seconds*/
		int minutes, seconds;
		minutes = difference / 60;
		seconds = difference % 60;
		if (seconds)
			str = g_strdup_printf (_("(%d %s %d %s)"), minutes, ngettext(_("minute"), _("minutes"), minutes), seconds, ngettext(_("second"), _("seconds"), seconds));
		else 
			str = g_strdup_printf (_("(%d %s)"), minutes, ngettext(_("minute"), _("minutes"), minutes));
	} else {
		guint hours, minutes, seconds;
		char *s_hours = NULL, *s_minutes = NULL, *s_seconds = NULL;
		
		hours = difference / 3600;
		minutes = (difference % 3600)/60;
		seconds = difference % 60;


		if (seconds)
			s_seconds = g_strdup_printf (ngettext(_(" %u second"), _(" %u seconds"), seconds), seconds);	
		if (minutes)
			s_minutes = g_strdup_printf (ngettext(_(" %u minute"), _(" %u minutes"), minutes), minutes);
		if (hours)
			s_hours = g_strdup_printf (ngettext(_("%u hour"),_("%u hours"), hours), hours);

		if (s_minutes && s_seconds)
			str = g_strconcat ("(", s_hours, s_minutes, s_seconds, ")", NULL);
		else if (s_minutes)
			str = g_strconcat ("(", s_hours, s_minutes, ")", NULL);
		else if (s_seconds)
			str = g_strconcat ("(", s_hours, s_seconds, ")", NULL);
		else 
			str = g_strconcat ("(", s_hours, ")", NULL);

		g_free (s_hours);
		g_free (s_minutes);
		g_free (s_seconds);
	}

	return g_strchug(str);
}

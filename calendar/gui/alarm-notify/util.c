/* Evolution calendar - utility functions
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "config-data.h"
#include "util.h"

/* Converts a time_t to a string, relative to the specified timezone */
char *
timet_to_str_with_zone (time_t t, icaltimezone *zone)
{
	struct icaltimetype itt;
	struct tm tm;
	char buf[256];

	if (t == -1)
		return g_strdup (_("invalid time"));

	itt = icaltime_from_timet_with_zone (t, FALSE, zone);
	tm = icaltimetype_to_tm (&itt);

	e_time_format_date_and_time (&tm, config_data_get_24_hour_format (),
				     FALSE, FALSE, buf, sizeof (buf));
	return g_strdup (buf);
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

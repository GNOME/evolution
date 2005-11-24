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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-i18n.h>
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

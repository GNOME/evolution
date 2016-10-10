/*
 * Evolution calendar - utility functions
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "config-data.h"
#include "util.h"

gboolean
datetime_is_date_only (ECalComponent *comp,
		       gboolean datetime_check)
{
	ECalComponentDateTime dt;
	gboolean is_date_only;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);

	dt.value = NULL;

	if (datetime_check == DATETIME_CHECK_DTSTART)
		e_cal_component_get_dtstart (comp, &dt);
	else
		e_cal_component_get_dtend (comp, &dt);

	is_date_only = dt.value && dt.value->is_date;

	e_cal_component_free_datetime (&dt);

	return is_date_only;
}

/* Converts a time_t to a string, relative to the specified timezone */
gchar *
timet_to_str_with_zone (time_t t,
                        icaltimezone *zone,
			gboolean date_only)
{
	struct icaltimetype itt;
	struct tm tm;

	if (t == -1)
		return g_strdup (_("invalid time"));

	itt = icaltime_from_timet_with_zone (t, FALSE, zone);
	tm = icaltimetype_to_tm (&itt);

	return e_datetime_format_format_tm ("calendar", "table", date_only ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

gchar *
calculate_time (time_t start,
                time_t end)
{
	time_t difference = end - start;
	gchar *str;
	gint   hours, minutes;
	gchar *times[5];
	gchar *joined;
	gint   i;

	i = 0;
	if (difference >= 24 * 3600) {
		gint days;

		days = difference / (24 * 3600);
		difference %= (24 * 3600);

		times[i++] = g_strdup_printf (ngettext ("%d day", "%d days", days), days);
	}
	if (difference >= 3600) {
		hours = difference / 3600;
		difference %= 3600;

		times[i++] = g_strdup_printf (ngettext ("%d hour", "%d hours", hours), hours);
	}
	if (difference >= 60) {
		minutes = difference / 60;
		difference %= 60;

		times[i++] = g_strdup_printf (ngettext ("%d minute", "%d minutes", minutes), minutes);
	}
	if (i == 0 || difference != 0) {
		/* TRANSLATORS: here, "second" is the time division (like "minute"), not the ordinal number (like "third") */
		times[i++] = g_strdup_printf (ngettext ("%d second", "%d seconds", difference), (gint) difference);
	}

	times[i] = NULL;
	joined = g_strjoinv (" ", times);
	str = g_strconcat ("(", joined, ")", NULL);
	while (i > 0)
		g_free (times[--i]);
	g_free (joined);

	return str;
}

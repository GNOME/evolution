/*
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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <libedataserver/libedataserver.h>

#include "e-meeting-utils.h"

gint
e_meeting_time_compare_times (EMeetingTime *time1,
                              EMeetingTime *time2)
{
	gint day_comparison;

	day_comparison = g_date_compare (&time1->date, &time2->date);

	if (day_comparison != 0)
		return day_comparison;

	if (time1->hour < time2->hour)
		return -1;
	if (time1->hour > time2->hour)
		return 1;

	if (time1->minute < time2->minute)
		return -1;
	if (time1->minute > time2->minute)
		return 1;

	/* The start times are exactly the same. */
	return 0;
}

void
e_meeting_xfb_data_init (EMeetingXfbData *xfb)
{
	g_return_if_fail (xfb != NULL);

	xfb->summary = NULL;
	xfb->location = NULL;
}

void
e_meeting_xfb_data_set (EMeetingXfbData *xfb,
                        const gchar *summary,
                        const gchar *location)
{
	g_return_if_fail (xfb != NULL);

	e_meeting_xfb_data_clear (xfb);
	xfb->summary = g_strdup (summary);
	xfb->location = g_strdup (location);
}

void
e_meeting_xfb_data_clear (EMeetingXfbData *xfb)
{
	g_return_if_fail (xfb != NULL);

	/* clearing the contents of xfb,
	 * but not the xfb structure itself
	 */

	g_clear_pointer (&xfb->summary, g_free);
	g_clear_pointer (&xfb->location, g_free);
}

/* Creates an XFB string from a string property of a vfreebusy
 * ICalProperty. The iCal string we read may be base64 encoded, but
 * we get no reliable indication whether it really is. So we
 * try to base64-decode, and failing that, assume the string
 * is plain. The result is validated for UTF-8. We try to convert
 * to UTF-8 from locale if the input is no valid UTF-8, and failing
 * that, force the result into valid UTF-8. We also limit the
 * length of the resulting string, since it gets displayed as a
 * tooltip text in the meeting time selector.
 */
gchar *
e_meeting_xfb_utf8_string_new_from_ical (const gchar *icalstring,
                                         gsize max_len)
{
	gchar *tmp = NULL;
	gchar *utf8s = NULL;
	gsize in_len = 0;
	gsize out_len = 0;
	GError *tmp_err = NULL;

	g_return_val_if_fail (max_len > 4, NULL);

	if (icalstring == NULL)
		return NULL;

	/* iCal does not carry charset hints, so we
	 * try UTF-8 first, then conversion using
	 * system locale info.
	 */

	/* if we have valid UTF-8, we're done converting */
	if (g_utf8_validate (icalstring, -1, NULL))
		goto valid;

	/* no valid UTF-8, trying to convert to it
	 * according to system locale
	 */
	tmp = g_locale_to_utf8 (
		icalstring, -1, &in_len, &out_len, &tmp_err);

	if (tmp_err == NULL)
		goto valid;

	g_warning ("%s: %s", G_STRFUNC, tmp_err->message);
	g_error_free (tmp_err);
	g_free (tmp);

	/* still no success, forcing it into UTF-8, using
	 * replacement chars to replace invalid ones
	 */
	tmp = e_util_utf8_data_make_valid (
		icalstring, strlen (icalstring));
 valid:
	if (tmp == NULL)
		tmp = g_strdup (icalstring);

	/* now that we're (forcibly) valid UTF-8, we can
	 * limit the size of the UTF-8 string for display
	 */

	if (g_utf8_strlen (tmp, -1) > (glong) max_len) {
		/* insert NULL termination to where we want to
		 * clip, take care to hit UTF-8 character boundary
		 */
		utf8s = g_utf8_offset_to_pointer (tmp, (glong) max_len - 4);
		*utf8s = '\0';
		/* create shortened UTF-8 string */
		utf8s = g_strdup_printf ("%s ...", tmp);
		g_free (tmp);
	} else {
		utf8s = tmp;
	}

	return utf8s;
}

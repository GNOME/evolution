/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-meeting-utils.h"


/* this is a dupe of e_util_utf8_data_make_valid()
 * from libedataserver (which we do not link to
 * in this module presently). _g_utf8_make_valid()
 * really should be a public function...
 */
static gchar*
util_utf8_data_make_valid (const gchar *data,
                           gsize data_bytes)
{
	/* almost identical copy of glib's _g_utf8_make_valid() */
	GString *string;
	const gchar *remainder, *invalid;
	gint remaining_bytes, valid_bytes;

	g_return_val_if_fail (data != NULL, NULL);

	string = NULL;
	remainder = (gchar *) data,
	remaining_bytes = data_bytes;

	while (remaining_bytes != 0) {
		if (g_utf8_validate (remainder, remaining_bytes, &invalid))
			break;
		valid_bytes = invalid - remainder;

		if (string == NULL)
			string = g_string_sized_new (remaining_bytes);

		g_string_append_len (string, remainder, valid_bytes);
		/* append U+FFFD REPLACEMENT CHARACTER */
		g_string_append (string, "\357\277\275");

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (string == NULL)
		return g_strndup ((gchar *) data, data_bytes);

	g_string_append (string, remainder);

	g_warn_if_fail (g_utf8_validate (string->str, -1, NULL));

	return g_string_free (string, FALSE);
}

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

	if (xfb->summary != NULL) {
		g_free (xfb->summary);
		xfb->summary = NULL;
	}
	if (xfb->location != NULL) {
		g_free (xfb->location);
		xfb->location = NULL;
	}
}

/* Creates an XFB string from a string property of a vfreebusy
 * icalproperty. The ical string we read may be base64 encoded, but
 * we get no reliable indication whether it really is. So we
 * try to base64-decode, and failing that, assume the string
 * is plain. The result is validated for UTF-8. We try to convert
 * to UTF-8 from locale if the input is no valid UTF-8, and failing
 * that, force the result into valid UTF-8. We also limit the
 * length of the resulting string, since it gets displayed as a
 * tooltip text in the meeting time selector.
 */
gchar*
e_meeting_xfb_utf8_string_new_from_ical (const gchar *icalstring,
                                         gsize max_len)
{
	guchar *u_tmp = NULL;
	gchar *tmp = NULL;
	gchar *utf8s = NULL;
	gsize in_len = 0;
	gsize out_len = 0;
	GError *tmp_err = NULL;

	g_return_val_if_fail (max_len > 4, NULL);
	
	if (icalstring == NULL)
		return NULL;

	/* The icalstring may or may not be base64 encoded,
	 * which leaves us with guessing - we try decoding, if
	 * that fails we try plain. If icalstring is meant to
	 * be plain, but is valid base64 nonetheless, then we've
	 * lost (since we cannot reliably detect that case)
	 */

	u_tmp = g_base64_decode (icalstring, &out_len);
	if (u_tmp == NULL)
		u_tmp = (guchar *) g_strdup (icalstring);

	/* ical does not carry charset hints, so we
	 * try utf-8 first, then conversion to locale.
	 * If both fail we retrieve as many 
	 */

	/* if we have valid UTF-8, we're done converting */
	if (g_utf8_validate ((const gchar *) u_tmp, -1, NULL))
		goto valid;

	/* no valid UTF-8, trying to convert to it
	 * according to system locale
	 */
	tmp = g_locale_to_utf8 ((const gchar *) u_tmp,
	                        -1,
	                        &in_len,
	                        &out_len,
	                        &tmp_err);

	if (tmp_err == NULL)
		goto valid;

	g_warning ("%s() %s", __func__, tmp_err->message);
	g_error_free (tmp_err);

	/* still no success, forcing it into UTF-8, using
	 * replacement chars to replace invalid ones
	 */
	tmp = util_utf8_data_make_valid ((const gchar *) u_tmp,
	                                 strlen ((const gchar *) u_tmp));
 valid:
	if (tmp == NULL)
		tmp = (gchar *) u_tmp;
	else
		g_free (u_tmp);

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

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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "e-mail-stripsig-filter.h"

G_DEFINE_TYPE (EMailStripSigFilter, e_mail_stripsig_filter, CAMEL_TYPE_MIME_FILTER)

static void
strip_signature (CamelMimeFilter *filter,
                 const gchar *in,
                 gsize len,
                 gsize prespace,
                 gchar **out,
                 gsize *outlen,
                 gsize *outprespace,
                 gint flush)
{
	EMailStripSigFilter *stripsig = (EMailStripSigFilter *) filter;
	register const gchar *inptr = in;
	const gchar *inend = in + len;
	const gchar *start = NULL;

	if (stripsig->midline) {
		while (inptr < inend && *inptr != '\n' && (stripsig->text_plain_only ||
		       inend - inptr < 4 || g_ascii_strncasecmp (inptr, "<BR>", 4) != 0))
			inptr++;

		if (!stripsig->text_plain_only && inend - inptr >= 4 && g_ascii_strncasecmp (inptr, "<BR>", 4) == 0) {
			stripsig->midline = FALSE;
			inptr += 4;
		} else if (inptr < inend) {
			stripsig->midline = FALSE;
			inptr++;
		}
	}

	while (inptr < inend) {
		if ((inend - inptr) >= 4 && !strncmp (inptr, "-- \n", 4)) {
			start = inptr;
			inptr += 4;
		} else if (!stripsig->text_plain_only &&
				(inend - inptr) >= 7 &&
				!g_ascii_strncasecmp (inptr, "-- <BR>", 7)) {
			start = inptr;
			inptr += 7;
		} else {
			while (inptr < inend && *inptr != '\n' && (stripsig->text_plain_only ||
			       inend - inptr < 4 || g_ascii_strncasecmp (inptr, "<BR>", 4) != 0))
				inptr++;

			if (inptr == inend) {
				stripsig->midline = TRUE;
				break;
			}

			if (!stripsig->text_plain_only && inend - inptr >= 4 && g_ascii_strncasecmp (inptr, "<BR>", 4) == 0)
				inptr += 4;
			else
				inptr++;
		}
	}

	if (start != NULL) {
		inptr = start;
		stripsig->midline = FALSE;
	}

	if (!flush && inend > inptr)
		camel_mime_filter_backup (filter, inptr, inend - inptr);
	else if (!start)
		inptr = inend;

	*out = (gchar *)in;
	*outlen = inptr - in;
	*outprespace = prespace;
}

static void
filter_filter (CamelMimeFilter *filter,
               const gchar *in,
               gsize len,
               gsize prespace,
               gchar **out,
               gsize *outlen,
               gsize *outprespace)
{
	strip_signature (
		filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void
filter_complete (CamelMimeFilter *filter,
                 const gchar *in,
                 gsize len,
                 gsize prespace,
                 gchar **out,
                 gsize *outlen,
                 gsize *outprespace)
{
	strip_signature (
		filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

/* should this 'flush' outstanding state/data bytes? */
static void
filter_reset (CamelMimeFilter *filter)
{
	EMailStripSigFilter *stripsig = (EMailStripSigFilter *) filter;

	stripsig->midline = FALSE;
}

static void
e_mail_stripsig_filter_class_init (EMailStripSigFilterClass *class)
{
	CamelMimeFilterClass *mime_filter_class;

	mime_filter_class = CAMEL_MIME_FILTER_CLASS (class);
	mime_filter_class->filter = filter_filter;
	mime_filter_class->complete = filter_complete;
	mime_filter_class->reset = filter_reset;
}

static void
e_mail_stripsig_filter_init (EMailStripSigFilter *filter)
{
}

/**
 * e_mail_stripsig_filter_new:
 * @text_plain_only: Whether should look for a text/plain signature
 * delimiter "-- \n" only or also an HTML signature delimiter "-- <BR>".
 *
 * Creates a new stripsig filter.
 *
 * Returns a new stripsig filter.
 **/
CamelMimeFilter *
e_mail_stripsig_filter_new (gboolean text_plain_only)
{
	EMailStripSigFilter *filter = g_object_new (E_TYPE_MAIL_STRIPSIG_FILTER, NULL);

	filter->text_plain_only = text_plain_only;

	return CAMEL_MIME_FILTER (filter);
}

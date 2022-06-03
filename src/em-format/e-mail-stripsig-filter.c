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

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>

#include "e-mail-stripsig-filter.h"

G_DEFINE_TYPE (EMailStripSigFilter, e_mail_stripsig_filter, CAMEL_TYPE_MIME_FILTER)

static gboolean
is_html_newline_marker (const gchar *text,
			gint len,
			gint *advance_by_chars,
			gboolean *out_need_more_chars)
{
	const gchar *cases[] = {
		"<br>",
		"<div>",
		"<div ",
		"</div>",
		"<p>",
		"<p ",
		"</p>",
		"<pre>",
		"<pre ",
		"</pre>",
		NULL };
	gint ii;

	if (!text || !*text || !advance_by_chars || !out_need_more_chars)
		return FALSE;

	*advance_by_chars = 0;
	*out_need_more_chars = FALSE;

	for (ii = 0; cases[ii]; ii++) {
		gint caselen = strlen (cases[ii]);

		if (len >= caselen && g_ascii_strncasecmp (text, cases[ii], caselen) == 0) {
			if (cases[ii][caselen - 1] != '>') {
				/* Need to find the tag end, in a lazy way */
				while (caselen < len && text[caselen] && text[caselen] != '>')
					caselen++;

				/* Advance after the '>' */
				if (caselen < len && text[caselen])
					caselen++;

				if (caselen >= len) {
					*out_need_more_chars = TRUE;
					return FALSE;
				}
			}

			*advance_by_chars = caselen;
			return TRUE;
		}
	}

	return FALSE;
}

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
	gint advance_by_chars = 0;
	gboolean need_more_chars = FALSE;

	if (stripsig->midline) {
		while (inptr < inend && *inptr != '\n' && (stripsig->text_plain_only ||
		       !is_html_newline_marker (inptr, inend - inptr, &advance_by_chars, &need_more_chars))) {
			if (need_more_chars && !flush)
				goto read_more;
			inptr++;
		}

		if (!stripsig->text_plain_only && is_html_newline_marker (inptr, inend - inptr, &advance_by_chars, &need_more_chars)) {
			stripsig->midline = FALSE;
			inptr += advance_by_chars;
		} else if (inptr < inend) {
			stripsig->midline = FALSE;
			if (need_more_chars && !flush)
				goto read_more;
			inptr++;
		}
	}

	while (inptr && inptr < inend) {
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
			       !is_html_newline_marker (inptr, inend - inptr, &advance_by_chars, &need_more_chars))) {
				if (need_more_chars && !flush)
					goto read_more;
				inptr++;
			}

			if (inptr == inend) {
				stripsig->midline = TRUE;
				break;
			}

			if (!stripsig->text_plain_only && is_html_newline_marker (inptr, inend - inptr, &advance_by_chars, &need_more_chars))
				inptr += advance_by_chars;
			else if (need_more_chars && !flush)
				goto read_more;
			else
				inptr++;
		}
	}

	if (start != NULL) {
		inptr = start;
		stripsig->midline = FALSE;
	}

 read_more:
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
 * delimiter "-- \n" only or also an HTML signature delimiter "-- &lt;BR&gt;".
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

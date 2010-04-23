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

#include "em-stripsig-filter.h"

G_DEFINE_TYPE (EMStripSigFilter, em_stripsig_filter, CAMEL_TYPE_MIME_FILTER)

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
	EMStripSigFilter *stripsig = (EMStripSigFilter *) filter;
	register const gchar *inptr = in;
	const gchar *inend = in + len;
	const gchar *start = NULL;

	if (stripsig->midline) {
		while (inptr < inend && *inptr != '\n')
			inptr++;

		if (inptr < inend) {
			stripsig->midline = FALSE;
			inptr++;
		}
	}

	while (inptr < inend) {
		if ((inend - inptr) >= 4 && !strncmp (inptr, "-- \n", 4)) {
			start = inptr;
			inptr += 4;
		} else {
			while (inptr < inend && *inptr != '\n')
				inptr++;

			if (inptr == inend) {
				stripsig->midline = TRUE;
				break;
			}

			inptr++;
		}
	}

	if (start != NULL)
		inptr = start;

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
	EMStripSigFilter *stripsig = (EMStripSigFilter *) filter;

	stripsig->midline = FALSE;
}

static void
em_stripsig_filter_class_init (EMStripSigFilterClass *class)
{
	CamelMimeFilterClass *mime_filter_class;

	mime_filter_class = CAMEL_MIME_FILTER_CLASS (class);
	mime_filter_class->filter = filter_filter;
	mime_filter_class->complete = filter_complete;
	mime_filter_class->reset = filter_reset;
}

static void
em_stripsig_filter_init (EMStripSigFilter *filter)
{
}

/**
 * em_stripsig_filter_new:
 *
 * Creates a new stripsig filter.
 *
 * Returns a new stripsig filter.
 **/
CamelMimeFilter *
em_stripsig_filter_new (void)
{
	return g_object_new (EM_TYPE_STRIPSIG_FILTER, NULL);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian, Inc.
 *
 *  Authors: Dan Winship <danw@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *           Peter Williams <peterw@ximian.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ctype.h>
#include <string.h>

#include "camel-mime-filter-stripheader.h"

static void filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
		    char **out, size_t *outlen, size_t *outprespace);
static void complete (CamelMimeFilter *f, char *in, size_t len,
		      size_t prespace, char **out, size_t *outlen,
		      size_t *outprespace);
static void reset (CamelMimeFilter *f);


static void
camel_mime_filter_stripheader_init (CamelMimeFilterStripHeader *cmf)
{
	cmf->seen_eoh = FALSE;
	cmf->in_header = FALSE;
	cmf->header = NULL;
	cmf->header_len = 0;
}

static void
camel_mime_filter_stripheader_finalize (CamelMimeFilterStripHeader *cmf)
{
	g_free (cmf->header);
}

static void
camel_mime_filter_stripheader_class_init (CamelMimeFilterStripHeaderClass *klass)
{
	CamelMimeFilterClass *mime_filter_class =
		(CamelMimeFilterClass *) klass;
	
	mime_filter_class->filter = filter;
	mime_filter_class->complete = complete;
	mime_filter_class->reset = reset;
}

CamelType
camel_mime_filter_stripheader_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type(), "CamelMimeFilterStripHeader",
					    sizeof (CamelMimeFilterStripHeader),
					    sizeof (CamelMimeFilterStripHeaderClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_stripheader_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_stripheader_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_stripheader_finalize);
	}

	return type;
}

static void
filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterStripHeader *sh = (CamelMimeFilterStripHeader *)f;
	char *p, *q;
	int left;

	/* all done? */
	if (sh->seen_eoh) {
		*out = in;
		*outlen = len;
		*outprespace = prespace;
		return;
	}

	/* nope */

	camel_mime_filter_set_size (f, len, FALSE);

	p = in;
	q = f->outbuf;

	while (p < (in + len)) {
		if (*p == '\n') {
			left = in + len - p;

			/* Not enough left to do anything useful. */
			if (left < (sh->header_len + 3)) {
				camel_mime_filter_backup (f, p, left);
				break;
			}

			/* MIME-ese for 'end of headers'. Our work here is done, */
			if (!strncmp (p, "\n\n", 2)) {

				if (sh->in_header) {
				/* we've already got a \n */
					p++;
					left--;
				}

				sh->seen_eoh = TRUE;
				memcpy (q, p, left);
				q += left;
				break;
			}

			/* (Maybe) Grab this \n */
			if (!sh->in_header)
				*q++ = *p;
			p++;

			if (!strncmp (p, sh->header, sh->header_len) && p[sh->header_len] == ':')
				/* ok it seems we /are/ in the header */
				sh->in_header = TRUE;
			else if (!isspace ((int)(*p)))
				/* if we were in a header and we are on a space
				 * in_header will remain true...*/
				sh->in_header = FALSE;

		}

		/* ok then */
		if (!sh->in_header)
			*q++ = *p;
		p++;
	}

	*out = f->outbuf;
	*outlen = q - f->outbuf;
	*outprespace = f->outpre;
}

static void 
complete (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	  char **out, size_t *outlen, size_t *outprespace)
{
	if (len)
		filter (f, in, len, prespace, out, outlen, outprespace);
}

static void
reset (CamelMimeFilter *f)
{
	CamelMimeFilterStripHeader *sh = (CamelMimeFilterStripHeader *)f;

	sh->seen_eoh = FALSE;
	sh->in_header = FALSE;
}

CamelMimeFilter *
camel_mime_filter_stripheader_new (const gchar *header)
{
	CamelMimeFilterStripHeader *sh = CAMEL_MIME_FILTER_STRIPHEADER(camel_object_new (CAMEL_MIME_FILTER_STRIPHEADER_TYPE));

	sh->header = g_strdup (header);
	sh->header_len = strlen (header);

	return (CamelMimeFilter *)sh;
}

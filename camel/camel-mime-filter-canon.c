/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

/* canonicalisation filter, used for secure mime incoming and outgoing */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>

#include "camel-mime-filter-canon.h"

static void filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
		    char **out, size_t *outlen, size_t *outprespace);
static void complete (CamelMimeFilter *f, char *in, size_t len,
		      size_t prespace, char **out, size_t *outlen,
		      size_t *outprespace);
static void reset (CamelMimeFilter *f);


static void
camel_mime_filter_canon_class_init (CamelMimeFilterCanonClass *klass)
{
	CamelMimeFilterClass *mime_filter_class = (CamelMimeFilterClass *) klass;
	
	mime_filter_class->filter = filter;
	mime_filter_class->complete = complete;
	mime_filter_class->reset = reset;
}

CamelType
camel_mime_filter_canon_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type(), "CamelMimeFilterCanon",
					    sizeof (CamelMimeFilterCanon),
					    sizeof (CamelMimeFilterCanonClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_canon_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

static void
filter_run(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace, int last)
{
	register unsigned char *inptr, c;
	const unsigned char *inend, *start;
	char *starto;
	register char *o;
	int lf = 0;
	guint32 flags;

	flags = ((CamelMimeFilterCanon *)f)->flags;

	/* first, work out how much space we need */
	inptr = in;
	inend = in+len;
	while (inptr < inend)
		if (*inptr++ == '\n')
			lf++;

	/* worst case, extra 3 chars per line
	   "From \n" -> "=46rom \r\n"
	   We add 1 extra incase we're called from complete, when we didn't end in \n */

	camel_mime_filter_set_size(f, len+lf*3+4, FALSE);

	o = f->outbuf;
	inptr = in;
	start = inptr;
	starto = o;
	while (inptr < inend) {
		/* first, check start of line, we always start at the start of the line */
		c = *inptr;
		if (flags & CAMEL_MIME_FILTER_CANON_FROM && c == 'F') {
			inptr++;
			if (inptr < inend-4) {
				if (strncmp(inptr, "rom ", 4) == 0) {
					strcpy(o, "=46rom ");
					inptr+=4;
					o+= 7;
				} else
					*o++ = 'F';
			} else if (last)
				*o++ = 'F';
			else
				break;
		}

		/* now scan for end of line */
		while (inptr < inend) {
			c = *inptr++;
			if (c == '\n') {
				/* check to strip trailing space */
				if (flags & CAMEL_MIME_FILTER_CANON_STRIP) {
					while (o>starto && (o[-1] == ' ' || o[-1] == '\t' || o[-1]=='\r'))
						o--;
				}
				/* check end of line canonicalisation */
				if (o>starto) {
					if (flags & CAMEL_MIME_FILTER_CANON_CRLF) {
						if (o[-1] != '\r')
							*o++ = '\r';
					} else {
						if (o[-1] == '\r')
							o--;
					}
				} else if (flags & CAMEL_MIME_FILTER_CANON_CRLF) {
					/* empty line */
					*o++ = '\r';
				}
				
				*o++ = c;
				start = inptr;
				starto = o;
				break;
			} else
				*o++ = c;
		}
	}

	/* TODO: We should probably track if we end somewhere in the middle of a line,
	   otherwise we potentially backup a full line, which could be large */

	/* we got to the end of the data without finding anything, backup to start and re-process next time around */
	if (last) {
		*outlen = o - f->outbuf;
	} else {
		camel_mime_filter_backup(f, start, inend - start);
		*outlen = starto - f->outbuf;
	}

	*out = f->outbuf;
	*outprespace = f->outpre;
}

static void
filter(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	filter_run(f, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
complete(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	filter_run(f, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
reset (CamelMimeFilter *f)
{
	/* no-op */
}

CamelMimeFilter *
camel_mime_filter_canon_new(guint32 flags)
{
	CamelMimeFilterCanon *chomp = CAMEL_MIME_FILTER_CANON (camel_object_new (CAMEL_MIME_FILTER_CANON_TYPE));

	chomp->flags = flags;

	return (CamelMimeFilter *) chomp;
}

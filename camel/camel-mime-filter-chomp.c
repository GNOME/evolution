/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2002 Ximian, Inc.
 *
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>

#include "camel-mime-filter-chomp.h"

static void filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
		    char **out, size_t *outlen, size_t *outprespace);
static void complete (CamelMimeFilter *f, char *in, size_t len,
		      size_t prespace, char **out, size_t *outlen,
		      size_t *outprespace);
static void reset (CamelMimeFilter *f);


static void
camel_mime_filter_chomp_class_init (CamelMimeFilterChompClass *klass)
{
	CamelMimeFilterClass *mime_filter_class =
		(CamelMimeFilterClass *) klass;
	
	mime_filter_class->filter = filter;
	mime_filter_class->complete = complete;
	mime_filter_class->reset = reset;
}

CamelType
camel_mime_filter_chomp_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type(), "CamelMimeFilterChomp",
					    sizeof (CamelMimeFilterChomp),
					    sizeof (CamelMimeFilterChompClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_chomp_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

static void
filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	char **out, size_t *outlen, size_t *outprespace)
{
	register unsigned char *inptr, *s;
	const unsigned char *inend;
	register char *outptr;
	
	camel_mime_filter_set_size (f, len + prespace, FALSE);
	
	inend = in + len;
	inptr = in;
	
	outptr = f->outbuf;
	
	while (inptr < inend) {
		s = inptr;
		while (inptr < inend && isspace ((int) *inptr))
			inptr++;
		
		if (inptr < inend) {
			while (s < inptr)
				*outptr++ = (char) *s++;
			
			while (inptr < inend && !isspace ((int) *inptr))
				*outptr++ = (char) *inptr++;
		} else {
#if 0
			if ((inend - s) >= 2 && *s == '\r' && *(s + 1) == '\n')
				*outptr++ = *s++;
			if (*s == '\n')
				*outptr++ = *s++;
#endif		
			if (s < inend)
				camel_mime_filter_backup (f, s, inend - s);
			break;
		}
	}
	
	*out = f->outbuf;
	*outlen = outptr - f->outbuf;
	*outprespace = f->outpre;
}

static void 
complete (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	  char **out, size_t *outlen, size_t *outprespace)
{
	unsigned char *inptr;
	char *outptr;
	
	if (len)
		filter (f, in, len, prespace, out, outlen, outprespace);
	
	if (f->backlen) {
		inptr = (unsigned char *) f->backbuf;
		outptr = f->outbuf + *outlen;
		
		/* in the case of a canonical eoln */
		if (*inptr == '\r' && *(inptr + 1) == '\n') {
			*outptr++ = *inptr++;
			(*outlen)++;
		}
		
		if (*inptr == '\n') {
			*outptr++ = *inptr++;
			(*outlen)++;
		}
		
		/* to protect against further complete calls */
		camel_mime_filter_backup (f, "", 0);
	}
}

static void
reset (CamelMimeFilter *f)
{
	/* no-op */
}

CamelMimeFilter *
camel_mime_filter_chomp_new (void)
{
	CamelMimeFilterChomp *chomp = CAMEL_MIME_FILTER_CHOMP (camel_object_new (CAMEL_MIME_FILTER_CHOMP_TYPE));
	
	return (CamelMimeFilter *) chomp;
}

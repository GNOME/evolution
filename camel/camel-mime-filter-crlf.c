/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian, Inc.
 *
 *  Authors: Dan Winship <danw@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include "camel-mime-filter-crlf.h"

static void filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
		    char **out, size_t *outlen, size_t *outprespace);
static void complete (CamelMimeFilter *f, char *in, size_t len,
		      size_t prespace, char **out, size_t *outlen,
		      size_t *outprespace);
static void reset (CamelMimeFilter *f);


static void
camel_mime_filter_crlf_class_init (CamelMimeFilterCRLFClass *klass)
{
	CamelMimeFilterClass *mime_filter_class =
		(CamelMimeFilterClass *) klass;
	
	mime_filter_class->filter = filter;
	mime_filter_class->complete = complete;
	mime_filter_class->reset = reset;
}

CamelType
camel_mime_filter_crlf_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type(), "CamelMimeFilterCRLF",
					    sizeof (CamelMimeFilterCRLF),
					    sizeof (CamelMimeFilterCRLFClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_crlf_class_init,
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
	CamelMimeFilterCRLF *crlf = (CamelMimeFilterCRLF *)f;
	register const char *inptr;
	const char *inend;
	gboolean do_dots;
	char *outptr;
	
	do_dots = crlf->mode == CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS;
	
	inptr = in;
	inend = in + len;
	
	if (crlf->direction == CAMEL_MIME_FILTER_CRLF_ENCODE) {
		camel_mime_filter_set_size (f, 3 * len, FALSE);
		
		outptr = f->outbuf;
		while (inptr < inend) {
			if (*inptr == '\r') {
				crlf->saw_cr = TRUE;
			} else if (*inptr == '\n') {
				crlf->saw_lf = TRUE;
				if (!crlf->saw_cr)
					*outptr++ = '\r';
				crlf->saw_cr = FALSE;
			} else {
				if (do_dots && *inptr == '.' && crlf->saw_lf)
					*outptr++ = '.';
				
				crlf->saw_cr = FALSE;
				crlf->saw_lf = FALSE;
			}
			
			*outptr++ = *inptr++;
		}
	} else {
		camel_mime_filter_set_size (f, len, FALSE);
		
		outptr = f->outbuf;
		while (inptr < inend) {
			if (*inptr == '\r') {
				crlf->saw_cr = TRUE;
			} else {
				if (crlf->saw_cr) {
					crlf->saw_cr = FALSE;
					
					if (*inptr == '\n') {
						crlf->saw_lf = TRUE;
						*outptr++ = *inptr++;
						continue;
					} else
						*outptr++ = '\r';
				}
				
				*outptr++ = *inptr;
			}
			
			if (do_dots && *inptr == '.') {
				if (crlf->saw_lf) {
					crlf->saw_dot = TRUE;
					crlf->saw_lf = FALSE;
					inptr++;
				} else if (crlf->saw_dot) {
					crlf->saw_dot = FALSE;
				}
			}
			
			crlf->saw_lf = FALSE;
			
			inptr++;
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
	if (len)
		filter (f, in, len, prespace, out, outlen, outprespace);
}

static void
reset (CamelMimeFilter *f)
{
	CamelMimeFilterCRLF *crlf = (CamelMimeFilterCRLF *)f;
	
	crlf->saw_cr = FALSE;
	crlf->saw_lf = TRUE;
	crlf->saw_dot = FALSE;
}

CamelMimeFilter *
camel_mime_filter_crlf_new (CamelMimeFilterCRLFDirection direction, CamelMimeFilterCRLFMode mode)
{
	CamelMimeFilterCRLF *crlf = CAMEL_MIME_FILTER_CRLF(camel_object_new (CAMEL_MIME_FILTER_CRLF_TYPE));
	
	crlf->direction = direction;
	crlf->mode = mode;
	crlf->saw_cr = FALSE;
	crlf->saw_lf = TRUE;
	crlf->saw_dot = FALSE;
	
	return (CamelMimeFilter *)crlf;
}

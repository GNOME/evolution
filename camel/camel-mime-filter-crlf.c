/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code, Inc.
 *
 *  Authors: Dan Winship <danw@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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

GtkType
camel_mime_filter_crlf_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeFilterCRLF",
			sizeof (CamelMimeFilterCRLF),
			sizeof (CamelMimeFilterCRLFClass),
			(GtkClassInitFunc) camel_mime_filter_crlf_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL
		};
		
		type = gtk_type_unique (camel_mime_filter_get_type (),
					&type_info);
	}

	return type;
}

static void
filter (CamelMimeFilter *f, char *in, size_t len, size_t prespace,
	char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterCRLF *crlf = (CamelMimeFilterCRLF *)f;
	gboolean do_dots;
	char *p, *q;

	do_dots = crlf->mode == CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS;

	if (crlf->direction == CAMEL_MIME_FILTER_CRLF_ENCODE) {
		camel_mime_filter_set_size (f, 2 * len, FALSE);

		p = in;
		q = f->outbuf;
		while (p < in + len) {
			if (*p == '\n')
				*q++ = '\r';
			else
				if (do_dots && *(p - 1) == '\n' && *p == '.')
					*q++ = '.';
			*q++ = *p++;
		}
	} else {
		camel_mime_filter_set_size (f, len, FALSE);

		p = in;
		q = f->outbuf;
		while (p < in + len) {
			if (*p == '\r') {
				crlf->saw_cr = TRUE;
				p++;
			} else {
				if (crlf->saw_cr) {
					if (*p != '\n')
						*q++ = '\r';
					crlf->saw_cr = FALSE;
				}
				*q++ = *p++;
			}

			if (do_dots) {
				if (*p == '.' && *(p - 1) == '\n') {
					crlf->saw_dot = TRUE;
					p++;
				} else {
					if (crlf->saw_dot) {
						if (*p == '.')
							p++;
						crlf->saw_dot = FALSE;
					}
					*q++ = *p++;
				}
			}
		}
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
	CamelMimeFilterCRLF *crlf = (CamelMimeFilterCRLF *)f;

	crlf->saw_cr = FALSE;
}

CamelMimeFilter *
camel_mime_filter_crlf_new (CamelMimeFilterCRLFDirection direction, CamelMimeFilterCRLFMode mode)
{
	CamelMimeFilterCRLF *crlf = gtk_type_new (CAMEL_MIME_FILTER_CRLF_TYPE);

	crlf->direction = direction;
	crlf->mode = mode;
	crlf->saw_cr = FALSE;

	return (CamelMimeFilter *)crlf;
}

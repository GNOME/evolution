/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#include <string.h>
#include <errno.h>

#include <libedataserver/e-iconv.h>

#include "camel-mime-filter-charset.h"
#include "camel-charset-map.h"

#define d(x)

static void camel_mime_filter_charset_class_init (CamelMimeFilterCharsetClass *klass);
static void camel_mime_filter_charset_init       (CamelMimeFilterCharset *obj);
static void camel_mime_filter_charset_finalize   (CamelObject *o);

static CamelMimeFilterClass *camel_mime_filter_charset_parent;

CamelType
camel_mime_filter_charset_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (), "CamelMimeFilterCharset",
					    sizeof (CamelMimeFilterCharset),
					    sizeof (CamelMimeFilterCharsetClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_charset_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_charset_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_charset_finalize);
	}
	
	return type;
}

static void
camel_mime_filter_charset_finalize(CamelObject *o)
{
	CamelMimeFilterCharset *f = (CamelMimeFilterCharset *)o;

	g_free(f->from);
	g_free(f->to);
	if (f->ic != (iconv_t) -1) {
		e_iconv_close (f->ic);
		f->ic = (iconv_t) -1;
	}
}

static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterCharset *f = (CamelMimeFilterCharset *)mf;
	char buf[16];
	char *buffer;
	size_t outlen = 16;
	
	/* what happens with the output bytes if this resets the state? */
	if (f->ic != (iconv_t) -1) {
		buffer = buf;
		e_iconv (f->ic, NULL, 0, &buffer, &outlen);
	}
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterCharset *charset = (CamelMimeFilterCharset *)mf;
	size_t inleft, outleft, converted = 0;
	const char *inbuf;
	char *outbuf;
	
	if (charset->ic == (iconv_t) -1)
		goto noop;
	
	camel_mime_filter_set_size (mf, len * 5 + 16, FALSE);
	outbuf = mf->outbuf;
	outleft = mf->outsize;
	
	inbuf = in;
	inleft = len;
	
	if (inleft > 0) {
		do {
			converted = e_iconv (charset->ic, &inbuf, &inleft, &outbuf, &outleft);
			if (converted == (size_t) -1) {
				if (errno == E2BIG) {
					/*
					 * E2BIG   There is not sufficient room at *outbuf.
					 *
					 * We just need to grow our outbuffer and try again.
					 */
					
					converted = outbuf - mf->outbuf;
					camel_mime_filter_set_size (mf, inleft * 5 + mf->outsize + 16, TRUE);
					outbuf = mf->outbuf + converted;
					outleft = mf->outsize - converted;
				} else if (errno == EILSEQ) {
					/*
					 * EILSEQ An invalid multibyte sequence has been  encountered
					 *        in the input.
					 *
					 * What we do here is eat the invalid bytes in the sequence and continue
					 */
					
					inbuf++;
					inleft--;
				} else if (errno == EINVAL) {
					/*
					 * EINVAL  An  incomplete  multibyte sequence has been encoun­
					 *         tered in the input.
					 *
					 * We assume that this can only happen if we've run out of
					 * bytes for a multibyte sequence, if not we're in trouble.
					 */
					
					break;
				} else
					goto noop;
			}
		} while (((int) inleft) > 0);
	}
	
	/* flush the iconv conversion */
	e_iconv (charset->ic, NULL, NULL, &outbuf, &outleft);
	
	*out = mf->outbuf;
	*outlen = mf->outsize - outleft;
	*outprespace = mf->outpre;
	
	return;
	
 noop:
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterCharset *charset = (CamelMimeFilterCharset *)mf;
	size_t inleft, outleft, converted = 0;
	const char *inbuf;
	char *outbuf;
	
	if (charset->ic == (iconv_t) -1)
		goto noop;
	
	camel_mime_filter_set_size (mf, len * 5 + 16, FALSE);
	outbuf = mf->outbuf + converted;
	outleft = mf->outsize - converted;
	
	inbuf = in;
	inleft = len;
	
	do {
		converted = e_iconv (charset->ic, &inbuf, &inleft, &outbuf, &outleft);
		if (converted == (size_t) -1) {
			if (errno == E2BIG || errno == EINVAL)
				break;
			
			if (errno == EILSEQ) {
				/*
				 * EILSEQ An invalid multibyte sequence has been  encountered
				 *        in the input.
				 *
				 * What we do here is eat the invalid bytes in the sequence and continue
				 */
				
				inbuf++;
				inleft--;
			} else {
				/* unknown error condition */
				goto noop;
			}
		}
	} while (((int) inleft) > 0);
	
	if (((int) inleft) > 0) {
		/* We've either got an E2BIG or EINVAL. Save the
                   remainder of the buffer as we'll process this next
                   time through */
		camel_mime_filter_backup (mf, inbuf, inleft);
	}
	
	*out = mf->outbuf;
	*outlen = outbuf - mf->outbuf;
	*outprespace = mf->outpre;
	
	return;
	
 noop:
	
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
camel_mime_filter_charset_class_init (CamelMimeFilterCharsetClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_charset_parent = CAMEL_MIME_FILTER_CLASS (camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));

	filter_class->reset = reset;
	filter_class->filter = filter;
	filter_class->complete = complete;
}

static void
camel_mime_filter_charset_init (CamelMimeFilterCharset *obj)
{
	obj->ic = (iconv_t)-1;
}

/**
 * camel_mime_filter_charset_new:
 *
 * Create a new CamelMimeFilterCharset object.
 * 
 * Return value: A new CamelMimeFilterCharset widget.
 **/
CamelMimeFilterCharset *
camel_mime_filter_charset_new (void)
{
	return CAMEL_MIME_FILTER_CHARSET (camel_object_new (camel_mime_filter_charset_get_type ()));
}

CamelMimeFilterCharset *
camel_mime_filter_charset_new_convert (const char *from_charset, const char *to_charset)
{
	CamelMimeFilterCharset *new;
	
	new = CAMEL_MIME_FILTER_CHARSET (camel_object_new (camel_mime_filter_charset_get_type ()));
	
	new->ic = e_iconv_open (to_charset, from_charset);
	if (new->ic == (iconv_t) -1) {
		g_warning ("Cannot create charset conversion from %s to %s: %s",
			   from_charset ? from_charset : "(null)",
			   to_charset ? to_charset : "(null)",
			   g_strerror (errno));
		camel_object_unref (new);
		new = NULL;
	} else {
		new->from = g_strdup (from_charset);
		new->to = g_strdup (to_charset);
	}
	
	return new;
}

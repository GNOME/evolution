/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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


#include <unicode.h>

#include <string.h>
#include <errno.h>

#include "camel-mime-filter-charset.h"


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
	if (f->ic != (unicode_iconv_t)-1) {
		unicode_iconv_close(f->ic);
		f->ic = (unicode_iconv_t) -1;
	}
}

static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterCharset *f = (CamelMimeFilterCharset *)mf;
	char buf[16];
	char *buffer;
	int outlen = 16;

	/* what happens with the output bytes if this resets the state? */
	if (f->ic != (unicode_iconv_t) -1) {
		buffer = buf;
		unicode_iconv(f->ic, NULL, 0, &buffer, &outlen);
	}
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterCharset *f = (CamelMimeFilterCharset *)mf;
	int converted;
	char *inbuf, *outbuf;
	int inlen, outlen;

	if (f->ic == (unicode_iconv_t) -1) {
		goto donothing;
	}

	/* FIXME: there's probably a safer way to size this ...? */
	/* We could always resize if we run out of room in outbuf (but it'd be nice not
	   to have to) */
	camel_mime_filter_set_size(mf, len*5, FALSE);
	inbuf = in;
	inlen = len;
	outbuf = mf->outbuf;
	outlen = mf->outsize;
	if (inlen>0) {
		converted = unicode_iconv(f->ic, &inbuf, &inlen, &outbuf, &outlen);
		if (converted == -1) {
			if (errno != EINVAL) {
				g_warning("error occured converting: %s", strerror(errno));
				goto donothing;
			}
		}

		if (inlen>0) {
			g_warning("Output lost in character conversion, invalid sequence encountered?");
		}
	}

	/* this 'resets' the output stream, returning back to the initial
	   shift state for multishift charactersets */
	converted = unicode_iconv(f->ic, NULL, 0, &outbuf, &outlen);
	if (converted == -1) {
		g_warning("Conversion failed to complete: %s", strerror(errno));
	}

	*out = mf->outbuf;
	*outlenptr = mf->outsize - outlen;
	*outprespace = mf->outpre;
	return;

donothing:
	*out = in;
	*outlenptr = len;
	*outprespace = prespace;
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterCharset *f = (CamelMimeFilterCharset *)mf;
	int converted;
	char *inbuf, *outbuf;
	int inlen, outlen;

	if (f->ic == (unicode_iconv_t) -1) {
		goto donothing;
	}

	/* FIXME: there's probably a safer way to size this ...? */
	camel_mime_filter_set_size(mf, len*5, FALSE);
	inbuf = in;
	inlen = len;
	outbuf = mf->outbuf;
	outlen = mf->outsize;
	converted = unicode_iconv(f->ic, &inbuf, &inlen, &outbuf, &outlen);
	if (converted == -1) {
		if (errno != EINVAL) {
			g_warning("error occured converting: %s", strerror(errno));
			goto donothing;
		}
	}

	/*
	  NOTE: This assumes EINVAL only occurs because we ran out of
	  bytes for a multibyte sequence, if not, we're in trouble.
	*/

	if (inlen>0) {
		camel_mime_filter_backup(mf, inbuf, inlen);
	}

	*out = mf->outbuf;
	*outlenptr = mf->outsize - outlen;
	*outprespace = mf->outpre;
	return;

donothing:
	*out = in;
	*outlenptr = len;
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
	obj->ic = (unicode_iconv_t)-1;
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
	CamelMimeFilterCharset *new = CAMEL_MIME_FILTER_CHARSET ( camel_object_new (camel_mime_filter_charset_get_type ()));
	return new;
}

CamelMimeFilterCharset *
camel_mime_filter_charset_new_convert(const char *from_charset, const char *to_charset)
{
	CamelMimeFilterCharset *new = CAMEL_MIME_FILTER_CHARSET ( camel_object_new (camel_mime_filter_charset_get_type ()));

	new->ic = unicode_iconv_open(to_charset, from_charset);
	if (new->ic == (unicode_iconv_t) -1) {
		g_warning("Cannot create charset conversion from %s to %s: %s", from_charset, to_charset, strerror(errno));
		camel_object_unref((CamelObject *)new);
		new = NULL;
	} else {
		new->from = g_strdup(from_charset);
		new->to = g_strdup(to_charset);
	}
	return new;
}

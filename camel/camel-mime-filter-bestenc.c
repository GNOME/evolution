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

#include <stdio.h>
#include <string.h>

#include "camel-mime-filter-bestenc.h"

static void camel_mime_filter_bestenc_class_init (CamelMimeFilterBestencClass *klass);
static void camel_mime_filter_bestenc_init       (CamelMimeFilter *obj);

static CamelMimeFilterClass *camel_mime_filter_bestenc_parent;

CamelType
camel_mime_filter_bestenc_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (), "CamelMimeFilterBestenc",
					    sizeof (CamelMimeFilterBestenc),
					    sizeof (CamelMimeFilterBestencClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_bestenc_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_bestenc_init,
					    NULL);
	}
	
	return type;
}

static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterBestenc *f = (CamelMimeFilterBestenc *)mf;

	f->count0 = 0;
	f->count8 = 0;
	f->countline = 0;
	f->total = 0;
	f->lastc = ~0;
	f->crlfnoorder = FALSE;
	f->fromcount = 0;
	f->hadfrom = FALSE;
	f->startofline = TRUE;

	camel_charset_init(&f->charset);
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBestenc *f = (CamelMimeFilterBestenc *)mf;
	register unsigned char *p, *pend;

	if (len == 0)
		goto donothing;

	if (f->flags & CAMEL_BESTENC_GET_ENCODING) {
		register unsigned int /* hopefully reg's are assinged in the order they appear? */
			c,
			lastc=f->lastc, 
			countline=f->countline,
			count0=f->count0,
			count8 = f->count8;

		/* Check ^From  lines first call, or have the start of a new line waiting? */
		if ((f->flags & CAMEL_BESTENC_NO_FROM) && !f->hadfrom
		    && (f->fromcount > 0 || f->startofline)) {
			if (f->fromcount + len >=5) {
				memcpy(&f->fromsave[f->fromcount], in, 5-f->fromcount);
				f->hadfrom = strncmp(f->fromsave, "From ", 5) == 0;
				f->fromcount = 0;
			} else {
				memcpy(&f->fromsave[f->fromcount], in, len);
				f->fromcount += len;
			}
		}

		f->startofline = FALSE;

		/* See rfc2045 section 2 for definitions of 7bit/8bit/binary */
		p = in;
		pend = p + len;
		while (p<pend) {
			c = *p++;
			/* check for 8 bit characters */
			if (c & 0x80)
				count8++;

			/* check for nul's */
			if (c == 0)
				count0++;

			/* check for wild '\r's in a unix format stream */
			if (c == '\r' && (f->flags & CAMEL_BESTENC_LF_IS_CRLF)) {
				f->crlfnoorder = TRUE;
			}

			/* check for end of line */
			if (c == '\n') {
				/* check for wild '\n's in canonical format stream */
				if (lastc == '\r' || (f->flags & CAMEL_BESTENC_LF_IS_CRLF)) {
					if (countline > f->maxline)
						f->maxline = countline;
					countline = 0;

					/* Check for "^From " lines */
					if ((f->flags & CAMEL_BESTENC_NO_FROM) && !f->hadfrom) {
						if (pend-p >= 5) {
							f->hadfrom = strncmp(p, "From ", 5) == 0;
						} else if (pend-p == 0) {
							f->startofline = TRUE;
						} else {
							f->fromcount = pend-p;
							memcpy(f->fromsave, p, pend-p);
						}
					}
				} else {
					f->crlfnoorder = TRUE;
				}
			} else {
				countline++;
			}
			lastc = c;
		}
		f->count8 = count8;
		f->count0 = count0;
		f->countline = countline;
		f->lastc = lastc;
	}

	f->total += len;

	if (f->flags & CAMEL_BESTENC_GET_CHARSET)
		camel_charset_step(&f->charset, in, len);

donothing:
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBestenc *f = (CamelMimeFilterBestenc *)mf;

	filter(mf, in, len, prespace, out, outlen, outprespace);

	if (f->countline > f->maxline)
		f->maxline = f->countline;
	f->countline = 0;
}

static void
camel_mime_filter_bestenc_class_init (CamelMimeFilterBestencClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;

	camel_mime_filter_bestenc_parent = (CamelMimeFilterClass *)(camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));

	filter_class->reset = reset;
	filter_class->filter = filter;
	filter_class->complete = complete;
}

static void
camel_mime_filter_bestenc_init (CamelMimeFilter *f)
{
	reset(f);
}

/**
 * camel_mime_filter_bestenc_new:
 * @flags: A bitmask of data required.
 *
 * Create a new CamelMimeFilterBestenc object. 
 * 
 * Return value:
 **/
CamelMimeFilterBestenc *
camel_mime_filter_bestenc_new (unsigned int flags)
{
	CamelMimeFilterBestenc *new = (CamelMimeFilterBestenc *)camel_object_new(camel_mime_filter_bestenc_get_type());
	new->flags = flags;
	return new;
}

/**
 * camel_mime_filter_bestenc_get_best_encoding:
 * @f: bestenc filter object
 * @required: maximum level of output encoding allowed.
 * 
 * Return the best encoding, given specific constraints, that can be used to
 * encode a stream of bytes.
 * 
 * Return value: 
 **/
CamelMimePartEncodingType
camel_mime_filter_bestenc_get_best_encoding(CamelMimeFilterBestenc *f, CamelBestencEncoding required)
{
	CamelMimePartEncodingType bestenc;
	int istext;
	
	istext = (required & CAMEL_BESTENC_TEXT) ? 1 : 0;
	required = required & ~CAMEL_BESTENC_TEXT;
	
#if 0
	printf("count0 = %d, count8 = %d, total = %d\n", f->count0, f->count8, f->total);
	printf("maxline = %d, crlfnoorder = %s\n", f->maxline, f->crlfnoorder?"TRUE":"FALSE");
	printf(" %d%% require encoding?\n", (f->count0+f->count8)*100 / f->total);
#endif

	/* if we're not allowed to have From lines and we had one, use an encoding
	   that will never let it show.  Unfortunately only base64 can at present,
	   although qp could be modified to allow it too */
	if ((f->flags & CAMEL_BESTENC_NO_FROM) && f->hadfrom)
		return CAMEL_MIME_PART_ENCODING_BASE64;

	/* if we need to encode, see how we do it */
	if (required == CAMEL_BESTENC_BINARY)
		bestenc = CAMEL_MIME_PART_ENCODING_BINARY;
	else if (istext && (f->count0 == 0 && f->count8 < (f->total * 17 / 100)))
		bestenc = CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		bestenc = CAMEL_MIME_PART_ENCODING_BASE64;
	
	/* if we have nocrlf order, or long lines, we need to encode always */
	if (f->crlfnoorder || f->maxline >= 998)
		return bestenc;

	/* if we have no 8 bit chars or nul's, we can just use 7 bit */
	if (f->count8 + f->count0 == 0)
		return CAMEL_MIME_PART_ENCODING_7BIT;

	/* otherwise, we see if we can use 8 bit, or not */
	switch(required) {
	case CAMEL_BESTENC_7BIT:
		return bestenc;
	case CAMEL_BESTENC_8BIT:
	case CAMEL_BESTENC_BINARY:
	default:
		if (f->count0 == 0)
			return CAMEL_MIME_PART_ENCODING_8BIT;
		else
			return bestenc;
	}

	return CAMEL_MIME_PART_ENCODING_DEFAULT;
}

/**
 * camel_mime_filter_bestenc_get_best_charset:
 * @f: bestenc filter object
 * 
 * Gets the best charset that can be used to contain this content.
 * 
 * Return value: 
 **/
const char *
camel_mime_filter_bestenc_get_best_charset(CamelMimeFilterBestenc *f)
{
	return camel_charset_best_name(&f->charset);
}

/**
 * camel_mime_filter_bestenc_set_flags:
 * @f: 
 * @flags: 
 * 
 * Set the flags for subsequent operations.
 **/
void
camel_mime_filter_bestenc_set_flags(CamelMimeFilterBestenc *f, unsigned int flags)
{
	f->flags = flags;
}

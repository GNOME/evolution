/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#include "camel-mime-filter-basic.h"

#include "camel-mime-utils.h"
#include <string.h>

static void reset(CamelMimeFilter *mf);
static void complete(CamelMimeFilter *mf, char *in, size_t len, 
		     size_t prespace, char **out, 
		     size_t *outlen, size_t *outprespace);
static void filter(CamelMimeFilter *mf, char *in, size_t len, 
		   size_t prespace, char **out, 
		   size_t *outlen, size_t *outprespace);

static void camel_mime_filter_basic_class_init (CamelMimeFilterBasicClass *klass);
static void camel_mime_filter_basic_init       (CamelMimeFilterBasic *obj);

static CamelMimeFilterClass *camel_mime_filter_basic_parent;

static void
camel_mime_filter_basic_class_init (CamelMimeFilterBasicClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_basic_parent = CAMEL_MIME_FILTER_CLASS(camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));

	filter_class->reset = reset;
	filter_class->filter = filter;
	filter_class->complete = complete;
}

static void
camel_mime_filter_basic_init (CamelMimeFilterBasic *obj)
{
	obj->state = 0;
	obj->save = 0;
}


CamelType
camel_mime_filter_basic_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (), "CamelMimeFilterBasic",
					    sizeof (CamelMimeFilterBasic),
					    sizeof (CamelMimeFilterBasicClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_basic_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_basic_init,
					    NULL);
	}
	
	return type;
}

/* should this 'flush' outstanding state/data bytes? */
static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;
	
	switch(f->type) {
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
		f->state = -1;
		break;
	default:
		f->state = 0;
	}
	f->save = 0;
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;
	size_t newlen;
	
	switch(f->type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		camel_mime_filter_set_size(mf, len*2+6, FALSE);
		newlen = camel_base64_encode_close(in, len, TRUE, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len*2+6);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
		/* *4 is definetly more than needed ... */
		camel_mime_filter_set_size(mf, len*4+4, FALSE);
		newlen = camel_quoted_decode_close(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len*4+4);
		break;
	case CAMEL_MIME_FILTER_BASIC_UU_ENC:
		/* won't go to more than 2 * (x + 2) + 62 */
		camel_mime_filter_set_size (mf, (len + 2) * 2 + 62, FALSE);
		newlen = camel_uuencode_close (in, len, mf->outbuf, f->uubuf, &f->state, &f->save);
		g_assert (newlen <= (len + 2) * 2 + 62);
		break;
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
 		camel_mime_filter_set_size(mf, len, FALSE);
		newlen = camel_base64_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size, well unless its not really qp, then +2 max */
		camel_mime_filter_set_size(mf, len+2, FALSE);
		newlen = camel_quoted_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len+2);
		break;
	case CAMEL_MIME_FILTER_BASIC_UU_DEC:
		if ((f->state & CAMEL_UUDECODE_STATE_BEGIN) && !(f->state & CAMEL_UUDECODE_STATE_END)) {
			/* "begin <mode> <filename>\n" has been found, so we can now start decoding */
			camel_mime_filter_set_size (mf, len + 3, FALSE);
			newlen = camel_uudecode_step (in, len, mf->outbuf, &f->state, &f->save);
		} else {
			newlen = 0;
		}
		break;
	default:
		g_warning("unknown type %d in CamelMimeFilterBasic", f->type);
		goto donothing;
	}

	*out = mf->outbuf;
	*outlen = newlen;
	*outprespace = mf->outpre;

	return;
donothing:
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

/* here we do all of the basic mime filtering */
static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;
	size_t newlen;
	
	switch(f->type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		camel_mime_filter_set_size(mf, len*2+6, FALSE);
		newlen = camel_base64_encode_step(in, len, TRUE, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len*2+6);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
		/* *4 is overly conservative, but will do */
		camel_mime_filter_set_size(mf, len*4+4, FALSE);
		newlen = camel_quoted_encode_step(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len*4+4);
		break;
	case CAMEL_MIME_FILTER_BASIC_UU_ENC:
		/* won't go to more than 2 * (x + 2) + 62 */
		camel_mime_filter_set_size (mf, (len + 2) * 2 + 62, FALSE);
		newlen = camel_uuencode_step (in, len, mf->outbuf, f->uubuf, &f->state, &f->save);
		g_assert (newlen <= (len + 2) * 2 + 62);
		break;
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len+3, FALSE);
		newlen = camel_base64_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len+3);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len + 2, FALSE);
		newlen = camel_quoted_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		g_assert(newlen <= len + 2);
		break;
	case CAMEL_MIME_FILTER_BASIC_UU_DEC:
		if (!(f->state & CAMEL_UUDECODE_STATE_BEGIN)) {
			register char *inptr, *inend;
			size_t left;
			
			inptr = in;
			inend = inptr + len;
			
			while (inptr < inend) {
				left = inend - inptr;
				if (left < 6) {
					if (!strncmp (inptr, "begin ", left))
						camel_mime_filter_backup (mf, inptr, left);
					break;
				} else if (!strncmp (inptr, "begin ", 6)) {
					for (in = inptr; inptr < inend && *inptr != '\n'; inptr++);
					if (inptr < inend) {
						inptr++;
						f->state |= CAMEL_UUDECODE_STATE_BEGIN;
						/* we can start uudecoding... */
						in = inptr;
						len = inend - in;
					} else {
						camel_mime_filter_backup (mf, in, left);
					}
					break;
				}
				
				/* go to the next line */
				for ( ; inptr < inend && *inptr != '\n'; inptr++);
				
				if (inptr < inend)
					inptr++;
			}
		}
		
		if ((f->state & CAMEL_UUDECODE_STATE_BEGIN) && !(f->state & CAMEL_UUDECODE_STATE_END)) {
			/* "begin <mode> <filename>\n" has been found, so we can now start decoding */
			camel_mime_filter_set_size (mf, len + 3, FALSE);
			newlen = camel_uudecode_step (in, len, mf->outbuf, &f->state, &f->save);
		} else {
			newlen = 0;
		}
		break;
	default:
		g_warning("unknown type %d in CamelMimeFilterBasic", f->type);
		goto donothing;
	}

	*out = mf->outbuf;
	*outlen = newlen;
	*outprespace = mf->outpre;

	return;
donothing:
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

/**
 * camel_mime_filter_basic_new:
 *
 * Create a new CamelMimeFilterBasic object.
 * 
 * Return value: A new CamelMimeFilterBasic widget.
 **/
CamelMimeFilterBasic *
camel_mime_filter_basic_new (void)
{
	CamelMimeFilterBasic *new = CAMEL_MIME_FILTER_BASIC ( camel_object_new (camel_mime_filter_basic_get_type ()));
	return new;
}

CamelMimeFilterBasic *
camel_mime_filter_basic_new_type(CamelMimeFilterBasicType type)
{
	CamelMimeFilterBasic *new;

	switch (type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
	case CAMEL_MIME_FILTER_BASIC_UU_ENC:
	case CAMEL_MIME_FILTER_BASIC_UU_DEC:
		new = camel_mime_filter_basic_new();
		new->type = type;
		break;
	default:
		g_warning("Invalid type of CamelMimeFilterBasic requested: %d", type);
		new = NULL;
		break;
	}
	camel_mime_filter_reset((CamelMimeFilter *)new);
	return new;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 * camel-multipart.c : Abstract class for a multipart 
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>

#include "camel-mime-part.h"
#include "camel-mime-message.h"
#include "camel-mime-parser.h"
#include "camel-stream-mem.h"
#include "camel-multipart-signed.h"
#include "camel-mime-part.h"
#include "camel-exception.h"
#include "libedataserver/md5-utils.h"

#include "camel-stream-filter.h"
#include "camel-seekable-substream.h"
#include "camel-mime-filter-crlf.h"
#include "camel-mime-filter-canon.h"
#include "camel-i18n.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))
	       #include <stdio.h>;*/

static void signed_add_part (CamelMultipart *multipart, CamelMimePart *part);
static void signed_add_part_at (CamelMultipart *multipart, CamelMimePart *part, guint index);
static void signed_remove_part (CamelMultipart *multipart, CamelMimePart *part);
static CamelMimePart *signed_remove_part_at (CamelMultipart *multipart, guint index);
static CamelMimePart *signed_get_part (CamelMultipart *multipart, guint index);
static guint signed_get_number (CamelMultipart *multipart);

static ssize_t write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void set_mime_type_field (CamelDataWrapper *data_wrapper, CamelContentType *mime_type);
static int construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static int signed_construct_from_parser (CamelMultipart *multipart, struct _CamelMimeParser *mp);

static CamelMultipartClass *parent_class = NULL;

/* Returns the class for a CamelMultipartSigned */
#define CMP_CLASS(so) CAMEL_MULTIPART_SIGNED_CLASS (CAMEL_OBJECT_GET_CLASS(so))

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void
camel_multipart_signed_class_init (CamelMultipartSignedClass *camel_multipart_signed_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS(camel_multipart_signed_class);
	CamelMultipartClass *mpclass = (CamelMultipartClass *)camel_multipart_signed_class;

	parent_class = (CamelMultipartClass *)camel_multipart_get_type();

	/* virtual method overload */
	camel_data_wrapper_class->construct_from_stream = construct_from_stream;
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->decode_to_stream = write_to_stream;
	camel_data_wrapper_class->set_mime_type_field = set_mime_type_field;

	mpclass->add_part = signed_add_part;
	mpclass->add_part_at = signed_add_part_at;
	mpclass->remove_part = signed_remove_part;
	mpclass->remove_part_at = signed_remove_part_at;
	mpclass->get_part = signed_get_part;
	mpclass->get_number = signed_get_number;
	mpclass->construct_from_parser = signed_construct_from_parser;

/*
	mpclass->get_boundary = signed_get_boundary;
	mpclass->set_boundary = signed_set_boundary;
*/
}

static void
camel_multipart_signed_init (gpointer object, gpointer klass)
{
	CamelMultipartSigned *multipart = (CamelMultipartSigned *)object;

	camel_data_wrapper_set_mime_type(CAMEL_DATA_WRAPPER(multipart), "multipart/signed");
	multipart->start1 = -1;
}

static void
camel_multipart_signed_finalize (CamelObject *object)
{
	CamelMultipartSigned *mps = (CamelMultipartSigned *)object;

	g_free(mps->protocol);
	g_free(mps->micalg);
	if (mps->signature)
		camel_object_unref((CamelObject *)mps->signature);
	if (mps->content)
		camel_object_unref((CamelObject *)mps->content);
	if (mps->contentraw)
		camel_object_unref((CamelObject *)mps->contentraw);
}

CamelType
camel_multipart_signed_get_type (void)
{
	static CamelType camel_multipart_signed_type = CAMEL_INVALID_TYPE;

	if (camel_multipart_signed_type == CAMEL_INVALID_TYPE) {
		camel_multipart_signed_type = camel_type_register (camel_multipart_get_type (), "CamelMultipartSigned",
								   sizeof (CamelMultipartSigned),
								   sizeof (CamelMultipartSignedClass),
								   (CamelObjectClassInitFunc) camel_multipart_signed_class_init,
								   NULL,
								   (CamelObjectInitFunc) camel_multipart_signed_init,
								   (CamelObjectFinalizeFunc) camel_multipart_signed_finalize);
	}

	return camel_multipart_signed_type;
}

/**
 * camel_multipart_signed_new:
 *
 * Create a new CamelMultipartSigned object.
 *
 * A MultipartSigned should be used to store and create parts of
 * type "multipart/signed".  This is because multipart/signed is
 * entirely broken-by-design (tm) and uses completely
 * different semantics to other mutlipart types.  It must be treated
 * as opaque data by any transport.  See rfc 3156 for details.
 *
 * There are 3 ways to create the part:
 * Use construct_from_stream.  If this is used, then you must
 * set the mime_type appropriately to match the data uses, so
 * that the multiple parts my be extracted.
 *
 * Use construct_from_parser.  The parser MUST be in the CAMEL_MIME_PARSER_STATE_HEADER
 * state, and the current content_type MUST be "multipart/signed" with
 * the appropriate boundary and it SHOULD include the appropriate protocol
 * and hash specifiers.
 *
 * Use sign_part.  A signature part will automatically be created
 * and the whole part may be written using write_to_stream to
 * create a 'transport-safe' version (as safe as can be expected with
 * such a broken specification).
 *
 * Return value: a new CamelMultipartSigned
 **/
CamelMultipartSigned *
camel_multipart_signed_new (void)
{
	CamelMultipartSigned *multipart;

	multipart = (CamelMultipartSigned *)camel_object_new(CAMEL_MULTIPART_SIGNED_TYPE);

	return multipart;
}

/* find the next boundary @bound from @start, return the start of the actual data
   @end points to the end of the data BEFORE the boundary */
static char *parse_boundary(char *start, const char *bound, char **end)
{
	char *data, *begin;

	begin = strstr(start, bound);
	if (begin == NULL)
		return NULL;

	data = begin+strlen(bound);
	if (begin > start && begin[-1] == '\n')
		begin--;
	if (begin > start && begin[-1] == '\r')
		begin--;
	if (data[0] == '\r')
		data++;
	if (data[0] == '\n')
		data++;

	*end = begin;
	return data;
}

/* yeah yuck.
   Well, we could probably use the normal mime parser, but then it would change our
   headers.
   This is good enough ... till its not! */
static int
parse_content(CamelMultipartSigned *mps)
{
	CamelMultipart *mp = (CamelMultipart *)mps;
	char *start, *end, *start2, *end2, *last, *post;
	CamelStreamMem *mem;
	char *bound;
	const char *boundary;

	boundary = camel_multipart_get_boundary(mp);
	if (boundary == NULL) {
		g_warning("Trying to get multipart/signed content without setting boundary first");
		return -1;
	}

	/* turn it into a string, and 'fix' it up */
	/* this is extremely dodgey but should work! */
	mem = (CamelStreamMem *)((CamelDataWrapper *)mps)->stream;
	if (mem == NULL) {
		g_warning("Trying to parse multipart/signed without constructing first");
		return -1;
	}

	camel_stream_write((CamelStream *)mem, "", 1);
	g_byte_array_set_size(mem->buffer, mem->buffer->len-1);
	last = mem->buffer->data + mem->buffer->len;

	bound = alloca(strlen(boundary)+5);
	sprintf(bound, "--%s", boundary);

	start = parse_boundary(mem->buffer->data, bound, &end);
	if (start == NULL || start[0] == 0)
		return -1;

	if (end > (char *)mem->buffer->data) {
		char *tmp = g_strndup(mem->buffer->data, start-(char *)mem->buffer->data-1);
		camel_multipart_set_preface(mp, tmp);
		g_free(tmp);
	}

	start2 = parse_boundary(start, bound, &end);
	if (start2 == NULL || start2[0] == 0)
		return -1;

	sprintf(bound, "--%s--", boundary);
	post = parse_boundary(start2, bound, &end2);
	if (post == NULL)
		return -1;

	if (post[0])
		camel_multipart_set_postface(mp, post);

	mps->start1 = start-(char *)mem->buffer->data;
	mps->end1 = end-(char *)mem->buffer->data;
	mps->start2 = start2-(char *)mem->buffer->data;
	mps->end2 = end2-(char *)mem->buffer->data;

	return 0;
}

/* we snoop the mime type to get boundary and hash info */
static void
set_mime_type_field(CamelDataWrapper *data_wrapper, CamelContentType *mime_type)
{
	CamelMultipartSigned *mps = (CamelMultipartSigned *)data_wrapper;

	((CamelDataWrapperClass *)parent_class)->set_mime_type_field(data_wrapper, mime_type);
	if (mime_type) {
		const char *micalg, *protocol;

		protocol = camel_content_type_param(mime_type, "protocol");
		g_free(mps->protocol);
		mps->protocol = g_strdup(protocol);

		micalg = camel_content_type_param(mime_type, "micalg");
		g_free(mps->micalg);
		mps->micalg = g_strdup(micalg);
	}
}

static void
signed_add_part(CamelMultipart *multipart, CamelMimePart *part)
{
	g_warning("Cannot add parts to a signed part using add_part");
}

static void
signed_add_part_at(CamelMultipart *multipart, CamelMimePart *part, guint index)
{
	g_warning("Cannot add parts to a signed part using add_part_at");
}

static void
signed_remove_part(CamelMultipart *multipart, CamelMimePart *part)
{
	g_warning("Cannot remove parts from a signed part using remove_part");
}

static CamelMimePart *
signed_remove_part_at (CamelMultipart *multipart, guint index)
{
	g_warning("Cannot remove parts from a signed part using remove_part");
	return NULL;
}

static CamelMimePart *
signed_get_part(CamelMultipart *multipart, guint index)
{
	CamelMultipartSigned *mps = (CamelMultipartSigned *)multipart;
	CamelDataWrapper *dw = (CamelDataWrapper *)multipart;
	CamelStream *stream;

	switch (index) {
	case CAMEL_MULTIPART_SIGNED_CONTENT:
		if (mps->content)
			return mps->content;
		if (mps->contentraw) {
			stream = mps->contentraw;
			camel_object_ref((CamelObject *)stream);
		} else if (mps->start1 == -1
			   && parse_content(mps) == -1
			   && (stream = ((CamelDataWrapper *)mps)->stream) == NULL) {
			g_warning("Trying to get content on an invalid multipart/signed");
			return NULL;
		} else if (dw->stream == NULL) {
			return NULL;
		} else if (mps->start1 == -1) {
			stream = dw->stream;
			camel_object_ref(stream);
		} else {
			stream = camel_seekable_substream_new((CamelSeekableStream *)dw->stream, mps->start1, mps->end1);
		}
		camel_stream_reset(stream);
		mps->content = camel_mime_part_new();
		camel_data_wrapper_construct_from_stream((CamelDataWrapper *)mps->content, stream);
		camel_object_unref(stream);
		return mps->content;
	case CAMEL_MULTIPART_SIGNED_SIGNATURE:
		if (mps->signature)
			return mps->signature;
		if (mps->start1 == -1
		    && parse_content(mps) == -1) {
			g_warning("Trying to get signature on invalid multipart/signed");
			return NULL;
		} else if (dw->stream == NULL) {
			return NULL;
		}
		stream = camel_seekable_substream_new((CamelSeekableStream *)dw->stream, mps->start2, mps->end2);
		camel_stream_reset(stream);
		mps->signature = camel_mime_part_new();
		camel_data_wrapper_construct_from_stream((CamelDataWrapper *)mps->signature, stream);
		camel_object_unref((CamelObject *)stream);
		return mps->signature;
	default:
		g_warning("trying to get object out of bounds for multipart");
	}

	return NULL;
}

static guint
signed_get_number(CamelMultipart *multipart)
{
	CamelDataWrapper *dw = (CamelDataWrapper *)multipart;
	CamelMultipartSigned *mps = (CamelMultipartSigned *)multipart;

	/* check what we have, so we return something reasonable */

	if ((mps->content || mps->contentraw) && mps->signature)
		return 2;

	if (mps->start1 == -1 && parse_content(mps) == -1) {
		if (dw->stream == NULL)
			return 0;
		else
			return 1;
	} else {
		return 2;
	}
}

static void
set_stream(CamelMultipartSigned *mps, CamelStream *mem)
{
	CamelDataWrapper *dw = (CamelDataWrapper *)mps;

	if (dw->stream)
		camel_object_unref((CamelObject *)dw->stream);
	dw->stream = (CamelStream *)mem;

	mps->start1 = -1;
	if (mps->content) {
		camel_object_unref((CamelObject *)mps->content);
		mps->content = NULL;
	}
	if (mps->contentraw) {
		camel_object_unref((CamelObject *)mps->contentraw);
		mps->contentraw = NULL;
	}
	if (mps->signature) {
		camel_object_unref((CamelObject *)mps->signature);
		mps->signature = NULL;
	}
}

static int
construct_from_stream(CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMultipartSigned *mps = (CamelMultipartSigned *)data_wrapper;
	CamelStream *mem = camel_stream_mem_new();

	if (camel_stream_write_to_stream(stream, mem) == -1)
		return -1;

	set_stream(mps, mem);

	return 0;
}

static int
signed_construct_from_parser(CamelMultipart *multipart, struct _CamelMimeParser *mp)
{
	int err;
	CamelContentType *content_type;
	CamelMultipartSigned *mps = (CamelMultipartSigned *)multipart;
	char *buf;
	size_t len;
	CamelStream *mem;

	/* we *must not* be in multipart state, otherwise the mime parser will
	   parse the headers which is a no no @#$@# stupid multipart/signed spec */
	g_assert(camel_mime_parser_state(mp) == CAMEL_MIME_PARSER_STATE_HEADER);

	/* All we do is copy it to a memstream */
	content_type = camel_mime_parser_content_type(mp);
	camel_multipart_set_boundary(multipart, camel_content_type_param(content_type, "boundary"));

	mem = camel_stream_mem_new();
	while (camel_mime_parser_step(mp, &buf, &len) != CAMEL_MIME_PARSER_STATE_BODY_END)
		camel_stream_write(mem, buf, len);

	set_stream(mps, mem);

	err = camel_mime_parser_errno(mp);
	if (err != 0) {
		errno = err;
		return -1;
	} else
		return 0;
}

static ssize_t
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMultipartSigned *mps = (CamelMultipartSigned *)data_wrapper;
	CamelMultipart *mp = (CamelMultipart *)mps;
	const char *boundary;
	ssize_t total = 0;
	ssize_t count;
	
	/* we have 3 basic cases:
	   1. constructed, we write out the data wrapper stream we got
	   2. signed content, we create and write out a new stream
	   3. invalid
	*/

	/* 1 */
	/* FIXME: locking? */
	if (data_wrapper->stream) {
		camel_stream_reset(data_wrapper->stream);
		return camel_stream_write_to_stream(data_wrapper->stream, stream);
	}

	/* 3 */
	if (mps->signature == NULL || mps->contentraw == NULL)
		return -1;

	/* 2 */
	boundary = camel_multipart_get_boundary(mp);
	if (mp->preface) {
		count = camel_stream_write_string(stream, mp->preface);
		if (count == -1)
			return -1;
		total += count;
	}

	/* first boundary */
	count = camel_stream_printf(stream, "\n--%s\n", boundary);
	if (count == -1)
		return -1;
	total += count;

	/* output content part */
	camel_stream_reset(mps->contentraw);
	count = camel_stream_write_to_stream(mps->contentraw, stream);
	if (count == -1)
		return -1;
	total += count;
	
	/* boundary */
	count = camel_stream_printf(stream, "\n--%s\n", boundary);
	if (count == -1)
		return -1;
	total += count;

	/* signature */
	count = camel_data_wrapper_write_to_stream((CamelDataWrapper *)mps->signature, stream);
	if (count == -1)
		return -1;
	total += count;

	/* write the terminating boudary delimiter */
	count = camel_stream_printf(stream, "\n--%s--\n", boundary);
	if (count == -1)
		return -1;
	total += count;

	/* and finally the postface */
	if (mp->postface) {
		count = camel_stream_write_string(stream, mp->postface);
		if (count == -1)
			return -1;
		total += count;
	}

	return total;	
}

CamelStream *
camel_multipart_signed_get_content_stream(CamelMultipartSigned *mps, CamelException *ex)
{
	CamelStream *constream;

	/* we need to be able to verify stuff we just signed as well as stuff we loaded from a stream/parser */

	if (mps->contentraw) {
		constream = mps->contentraw;
		camel_object_ref((CamelObject *)constream);
	} else {
		CamelStream *sub;
		CamelMimeFilter *canon_filter;

		if (mps->start1 == -1 && parse_content(mps) == -1) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("parse error"));
			return NULL;
		}

		/* first, prepare our parts */
		sub = camel_seekable_substream_new((CamelSeekableStream *)((CamelDataWrapper *)mps)->stream, mps->start1, mps->end1);
		constream = (CamelStream *)camel_stream_filter_new_with_stream(sub);
		camel_object_unref((CamelObject *)sub);
		
		/* Note: see rfc2015 or rfc3156, section 5 */
		canon_filter = camel_mime_filter_canon_new (CAMEL_MIME_FILTER_CANON_CRLF);
		camel_stream_filter_add((CamelStreamFilter *)constream, (CamelMimeFilter *)canon_filter);
		camel_object_unref((CamelObject *)canon_filter);
	}

	return constream;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-part-utils : Utility for mime parsing and so on
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 * 	    Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <config.h>
#include <string.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "gmime-utils.h"
#include "camel-mime-part-utils.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-mime-body-part.h"
#include "camel-seekable-substream.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-mime-filter-basic.h"
#include "camel-mime-filter-charset.h"

#define d(x)

/* simple data wrapper */
static void
simple_data_wrapper_construct_from_parser(CamelDataWrapper *dw, CamelMimeParser *mp)
{
	GByteArray *buffer;
	char *buf;
	int len;
	off_t start, end;
	CamelMimeFilter *fdec = NULL, *fch = NULL;
	struct _header_content_type *ct;
	int decid=-1, chrid=-1, cache=FALSE;
	CamelStream *source;
	char *encoding;

	d(printf("constructing data-wrapper\n"));

		/* Ok, try and be smart.  If we're storing a small message (typical) convert it,
		   and store it in memory as we parse it ... if not, throw away the conversion
		   and scan till the end ... */

		/* if we can't seek, dont have a stream/etc, then we must cache it */
	source = camel_mime_parser_stream(mp);
	gtk_object_ref((GtkObject *)source);
	if (source == NULL
	    || !CAMEL_IS_SEEKABLE_STREAM(source))
		cache = TRUE;

	/* first, work out conversion, if any, required, we dont care about what we dont know about */
	encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));
	if (encoding) {
		if (!strcasecmp(encoding, "base64")) {
			d(printf("Adding base64 decoder ...\n"));
			fdec = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
			decid = camel_mime_parser_filter_add(mp, fdec);
		} else if (!strcasecmp(encoding, "quoted-printable")) {
			d(printf("Adding quoted-printable decoder ...\n"));
			fdec = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
			decid = camel_mime_parser_filter_add(mp, fdec);
		}
		g_free(encoding);
	}

	/* if we're doing text, then see if we have to convert it to UTF8 as well */
	ct = camel_mime_parser_content_type(mp);
	if (header_content_type_is(ct, "text", "*")) {
		const char *charset = header_content_type_param(ct, "charset");
		if (charset!=NULL
		    && !(strcasecmp(charset, "us-ascii")==0
			 || strcasecmp(charset, "utf-8")==0)) {
			d(printf("Adding conversion filter from %s to utf-8\n", charset));
			fch = (CamelMimeFilter *)camel_mime_filter_charset_new_convert(charset, "utf-8");
			if (fch) {
				chrid = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)fch);
			} else {
				g_warning("Cannot convert '%s' to 'utf-8', message display may be corrupt", charset);
			}
		}

	}

	buffer = g_byte_array_new();

	start = camel_mime_parser_tell(mp);
	while ( camel_mime_parser_step(mp, &buf, &len) != HSCAN_BODY_END ) {
		if (buffer) {
			if (buffer->len > 20480 && !cache) {
				/* is this a 'big' message?  Yes?  We dont want to convert it all then.*/
				camel_mime_parser_filter_remove(mp, decid);
				camel_mime_parser_filter_remove(mp, chrid);
				decid = -1;
				chrid = -1;
				g_byte_array_free(buffer, TRUE);
				buffer = NULL;
			} else {
				g_byte_array_append(buffer, buf, len);
			}
		}
	}

	if (buffer) {
		CamelStream *mem;
		d(printf("Small message part, kept in memory!\n"));
		mem = camel_stream_mem_new_with_byte_array(buffer);
		camel_data_wrapper_set_output_stream (dw, mem);
	} else {
		CamelSeekableSubstream *sub;
		CamelStreamFilter *filter;

		d(printf("Big message part, left on disk ...\n"));

		end = camel_mime_parser_tell(mp);
		sub = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds ((CamelSeekableStream *)source, start, end);
		if (fdec || fch) {
			filter = camel_stream_filter_new_with_stream((CamelStream *)sub);
			if (fdec) {
				camel_mime_filter_reset(fdec);
				camel_stream_filter_add(filter, fdec);
			}
			if (fch) {
				camel_mime_filter_reset(fch);
				camel_stream_filter_add(filter, fch);
			}
			camel_data_wrapper_set_output_stream (dw, (CamelStream *)filter);
		} else {
			camel_data_wrapper_set_output_stream (dw, (CamelStream *)sub);
		}
	}

	camel_mime_parser_filter_remove(mp, decid);
	camel_mime_parser_filter_remove(mp, chrid);

	if (fdec)
		gtk_object_unref((GtkObject *)fdec);
	if (fch)
		gtk_object_unref((GtkObject *)fch);
	gtk_object_unref((GtkObject *)source);

}

/* This replaces the data wrapper repository ... and/or could be replaced by it? */
void
camel_mime_part_construct_content_from_parser(CamelMimePart *dw, CamelMimeParser *mp)
{
	CamelDataWrapper *content = NULL;
	char *buf;
	int len;

	switch (camel_mime_parser_state(mp)) {
	case HSCAN_HEADER:
		d(printf("Creating body part\n"));
		content = camel_data_wrapper_new();
		simple_data_wrapper_construct_from_parser(content, mp);
		break;
	case HSCAN_MESSAGE:
		d(printf("Creating message part\n"));
		content = (CamelDataWrapper *)camel_mime_message_new();
		camel_mime_part_construct_from_parser((CamelMimePart *)content, mp);
		break;
	case HSCAN_MULTIPART: {
		CamelDataWrapper *bodypart;

#warning This should use a camel-mime-multipart
		d(printf("Creating multi-part\n"));
		content = (CamelDataWrapper *)camel_multipart_new();

		/* get/set boundary? */

		while (camel_mime_parser_step(mp, &buf, &len) != HSCAN_MULTIPART_END) {
			camel_mime_parser_unstep(mp);
			bodypart = (CamelDataWrapper *)camel_mime_body_part_new();
			camel_mime_part_construct_from_parser((CamelMimePart *)bodypart, mp);
			camel_multipart_add_part((CamelMultipart *)content, (CamelMimeBodyPart *)bodypart);
		}

		d(printf("Created multi-part\n"));
		break; }
	default:
		g_warning("Invalid state encountered???: %d", camel_mime_parser_state(mp));
	}
	if (content) {
#warning there just has got to be a better way ... to transfer the mime-type to the datawrapper
		/* would you believe you have to set this BEFORE you set the content object???  oh my god !!!! */
		camel_data_wrapper_set_mime_type_field (content, 
							camel_mime_part_get_content_type ((CamelMimePart *)dw));
		camel_medium_set_content_object((CamelMedium *)dw, content);
	}
}


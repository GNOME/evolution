/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-part-utils : Utility for mime parsing and so on
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 * 	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "string-utils.h"
#include "camel-mime-part-utils.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-seekable-substream.h"
#include "camel-stream-fs.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-mime-filter-basic.h"
#include "camel-mime-filter-charset.h"
#include "camel-mime-filter-crlf.h"
#include "camel-html-parser.h"
#include "camel-charset-map.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

/* example: <META http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"> */

static const char *
check_html_charset (CamelMimeParser *mp, CamelMimeFilterBasicType enctype)
{
	const char *buf;
	off_t offset;
	int length;
	CamelHTMLParser *hp;
	const char *charset = NULL;
	camel_html_parser_t state;
	struct _header_content_type *ct;
	CamelMimeFilterBasic *fdec = NULL;

	/* if we can't find the charset within the first 2k, we ain't gonna find it */
	offset = camel_mime_parser_tell(mp);
	length = camel_mime_parser_read(mp, &buf, 2048);

	d(printf("Checking html for meta content-type: '%.*s'", len, buf));

	if (length == 0) {
		camel_mime_parser_seek(mp, offset, SEEK_SET);
		return NULL;
	}

	/* if we need to first base64/qp decode, do this here, sigh */
	hp = camel_html_parser_new();
	if (enctype != 0) {
		int dummy, len;
		char *buffer;

		fdec = camel_mime_filter_basic_new_type(enctype);
		camel_mime_filter_filter((CamelMimeFilter *)fdec, (char *)buf, length, 0, &buffer, &len, &dummy);
		camel_html_parser_set_data(hp, buffer, len, TRUE);
	} else {
		camel_html_parser_set_data(hp, buf, length, TRUE);
	}
	
	do {
		const char *data;
		int len;
		const char *val;
		
		state = camel_html_parser_step(hp, &data, &len);
		
		/* example: <META http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"> */
		
		switch(state) {
		case CAMEL_HTML_PARSER_ELEMENT:
			val = camel_html_parser_tag(hp);
			d(printf("Got tag: %s\n", tag));
			if (g_strcasecmp(val, "meta") == 0
			    && (val = camel_html_parser_attr(hp, "http-equiv"))
			    && g_strcasecmp(val, "content-type") == 0
			    && (val = camel_html_parser_attr(hp, "content"))
			    && (ct = header_content_type_decode(val))) {
				charset = header_content_type_param(ct, "charset");
				if (charset)
					charset = camel_charset_to_iconv (charset);
				header_content_type_unref(ct);
			}
			break;
		default:
			/* ignore everything else */
			break;
		}
	} while (charset == NULL && state != CAMEL_HTML_PARSER_EOF);

	camel_object_unref((CamelObject *)hp);
	if (fdec)
		camel_object_unref((CamelObject *)fdec);

	camel_mime_parser_seek(mp, offset, SEEK_SET);

	return charset;
}

static GByteArray *convert_buffer(GByteArray *in, const char *to, const char *from)
{
	iconv_t ic;
	int inlen, outlen, convlen, i=2;
	char *inbuf, *outbuf;
	char *buffer;
	GByteArray *out = NULL;

	ic = iconv_open(to, from);
	if (ic == (iconv_t) -1) {
		g_warning("Cannot convert from '%s' to '%s': %s", from, to, strerror(errno));
		return NULL;
	}

	do {
		/* make plenty of space? */
		outlen = in->len * i + 16;
		buffer = g_malloc(outlen);

		inbuf = in->data;
		inlen = in->len;
		outbuf = buffer;

		convlen = iconv(ic, (const char **)&inbuf, &inlen, &outbuf, &outlen);
		if (convlen == -1) {
			g_free(buffer);
			g_warning("conversion failed: %s", strerror(errno));
			/* we didn't have enough space */
			if (errno == E2BIG && i<6) {
				i++;
				continue;
			}
			break;
		}

		out = g_byte_array_new();
		g_byte_array_append(out, buffer, convlen);

		/* close off the conversion */
		outbuf = buffer;
		outlen = in->len * i + 16;
		convlen = iconv(ic, NULL, 0, &outbuf, &outlen);
		if (convlen != -1)
			g_byte_array_append(out, buffer, convlen);
		g_free(buffer);
		break;
	} while (1);

	iconv_close(ic);

	return out;
}

/* simple data wrapper */
static void
simple_data_wrapper_construct_from_parser (CamelDataWrapper *dw, CamelMimeParser *mp)
{
	CamelMimeFilter *fdec = NULL, *fcrlf = NULL /*, *fch = NULL*/;
	int len, decid = -1, crlfid = -1 /*, chrid = -1*/;
	struct _header_content_type *ct;
	CamelSeekableStream *seekable_source = NULL;
	CamelStream *source;
	GByteArray *buffer;
	/*off_t start = 0, end;*/
	char *encoding, *buf;
	const char *charset = NULL;
	CamelMimeFilterBasicType enctype = 0;
	CamelStream *mem;

	d(printf("constructing data-wrapper\n"));
	
	/* Ok, try and be smart.  If we're storing a small message (typical) convert it,
	   and store it in memory as we parse it ... if not, throw away the conversion
	   and scan till the end ... */
	
	/* if we can't seek, dont have a stream/etc, then we must cache it */
	source = camel_mime_parser_stream (mp);
	if (source) {
		camel_object_ref ((CamelObject *)source);
		if (CAMEL_IS_SEEKABLE_STREAM (source)) {
			seekable_source = CAMEL_SEEKABLE_STREAM (source);
		}
	}
	
	/* first, work out conversion, if any, required, we dont care about what we dont know about */
	encoding = header_content_encoding_decode (camel_mime_parser_header (mp, "content-transfer-encoding", NULL));
	if (encoding) {
		if (!strcasecmp (encoding, "base64")) {
			d(printf("Adding base64 decoder ...\n"));
			enctype = CAMEL_MIME_FILTER_BASIC_BASE64_DEC;
		} else if (!strcasecmp(encoding, "quoted-printable")) {
			d(printf("Adding quoted-printable decoder ...\n"));
			enctype = CAMEL_MIME_FILTER_BASIC_QP_DEC;
		}
		g_free (encoding);

		if (enctype != 0) {
			fdec = (CamelMimeFilter *)camel_mime_filter_basic_new_type(enctype);
			decid = camel_mime_parser_filter_add (mp, fdec);
		}
	}
	
	/* If we're doing text, we also need to do CRLF->LF and may have to convert it to UTF8 as well. */
	ct = camel_mime_parser_content_type (mp);
	if (header_content_type_is (ct, "text", "*")) {
		charset = header_content_type_param (ct, "charset");
		
		if (fdec) {
			d(printf("Adding CRLF conversion filter\n"));
			fcrlf = (CamelMimeFilter *)camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE,
									       CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
			crlfid = camel_mime_parser_filter_add (mp, fcrlf);
		}
		
		/* Possible Lame Mailer Alert... check the META tags for a charset */
		/* FIXME: do this as part of the normal data reading, so we dont have to do it twice */
		if (!charset && header_content_type_is (ct, "text", "html"))
			charset = check_html_charset (mp, enctype);
	}
	
	/* read in the entire content */
	buffer = g_byte_array_new();
	while (camel_mime_parser_step(mp, &buf, &len) != HSCAN_BODY_END) {
		d(printf("appending o/p data: %d: %.*s\n", len, len, buf));
		g_byte_array_append(buffer, buf, len);
	}

	/* if we need to do charset conversion, see if we can/it works/etc */
	if (charset && !(strcasecmp(charset, "us-ascii") == 0
			 || strcasecmp(charset, "utf-8") == 0
			 || strncasecmp(charset, "x-", 2) == 0)) {
		GByteArray *out;

		out = convert_buffer(buffer, "UTF-8", charset);
		if (out) {
			/* converted ok, use this data instead */
			g_byte_array_free(buffer, TRUE);
			buffer = out;
		} else {
			g_warning("Storing text as raw, unknown charset '%s' or invalid format", charset);
			/* else failed to convert, leave as raw? */
			dw->rawtext = TRUE;
			/* should we change the content-type header? */
		}
	}

	d(printf("message part kept in memory!\n"));
		
	mem = camel_stream_mem_new_with_byte_array(buffer);
	camel_data_wrapper_construct_from_stream(dw, mem);
	camel_object_unref((CamelObject *)mem);

	camel_mime_parser_filter_remove (mp, decid);
	camel_mime_parser_filter_remove (mp, crlfid);
	
	if (fdec)
		camel_object_unref ((CamelObject *)fdec);
	if (fcrlf)
		camel_object_unref ((CamelObject *)fcrlf);
	if (source)
		camel_object_unref ((CamelObject *)source);
}

/* This replaces the data wrapper repository ... and/or could be replaced by it? */
void
camel_mime_part_construct_content_from_parser (CamelMimePart *dw, CamelMimeParser *mp)
{
	CamelDataWrapper *content = NULL;
	char *buf;
	int len;
	
	switch (camel_mime_parser_state (mp)) {
	case HSCAN_HEADER:
		d(printf("Creating body part\n"));
		content = camel_data_wrapper_new ();
		simple_data_wrapper_construct_from_parser (content, mp);
		break;
	case HSCAN_MESSAGE:
		d(printf("Creating message part\n"));
		content = (CamelDataWrapper *) camel_mime_message_new ();
		camel_mime_part_construct_from_parser ((CamelMimePart *)content, mp);
		break;
	case HSCAN_MULTIPART: {
		CamelDataWrapper *bodypart;

		/* FIXME: we should use a came-mime-mutlipart, not jsut a camel-multipart, but who cares */
		d(printf("Creating multi-part\n"));
		content = (CamelDataWrapper *)camel_multipart_new ();
		
		/* FIXME: use the real boundary? */
		camel_multipart_set_boundary ((CamelMultipart *)content, NULL);
		while (camel_mime_parser_step (mp, &buf, &len) != HSCAN_MULTIPART_END) {
			camel_mime_parser_unstep (mp);
			bodypart = (CamelDataWrapper *)camel_mime_part_new ();
			camel_mime_part_construct_from_parser ((CamelMimePart *)bodypart, mp);
			camel_multipart_add_part ((CamelMultipart *)content, (CamelMimePart *)bodypart);
			camel_object_unref ((CamelObject *)bodypart);
		}
		
		/* these are only return valid data in the MULTIPART_END state */
		camel_multipart_set_preface ((CamelMultipart *)content, camel_mime_parser_preface (mp));
		camel_multipart_set_postface ((CamelMultipart *)content, camel_mime_parser_postface (mp));
		
		d(printf("Created multi-part\n"));
		break; }
	default:
		g_warning("Invalid state encountered???: %d", camel_mime_parser_state (mp));
	}
	if (content) {
		/* would you believe you have to set this BEFORE you set the content object???  oh my god !!!! */
		camel_data_wrapper_set_mime_type_field (content, 
							camel_mime_part_get_content_type ((CamelMimePart *)dw));
		camel_medium_set_content_object ((CamelMedium *)dw, content);
		camel_object_unref ((CamelObject *)content);
	}
}

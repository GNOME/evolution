/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-part-utils : Utility for mime parsing and so on
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *          Michael Zucchi <notzed@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <gal/util/e-iconv.h>

#include "string-utils.h"
#include "camel-charset-map.h"
#include "camel-mime-part-utils.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-multipart-signed.h"
#include "camel-multipart-encrypted.h"
#include "camel-seekable-substream.h"
#include "camel-stream-fs.h"
#include "camel-stream-filter.h"
#include "camel-stream-mem.h"
#include "camel-mime-filter-basic.h"
#include "camel-mime-filter-charset.h"
#include "camel-mime-filter-crlf.h"
#include "camel-mime-filter-save.h"
#include "camel-html-parser.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))
	       #include <stdio.h>*/

/* example: <META http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"> */

static const char *
check_html_charset(char *buffer, int length)
{
	CamelHTMLParser *hp;
	const char *charset = NULL;
	camel_html_parser_t state;
	struct _header_content_type *ct;

	/* if we need to first base64/qp decode, do this here, sigh */
	hp = camel_html_parser_new();
	camel_html_parser_set_data(hp, buffer, length, TRUE);
	
	do {
		const char *data;
		int len;
		const char *val;
		
		state = camel_html_parser_step(hp, &data, &len);
		
		/* example: <META http-equiv="Content-Type" content="text/html; charset=ISO-8859-1"> */
		
		switch(state) {
		case CAMEL_HTML_PARSER_ELEMENT:
			val = camel_html_parser_tag(hp);
			d(printf("Got tag: %s\n", val));
			if (strcasecmp(val, "meta") == 0
			    && (val = camel_html_parser_attr(hp, "http-equiv"))
			    && strcasecmp(val, "content-type") == 0
			    && (val = camel_html_parser_attr(hp, "content"))
			    && (ct = header_content_type_decode(val))) {
				charset = header_content_type_param(ct, "charset");
				charset = e_iconv_charset_name (charset);
				header_content_type_unref(ct);
			}
			break;
		default:
			/* ignore everything else */
			break;
		}
	} while (charset == NULL && state != CAMEL_HTML_PARSER_EOF);

	camel_object_unref (hp);

	return charset;
}

static GByteArray *
convert_buffer (GByteArray *in, const char *to, const char *from)
{
	size_t inleft, outleft, outlen, converted = 0;
	GByteArray *out = NULL;
	const char *inbuf;
	char *outbuf;
	iconv_t cd;
	
	if (in->len == 0)
		return g_byte_array_new();
	
	d(printf("converting buffer from %s to %s:\n", from, to));
	d(fwrite(in->data, 1, (int)in->len, stdout));
	d(printf("\n"));
	
	cd = e_iconv_open(to, from);
	if (cd == (iconv_t) -1) {
		g_warning ("Cannot convert from '%s' to '%s': %s", from, to, strerror (errno));
		return NULL;
	}
	
	outlen = in->len * 2 + 16;
	out = g_byte_array_new ();
	g_byte_array_set_size (out, outlen);
	
	inbuf = in->data;
	inleft = in->len;
	
	do {
		outbuf = out->data + converted;
		outleft = outlen - converted;
		
		converted = e_iconv (cd, &inbuf, &inleft, &outbuf, &outleft);
		if (converted == (size_t) -1) {
			if (errno != E2BIG && errno != EINVAL)
				goto fail;
		}
		
		/*
		 * E2BIG   There is not sufficient room at *outbuf.
		 *
		 * We just need to grow our outbuffer and try again.
		 */
		
		converted = outbuf - (char *)out->data;
		if (errno == E2BIG) {
			outlen += inleft * 2 + 16;
			out = g_byte_array_set_size (out, outlen);
			outbuf = out->data + converted;
		}
		
	} while (errno == E2BIG && inleft > 0);
	
	/*
	 * EINVAL  An  incomplete  multibyte sequence has been encoun­
	 *         tered in the input.
	 *
	 * We'll just have to ignore it...
	 */
	
	/* flush the iconv conversion */
	e_iconv (cd, NULL, NULL, &outbuf, &outleft);
	
	/* now set the true length on the GByteArray */
	converted = outbuf - (char *)out->data;
	g_byte_array_set_size (out, converted);
	
	d(printf("converted data:\n"));
	d(fwrite(out->data, 1, (int)out->len, stdout));
	d(printf("\n"));
	
	e_iconv_close (cd);
	
	return out;
	
 fail:
	g_warning ("Cannot convert from '%s' to '%s': %s", from, to, strerror (errno));
	
	g_byte_array_free (out, TRUE);
	
	e_iconv_close (cd);
	
	return NULL;
}

/* We don't really use the charset argument except for debugging... */
static gboolean
broken_windows_charset (GByteArray *buffer, const char *charset)
{
	register unsigned char *inptr;
	unsigned char *inend;
	
	inptr = buffer->data;
	inend = inptr + buffer->len;
	
	while (inptr < inend) {
		register unsigned char c = *inptr++;
		
		if (c >= 128 && c <= 159) {
			g_warning ("Encountered Windows charset parading as %s", charset);
			return TRUE;
		}
	}
	
	return FALSE;
}

static gboolean
is_7bit (GByteArray *buffer)
{
	register unsigned int i;
	
	for (i = 0; i < buffer->len; i++)
		if (buffer->data[i] > 127)
			return FALSE;
	
	return TRUE;
}

static const char *iso_charsets[] = {
	"us-ascii",
	"iso-8859-1",
	"iso-8859-2",
	"iso-8859-3",
	"iso-8859-4",
	"iso-8859-5",
	"iso-8859-6",
	"iso-8859-7",
	"iso-8859-8",
	"iso-8859-9",
	"iso-8859-10",
	"iso-8859-11",
	"iso-8859-12",
	"iso-8859-13",
	"iso-8859-14",
	"iso-8859-15",
	"iso-8859-16"
};

#define NUM_ISO_CHARSETS (sizeof (iso_charsets) / sizeof (iso_charsets[0]))

static const char *
canon_charset_name (const char *charset)
{
	const char *ptr;
	char *endptr;
	int iso;
	
	if (strncasecmp (charset, "iso", 3) != 0)
		return charset;
	
	ptr = charset + 3;
	if (*ptr == '-' || *ptr == '_')
		ptr++;
	
	/* if it's not an iso-8859-# charset, we don't care about it */
	if (strncmp (ptr, "8859", 4) != 0)
		return charset;
	
	ptr += 4;
	if (*ptr == '-' || *ptr == '_')
		ptr++;
	
	iso = strtoul (ptr, &endptr, 10);
	if (endptr == ptr || *endptr != '\0')
		return charset;
	
	if (iso >= NUM_ISO_CHARSETS)
		return charset;
	
	return iso_charsets[iso];
}

/* simple data wrapper */
static void
simple_data_wrapper_construct_from_parser (CamelDataWrapper *dw, CamelMimeParser *mp)
{
	CamelMimeFilter *fdec = NULL, *fcrlf = NULL;
	CamelMimeFilterBasicType enctype = 0;
	int len, decid = -1, crlfid = -1;
	struct _header_content_type *ct;
	const char *charset = NULL;
	char *encoding, *buf;
	GByteArray *buffer;
	CamelStream *mem;
	
	d(printf ("simple_data_wrapper_construct_from_parser()\n"));
	
	/* first, work out conversion, if any, required, we dont care about what we dont know about */
	encoding = header_content_encoding_decode (camel_mime_parser_header (mp, "Content-Transfer-Encoding", NULL));
	if (encoding) {
		if (!strcasecmp (encoding, "base64")) {
			d(printf("Adding base64 decoder ...\n"));
			enctype = CAMEL_MIME_FILTER_BASIC_BASE64_DEC;
		} else if (!strcasecmp (encoding, "quoted-printable")) {
			d(printf("Adding quoted-printable decoder ...\n"));
			enctype = CAMEL_MIME_FILTER_BASIC_QP_DEC;
		} else if (!strcasecmp (encoding, "x-uuencode")) {
			d(printf("Adding uudecoder ...\n"));
			enctype = CAMEL_MIME_FILTER_BASIC_UU_DEC;
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
		charset = e_iconv_charset_name (charset);
		
		if (fdec) {
			d(printf ("Adding CRLF conversion filter\n"));
			fcrlf = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE,
							    CAMEL_MIME_FILTER_CRLF_MODE_CRLF_ONLY);
			crlfid = camel_mime_parser_filter_add (mp, fcrlf);
		}
	}
	
	/* read in the entire content */
	buffer = g_byte_array_new ();
	while (camel_mime_parser_step (mp, &buf, &len) != HSCAN_BODY_END) {
		d(printf("appending o/p data: %d: %.*s\n", len, len, buf));
		g_byte_array_append (buffer, buf, len);
	}
	
	/* check for broken Outlook/Web mailers that like to send html marked as text/plain */
	if (header_content_type_is (ct, "text", "plain")) {
		register const unsigned char *inptr;
		const unsigned char *inend;
		
		inptr = buffer->data;
		inend = inptr + buffer->len;
		
		while (inptr < inend && isspace ((int) *inptr))
			inptr++;

		if (((inend-inptr) > 5 && g_ascii_strncasecmp(inptr, "<html", 5) == 0)
		    || ((inend-inptr) > 9 && g_ascii_strncasecmp(inptr, "<!doctype", 9) == 0)) {
			/* re-tag as text/html */
			g_free (ct->subtype);
			ct->subtype = g_strdup ("html");
		}
	}
	
	/* Possible Lame Mailer Alert... check the META tags for a charset */
	if (!charset && header_content_type_is (ct, "text", "html")) {
		if ((charset = check_html_charset (buffer->data, buffer->len)))
			header_content_type_set_param (ct, "charset", charset);
	}
	
	/* if we need to do charset conversion, see if we can/it works/etc */
	if (charset && !(strcasecmp (charset, "us-ascii") == 0
			 || strcasecmp (charset, "utf-8") == 0
			 || strncasecmp (charset, "x-", 2) == 0)) {
		GByteArray *out;
		
		/* You often see Microsoft Windows users announcing their texts
		 * as being in ISO-8859-1 even when in fact they contain funny
		 * characters from the Windows-CP1252 superset.
		 */
		charset = canon_charset_name (charset);
		if (!strncasecmp (charset, "iso-8859", 8)) {
			/* check for Windows-specific chars... */
			if (broken_windows_charset (buffer, charset))
				charset = camel_charset_iso_to_windows (charset);
		}
		
		out = convert_buffer (buffer, "UTF-8", charset);
		if (out) {
			/* converted ok, use this data instead */
			g_byte_array_free(buffer, TRUE);
			buffer = out;
		} else {
			/* else failed to convert, leave as raw? */
			g_warning("Storing text as raw, unknown charset '%s' or invalid format", charset);
			dw->rawtext = TRUE;
		}
	} else if (header_content_type_is (ct, "text", "*")) {
		if (charset == NULL || !strcasecmp (charset, "us-ascii")) {
			/* check that it's 7bit */
			dw->rawtext = !is_7bit (buffer);
		} else if (!strncasecmp (charset, "x-", 2)) {
			/* we're not even going to bother trying to convert, so set the
			   rawtext bit to TRUE and let the mailer deal with it. */
			dw->rawtext = TRUE;
		} else if (!strcasecmp (charset, "utf-8") && buffer->len) {
			/* check that it is valid utf8 */
			dw->rawtext = !g_utf8_validate (buffer->data, buffer->len, NULL);
		}
	}
	
	d(printf("message part kept in memory!\n"));
	
	mem = camel_stream_mem_new_with_byte_array(buffer);
	camel_data_wrapper_construct_from_stream(dw, mem);
	camel_object_unref((CamelObject *)mem);

	camel_mime_parser_filter_remove(mp, decid);
	camel_mime_parser_filter_remove(mp, crlfid);
	
	if (fdec)
		camel_object_unref((CamelObject *)fdec);
	if (fcrlf)
		camel_object_unref((CamelObject *)fcrlf);
}

/* This replaces the data wrapper repository ... and/or could be replaced by it? */
void
camel_mime_part_construct_content_from_parser (CamelMimePart *dw, CamelMimeParser *mp)
{
	CamelDataWrapper *content = NULL;
	CamelContentType *ct;
	
	ct = camel_mime_parser_content_type (mp);

	switch (camel_mime_parser_state (mp)) {
	case HSCAN_HEADER:
		d(printf("Creating body part\n"));
		/* multipart/signed is some fucked up type that we must treat as binary data, fun huh, idiots. */
		if (header_content_type_is (ct, "multipart", "signed")) {
			content = (CamelDataWrapper *) camel_multipart_signed_new ();
			camel_multipart_construct_from_parser ((CamelMultipart *) content, mp);
		} else {
			content = camel_data_wrapper_new ();
			simple_data_wrapper_construct_from_parser (content, mp);
		}
		break;
	case HSCAN_MESSAGE:
		d(printf("Creating message part\n"));
		content = (CamelDataWrapper *) camel_mime_message_new ();
		camel_mime_part_construct_from_parser ((CamelMimePart *)content, mp);
		break;
	case HSCAN_MULTIPART:
		d(printf("Creating multi-part\n"));
		if (header_content_type_is (ct, "multipart", "encrypted"))
			content = (CamelDataWrapper *) camel_multipart_encrypted_new ();
		else if (header_content_type_is (ct, "multipart", "signed"))
			content = (CamelDataWrapper *) camel_multipart_signed_new ();
		else
			content = (CamelDataWrapper *) camel_multipart_new ();
		
		camel_multipart_construct_from_parser((CamelMultipart *)content, mp);
		d(printf("Created multi-part\n"));
		break;
	default:
		g_warning("Invalid state encountered???: %d", camel_mime_parser_state (mp));
	}
	if (content) {
		/* would you believe you have to set this BEFORE you set the content object???  oh my god !!!! */
		camel_data_wrapper_set_mime_type_field (content, camel_mime_part_get_content_type (dw));
		camel_medium_set_content_object ((CamelMedium *)dw, content);
		
		/* Note: we don't set ct as the content-object's mime-type above because
		 * camel_medium_set_content_object() may re-write the Content-Type header
		 * (see CamelMimePart::set_content_object) if we did that (which is a Bad Thing).
		 * However, if we set it *afterward*, we can still use any special auto-detections
		 * that we found in simple_data_wrapper_construct_from_parser(). This is important
		 * later when we go to render the MIME parts in mail-format.c */
		camel_data_wrapper_set_mime_type_field (content, ct);
		
		camel_object_unref (content);
	}
}

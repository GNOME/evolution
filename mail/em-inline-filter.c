/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-inline-filter.h"
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>

#define d(x) 

static void em_inline_filter_class_init (EMInlineFilterClass *klass);
static void em_inline_filter_init (CamelObject *object);
static void em_inline_filter_finalize (CamelObject *object);

static void emif_filter(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace);
static void emif_complete(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace);
static void emif_reset(CamelMimeFilter *f);

static CamelMimeFilterClass *parent_class = NULL;

CamelType
em_inline_filter_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		parent_class = (CamelMimeFilterClass *)camel_mime_filter_get_type();

		type = camel_type_register(camel_mime_filter_get_type(),
					   "EMInlineFilter",
					   sizeof (EMInlineFilter),
					   sizeof (EMInlineFilterClass),
					   (CamelObjectClassInitFunc) em_inline_filter_class_init,
					   NULL,
					   (CamelObjectInitFunc) em_inline_filter_init,
					   (CamelObjectFinalizeFunc) em_inline_filter_finalize);
	}
	
	return type;
}

static void
em_inline_filter_class_init (EMInlineFilterClass *klass)
{
	((CamelMimeFilterClass *)klass)->filter = emif_filter;
	((CamelMimeFilterClass *)klass)->complete = emif_complete;
	((CamelMimeFilterClass *)klass)->reset = emif_reset;
}

static void
em_inline_filter_init (CamelObject *object)
{
	EMInlineFilter *emif = (EMInlineFilter *)object;

	emif->data = g_byte_array_new();
}


static void
em_inline_filter_finalize (CamelObject *object)
{
	EMInlineFilter *emif = (EMInlineFilter *)object;

	if (emif->base_type)
		camel_content_type_unref(emif->base_type);

	emif_reset((CamelMimeFilter *)emif);
	g_byte_array_free(emif->data, TRUE);
	g_free(emif->filename);
}

enum {
	EMIF_PLAIN,
	EMIF_UUENC,
	EMIF_BINHEX,
	EMIF_POSTSCRIPT,
	EMIF_PGPSIGNED,
};
const struct {
	const char *name;
	CamelTransferEncoding type;
	int plain:1;
} emif_types[] = {
	{ "text/plain", CAMEL_TRANSFER_ENCODING_DEFAULT, 1, },
	{ "application/octet-stream", CAMEL_TRANSFER_ENCODING_UUENCODE, },
	{ "application/mac-binhex40", CAMEL_TRANSFER_ENCODING_7BIT, },
	{ "application/postscript", CAMEL_TRANSFER_ENCODING_7BIT, },
	{ "text/plain", CAMEL_TRANSFER_ENCODING_7BIT, 1, },
};

static void
emif_add_part(EMInlineFilter *emif, const char *data, int len)
{
	CamelTransferEncoding type;
	CamelStream *mem;
	CamelDataWrapper *dw;
	CamelMimePart *part;

	if (emif->state == EMIF_PLAIN)
		type = emif->base_encoding;
	else
		type = emif_types[emif->state].type;

	g_byte_array_append(emif->data, data, len);
	mem = camel_stream_mem_new_with_byte_array(emif->data);
	emif->data = g_byte_array_new();

	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, mem);
	camel_object_unref(mem);
	if (emif_types[emif->state].plain && emif->base_type)
		camel_data_wrapper_set_mime_type_field(dw, emif->base_type);
	else
		camel_data_wrapper_set_mime_type(dw, emif_types[emif->state].name);
	dw->encoding = type;

	part = camel_mime_part_new();
	camel_medium_set_content_object((CamelMedium *)part, dw);
	camel_mime_part_set_encoding(part, type);
	camel_object_unref(dw);

	if (emif->filename) {
		camel_mime_part_set_filename(part, emif->filename);
		g_free(emif->filename);
		emif->filename = NULL;
	}

	emif->parts = g_slist_append(emif->parts, part);
}

static int
emif_scan(CamelMimeFilter *f, char *in, size_t len, int final)
{
	EMInlineFilter *emif = (EMInlineFilter *)f;
	char *inptr = in, *inend = in+len;
	char *data_start = in;
	char *start = in;

	while (inptr < inend) {
		start = inptr;

		while (inptr < inend && *inptr != '\n')
			inptr++;
			
		if (inptr == inend) {
			if (!final) {
				camel_mime_filter_backup(f, start, inend-start);
				inend = start;
			}
			break;
		}

		*inptr++ = 0;

		switch(emif->state) {
		case EMIF_PLAIN:
			/* This could use some funky plugin shit, but this'll do for now */
			if (strncmp(start, "begin ", 6) == 0
			    && start[6] >= '0' && start[6] <= '7') {
				int i = 7;

				while (start[i] >='0' && start[i] <='7')
					i++;

				inptr[-1] = '\n';

				if (start[i++] != ' ')
					break;

				emif_add_part(emif, data_start, start-data_start);
				emif->filename = g_strndup(start+i, inptr-start-i-1);
				data_start = start;
				emif->state = EMIF_UUENC;
			} else if (strncmp(start, "(This file must be converted with BinHex 4.0)", 45) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_BINHEX;
			} else if (strncmp(start, "%!PS-Adobe-", 11) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_POSTSCRIPT;
#if 0
/* This should be hooked in once someone can work out how to handle it.
   Maybe we need a multipart_gpg_inline_signed or some crap, if it
   can't be converted to a real multipart/signed */
			} else if (strncmp(start, "-----BEGIN PGP SIGNED MESSAGE-----", 34) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_PGPSIGNED;
#endif
			}
			break;
		case EMIF_UUENC:
			if (strcmp(start, "end") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			} else {
				int len, linelen;

				/* check the length byte matches the data, if not, output what we have and re-scan this line */
				len = ((start[0] - ' ') & 077);
				linelen = inptr-start-1;
				while (linelen > 0 && (start[linelen] == '\r' || start[linelen] == '\n'))
					linelen--;
				linelen--;
				linelen /= 4;
				linelen *= 3;
				if (!(len == linelen || len == linelen-1 || len == linelen-2)) {
					inptr[-1] = '\n';
					emif_add_part(emif, data_start, start-data_start);
					data_start = start;
					inptr = start;
					emif->state = EMIF_PLAIN;
					continue;
				}
			}
			break;
		case EMIF_BINHEX:
			if (inptr > (start+1) && inptr[-2] == ':') {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_POSTSCRIPT:
			if (strcmp(start, "%%EOF") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_PGPSIGNED:
			/* This is currently a noop - it just turns it into a text part */
			if (strcmp(start, "-----END PGP SIGNATURE-----") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		}

		inptr[-1] = '\n';
	}

	if (final) {
		emif_add_part(emif, data_start, inend-data_start);
	} else {
		g_byte_array_append(emif->data, data_start, inend-data_start);
	}

	return 0;
}

static void
emif_filter(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	emif_scan(f, in, len, FALSE);

	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
emif_complete(CamelMimeFilter *f, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	emif_scan(f, in, len, TRUE);

	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
emif_reset(CamelMimeFilter *f)
{
	EMInlineFilter *emif = (EMInlineFilter *)f;
	GSList *l;

	l = emif->parts;
	while (l) {
		GSList *n = l->next;

		camel_object_unref(l->data);
		g_slist_free_1(l);

		l = n;
	}
	emif->parts = NULL;
	g_byte_array_set_size(emif->data, 0);
}

/**
 * em_inline_filter_new:
 * @base_encoding: The base transfer-encoding of the
 * raw data being processed.
 * @base_type: The base content-type of the raw data, should always be
 * text/plain.
 * 
 * Create a filter which will scan a (text) stream for
 * embedded parts.  You can then retrieve the contents
 * as a CamelMultipart object.
 * 
 * Return value: 
 **/
EMInlineFilter *
em_inline_filter_new(CamelTransferEncoding base_encoding, CamelContentType *base_type)
{
	EMInlineFilter *emif;

	emif = (EMInlineFilter *)camel_object_new(em_inline_filter_get_type());
	emif->base_encoding = base_encoding;
	if (base_type) {
		emif->base_type = base_type;
		camel_content_type_ref(emif->base_type);
	}

	return emif;
}

CamelMultipart *
em_inline_filter_get_multipart(EMInlineFilter *emif)
{
	GSList *l = emif->parts;
	CamelMultipart *mp;

	mp = camel_multipart_new();
	while (l) {
		camel_multipart_add_part(mp, l->data);
		l = l->next;
	}

	return mp;
}

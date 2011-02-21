/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-inline-filter.h"

#include "em-utils.h"
#include "em-format/em-format.h"

#define d(x)

G_DEFINE_TYPE (EMInlineFilter, em_inline_filter, CAMEL_TYPE_MIME_FILTER)

enum {
	EMIF_PLAIN,
	EMIF_BINHEX,
	EMIF_POSTSCRIPT,
	EMIF_PGPSIGNED,
	EMIF_PGPENCRYPTED
};

static const struct {
	const gchar *type;
	const gchar *subtype;
	CamelTransferEncoding encoding;
	guint plain:1;
} emif_types[] = {
	{ "text",        "plain",                 CAMEL_TRANSFER_ENCODING_DEFAULT,  1, },
	{ "application", "mac-binhex40",          CAMEL_TRANSFER_ENCODING_7BIT,     0, },
	{ "application", "postscript",            CAMEL_TRANSFER_ENCODING_7BIT,     0, },
	{ "application", "x-inlinepgp-signed",    CAMEL_TRANSFER_ENCODING_DEFAULT,  0, },
	{ "application", "x-inlinepgp-encrypted", CAMEL_TRANSFER_ENCODING_DEFAULT,  0, },
};

static void
inline_filter_add_part(EMInlineFilter *emif, const gchar *data, gint len)
{
	CamelTransferEncoding encoding;
	CamelContentType *content_type;
	CamelDataWrapper *dw;
	const gchar *mimetype;
	CamelMimePart *part;
	CamelStream *mem;
	gchar *type;

	if (emif->state == EMIF_PLAIN || emif->state == EMIF_PGPSIGNED || emif->state == EMIF_PGPENCRYPTED)
		encoding = emif->base_encoding;
	else
		encoding = emif_types[emif->state].encoding;

	g_byte_array_append(emif->data, (guchar *)data, len);
	/* check the part will actually have content */
	if (emif->data->len <= 0) {
		return;
	}

	mem = camel_stream_mem_new_with_byte_array (emif->data);
	emif->data = g_byte_array_new();

	dw = camel_data_wrapper_new();
	if (encoding == emif->base_encoding && (encoding == CAMEL_TRANSFER_ENCODING_BASE64 || encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE)) {
		CamelMimeFilter *enc_filter = camel_mime_filter_basic_new (encoding == CAMEL_TRANSFER_ENCODING_BASE64 ? CAMEL_MIME_FILTER_BASIC_BASE64_ENC : CAMEL_MIME_FILTER_BASIC_QP_ENC);
		CamelStream *filter_stream;

		filter_stream = camel_stream_filter_new (mem);
		camel_stream_filter_add (CAMEL_STREAM_FILTER (filter_stream), enc_filter);

		/* properly encode content */
		camel_data_wrapper_construct_from_stream (dw, filter_stream, NULL);

		g_object_unref (enc_filter);
		g_object_unref (filter_stream);
	} else {
		camel_data_wrapper_construct_from_stream (dw, mem, NULL);
	}
	g_object_unref (mem);

	if (emif_types[emif->state].plain && emif->base_type) {
		/* create a copy */
		type = camel_content_type_format (emif->base_type);
		content_type = camel_content_type_decode (type);
		g_free (type);
	} else {
		/* we want to preserve all params */
		type = camel_content_type_format (emif->base_type);
		content_type = camel_content_type_decode (type);
		g_free (type);

		g_free (content_type->type);
		g_free (content_type->subtype);
		content_type->type = g_strdup (emif_types[emif->state].type);
		content_type->subtype = g_strdup (emif_types[emif->state].subtype);
	}

	camel_data_wrapper_set_mime_type_field (dw, content_type);
	camel_content_type_unref (content_type);
	dw->encoding = encoding;

	part = camel_mime_part_new();
	camel_medium_set_content ((CamelMedium *)part, dw);
	camel_mime_part_set_encoding(part, encoding);
	g_object_unref (dw);

	if (emif->filename)
		camel_mime_part_set_filename(part, emif->filename);

	/* pre-snoop the mime type of unknown objects, and poke and hack it into place */
	if (camel_content_type_is(dw->mime_type, "application", "octet-stream")
	    && (mimetype = em_format_snoop_type(part))
	    && strcmp(mimetype, "application/octet-stream") != 0) {
		camel_data_wrapper_set_mime_type(dw, mimetype);
		camel_mime_part_set_content_type(part, mimetype);
		if (emif->filename)
			camel_mime_part_set_filename(part, emif->filename);
	}

	g_free(emif->filename);
	emif->filename = NULL;

	emif->parts = g_slist_append(emif->parts, part);
}

static gint
inline_filter_scan(CamelMimeFilter *f, gchar *in, gsize len, gint final)
{
	EMInlineFilter *emif = (EMInlineFilter *)f;
	gchar *inptr = in, *inend = in+len;
	gchar *data_start = in;
	gchar *start = in;

	while (inptr < inend) {
		gint rest_len;
		gboolean set_null_byte = FALSE;

		start = inptr;

		while (inptr < inend && *inptr != '\n')
			inptr++;

		if (inptr == inend && start == inptr) {
			if (!final) {
				camel_mime_filter_backup(f, start, inend-start);
				inend = start;
			}
			break;
		}

		rest_len = inend - start;
		if (inptr < inend) {
			*inptr++ = 0;
			set_null_byte = TRUE;
		}

		#define restore_inptr() G_STMT_START { if (set_null_byte) inptr[-1] = '\n'; } G_STMT_END

		switch (emif->state) {
		case EMIF_PLAIN:
			if (rest_len >= 45 && strncmp (start, "(This file must be converted with BinHex 4.0)", 45) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_BINHEX;
			} else if (rest_len >= 11 && strncmp (start, "%!PS-Adobe-", 11) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_POSTSCRIPT;
			} else if (rest_len >= 34 && strncmp (start, "-----BEGIN PGP SIGNED MESSAGE-----", 34) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_PGPSIGNED;
			} else if (rest_len >= 27 && strncmp (start, "-----BEGIN PGP MESSAGE-----", 27) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_PGPENCRYPTED;
			}

			break;
		case EMIF_BINHEX:
			if (inptr > (start+1) && inptr[-2] == ':') {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_POSTSCRIPT:
			if (rest_len >= 5 && strncmp (start, "%%EOF", 5) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_PGPSIGNED:
			if (rest_len >= 27 && strncmp (start, "-----END PGP SIGNATURE-----", 27) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_PGPENCRYPTED:
			if (rest_len >= 25 && strncmp (start, "-----END PGP MESSAGE-----", 25) == 0) {
				restore_inptr ();
				inline_filter_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		}

		restore_inptr ();

		#undef restore_inptr
	}

	if (final) {
		/* always stop as plain, especially when not read those tags fully */
		emif->state = EMIF_PLAIN;

		inline_filter_add_part (emif, data_start, inend - data_start);
	} else if (start > data_start) {
		/* backup the last line, in case the tag is divided within buffers */
		camel_mime_filter_backup (f, start, inend - start);
		g_byte_array_append (emif->data, (guchar *)data_start, start - data_start);
	} else {
		g_byte_array_append (emif->data, (guchar *)data_start, inend - data_start);
	}

	return 0;
}

static void
inline_filter_finalize (GObject *object)
{
	EMInlineFilter *emif = EM_INLINE_FILTER (object);

	if (emif->base_type)
		camel_content_type_unref(emif->base_type);

	camel_mime_filter_reset (CAMEL_MIME_FILTER (object));
	g_byte_array_free(emif->data, TRUE);
	g_free(emif->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_inline_filter_parent_class)->finalize (object);
}

static void
inline_filter_filter (CamelMimeFilter *filter,
                      const gchar *in,
                      gsize len,
                      gsize prespace,
                      gchar **out,
                      gsize *outlen,
                      gsize *outprespace)
{
	inline_filter_scan (filter, (gchar *)in, len, FALSE);

	*out = (gchar *)in;
	*outlen = len;
	*outprespace = prespace;
}

static void
inline_filter_complete (CamelMimeFilter *filter,
                        const gchar *in,
                        gsize len,
                        gsize prespace,
                        gchar **out,
                        gsize *outlen,
                        gsize *outprespace)
{
	inline_filter_scan (filter, (gchar *)in, len, TRUE);

	*out = (gchar *)in;
	*outlen = len;
	*outprespace = prespace;
}

static void
inline_filter_reset (CamelMimeFilter *filter)
{
	EMInlineFilter *emif = EM_INLINE_FILTER (filter);
	GSList *l;

	l = emif->parts;
	while (l) {
		GSList *n = l->next;

		g_object_unref (l->data);
		g_slist_free_1(l);

		l = n;
	}
	emif->parts = NULL;
	g_byte_array_set_size(emif->data, 0);
}

static void
em_inline_filter_class_init (EMInlineFilterClass *class)
{
	GObjectClass *object_class;
	CamelMimeFilterClass *mime_filter_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = inline_filter_finalize;

	mime_filter_class = CAMEL_MIME_FILTER_CLASS (class);
	mime_filter_class->filter = inline_filter_filter;
	mime_filter_class->complete = inline_filter_complete;
	mime_filter_class->reset = inline_filter_reset;
}

static void
em_inline_filter_init (EMInlineFilter *emif)
{
	emif->data = g_byte_array_new();
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

	emif = g_object_new (EM_TYPE_INLINE_FILTER, NULL);
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

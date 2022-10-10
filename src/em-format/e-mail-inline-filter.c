/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include "e-mail-inline-filter.h"
#include "e-mail-part-utils.h"

#define d(x)

G_DEFINE_TYPE (EMailInlineFilter, e_mail_inline_filter, CAMEL_TYPE_MIME_FILTER)

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
	guint plain : 1;
} emif_types[] = {
	{ "text", "plain",
	  CAMEL_TRANSFER_ENCODING_DEFAULT, 1 },

	{ "application", "mac-binhex40",
	  CAMEL_TRANSFER_ENCODING_7BIT, 0 },

	{ "application", "postscript",
	  CAMEL_TRANSFER_ENCODING_7BIT, 0 },

	{ "application", "x-inlinepgp-signed",
	  CAMEL_TRANSFER_ENCODING_DEFAULT, 0 },

	{ "application", "x-inlinepgp-encrypted",
	  CAMEL_TRANSFER_ENCODING_DEFAULT, 0 }
};

static CamelMimePart *
construct_part_from_stream (CamelStream *mem,
                            const GByteArray *data)
{
	CamelMimePart *part = NULL;
	CamelMimeParser *parser;

	g_return_val_if_fail (mem != NULL, NULL);
	g_return_val_if_fail (data != NULL, NULL);

	if (data->len <= 13 || g_ascii_strncasecmp ((const gchar *) data->data, "Content-Type:", 13) != 0)
		return NULL;

	parser = camel_mime_parser_new ();
	camel_mime_parser_scan_from (parser, FALSE);
	camel_mime_parser_scan_pre_from (parser, FALSE);

	if (camel_mime_parser_init_with_stream (parser, mem, NULL) != -1) {
		part = camel_mime_part_new ();
		if (!camel_mime_part_construct_from_parser_sync (part, parser, NULL, NULL)) {
			g_object_unref (part);
			part = NULL;
		}
	}

	g_object_unref (parser);

	return part;
}

static void
inline_filter_add_part (EMailInlineFilter *emif,
                        const gchar *data,
                        gint len)
{
	CamelTransferEncoding encoding;
	CamelContentType *content_type;
	CamelDataWrapper *dw;
	CamelMimePart *part;
	CamelStream *mem;
	gchar *type;

	if (emif->state == EMIF_PLAIN || emif->state == EMIF_PGPSIGNED || emif->state == EMIF_PGPENCRYPTED)
		encoding = emif->base_encoding;
	else
		encoding = emif_types[emif->state].encoding;

	g_byte_array_append (emif->data, (guchar *) data, len);
	/* check the part will actually have content */
	if (emif->data->len <= 0) {
		return;
	}

	mem = camel_stream_mem_new_with_byte_array (emif->data);
	part = construct_part_from_stream (mem, emif->data);
	if (part) {
		g_object_unref (mem);
		emif->data = g_byte_array_new ();
		g_free (emif->filename);
		emif->filename = NULL;

		emif->parts = g_slist_append (emif->parts, part);
		emif->found_any = TRUE;

		return;
	}

	emif->data = g_byte_array_new ();
	g_seekable_seek (G_SEEKABLE (mem), 0, G_SEEK_SET, NULL, NULL);

	dw = camel_data_wrapper_new ();
	if (encoding == emif->base_encoding && (encoding == CAMEL_TRANSFER_ENCODING_BASE64 || encoding == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE)) {
		CamelMimeFilter *enc_filter = camel_mime_filter_basic_new (encoding == CAMEL_TRANSFER_ENCODING_BASE64 ? CAMEL_MIME_FILTER_BASIC_BASE64_ENC : CAMEL_MIME_FILTER_BASIC_QP_ENC);
		CamelStream *filter_stream;

		filter_stream = camel_stream_filter_new (mem);
		camel_stream_filter_add (CAMEL_STREAM_FILTER (filter_stream), enc_filter);

		/* properly encode content */
		camel_data_wrapper_construct_from_stream_sync (
			dw, filter_stream, NULL, NULL);

		g_object_unref (enc_filter);
		g_object_unref (filter_stream);
	} else {
		camel_data_wrapper_construct_from_stream_sync (
			dw, mem, NULL, NULL);
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

	camel_data_wrapper_take_mime_type_field (dw, content_type);
	camel_data_wrapper_set_encoding (dw, encoding);

	part = camel_mime_part_new ();
	camel_medium_set_content ((CamelMedium *) part, dw);
	camel_mime_part_set_encoding (part, encoding);
	g_object_unref (dw);

	if (emif->filename)
		camel_mime_part_set_filename (part, emif->filename);

	/* pre-snoop the mime type of unknown objects, and poke and hack it into place */
	if (camel_content_type_is (camel_data_wrapper_get_mime_type_field (dw), "application", "octet-stream")) {
		gchar *guessed_mime_type;

		guessed_mime_type = e_mail_part_guess_mime_type (part);

		if (guessed_mime_type &&
		    strcmp (guessed_mime_type, "application/octet-stream") != 0) {
			camel_data_wrapper_set_mime_type (dw, guessed_mime_type);
			camel_mime_part_set_content_type (part, guessed_mime_type);
			if (emif->filename)
				camel_mime_part_set_filename (part, emif->filename);
		}

		g_free (guessed_mime_type);
	}

	g_free (emif->filename);
	emif->filename = NULL;

	emif->parts = g_slist_append (emif->parts, part);
}

static gboolean
newline_or_whitespace_follows (const gchar *str,
                               guint len,
                               guint skip_first)
{
	if (len <= skip_first)
		return len == skip_first;

	str += skip_first;
	len -= skip_first;

	while (len > 0 && *str != '\n') {
		if (!*str)
			return TRUE;

		if (!camel_mime_is_lwsp (*str))
			return FALSE;

		len--;
		str++;
	}

	return len == 0 || *str == '\n';
}

static gint
inline_filter_scan (CamelMimeFilter *f,
                    gchar *in,
                    gsize len,
                    gint final)
{
	EMailInlineFilter *emif = (EMailInlineFilter *) f;
	gchar *inptr = in, *inend = in + len;
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
				camel_mime_filter_backup (f, start, inend - start);
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
				inline_filter_add_part (emif, data_start, start - data_start);
				data_start = start;
				emif->state = EMIF_BINHEX;
			} else if (rest_len >= 11 && strncmp (start, "%!PS-Adobe-", 11) == 0) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, start - data_start);
				data_start = start;
				emif->state = EMIF_POSTSCRIPT;
			} else if (rest_len >= 34 && strncmp (start, "-----BEGIN PGP SIGNED MESSAGE-----", 34) == 0 &&
				   newline_or_whitespace_follows (start, rest_len, 34)) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, start - data_start);
				data_start = start;
				emif->state = EMIF_PGPSIGNED;
			} else if (rest_len >= 27 && strncmp (start, "-----BEGIN PGP MESSAGE-----", 27) == 0 &&
				   newline_or_whitespace_follows (start, rest_len, 27)) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, start - data_start);
				data_start = start;
				emif->state = EMIF_PGPENCRYPTED;
			}

			break;
		case EMIF_BINHEX:
			if (inptr > (start + 1) && inptr[-2] == ':') {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, inptr - data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
				emif->found_any = TRUE;
			}
			break;
		case EMIF_POSTSCRIPT:
			if (rest_len >= 5 && strncmp (start, "%%EOF", 5) == 0) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, inptr - data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
				emif->found_any = TRUE;
			}
			break;
		case EMIF_PGPSIGNED:
			if (rest_len >= 27 && strncmp (start, "-----END PGP SIGNATURE-----", 27) == 0 &&
			    newline_or_whitespace_follows (start, rest_len, 27)) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, inptr - data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
				emif->found_any = TRUE;
			}
			break;
		case EMIF_PGPENCRYPTED:
			if (rest_len >= 25 && strncmp (start, "-----END PGP MESSAGE-----", 25) == 0 &&
			    newline_or_whitespace_follows (start, rest_len, 25)) {
				restore_inptr ();
				inline_filter_add_part (emif, data_start, inptr - data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
				emif->found_any = TRUE;
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
		g_byte_array_append (emif->data, (guchar *) data_start, start - data_start);
	} else {
		g_byte_array_append (emif->data, (guchar *) data_start, inend - data_start);
	}

	return 0;
}

static void
inline_filter_finalize (GObject *object)
{
	EMailInlineFilter *emif = E_MAIL_INLINE_FILTER (object);

	if (emif->base_type)
		camel_content_type_unref (emif->base_type);

	camel_mime_filter_reset (CAMEL_MIME_FILTER (object));
	g_byte_array_free (emif->data, TRUE);
	g_free (emif->filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_inline_filter_parent_class)->finalize (object);
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
	inline_filter_scan (filter, (gchar *) in, len, FALSE);

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
	inline_filter_scan (filter, (gchar *) in, len, TRUE);

	*out = (gchar *)in;
	*outlen = len;
	*outprespace = prespace;
}

static void
inline_filter_reset (CamelMimeFilter *filter)
{
	EMailInlineFilter *emif = E_MAIL_INLINE_FILTER (filter);
	GSList *l;

	l = emif->parts;
	while (l) {
		GSList *n = l->next;

		g_object_unref (l->data);
		g_slist_free_1 (l);

		l = n;
	}
	emif->parts = NULL;
	g_byte_array_set_size (emif->data, 0);
	emif->found_any = FALSE;
}

static void
e_mail_inline_filter_class_init (EMailInlineFilterClass *class)
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
e_mail_inline_filter_init (EMailInlineFilter *emif)
{
	emif->data = g_byte_array_new ();
	emif->found_any = FALSE;
}

/**
 * em_inline_filter_new:
 * @base_encoding: The base transfer-encoding of the
 * raw data being processed.
 * @base_type: The base content-type of the raw data, should always be
 * text/plain.
 * @filename: Filename of the part, or NULL
 *
 * Create a filter which will scan a (text) stream for
 * embedded parts.  You can then retrieve the contents
 * as a CamelMultipart object.
 *
 * Return value:
 **/
EMailInlineFilter *
e_mail_inline_filter_new (CamelTransferEncoding base_encoding,
                          CamelContentType *base_type,
                          const gchar *filename)
{
	EMailInlineFilter *emif;

	emif = g_object_new (E_TYPE_MAIL_INLINE_FILTER, NULL);
	emif->base_encoding = base_encoding;
	if (base_type) {
		emif->base_type = base_type;
		camel_content_type_ref (emif->base_type);
	}

	if (filename && *filename)
		emif->filename = g_strdup (filename);

	return emif;
}

CamelMultipart *
e_mail_inline_filter_get_multipart (EMailInlineFilter *emif)
{
	GSList *l = emif->parts;
	CamelMultipart *mp;

	mp = camel_multipart_new ();
	while (l) {
		camel_multipart_add_part (mp, l->data);
		l = l->next;
	}

	return mp;
}

gboolean
e_mail_inline_filter_found_any (EMailInlineFilter *emif)
{
	g_return_val_if_fail (emif != NULL, FALSE);

	return emif->found_any;
}

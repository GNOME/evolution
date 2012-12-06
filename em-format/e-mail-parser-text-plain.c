/*
 * e-mail-parser-text-plain.c
 *
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-format-extensions.h"

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-inline-filter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>
#include <ctype.h>

typedef struct _EMailParserTextPlain {
	GObject parent;
} EMailParserTextPlain;

typedef struct _EMailParserTextPlainClass {
	GObjectClass parent_class;
} EMailParserTextPlainClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserTextPlain,
	e_mail_parser_text_plain,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = {
	"text/plain",
	"text/*",
	NULL
};

static gboolean
part_is_empty (CamelMimePart *part)
{
	CamelDataWrapper *dw;
	GByteArray *ba;
	guint i;

	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	ba = camel_data_wrapper_get_byte_array (dw);

	if (!ba)
		return TRUE;

	for (i = 0; i < ba->len; i++) {

		/* Checks for \n, \t, \f, \r, \v and space */
		if (!isspace (ba->data[i]))
			return FALSE;
	}

	return TRUE;
}

static gboolean
process_part (EMailParser *parser,
              GString *part_id,
              gint part_number,
              CamelMimePart *part,
              gboolean is_attachment,
              GCancellable *cancellable,
              GQueue *out_mail_parts)
{
	CamelContentType *type;
	EMailPart *empart;
	gint s_len = part_id->len;

	if (part_is_empty (part))
		return TRUE;

	type = camel_mime_part_get_content_type (part);
	if (!camel_content_type_is (type, "text", "*")) {
		e_mail_parser_parse_part (
			parser, CAMEL_MIME_PART (part), part_id,
			cancellable, out_mail_parts);

	} else if (!camel_content_type_is (type, "text", "calendar")) {
		GQueue work_queue = G_QUEUE_INIT;

		g_string_append_printf (part_id, ".plain_text.%d", part_number);

		empart = e_mail_part_new (part, part_id->str);
		empart->mime_type = camel_content_type_simple (type);

		g_string_truncate (part_id, s_len);

		g_queue_push_tail (&work_queue, empart);

		if (is_attachment)
			e_mail_parser_wrap_as_attachment (
				parser, part, part_id, &work_queue);

		e_queue_transfer (&work_queue, out_mail_parts);

	} else {
		g_string_append_printf (part_id, ".inline.%d", part_number);

		e_mail_parser_parse_part (
			parser, CAMEL_MIME_PART (part), part_id,
			cancellable, out_mail_parts);

		g_string_truncate (part_id, s_len);
	}

	return TRUE;
}

static gboolean
empe_text_plain_parse (EMailParserExtension *extension,
                       EMailParser *parser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable,
                       GQueue *out_mail_parts)
{
	CamelStream *filtered_stream, *null;
	CamelMultipart *mp;
	CamelDataWrapper *dw;
	CamelContentType *type;
	gint i, count;
	EMailInlineFilter *inline_filter;
	gboolean charset_added = FALSE;
	const gchar *snoop_type = NULL;
	gboolean is_attachment;
	gint n_parts_added = 0;

	dw = camel_medium_get_content ((CamelMedium *) part);
	if (!dw)
		return FALSE;

	/* This scans the text part for inline-encoded data, creates
	 * a multipart of all the parts inside it. */

	/* FIXME: We should discard this multipart if it only contains
	 * the original text, but it makes this hash lookup more complex */
	if (!dw->mime_type)
		snoop_type = e_mail_part_snoop_type (part);

	/* if we had to snoop the part type to get here, then
	 * use that as the base type, yuck */
	if (snoop_type == NULL
		|| (type = camel_content_type_decode (snoop_type)) == NULL) {
		type = dw->mime_type;
		camel_content_type_ref (type);
	}

	if (dw->mime_type && type != dw->mime_type && camel_content_type_param (dw->mime_type, "charset")) {
		camel_content_type_set_param (type, "charset", camel_content_type_param (dw->mime_type, "charset"));
		charset_added = TRUE;
	}

	null = camel_stream_null_new ();
	filtered_stream = camel_stream_filter_new (null);
	g_object_unref (null);
	inline_filter = e_mail_inline_filter_new (
		camel_mime_part_get_encoding (part),
		type,
		camel_mime_part_get_filename (part));

	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (inline_filter));
	camel_data_wrapper_decode_to_stream_sync (
		dw, (CamelStream *) filtered_stream, cancellable, NULL);
	camel_stream_close ((CamelStream *) filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	if (!e_mail_inline_filter_found_any (inline_filter)) {
		g_object_unref (inline_filter);
		camel_content_type_unref (type);

		return process_part (
			parser, part_id, 0,
			part, e_mail_part_is_attachment (part),
			cancellable, out_mail_parts);
	}

	mp = e_mail_inline_filter_get_multipart (inline_filter);

	if (charset_added) {
		camel_content_type_set_param (type, "charset", NULL);
	}

	g_object_unref (inline_filter);
	camel_content_type_unref (type);

	/* We handle our made-up multipart here, so we don't recursively call ourselves */
	count = camel_multipart_get_number (mp);

	is_attachment = ((count == 1) && (e_mail_part_is_attachment (part)));

	for (i = 0; i < count; i++) {
		CamelMimePart *newpart = camel_multipart_get_part (mp, i);

		if (!newpart)
			continue;

		n_parts_added += process_part (
			parser, part_id, i,
			newpart, is_attachment,
			cancellable, out_mail_parts);
	}

	g_object_unref (mp);

	return n_parts_added;
}

static void
e_mail_parser_text_plain_class_init (EMailParserTextPlainClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->mime_types = parser_mime_types;
	iface->parse = empe_text_plain_parse;
}

static void
e_mail_parser_text_plain_init (EMailParserTextPlain *parser)
{

}

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
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserTextPlain,
	e_mail_parser_text_plain,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = { "text/plain", "text/*", NULL };

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

static GSList *
process_part (EMailParser *parser,
              GString *part_id,
              gint part_number,
              CamelMimePart *part,
              gboolean is_attachment,
              GCancellable *cancellable)
{
	CamelContentType *type;
	EMailPart *empart;
	gint s_len = part_id->len;
	GSList *parts;

	if (part_is_empty (part)) {
		return g_slist_alloc ();
	}

	type = camel_mime_part_get_content_type (part);
	if (!camel_content_type_is (type, "text", "calendar")) {

		g_string_append_printf (part_id, ".plain_text.%d", part_number);

		empart = e_mail_part_new (part, part_id->str);
		empart->mime_type = camel_content_type_simple (type);

		g_string_truncate (part_id, s_len);

		if (is_attachment) {

			return e_mail_parser_wrap_as_attachment (
					parser, part,
					g_slist_append (NULL, empart),
					part_id, cancellable);

		}

		return g_slist_append (NULL, empart);
	}

	g_string_append_printf (part_id, ".inline.%d", part_number);

	parts = e_mail_parser_parse_part (
			parser, CAMEL_MIME_PART (part),
			part_id, cancellable);

	g_string_truncate (part_id, s_len);

	return parts;
}

static GSList *
empe_text_plain_parse (EMailParserExtension *extension,
                       EMailParser *parser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable)
{
	GSList *parts;
	CamelStream *filtered_stream, *null;
	CamelMultipart *mp;
	CamelDataWrapper *dw;
	CamelContentType *type;
	gint i, count;
	EMailInlineFilter *inline_filter;
	gboolean charset_added = FALSE;
	const gchar *snoop_type = NULL;
	gboolean is_attachment;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	dw = camel_medium_get_content ((CamelMedium *) part);
	if (!dw)
		return NULL;

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
	inline_filter = e_mail_inline_filter_new (camel_mime_part_get_encoding (part), type);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (inline_filter));
	camel_data_wrapper_decode_to_stream_sync (
		dw, (CamelStream *) filtered_stream, cancellable, NULL);
	camel_stream_close ((CamelStream *) filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	mp = e_mail_inline_filter_get_multipart (inline_filter);

	if (charset_added) {
		camel_content_type_set_param (type, "charset", NULL);
	}

	g_object_unref (inline_filter);
	camel_content_type_unref (type);

	/* We handle our made-up multipart here, so we don't recursively call ourselves */
	count = camel_multipart_get_number (mp);
	parts = NULL;

	is_attachment = ((count == 1) && (e_mail_part_is_attachment (part)));

	for (i = 0; i < count; i++) {
		CamelMimePart *newpart = camel_multipart_get_part (mp, i);

		if (!newpart)
			continue;

		parts = g_slist_concat (parts,
				process_part (
					parser, part_id, i,
					newpart, is_attachment,
					cancellable));
	}

	g_object_unref (mp);

	return parts;
}

static const gchar **
empe_text_plain_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_text_plain_class_init (EMailParserTextPlainClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_text_plain_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_text_plain_mime_types;
}

static void
e_mail_parser_text_plain_init (EMailParserTextPlain *parser)
{

}

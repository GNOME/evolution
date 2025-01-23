/*
 * e-mail-parser-text-plain.c
 *
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
 */

#include "evolution-config.h"

#include <ctype.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-inline-filter.h"
#include "e-mail-parser-extension.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserTextPlain;
typedef EMailParserExtensionClass EMailParserTextPlainClass;

GType e_mail_parser_text_plain_get_type (void);

G_DEFINE_TYPE (
	EMailParserTextPlain,
	e_mail_parser_text_plain,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/plain",
	"text/*",
	NULL
};

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
	EMailPart *mail_part;
	gint s_len = part_id->len;
	gboolean handled = TRUE;

	type = camel_mime_part_get_content_type (part);
	if (!camel_content_type_is (type, "text", "*")) {
		handled = e_mail_parser_parse_part (
			parser, CAMEL_MIME_PART (part), part_id,
			cancellable, out_mail_parts);

	} else if (!camel_content_type_is (type, "text", "calendar")) {
		GQueue work_queue = G_QUEUE_INIT;
		gchar *mime_type;

		g_string_append_printf (part_id, ".plain_text.%d", part_number);

		mail_part = e_mail_part_new (part, part_id->str);

		mime_type = camel_content_type_simple (type);
		e_mail_part_set_mime_type (mail_part, mime_type);
		g_free (mime_type);

		g_string_truncate (part_id, s_len);

		g_queue_push_tail (&work_queue, mail_part);

		if (is_attachment)
			e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

		e_queue_transfer (&work_queue, out_mail_parts);

	} else {
		g_string_append_printf (part_id, ".inline.%d", part_number);

		handled = e_mail_parser_parse_part (
			parser, CAMEL_MIME_PART (part), part_id,
			cancellable, out_mail_parts);

		g_string_truncate (part_id, s_len);
	}

	return handled;
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
	gint ii, count;
	EMailInlineFilter *inline_filter;
	gboolean charset_added = FALSE;
	gchar *guessed_mime_type = NULL;
	gboolean is_attachment;
	gboolean handled = FALSE;

	dw = camel_medium_get_content ((CamelMedium *) part);
	if (!dw)
		return FALSE;

	/* This scans the text part for inline-encoded data, creates
	 * a multipart of all the parts inside it. */

	/* FIXME: We should discard this multipart if it only contains
	 * the original text, but it makes this hash lookup more complex */
	if (!camel_data_wrapper_get_mime_type_field (dw))
		guessed_mime_type = e_mail_part_guess_mime_type (part);

	/* if we had to guess the part type to get here, then
	 * use that as the base type, yuck */
	if (guessed_mime_type == NULL ||
	    (type = camel_content_type_decode (guessed_mime_type)) == NULL) {
		type = camel_data_wrapper_get_mime_type_field (dw);
		camel_content_type_ref (type);
	}

	g_clear_pointer (&guessed_mime_type, g_free);

	if (camel_data_wrapper_get_mime_type_field (dw) && type != camel_data_wrapper_get_mime_type_field (dw) &&
	    camel_content_type_param (camel_data_wrapper_get_mime_type_field (dw), "charset")) {
		camel_content_type_set_param (type, "charset", camel_content_type_param (camel_data_wrapper_get_mime_type_field (dw), "charset"));
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
		is_attachment = e_mail_part_is_attachment (part);

		if (is_attachment && CAMEL_IS_MIME_MESSAGE (part) &&
		    !(camel_content_type_is (camel_data_wrapper_get_mime_type_field (dw), "text", "*")
		     && camel_mime_part_get_filename (part) == NULL)) {
			e_mail_parser_wrap_as_non_expandable_attachment (parser, part, part_id, out_mail_parts);

			/* The main message part has a Content-Disposition header */
			is_attachment = FALSE;
			handled = TRUE;
		}

		g_object_unref (inline_filter);
		camel_content_type_unref (type);

		return process_part (
			parser, part_id, 0,
			part, is_attachment,
			cancellable, out_mail_parts) || handled;
	}

	mp = e_mail_inline_filter_get_multipart (inline_filter);

	if (charset_added) {
		camel_content_type_set_param (type, "charset", NULL);
	}

	g_object_unref (inline_filter);
	camel_content_type_unref (type);

	/* We handle our made-up multipart here,
	 * so we don't recursively call ourselves. */

	count = camel_multipart_get_number (mp);

	is_attachment = ((count == 1) && (e_mail_part_is_attachment (part)));

	for (ii = 0; ii < count; ii++) {
		CamelMimePart *newpart = camel_multipart_get_part (mp, ii);

		if (newpart != NULL) {
			handled |= process_part (
				parser, part_id, ii,
				newpart, is_attachment,
				cancellable, out_mail_parts);
		}
	}

	g_object_unref (mp);

	return handled;
}

static void
e_mail_parser_text_plain_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_text_plain_parse;
}

static void
e_mail_parser_text_plain_init (EMailParserExtension *extension)
{
}

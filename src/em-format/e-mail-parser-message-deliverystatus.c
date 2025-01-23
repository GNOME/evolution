/*
 * e-mail-parser-message-deliverystatus.c
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

#include <string.h>

#include <e-util/e-util.h>

#include "e-mail-part-attachment.h"
#include "e-mail-part-headers.h"
#include "e-mail-parser-extension.h"

typedef EMailParserExtension EMailParserMessageDeliveryStatus;
typedef EMailParserExtensionClass EMailParserMessageDeliveryStatusClass;

GType e_mail_parser_message_delivery_status_get_type (void);

G_DEFINE_TYPE (
	EMailParserMessageDeliveryStatus,
	e_mail_parser_message_delivery_status,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"message/delivery-status",
	"message/feedback-report",
	"message/disposition-notification",
	"text/rfc822-headers",
	NULL
};

static gboolean
empe_msg_deliverystatus_parse (EMailParserExtension *extension,
                               EMailParser *ml_parser,
                               CamelMimePart *part,
                               GString *part_id,
                               GCancellable *cancellable,
                               GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	CamelContentType *ct;
	EMailPart *mail_part = NULL;
	gboolean show_inline;
	gsize len;

	ct = camel_mime_part_get_content_type (part);
	show_inline = ct && camel_content_type_is (ct, "message", "feedback-report");

	len = part_id->len;
	g_string_append (part_id, ".delivery-status");

	if (ct && camel_content_type_is (ct, "text", "rfc822-headers")) {
		CamelMimePart *replace_part;
		CamelMimeParser *parser;
		CamelStream *stream;
		gboolean success;

		show_inline = TRUE;

		stream = camel_stream_mem_new ();
		parser = camel_mime_parser_new ();
		replace_part = camel_mime_part_new ();

		success = camel_data_wrapper_decode_to_stream_sync (camel_medium_get_content (CAMEL_MEDIUM (part)), stream, cancellable, NULL);
		if (success) {
			g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, cancellable, NULL);

			success = camel_mime_parser_init_with_stream (parser, stream, NULL) != -1;
		}

		success = success && camel_mime_part_construct_from_parser_sync (replace_part, parser, cancellable, NULL);

		if (success) {
			const CamelNameValueArray *headers;

			headers = camel_medium_get_headers (CAMEL_MEDIUM (replace_part));
			success = camel_name_value_array_get_length (headers) > 0;
		}

		if (success) {
			mail_part = e_mail_part_headers_new (replace_part, part_id->str);
			e_mail_part_set_mime_type (mail_part, "text/rfc822-headers");
		}

		g_object_unref (replace_part);
		g_object_unref (parser);
		g_object_unref (stream);
	}

	if (!mail_part) {
		mail_part = e_mail_part_new (part, part_id->str);
		e_mail_part_set_mime_type (mail_part, "text/plain");
	}

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	/* The only reason for having a separate parser for
	 * message/delivery-status is to display the part as an attachment */
	e_mail_parser_wrap_as_attachment (ml_parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	if (!show_inline) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		show_inline = g_settings_get_boolean (settings, "display-delivery-notification-inline");
		g_object_unref (settings);
	}

	if (show_inline) {
		EMailPart *attachment_part;

		attachment_part = g_queue_peek_head (&work_queue);
		if (attachment_part && E_IS_MAIL_PART_ATTACHMENT (attachment_part))
			attachment_part->force_inline = TRUE;
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_message_delivery_status_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_msg_deliverystatus_parse;
}

static void
e_mail_parser_message_delivery_status_init (EMailParserExtension *extension)
{
}

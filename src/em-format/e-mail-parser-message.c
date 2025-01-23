/*
 * e-mail-parser-message.c
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
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-parser-extension.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserMessage;
typedef EMailParserExtensionClass EMailParserMessageClass;

GType e_mail_parser_message_get_type (void);

G_DEFINE_TYPE (
	EMailParserMessage,
	e_mail_parser_message,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/vnd.evolution.message",
	NULL
};

static gboolean
empe_message_parse (EMailParserExtension *extension,
                    EMailParser *parser,
                    CamelMimePart *part,
                    GString *part_id,
                    GCancellable *cancellable,
                    GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	CamelContentType *ct;
	EMailPart *mail_part;
	gchar *mime_type;

	/* Headers */
	e_mail_parser_parse_part_as (
		parser, part, part_id,
		"application/vnd.evolution.headers",
		cancellable, out_mail_parts);

	ct = camel_mime_part_get_content_type (part);
	mime_type = camel_content_type_simple (ct);

	if (camel_content_type_is (ct, "message", "*")) {
		/* get mime type of the content of the message,
		 * instead of using a generic message/rfc822 */
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		if (content) {
			ct = camel_data_wrapper_get_mime_type_field (content);

			g_free (mime_type);
			mime_type = camel_content_type_simple (ct);
		}
	}

	/* Actual message body */

	e_mail_parser_parse_part_as (
		parser, part, part_id, mime_type,
		cancellable, &work_queue);

	/* If the EMailPart representing the message body is marked as an
	 * attachment, wrap it as such so it gets added to the attachment
	 * bar but also set the "force_inline" flag since it doesn't make
	 * sense to collapse the message body if we can render it. */
	mail_part = g_queue_peek_head (&work_queue);
	if (mail_part != NULL && !E_IS_MAIL_PART_ATTACHMENT (mail_part)) {
		if (e_mail_part_get_is_attachment (mail_part)) {
			e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

			mail_part = g_queue_peek_head (&work_queue);

			if (mail_part != NULL)
				mail_part->force_inline = TRUE;
		}
	}

	if (CAMEL_IS_MIME_MESSAGE (part)) {
		CamelInternetAddress *from_address;

		from_address = camel_mime_message_get_from (CAMEL_MIME_MESSAGE (part));
		if (from_address) {
			GList *link;

			for (link = g_queue_peek_head_link (&work_queue); link; link = g_list_next (link)) {
				mail_part = link->data;

				if (mail_part)
					e_mail_part_verify_validity_sender (mail_part, from_address);
			}
		}
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	g_free (mime_type);

	return TRUE;
}

static void
e_mail_parser_message_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_message_parse;
}

static void
e_mail_parser_message_init (EMailParserExtension *extension)
{
}

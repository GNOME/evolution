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
	NULL
};

static gboolean
empe_msg_deliverystatus_parse (EMailParserExtension *extension,
                               EMailParser *parser,
                               CamelMimePart *part,
                               GString *part_id,
                               GCancellable *cancellable,
                               GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	EMailPart *mail_part;
	gsize len;

	len = part_id->len;
	g_string_append (part_id, ".delivery-status");
	mail_part = e_mail_part_new (part, part_id->str);
	e_mail_part_set_mime_type (mail_part, "text/plain");

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	/* The only reason for having a separate parser for
	 * message/delivery-status is to display the part as an attachment */
	e_mail_parser_wrap_as_attachment (parser, part, part_id, &work_queue);

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

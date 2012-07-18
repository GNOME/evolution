/*
 * e-mail-parser-message-deliverystatus.c
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

#include <glib-object.h>

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserMessageDeliveryStatus {
	GObject parent;
} EMailParserMessageDeliveryStatus;

typedef struct _EMailParserMessageDeliveryStatusClass {
	GObjectClass parent_class;
} EMailParserMessageDeliveryStatusClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMessageDeliveryStatus,
	e_mail_parser_message_delivery_status,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar* parser_mime_types[] = { "message/delivery-status",
					    "message/feedback-report",
					    "message/disposition-notification",
					    NULL };

static GSList *
empe_msg_deliverystatus_parse (EMailParserExtension *extension,
                               EMailParser *parser,
                               CamelMimePart *part,
                               GString *part_id,
                               GCancellable *cancellable)
{
	EMailPart *mail_part;
	gsize len;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	len = part_id->len;
	g_string_append (part_id, ".delivery-status");
	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = g_strdup ("text/plain");

	g_string_truncate (part_id, len);

	/* The only reason for having a separate parser for
	 * message/delivery-status is to display the part as an attachment */
	return e_mail_parser_wrap_as_attachment (
			parser, part, g_slist_append (NULL, mail_part),
			part_id, cancellable);
}

static const gchar **
empe_msg_deliverystatus_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_message_delivery_status_class_init (EMailParserMessageDeliveryStatusClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_msg_deliverystatus_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_msg_deliverystatus_mime_types;
}

static void
e_mail_parser_message_delivery_status_init (EMailParserMessageDeliveryStatus *parser)
{

}

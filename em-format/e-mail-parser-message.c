/*
 * e-mail-parser-message.c
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

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include "e-mail-part-utils.h"
#include <libemail-engine/e-mail-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

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
	CamelContentType *ct;
	gchar *mime_type;

	/* Headers */
	e_mail_parser_parse_part_as (
		parser, part, part_id,
		"application/vnd.evolution.headers",
		cancellable, out_mail_parts);

	/* Attachment Bar */
	e_mail_parser_parse_part_as (
		parser, part, part_id,
		"application/vnd.evolution.widget.attachment-bar",
		cancellable, out_mail_parts);

	ct = camel_mime_part_get_content_type (part);
	mime_type = camel_content_type_simple (ct);

	if (mime_type && g_ascii_strcasecmp (mime_type, "message/rfc822") == 0) {
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
		cancellable, out_mail_parts);

	g_free (mime_type);

	return TRUE;
}

static void
e_mail_parser_message_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->parse = empe_message_parse;
}

static void
e_mail_parser_message_init (EMailParserExtension *extension)
{
}

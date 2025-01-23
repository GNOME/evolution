/*
 * e-mail-parser-message-rfc822.c
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

#include "e-mail-parser-extension.h"
#include "e-mail-part-list.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserMessageRFC822;
typedef EMailParserExtensionClass EMailParserMessageRFC822Class;

GType e_mail_parser_message_rfc822_get_type (void);

G_DEFINE_TYPE (
	EMailParserMessageRFC822,
	e_mail_parser_message_rfc822,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"message/rfc822",
	"message/news",
	NULL
};

static gboolean
empe_msg_rfc822_parse (EMailParserExtension *extension,
                       EMailParser *eparser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable,
                       GQueue *out_mail_parts)
{
	EMailPart *mail_part;
	gint len;
	CamelMimePart *message;
	CamelDataWrapper *dw;
	CamelStream *new_stream;
	CamelMimeParser *mime_parser;
	CamelContentType *ct;

	len = part_id->len;
	g_string_append (part_id, ".rfc822");

	/* Create an empty PURI that will represent start of the RFC message */
	mail_part = e_mail_part_new (part, part_id->str);
	e_mail_part_set_mime_type (mail_part, "message/rfc822");
	g_queue_push_tail (out_mail_parts, mail_part);

	/* Sometime the actual message is encapsulated in another
	 * CamelMimePart, sometimes the CamelMimePart itself represents
	 * the RFC822 message. */
	ct = camel_mime_part_get_content_type (part);
	if (camel_content_type_is (ct, "message", "*")) {
		new_stream = camel_stream_mem_new ();
		mime_parser = camel_mime_parser_new ();
		message = (CamelMimePart *) camel_mime_message_new ();

		dw = camel_medium_get_content (CAMEL_MEDIUM (part));
		camel_data_wrapper_decode_to_stream_sync (
			dw, new_stream, cancellable, NULL);
		g_seekable_seek (
			G_SEEKABLE (new_stream), 0,
			G_SEEK_SET, cancellable, NULL);
		camel_mime_parser_init_with_stream (
			mime_parser, new_stream, NULL);
		camel_mime_part_construct_from_parser_sync (
			message, mime_parser, cancellable, NULL);

		g_object_unref (mime_parser);
		g_object_unref (new_stream);
	} else {
		message = g_object_ref (part);
	}

	e_mail_parser_parse_part_as (
		eparser, message, part_id,
		"application/vnd.evolution.message",
		cancellable, out_mail_parts);

	g_object_unref (message);

	/* Add another generic EMailPart that represents end of the RFC
	 * message.  The em_format_write() function will skip all parts
	 * between the ".rfc822" part and ".rfc822.end" part as they will
	 * be rendered in an <iframe>. */
	g_string_append (part_id, ".end");
	mail_part = e_mail_part_new (message, part_id->str);
	mail_part->is_hidden = TRUE;
	g_queue_push_tail (out_mail_parts, mail_part);

	g_string_truncate (part_id, len);

	if (e_mail_part_is_attachment (message))
		e_mail_parser_wrap_as_attachment (eparser, message, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_message_rfc822_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->flags =
		E_MAIL_PARSER_EXTENSION_INLINE |
		E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
	class->parse = empe_msg_rfc822_parse;
}

static void
e_mail_parser_message_rfc822_init (EMailParserExtension *extension)
{
}

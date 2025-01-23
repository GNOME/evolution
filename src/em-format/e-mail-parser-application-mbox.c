/*
 * e-mail-parser-application-mbox.c
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
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserApplicationMBox;
typedef EMailParserExtensionClass EMailParserApplicationMBoxClass;

GType e_mail_parser_application_mbox_get_type (void);

G_DEFINE_TYPE (
	EMailParserApplicationMBox,
	e_mail_parser_application_mbox,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/mbox",
	NULL
};

static void
empe_app_mbox_add_message (EMailParser *parser,
			   CamelMimeMessage *message,
			   gint nth_message,
			   GString *part_id,
			   GCancellable *cancellable,
			   GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	CamelMimePart *opart;
	gint old_len;

	old_len = part_id->len;

	g_string_append_printf (part_id, ".mbox.%d", nth_message);

	opart = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (opart), CAMEL_DATA_WRAPPER (message));
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (opart), "message/rfc822");

	e_mail_parser_parse_part_as (
		parser, opart, part_id, "message/rfc822",
		cancellable, &work_queue);

	/* Wrap every message as attachment */
	e_mail_parser_wrap_as_attachment (parser, opart, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	/* Inline all messages in mbox */
	if (!g_queue_is_empty (&work_queue)) {
		EMailPart *pp = g_queue_peek_head (&work_queue);

		pp->force_inline = TRUE;
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	g_string_truncate (part_id, old_len);

	g_object_unref (opart);
}

static gboolean
empe_app_mbox_parse (EMailParserExtension *extension,
                     EMailParser *parser,
                     CamelMimePart *part,
                     GString *part_id,
                     GCancellable *cancellable,
                     GQueue *out_mail_parts)
{
	CamelMimeParser *mime_parser;
	CamelStream *mem_stream;
	CamelMimeParserState state;
	gint messages;
	GError *error = NULL;

	/* Extract messages from the application/mbox part and
	 * render them as a flat list of messages. */

	/* XXX If the mbox has multiple messages, maybe render them
	 *     as a multipart/digest so each message can be expanded
	 *     or collapsed individually.
	 *
	 *     See attachment_handler_mail_x_uid_list() for example. */

	/* XXX This is based on em_utils_read_messages_from_stream().
	 *     Perhaps refactor that function to return an array of
	 *     messages instead of assuming we want to append them
	 *     to a folder? */

	mime_parser = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mime_parser, TRUE);

	mem_stream = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream_sync (
		camel_medium_get_content (CAMEL_MEDIUM (part)),
		mem_stream, NULL, NULL);
	g_seekable_seek (G_SEEKABLE (mem_stream), 0, G_SEEK_SET, cancellable, NULL);

	camel_mime_parser_init_with_stream (mime_parser, mem_stream, &error);
	if (error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Error parsing MBOX part: %s"),
			error->message);
		g_object_unref (mem_stream);
		g_object_unref (mime_parser);
		g_error_free (error);
		return TRUE;
	}

	/* Extract messages from the mbox. */
	messages = 0;
	state = camel_mime_parser_step (mime_parser, NULL, NULL);

	while (state == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *message;

		message = camel_mime_message_new ();

		if (!camel_mime_part_construct_from_parser_sync (
			CAMEL_MIME_PART (message), mime_parser, NULL, NULL)) {
			g_object_unref (message);
			break;
		}

		empe_app_mbox_add_message (parser, message, messages, part_id, cancellable, out_mail_parts);
		messages++;

		g_object_unref (message);

		/* Skip past CAMEL_MIME_PARSER_STATE_FROM_END. */
		camel_mime_parser_step (mime_parser, NULL, NULL);

		state = camel_mime_parser_step (mime_parser, NULL, NULL);
	}

	if (!messages) {
		CamelMimeMessage *message;

		g_seekable_seek (G_SEEKABLE (mem_stream), 0, G_SEEK_SET, cancellable, NULL);

		message = camel_mime_message_new ();

		if (camel_data_wrapper_construct_from_stream_sync (CAMEL_DATA_WRAPPER (message), mem_stream, NULL, NULL)) {
			empe_app_mbox_add_message (parser, message, messages, part_id, cancellable, out_mail_parts);
			messages++;
		}

		g_object_unref (message);
	}

	g_object_unref (mime_parser);
	g_object_unref (mem_stream);

	return messages > 0;
}

static void
e_mail_parser_application_mbox_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->flags =
		E_MAIL_PARSER_EXTENSION_INLINE |
		E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
	class->parse = empe_app_mbox_parse;
}

static void
e_mail_parser_application_mbox_init (EMailParserExtension *extension)
{
}

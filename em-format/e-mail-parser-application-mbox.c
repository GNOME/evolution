/*
 * e-mail-parser-application-mbox.c
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
#include <glib/gi18n-lib.h>

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserApplicationMBox {
	GObject parent;
} EMailParserApplicationMBox;

typedef struct _EMailParserApplicationMBoxClass {
	GObjectClass parent_class;
} EMailParserApplicationMBoxClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserApplicationMBox,
	e_mail_parser_application_mbox,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "application/mbox",
					    NULL };

static GSList *
empe_app_mbox_parse (EMailParserExtension *extension,
                     EMailParser *parser,
                     CamelMimePart *part,
                     GString *part_id,
                     GCancellable *cancellable)
{
	CamelMimeParser *mime_parser;
	CamelStream *mem_stream;
	camel_mime_parser_state_t state;
	gint old_len;
	gint messages;
	GSList *parts;
	GError *error;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

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

	error =  NULL;
	camel_mime_parser_init_with_stream (mime_parser, mem_stream, &error);
	if (error) {
		parts = e_mail_parser_error (parser, cancellable,
				_("Error parsing MBOX part: %s"),
				error->message ?
					error->message :
					_("Unknown error"));
		g_clear_error (&error);
		g_object_unref (mem_stream);
		g_object_unref (mime_parser);
		return parts;
	}

	g_object_unref (mem_stream);

	old_len = part_id->len;

	/* Extract messages from the mbox. */
	messages = 0;
	state = camel_mime_parser_step (mime_parser, NULL, NULL);

	parts = NULL;
	while (state == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *message;
		CamelMimePart *opart;
		GSList *new_parts;

		message = camel_mime_message_new ();
		opart = CAMEL_MIME_PART (message);

		if (!camel_mime_part_construct_from_parser_sync (
			opart, mime_parser, NULL, NULL)) {
			g_object_unref (message);
			break;
		}

		g_string_append_printf (part_id, ".mbox.%d", messages);

		opart = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (opart), CAMEL_DATA_WRAPPER (message));
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (opart), "message/rfc822");

		new_parts = e_mail_parser_parse_part_as (
				parser, opart,
				part_id, "message/rfc822", cancellable);

		/* Wrap every message as attachment */
		new_parts = e_mail_parser_wrap_as_attachment (
				parser, opart,
				new_parts, part_id, cancellable);

		/* Inline all messages in mbox */
		if (new_parts && new_parts->data) {
			EMailPart *p = new_parts->data;

			p->force_inline = TRUE;
		}

		parts = g_slist_concat (parts, new_parts);

		g_string_truncate (part_id, old_len);

		g_object_unref (message);
		g_object_unref (opart);

		/* Skip past CAMEL_MIME_PARSER_STATE_FROM_END. */
		camel_mime_parser_step (mime_parser, NULL, NULL);

		state = camel_mime_parser_step (mime_parser, NULL, NULL);

		messages++;
	}

	g_object_unref (mime_parser);

	return parts;
}

static guint32
empe_app_mbox_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_INLINE |
		E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
}

static const gchar **
empe_app_mbox_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_application_mbox_class_init (EMailParserApplicationMBoxClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *interface)
{
	interface->parse = empe_app_mbox_parse;
	interface->get_flags = empe_app_mbox_get_flags;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *interface)
{
	interface->mime_types = empe_app_mbox_mime_types;
}

static void
e_mail_parser_application_mbox_init (EMailParserApplicationMBox *self)
{
}

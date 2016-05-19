/*
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
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-mail-parser-itip.h"

#include <shell/e-shell.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include "e-mail-part-itip.h"
#include "itip-view.h"

#define d(x)

typedef EMailParserExtension EMailParserItip;
typedef EMailParserExtensionClass EMailParserItipClass;

typedef EExtension EMailParserItipLoader;
typedef EExtensionClass EMailParserItipLoaderClass;

GType e_mail_parser_itip_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserItip,
	e_mail_parser_itip,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/calendar",
	"application/ics",
	NULL
};

static gboolean
empe_itip_parse (EMailParserExtension *extension,
                 EMailParser *parser,
                 CamelMimePart *part,
                 GString *part_id,
                 GCancellable *cancellable,
                 GQueue *out_mail_parts)
{
	EMailPartItip *itip_part;
	CamelDataWrapper *content;
	CamelStream *stream;
	GByteArray *byte_array;
	gint len;
	const CamelContentDisposition *disposition;
	GQueue work_queue = G_QUEUE_INIT;

	len = part_id->len;
	g_string_append_printf (part_id, ".itip");

	itip_part = e_mail_part_itip_new (part, part_id->str);
	itip_part->itip_mime_part = g_object_ref (part);

	/* This is non-gui thread. Download the part for using in the main thread */
	content = camel_medium_get_content ((CamelMedium *) part);

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL);

	if (byte_array->len == 0)
		itip_part->vcalendar = NULL;
	else
		itip_part->vcalendar = g_strndup (
			(gchar *) byte_array->data, byte_array->len);

	g_object_unref (stream);

	g_queue_push_tail (&work_queue, itip_part);

	disposition = camel_mime_part_get_content_disposition (part);
	if (disposition &&
	    (g_strcmp0 (disposition->disposition, "attachment") == 0)) {
		e_mail_parser_wrap_as_attachment (
			parser, part, part_id, &work_queue);
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	g_string_truncate (part_id, len);

	return TRUE;
}

static void
e_mail_parser_itip_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = empe_itip_parse;
}

static void
e_mail_parser_itip_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_itip_init (EMailParserExtension *class)
{
}

void
e_mail_parser_itip_type_register (GTypeModule *type_module)
{
	e_mail_parser_itip_register_type (type_module);
}

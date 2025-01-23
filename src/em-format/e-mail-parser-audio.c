/*
 * e-mail-parser-audio.c
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
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include "e-mail-extension-registry.h"
#include "e-mail-parser-extension.h"
#include "e-mail-part-audio.h"
#include "e-mail-part.h"

/* This is currently disabled, because the WebKit player requires javascript,
   which is disabled in Evolution. */

typedef EMailParserExtension EMailParserAudio;
typedef EMailParserExtensionClass EMailParserAudioClass;

GType e_mail_parser_audio_get_type (void);

G_DEFINE_TYPE (
	EMailParserAudio,
	e_mail_parser_audio,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"audio/ac3",
	"audio/x-ac3",
	"audio/basic",
	"audio/mpeg",
	"audio/x-mpeg",
	"audio/mpeg3",
	"audio/x-mpeg3",
	"audio/mp3",
	"audio/x-mp3",
	"audio/mp4",
	"audio/flac",
	"audio/x-flac",
	"audio/mod",
	"audio/x-mod",
	"audio/x-wav",
	"audio/microsoft-wav",
	"audio/x-wma",
	"audio/x-ms-wma",
	"audio/ogg",
	"audio/x-vorbis+ogg",
	"application/ogg",
	"application/x-ogg",
	NULL
};

static gboolean
mail_parser_audio_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_queue)
{
	EMailPart *mail_part;
	GQueue work_queue = G_QUEUE_INIT;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".audio");

	camel_mime_part_set_disposition (part, "inline");

	mail_part = e_mail_part_audio_new (part, part_id->str);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_queue);

	return TRUE;
}

static void
e_mail_parser_audio_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = mail_parser_audio_parse;
}

static void
e_mail_parser_audio_init (EMailParserExtension *extension)
{
}

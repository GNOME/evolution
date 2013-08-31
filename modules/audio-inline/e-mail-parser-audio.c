/*
 * e-mail-parser-audio.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "e-mail-parser-audio.h"
#include "e-mail-part-audio.h"

#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef EMailParserExtension EMailParserAudio;
typedef EMailParserExtensionClass EMailParserAudioClass;

typedef EExtension EMailParserAudioLoader;
typedef EExtensionClass EMailParserAudioLoaderClass;

GType e_mail_parser_audio_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
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
	g_string_append (part_id, ".org-gnome-audio-button-panel");

	camel_mime_part_set_disposition (part, "inline");

	d (printf ("audio formatter: format classid %s\n", part_id->str));

	mail_part = e_mail_part_audio_new (part, part_id->str);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	e_mail_parser_wrap_as_attachment (
		parser, part, part_id, &work_queue);

	e_queue_transfer (&work_queue, out_mail_queue);

	return TRUE;
}

static void
e_mail_parser_audio_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = mail_parser_audio_parse;
}

static void
e_mail_parser_audio_class_finalize (EMailParserExtensionClass *class)
{

}

static void
e_mail_parser_audio_init (EMailParserExtension *extension)
{
}

void
e_mail_parser_audio_type_register (GTypeModule *type_module)
{
	e_mail_parser_audio_register_type (type_module);
}


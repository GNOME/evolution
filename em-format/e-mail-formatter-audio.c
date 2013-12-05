/*
 * e-mail-formatter-audio.c
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

#include <camel/camel.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-formatter.h"
#include "e-mail-part-audio.h"

#define d(x)

typedef EMailFormatterExtension EMailFormatterAudio;
typedef EMailFormatterExtensionClass EMailFormatterAudioClass;

GType e_mail_formatter_audio_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterAudio,
	e_mail_formatter_audio,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.widget.audio",
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
mail_formatter_audio_format (EMailFormatterExtension *extension,
                             EMailFormatter *formatter,
                             EMailFormatterContext *context,
                             EMailPart *part,
                             CamelStream *stream,
                             GCancellable *cancellable)
{
	CamelMimePart *mime_part;
	CamelDataWrapper *content;
	CamelTransferEncoding encoding;
	CamelStream *mem_stream;
	GByteArray *byte_array;
	const gchar *mime_type;
	gchar *html;
	GError *local_error = NULL;

	mime_part = e_mail_part_ref_mime_part (part);
	encoding = camel_mime_part_get_encoding (mime_part);
	content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	mime_type = e_mail_part_get_mime_type (part);
	if (mime_type == NULL)
		mime_type = "audio/*";

	mem_stream = camel_stream_mem_new ();
	byte_array = camel_stream_mem_get_byte_array (
		CAMEL_STREAM_MEM (mem_stream));

	if (encoding == CAMEL_TRANSFER_ENCODING_BASE64) {
		camel_data_wrapper_write_to_stream_sync (
			content, mem_stream, cancellable, &local_error);

		html = g_strdup_printf (
			"<audio controls>"
			"<source src=\"data:%s;base64,%s\"/>"
			"</audio>",
			mime_type, (gchar *) byte_array->data);

	} else {
		gchar *base64;

		camel_data_wrapper_decode_to_stream_sync (
			content, mem_stream, cancellable, &local_error);

		base64 = g_base64_encode (
			(guchar *) byte_array->data, byte_array->len);
		html = g_strdup_printf (
			"<audio controls>"
			"<source src=\"data:%s;base64,%s\"/>"
			"</audio>",
			mime_type, base64);
		g_free (base64);
	}

	/* XXX Should show the error message in the UI somehow. */
	if (local_error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}

	camel_stream_write_string (stream, html, NULL, NULL);

	g_free (html);

	g_object_unref (mime_part);
	g_object_unref (mem_stream);

	return TRUE;
}

static void
e_mail_formatter_audio_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Audio Player");
	class->description = _("Play the attachment in embedded audio player");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = mail_formatter_audio_format;
}

static void
e_mail_formatter_audio_init (EMailFormatterExtension *extension)
{
}


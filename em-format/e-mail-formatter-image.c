/*
 * image-any.c
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

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-inline-filter.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef EMailFormatterExtension EMailFormatterImage;
typedef EMailFormatterExtensionClass EMailFormatterImageClass;

GType e_mail_formatter_image_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterImage,
	e_mail_formatter_image,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"image/gif",
	"image/jpeg",
	"image/png",
	"image/x-png",
	"image/x-bmp",
	"image/bmp",
	"image/svg",
	"image/x-cmu-raster",
	"image/x-ico",
	"image/x-portable-anymap",
	"image/x-portable-bitmap",
	"image/x-portable-graymap",
	"image/x-portable-pixmap",
	"image/x-xpixmap",
	"image/jpg",
	"image/pjpeg",
	"image/*",
	NULL
};

static gboolean
emfe_image_format (EMailFormatterExtension *extension,
                   EMailFormatter *formatter,
                   EMailFormatterContext *context,
                   EMailPart *part,
                   CamelStream *stream,
                   GCancellable *cancellable)
{
	gchar *content;
	CamelDataWrapper *dw;
	GByteArray *ba;
	CamelStream *raw_content;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
	g_return_val_if_fail (dw, FALSE);

	raw_content = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream_sync (dw, raw_content, cancellable, NULL);
	ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (raw_content));

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {

		if (!e_mail_formatter_get_animate_images (formatter)) {

			gchar *buff;
			gsize len;

			e_mail_part_animation_extract_frame (ba, &buff, &len);

			camel_stream_write (stream, buff, len, cancellable, NULL);

			g_free (buff);

		} else {

			camel_stream_write (
				stream, (gchar *) ba->data,
				ba->len, cancellable, NULL);
		}

	} else {

		gchar *buffer;

		if (!e_mail_formatter_get_animate_images (formatter)) {

			gchar *buff;
			gsize len;

			e_mail_part_animation_extract_frame (ba, &buff, &len);

			content = g_base64_encode ((guchar *) buff, len);
			g_free (buff);

		} else {
			content = g_base64_encode ((guchar *) ba->data, ba->len);
		}

		/* The image is already base64-encrypted so we can directly
		 * paste it to the output */
		buffer = g_strdup_printf (
			"<img src=\"data:%s;base64,%s\" style=\"max-width: 100%%;\" />",
			part->mime_type ? part->mime_type : "image/*", content);

		camel_stream_write_string (stream, buffer, cancellable, NULL);
		g_free (buffer);
		g_free (content);
	}

	g_object_unref (raw_content);

	return TRUE;
}

static void
e_mail_formatter_image_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Regular Image");
	class->description = _("Display part as an image");
	class->mime_types = formatter_mime_types;
	class->format = emfe_image_format;
}

static void
e_mail_formatter_image_init (EMailFormatterExtension *extension)
{
}

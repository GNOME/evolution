/*
 * image-any.c
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

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"
#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

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
                   GOutputStream *stream,
                   GCancellable *cancellable)
{
	gchar *content;
	CamelMimePart *mime_part;
	CamelDataWrapper *dw;
	GBytes *bytes;
	GOutputStream *raw_content;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	mime_part = e_mail_part_ref_mime_part (part);
	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	g_return_val_if_fail (dw, FALSE);

	raw_content = g_memory_output_stream_new_resizable ();
	camel_data_wrapper_decode_to_output_stream_sync (
		dw, raw_content, cancellable, NULL);
	g_output_stream_close (raw_content, NULL, NULL);

	bytes = g_memory_output_stream_steal_as_bytes (
		G_MEMORY_OUTPUT_STREAM (raw_content));

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {

		if (!e_mail_formatter_get_animate_images (formatter)) {

			gchar *buff;
			gsize len;

			e_mail_part_animation_extract_frame (
				bytes, &buff, &len);

			g_output_stream_write_all (
				stream, buff, len, NULL, cancellable, NULL);

			g_free (buff);

		} else {
			gconstpointer data;
			gsize size;

			data = g_bytes_get_data (bytes, &size);

			g_output_stream_write_all (
				stream, data, size, NULL, cancellable, NULL);
		}

	} else {
		gchar *buffer;
		const gchar *mime_type;

		if (!e_mail_formatter_get_animate_images (formatter)) {

			gchar *buff;
			gsize len;

			e_mail_part_animation_extract_frame (
				bytes, &buff, &len);

			content = g_base64_encode ((guchar *) buff, len);
			g_free (buff);

		} else {
			gconstpointer data;
			gsize size;

			data = g_bytes_get_data (bytes, &size);
			content = g_base64_encode (data, size);
		}

		mime_type = e_mail_part_get_mime_type (part);
		if (mime_type == NULL)
			mime_type = "image/*";

		/* The image is already base64-encrypted so we can directly
		 * paste it to the output */
		buffer = g_strdup_printf (
			"<img src=\"data:%s;base64,%s\" "
			"     style=\"max-width: 100%%;\" />",
			mime_type, content);

		g_output_stream_write_all (
			stream, buffer, strlen (buffer),
			NULL, cancellable, NULL);

		g_free (buffer);
		g_free (content);
	}

	g_bytes_unref (bytes);

	g_object_unref (raw_content);

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_image_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Regular Image");
	class->description = _("Display part as an image");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_image_format;
}

static void
e_mail_formatter_image_init (EMailFormatterExtension *extension)
{
}

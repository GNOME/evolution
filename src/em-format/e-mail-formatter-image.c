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
	CamelMimePart *mime_part;
	CamelContentType *content_type;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	mime_part = e_mail_part_ref_mime_part (part);

	content_type = camel_mime_part_get_content_type (mime_part);

	/* Skip TIFF images, which cannot be shown inline */
	if (content_type && (
	    camel_content_type_is (content_type, "image", "tiff") ||
	    camel_content_type_is (content_type, "image", "tif"))) {
		g_clear_object (&mime_part);
		return FALSE;
	}

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		CamelDataWrapper *dw;
		GBytes *bytes;
		GOutputStream *raw_content;

		dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		g_return_val_if_fail (dw, FALSE);

		raw_content = g_memory_output_stream_new_resizable ();
		camel_data_wrapper_decode_to_output_stream_sync (
			dw, raw_content, cancellable, NULL);
		g_output_stream_close (raw_content, NULL, NULL);

		bytes = g_memory_output_stream_steal_as_bytes (
			G_MEMORY_OUTPUT_STREAM (raw_content));

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

		g_bytes_unref (bytes);
		g_object_unref (raw_content);
	} else {
		gchar *buffer, *uri;
		const gchar *filename;

		filename = camel_mime_part_get_filename (mime_part);

		uri = e_mail_part_build_uri (
			e_mail_part_list_get_folder (context->part_list),
			e_mail_part_list_get_message_uid (context->part_list),
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"filename", G_TYPE_STRING, filename ? filename : "",
			NULL);

		buffer = g_strdup_printf ("<img src=\"%s\" style=\"max-width:100%%;\" />", uri);

		g_output_stream_write_all (
			stream, buffer, strlen (buffer),
			NULL, cancellable, NULL);

		g_free (buffer);
		g_free (uri);
	}

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

/*
 * e-mail-formatter-text-plain.c
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
#include "e-mail-part-utils.h"

typedef EMailFormatterExtension EMailFormatterTextPlain;
typedef EMailFormatterExtensionClass EMailFormatterTextPlainClass;

GType e_mail_formatter_text_plain_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterTextPlain,
	e_mail_formatter_text_plain,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/plain",
	"text/*",
	"message/*",
	"application/vnd.evolution.plaintext",
	NULL
};

static gboolean
emfe_text_plain_format (EMailFormatterExtension *extension,
                        EMailFormatter *formatter,
                        EMailFormatterContext *context,
                        EMailPart *part,
                        GOutputStream *stream,
                        GCancellable *cancellable)
{
	GOutputStream *filtered_stream;
	CamelMimeFilter *filter;
	const gchar *format;
	const gchar *string;
	guint32 rgb;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {
		CamelMimeFilterToHTMLFlags flags;
		CamelMimePart *mime_part;
		CamelDataWrapper *dw;

		if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
			string = e_mail_formatter_get_sub_html_header (formatter);

			g_output_stream_write_all (
				stream, string, strlen (string),
				NULL, cancellable, NULL);

			/* No need for body margins within <iframe> */
			string = "<style>body{ margin: 0; }</style>";

			g_output_stream_write_all (
				stream, string, strlen (string),
				NULL, cancellable, NULL);
		}

		flags = e_mail_formatter_get_text_format_flags (formatter);
		flags |= CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES | CAMEL_MIME_FILTER_TOHTML_PRESERVE_TABS;

		mime_part = e_mail_part_ref_mime_part (part);
		dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		if (dw == NULL) {
			g_object_unref (mime_part);
			return FALSE;
		}

		/* Check for RFC 2646 flowed text. */
		if (camel_content_type_is (camel_data_wrapper_get_mime_type_field (dw), "text", "plain")
		&& (format = camel_content_type_param (camel_data_wrapper_get_mime_type_field (dw), "format"))
		&& !g_ascii_strcasecmp (format, "flowed"))
			flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

		rgb = e_rgba_to_value (
			e_mail_formatter_get_color (
				formatter, E_MAIL_FORMATTER_COLOR_CITATION));

		filter = camel_mime_filter_tohtml_new (flags, rgb);
		filtered_stream =
			camel_filter_output_stream_new (stream, filter);
		g_filter_output_stream_set_close_base_stream (
			G_FILTER_OUTPUT_STREAM (filtered_stream), FALSE);
		g_object_unref (filter);

		string =
			"<div class=\"part-container pre "
			"-e-web-view-background-color -e-web-view-text-color\" "
			"style=\"border: none; padding: 0; margin: 0;\">";

		g_output_stream_write_all (
			stream, string, strlen (string),
			NULL, cancellable, NULL);

		e_mail_formatter_format_text (
			formatter, part, filtered_stream, cancellable);
		g_output_stream_flush (filtered_stream, cancellable, NULL);

		g_object_unref (filtered_stream);

		string = "</div>\n";

		g_output_stream_write_all (
			stream, string, strlen (string),
			NULL, cancellable, NULL);

		if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
			string = "</body></html>";

			g_output_stream_write_all (
				stream, string, strlen (string),
				NULL, cancellable, NULL);
		}

		g_object_unref (mime_part);

		return TRUE;

	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		gchar *uri, *str;
		const gchar *default_charset, *charset;

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container-nostyle\" >"
			"<iframe width=\"100%%\" height=\"10\""
			" id=\"%s.iframe\" name=\"%s\" "
			" frameborder=\"0\" src=\"%s\" "
			" class=\"-e-mail-formatter-frame-color %s"
			" -e-web-view-text-color\" >"
			"</iframe>"
			"</div>",
			e_mail_part_get_id (part),
			e_mail_part_get_id (part),
			uri,
			e_mail_part_get_frame_security_style (part));

		g_output_stream_write_all (
			stream, str, strlen (str),
			NULL, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

	return TRUE;
}

static void
e_mail_formatter_text_plain_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Plain Text");
	class->description = _("Format part as plain text");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_text_plain_format;
}

static void
e_mail_formatter_text_plain_init (EMailFormatterExtension *extension)
{
}

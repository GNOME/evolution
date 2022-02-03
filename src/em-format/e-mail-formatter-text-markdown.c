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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-utils.h"

typedef EMailFormatterExtension EMailFormatterTextMarkdown;
typedef EMailFormatterExtensionClass EMailFormatterTextMarkdownClass;

GType e_mail_formatter_text_markdown_get_type (void);

G_DEFINE_TYPE (EMailFormatterTextMarkdown, e_mail_formatter_text_markdown, E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/markdown",
	NULL
};

static gboolean
emfe_text_markdown_format (EMailFormatterExtension *extension,
			   EMailFormatter *formatter,
			   EMailFormatterContext *context,
			   EMailPart *part,
			   GOutputStream *stream,
			   GCancellable *cancellable)
{
	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {
		CamelMimePart *mime_part;
		CamelDataWrapper *dw;
		GOutputStream *output_stream;
		gchar *html;

		mime_part = e_mail_part_ref_mime_part (part);
		dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		if (dw == NULL) {
			g_object_unref (mime_part);
			return FALSE;
		}

		output_stream = g_memory_output_stream_new_resizable ();

		e_mail_formatter_format_text (formatter, part, output_stream, cancellable);
		g_output_stream_flush (output_stream, cancellable, NULL);

		html = e_markdown_utils_text_to_html ((const gchar *) g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output_stream)),
			g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output_stream)));

		g_object_unref (output_stream);
		g_object_unref (mime_part);

		if (html) {
			const gchar *string;

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

			string =
				"<div class=\"part-container "
				"-e-web-view-background-color -e-web-view-text-color\" "
				"style=\"border: none; padding: 8px; margin: 0;\">";

			g_output_stream_write_all (
				stream, string, strlen (string),
				NULL, cancellable, NULL);

			g_output_stream_write_all (stream, html, strlen (html), NULL, cancellable, NULL);
			g_free (html);

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

			return TRUE;
		}

		return FALSE;

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
e_mail_formatter_text_markdown_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Markdown Text");
	class->description = _("Format part as markdown text");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_text_markdown_format;
}

static void
e_mail_formatter_text_markdown_init (EMailFormatterExtension *extension)
{
}

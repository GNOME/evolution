/*
 * e-mail-formatter-text-plain.c
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

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-inline-filter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef EMailFormatterExtension EMailFormatterTextPlain;
typedef EMailFormatterExtensionClass EMailFormatterTextPlainClass;

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
                        CamelStream *stream,
                        GCancellable *cancellable)
{
	CamelDataWrapper *dw;
	CamelStream *filtered_stream;
	CamelMimeFilter *html_filter;
	gchar *content;
	const gchar *format;
	guint32 filter_flags;
	guint32 rgb;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {

		if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
			gchar *header;
			header = e_mail_formatter_get_html_header (formatter);
			camel_stream_write_string (stream, header, cancellable, NULL);
			g_free (header);

			/* No need for body margins within <iframe> */
			camel_stream_write_string (
				stream,
				"<style>body{ margin: 0; }</style>",
				cancellable, NULL);
		}

		filter_flags = e_mail_formatter_get_text_format_flags (formatter);

		dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
		if (!dw)
			return FALSE;

		/* Check for RFC 2646 flowed text. */
		if (camel_content_type_is (dw->mime_type, "text", "plain")
		&& (format = camel_content_type_param (dw->mime_type, "format"))
		&& !g_ascii_strcasecmp (format, "flowed"))
			filter_flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

		rgb = e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (
				formatter, E_MAIL_FORMATTER_COLOR_CITATION));

		filtered_stream = camel_stream_filter_new (stream);
		html_filter = camel_mime_filter_tohtml_new (filter_flags, rgb);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream), html_filter);
		g_object_unref (html_filter);

		content = g_strdup_printf (
			"<div class=\"part-container pre\" style=\""
			"border: none; padding: 8px; margin: 0; "
			"background-color: #%06x; color: #%06x;\">\n",
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_CONTENT)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_TEXT)));

		camel_stream_write_string (stream, content, cancellable, NULL);
		e_mail_formatter_format_text (formatter, part, filtered_stream, cancellable);
		camel_stream_flush (filtered_stream, cancellable, NULL);

		g_object_unref (filtered_stream);
		g_free (content);

		camel_stream_write_string (stream, "</div>\n", cancellable, NULL);

		if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
			camel_stream_write_string (
				stream, "</body></html>",
				cancellable, NULL);
		}

		return TRUE;

	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		gchar *uri, *str;

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, part->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container-nostyle\" >"
			"<iframe width=\"100%%\" height=\"10\""
			" id=\"%s.iframe\" name=\"%s\" "
			" frameborder=\"0\" src=\"%s\" "
			" style=\"border: 1px solid #%06x; background-color: #%06x;\">"
			"</iframe>"
			"</div>",
			part->id, part->id, uri,
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_CONTENT)));

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

	return TRUE;
}

static const gchar *
emfe_text_plain_get_description (EMailFormatterExtension *extension)
{
	return _("Format part as plain text");
}

static void
e_mail_formatter_text_plain_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Plain Text");
	class->mime_types = formatter_mime_types;
	class->format = emfe_text_plain_format;
	class->get_description = emfe_text_plain_get_description;
}

static void
e_mail_formatter_text_plain_init (EMailFormatterExtension *extension)
{
}

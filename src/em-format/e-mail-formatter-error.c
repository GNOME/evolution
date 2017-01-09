/*
 * e-mail-formatter-error.c
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

typedef EMailFormatterExtension EMailFormatterError;
typedef EMailFormatterExtensionClass EMailFormatterErrorClass;

GType e_mail_formatter_error_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterError,
	e_mail_formatter_error,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.error",
	NULL
};

static gboolean
emfe_error_format (EMailFormatterExtension *extension,
                   EMailFormatter *formatter,
                   EMailFormatterContext *context,
                   EMailPart *part,
                   GOutputStream *stream,
                   GCancellable *cancellable)
{
	GOutputStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelMimePart *mime_part;
	CamelDataWrapper *dw;
	const gchar *string;
	gchar *html;
	gint icon_width, icon_height;

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &icon_width, &icon_height)) {
		icon_width = 48;
		icon_height = 48;
	}

	mime_part = e_mail_part_ref_mime_part (part);
	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	html = g_strdup_printf (
		"<div class=\"part-container -e-mail-formatter-frame-color "
		"-e-mail-formatter-body-color -e-web-view-text-color\">"
		"<div class=\"part-container-inner-margin pre\">\n"
		"<table border=\"0\" cellspacing=\"10\" "
		"cellpadding=\"0\" width=\"100%%\">\n"
		"<tr valign=\"top\"><td width=50>"
		"<img src=\"gtk-stock://%s/?size=%d\" width=\"%dpx\" height=\"%dpx\"/></td>\n"
		"<td style=\"color: red;\">",
		"dialog-error", GTK_ICON_SIZE_DIALOG, icon_width, icon_height);

	g_output_stream_write_all (
		stream, html, strlen (html), NULL, cancellable, NULL);

	g_free (html);

	filter = camel_mime_filter_tohtml_new (
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	filtered_stream = camel_filter_output_stream_new (stream, filter);
	g_filter_output_stream_set_close_base_stream (
		G_FILTER_OUTPUT_STREAM (filtered_stream), FALSE);
	g_object_unref (filter);

	camel_data_wrapper_decode_to_output_stream_sync (
		dw, filtered_stream, cancellable, NULL);
	g_output_stream_flush (filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	string = "</td></tr></table></div></div>";

	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_error_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_error_format;
}

static void
e_mail_formatter_error_init (EMailFormatterExtension *extension)
{
}

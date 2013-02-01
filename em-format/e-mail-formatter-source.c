/*
 * evolution-source.c
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

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterSource;
typedef EMailFormatterExtensionClass EMailFormatterSourceClass;

GType e_mail_formatter_source_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterSource,
	e_mail_formatter_source,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.source",
	NULL
};

static gboolean
emfe_source_format (EMailFormatterExtension *extension,
                    EMailFormatter *formatter,
                    EMailFormatterContext *context,
                    EMailPart *part,
                    CamelStream *stream,
                    GCancellable *cancellable)
{
	GString *buffer;
	CamelStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelDataWrapper *dw = (CamelDataWrapper *) part->part;

	filtered_stream = camel_stream_filter_new (stream);

	filter = camel_mime_filter_tohtml_new (
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), filter);
	g_object_unref (filter);

	buffer = g_string_new ("");

	if (CAMEL_IS_MIME_MESSAGE (part->part)) {
		g_string_append_printf (
			buffer,
			"<div class=\"part-container\" "
			"style=\"border: 0; background: #%06x; color: #%06x;\" >",
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_BODY)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_TEXT)));
	} else {
		g_string_append_printf (
			buffer,
			"<div class=\"part-container\" "
			"style=\"border-color: #%06x; background: #%06x; color: #%06x;\">"
			"<div class=\"part-container-inner-margin pre\">\n",
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_BODY)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_TEXT)));
	}

	camel_stream_write_string (
		stream, buffer->str, cancellable, NULL);
	camel_stream_write_string (
		stream, "<code class=\"pre\">", cancellable, NULL);

	camel_data_wrapper_write_to_stream_sync (
		dw, filtered_stream,
		cancellable, NULL);
	camel_stream_flush (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	camel_stream_write_string (
		stream, "</code>", cancellable, NULL);

	g_string_free (buffer, TRUE);

	if (CAMEL_IS_MIME_MESSAGE (part->part)) {
		camel_stream_write_string (stream, "</div>", cancellable, NULL);
	} else {
		camel_stream_write_string (stream, "</div></div>", cancellable, NULL);
	}

	return TRUE;
}

static void
e_mail_formatter_source_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Source");
	class->description = _("Display source of a MIME part");
	class->mime_types = formatter_mime_types;
	class->format = emfe_source_format;
}

static void
e_mail_formatter_source_init (EMailFormatterExtension *extension)
{
}

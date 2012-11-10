/*
 * e-mail-formatter-text-enriched.c
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
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

static const gchar *formatter_mime_types[] = { "text/enriched",
					       "text/richtext",
					       NULL };

typedef struct _EMailFormatterTextEnriched {
	GObject parent;
} EMailFormatterTextEnriched;

typedef struct _EMailFormatterTextEnrichedClass {
	GObjectClass parent_class;
} EMailFormatterTextEnrichedClass;

static void e_mail_formatter_formatter_extension_interace_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterTextEnriched,
	e_mail_formatter_text_enriched,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interace_init));

static gboolean
emfe_text_enriched_format (EMailFormatterExtension *extension,
                           EMailFormatter *formatter,
                           EMailFormatterContext *context,
                           EMailPart *part,
                           CamelStream *stream,
                           GCancellable *cancellable)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *enriched;
	guint32 filter_flags = 0;
	GString *buffer;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (!g_strcmp0 (part->mime_type, "text/richtext")) {
		filter_flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
	}

	enriched = camel_mime_filter_enriched_new (filter_flags);
	filtered_stream = camel_stream_filter_new (stream);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), enriched);
	g_object_unref (enriched);

	buffer = g_string_new ("");

	g_string_append_printf (
		buffer,
		"<div class=\"part-container\" style=\"border-color: #%06x; "
		"background-color: #%06x; color: #%06x;\">"
		"<div class=\"part-container-inner-margin\">\n",
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_CONTENT)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_TEXT)));

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);
	g_string_free (buffer, TRUE);

	e_mail_formatter_format_text (
		formatter, part, filtered_stream, cancellable);
	camel_stream_flush (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	camel_stream_write_string (stream, "</div></div>", cancellable, NULL);

	return TRUE;
}

static const gchar *
emfe_text_enriched_get_display_name (EMailFormatterExtension *extension)
{
	return _("Richtext");
}

static const gchar *
emfe_text_enriched_get_description (EMailFormatterExtension *extension)
{
	return _("Display part as enriched text");
}

static const gchar **
emfe_text_enriched_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_text_enriched_class_init (EMailFormatterTextEnrichedClass *class)
{
}

static void
e_mail_formatter_formatter_extension_interace_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_text_enriched_format;
	iface->get_display_name = emfe_text_enriched_get_display_name;
	iface->get_description = emfe_text_enriched_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_text_enriched_mime_types;
}

static void
e_mail_formatter_text_enriched_init (EMailFormatterTextEnriched *formatter)
{

}

/*
 * e-mail-formatter-quote-text-enriched.c
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

#include <camel/camel.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-quote.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterQuoteTextEnriched;
typedef EMailFormatterExtensionClass EMailFormatterQuoteTextEnrichedClass;

GType e_mail_formatter_quote_text_enriched_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterQuoteTextEnriched,
	e_mail_formatter_quote_text_enriched,
	E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/enriched",
	"text/richtext",
	NULL
};

static gboolean
emqfe_text_enriched_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            GOutputStream *stream,
                            GCancellable *cancellable)
{
	GOutputStream *filtered_stream;
	CamelMimeFilter *filter;
	const gchar *mime_type;
	const gchar *string;
	guint32 camel_flags = 0;

	mime_type = e_mail_part_get_mime_type (part);

	if (g_strcmp0 (mime_type, "text/richtext") == 0) {
		camel_flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
		string = "\n<!-- text/richtext -->\n";
	} else {
		string = "\n<!-- text/enriched -->\n";
	}
	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	string = "<br><hr><br>";
	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	filter = camel_mime_filter_enriched_new (camel_flags);
	filtered_stream = camel_filter_output_stream_new (stream, filter);
	g_filter_output_stream_set_close_base_stream (
		G_FILTER_OUTPUT_STREAM (filtered_stream), FALSE);
	g_object_unref (filter);

	e_mail_formatter_format_text (
		formatter, part, filtered_stream, cancellable);
	g_output_stream_flush (filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	return TRUE;
}

static void
e_mail_formatter_quote_text_enriched_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Richtext");
	class->description = _("Display part as enriched text");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_HIGH;
	class->format = emqfe_text_enriched_format;
}

static void
e_mail_formatter_quote_text_enriched_init (EMailFormatterExtension *extension)
{
}

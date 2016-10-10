/*
 * e-mail-formatter-quote-text-html.c
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-quote.h"
#include "e-mail-part-utils.h"
#include "e-mail-stripsig-filter.h"

typedef EMailFormatterExtension EMailFormatterQuoteTextHTML;
typedef EMailFormatterExtensionClass EMailFormatterQuoteTextHTMLClass;

GType e_mail_formatter_quote_text_html_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterQuoteTextHTML,
	e_mail_formatter_quote_text_html,
	E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/html",
	NULL
};

static gboolean
emqfe_text_html_format (EMailFormatterExtension *extension,
                        EMailFormatter *formatter,
                        EMailFormatterContext *context,
                        EMailPart *part,
                        GOutputStream *stream,
                        GCancellable *cancellable)
{
	EMailFormatterQuoteContext *qf_context;
	GOutputStream *filtered_stream;
	const gchar *string;

	qf_context = (EMailFormatterQuoteContext *) context;

	string = "<!-- text/html -->";
	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	filtered_stream = g_object_ref (stream);

	if ((qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG) == 0) {
		CamelMimeFilter *filter;
		GOutputStream *temp_stream;

		filter = e_mail_stripsig_filter_new (FALSE);
		temp_stream = camel_filter_output_stream_new (
			filtered_stream, filter);
		g_filter_output_stream_set_close_base_stream (
			G_FILTER_OUTPUT_STREAM (temp_stream), FALSE);
		g_object_unref (filtered_stream);
		filtered_stream = temp_stream;
		g_object_unref (filter);
	}

	e_mail_formatter_format_text (
		formatter, part, filtered_stream, cancellable);

	g_output_stream_flush (filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	return TRUE;
}

static void
e_mail_formatter_quote_text_html_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("HTML");
	class->description = _("Format part as HTML");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_HIGH;
	class->format = emqfe_text_html_format;
}

static void
e_mail_formatter_quote_text_html_init (EMailFormatterExtension *extension)
{
}

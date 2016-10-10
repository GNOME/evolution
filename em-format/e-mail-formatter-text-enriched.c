/*
 * e-mail-formatter-text-enriched.c
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

typedef EMailFormatterExtension EMailFormatterTextEnriched;
typedef EMailFormatterExtensionClass EMailFormatterTextEnrichedClass;

GType e_mail_formatter_text_enriched_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterTextEnriched,
	e_mail_formatter_text_enriched,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/enriched",
	"text/richtext",
	NULL
};

static gboolean
emfe_text_enriched_format (EMailFormatterExtension *extension,
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
	gchar *str;
	guint32 filter_flags = 0;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	mime_type = e_mail_part_get_mime_type (part);

	if (g_strcmp0 (mime_type, "text/richtext") == 0)
		filter_flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;

	filter = camel_mime_filter_enriched_new (filter_flags);
	filtered_stream = camel_filter_output_stream_new (stream, filter);
	g_filter_output_stream_set_close_base_stream (
		G_FILTER_OUTPUT_STREAM (filtered_stream), FALSE);
	g_object_unref (filter);

	str = g_strdup_printf (
		"<div class=\"part-container -e-mail-formatter-frame-color %s"
		"-e-web-view-background-color -e-web-view-text-color\">"
		"<div class=\"part-container-inner-margin\">\n",
		e_mail_part_get_frame_security_style (part));

	g_output_stream_write_all (
		stream, str, strlen (str), NULL, cancellable, NULL);
	g_free (str);

	e_mail_formatter_format_text (
		formatter, part, filtered_stream, cancellable);
	g_output_stream_flush (filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	string = "</div></div>";

	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	return TRUE;
}

static void
e_mail_formatter_text_enriched_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Richtext");
	class->description = _("Display part as enriched text");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_text_enriched_format;
}

static void
e_mail_formatter_text_enriched_init (EMailFormatterExtension *extension)
{
}

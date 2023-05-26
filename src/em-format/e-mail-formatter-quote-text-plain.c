/*
 * e-mail-formatter-quote-text-plain.c
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
#include "e-mail-part-utils.h"
#include "e-mail-stripsig-filter.h"

typedef EMailFormatterExtension EMailFormatterQuoteTextPlain;
typedef EMailFormatterExtensionClass EMailFormatterQuoteTextPlainClass;

GType e_mail_formatter_quote_text_plain_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterQuoteTextPlain,
	e_mail_formatter_quote_text_plain,
	E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/plain",
	NULL
};

static gboolean
emqfe_text_plain_format (EMailFormatterExtension *extension,
                         EMailFormatter *formatter,
                         EMailFormatterContext *context,
                         EMailPart *part,
                         GOutputStream *stream,
                         GCancellable *cancellable)
{
	GOutputStream *filtered_stream;
	GOutputStream *temp_stream;
	GSettings *settings;
	CamelMimeFilter *filter;
	CamelMimePart *mime_part;
	CamelContentType *type;
	EMailFormatterQuoteContext *qf_context;
	CamelMimeFilterToHTMLFlags text_flags;
	const gchar *format;
	guint32 rgb = 0x737373;

	mime_part = e_mail_part_ref_mime_part (part);
	if (mime_part == NULL)
		return FALSE;

	qf_context = (EMailFormatterQuoteContext *) context;

	text_flags =
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "composer-wrap-quoted-text-in-replies"))
		text_flags |= CAMEL_MIME_FILTER_TOHTML_DIV |
			      CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
			      CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			      CAMEL_MIME_FILTER_TOHTML_PRESERVE_TABS;
	else
		text_flags |= CAMEL_MIME_FILTER_TOHTML_PRE;

	g_clear_object (&settings);

	/* XXX Should we define a separate EMailFormatter property
	 *     for using CAMEL_MIME_FILTER_TOHTML_QUOTE_CITATION? */
	if (e_mail_formatter_get_mark_citations (formatter))
		text_flags |= CAMEL_MIME_FILTER_TOHTML_QUOTE_CITATION;

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (mime_part);
	if (camel_content_type_is (type, "text", "plain")
	    && (format = camel_content_type_param (type, "format"))
	    && !g_ascii_strcasecmp (format, "flowed"))
		text_flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

	filtered_stream = g_object_ref (stream);

	filter = camel_mime_filter_tohtml_new (text_flags, rgb);
	temp_stream = camel_filter_output_stream_new (filtered_stream, filter);
	g_filter_output_stream_set_close_base_stream (
		G_FILTER_OUTPUT_STREAM (temp_stream), FALSE);
	g_object_unref (filtered_stream);
	filtered_stream = temp_stream;
	g_object_unref (filter);

	if ((qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG) == 0) {
		filter = e_mail_stripsig_filter_new (TRUE);
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

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_quote_text_plain_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Plain Text");
	class->description = _("Format part as plain text");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_HIGH;
	class->format = emqfe_text_plain_format;
}

static void
e_mail_formatter_quote_text_plain_init (EMailFormatterExtension *extension)
{
}

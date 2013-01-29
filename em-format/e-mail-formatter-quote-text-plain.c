/*
 * e-mail-formatter-quote-text-plain.c
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

#include <camel/camel.h>

#include <e-util/e-util.h>

#include "e-mail-format-extensions.h"
#include "e-mail-formatter-quote.h"
#include "e-mail-part-utils.h"
#include "e-mail-stripsig-filter.h"

typedef EMailFormatterExtension EMailFormatterQuoteTextPlain;
typedef EMailFormatterExtensionClass EMailFormatterQuoteTextPlainClass;

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
                         CamelStream *stream,
                         GCancellable *cancellable)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelMimeFilter *sig_strip;
	CamelContentType *type;
	EMailFormatterQuoteContext *qf_context;
	const gchar *format;
	guint32 rgb = 0x737373, text_flags;

	if (!part->part)
		return FALSE;

	qf_context = (EMailFormatterQuoteContext *) context;

	text_flags = CAMEL_MIME_FILTER_TOHTML_PRE |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;

	if (e_mail_formatter_get_mark_citations (formatter)) {
		text_flags |= CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	}

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (part->part);
	if (camel_content_type_is (type, "text", "plain")
	    && (format = camel_content_type_param (type, "format"))
	    && !g_ascii_strcasecmp (format, "flowed"))
		text_flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

	filtered_stream = camel_stream_filter_new (stream);

	if ((qf_context->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG) == 0) {
		sig_strip = e_mail_stripsig_filter_new (TRUE);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream), sig_strip);
		g_object_unref (sig_strip);
	}

	html_filter = camel_mime_filter_tohtml_new (text_flags, rgb);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), html_filter);
	g_object_unref (html_filter);

	e_mail_formatter_format_text (
		formatter, part, filtered_stream, cancellable);

	camel_stream_flush (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	return TRUE;
}

static void
e_mail_formatter_quote_text_plain_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Plain Text");
	class->description = _("Format part as plain text");
	class->mime_types = formatter_mime_types;
	class->format = emqfe_text_plain_format;
}

static void
e_mail_formatter_quote_text_plain_init (EMailFormatterExtension *extension)
{
}

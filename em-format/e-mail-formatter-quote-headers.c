/*
 * e-mail-formatter-quote-headers.c
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

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-inline-filter.h>
#include <libemail-engine/e-mail-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailFormatterQuoteHeaders {
	GObject parent;
} EMailFormatterQuoteHeaders;

typedef struct _EMailFormatterQuoteHeadersClass {
	GObjectClass parent_class;
} EMailFormatterQuoteHeadersClass;

static const gchar *formatter_mime_types[] = { "application/vnd.evolution.headers", NULL };

static void e_mail_formatter_quote_formatter_extension_interface_init
					(EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_quote_mail_extension_interface_init
					(EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterQuoteHeaders,
	e_mail_formatter_quote_headers,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_quote_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_quote_formatter_extension_interface_init))

static gboolean
emqfe_headers_format (EMailFormatterExtension *extension,
                      EMailFormatter *formatter,
                      EMailFormatterContext *context,
                      EMailPart *part,
                      CamelStream *stream,
                      GCancellable *cancellable)
{
	CamelContentType *ct;
	const gchar *charset;
	GList *iter;
	GString *buffer;
	const GQueue *default_headers;

	if (!part)
		return FALSE;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part->part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);

	buffer = g_string_new ("");

        /* dump selected headers */
	default_headers = e_mail_formatter_get_headers (formatter);
	for (iter = default_headers->head; iter; iter = iter->next) {
		struct _camel_header_raw *raw_header;
		EMailFormatterHeader *h = iter->data;
		guint32 flags;

		flags = h->flags & ~E_MAIL_FORMATTER_HEADER_FLAG_HTML;
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE;

		for (raw_header = part->part->headers; raw_header; raw_header = raw_header->next) {

			if (g_strcmp0 (raw_header->name, h->name) == 0) {

				e_mail_formatter_format_header (
					formatter, buffer, (CamelMedium *) part->part,
					raw_header, flags, charset);

				g_string_append (buffer, "<br>\n");
				break;
			}
		}
	}
	g_string_append (buffer, "<br>\n");

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return TRUE;
}

static const gchar *
emqfe_headers_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emqfe_headers_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emqfe_headers_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_quote_headers_class_init (EMailFormatterQuoteHeadersClass *klass)
{
}

static void
e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emqfe_headers_format;
	iface->get_display_name = emqfe_headers_get_display_name;
	iface->get_description = emqfe_headers_get_description;
}

static void
e_mail_formatter_quote_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emqfe_headers_mime_types;
}

static void
e_mail_formatter_quote_headers_init (EMailFormatterQuoteHeaders *formatter)
{

}

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

static void e_mail_formatter_quote_formatter_extension_interface_init
					(EMailFormatterExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterQuoteHeaders,
	e_mail_formatter_quote_headers,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_quote_formatter_extension_interface_init))

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.headers",
	NULL
};

static void
emfqe_format_text_header (EMailFormatter *emf,
                          GString *buffer,
                          const gchar *label,
                          const gchar *value,
                          guint32 flags,
                          gint is_html)
{
	const gchar *html;
	gchar *mhtml = NULL;

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!is_html)
		html = mhtml = camel_text_to_html (value, 0, 0);
	else
		html = value;

	if (flags & E_MAIL_FORMATTER_HEADER_FLAG_BOLD)
		g_string_append_printf (
			buffer, "<b>%s</b>: %s<br>", label, html);
	else
		g_string_append_printf (
			buffer, "%s: %s<br>", label, html);

	g_free (mhtml);
}

/* XXX: This is copied in e-mail-formatter-utils.c */
static const gchar *addrspec_hdrs[] = {
	"Sender", "From", "Reply-To", "To", "Cc", "Bcc",
	"Resent-Sender", "Resent-From", "Resent-Reply-To",
	"Resent-To", "Resent-Cc", "Resent-Bcc", NULL
};

static void
emfqe_format_header (EMailFormatter *formatter,
                     GString *buffer,
                     CamelMedium *part,
                     struct _camel_header_raw *header,
                     guint32 flags,
                     const gchar *charset)
{
	CamelMimeMessage *msg = (CamelMimeMessage *) part;
	gchar *name, *buf, *value = NULL;
	const gchar *txt, *label;
	gboolean addrspec = FALSE;
	gint is_html = FALSE;
	gint i;

	name = g_alloca (strlen (header->name) + 1);
	strcpy (name, header->name);
	e_mail_formatter_canon_header_name (name);

	/* Never quote Bcc headers */
	if (g_str_equal (name, "Bcc") || g_str_equal (name, "Resent-Bcc"))
		return;

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp (name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;

		if (!(txt = camel_medium_get_header (part, name)))
			return;

		buf = camel_header_unfold (txt);
		addrs = camel_header_address_decode (
			txt, e_mail_formatter_get_charset (formatter) ?
			e_mail_formatter_get_charset (formatter) :
			e_mail_formatter_get_default_charset (formatter));
		if (addrs == NULL) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new ("");
		e_mail_formatter_format_address (formatter, html,
			addrs, name, FALSE, FALSE);
		camel_header_address_unref (addrs);
		txt = value = html->str;
		g_string_free (html, FALSE);
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
		is_html = TRUE;
	} else if (!strcmp (name, "Subject")) {
		txt = camel_mime_message_get_subject (msg);
		label = _("Subject");
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp (name, "X-Evolution-Mailer")) { /* pseudo-header */
		if (!(txt = camel_medium_get_header (part, "x-mailer")))
			if (!(txt = camel_medium_get_header (part, "user-agent")))
				if (!(txt = camel_medium_get_header (part, "x-newsreader")))
					if (!(txt = camel_medium_get_header (part, "x-mimeole")))
						return;

		txt = value = camel_header_format_ctext (txt, charset);

		label = _("Mailer");
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else if (!strcmp (name, "Date") || !strcmp (name, "Resent-Date")) {
		if (!(txt = camel_medium_get_header (part, name)))
			return;

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
	} else {
		txt = camel_medium_get_header (part, name);
		buf = camel_header_unfold (txt);
		txt = value = camel_header_decode_string (txt, charset);
		g_free (buf);
	}

	emfqe_format_text_header (formatter, buffer, label, txt, flags, is_html);

	g_free (value);
}

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

				emfqe_format_header (
					formatter, buffer, (CamelMedium *) part->part,
					raw_header, flags, charset);
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

static void
e_mail_formatter_quote_headers_class_init (EMailFormatterQuoteHeadersClass *class)
{
}

static void
e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->mime_types = formatter_mime_types;
	iface->format = emqfe_headers_format;
	iface->get_display_name = emqfe_headers_get_display_name;
	iface->get_description = emqfe_headers_get_description;
}

static void
e_mail_formatter_quote_headers_init (EMailFormatterQuoteHeaders *formatter)
{

}

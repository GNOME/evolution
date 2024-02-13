/*
 * e-mail-formatter-quote-headers.c
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
#include <libemail-engine/libemail-engine.h>

#include "e-mail-formatter-quote.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-headers.h"

#define HEADER_PREFIX "<div class=\"-x-evo-paragraph\" data-headers>"
#define HEADER_SUFFIX "</div>"

typedef EMailFormatterExtension EMailFormatterQuoteHeaders;
typedef EMailFormatterExtensionClass EMailFormatterQuoteHeadersClass;

GType e_mail_formatter_quote_headers_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterQuoteHeaders,
	e_mail_formatter_quote_headers,
	E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION)

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

	g_string_append_printf (buffer, HEADER_PREFIX);

	if ((flags & E_MAIL_FORMATTER_HEADER_FLAG_BOLD) &&
	    !(flags & E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING))
		g_string_append_printf (
			buffer, "<b>%s</b>: %s", label, html);
	else
		g_string_append_printf (
			buffer, "%s: %s", label, html);

	g_string_append_printf (buffer, HEADER_SUFFIX);

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
		     EMailFormatterContext *context,
                     GString *buffer,
                     EMailPart *part,
                     const gchar *header_name,
                     const gchar *charset)
{
	CamelMimePart *mime_part;
	EMailFormatterHeaderFlags flags;
	gchar *canon_name, *buf, *value = NULL;
	const gchar *txt, *label;
	gboolean addrspec = FALSE;
	gint is_html = FALSE;
	gint i;

	/* Skip Face header in prints, which includes also message forward */
	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING &&
	    g_ascii_strcasecmp (header_name, "Face") == 0)
		return;

	flags = E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE;

	if ((context->flags & E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING) != 0)
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING;

	canon_name = g_alloca (strlen (header_name) + 1);
	strcpy (canon_name, header_name);
	e_mail_formatter_canon_header_name (canon_name);

	/* Never quote Bcc/Resent-Bcc headers. */
	if (g_str_equal (canon_name, "Bcc"))
		return;
	if (g_str_equal (canon_name, "Resent-Bcc"))
		return;

	mime_part = e_mail_part_ref_mime_part (part);

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (g_str_equal (canon_name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(canon_name);

	if (addrspec) {
		CamelMedium *medium;
		struct _camel_header_address *addrs;
		GString *html;
		gchar *fmt_charset;

		medium = CAMEL_MEDIUM (mime_part);
		txt = camel_medium_get_header (medium, canon_name);
		if (txt == NULL) {
			g_object_unref (mime_part);
			return;
		}

		fmt_charset = e_mail_formatter_dup_charset (formatter);
		if (!fmt_charset)
			fmt_charset = e_mail_formatter_dup_default_charset (formatter);

		buf = camel_header_unfold (txt);
		addrs = camel_header_address_decode (txt, fmt_charset);
		g_free (fmt_charset);

		if (addrs == NULL) {
			g_free (buf);
			g_object_unref (mime_part);
			return;
		}

		g_free (buf);

		html = g_string_new ("");
		e_mail_formatter_format_address (formatter, html,
			addrs, canon_name, FALSE, FALSE);
		camel_header_address_unref (addrs);
		txt = value = g_string_free (html, FALSE);
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;
		is_html = TRUE;

	} else if (g_str_equal (canon_name, "Subject")) {
		CamelMimeMessage *message;

		message = CAMEL_MIME_MESSAGE (mime_part);
		txt = camel_mime_message_get_subject (message);
		label = _("Subject");
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "X-Evolution-Mailer")) {
		CamelMedium *medium;

		medium = CAMEL_MEDIUM (mime_part);
		txt = camel_medium_get_header (medium, "user-agent");
		if (txt == NULL)
			txt = camel_medium_get_header (medium, "x-mailer");
		if (txt == NULL)
			txt = camel_medium_get_header (medium, "x-newsreader");
		if (txt == NULL)
			txt = camel_medium_get_header (medium, "x-mimeole");
		if (txt == NULL) {
			g_object_unref (mime_part);
			return;
		}

		txt = value = camel_header_format_ctext (txt, charset);

		label = _("Mailer");
		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

	} else if (g_str_equal (canon_name, "Date") ||
		   g_str_equal (canon_name, "Resent-Date")) {
		CamelMedium *medium;
		GSettings *settings;
		time_t date;
		gint offset = 0;

		medium = CAMEL_MEDIUM (mime_part);
		txt = camel_medium_get_header (medium, canon_name);
		if (txt == NULL) {
			g_object_unref (mime_part);
			return;
		}

		flags |= E_MAIL_FORMATTER_HEADER_FLAG_BOLD;

		date = camel_header_decode_date (txt, &offset);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (date > 0 && ((!offset && g_settings_get_boolean (settings, "composer-reply-credits-utc-to-localtime")) ||
		    g_settings_get_boolean (settings, "composer-reply-credits-to-localtime"))) {
			struct tm local;

			e_localtime_with_offset (date, &local, &offset);

			if (g_settings_get_boolean (settings, "composer-reply-credits-date-user-format")) {
				value = e_datetime_format_format_tm ("mail", "header", DTFormatKindDateTime, &local);
			} else {
				gint tzone = 0;

				tzone = offset / 3600;
				tzone = (tzone * 100) + ((offset / 60) % 60);

				value = camel_header_format_date (date, tzone);
			}
		} else if (date > 0 && g_settings_get_boolean (settings, "composer-reply-credits-date-user-format")) {
			value = e_datetime_format_format ("mail", "header", DTFormatKindDateTime, date);
		}

		if (value && *value)
			txt = value;

		g_object_unref (settings);
	} else {
		CamelMedium *medium;

		medium = CAMEL_MEDIUM (mime_part);
		txt = camel_medium_get_header (medium, canon_name);
		buf = camel_header_unfold (txt);
		txt = value = camel_header_decode_string (txt, charset);
		g_free (buf);
	}

	emfqe_format_text_header (formatter, buffer, label, txt, flags, is_html);

	g_free (value);

	g_object_unref (mime_part);
}

static gboolean
emqfe_headers_format (EMailFormatterExtension *extension,
                      EMailFormatter *formatter,
                      EMailFormatterContext *context,
                      EMailPart *part,
                      GOutputStream *stream,
                      GCancellable *cancellable)
{
	CamelContentType *ct;
	CamelMimePart *mime_part;
	const gchar *charset;
	GString *buffer;
	gchar **default_headers;
	guint ii, length = 0;

	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), FALSE);

	mime_part = e_mail_part_ref_mime_part (part);

	ct = camel_mime_part_get_content_type (mime_part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);

	buffer = g_string_new ("");

	/* dump selected headers */

	default_headers = e_mail_part_headers_dup_default_headers (
		E_MAIL_PART_HEADERS (part));
	if (default_headers != NULL)
		length = g_strv_length (default_headers);

	for (ii = 0; ii < length; ii++)
		emfqe_format_header (
			formatter, context, buffer, part,
			default_headers[ii], charset);

	g_strfreev (default_headers);

	g_string_append (buffer, HEADER_PREFIX);
	g_string_append (buffer, "<br>");
	g_string_append (buffer, HEADER_SUFFIX);

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	g_string_free (buffer, TRUE);

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_quote_headers_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_HIGH;
	class->format = emqfe_headers_format;
}

static void
e_mail_formatter_quote_headers_init (EMailFormatterExtension *extension)
{
}

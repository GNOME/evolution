/*
 * e-mail-formatter-headers.c
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

#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>
#include "e-util/e-util-private.h"
#include <libemail-engine/libemail-engine.h>
#include <shell/e-shell.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-headers.h"

typedef EMailFormatterExtension EMailFormatterHeaders;
typedef EMailFormatterExtensionClass EMailFormatterHeadersClass;

GType e_mail_formatter_headers_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterHeaders,
	e_mail_formatter_headers,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	E_MAIL_PART_HEADERS_MIME_TYPE,
	"text/rfc822-headers",
	NULL
};

static void
format_short_headers (EMailFormatter *formatter,
                      GString *buffer,
                      EMailPart *part,
                      guint32 flags,
                      GCancellable *cancellable)
{
	CamelMimePart *mime_part;
	GtkTextDirection direction;
	gchar *hdr_charset;
	gchar *evolution_imagesdir;
	gchar *subject = NULL;
	struct _camel_header_address *addrs = NULL;
	const CamelNameValueArray *headers;
	guint ii, len;
	gint icon_width, icon_height;
	GString *from;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	mime_part = e_mail_part_ref_mime_part (part);
	direction = gtk_widget_get_default_direction ();

	hdr_charset = e_mail_formatter_dup_charset (formatter);
	if (!hdr_charset)
		hdr_charset = e_mail_formatter_dup_default_charset (formatter);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);
	from = g_string_new ("");

	g_string_append_printf (
		buffer,
		"<table class=\"header\" "
		"id=\"__evo-short-headers\" style=\"display: %s\">",
		flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED ? "table" : "none");

	headers = camel_medium_get_headers (CAMEL_MEDIUM (mime_part));
	len = camel_name_value_array_get_length (headers);
	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, ii, &header_name, &header_value) ||
		    !header_name)
			continue;

		if (!g_ascii_strcasecmp (header_name, "From")) {
			GString *tmp;
			if (!(addrs = camel_header_address_decode (header_value, hdr_charset))) {
				continue;
			}
			tmp = g_string_new ("");
			e_mail_formatter_format_address (
				formatter, tmp, addrs, header_name, FALSE,
				!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			if (tmp->len > 0)
				g_string_printf (
					from, "%s: %s",
					_("From"), tmp->str);
			g_string_free (tmp, TRUE);

		} else if (!g_ascii_strcasecmp (header_name, "Subject")) {
			gchar *buf = NULL;
			subject = camel_header_unfold (header_value);
			buf = camel_header_decode_string (subject, hdr_charset);
			g_free (subject);
			subject = camel_text_to_html (
				buf, CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
			g_free (buf);
		}
	}

	g_free (hdr_charset);

	g_string_append (buffer, "<tr class=\"header\">");
	if (direction == GTK_TEXT_DIR_RTL)
		g_string_append (buffer, "<td class=\"header rtl\">");
	else
		g_string_append (buffer, "<td class=\"header ltr\">");
	g_string_append (buffer, "<strong>");
	if (subject != NULL && *subject != '\0')
		g_string_append (buffer, subject);
	else
		g_string_append (buffer, _("(no subject)"));
	g_string_append (buffer, "</strong>");
	if (from->len > 0)
		g_string_append_printf (buffer, " (%s)", from->str);
	g_string_append (buffer, "</td>");

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, &icon_height)) {
		icon_width = 16;
		icon_height = 16;
	}

	g_string_append (buffer, "<td align=\"right\" valign=\"top\">");
	g_string_append_printf (buffer,
		"<img src=\"gtk-stock://insert-image/?size=%d\" width=\"%dpx\" height=\"%dpx\""
		" id=\"__evo-remote-content-img-small\" class=\"__evo-remote-content-img\" title=\"%s\" style=\"cursor:pointer;\" hidden/>",
		GTK_ICON_SIZE_MENU, icon_width, icon_height,
		_("Remote content download had been blocked for this message."));
	g_string_append_printf (buffer,
		"&nbsp;"
		"<img src=\"gtk-stock://stock_signature/?size=%d\" width=\"%dpx\" height=\"%dpx\""
		" id=\"__evo-autocrypt-import-img-small\" class=\"__evo-autocrypt-import-img\" title=\"%s\" style=\"cursor:pointer;\" hidden/>",
		GTK_ICON_SIZE_MENU, icon_width, icon_height,
		_("Import OpenPGP key provided in this message"));
	g_string_append (buffer, "</td>");

	g_string_append (buffer, "</tr></table>");

	g_free (subject);
	if (addrs)
		camel_header_address_list_clear (&addrs);

	g_string_free (from, TRUE);
	g_free (evolution_imagesdir);

	g_object_unref (mime_part);
}

static void
write_contact_picture (CamelMimePart *mime_part,
                       gint size,
                       GString *buffer)
{
	gchar *b64, *content_type;
	CamelDataWrapper *dw;
	CamelContentType *ct;
	GByteArray *ba = NULL;

	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	if (dw != NULL)
		ba = camel_data_wrapper_get_byte_array (dw);

	if (ba == NULL || ba->len == 0) {
		const gchar *filename;

		filename = camel_mime_part_get_filename (mime_part);

		if (filename != NULL) {
			if (size >= 0) {
				g_string_append_printf (
					buffer,
					"<img width=\"%dpx\" src=\"evo-file://%s\" />",
					size, filename);
			} else {
				g_string_append_printf (
					buffer,
					"<img src=\"evo-file://%s\" />",
					filename);
			}
		}

		return;
	}

	b64 = g_base64_encode (ba->data, ba->len);
	ct = camel_mime_part_get_content_type (mime_part);
	content_type = camel_content_type_simple (ct);

	if (size >= 0) {
		g_string_append_printf (
			buffer,
			"<img width=\"%dpx\" src=\"data:%s;base64,%s\">",
			size, content_type, b64);
	} else {
		g_string_append_printf (
			buffer,
			"<img src=\"data:%s;base64,%s\">",
			content_type, b64);
	}

	g_free (b64);
	g_free (content_type);
}

static void
format_full_headers (EMailFormatter *formatter,
                     GString *buffer,
                     EMailPart *part,
                     EMailFormatterContext *context,
		     gboolean is_rfc822_headers,
                     GCancellable *cancellable)
{
	guint32 mode = context->mode;
	guint32 flags = context->flags;
	CamelMimePart *mime_part;
	const gchar *charset;
	CamelContentType *ct;
	const CamelNameValueArray *headers;
	const gchar *photo_name = NULL;
	guchar *face_header_value = NULL;
	gsize face_header_len = 0;
	gchar *header_sender = NULL, *header_from = NULL, *name;
	gboolean mail_from_delegate = FALSE;
	gboolean show_sender_photo;
	gchar *hdr_charset;
	gchar *evolution_imagesdir;
	const gchar *direction;
	guint ii, len;
	guint32 formatting_flag = 0;
	gint icon_width, icon_height;

	g_return_if_fail (E_IS_MAIL_PART_HEADERS (part));

	if (g_cancellable_is_cancelled (cancellable))
		return;

	mime_part = e_mail_part_ref_mime_part (part);

	switch (gtk_widget_get_default_direction ()) {
		case GTK_TEXT_DIR_RTL:
			direction = "rtl";
			break;
		case GTK_TEXT_DIR_LTR:
			direction = "ltr";
			break;
		default:
			direction = "inherit";
			break;
	}

	if (is_rfc822_headers || (context->flags & E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING) != 0)
		formatting_flag |= E_MAIL_FORMATTER_HEADER_FLAG_NO_FORMATTING;

	ct = camel_mime_part_get_content_type (mime_part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
	hdr_charset = e_mail_formatter_dup_charset (formatter);
	if (!hdr_charset)
		hdr_charset = e_mail_formatter_dup_default_charset (formatter);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);

	g_string_append_printf (
		buffer,
		"<table cellspacing=\"0\" cellpadding=\"0\" "
		"border=\"0\" width=\"100%%\" "
		"id=\"__evo-full-%sheaders\" "
		"style=\"display: %s; direction: %s;\">",
		is_rfc822_headers ? "rfc822-" : "",
		flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED ? "none" : "table",
		direction);

	headers = camel_medium_get_headers (CAMEL_MEDIUM (mime_part));
	len = camel_name_value_array_get_length (headers);
	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;

		if (!camel_name_value_array_get (headers, ii, &header_name, &header_value) ||
		    !header_name)
			continue;

		if (!g_ascii_strcasecmp (header_name, "Sender")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header_value, hdr_charset)))
				break;

			html = g_string_new ("");
			name = e_mail_formatter_format_address (
				formatter, html, addrs, header_name, FALSE,
				~(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			camel_header_address_list_clear (&addrs);
			header_sender = g_string_free (html, FALSE);

			g_free (name);

		} else if (!g_ascii_strcasecmp (header_name, "From")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header_value, hdr_charset)))
				break;

			html = g_string_new ("");
			name = e_mail_formatter_format_address (
				formatter, html, addrs, header_name, FALSE,
				!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			camel_header_address_list_clear (&addrs);
			header_from = g_string_free (html, FALSE);

			g_free (name);

		} else if (!g_ascii_strcasecmp (header_name, "X-Evolution-Mail-From-Delegate")) {
			mail_from_delegate = TRUE;
		}
	}

	g_free (hdr_charset);

	if (!is_rfc822_headers && header_sender && header_from && mail_from_delegate) {
		gchar *bold_sender, *bold_from;

		g_string_append (
			buffer,
			"<tr valign=\"top\"><td><table border=1 width=\"100%\" "
			"cellspacing=2 cellpadding=2><tr>");
		if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
			g_string_append (
				buffer, "<td align=\"right\" width=\"100%\">");
		else
			g_string_append (
				buffer, "<td align=\"left\" width=\"100%\">");
		bold_sender = g_strconcat ("<b>", header_sender, "</b>", NULL);
		bold_from = g_strconcat ("<b>", header_from, "</b>", NULL);
		g_string_append_printf (
			buffer,
			/* Translators: This message suggests to the recipients
			 * that the sender of the mail is different from the one
			 * listed in From field. */
			_("This message was sent by %s on behalf of %s"),
			bold_sender, bold_from);
		g_string_append (buffer, "</td></tr></table></td></tr>");
		g_free (bold_sender);
		g_free (bold_from);
	}

	g_free (header_sender);
	g_free (header_from);

	g_string_append (
		buffer,
		"<tr valign=\"top\"><td width=\"100%\">"
		"<table class=\"header\">\n");

	g_free (evolution_imagesdir);

	/* dump selected headers */
	if ((mode & E_MAIL_FORMATTER_MODE_ALL_HEADERS) != 0) {
		for (ii = 0; ii < len; ii++) {
			const gchar *header_name = NULL, *header_value = NULL;

			if (camel_name_value_array_get (headers, ii, &header_name, &header_value) && header_name) {
				e_mail_formatter_format_header (formatter, buffer, header_name, header_value,
					E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS | formatting_flag, charset);
			}
		}
		e_mail_formatter_format_security_header (formatter, context, buffer, part, E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS);
	} else {
		CamelMedium *medium;
		gchar **default_headers;
		guint length = 0;

		medium = CAMEL_MEDIUM (mime_part);

		default_headers =
			e_mail_part_headers_dup_default_headers (
			E_MAIL_PART_HEADERS (part));
		if (default_headers != NULL)
			length = g_strv_length (default_headers);

		for (ii = 0; ii < length; ii++) {
			const gchar *header_name;
			const gchar *header_value = NULL;

			header_name = default_headers[ii];

			/* X-Evolution-Mailer is a pseudo-header and
			 * requires special treatment to extract the
			 * real header value. */
			if (g_ascii_strcasecmp (header_name, "X-Evolution-Mailer") == 0) {
				/* Check for "X-MimeOLE" last,
				 * as it's the least preferred. */
				if (header_value == NULL)
					header_value = camel_medium_get_header (
						medium, "User-Agent");
				if (header_value == NULL)
					header_value = camel_medium_get_header (
						medium, "X-Mailer");
				if (header_value == NULL)
					header_value = camel_medium_get_header (
						medium, "X-Newsreader");
				if (header_value == NULL)
					header_value = camel_medium_get_header (
						medium, "X-MimeOLE");
			} else {
				header_value = camel_medium_get_header (
					medium, header_name);
			}

			if (header_value == NULL)
				continue;

			if (g_ascii_strcasecmp (header_name, "From") == 0)
				photo_name = header_value;

			if (g_ascii_strcasecmp (header_name, "Face") == 0) {
				if (face_header_value == NULL) {
					const gchar *cp = header_value;

					/* Skip over spaces */
					while (*cp == ' ')
						cp++;

					face_header_value = g_base64_decode (
						cp, &face_header_len);
					face_header_value = g_realloc (
						face_header_value,
						face_header_len + 1);
					face_header_value[face_header_len] = 0;
				}
				continue;
			}

			e_mail_formatter_format_header (
				formatter, buffer,
				header_name,
				header_value,
				formatting_flag, charset);
		}

		g_strfreev (default_headers);
		e_mail_formatter_format_security_header (formatter, context, buffer, part, 0);
	}

	g_string_append (buffer, "</table></td>");

	show_sender_photo = !is_rfc822_headers &&
		e_mail_formatter_get_show_sender_photo (formatter);

	/* Prefer contact photos over archaic "Face" headers. */
	if (show_sender_photo && photo_name != NULL) {
		gchar *escaped_name;

		escaped_name = g_uri_escape_string (photo_name, NULL, FALSE);
		g_string_append (
			buffer,
			"<td align=\"right\" valign=\"top\">");
		g_string_append_printf (
			buffer,
			"<img src=\"mail://contact-photo?mailaddr=\" "
			"data-mailaddr=\"%s\" id=\"__evo-contact-photo\"/>",
			escaped_name);
		g_string_append (buffer, "</td>");

		g_free (escaped_name);

	} else if (!is_rfc822_headers && face_header_value != NULL) {
		CamelMimePart *image_part;

		image_part = camel_mime_part_new ();
		camel_mime_part_set_content (
			image_part,
			(const gchar *) face_header_value,
			face_header_len, "image/png");

		g_string_append (
			buffer,
			"<td align=\"right\" valign=\"top\">");
		write_contact_picture (image_part, 48, buffer);
		g_string_append (buffer, "</td>");

		g_object_unref (image_part);
	}

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_LARGE_TOOLBAR, &icon_width, &icon_height)) {
		icon_width = 24;
		icon_height = 24;
	}

	g_string_append (buffer, "<td align=\"right\" valign=\"top\">");
	g_string_append_printf (buffer,
		"<img src=\"gtk-stock://insert-image/?size=%d\" width=\"%dpx\" height=\"%dpx\""
		" id=\"__evo-remote-content-img-large\" class=\"__evo-remote-content-img\" title=\"%s\" style=\"cursor:pointer;\" hidden/>",
		GTK_ICON_SIZE_LARGE_TOOLBAR, icon_width, icon_height,
		_("Remote content download had been blocked for this message."));
	g_string_append_printf (buffer,
		"&nbsp;"
		"<img src=\"gtk-stock://stock_signature/?size=%d\" width=\"%dpx\" height=\"%dpx\""
		" id=\"__evo-autocrypt-import-img-large\" class=\"__evo-autocrypt-import-img\" title=\"%s\" style=\"cursor:pointer;\" hidden/>",
		GTK_ICON_SIZE_LARGE_TOOLBAR, icon_width, icon_height,
		_("Import OpenPGP key provided in this message"));
	g_string_append (buffer, "</td>");

	g_string_append (buffer, "</tr></table>");

	g_free (face_header_value);
	g_object_unref (mime_part);
}

static gboolean
emfe_headers_format (EMailFormatterExtension *extension,
                     EMailFormatter *formatter,
                     EMailFormatterContext *context,
                     EMailPart *part,
                     GOutputStream *stream,
                     GCancellable *cancellable)
{
	CamelMimePart *mime_part;
	GString *buffer;
	const gchar *direction, *mime_type;
	gboolean is_collapsable;
	gboolean is_collapsed;
	gboolean is_rfc822_headers;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	mime_part = e_mail_part_ref_mime_part (part);
	if (mime_part == NULL)
		return FALSE;

	switch (gtk_widget_get_default_direction ()) {
		case GTK_TEXT_DIR_RTL:
			direction = "rtl";
			break;
		case GTK_TEXT_DIR_LTR:
			direction = "ltr";
			break;
		default:
			direction = "inherit";
			break;
	}

	mime_type = e_mail_part_get_mime_type (part);
	is_rfc822_headers = mime_type && g_ascii_strcasecmp (mime_type, "text/rfc822-headers") == 0;

	if (is_rfc822_headers && (context->mode == E_MAIL_FORMATTER_MODE_PRINTING || !E_IS_MAIL_PART_HEADERS (part))) {
		g_object_unref (mime_part);

		return e_mail_formatter_format_as (formatter, context, part, stream, "text/plain", cancellable);
	}

	is_collapsable = !is_rfc822_headers &&
		(context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE);

	is_collapsed = !is_rfc822_headers &&
		(context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED);

	buffer = g_string_new ("");

	if (is_rfc822_headers) {
		g_string_append_printf (buffer,
			"<div class=\"headers pre -e-mail-formatter-body-color -e-mail-formatter-frame-color\""
			" style=\"border-width: 1px; border-style: solid;\" id=\"%s\">"
			"<table class=\"part-container -e-web-view-background-color -e-web-view-text-color\""
			" border=\"0\" width=\"100%%\" style=\"border: none; padding: 8px; margin: 0; direction: %s; border-spacing: 0px;\">"
			"<tr>",
			e_mail_part_get_id (part),
			direction);
	} else {
		g_string_append_printf (
			buffer,
			"%s id=\"%s\"><table class=\"-e-mail-formatter-header-color\" border=\"0\" width=\"100%%\" "
			"style=\"direction: %s; border-spacing: 0px\">"
			"<tr>",
			(context->mode != E_MAIL_FORMATTER_MODE_PRINTING) ?
				"<div class=\"headers -e-mail-formatter-body-color\"" :
				"<div class=\"headers\" style=\"background-color: #ffffff;\"",
			e_mail_part_get_id (part),
			direction);
	}

	if (is_collapsable) {
		gint icon_width, icon_height;

		if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &icon_width, &icon_height)) {
			icon_width = 16;
			icon_height = 16;
		}

		g_string_append_printf (
			buffer,
			"<td valign=\"top\" width=\"18\" style=\"padding-left: 0px\">"
			"<button type=\"button\" class=\"header-collapse\" id=\"__evo-collapse-headers-img\">"
			"<img src=\"gtk-stock://%s?size=%d\" width=\"%dpx\" height=\"%dpx\" class=\"-evo-color-scheme-light\"/>"
			"<img src=\"gtk-stock://%s?size=%d&amp;color-scheme=dark\" width=\"%dpx\" height=\"%dpx\" class=\"-evo-color-scheme-dark\"/>"
			"</button>"
			"</td>",
			is_collapsed ? "x-evolution-pan-end" : "x-evolution-pan-down", GTK_ICON_SIZE_BUTTON, icon_width, icon_height,
			is_collapsed ? "x-evolution-pan-end" : "x-evolution-pan-down", GTK_ICON_SIZE_BUTTON, icon_width, icon_height);
	}

	g_string_append (buffer, "<td>");

	if (is_collapsable)
		format_short_headers (
			formatter,
			buffer, part,
			context->flags,
			cancellable);

	format_full_headers (
		formatter,
		buffer,
		part,
		context,
		is_rfc822_headers,
		cancellable);

	g_string_append (buffer, "</td>");

	g_string_append (buffer, "</tr></table></div>");

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	g_string_free (buffer, TRUE);

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_headers_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_headers_format;
}

static void
e_mail_formatter_headers_init (EMailFormatterExtension *extension)
{
}

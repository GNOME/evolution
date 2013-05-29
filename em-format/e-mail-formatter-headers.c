/*
 * e-mail-formatter-headers.c
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libemail-engine/e-mail-utils.h>
#include <libedataserver/libedataserver.h>
#include <e-util/e-util.h>
#include <shell/e-shell.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterHeaders;
typedef EMailFormatterExtensionClass EMailFormatterHeadersClass;

GType e_mail_formatter_headers_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterHeaders,
	e_mail_formatter_headers,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.headers",
	NULL
};

static void
format_short_headers (EMailFormatter *formatter,
                      GString *buffer,
                      CamelMedium *part,
                      guint32 flags,
                      GCancellable *cancellable)
{
	GtkTextDirection direction;
	const gchar *charset;
	CamelContentType *ct;
	gchar *hdr_charset;
	gchar *evolution_imagesdir;
	gchar *subject = NULL;
	struct _camel_header_address *addrs = NULL;
	struct _camel_header_raw *header;
	GString *from;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	direction = gtk_widget_get_default_direction ();

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
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

	header = ((CamelMimePart *) part)->headers;
	while (header) {
		if (!g_ascii_strcasecmp (header->name, "From")) {
			GString *tmp;
			if (!(addrs = camel_header_address_decode (header->value, hdr_charset))) {
				header = header->next;
				continue;
			}
			tmp = g_string_new ("");
			e_mail_formatter_format_address (
				formatter, tmp, addrs, header->name, FALSE,
				!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			if (tmp->len > 0)
				g_string_printf (
					from, "%s: %s",
					_("From"), tmp->str);
			g_string_free (tmp, TRUE);

		} else if (!g_ascii_strcasecmp (header->name, "Subject")) {
			gchar *buf = NULL;
			subject = camel_header_unfold (header->value);
			buf = camel_header_decode_string (subject, hdr_charset);
			g_free (subject);
			subject = camel_text_to_html (
				buf, CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
			g_free (buf);
		}
		header = header->next;
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
	g_string_append (buffer, "</td></tr>");

	g_string_append (buffer, "</table>");

	g_free (subject);
	if (addrs)
		camel_header_address_list_clear (&addrs);

	g_string_free (from, TRUE);
	g_free (evolution_imagesdir);
}

static void
write_contact_picture (CamelMimePart *part,
                       gint size,
                       GString *buffer)
{
	gchar *b64, *content_type;
	CamelDataWrapper *dw;
	CamelContentType *ct;
	GByteArray *ba;

	ba = NULL;
	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	if (dw) {
		ba = camel_data_wrapper_get_byte_array (dw);
	}

	if (!ba || ba->len == 0) {

		if (camel_mime_part_get_filename (part)) {

			if (size >= 0) {
				g_string_append_printf (
					buffer,
					"<img width=\"%d\" src=\"evo-file://%s\" />",
					size, camel_mime_part_get_filename (part));
			} else {
				g_string_append_printf (
					buffer,
					"<img src=\"evo-file://%s\" />",
					camel_mime_part_get_filename (part));
			}
		}

		return;
	}

	b64 = g_base64_encode (ba->data, ba->len);
	ct = camel_mime_part_get_content_type (part);
	content_type = camel_content_type_simple (ct);

	if (size >= 0) {
		g_string_append_printf (
			buffer,
			"<img width=\"%d\" src=\"data:%s;base64,%s\">",
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
                     CamelMedium *part,
                     guint32 mode,
                     guint32 flags,
                     GCancellable *cancellable)
{
	const gchar *charset;
	CamelContentType *ct;
	struct _camel_header_raw *header;
	const gchar *photo_name = NULL;
	gboolean face_decoded  = FALSE, contact_has_photo = FALSE;
	guchar *face_header_value = NULL;
	gsize face_header_len = 0;
	gchar *header_sender = NULL, *header_from = NULL, *name;
	gboolean mail_from_delegate = FALSE;
	gchar *hdr_charset;
	gchar *evolution_imagesdir;
	const gchar *direction;

	if (g_cancellable_is_cancelled (cancellable))
		return;

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

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
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
		"id=\"__evo-full-headers\" "
		"style=\"display: %s; direction: %s;\">",
		flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED ? "none" : "table",
		direction);

	header = ((CamelMimePart *) part)->headers;
	while (header != NULL) {
		if (!g_ascii_strcasecmp (header->name, "Sender")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new ("");
			name = e_mail_formatter_format_address (
				formatter, html, addrs, header->name, FALSE,
				~(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			header_sender = html->str;
			camel_header_address_list_clear (&addrs);

			g_string_free (html, FALSE);
			g_free (name);

		} else if (!g_ascii_strcasecmp (header->name, "From")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new ("");
			name = e_mail_formatter_format_address (
				formatter, html, addrs, header->name, FALSE,
				!(flags & E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE));

			header_from = html->str;
			camel_header_address_list_clear (&addrs);

			g_string_free (html, FALSE);
			g_free (name);

		} else if (!g_ascii_strcasecmp (header->name, "X-Evolution-Mail-From-Delegate")) {
			mail_from_delegate = TRUE;
		}

		header = header->next;
	}

	g_free (hdr_charset);

	if (header_sender && header_from && mail_from_delegate) {
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
		/* Translators: This message suggests to the receipients
		 * that the sender of the mail is different from the one
		 * listed in From field. */
		g_string_append_printf (
			buffer,
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
	if (mode & E_MAIL_FORMATTER_MODE_ALL_HEADERS) {
		header = ((CamelMimePart *) part)->headers;
		while (header != NULL) {
			e_mail_formatter_format_header (
				formatter, buffer, part, header,
				E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS, charset);
			header = header->next;
		}
	} else {
		GQueue *headers_queue;
		GList *link;
		gint mailer_shown = FALSE;

		headers_queue = e_mail_formatter_dup_headers (formatter);
		link = g_queue_peek_head_link (headers_queue);

		while (link != NULL) {
			EMailFormatterHeader *h = link->data;
			gint mailer, face;

			header = ((CamelMimePart *) part)->headers;
			mailer = !g_ascii_strcasecmp (h->name, "X-Evolution-Mailer");
			face = !g_ascii_strcasecmp (h->name, "Face");

			while (header != NULL) {
				if (e_mail_formatter_get_show_sender_photo (formatter) &&
					!photo_name && !g_ascii_strcasecmp (header->name, "From"))
					photo_name = header->value;

				if (!mailer_shown && mailer && (
				    !g_ascii_strcasecmp (header->name, "X-Mailer") ||
				    !g_ascii_strcasecmp (header->name, "User-Agent") ||
				    !g_ascii_strcasecmp (header->name, "X-Newsreader") ||
				    !g_ascii_strcasecmp (header->name, "X-MimeOLE"))) {
					struct _camel_header_raw xmailer, *use_header = NULL;

					if (!g_ascii_strcasecmp (header->name, "X-MimeOLE")) {
						for (use_header = header->next; use_header; use_header = use_header->next) {
							if (!g_ascii_strcasecmp (use_header->name, "X-Mailer") ||
							    !g_ascii_strcasecmp (use_header->name, "User-Agent") ||
							    !g_ascii_strcasecmp (use_header->name, "X-Newsreader")) {
								/* even we have X-MimeOLE, then use rather the standard one, when available */
								break;
							}
						}
					}

					if (!use_header)
						use_header = header;

					xmailer.name = (gchar *) "X-Evolution-Mailer";
					xmailer.value = use_header->value;
					mailer_shown = TRUE;

					e_mail_formatter_format_header (
						formatter, buffer, part,
						&xmailer, h->flags, charset);
				} else if (!face_decoded && face && !g_ascii_strcasecmp (header->name, "Face")) {
					gchar *cp = header->value;

					/* Skip over spaces */
					while (*cp == ' ')
						cp++;

					face_header_value = g_base64_decode (
						cp, &face_header_len);
					face_header_value = g_realloc (
						face_header_value,
						face_header_len + 1);
					face_header_value[face_header_len] = 0;
					face_decoded = TRUE;
				/* Showing an encoded "Face" header makes little sense */
				} else if (!g_ascii_strcasecmp (header->name, h->name) && !face) {
					e_mail_formatter_format_header (
						formatter, buffer, part,
						header, h->flags, charset);
				}

				header = header->next;
			}

			link = g_list_next (link);
		}

		g_queue_free_full (headers_queue, (GDestroyNotify) e_mail_formatter_header_free);
	}

	g_string_append (buffer, "</table></td>");

	if (photo_name) {
		gchar *name;

		name = g_uri_escape_string (photo_name, NULL, FALSE);
		g_string_append (
			buffer,
			"<td align=\"right\" valign=\"top\">");
		g_string_append_printf (
			buffer,
			"<img src=\"mail://contact-photo?mailaddr=\" "
			"data-mailaddr=\"%s\" id=\"__evo-contact-photo\"/>",
			name);
		g_string_append (buffer, "</td>");

		g_free (name);
	}

	if (!contact_has_photo && face_decoded) {
		CamelMimePart *part;

		part = camel_mime_part_new ();
		camel_mime_part_set_content (
			(CamelMimePart *) part,
			(const gchar *) face_header_value,
			face_header_len, "image/png");

		g_string_append (
			buffer,
			"<td align=\"right\" valign=\"top\">");
		write_contact_picture (part, 48, buffer);
		g_string_append (buffer, "</td>");

		g_object_unref (part);
		g_free (face_header_value);
	}

	g_string_append (buffer, "</tr></table>");
}

static gboolean
emfe_headers_format (EMailFormatterExtension *extension,
                     EMailFormatter *formatter,
                     EMailFormatterContext *context,
                     EMailPart *part,
                     CamelStream *stream,
                     GCancellable *cancellable)
{
	CamelMimePart *mime_part;
	GString *buffer;
	const GdkRGBA white = { 1.0, 1.0, 1.0, 1.0 };
	const GdkRGBA *body_rgba = &white;
	const GdkRGBA *header_rgba;
	const gchar *direction;
	gboolean is_collapsable;
	gboolean is_collapsed;

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

	is_collapsable =
		(context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE);

	is_collapsed =
		(context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED);

	buffer = g_string_new ("");

	if (context->mode != E_MAIL_FORMATTER_MODE_PRINTING)
		body_rgba = e_mail_formatter_get_color (
			formatter, E_MAIL_FORMATTER_COLOR_BODY);

	header_rgba = e_mail_formatter_get_color (
		formatter, E_MAIL_FORMATTER_COLOR_HEADER);

	g_string_append_printf (
		buffer,
		"<div class=\"headers\" style=\"background: #%06x;\" id=\"%s\">"
		"<table border=\"0\" width=\"100%%\" "
		"style=\"color: #%06x; direction: %s\">"
		"<tr>",
		e_rgba_to_value (body_rgba),
		e_mail_part_get_id (part),
		e_rgba_to_value (header_rgba),
		direction);

	if (is_collapsable)
		g_string_append_printf (
			buffer,
			"<td valign=\"top\" width=\"16\">"
			"<img src=\"evo-file://%s/%s\" class=\"navigable\" "
			"     id=\"__evo-collapse-headers-img\" />"
			"</td>",
			EVOLUTION_IMAGESDIR,
			is_collapsed ? "plus.png" : "minus.png");

	g_string_append (buffer, "<td>");

	if (is_collapsable)
		format_short_headers (
			formatter, buffer,
			CAMEL_MEDIUM (mime_part),
			context->flags,
			cancellable);

	format_full_headers (
		formatter, buffer,
		CAMEL_MEDIUM (mime_part),
		context->mode,
		context->flags,
		cancellable);

	g_string_append (buffer, "</td>");

	g_string_append (buffer, "</tr></table></div>");

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

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

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

#include "e-mail-format-extensions.h"
#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterHeaders;
typedef EMailFormatterExtensionClass EMailFormatterHeadersClass;

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
	const gchar *charset;
	CamelContentType *ct;
	const gchar *hdr_charset;
	gchar *evolution_imagesdir;
	gchar *subject = NULL;
	struct _camel_header_address *addrs = NULL;
	struct _camel_header_raw *header;
	GString *from;
	gboolean is_rtl;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
	hdr_charset = e_mail_formatter_get_charset (formatter) ?
			e_mail_formatter_get_charset (formatter) :
			e_mail_formatter_get_default_charset (formatter);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);
	from = g_string_new ("");

	g_string_append_printf (
		buffer,
		"<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" "
		"id=\"__evo-short-headers\" style=\"display: %s\">",
		flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED ? "block" : "none");

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

			if (tmp->len)
				g_string_printf (from, _("From: %s"), tmp->str);
			g_string_free (tmp, TRUE);

		} else if (!g_ascii_strcasecmp (header->name, "Subject")) {
			gchar *buf = NULL;
			subject = camel_header_unfold (header->value);
			buf = camel_header_decode_string (subject, hdr_charset);
			g_free (subject);
			subject = camel_text_to_html (buf, CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
			g_free (buf);
		}
		header = header->next;
	}

	is_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL;
	if (is_rtl) {
		g_string_append_printf (
			buffer,
			"<tr><td width=\"100%%\" align=\"right\">%s%s%s <strong>%s</strong></td></tr>",
			from->len ? "(" : "", from->str, from->len ? ")" : "",
			subject ? subject : _("(no subject)"));
	} else {
		g_string_append_printf (
			buffer,
			"<tr><td><strong>%s</strong> %s%s%s</td></tr>",
			subject ? subject : _("(no subject)"),
			from->len ? "(" : "", from->str, from->len ? ")" : "");
	}

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

static CamelMimePart *
load_picture_from_file (const gchar *mime_type,
                         const gchar *filename,
                         GCancellable *cancellable)
{
	CamelMimePart *part;
	CamelStream *stream;
	CamelDataWrapper *dw;
	gchar *basename;

	stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0, NULL);
	if (stream == NULL)
		return NULL;

	dw = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream_sync (
		dw, stream, cancellable, NULL);
	g_object_unref (stream);
	if (mime_type)
		camel_data_wrapper_set_mime_type (dw, mime_type);
	part = camel_mime_part_new ();
	camel_medium_set_content ((CamelMedium *) part, dw);
	g_object_unref (dw);
	basename = g_path_get_basename (filename);
	camel_mime_part_set_filename (part, basename);
	g_free (basename);

	return part;
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
	gboolean have_icon = FALSE;
	const gchar *photo_name = NULL;
	gboolean face_decoded  = FALSE, contact_has_photo = FALSE;
	guchar *face_header_value = NULL;
	gsize face_header_len = 0;
	gchar *header_sender = NULL, *header_from = NULL, *name;
	gboolean mail_from_delegate = FALSE;
	const gchar *hdr_charset;
	gchar *evolution_imagesdir;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);
	hdr_charset = e_mail_formatter_get_charset (formatter) ?
			e_mail_formatter_get_charset (formatter) :
			e_mail_formatter_get_default_charset (formatter);

	evolution_imagesdir = g_filename_to_uri (EVOLUTION_IMAGESDIR, NULL, NULL);

	g_string_append_printf (
		buffer,
		"<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" "
		"id=\"__evo-full-headers\" style=\"display: %s\" width=\"100%%\">",
		flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED ? "none" : "block");

	header = ((CamelMimePart *) part)->headers;
	while (header) {
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
		"<table border=0 cellpadding=\"0\">\n");

	g_free (evolution_imagesdir);

	/* dump selected headers */
	if (mode & E_MAIL_FORMATTER_MODE_ALL_HEADERS) {
		header = ((CamelMimePart *) part)->headers;
		while (header) {
			e_mail_formatter_format_header (
				formatter, buffer, part, header,
				E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS, charset);
			header = header->next;
		}
	} else {
		GList *link;
		gint mailer_shown = FALSE;

		link = g_queue_peek_head_link (
				(GQueue *) e_mail_formatter_get_headers (formatter));

		while (link != NULL) {
			EMailFormatterHeader *h = link->data;
			gint mailer, face;

			header = ((CamelMimePart *) part)->headers;
			mailer = !g_ascii_strcasecmp (h->name, "X-Evolution-Mailer");
			face = !g_ascii_strcasecmp (h->name, "Face");

			while (header) {
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
					if (strstr (use_header->value, "Evolution"))
						have_icon = TRUE;
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
	}

	g_string_append (buffer, "</table></td>");

	if (photo_name) {
		gboolean only_local_photo;
		gchar *name;

		name = g_uri_escape_string (photo_name, NULL, FALSE);
		only_local_photo = e_mail_formatter_get_only_local_photos (formatter);
		g_string_append (buffer, "<td align=\"right\" valign=\"top\">");

		g_string_append_printf (
			buffer,
			"<img src=\"mail://contact-photo?mailaddr=&only-local-photo=1\" "
			"data-mailaddr=\"%s\" %s id=\"__evo-contact-photo\"/>",
			name, only_local_photo ? "data-onlylocal=1" : "");
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

		g_string_append (buffer, "<td align=\"right\" valign=\"top\">");
		write_contact_picture (part, 48, buffer);
		g_string_append (buffer, "</td>");

		g_object_unref (part);
		g_free (face_header_value);
	}

	if (have_icon) {
		GtkIconInfo *icon_info;
		CamelMimePart *iconpart = NULL;

		icon_info = gtk_icon_theme_lookup_icon (
				gtk_icon_theme_get_default (),
				"evolution", 16, GTK_ICON_LOOKUP_NO_SVG);
		if (icon_info != NULL) {
			iconpart = load_picture_from_file (
				"image/png", gtk_icon_info_get_filename (icon_info),
				cancellable);
			gtk_icon_info_free (icon_info);
		}
		if (iconpart) {
			g_string_append (buffer, "<td align=\"right\" valign=\"top\">");
			write_contact_picture (iconpart, 16, buffer);
			g_string_append (buffer, "</td>");

			g_object_unref (iconpart);
		}
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
	GString *buffer;
	gint bg_color;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (!part->part)
		return FALSE;

	buffer = g_string_new ("");

	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		GdkColor white = { 0, G_MAXUINT16, G_MAXUINT16, G_MAXUINT16 };
		bg_color = e_color_to_value (&white);
	} else {
		bg_color = e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_BODY));
	}

	g_string_append_printf (
		buffer,
		"<div class=\"headers\" style=\"background: #%06x;\" id=\"%s\">"
		"<table border=\"0\" width=\"100%%\" style=\"color: #%06x;\">\n"
		"<tr><td valign=\"top\" width=\"16\">\n",
		bg_color,
		part->id,
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (
				formatter,
				E_MAIL_FORMATTER_COLOR_HEADER)));

	if (context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE) {
		g_string_append_printf (
			buffer,
			"<img src=\"evo-file://%s/%s\" class=\"navigable\" "
			"id=\"__evo-collapse-headers-img\" />"
			"</td><td>",
			EVOLUTION_IMAGESDIR,
			(context->flags & E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED) ?
				"plus.png" : "minus.png");

		format_short_headers (formatter, buffer,
			(CamelMedium *) part->part, context->flags, cancellable);
	}

	format_full_headers (formatter, buffer,
		(CamelMedium *) part->part, context->mode, context->flags, cancellable);

	g_string_append (buffer, "</td></tr></table></div>");

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return TRUE;
}

static void
e_mail_formatter_headers_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->format = emfe_headers_format;
}

static void
e_mail_formatter_headers_init (EMailFormatterExtension *extension)
{
}

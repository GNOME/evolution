/*
 * e-mail-formatter-message-rfc822.c
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

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-part-list.h"
#include "e-mail-part-utils.h"

typedef EMailFormatterExtension EMailFormatterMessageRFC822;
typedef EMailFormatterExtensionClass EMailFormatterMessageRFC822Class;

GType e_mail_formatter_message_rfc822_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterMessageRFC822,
	e_mail_formatter_message_rfc822,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"message/rfc822",
	"application/vnd.evolution.rfc822.end",
	NULL
};

static gboolean
emfe_message_rfc822_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            CamelStream *stream,
                            GCancellable *cancellable)
{
	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		GQueue queue = G_QUEUE_INIT;
		GList *head, *link;
		gchar *header, *end;

		header = e_mail_formatter_get_html_header (formatter);
		camel_stream_write_string (stream, header, cancellable, NULL);
		g_free (header);

		/* Print content of the message normally */
		context->mode = E_MAIL_FORMATTER_MODE_NORMAL;

		e_mail_part_list_queue_parts (
			context->part_list, part->id, &queue);

		/* Discard the first EMailPart. */
		if (!g_queue_is_empty (&queue))
			e_mail_part_unref (g_queue_pop_head (&queue));

		head = g_queue_peek_head_link (&queue);

		end = g_strconcat (part->id, ".end", NULL);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;

			/* Check for nested rfc822 messages */
			if (g_str_has_suffix (p->id, ".rfc822")) {
				gchar *sub_end = g_strconcat (p->id, ".end", NULL);

				while (link != NULL) {
					p = link->data;

					if (g_strcmp0 (p->id, sub_end) == 0)
						break;

					link = g_list_next (link);
				}
				g_free (sub_end);
				continue;
			}

			if ((g_strcmp0 (p->id, end) == 0))
				break;

			if (p->is_hidden)
				continue;

			e_mail_formatter_format_as (
				formatter, context, p,
				stream, NULL, cancellable);
		}

		g_free (end);

		while (!g_queue_is_empty (&queue))
			e_mail_part_unref (g_queue_pop_head (&queue));

		context->mode = E_MAIL_FORMATTER_MODE_RAW;

		camel_stream_write_string (stream, "</body></html>", cancellable, NULL);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		GQueue queue = G_QUEUE_INIT;
		GList *head, *link;
		gchar *end;

		/* Part is EMailPartAttachment */
		e_mail_part_list_queue_parts (
			context->part_list, part->id, &queue);

		/* Discard the first EMailPart. */
		if (!g_queue_is_empty (&queue))
			e_mail_part_unref (g_queue_pop_head (&queue));

		if (g_queue_is_empty (&queue))
			return FALSE;

		part = g_queue_pop_head (&queue);
		end = g_strconcat (part->id, ".end", NULL);
		e_mail_part_unref (part);

		head = g_queue_peek_head_link (&queue);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;

			/* Skip attachment bar */
			if (g_str_has_suffix (part->id, ".attachment-bar"))
				continue;

			/* Check for nested rfc822 messages */
			if (g_str_has_suffix (p->id, ".rfc822")) {
				gchar *sub_end = g_strconcat (p->id, ".end", NULL);

				while (link != NULL) {
					p = link->data;

					if (g_strcmp0 (p->id, sub_end) == 0)
						break;

					link = g_list_next (link);
				}
				g_free (sub_end);
				continue;
			}

			if ((g_strcmp0 (p->id, end) == 0))
				break;

			if (p->is_hidden)
				continue;

			e_mail_formatter_format_as (
				formatter, context, p,
				stream, NULL, cancellable);
		}

		g_free (end);

		while (!g_queue_is_empty (&queue))
			e_mail_part_unref (g_queue_pop_head (&queue));

	} else {
		EMailPart *p;
		CamelFolder *folder;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *str;
		gchar *uri;

		p = e_mail_part_list_ref_part (context->part_list, part->id);
		if (p == NULL)
			return FALSE;

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, p->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"headers_collapsable", G_TYPE_INT, 0,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container\" style=\"border-color: #%06x; "
			"background-color: #%06x;\">\n"
			"<iframe width=\"100%%\" height=\"10\""
			" id=\"%s.iframe\" "
			" frameborder=\"0\" src=\"%s\" name=\"%s\"></iframe>"
			"</div>",
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_BODY)),
			part->id, uri, part->id);

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);

		e_mail_part_unref (p);
	}

	return TRUE;
}

static void
e_mail_formatter_message_rfc822_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("RFC822 message");
	class->description = _("Format part as an RFC822 message");
	class->mime_types = formatter_mime_types;
	class->format = emfe_message_rfc822_format;
}

static void
e_mail_formatter_message_rfc822_init (EMailFormatterExtension *extension)
{
}

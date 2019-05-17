/*
 * e-mail-formatter-message-rfc822.c
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
                            GOutputStream *stream,
                            GCancellable *cancellable)
{
	const gchar *part_id;

	part_id = e_mail_part_get_id (part);

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		GQueue queue = G_QUEUE_INIT;
		GList *head, *link;
		gchar *header, *end;
		const gchar *string;

		header = e_mail_formatter_get_html_header (formatter);
		g_output_stream_write_all (
			stream, header, strlen (header),
			NULL, cancellable, NULL);
		g_free (header);

		/* Print content of the message normally */
		context->mode = E_MAIL_FORMATTER_MODE_NORMAL;

		e_mail_part_list_queue_parts (
			context->part_list, part_id, &queue);

		/* Discard the first EMailPart. */
		if (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

		head = g_queue_peek_head_link (&queue);

		end = g_strconcat (part_id, ".end", NULL);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;
			const gchar *p_id;

			p_id = e_mail_part_get_id (p);

			/* Check for nested rfc822 messages */
			if (e_mail_part_id_has_suffix (p, ".rfc822")) {
				gchar *sub_end = g_strconcat (p_id, ".end", NULL);

				while (link != NULL) {
					p = link->data;

					p_id = e_mail_part_get_id (p);

					if (g_strcmp0 (p_id, sub_end) == 0)
						break;

					link = g_list_next (link);
				}
				g_free (sub_end);
				continue;
			}

			if ((g_strcmp0 (p_id, end) == 0))
				break;

			if (p->is_hidden)
				continue;

			e_mail_formatter_format_as (
				formatter, context, p,
				stream, NULL, cancellable);
		}

		g_free (end);

		while (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

		context->mode = E_MAIL_FORMATTER_MODE_RAW;

		string = "</body></html>";

		g_output_stream_write_all (
			stream, string, strlen (string),
			NULL, cancellable, NULL);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		GQueue queue = G_QUEUE_INIT;
		GList *head, *link;
		gchar *end;

		/* Part is EMailPartAttachment */
		e_mail_part_list_queue_parts (
			context->part_list, part_id, &queue);

		/* Discard the first EMailPart. */
		if (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

		if (g_queue_is_empty (&queue))
			return FALSE;

		part = g_queue_pop_head (&queue);
		end = g_strconcat (part_id, ".end", NULL);
		g_object_unref (part);

		head = g_queue_peek_head_link (&queue);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;
			const gchar *p_id;

			p_id = e_mail_part_get_id (p);

			/* Check for nested rfc822 messages */
			if (e_mail_part_id_has_suffix (p, ".rfc822")) {
				gchar *sub_end = g_strconcat (p_id, ".end", NULL);

				while (link != NULL) {
					p = link->data;

					p_id = e_mail_part_get_id (p);

					if (g_strcmp0 (p_id, sub_end) == 0)
						break;

					link = g_list_next (link);
				}
				g_free (sub_end);
				continue;
			}

			if ((g_strcmp0 (p_id, end) == 0))
				break;

			if (p->is_hidden)
				continue;

			e_mail_formatter_format_as (
				formatter, context, p,
				stream, NULL, cancellable);
		}

		g_free (end);

		while (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

	} else {
		EMailPart *p;
		CamelFolder *folder;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *str;
		gchar *uri;

		p = e_mail_part_list_ref_part (context->part_list, part_id);
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
			"part_id", G_TYPE_STRING, e_mail_part_get_id (p),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"headers_collapsable", G_TYPE_INT, 0,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container "
			"-e-mail-formatter-body-color\">\n"
			"<iframe width=\"100%%\" height=\"10\""
			" id=\"%s.iframe\" "
			" class=\"-e-mail-formatter-frame-color\""
			" frameborder=\"0\" src=\"%s\" name=\"%s\"></iframe>"
			"</div>",
			part_id, uri, part_id);

		g_output_stream_write_all (
			stream, str, strlen (str),
			NULL, cancellable, NULL);

		g_free (str);
		g_free (uri);

		g_object_unref (p);
	}

	return TRUE;
}

static void
e_mail_formatter_message_rfc822_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("RFC822 message");
	class->description = _("Format part as an RFC822 message");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_message_rfc822_format;
}

static void
e_mail_formatter_message_rfc822_init (EMailFormatterExtension *extension)
{
}

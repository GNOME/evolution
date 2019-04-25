/*
 * e-mail-formatter-quote-message-rfc822.c
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

#include "e-mail-formatter-quote.h"
#include "e-mail-part-list.h"
#include "e-mail-part-utils.h"

typedef EMailFormatterExtension EMailFormatterQuoteMessageRFC822;
typedef EMailFormatterExtensionClass EMailFormatterQuoteMessageRFC822Class;

GType e_mail_formatter_quote_message_rfc822_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterQuoteMessageRFC822,
	e_mail_formatter_quote_message_rfc822,
	E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"message/rfc822",
	"application/vnd.evolution.rfc822.end",
	NULL
};

static gboolean
emfqe_message_rfc822_format (EMailFormatterExtension *extension,
                             EMailFormatter *formatter,
                             EMailFormatterContext *context,
                             EMailPart *part,
                             GOutputStream *stream,
                             GCancellable *cancellable)
{
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	gchar *header, *end;
	EMailFormatterQuoteContext *qc = (EMailFormatterQuoteContext *) context;
	const gchar *part_id;
	const gchar *string;

	part_id = e_mail_part_get_id (part);

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	header = e_mail_formatter_get_html_header (formatter);
	g_output_stream_write_all (
		stream, header, strlen (header), NULL, cancellable, NULL);
	g_free (header);

	e_mail_part_list_queue_parts (context->part_list, part_id, &queue);

	if (g_queue_is_empty (&queue))
		return FALSE;

	/* Discard the first EMailPart. */
	g_object_unref (g_queue_pop_head (&queue));

	head = g_queue_peek_head (&queue);

	end = g_strconcat (part_id, ".end", NULL);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *p = link->data;
		const gchar *p_id;

		p_id = e_mail_part_get_id (p);

		if (e_mail_part_id_has_suffix (p, ".headers.")) {
			if (qc->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS) {
				e_mail_formatter_format_as (
					formatter, context, part, stream,
					"application/vnd.evolution.headers",
					cancellable);
			}

			continue;
		}

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

	string = "</body></html>";
	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	return TRUE;
}

static void
e_mail_formatter_quote_message_rfc822_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_HIGH;
	class->format = emfqe_message_rfc822_format;
}

static void
e_mail_formatter_quote_message_rfc822_init (EMailFormatterExtension *extension)
{
}

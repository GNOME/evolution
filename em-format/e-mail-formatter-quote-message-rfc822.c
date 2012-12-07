/*
 * e-mail-formatter-quote-message-rfc822.c
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
#include <glib-object.h>

#include <em-format/e-mail-formatter-quote.h>
#include <em-format/e-mail-part-list.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef EMailFormatterExtension EMailFormatterQuoteMessageRFC822;
typedef EMailFormatterExtensionClass EMailFormatterQuoteMessageRFC822Class;

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
                             CamelStream *stream,
                             GCancellable *cancellable)
{
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	gchar *header, *end;
	EMailFormatterQuoteContext *qc = (EMailFormatterQuoteContext *) context;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	header = e_mail_formatter_get_html_header (formatter);
	camel_stream_write_string (stream, header, cancellable, NULL);
	g_free (header);

	e_mail_part_list_queue_parts (context->part_list, part->id, &queue);

	if (g_queue_is_empty (&queue))
		return FALSE;

	/* Discard the first EMailPart. */
	e_mail_part_unref (g_queue_pop_head (&queue));

	head = g_queue_peek_head (&queue);

	end = g_strconcat (part->id, ".end", NULL);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *p = link->data;

		/* Skip attachment bar */
		if (g_str_has_suffix (p->id, ".attachment-bar"))
			continue;

		if (g_str_has_suffix (p->id, ".headers.")) {
			if (qc->qf_flags & E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS) {
				e_mail_formatter_format_as (
					formatter, context, part, stream,
					"application/vnd.evolution.headers",
					cancellable);
			}

			continue;
		}

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

	camel_stream_write_string (stream, "</body></html>", cancellable, NULL);

	return TRUE;
}

static void
e_mail_formatter_quote_message_rfc822_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->format = emfqe_message_rfc822_format;
}

static void
e_mail_formatter_quote_message_rfc822_init (EMailFormatterExtension *extension)
{
}

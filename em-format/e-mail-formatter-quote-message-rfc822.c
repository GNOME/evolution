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

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter-quote.h>
#include <em-format/e-mail-part-list.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

static const gchar * formatter_mime_types[] = { "message/rfc822",
					       "application/vnd.evolution.rfc822.end",
					       NULL };

typedef struct _EMailFormatterQuoteMessageRFC822 {
	GObject parent;
} EMailFormatterQuoteMessageRFC822;

typedef struct _EMailFormatterQuoteMessageRFC822Class {
	GObjectClass parent_class;
} EMailFormatterQuoteMessageRFC822Class;

static void e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_quote_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterQuoteMessageRFC822,
	e_mail_formatter_quote_message_rfc822,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_quote_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_quote_formatter_extension_interface_init));

static gboolean
emfqe_message_rfc822_format (EMailFormatterExtension *extension,
                             EMailFormatter *formatter,
                             EMailFormatterContext *context,
                             EMailPart *part,
                             CamelStream *stream,
                             GCancellable *cancellable)
{
	GSList *iter;
	gchar *header, *end;
	EMailFormatterQuoteContext *qc = (EMailFormatterQuoteContext *) context;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	header = e_mail_formatter_get_html_header (formatter);
	camel_stream_write_string (stream, header, cancellable, NULL);
	g_free (header);

	iter = e_mail_part_list_get_iter (context->parts, part->id);
	if (!iter) {
		return FALSE;
	}

	end = g_strconcat (part->id, ".end", NULL);
	for (iter = g_slist_next (iter); iter; iter = g_slist_next (iter)) {
		EMailPart * p = iter->data;
		if (!p)
			continue;

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

			while (iter) {
				p = iter->data;
				if (!p) {
					iter = g_slist_next (iter);
					if (!iter) {
						break;
					}
					continue;
				}

				if (g_strcmp0 (p->id, sub_end) == 0) {
					break;
				}

				iter = g_slist_next (iter);
				if (!iter) {
					break;
				}
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

	camel_stream_write_string (stream, "</body></html>", cancellable, NULL);

	return TRUE;
}

static const gchar *
emfqe_message_rfc822_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emfqe_message_rfc822_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emfqe_message_rfc822_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_quote_message_rfc822_class_init (EMailFormatterQuoteMessageRFC822Class *class)
{
}

static void
e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfqe_message_rfc822_format;
	iface->get_display_name = emfqe_message_rfc822_get_display_name;
	iface->get_description = emfqe_message_rfc822_get_description;
}

static void
e_mail_formatter_quote_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfqe_message_rfc822_mime_types;
}

static void
e_mail_formatter_quote_message_rfc822_init (EMailFormatterQuoteMessageRFC822 *formatter)
{

}

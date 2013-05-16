/*
 * e-mail-formatter-print-headers.c
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

#include <camel/camel.h>

#include <e-util/e-util.h>
#include <libemail-engine/e-mail-utils.h>

#include "e-mail-formatter-print.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterPrintHeaders;
typedef EMailFormatterExtensionClass EMailFormatterPrintHeadersClass;

GType e_mail_formatter_print_headers_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterPrintHeaders,
	e_mail_formatter_print_headers,
	E_TYPE_MAIL_FORMATTER_PRINT_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.headers",
	NULL
};

static gboolean
emfpe_headers_format (EMailFormatterExtension *extension,
                      EMailFormatter *formatter,
                      EMailFormatterContext *context,
                      EMailPart *part,
                      CamelStream *stream,
                      GCancellable *cancellable)
{
	struct _camel_header_raw raw_header;
	GString *str, *tmp;
	gchar *subject;
	const gchar *buf;
	gint attachments_count;
	gchar *part_id_prefix;
	GQueue *headers_queue;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *part_id;

	buf = camel_medium_get_header (CAMEL_MEDIUM (part->part), "subject");
	subject = camel_header_decode_string (buf, "UTF-8");
	str = g_string_new ("");
	g_string_append_printf (str, "<h1>%s</h1>\n", subject);
	g_free (subject);

	g_string_append (
		str,
		"<table border=\"0\" cellspacing=\"5\" "
		"cellpadding=\"0\" class=\"printing-header\">\n");

	headers_queue = e_mail_formatter_dup_headers (formatter);
	for (link = headers_queue->head; link != NULL; link = g_list_next (link)) {
		EMailFormatterHeader *header = link->data;
		raw_header.name = header->name;

		/* Skip 'Subject' header, it's already displayed. */
		if (g_ascii_strncasecmp (header->name, "Subject", 7) == 0)
			continue;

		if (header->value && *header->value) {
			raw_header.value = header->value;
			e_mail_formatter_format_header (
				formatter, str,
				CAMEL_MEDIUM (part->part), &raw_header,
				header->flags | E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS,
				"UTF-8");
		} else {
			CamelMimeMessage *message;
			const gchar *header_value;

			message = e_mail_part_list_get_message (context->part_list);

			header_value = camel_medium_get_header (
				CAMEL_MEDIUM (message), header->name);
			raw_header.value = g_strdup (header_value);

			if (raw_header.value && *raw_header.value) {
				e_mail_formatter_format_header (
					formatter, str,
					CAMEL_MEDIUM (part->part), &raw_header,
					header->flags | E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS,
					"UTF-8");
			}

			if (raw_header.value)
				g_free (raw_header.value);
		}
	}

	g_queue_free_full (headers_queue, (GDestroyNotify) e_mail_formatter_header_free);

	/* Get prefix of this PURI */
	part_id = e_mail_part_get_id (part);
	part_id_prefix = g_strndup (part_id, g_strrstr (part_id, ".") - part_id);

	/* Add encryption/signature header */
	raw_header.name = _("Security");
	tmp = g_string_new ("");

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	/* Find first secured part. */
	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;

		if (g_queue_is_empty (&mail_part->validities))
			continue;

		if (!g_str_has_prefix (mail_part->id, part_id_prefix))
			continue;

		if (e_mail_part_get_validity (mail_part, E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_SIGNED)) {
			g_string_append (tmp, _("GPG signed"));
		}

		if (e_mail_part_get_validity (mail_part, E_MAIL_PART_VALIDITY_PGP | E_MAIL_PART_VALIDITY_ENCRYPTED)) {
			if (tmp->len > 0)
				g_string_append (tmp, ", ");
			g_string_append (tmp, _("GPG encrpyted"));
		}

		if (e_mail_part_get_validity (mail_part, E_MAIL_PART_VALIDITY_SMIME | E_MAIL_PART_VALIDITY_SIGNED)) {
			if (tmp->len > 0)
				g_string_append (tmp, ", ");
			g_string_append (tmp, _("S/MIME signed"));
		}

		if (e_mail_part_get_validity (mail_part, E_MAIL_PART_VALIDITY_SMIME | E_MAIL_PART_VALIDITY_ENCRYPTED)) {
			if (tmp->len > 0)
				g_string_append (tmp, ", ");
			g_string_append (tmp, _("S/MIME encrpyted"));
		}

		break;
	}

	if (tmp->len > 0) {
		raw_header.value = tmp->str;
		e_mail_formatter_format_header (
			formatter, str,
			CAMEL_MEDIUM (part->part), &raw_header,
			E_MAIL_FORMATTER_HEADER_FLAG_BOLD |
			E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS, "UTF-8");
	}
	g_string_free (tmp, TRUE);

	/* Count attachments and display the number as a header */
	attachments_count = 0;

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = E_MAIL_PART (link->data);

		if (!g_str_has_prefix (mail_part->id, part_id_prefix))
			continue;

		if (!mail_part->is_attachment)
			continue;

		if (mail_part->is_hidden)
			continue;

		if (mail_part->cid != NULL)
			continue;

		attachments_count++;
	}

	if (attachments_count > 0) {
		raw_header.name = _("Attachments");
		raw_header.value = g_strdup_printf ("%d", attachments_count);
		e_mail_formatter_format_header (
			formatter, str,
			CAMEL_MEDIUM (part->part), &raw_header,
			E_MAIL_FORMATTER_HEADER_FLAG_BOLD |
			E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS, "UTF-8");
		g_free (raw_header.value);
	}

	while (!g_queue_is_empty (&queue))
		e_mail_part_unref (g_queue_pop_head (&queue));

	g_string_append (str, "</table>");

	camel_stream_write_string (stream, str->str, cancellable, NULL);
	g_string_free (str, TRUE);
	g_free (part_id_prefix);

	return TRUE;
}

static void
e_mail_formatter_print_headers_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfpe_headers_format;
}

static void
e_mail_formatter_print_headers_init (EMailFormatterExtension *extension)
{
}

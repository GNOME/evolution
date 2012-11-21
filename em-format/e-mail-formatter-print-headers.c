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

#include "e-mail-format-extensions.h"

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-inline-filter.h>
#include <libemail-engine/e-mail-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailFormatterPrintHeaders {
	GObject parent;
} EMailFormatterPrintHeaders;

typedef struct _EMailFormatterPrintHeadersClass {
	GObjectClass parent_class;
} EMailFormatterPrintHeadersClass;

static const gchar *formatter_mime_types[] = { "application/vnd.evolution.headers", NULL };

static void e_mail_formatter_print_formatter_extension_interface_init
					(EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_print_mail_extension_interface_init
					(EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterPrintHeaders,
	e_mail_formatter_print_headers,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_print_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_print_formatter_extension_interface_init))

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
	GSList *parts_iter;
	GList *iter;
	gint attachments_count;
	gchar *part_id_prefix;
	const GQueue *headers;

	buf = camel_medium_get_header (CAMEL_MEDIUM (part->part), "subject");
	subject = camel_header_decode_string (buf, "UTF-8");
	str = g_string_new ("");
	g_string_append_printf (str, "<h1>%s</h1>\n", subject);
	g_free (subject);

	g_string_append (
		str,
		"<table border=\"0\" cellspacing=\"5\" "
		"cellpadding=\"0\" class=\"printing-header\">\n");

	headers = e_mail_formatter_get_headers (formatter);
	for (iter = headers->head; iter; iter = iter->next) {

		EMailFormatterHeader *header = iter->data;
		raw_header.name = header->name;

		/* Skip 'Subject' header, it's already displayed. */
		if (g_ascii_strncasecmp (header->name, "Subject", 7) == 0)
			continue;

		if (header->value && *header->value) {
			raw_header.value = header->value;
			e_mail_formatter_format_header (formatter, str,
				CAMEL_MEDIUM (part->part), &raw_header,
				header->flags | E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS,
				"UTF-8");
		} else {
			raw_header.value = g_strdup (camel_medium_get_header (
				CAMEL_MEDIUM (context->message), header->name));

			if (raw_header.value && *raw_header.value) {
				e_mail_formatter_format_header (formatter, str,
					CAMEL_MEDIUM (part->part), &raw_header,
					header->flags | E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS,
					"UTF-8");
			}

			if (raw_header.value)
				g_free (raw_header.value);
		}
	}

        /* Get prefix of this PURI */
	part_id_prefix = g_strndup (part->id, g_strrstr (part->id, ".") - part->id);

	/* Add encryption/signature header */
	raw_header.name = _("Security");
	tmp = g_string_new ("");
	/* Find first secured part. */
	for (parts_iter = context->parts; parts_iter; parts_iter = parts_iter->next) {

		EMailPart *mail_part = parts_iter->data;
		if (mail_part == NULL)
			continue;

		if (!mail_part->validities)
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
			formatter, str, CAMEL_MEDIUM (part->part), &raw_header,
			E_MAIL_FORMATTER_HEADER_FLAG_BOLD |
			E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS, "UTF-8");
	}
	g_string_free (tmp, TRUE);

	/* Count attachments and display the number as a header */
	attachments_count = 0;

	for (parts_iter = context->parts; parts_iter; parts_iter = parts_iter->next) {

		EMailPart *mail_part = parts_iter->data;
		if (!mail_part)
			continue;

		if (!g_str_has_prefix (mail_part->id, part_id_prefix))
			continue;

		if (mail_part->is_attachment && !mail_part->cid &&
		    !mail_part->is_hidden) {
			attachments_count++;
		}
	}

	if (attachments_count > 0) {
		raw_header.name = _("Attachments");
		raw_header.value = g_strdup_printf ("%d", attachments_count);
		e_mail_formatter_format_header (
			formatter, str, CAMEL_MEDIUM (part->part), &raw_header,
			E_MAIL_FORMATTER_HEADER_FLAG_BOLD |
			E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS, "UTF-8");
		g_free (raw_header.value);
	}

	g_string_append (str, "</table>");

	camel_stream_write_string (stream, str->str, cancellable, NULL);
	g_string_free (str, TRUE);
	g_free (part_id_prefix);

	return TRUE;
}

static const gchar *
emfpe_headers_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emfpe_headers_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emfpe_headers_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_print_headers_class_init (EMailFormatterPrintHeadersClass *class)
{
}

static void
e_mail_formatter_print_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfpe_headers_format;
	iface->get_display_name = emfpe_headers_get_display_name;
	iface->get_description = emfpe_headers_get_description;
}

static void
e_mail_formatter_print_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfpe_headers_mime_types;
}

static void
e_mail_formatter_print_headers_init (EMailFormatterPrintHeaders *formatter)
{

}

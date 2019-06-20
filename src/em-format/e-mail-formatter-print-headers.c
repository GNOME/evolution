/*
 * e-mail-formatter-print-headers.c
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
#include <libemail-engine/libemail-engine.h>

#include "e-mail-formatter-print.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-headers.h"

typedef EMailFormatterExtension EMailFormatterPrintHeaders;
typedef EMailFormatterExtensionClass EMailFormatterPrintHeadersClass;

GType e_mail_formatter_print_headers_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterPrintHeaders,
	e_mail_formatter_print_headers,
	E_TYPE_MAIL_FORMATTER_PRINT_EXTENSION)

static const gchar *formatter_mime_types[] = {
	E_MAIL_PART_HEADERS_MIME_TYPE,
	"text/rfc822-headers",
	NULL
};

static gboolean
emfpe_headers_format (EMailFormatterExtension *extension,
                      EMailFormatter *formatter,
                      EMailFormatterContext *context,
                      EMailPart *part,
                      GOutputStream *stream,
                      GCancellable *cancellable)
{
	EMailPartHeaders *headers_part;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_valid;
	GString *str;
	gchar *subject;
	const gchar *buf, *mime_type;
	gint attachments_count;
	gchar *part_id_prefix;
	CamelMimePart *mime_part;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *part_id;

	g_return_val_if_fail (E_IS_MAIL_PART (part), FALSE);

	mime_type = e_mail_part_get_mime_type (part);
	if ((mime_type && g_ascii_strcasecmp (mime_type, "text/rfc822-headers") == 0) || !E_IS_MAIL_PART_HEADERS (part))
		return e_mail_formatter_format_as (formatter, context, part, stream, "text/plain", cancellable);

	mime_part = e_mail_part_ref_mime_part (part);

	buf = camel_medium_get_header (CAMEL_MEDIUM (mime_part), "subject");
	subject = camel_header_decode_string (buf, "UTF-8");
	str = g_string_new ("");
	g_string_append_printf (str, "<h1>%s</h1>\n", subject);
	g_free (subject);

	g_string_append (
		str,
		"<table border=\"0\" cellspacing=\"5\" "
		"cellpadding=\"0\" class=\"printing-header\">\n");

	headers_part = E_MAIL_PART_HEADERS (part);
	tree_model = e_mail_part_headers_ref_print_model (headers_part);
	iter_valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_valid) {
		gchar *header_name = NULL;
		gchar *header_value = NULL;
		gboolean include = FALSE;

		gtk_tree_model_get (
			tree_model, &iter,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE,
			&include,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_NAME,
			&header_name,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_VALUE,
			&header_value,
			-1);

		if (include)
			e_mail_formatter_format_header (
				formatter, str,
				header_name, header_value,
				E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS | E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE,
				"UTF-8");

		g_free (header_name);
		g_free (header_value);

		iter_valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	g_object_unref (tree_model);

	e_mail_formatter_format_security_header (formatter, context, str, part, E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS);

	/* Get prefix of this PURI */
	part_id = e_mail_part_get_id (part);
	part_id_prefix = g_strndup (part_id, g_strrstr (part_id, ".") - part_id);

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);
	head = g_queue_peek_head_link (&queue);

	/* Count attachments and display the number as a header */
	attachments_count = 0;

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = E_MAIL_PART (link->data);

		if (!e_mail_part_id_has_prefix (mail_part, part_id_prefix))
			continue;

		if (!e_mail_part_get_is_attachment (mail_part))
			continue;

		if (mail_part->is_hidden ||
		    !e_mail_part_get_is_printable (mail_part))
			continue;

		if (e_mail_part_get_cid (mail_part) != NULL)
			continue;

		attachments_count++;
	}

	if (attachments_count > 0) {
		gchar *header_value;

		header_value = g_strdup_printf ("%d", attachments_count);
		e_mail_formatter_format_header (
			formatter, str,
			_("Attachments"), header_value,
			E_MAIL_FORMATTER_HEADER_FLAG_BOLD |
			E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS, "UTF-8");
		g_free (header_value);
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	g_string_append (str, "</table>");

	g_output_stream_write_all (
		stream, str->str, str->len, NULL, cancellable, NULL);

	g_string_free (str, TRUE);
	g_free (part_id_prefix);

	g_object_unref (mime_part);

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

/*
 * e-mail-formatter-qoute-attachment.c
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
#include "e-mail-part-attachment.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#define d(x)

typedef struct _EMailFormatterQuoteAttachment {
	GObject parent;
} EMailFormatterQuoteAttachment;

typedef struct _EMailFormatterQuoteAttachmentClass {
	GObjectClass parent_class;
} EMailFormatterQuoteAttachmentClass;

static void e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_quote_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterQuoteAttachment,
	e_mail_formatter_quote_attachment,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_quote_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_quote_formatter_extension_interface_init)
)

static const gchar *formatter_mime_types[] = { "application/vnd.evolution.attachment",
					       NULL };

static gboolean
emfqe_attachment_format (EMailFormatterExtension *extension,
                         EMailFormatter *formatter,
                         EMailFormatterContext *context,
                         EMailPart *part,
                         CamelStream *stream,
                         GCancellable *cancellable)
{
	gchar *text, *html;
	guint32 text_format_flags;
	EMailPartAttachment *empa;
	EMailPart *att_part;
	GSList *iter;

	empa = E_MAIL_PART_ATTACHMENT (part);

	if (!empa->attachment_view_part_id)
		return FALSE;

	iter = e_mail_part_list_get_iter (
		context->part_list->list, empa->attachment_view_part_id);
	if (!iter || !iter->data)
		return FALSE;

	att_part = iter->data;

	camel_stream_write_string (stream, "<br><br>", cancellable, NULL);

	text_format_flags =
		e_mail_formatter_get_text_format_flags (formatter);
	text = e_mail_part_describe (
		part->part,
		empa ? empa->snoop_mime_type : part->mime_type);

	html = camel_text_to_html (
		text,
		text_format_flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS,
		0);
	camel_stream_write_string (stream, html, cancellable, NULL);
	camel_stream_write_string (stream, "<br>", cancellable, NULL);
	g_free (html);
	g_free (text);

	camel_stream_write_string (
		stream,
		"<!--+GtkHTML:<DATA class=\"ClueFlow\" "
		"key=\"orig\" value=\"1\">-->\n"
		"<blockquote type=cite>\n", cancellable, NULL);

	e_mail_formatter_format_as (
		formatter, context, att_part, stream, NULL, cancellable);

	camel_stream_write_string (
		stream,
		"</blockquote><!--+GtkHTML:"
		"<DATA class=\"ClueFlow\" clear=\"orig\">-->",
		cancellable, NULL);

	return TRUE;
}

static const gchar *
emfqe_attachment_get_display_name (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar *
emfqe_attachment_get_description (EMailFormatterExtension *extension)
{
	return NULL;
}

static const gchar **
emfqe_attachment_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_quote_attachment_class_init (EMailFormatterQuoteAttachmentClass *class)
{
}

static void
e_mail_formatter_quote_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfqe_attachment_format;
	iface->get_display_name = emfqe_attachment_get_display_name;
	iface->get_description = emfqe_attachment_get_description;
}

static void
e_mail_formatter_quote_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfqe_attachment_mime_types;
}

static void
e_mail_formatter_quote_attachment_init (EMailFormatterQuoteAttachment *formatter)
{

}

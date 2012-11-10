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

#include "e-mail-format-extensions.h"

#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-list.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

static const gchar * formatter_mime_types[] = { "message/rfc822",
					       "application/vnd.evolution.rfc822.end",
					       NULL };

typedef struct _EMailFormatterMessageRFC822 {
	GObject parent;
} EMailFormatterMessageRFC822;

typedef struct _EMailFormatterMessageRFC822Class {
	GObjectClass parent_class;
} EMailFormatterMessageRFC822Class;

static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterMessageRFC822,
	e_mail_formatter_message_rfc822,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

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
		GSList *iter;
		gchar *header, *end;

		header = e_mail_formatter_get_html_header (formatter);
		camel_stream_write_string (stream, header, cancellable, NULL);
		g_free (header);

		/* Print content of the message normally */
		context->mode = E_MAIL_FORMATTER_MODE_NORMAL;

		iter = e_mail_part_list_get_iter (context->parts, part->id);

		end = g_strconcat (part->id, ".end", NULL);
		for (iter = iter->next; iter; iter = g_slist_next (iter)) {
			EMailPart * p = iter->data;
			if (!p)
				continue;

			/* Check for nested rfc822 messages */
			if (g_str_has_suffix (p->id, ".rfc822")) {
				gchar *sub_end = g_strconcat (p->id, ".end", NULL);

				while (iter) {
					p = iter->data;
					if (!p) {
						iter = iter->next;
						continue;
					}

					if (g_strcmp0 (p->id, sub_end) == 0) {
						break;
					}

					iter = iter->next;
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

		context->mode = E_MAIL_FORMATTER_MODE_RAW;

		camel_stream_write_string (stream, "</body></html>", cancellable, NULL);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {

		GSList *iter;
		gchar *end;

		/* Part is EMailPartAttachment */
		iter = e_mail_part_list_get_iter (context->parts, part->id);
		iter = g_slist_next (iter);

		if (!iter || !iter->next || !iter->data)
			return FALSE;

		part = iter->data;
		end = g_strconcat (part->id, ".end", NULL);

		for (iter = iter->next; iter; iter = g_slist_next (iter)) {
			EMailPart * p = iter->data;
			if (!p)
				continue;

			/* Skip attachment bar */
			if (g_str_has_suffix (part->id, ".attachment-bar"))
				continue;

			/* Check for nested rfc822 messages */
			if (g_str_has_suffix (p->id, ".rfc822")) {
				gchar *sub_end = g_strconcat (p->id, ".end", NULL);

				while (iter) {
					p = iter->data;
					if (!p) {
						iter = iter->next;
						continue;
					}

					if (g_strcmp0 (p->id, sub_end) == 0) {
						break;
					}

					iter = iter->next;
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

	} else {
		gchar *str;
		gchar *uri;

		EMailPart *p;
		GSList *iter;

		iter = e_mail_part_list_get_iter (context->parts, part->id);
		if (!iter || !iter->next)
			return FALSE;

		p = iter->data;

		uri = e_mail_part_build_uri (
			context->folder, context->message_uid,
			"part_id", G_TYPE_STRING, p->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"headers_collapsable", G_TYPE_INT, 0,
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
	}

	return TRUE;
}

static const gchar *
emfe_message_rfc822_get_display_name (EMailFormatterExtension *extension)
{
	return _("RFC822 message");
}

static const gchar *
emfe_message_rfc822_get_description (EMailFormatterExtension *extension)
{
	return _("Format part as an RFC822 message");
}

static const gchar **
emfe_message_rfc822_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_message_rfc822_class_init (EMailFormatterMessageRFC822Class *class)
{
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_message_rfc822_format;
	iface->get_display_name = emfe_message_rfc822_get_display_name;
	iface->get_description = emfe_message_rfc822_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_message_rfc822_mime_types;
}

static void
e_mail_formatter_message_rfc822_init (EMailFormatterMessageRFC822 *formatter)
{

}

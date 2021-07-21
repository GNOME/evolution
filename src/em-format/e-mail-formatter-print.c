/*
 * e-mail-formatter-print.c
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

#include "e-mail-formatter-print.h"

#include "e-mail-part-attachment.h"
#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-part.h"

#include <gdk/gdk.h>
#include <glib/gi18n.h>

#define STYLESHEET_URI "evo-file://$EVOLUTION_WEBKITDATADIR/webview-print.css"

/* internal formatter extensions */
GType e_mail_formatter_print_headers_get_type (void);

static gpointer e_mail_formatter_print_parent_class = 0;

static void
mail_formatter_print_write_attachments (EMailFormatter *formatter,
                                        GQueue *attachments,
                                        GOutputStream *stream,
                                        GCancellable *cancellable)
{
	GString *str;

	str = g_string_new (
		"<table border=\"0\" cellspacing=\"5\" cellpadding=\"0\" "
		"class=\"attachments-list\" >\n");
	g_string_append_printf (
		str,
		"<tr><th colspan=\"2\"><h1>%s</h1></td></tr>\n"
		"<tr><th>%s</th><th>%s</th></tr>\n",
		_("Attachments"), _("Name"), _("Size"));

	while (!g_queue_is_empty (attachments)) {
		EMailPartAttachment *part;
		EAttachment *attachment;
		GFileInfo *file_info;
		const gchar *display_name;
		gchar *description;
		gchar *name;
		gchar *size;

		part = g_queue_pop_head (attachments);
		attachment = e_mail_part_attachment_ref_attachment (part);

		file_info = e_attachment_ref_file_info (attachment);
		if (file_info == NULL) {
			g_object_unref (attachment);
			continue;
		}

		description = e_attachment_dup_description (attachment);
		display_name = g_file_info_get_display_name (file_info);

		if (description != NULL && *description != '\0') {
			name = g_strdup_printf (
				"%s (%s)", description, display_name);
		} else {
			name = g_strdup (display_name);
		}

		size = g_format_size (g_file_info_get_size (file_info));

		g_string_append_printf (
			str, "<tr><td>%s</td><td>%s</td></tr>\n",
			name, size);

		g_free (description);
		g_free (name);
		g_free (size);

		g_object_unref (attachment);
		g_object_unref (file_info);
	}

	g_string_append (str, "</table>\n");

	g_output_stream_write_all (
		stream, str->str, str->len, NULL, cancellable, NULL);

	g_string_free (str, TRUE);
}

static void
mail_formatter_print_run (EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          GOutputStream *stream,
                          GCancellable *cancellable)
{
	GQueue queue = G_QUEUE_INIT;
	GQueue attachments = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *string;

	context->mode = E_MAIL_FORMATTER_MODE_PRINTING;

	string =
		"<!DOCTYPE HTML>\n"
		"<html>\n"
		"<head>\n"
		"<meta name=\"generator\" content=\"Evolution Mail\" />\n"
		"<meta name=\"color-scheme\" content=\"light dark\">\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" media=\"print\" href=\"" STYLESHEET_URI "\"/>\n"
		"</head>\n"
		"<body style=\"background: #FFF; color: #000;\">";

	g_output_stream_write_all (
		stream, string, strlen (string), NULL, cancellable, NULL);

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = E_MAIL_PART (link->data);
		const gchar *mime_type;
		gboolean ok;

		if (g_cancellable_is_cancelled (cancellable))
			break;

		if (part->is_hidden && !part->is_error) {
			if (e_mail_part_id_has_suffix (part, ".rfc822")) {
				link = e_mail_formatter_find_rfc822_end_iter (link);
			}

			continue;
		}

		if (!e_mail_part_get_is_printable (part))
			continue;

		mime_type = e_mail_part_get_mime_type (part);
		if (mime_type == NULL)
			continue;

		if (e_mail_part_get_is_attachment (part)) {
			if (e_mail_part_get_cid (part) != NULL)
				continue;

			g_queue_push_tail (&attachments, part);
		}

		ok = e_mail_formatter_format_as (
			formatter, context, part, stream,
			mime_type, cancellable);

		/* If the written part was message/rfc822 then
		 * jump to the end of the message, because content
		 * of the whole message has been formatted by
		 * message_rfc822 formatter */
		if (ok && e_mail_part_id_has_suffix (part, ".rfc822")) {
			link = e_mail_formatter_find_rfc822_end_iter (link);

			continue;
		}
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	/* This consumes the attachments queue. */
	if (!g_queue_is_empty (&attachments))
		mail_formatter_print_write_attachments (
			formatter, &attachments,
			stream, cancellable);

	string = "</body></html>";

	g_output_stream_write_all (
		stream, string, strlen (string),
		NULL, cancellable, NULL);
}

static void
mail_formatter_update_style (EMailFormatter *formatter,
                             GtkStateFlags state)
{
	/* White background */
	GdkRGBA body_color = { 1.0, 1.0, 1.0, 1.0 };
	/* Black text */
	GdkRGBA text_color = { 0.0, 0.0, 0.0, 0.0 };

	g_object_freeze_notify (G_OBJECT (formatter));

	/* Chain up to parent's update_style() method. */
	E_MAIL_FORMATTER_CLASS (e_mail_formatter_print_parent_class)->
		update_style (formatter, state);

	e_mail_formatter_set_color (
		formatter, E_MAIL_FORMATTER_COLOR_FRAME, &body_color);
	e_mail_formatter_set_color (
		formatter, E_MAIL_FORMATTER_COLOR_CONTENT, &body_color);
	e_mail_formatter_set_color (
		formatter, E_MAIL_FORMATTER_COLOR_TEXT, &text_color);

	g_object_thaw_notify (G_OBJECT (formatter));
}

static void
e_mail_formatter_print_init (EMailFormatterPrint *formatter)
{
}

static void
e_mail_formatter_print_class_init (EMailFormatterPrintClass *class)
{
	EMailFormatterClass *formatter_class;

	e_mail_formatter_print_parent_class = g_type_class_peek_parent (class);

	formatter_class = E_MAIL_FORMATTER_CLASS (class);
	formatter_class->run = mail_formatter_print_run;
	formatter_class->update_style = mail_formatter_update_style;
}

static void
e_mail_formatter_print_base_init (EMailFormatterPrintClass *class)
{
	/* Register internal extensions. */
	g_type_ensure (e_mail_formatter_print_headers_get_type ());

	e_mail_formatter_extension_registry_load (
		E_MAIL_FORMATTER_CLASS (class)->extension_registry,
		E_TYPE_MAIL_FORMATTER_PRINT_EXTENSION);

	E_MAIL_FORMATTER_CLASS (class)->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
}

EMailFormatter *
e_mail_formatter_print_new (void)
{
	return g_object_new (E_TYPE_MAIL_FORMATTER_PRINT, NULL);
}

GType
e_mail_formatter_print_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EMailFormatterClass),
			(GBaseInitFunc) e_mail_formatter_print_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_mail_formatter_print_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */
			sizeof (EMailFormatterPrint),
			0,	/* n_preallocs */
			(GInstanceInitFunc) e_mail_formatter_print_init,
			NULL	/* value_table */
		};

		type = g_type_register_static (
			E_TYPE_MAIL_FORMATTER,
			"EMailFormatterPrint", &type_info, 0);
	}

	return type;
}

/* ------------------------------------------------------------------------- */

G_DEFINE_ABSTRACT_TYPE (
	EMailFormatterPrintExtension,
	e_mail_formatter_print_extension,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static void
e_mail_formatter_print_extension_class_init (EMailFormatterPrintExtensionClass *class)
{
}

static void
e_mail_formatter_print_extension_init (EMailFormatterPrintExtension *extension)
{
}


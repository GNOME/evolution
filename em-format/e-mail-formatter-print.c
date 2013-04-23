/*
 * e-mail-formatter-print.c
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

#include "e-mail-formatter-print.h"

#include "e-mail-part-attachment.h"
#include "e-mail-formatter-extension.h"
#include "e-mail-formatter-utils.h"
#include "e-mail-part.h"

#include <gdk/gdk.h>
#include <glib/gi18n.h>

/* internal formatter extensions */
GType e_mail_formatter_print_headers_get_type (void);

void e_mail_formatter_print_internal_extensions_load (EMailExtensionRegistry *ereg);

static gpointer e_mail_formatter_print_parent_class = 0;

static void
write_attachments_list (EMailFormatter *formatter,
                        EMailFormatterContext *context,
                        GSList *attachments,
                        CamelStream *stream,
                        GCancellable *cancellable)
{
	GString *str;
	GSList *link;

	if (!attachments)
		return;

	str = g_string_new (
		"<table border=\"0\" cellspacing=\"5\" cellpadding=\"0\" "
		"class=\"attachments-list\" >\n");
	g_string_append_printf (
		str,
		"<tr><th colspan=\"2\"><h1>%s</h1></td></tr>\n"
		"<tr><th>%s</th><th>%s</th></tr>\n",
		_("Attachments"), _("Name"), _("Size"));

	for (link = attachments; link != NULL; link = g_slist_next (link)) {
		EMailPartAttachment *part = link->data;
		EAttachment *attachment;
		GFileInfo *fi;
		gchar *name, *size;

		if (!part)
			continue;

		attachment = part->attachment;
		fi = e_attachment_get_file_info (attachment);
		if (!fi)
			continue;

		if (e_attachment_get_description (attachment) &&
                    *e_attachment_get_description (attachment)) {
			name = g_strdup_printf (
				"%s (%s)",
				e_attachment_get_description (attachment),
				g_file_info_get_display_name (fi));
		} else {
			name = g_strdup (g_file_info_get_display_name (fi));
		}

		size = g_format_size (g_file_info_get_size (fi));

		g_string_append_printf (
			str, "<tr><td>%s</td><td>%s</td></tr>\n",
			name, size);

		g_free (name);
		g_free (size);
	}

	g_string_append (str, "</table>\n");

	camel_stream_write_string (stream, str->str, cancellable, NULL);
	g_string_free (str, TRUE);
}

static void
mail_formatter_print_run (EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          CamelStream *stream,
                          GCancellable *cancellable)
{
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	GSList *attachments;

	context->mode = E_MAIL_FORMATTER_MODE_PRINTING;

	camel_stream_write_string (
		stream,
		"<!DOCTYPE HTML>\n<html>\n"
		"<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\" />\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" media=\"print\" "
		"href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview-print.css\" />\n"
		"</head>\n"
		"<body style=\"background: #FFF; color: #000;\">",
		cancellable, NULL);

	attachments = NULL;

	e_mail_part_list_queue_parts (context->part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = link->data;
		gboolean ok;

		if (g_cancellable_is_cancelled (cancellable))
			break;

		if (part->is_hidden && !part->is_error) {
			if (g_str_has_suffix (part->id, ".rfc822")) {
				link = e_mail_formatter_find_rfc822_end_iter (link);
			}

			continue;
		}

		if (!part->mime_type)
			continue;

		if (part->is_attachment) {
			if (part->cid != NULL)
				continue;

			attachments = g_slist_append (attachments, part);
		}

		ok = e_mail_formatter_format_as (
			formatter, context, part, stream,
			part->mime_type, cancellable);

		/* If the written part was message/rfc822 then
		 * jump to the end of the message, because content
		 * of the whole message has been formatted by
		 * message_rfc822 formatter */
		if (ok && g_str_has_suffix (part->id, ".rfc822")) {
			link = e_mail_formatter_find_rfc822_end_iter (link);

			continue;
		}
	}

	while (!g_queue_is_empty (&queue))
		e_mail_part_unref (g_queue_pop_head (&queue));

	write_attachments_list (formatter, context, attachments, stream, cancellable);

	g_slist_free (attachments);

	camel_stream_write_string (stream, "</body></html>", cancellable, NULL);
}

static void
mail_formatter_update_style (EMailFormatter *formatter,
			     GtkStateFlags state)
{
	EMailFormatterClass *formatter_class;

	/* White background */
	GdkRGBA body_color = { 1.0, 1.0, 1.0, 1.0 };
	/* Black text */
	GdkRGBA text_color = { 0.0, 0.0, 0.0, 0.0 };

	g_object_freeze_notify (G_OBJECT (formatter));

	/* Set the other colors */
	formatter_class = E_MAIL_FORMATTER_CLASS (e_mail_formatter_print_parent_class);
	formatter_class->update_style (formatter, state);

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
e_mail_formatter_print_finalize (GObject *object)
{
	/* Chain up to parent's finalize() */
	G_OBJECT_CLASS (e_mail_formatter_print_parent_class)->finalize (object);
}

static void
e_mail_formatter_print_class_init (EMailFormatterPrintClass *class)
{
	GObjectClass *object_class;
	EMailFormatterClass *formatter_class;

	e_mail_formatter_print_parent_class = g_type_class_peek_parent (class);

	formatter_class = E_MAIL_FORMATTER_CLASS (class);
	formatter_class->run = mail_formatter_print_run;
	formatter_class->update_style = mail_formatter_update_style;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_mail_formatter_print_finalize;
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


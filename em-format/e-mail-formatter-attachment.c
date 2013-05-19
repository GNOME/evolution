/*
 * e-mail-formatter-attachment.c
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

#include <glib/gi18n-lib.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-attachment-bar.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

#define d(x)

typedef EMailFormatterExtension EMailFormatterAttachment;
typedef EMailFormatterExtensionClass EMailFormatterAttachmentClass;

GType e_mail_formatter_attachment_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterAttachment,
	e_mail_formatter_attachment,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	E_MAIL_PART_ATTACHMENT_MIME_TYPE,
	"application/vnd.evolution.widget.attachment-button",
	NULL
};

static EAttachmentStore *
find_attachment_store (EMailPartList *part_list,
                       EMailPart *start)
{
	EAttachmentStore *store = NULL;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	const gchar *start_id;
	gchar *tmp, *pos;
	EMailPart *part;
	gchar *id;

	start_id = e_mail_part_get_id (start);

	e_mail_part_list_queue_parts (part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	id = g_strconcat (start_id, ".attachment-bar", NULL);
	tmp = g_strdup (id);
	part = NULL;
	do {
		d (printf ("Looking up attachment bar as %s\n", id));

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;
			const gchar *p_id;

			p_id = e_mail_part_get_id (p);

			if (g_strcmp0 (p_id, id) == 0) {
				part = p;
				break;
			}
		}

		pos = g_strrstr (tmp, ".");
		if (!pos)
			break;

		g_free (id);
		g_free (tmp);
		tmp = g_strndup (start_id, pos - tmp);
		id = g_strdup_printf ("%s.attachment-bar", tmp);

	} while (pos && !part);

	g_free (id);
	g_free (tmp);

	if (part != NULL)
		store = E_MAIL_PART_ATTACHMENT_BAR (part)->store;

	while (!g_queue_is_empty (&queue))
		e_mail_part_unref (g_queue_pop_head (&queue));

	return store;
}

static gboolean
emfe_attachment_format (EMailFormatterExtension *extension,
                        EMailFormatter *formatter,
                        EMailFormatterContext *context,
                        EMailPart *part,
                        CamelStream *stream,
                        GCancellable *cancellable)
{
	gchar *str, *text, *html;
	gchar *button_id;
	EAttachmentStore *store;
	EMailExtensionRegistry *reg;
	GQueue *extensions;
	EMailPartAttachment *empa;
	CamelMimePart *mime_part;
	CamelMimeFilterToHTMLFlags flags;
	const gchar *attachment_part_id;
	const gchar *part_id;

	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT (part), FALSE);

	empa = (EMailPartAttachment *) part;
	part_id = e_mail_part_get_id (part);

	if ((context->mode == E_MAIL_FORMATTER_MODE_NORMAL) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_ALL_HEADERS)) {
		EAttachment *attachment;
		GList *head, *link;

		attachment = e_mail_part_attachment_ref_attachment (
			E_MAIL_PART_ATTACHMENT (part));

		head = g_queue_peek_head_link (&part->validities);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPartValidityPair *pair = link->data;

			if (pair == NULL)
				continue;

			if ((pair->validity_type & E_MAIL_PART_VALIDITY_SIGNED) != 0)
				e_attachment_set_signed (
					attachment,
					pair->validity->sign.status);

			if ((pair->validity_type & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0)
				e_attachment_set_encrypted (
					attachment,
					pair->validity->encrypt.status);
		}

		store = find_attachment_store (context->part_list, part);
		if (store) {
			GList *attachments = e_attachment_store_get_attachments (store);
			if (!g_list_find (attachments, attachment)) {
				e_attachment_store_add_attachment (
					store, attachment);
			}
			g_list_free (attachments);
		} else {
			g_warning ("Failed to locate attachment-bar for %s", part_id);
		}

		g_object_unref (attachment);
	}

	/* If the attachment is requested as RAW, then call the
	 * handler directly and do not append any other code. */
	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {
		EMailExtensionRegistry *registry;
		GQueue *extensions;
		GList *iter;

		registry = e_mail_formatter_get_extension_registry (formatter);

		extensions = e_mail_extension_registry_get_for_mime_type (
			registry, empa->snoop_mime_type);
		if (extensions == NULL)
			extensions = e_mail_extension_registry_get_fallback (
				registry, empa->snoop_mime_type);

		if (!extensions)
			return FALSE;

		if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
			gchar *name;
			EAttachment *attachment;
			GFileInfo *fi;
			const gchar *description;
			const gchar *display_name;

			attachment = e_mail_part_attachment_ref_attachment (
				E_MAIL_PART_ATTACHMENT (part));

			fi = e_attachment_get_file_info (attachment);
			display_name = g_file_info_get_display_name (fi);

			description = e_attachment_get_description (attachment);
			if (description && *description) {
				name = g_strdup_printf (
					"<h2>Attachment: %s (%s)</h2>\n",
					description, display_name);
			} else {
				name = g_strdup_printf (
					"<h2>Attachment: %s</h2>\n",
					display_name);
			}

			camel_stream_write_string (stream, name, cancellable, NULL);
			g_free (name);

			g_object_unref (attachment);
		}

		for (iter = g_queue_peek_head_link (extensions); iter; iter = iter->next) {

			EMailFormatterExtension *ext;
			ext = iter->data;
			if (!ext)
				continue;

			if (e_mail_formatter_extension_format (ext, formatter,
				context, part, stream, cancellable)) {
				return TRUE;
			}
		}

		return FALSE;
	}

	/* E_MAIL_FORMATTER_MODE_NORMAL: */

	reg = e_mail_formatter_get_extension_registry (formatter);
	extensions = e_mail_extension_registry_get_for_mime_type (
				reg, empa->snoop_mime_type);

	if (!extensions) {
		extensions = e_mail_extension_registry_get_fallback (
				reg, empa->snoop_mime_type);
	}

	mime_part = e_mail_part_ref_mime_part (part);
	text = e_mail_part_describe (mime_part, empa->snoop_mime_type);
	flags = e_mail_formatter_get_text_format_flags (formatter);
	html = camel_text_to_html (
		text, flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	g_free (text);
	g_object_unref (mime_part);

	if (empa->attachment_view_part_id)
		attachment_part_id = empa->attachment_view_part_id;
	else
		attachment_part_id = part_id;

	button_id = g_strconcat (attachment_part_id, ".attachment_button", NULL);

	str = g_strdup_printf (
		"<div class=\"attachment\">"
		"<table width=\"100%%\" border=\"0\">"
		"<tr valign=\"middle\">"
		"<td align=\"left\" width=\"100\">"
		"<object type=\"application/vnd.evolution.widget.attachment-button\" "
		"height=\"20\" width=\"100\" data=\"%s\" id=\"%s\"></object>"
		"</td>"
		"<td align=\"left\">%s</td>"
		"</tr>", part_id, button_id, html);

	camel_stream_write_string (stream, str, cancellable, NULL);
	g_free (button_id);
	g_free (str);
	g_free (html);

	if (extensions) {
		GList *iter;
		CamelStream *content_stream;
		gboolean ok;

		content_stream = camel_stream_mem_new ();
		ok = FALSE;
		if (empa->attachment_view_part_id != NULL) {
			EMailPart *attachment_view_part;

			attachment_view_part = e_mail_part_list_ref_part (
				context->part_list,
				empa->attachment_view_part_id);

			/* Avoid recursion. */
			if (attachment_view_part == part) {
				e_mail_part_unref (attachment_view_part);
				attachment_view_part = NULL;
			}

			if (attachment_view_part != NULL) {
				ok = e_mail_formatter_format_as (
					formatter, context,
					attachment_view_part,
					content_stream, NULL,
					cancellable);
				e_mail_part_unref (attachment_view_part);
			}

		} else {

			for (iter = g_queue_peek_head_link (extensions); iter; iter = iter->next) {

				EMailFormatterExtension *ext;

				ext = iter->data;
				if (!ext)
					continue;

				if (e_mail_formatter_extension_format (
						ext, formatter, context,
						part, content_stream,
						cancellable)) {
					ok = TRUE;
					break;
				}
			}
		}

		if (ok) {
			str = g_strdup_printf (
				"<tr><td colspan=\"2\">"
				"<div class=\"attachment-wrapper\" id=\"%s\">",
				attachment_part_id);

			camel_stream_write_string (
				stream, str, cancellable, NULL);
			g_free (str);

			g_seekable_seek (
				G_SEEKABLE (content_stream), 0,
				G_SEEK_SET, cancellable, NULL);
			camel_stream_write_to_stream (
					content_stream, stream,
					cancellable, NULL);

			camel_stream_write_string (
				stream, "</div></td></tr>", cancellable, NULL);
		}

		g_object_unref (content_stream);
	}

	camel_stream_write_string (stream, "</table></div>", cancellable, NULL);

	return TRUE;
}

static GtkWidget *
emfe_attachment_get_widget (EMailFormatterExtension *extension,
                            EMailPartList *context,
                            EMailPart *part,
                            GHashTable *params)
{
	EAttachment *attachment;
	EAttachmentStore *store;
	EAttachmentView *view;
	GtkWidget *widget;
	const gchar *part_id;

	g_return_val_if_fail (E_IS_MAIL_PART_ATTACHMENT (part), NULL);

	attachment = e_mail_part_attachment_ref_attachment (
		E_MAIL_PART_ATTACHMENT (part));

	part_id = e_mail_part_get_id (part);

	store = find_attachment_store (context, part);
	widget = e_attachment_button_new ();
	g_object_set_data_full (
		G_OBJECT (widget),
		"uri", g_strdup (part_id),
		(GDestroyNotify) g_free);
	e_attachment_button_set_attachment (
		E_ATTACHMENT_BUTTON (widget), attachment);

	view = g_object_get_data (G_OBJECT (store), "attachment-bar");
	if (view != NULL)
		e_attachment_button_set_view (
			E_ATTACHMENT_BUTTON (widget), view);

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_show (widget);

	g_object_unref (attachment);

	return widget;
}

static void
e_mail_formatter_attachment_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Attachment");
	class->description =  _("Display as attachment");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_attachment_format;
	class->get_widget = emfe_attachment_get_widget;
}

static void
e_mail_formatter_attachment_init (EMailFormatterExtension *extension)
{
}

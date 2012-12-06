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

#include "e-mail-format-extensions.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-attachment-bar.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-inline-filter.h>
#include <e-util/e-util.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include <widgets/misc/e-attachment-button.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#define d(x)

typedef struct _EMailFormatterAttachment {
	GObject parent;
} EMailFormatterAttachment;

typedef struct _EMailFormatterAttachmentClass {
	GObjectClass parent_class;
} EMailFormatterAttachmentClass;

static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterAttachment,
	e_mail_formatter_attachment,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init)
)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.attachment",
	"application/vnd.evolution.widget.attachment-button",
	NULL
};

static EAttachmentStore *
find_attachment_store (EMailPartList *part_list,
                       const gchar *start_id)
{
	EAttachmentStore *store = NULL;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	gchar *tmp, *pos;
	EMailPart *part;
	gchar *id;

	e_mail_part_list_queue_parts (part_list, NULL, &queue);

	head = g_queue_peek_head_link (&queue);

	id = g_strconcat (start_id, ".attachment-bar", NULL);
	tmp = g_strdup (id);
	part = NULL;
	do {
		d (printf ("Looking up attachment bar as %s\n", id));

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *p = link->data;

			if (g_strcmp0 (p->id, id) == 0) {
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
		store = ((EMailPartAttachmentBar *) part)->store;

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
	gchar *attachment_part_id;

	g_return_val_if_fail (E_MAIL_PART_IS (part, EMailPartAttachment), FALSE);

	empa = (EMailPartAttachment *) part;

	if ((context->mode == E_MAIL_FORMATTER_MODE_NORMAL) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_ALL_HEADERS)) {
		if (part->validities) {
			GSList *lst;

			for (lst = part->validities; lst; lst = lst->next) {
				EMailPartValidityPair *pair = lst->data;

				if (!pair)
					continue;

				if ((pair->validity_type & E_MAIL_PART_VALIDITY_SIGNED) != 0)
					e_attachment_set_signed (empa->attachment, pair->validity->sign.status);

				if ((pair->validity_type & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0)
					e_attachment_set_encrypted (empa->attachment, pair->validity->encrypt.status);
			}
		}

		store = find_attachment_store (context->part_list, part->id);
		if (store) {
			GList *attachments = e_attachment_store_get_attachments (store);
			if (!g_list_find (attachments, empa->attachment)) {
				e_attachment_store_add_attachment (
					store, empa->attachment);
			}
			g_list_free (attachments);
		} else {
			g_warning ("Failed to locate attachment-bar for %s", part->id);
		}
	}

        /* If the attachment is requested as RAW, then call the handler directly
         * and do not append any other code. */
	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {
		EMailExtensionRegistry *reg;
		GQueue *extensions;
		GList *iter;
		reg = e_mail_formatter_get_extension_registry (formatter);

		extensions = e_mail_extension_registry_get_for_mime_type (
					reg, empa->snoop_mime_type);
		if (!extensions) {
			extensions = e_mail_extension_registry_get_fallback (
					reg, empa->snoop_mime_type);
		}

		if (!extensions)
			return FALSE;

		if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
			gchar *name;
			EAttachment *attachment;
			GFileInfo *fi;
			const gchar *description;

			attachment = empa->attachment;
			fi = e_attachment_get_file_info (attachment);

			description = e_attachment_get_description (attachment);
			if (description && *description) {
				name = g_strdup_printf (
					"<h2>Attachment: %s (%s)</h2>\n",
					description, g_file_info_get_display_name (fi));
			} else {
				name = g_strdup_printf (
					"<h2>Attachment: %s</h2>\n",
					g_file_info_get_display_name (fi));
			}

			camel_stream_write_string (stream, name, cancellable, NULL);
			g_free (name);
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

	text = e_mail_part_describe (part->part, empa->snoop_mime_type);
	html = camel_text_to_html (
		text, e_mail_formatter_get_text_format_flags (formatter) &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	g_free (text);

	if (empa->attachment_view_part_id)
		attachment_part_id = empa->attachment_view_part_id;
	else
		attachment_part_id = part->id;

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
		"</tr>", part->id, button_id, html);

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
	EMailPartAttachment *empa;
	EAttachmentStore *store;
	EAttachmentView *view;
	GtkWidget *widget;

	g_return_val_if_fail (E_MAIL_PART_IS (part, EMailPartAttachment), NULL);
	empa = (EMailPartAttachment *) part;

	store = find_attachment_store (context, part->id);
	widget = e_attachment_button_new ();
	g_object_set_data (G_OBJECT (widget), "uri", part->id);
	e_attachment_button_set_attachment (
		E_ATTACHMENT_BUTTON (widget), empa->attachment);
	view = g_object_get_data (G_OBJECT (store), "attachment-bar");
	if (view) {
		e_attachment_button_set_view (
			E_ATTACHMENT_BUTTON (widget), view);
	}

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_show (widget);

	return widget;
}

static const gchar *
emfe_attachment_get_display_name (EMailFormatterExtension *extension)
{
	return _("Attachment");
}

static const gchar *
emfe_attachment_get_description (EMailFormatterExtension *extension)
{
	return _("Display as attachment");
}

static void
e_mail_formatter_attachment_class_init (EMailFormatterAttachmentClass *class)
{
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->mime_types = formatter_mime_types;
	iface->format = emfe_attachment_format;
	iface->get_widget = emfe_attachment_get_widget;
	iface->get_display_name = emfe_attachment_get_display_name;
	iface->get_description = emfe_attachment_get_description;
}

static void
e_mail_formatter_attachment_init (EMailFormatterAttachment *formatter)
{

}

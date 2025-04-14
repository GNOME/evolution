/*
 * e-mail-formatter-attachment.c
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

#include <glib/gi18n-lib.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"
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
	"application/vnd.evolution.attachment-button",
	NULL
};

static gboolean
emfe_attachment_format (EMailFormatterExtension *extension,
                        EMailFormatter *formatter,
                        EMailFormatterContext *context,
                        EMailPart *part,
                        GOutputStream *stream,
                        GCancellable *cancellable)
{
	gchar *text, *html;
	gchar *button_id;
	EMailExtensionRegistry *registry;
	GQueue *extensions;
	EMailPartAttachment *empa;
	CamelMimePart *mime_part;
	CamelMimeFilterToHTMLFlags flags;
	GOutputStream *content_stream = NULL;
	GString *buffer;
	gint icon_width, icon_height;
	gchar *icon_uri;
	CamelContentType *content_type;
	gboolean part_too_large = FALSE;
	gpointer attachment_ptr = NULL;
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

		attachment = e_mail_part_attachment_ref_attachment (E_MAIL_PART_ATTACHMENT (part));
		attachment_ptr = attachment;

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

		e_attachment_set_initially_shown (attachment, e_mail_part_should_show_inline (part));

		e_mail_formatter_claim_attachment (formatter, attachment);

		if (e_attachment_get_is_possible (attachment)) {
			g_object_unref (attachment);
			return TRUE;
		}

		g_object_unref (attachment);
	}

	registry = e_mail_formatter_get_extension_registry (formatter);

	extensions = e_mail_extension_registry_get_for_mime_type (
		registry, e_mail_part_attachment_get_guessed_mime_type (empa));
	if (extensions == NULL)
		extensions = e_mail_extension_registry_get_fallback (
			registry, e_mail_part_attachment_get_guessed_mime_type (empa));

	/* If the attachment is requested as RAW, then call the
	 * handler directly and do not append any other code. */
	if ((context->mode == E_MAIL_FORMATTER_MODE_RAW) ||
	    (context->mode == E_MAIL_FORMATTER_MODE_PRINTING)) {
		GList *head, *link;
		gboolean success = FALSE;

		if (extensions == NULL)
			return FALSE;

		if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
			gchar *name;
			EAttachment *attachment;
			GFileInfo *file_info;
			GSettings *settings;
			const gchar *display_name;
			gchar *description;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");
			if (!g_settings_get_boolean (settings, "print-attachments")) {
				g_clear_object (&settings);
				return TRUE;
			}
			g_clear_object (&settings);

			attachment = e_mail_part_attachment_ref_attachment (
				E_MAIL_PART_ATTACHMENT (part));

			file_info = e_attachment_ref_file_info (attachment);
			if (file_info && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
				display_name = g_file_info_get_display_name (file_info);
			else
				display_name = "";

			description = e_attachment_dup_description (attachment);
			if (description != NULL && *description != '\0') {
				name = g_strdup_printf (
					"<h2>Attachment: %s (%s)</h2>\n",
					description, display_name);
			} else {
				name = g_strdup_printf (
					"<h2>Attachment: %s</h2>\n",
					display_name);
			}

			g_output_stream_write_all (
				stream, name, strlen (name),
				NULL, cancellable, NULL);

			g_free (description);
			g_free (name);

			g_clear_object (&attachment);
			g_clear_object (&file_info);
		}

		head = g_queue_peek_head_link (extensions);

		for (link = head; link != NULL; link = g_list_next (link)) {
			success = e_mail_formatter_extension_format (
				E_MAIL_FORMATTER_EXTENSION (link->data),
				formatter, context, part, stream, cancellable);
			if (success)
				break;
		}

		return success;
	}

	/* E_MAIL_FORMATTER_MODE_NORMAL: */

	mime_part = e_mail_part_ref_mime_part (part);
	text = e_mail_part_describe (mime_part, e_mail_part_attachment_get_guessed_mime_type (empa));
	flags = e_mail_formatter_get_text_format_flags (formatter);
	html = camel_text_to_html (
		text, flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	g_free (text);

	content_type = camel_mime_part_get_content_type (mime_part);

	if (camel_content_type_is (content_type, "text", "*") ||
	    camel_content_type_is (content_type, "application", "xml") ||
	    (e_mail_part_attachment_get_guessed_mime_type (empa) && (
	     g_ascii_strncasecmp (e_mail_part_attachment_get_guessed_mime_type (empa), "text/", 5) == 0 ||
	     g_ascii_strcasecmp (e_mail_part_attachment_get_guessed_mime_type (empa), "application/xml") == 0))) {
		GSettings *settings;
		gsize size_limit;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		size_limit = g_settings_get_int (settings, "preview-text-size-limit");
		g_object_unref (settings);

		if (size_limit > 0) {
			gsize part_size;

			part_size = camel_data_wrapper_calculate_decoded_size_sync (CAMEL_DATA_WRAPPER (mime_part), cancellable, NULL);
			part_too_large = part_size > 1024 * size_limit;

			if (part_too_large && part_size != ((gsize) -1)) {
				EAttachment *attachment;

				e_mail_part_attachment_set_expandable (empa, FALSE);

				attachment = e_mail_part_attachment_ref_attachment (E_MAIL_PART_ATTACHMENT (part));
				e_attachment_set_can_show (attachment, FALSE);
				e_attachment_set_initially_shown (attachment, FALSE);
				g_object_unref (attachment);
			}
		}
	}

	g_object_unref (mime_part);

	if (empa->part_id_with_attachment)
		attachment_part_id = empa->part_id_with_attachment;
	else
		attachment_part_id = part_id;

	button_id = g_strconcat (attachment_part_id, ".attachment_button", NULL);

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &icon_width, &icon_height)) {
		icon_width = 16;
		icon_height = 16;
	}

	if (extensions != NULL) {
		gboolean success = FALSE;

		content_stream = g_memory_output_stream_new_resizable ();

		if (empa->part_id_with_attachment != NULL) {
			EMailPart *attachment_view_part;

			attachment_view_part = e_mail_part_list_ref_part (
				context->part_list,
				empa->part_id_with_attachment);

			/* Avoid recursion. */
			if (attachment_view_part == part)
				g_clear_object (&attachment_view_part);

			if (attachment_view_part != NULL) {
				success = e_mail_formatter_format_as (
					formatter, context,
					attachment_view_part,
					content_stream, NULL,
					cancellable);
				g_object_unref (attachment_view_part);
			}

		} else {
			GList *head, *link;

			head = g_queue_peek_head_link (extensions);

			for (link = head; link != NULL; link = g_list_next (link)) {
				success = e_mail_formatter_extension_format (
					E_MAIL_FORMATTER_EXTENSION (link->data),
					formatter, context,
					part, content_stream,
					cancellable);
				if (success)
					break;
			}
		}

		e_mail_part_attachment_set_expandable (empa, success);
	}

	icon_uri = e_mail_part_build_uri (
		e_mail_part_list_get_folder (context->part_list),
		e_mail_part_list_get_message_uid (context->part_list),
		"part_id", G_TYPE_STRING, part_id,
		"attachment_icon", G_TYPE_POINTER, attachment_ptr,
		"size", G_TYPE_INT, icon_width,
		NULL);

	/* XXX Wild guess at the initial size. */
	buffer = g_string_sized_new (8192);

	g_string_append_printf (
		buffer,
		"<div class=\"attachment\">"
		"<table width=\"100%%\" border=\"0\" style=\"border-spacing: 0px\">"
		"<tr valign=\"middle\">"
		"<td align=\"left\" width=\"1px\" style=\"white-space:pre;\">"
		"<button type=\"button\" class=\"attachment-expander\" id=\"%s\" value=\"%p\" style=\"vertical-align:middle; margin:0px;\" title=\"%s\">"
		"<img id=\"attachment-expander-img-%p\" src=\"gtk-stock://%s?size=%d\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle;\">"
		"<img src=\"%s\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle;\">"
		"</button>"
		"<button type=\"button\" class=\"attachment-menu\" id=\"%s\" value=\"%p\" style=\"vertical-align:middle; margin:0px;\" title=\"%s\">"
		"<img src=\"gtk-stock://x-evolution-arrow-down?size=%d\" width=\"%dpx\" height=\"%dpx\" style=\"vertical-align:middle;\">"
		"</button>"
		"</td><td align=\"left\">%s</td></tr>",
		part_id, attachment_ptr,
		!part_too_large && (e_mail_part_should_show_inline (part) || e_mail_part_attachment_get_expandable (empa)) ?
		_("Toggle View Inline") : _("Open in default application"),
		attachment_ptr,
		part_too_large ? "go-top" : e_mail_part_should_show_inline (part) ? "go-down" : e_mail_part_attachment_get_expandable (empa) ? "go-next" : "go-top",
		GTK_ICON_SIZE_BUTTON, icon_width, icon_height,
		icon_uri, icon_width, icon_height,
		part_id, attachment_ptr, _("Options"), GTK_ICON_SIZE_BUTTON, icon_width, icon_height,
		html);

	g_free (icon_uri);
	g_free (button_id);
	g_free (html);

	if (!part_too_large && content_stream && e_mail_part_attachment_get_expandable (empa)) {
		gchar *wrapper_element_id;
		gconstpointer data;
		gsize size;

		wrapper_element_id = g_strdup_printf ("attachment-wrapper-%p", attachment_ptr);

		data = g_memory_output_stream_get_data (
			G_MEMORY_OUTPUT_STREAM (content_stream));
		size = g_memory_output_stream_get_data_size (
			G_MEMORY_OUTPUT_STREAM (content_stream));

		g_string_append_printf (
			buffer,
			"<tr><td colspan=\"2\">"
			"<div class=\"attachment-wrapper\" id=\"%s\"",
			wrapper_element_id);

		if (e_mail_part_should_show_inline (part)) {
			g_string_append_c (buffer, '>');
			g_string_append_len (buffer, data, size);
		} else {
			gchar *inner_html_data;

			inner_html_data = g_markup_escape_text (data, size);

			g_string_append_printf (buffer, " related-part-id=\"%s\" inner-html-data=\"%s\">",
				attachment_part_id, inner_html_data);

			g_free (inner_html_data);
		}

		g_string_append (buffer, "</div></td></tr>");

		g_free (wrapper_element_id);
	}

	g_clear_object (&content_stream);

	g_string_append (buffer, "</table></div>");

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return TRUE;
}

static void
e_mail_formatter_attachment_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Attachment");
	class->description = _("Display as attachment");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_attachment_format;
}

static void
e_mail_formatter_attachment_init (EMailFormatterExtension *extension)
{
}

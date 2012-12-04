/*
 * e-mail-formatter-text-html.c
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

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-inline-filter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include <ctype.h>
#include <string.h>

static const gchar *formatter_mime_types[] = { "text/html", NULL };

typedef struct _EMailFormatterTextHTML {
	GObject parent;
} EMailFormatterTextHTML;

typedef struct _EMailFormatterTextHTMLClass {
	GObjectClass parent_class;
} EMailFormatterTextHTMLClass;

static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailFormatterTextHTML,
	e_mail_formatter_text_html,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static gchar *
get_tag (const gchar *utf8_string,
         const gchar *tag_name,
         gchar *opening,
         gchar *closing)
{
	gchar *t;
	gunichar c;
	gboolean has_end;

	c = '\0';
	t = g_utf8_find_prev_char (utf8_string, closing);
	while (t != opening) {

		c = g_utf8_get_char (t);
		if (!g_unichar_isspace (c))
			break;
	}

	/* Not a pair tag */
	if (c == '/')
		return g_strndup (opening, closing - opening + 1);

	t = closing;
	while (t) {
		c = g_utf8_get_char (t);
		if (c == '<') {
			if (t[1] == '!' && t[2] == '-' && t[3] == '-') {
				/* it's a comment start, read until the end of "-->" */
				gchar *end = strstr (t + 4, "-->");
				if (end) {
					t = end + 2;
				} else
					break;
			} else
				break;
		}

		t = g_utf8_find_next_char (t, NULL);
	}

	has_end = FALSE;
	do {
		c = g_utf8_get_char (t);

		if (c == '/') {
			has_end = TRUE;
			break;
		}

		if (c == '>') {
			has_end = FALSE;
			break;
		}

		t = g_utf8_find_next_char (t, NULL);

	} while (t);

	/* Broken HTML? */
	if (!has_end)
		return NULL;

	do {
		c = g_utf8_get_char (t);
		if ((c != ' ') && (c != '/'))
			break;

		t = g_utf8_find_next_char (t, NULL);
	} while (t);

	/* tag_name is always ASCII */
	if (g_ascii_strncasecmp (t, tag_name, strlen (tag_name)) == 0) {

		closing = g_utf8_strchr (t, -1, '>');

		return g_strndup (opening, closing - opening + 1);
	}

	/* Broken HTML? */
	return NULL;
}

static gboolean
emfe_text_html_format (EMailFormatterExtension *extension,
                       EMailFormatter *formatter,
                       EMailFormatterContext *context,
                       EMailPart *part,
                       CamelStream *stream,
                       GCancellable *cancellable)
{
	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		/* FORMATTER FIXME: Shouldn't we have some extra method for
		 * BASE64 and QP decoding?? */
		e_mail_formatter_format_text (formatter, part, stream, cancellable);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		GString *string;
		GByteArray *ba;
		gchar *pos;
		GList *tags, *iter;
		gboolean valid;
		gchar *tag;
		const gchar *document_end;
		gint length;
		gint i;
		CamelStream *decoded_stream;

		decoded_stream = camel_stream_mem_new ();
		/* FORMATTER FIXME: See above */
		e_mail_formatter_format_text (formatter, part, decoded_stream, cancellable);
		g_seekable_seek (G_SEEKABLE (decoded_stream), 0, G_SEEK_SET, cancellable, NULL);

		ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (decoded_stream));
		string = g_string_new_len ((gchar *) ba->data, ba->len);

		g_object_unref (decoded_stream);

		if (!g_utf8_validate (string->str, -1, NULL)) {
			gchar *valid_utf8;

			valid_utf8 = e_util_utf8_make_valid (string->str);
			g_string_free (string, TRUE);
			string = g_string_new (valid_utf8);
			g_free (valid_utf8);
		}

		tags = NULL;
		pos = string->str;
		valid = FALSE;

		do {
			gchar *tmp;
			gchar *closing;
			gchar *opening;

			tmp = g_utf8_find_next_char (pos, NULL);
			pos = g_utf8_strchr (tmp, -1, '<');
			if (!pos)
				break;

			opening = pos;
			closing = g_utf8_strchr (pos, -1, '>');

			/* Find where the actual tag name begins */
			tag = g_utf8_find_next_char (pos, NULL);
			while ((tag = g_utf8_find_next_char (pos, NULL)) != NULL) {
				gunichar c = g_utf8_get_char (tag);
				if (!g_unichar_isspace (c))
					break;

			}

			if (g_ascii_strncasecmp (tag, "style", 5) == 0) {
				tags = g_list_append (
					tags,
					get_tag (string->str, "style", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "script", 6) == 0) {
				tags = g_list_append (
					tags,
					get_tag (string->str, "script", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "link", 4) == 0) {
				tags = g_list_append (
					tags,
					get_tag (string->str, "link", opening, closing));
			} else if (g_ascii_strncasecmp (tag, "body", 4) == 0) {
				valid = TRUE;
				break;
			}

		} while (pos);

		/* Something's wrong, let's write the entire HTML and hope
		 * that WebKit can handle it */
		if (!valid) {
			EMailFormatterContext c = {
				.part_list = context->part_list,
				.flags = context->flags,
				.mode = E_MAIL_FORMATTER_MODE_RAW,
			};

			emfe_text_html_format (
				extension, formatter, &c, part, stream, cancellable);
			return FALSE;
		}

		/*	       include the "body" as well -----v */
		g_string_erase (string, 0, tag - string->str + 4);
		g_string_prepend (string, "<div ");

		for (iter = tags; iter; iter = iter->next) {
			if (iter->data)
				g_string_prepend (string, iter->data);
		}

		g_list_free_full (tags, g_free);

		document_end = NULL;
		/* We can probably use ASCII functions here */
		if (g_strrstr (string->str, "</body>")) {
			document_end = ">ydob/<";
		}

		if (g_strrstr (string->str, "</html>")) {
			if (document_end) {
				document_end = ">lmth/<>ydob/<";
			} else {
				document_end = ">lmth/<";
			}
		}

		if (document_end) {
			length = strlen (document_end);
			tag = string->str + string->len - 1;
			i = 0;
			valid = FALSE;
			while (i < length - 1) {
				gunichar c;

				c = g_utf8_get_char (tag);
				if (g_unichar_isspace (c)) {
					tag = g_utf8_find_prev_char (string->str, tag);
					continue;
				}

				c = g_unichar_tolower (c);

				if (c == document_end[i]) {
					tag = g_utf8_find_prev_char (string->str, tag);
					i++;
					valid = TRUE;
					continue;
				}

				tag = g_utf8_find_prev_char (string->str, tag);
				valid = FALSE;
			}
		}

		if (valid)
			g_string_truncate (string, tag - string->str);

		camel_stream_write_string (stream, string->str, cancellable, NULL);

		g_string_free (string, TRUE);
	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		gchar *uri, *str;

		folder = context->part_list->folder;
		message_uid = context->part_list->message_uid;

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, part->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			NULL);

		str = g_strdup_printf (
			"<div class=\"part-container-nostyle\">"
			"<iframe width=\"100%%\" height=\"10\" "
			" frameborder=\"0\" src=\"%s\" "
			" id=\"%s.iframe\" name=\"%s\" "
			" style=\"border: 1px solid #%06x; background-color: #%06x;\">"
			"</iframe>"
			"</div>",
			uri,
			part->id,
			part->id,
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_color_to_value ((GdkColor *)
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_CONTENT)));

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

	return TRUE;
}

static const gchar *
emfe_text_html_get_display_name (EMailFormatterExtension *extension)
{
	return _("HTML");
}

static const gchar *
emfe_text_html_get_description (EMailFormatterExtension *extension)
{
	return _("Format part as HTML");
}

static const gchar **
emfe_text_html_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_text_html_class_init (EMailFormatterTextHTMLClass *class)
{
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_text_html_format;
	iface->get_display_name = emfe_text_html_get_display_name;
	iface->get_description = emfe_text_html_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_text_html_mime_types;
}

static void
e_mail_formatter_text_html_init (EMailFormatterTextHTML *formatter)
{

}

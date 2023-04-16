/*
 * e-mail-formatter-text-html.c
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

#include <ctype.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"
#include "e-mail-part-utils.h"

typedef EMailFormatterExtension EMailFormatterTextHTML;
typedef EMailFormatterExtensionClass EMailFormatterTextHTMLClass;

GType e_mail_formatter_text_html_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterTextHTML,
	e_mail_formatter_text_html,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/html",
	NULL
};

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
	while (t && t > opening) {

		c = g_utf8_get_char (t);
		if (!g_unichar_isspace (c))
			break;

		t = g_utf8_find_prev_char (utf8_string, t);
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
                       GOutputStream *stream,
                       GCancellable *cancellable)
{
	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		e_mail_formatter_format_text (
			formatter, part, stream, cancellable);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		GOutputStream *decoded_stream;
		GString *string = NULL;
		gchar *pos;
		GList *tags, *iter;
		gboolean valid;
		gchar *tag;
		const gchar *document_end;
		gpointer data;
		gsize length;
		gint i;

		decoded_stream = g_memory_output_stream_new_resizable ();

		/* FORMATTER FIXME: See above */
		e_mail_formatter_format_text (
			formatter, part, decoded_stream, cancellable);

		data = g_memory_output_stream_get_data (
			G_MEMORY_OUTPUT_STREAM (decoded_stream));
		length = g_memory_output_stream_get_data_size (
			G_MEMORY_OUTPUT_STREAM (decoded_stream));

		if (length > 2 && data) {
			gunichar2 *maybe_utf16 = data;

			if (*maybe_utf16 == (gunichar2) 0xFFFE) {
				gunichar2 *ptr;

				for (ptr = maybe_utf16, i = 0; i < length / 2; i++, ptr++) {
					*ptr = GUINT16_SWAP_LE_BE (*ptr);
				}
			}

			if (*maybe_utf16 == (gunichar2) 0xFEFF) {
				gchar *utf8;

				maybe_utf16++;
				utf8 = g_utf16_to_utf8 (maybe_utf16, length / 2, NULL, NULL, NULL);

				if (utf8 && *utf8)
					string = g_string_new (utf8);

				g_free (utf8);
			}
		}

		if (!string)
			string = g_string_new_len ((gchar *) data, length);

		g_object_unref (decoded_stream);

		if (!g_utf8_validate (string->str, -1, NULL)) {
			gchar *valid_utf8;

			valid_utf8 = e_util_utf8_make_valid (string->str);
			g_string_assign (string, valid_utf8);
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
			while (tag = g_utf8_find_next_char (pos, NULL), tag != NULL) {
				gunichar c = g_utf8_get_char (tag);
				if (!g_unichar_isspace (c))
					break;
				pos = tag;
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
				extension, formatter, &c,
				part, stream, cancellable);
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
		} else {
			/* do not cut, if there is no end tag */
			valid = FALSE;
		}

		if (valid)
			g_string_truncate (string, tag - string->str);

		g_output_stream_write_all (
			stream, string->str, string->len,
			NULL, cancellable, NULL);

		g_string_free (string, TRUE);
	} else {
		CamelFolder *folder;
		GSettings *settings;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *uri, *str;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			NULL);

		/* HTML messages expect white background and black color for text.
		 * If Evolution uses a dark theme, then the dark background with
		 * a black text is hard to read, thus force white background color.
		 * The HTML content can still overwrite both colors.
		*/
		str = g_strdup_printf (
			"<div class=\"part-container-nostyle\">"
			"<iframe width=\"100%%\" height=\"10\" "
			" frameborder=\"0\" src=\"%s\" "
			" id=\"%s.iframe\" name=\"%s\" "
			" class=\"-e-mail-formatter-frame-color %s\" "
			" %s>"
			"</iframe>"
			"</div>",
			uri,
			e_mail_part_get_id (part),
			e_mail_part_get_id (part),
			e_mail_part_get_frame_security_style (part),
			g_settings_get_boolean (settings, "preview-unset-html-colors") ? "x-e-unset-colors=\"1\"" : "style=\"background-color: #fff; color-scheme: light\"");

		g_output_stream_write_all (
			stream, str, strlen (str),
			NULL, cancellable, NULL);

		g_clear_object (&settings);
		g_free (str);
		g_free (uri);
	}

	return TRUE;
}

static void
e_mail_formatter_text_html_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("HTML");
	class->description = _("Format part as HTML");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_text_html_format;
}

static void
e_mail_formatter_text_html_init (EMailFormatterExtension *extension)
{
}

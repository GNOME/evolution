/*
 * text-highlight.c
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

#include "e-mail-formatter-text-highlight.h"
#include "languages.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n-lib.h>
#include <X11/Xlib.h>
#include <camel/camel.h>

typedef EMailFormatterExtension EMailFormatterTextHighlight;
typedef EMailFormatterExtensionClass EMailFormatterTextHighlightClass;

typedef EExtension EMailFormatterTextHighlightLoader;
typedef EExtensionClass EMailFormatterTextHighlightLoaderClass;

GType e_mail_formatter_text_highlight_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterTextHighlight,
	e_mail_formatter_text_highlight,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static gchar *
get_syntax (EMailPart *part,
            const gchar *uri)
{
	gchar *syntax = NULL;
	CamelContentType *ct = NULL;

	if (uri) {
		SoupURI *soup_uri = soup_uri_new (uri);
		GHashTable *query = soup_form_decode (soup_uri->query);

		syntax = g_hash_table_lookup (query, "__formatas");
		if (syntax) {
			syntax = g_strdup (syntax);
		}
		g_hash_table_destroy (query);
		soup_uri_free (soup_uri);
	}

	/* Try to detect syntax by content-type first */
	if (syntax == NULL) {
		ct = camel_mime_part_get_content_type (part->part);
		if (ct) {
			gchar *mime_type = camel_content_type_simple (ct);

			syntax = (gchar *) get_syntax_for_mime_type (mime_type);
			syntax = syntax ? g_strdup (syntax) : NULL;
			g_free (mime_type);
		}
	}

	/* If it fails or the content type too generic, try to detect it by
	 * filename extension */
	if (syntax == NULL ||
	    (ct != NULL &&
	     (camel_content_type_is (ct, "application", "octet-stream") ||
	     (camel_content_type_is (ct, "text", "plain"))))) {
		const gchar *filename;

		filename = camel_mime_part_get_filename (part->part);
		if (filename != NULL) {
			gchar *ext = g_strrstr (filename, ".");
			if (ext != NULL) {
				syntax = (gchar *) get_syntax_for_ext (ext + 1);
				syntax = syntax ? g_strdup (syntax) : NULL;
			}
		}
	}

	/* Out of ideas - use plain text */
	if (syntax == NULL) {
		syntax = g_strdup ("txt");
	}

	return syntax;
}

static gboolean
emfe_text_highlight_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            CamelStream *stream,
                            GCancellable *cancellable)
{
	CamelContentType *ct;
	gboolean success = FALSE;

	ct = camel_mime_part_get_content_type (part->part);

	/* Don't format text/html unless it's an attachment */
	if (ct && camel_content_type_is (ct, "text", "html")) {
		const CamelContentDisposition *disp;

		disp = camel_mime_part_get_content_disposition (part->part);

		if (disp == NULL)
			goto exit;

		if (g_strcmp0 (disp->disposition, "attachment") != 0)
			goto exit;
	}

	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		CamelDataWrapper *dw;
		CamelStream *filter_stream;
		CamelMimeFilter *mime_filter;

		dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
		if (dw == NULL)
			goto exit;

		camel_stream_write_string (
			stream, "<pre><div class=\"pre\">", cancellable, NULL);

		filter_stream = camel_stream_filter_new (stream);
		mime_filter = camel_mime_filter_tohtml_new (
			CAMEL_MIME_FILTER_TOHTML_PRE |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES, 0);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filter_stream), mime_filter);
		g_object_unref (mime_filter);

		e_mail_formatter_format_text (
			formatter, part, filter_stream, cancellable);

		camel_stream_flush (filter_stream, cancellable, NULL);
		g_object_unref (filter_stream);

		camel_stream_write_string (
			stream, "</div></pre>", cancellable, NULL);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		gint pipe_stdin, pipe_stdout;
		GPid pid;
		CamelDataWrapper *dw;
		gchar *font_family, *font_size, *syntax;
		PangoFontDescription *fd;
		GSettings *settings;
		gchar *font = NULL;

		const gchar *argv[] = {
			HIGHLIGHT_COMMAND,
			NULL,	/* --font= */
			NULL,   /* --font-size= */
			NULL,   /* --syntax= */
			"--out-format=html",
			"--include-style",
			"--inline-css",
			"--style=bclear",
			"--failsafe",
			NULL };

		dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
		if (dw == NULL)
			goto exit;

		syntax = get_syntax (part, context->uri);

		/* Use the traditional text/plain formatter for plain-text */
		if (g_strcmp0 (syntax, "txt") == 0) {
			g_free (syntax);
			goto exit;
		}

		settings = g_settings_new ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "use-custom-font"))
			font = g_settings_get_string (
				settings, "monospace-font");
		g_object_unref (settings);

		if (font == NULL) {
			settings = g_settings_new (
				"org.gnome.desktop.interface");
			font = g_settings_get_string (
				settings, "monospace-font-name");
			g_object_unref (settings);
		}

		if (font == NULL)
			font = g_strdup ("monospace 10");

		fd = pango_font_description_from_string (font);

		g_free (font);

		font_family = g_strdup_printf (
			"--font='%s'",
			pango_font_description_get_family (fd));
		font_size = g_strdup_printf (
			"--font-size=%d",
			pango_font_description_get_size (fd) / PANGO_SCALE);

		argv[1] = font_family;
		argv[2] = font_size;
		argv[3] = g_strdup_printf ("--syntax=%s", syntax);
		g_free (syntax);

		success = g_spawn_async_with_pipes (
			NULL, (gchar **) argv, NULL, 0, NULL, NULL,
			&pid, &pipe_stdin, &pipe_stdout, NULL, NULL);

		if (success) {
			CamelStream *read;
			CamelStream *write;
			CamelStream *utf8;
			GByteArray *ba;
			gchar *tmp;

			write = camel_stream_fs_new_with_fd (pipe_stdin);
			read = camel_stream_fs_new_with_fd (pipe_stdout);

			/* Decode the content of mime part to the 'utf8' stream */
			utf8 = camel_stream_mem_new ();
			camel_data_wrapper_decode_to_stream_sync (
				dw, utf8, cancellable, NULL);

			/* Convert the binary data do someting displayable */
			ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (utf8));
			tmp = e_util_utf8_data_make_valid ((gchar *) ba->data, ba->len);

			/* Send the sanitized data to the highlighter */
			camel_stream_write_string (write, tmp, cancellable, NULL);
			g_free (tmp);
			g_object_unref (utf8);
			g_object_unref (write);

			g_spawn_close_pid (pid);

			g_seekable_seek (G_SEEKABLE (read), 0, G_SEEK_SET, cancellable, NULL);
			camel_stream_write_to_stream (read, stream, cancellable, NULL);
			g_object_unref (read);
		} else {
			/* We can't call e_mail_formatter_format_as on text/plain,
			 * because text-highlight is registered as an handler for
			 * text/plain, so we would end up in an endless recursion.
			 *
			 * Just return FALSE here and EMailFormatter will automatically
			 * fall back to the default text/plain formatter */
			if (camel_content_type_is (ct, "text", "plain")) {
				g_free (font_family);
				g_free (font_size);
				g_free ((gchar *) argv[3]);
				pango_font_description_free (fd);
				goto exit;

			} else {
				/* In case of any other content, force use of
				 * text/plain formatter, because returning FALSE
				 * for text/x-patch or application/php would show
				 * an error, as there is no other handler registered
				 * for these */
				e_mail_formatter_format_as (
					formatter, context, part, stream,
					"application/vnd.evolution.plaintext",
					cancellable);
			}
		}

		g_free (font_family);
		g_free (font_size);
		g_free ((gchar *) argv[3]);
		pango_font_description_free (fd);

	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *uri, *str;
		gchar *syntax;

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		syntax = get_syntax (part, NULL);

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"__formatas", G_TYPE_STRING, syntax,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			NULL);

		g_free (syntax);

		str = g_strdup_printf (
			"<div class=\"part-container-nostyle\" >"
			"<iframe width=\"100%%\" height=\"10\""
			" id=\"%s\" name=\"%s\" "
			" frameborder=\"0\" src=\"%s\" "
			" style=\"border: 1px solid #%06x; background-color: #%06x;\">"
			"</iframe>"
			"</div>",
			e_mail_part_get_id (part),
			e_mail_part_get_id (part),
			uri,
			e_rgba_to_value (
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
			e_rgba_to_value (
				e_mail_formatter_get_color (
					formatter, E_MAIL_FORMATTER_COLOR_CONTENT)));

		camel_stream_write_string (stream, str, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

	success = TRUE;

exit:
	return success;
}

static void
e_mail_formatter_text_highlight_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Text Highlight");
	class->description = _("Syntax highlighting of mail parts");
	class->mime_types = get_mime_types ();
	class->format = emfe_text_highlight_format;
}

static void
e_mail_formatter_text_highlight_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_text_highlight_init (EMailFormatterExtension *extension)
{
}

void
e_mail_formatter_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_formatter_text_highlight_register_type (type_module);
}


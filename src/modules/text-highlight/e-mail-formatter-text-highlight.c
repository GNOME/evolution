/*
 * text-highlight.c
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

#include "e-mail-formatter-text-highlight.h"
#include "languages.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef EMailFormatterExtension EMailFormatterTextHighlight;
typedef EMailFormatterExtensionClass EMailFormatterTextHighlightClass;

typedef EExtension EMailFormatterTextHighlightLoader;
typedef EExtensionClass EMailFormatterTextHighlightLoaderClass;

typedef struct _TextHighlightClosure TextHighlightClosure;

struct _TextHighlightClosure {
	gboolean wrote_anything;
	CamelStream *read_stream;
	GOutputStream *output_stream;
	GCancellable *cancellable;
	GError *error;
};

GType e_mail_formatter_text_highlight_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterTextHighlight,
	e_mail_formatter_text_highlight,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static gboolean
emfe_text_highlight_formatter_is_enabled (void)
{
	GSettings *settings;
	gboolean enabled;

	settings = e_util_ref_settings ("org.gnome.evolution.text-highlight");
	enabled = g_settings_get_boolean (settings, "enabled");
	g_object_unref (settings);

	return enabled;
}

static gchar *
get_syntax (EMailPart *part,
            const gchar *uri)
{
	gchar *syntax = NULL;
	CamelContentType *ct = NULL;
	CamelMimePart *mime_part;

	mime_part = e_mail_part_ref_mime_part (part);

	if (uri) {
		GUri *guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		GHashTable *query = soup_form_decode (g_uri_get_query (guri));

		syntax = g_hash_table_lookup (query, "__formatas");
		if (syntax) {
			syntax = g_strdup (syntax);
		}
		g_hash_table_destroy (query);
		g_uri_unref (guri);
	}

	/* Try to detect syntax by content-type first */
	if (syntax == NULL) {
		ct = camel_mime_part_get_content_type (mime_part);
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

		filename = camel_mime_part_get_filename (mime_part);
		if (filename != NULL) {
			gchar *ext = g_strrstr (filename, ".");
			if (ext != NULL) {
				g_free (syntax);
				syntax = (gchar *) get_syntax_for_ext (ext + 1);
				syntax = syntax ? g_strdup (syntax) : NULL;
			}
		}
	}

	/* Out of ideas - use plain text */
	if (syntax == NULL) {
		syntax = g_strdup ("txt");
	}

	g_object_unref (mime_part);

	return syntax;
}

static gpointer
text_hightlight_read_data_thread (gpointer user_data)
{
	TextHighlightClosure *closure = user_data;
	gint nbuffer = 10240;
	gssize read;
	gsize wrote = 0;
	gchar *buffer;

	g_return_val_if_fail (closure != NULL, NULL);

	buffer = g_new (gchar, nbuffer);

	strcpy (buffer, "<style>body{margin:0; padding:8px;}</style>");
	read = strlen (buffer);

	if (!g_output_stream_write_all (closure->output_stream, buffer, read, &wrote, closure->cancellable, &closure->error) ||
	    (gssize) wrote != read || closure->error) {
		g_free (buffer);
		return NULL;
	}

	while (!camel_stream_eos (closure->read_stream) &&
	       !g_cancellable_set_error_if_cancelled (closure->cancellable, &closure->error)) {
		wrote = 0;

		read = camel_stream_read (closure->read_stream, buffer, nbuffer, closure->cancellable, &closure->error);
		if (read < 0 || closure->error)
			break;

		closure->wrote_anything = closure->wrote_anything || read > 0;

		if (!g_output_stream_write_all (closure->output_stream, buffer, read, &wrote, closure->cancellable, &closure->error) ||
		    (gssize) wrote != read || closure->error)
			break;
	}

	g_free (buffer);

	return NULL;
}

static gboolean
text_highlight_feed_data (GOutputStream *output_stream,
                          CamelDataWrapper *data_wrapper,
                          gint pipe_stdin,
                          gint pipe_stdout,
                          GCancellable *cancellable,
                          GError **error)
{
	TextHighlightClosure closure;
	CamelContentType *content_type;
	CamelStream *write_stream;
	gboolean success = TRUE;
	GThread *thread;

	closure.wrote_anything = FALSE;
	closure.read_stream = camel_stream_fs_new_with_fd (pipe_stdout);
	closure.output_stream = output_stream;
	closure.cancellable = cancellable;
	closure.error = NULL;

	write_stream = camel_stream_fs_new_with_fd (pipe_stdin);

	thread = g_thread_new (NULL, text_hightlight_read_data_thread, &closure);

	content_type = camel_data_wrapper_get_mime_type_field (data_wrapper);
	if (content_type) {
		const gchar *charset = camel_content_type_param (content_type, "charset");

		/* Convert to UTF-8 charset, if needed, which the 'highlight' expects;
		   it can cope with non-UTF-8 letters, thus no need for a content UTF-8-validation */
		if (charset && g_ascii_strcasecmp (charset, "utf-8") != 0) {
			CamelMimeFilter *filter;

			filter = camel_mime_filter_charset_new (charset, "UTF-8");
			if (filter != NULL) {
				CamelStream *filtered = camel_stream_filter_new (write_stream);

				if (filtered) {
					camel_stream_filter_add (CAMEL_STREAM_FILTER (filtered), filter);
					g_object_unref (write_stream);
					write_stream = filtered;
				}

				g_object_unref (filter);
			}
		}
	}

	if (camel_data_wrapper_decode_to_stream_sync (data_wrapper, write_stream, cancellable, error) < 0) {
		g_cancellable_cancel (cancellable);
		success = FALSE;
	} else {
		/* Close the stream, thus the highlight knows no more data will come */
		g_clear_object (&write_stream);
	}

	g_thread_join (thread);

	g_clear_object (&closure.read_stream);
	g_clear_object (&write_stream);

	if (closure.error) {
		if (error && !*error)
			g_propagate_error (error, closure.error);
		else
			g_clear_error (&closure.error);

		return FALSE;
	}

	return success && closure.wrote_anything;
}

static gboolean
emfe_text_highlight_format (EMailFormatterExtension *extension,
                            EMailFormatter *formatter,
                            EMailFormatterContext *context,
                            EMailPart *part,
                            GOutputStream *stream,
                            GCancellable *cancellable)
{
	CamelMimePart *mime_part;
	CamelContentType *ct;
	gboolean success = FALSE;

	mime_part = e_mail_part_ref_mime_part (part);
	ct = camel_mime_part_get_content_type (mime_part);

	/* Don't format text/html unless it's an attachment */
	if (ct && camel_content_type_is (ct, "text", "html")) {
		const CamelContentDisposition *disp;

		disp = camel_mime_part_get_content_disposition (mime_part);

		if (disp == NULL)
			goto exit;

		if (g_strcmp0 (disp->disposition, "attachment") != 0)
			goto exit;
	}

	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		/* Don't interfere with printing. */
		goto exit;

	} else if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		gint pipe_stdin, pipe_stdout;
		GPid pid;
		CamelDataWrapper *dw;
		gchar *font_family, *font_size, *syntax, *theme;
		PangoFontDescription *fd;
		GSettings *settings;
		gchar *font = NULL;

		const gchar *argv[] = {
			HIGHLIGHT_COMMAND,
			NULL,	/* --font= */
			NULL,   /* --font-size= */
			NULL,   /* --syntax= */
			NULL,   /* --style= */
			"--out-format=html",
			"--include-style",
			"--inline-css",
			"--encoding=none",
			"--force",
			NULL };

		if (!emfe_text_highlight_formatter_is_enabled ()) {
			gboolean can_process = FALSE;

			if (context->uri) {
				GUri *guri;

				guri = g_uri_parse (context->uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
				if (guri) {
					GHashTable *query;

					query = soup_form_decode (g_uri_get_query (guri));
					can_process = query && g_strcmp0 (g_hash_table_lookup (query, "__force_highlight"), "true") == 0;
					if (query)
						g_hash_table_destroy (query);
					g_uri_unref (guri);
				}
			}

			if (!can_process) {
				success = e_mail_formatter_format_as (
					formatter, context, part, stream,
					"application/vnd.evolution.plaintext",
					cancellable);
				goto exit;
			}
		}

		dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		if (dw == NULL)
			goto exit;

		syntax = get_syntax (part, context->uri);

		/* Use the traditional text/plain formatter for plain-text */
		if (g_strcmp0 (syntax, "txt") == 0) {
			g_free (syntax);
			goto exit;
		}

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "use-custom-font"))
			font = g_settings_get_string (
				settings, "monospace-font");
		g_object_unref (settings);

		if (font == NULL) {
			settings = e_util_ref_settings ("org.gnome.desktop.interface");
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

		settings = e_util_ref_settings ("org.gnome.evolution.text-highlight");
		theme = g_settings_get_string (settings, "theme");
		g_object_unref (settings);

		if (!theme || !*theme) {
			const GdkRGBA *rgba;
			gdouble brightness;
			gboolean is_dark_theme;

			g_free (theme);

			rgba = e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_TEXT);
			brightness =
				(0.2109 * 255.0 * rgba->red) +
				(0.5870 * 255.0 * rgba->green) +
				(0.1021 * 255.0 * rgba->blue);

			is_dark_theme = brightness > 140;

			theme = g_strdup (is_dark_theme ? "kellys" : "bclear");
		}

		argv[1] = font_family;
		argv[2] = font_size;
		argv[3] = g_strdup_printf ("--syntax=%s", syntax);
		argv[4] = g_strdup_printf ("--style=%s", theme);
		g_free (syntax);
		g_free (theme);

		success = g_spawn_async_with_pipes (
			NULL, (gchar **) argv, NULL, 0, NULL, NULL,
			&pid, &pipe_stdin, &pipe_stdout, NULL, NULL);

		if (success) {
			GError *local_error = NULL;

			success = text_highlight_feed_data (
				stream, dw,
				pipe_stdin, pipe_stdout,
				cancellable, &local_error);

			if (g_error_matches (
				local_error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED)) {
				/* Do nothing. */

			} else if (local_error != NULL) {
				g_warning (
					"%s: %s", G_STRFUNC,
					local_error->message);
			}

			g_clear_error (&local_error);

			g_spawn_close_pid (pid);
		}

		if (!success) {
			/* We can't call e_mail_formatter_format_as on text/plain,
			 * because text-highlight is registered as a handler for
			 * text/plain, so we would end up in an endless recursion.
			 *
			 * Just return FALSE here and EMailFormatter will automatically
			 * fall back to the default text/plain formatter */
			if (!camel_content_type_is (ct, "text", "plain")) {
				/* In case of any other content, force use of
				 * text/plain formatter, because returning FALSE
				 * for text/x-patch or application/php would show
				 * an error, as there is no other handler registered
				 * for these */
				success = e_mail_formatter_format_as (
					formatter, context, part, stream,
					"application/vnd.evolution.plaintext",
					cancellable);
			}
		}

		g_free (font_family);
		g_free (font_size);
		g_free ((gchar *) argv[3]);
		g_free ((gchar *) argv[4]);
		pango_font_description_free (fd);

		if (!success)
			goto exit;
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
			" class=\"-e-mail-formatter-frame-color %s -e-web-view-background-color\" "
			" frameborder=\"0\" src=\"%s\" "
			" >"
			"</iframe>"
			"</div>",
			e_mail_part_get_id (part),
			e_mail_part_get_id (part),
			e_mail_part_get_frame_security_style (part),
			uri);

		g_output_stream_write_all (
			stream, str, strlen (str),
			NULL, cancellable, NULL);

		g_free (str);
		g_free (uri);
	}

	success = TRUE;

exit:
	g_object_unref (mime_part);

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


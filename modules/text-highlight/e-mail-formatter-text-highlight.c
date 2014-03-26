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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-formatter-text-highlight.h"
#include "languages.h"

/* FIXME Delete these once we can use GSubprocess. */
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

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

typedef struct _TextHighlightClosure TextHighlightClosure;

struct _TextHighlightClosure {
	GMainLoop *main_loop;
	GError *input_error;
	GError *output_error;
};

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
	CamelMimePart *mime_part;

	mime_part = e_mail_part_ref_mime_part (part);

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

static void
text_highlight_input_spliced (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	TextHighlightClosure *closure = user_data;

	g_output_stream_splice_finish (
		G_OUTPUT_STREAM (source_object),
		result, &closure->input_error);
}

static void
text_highlight_output_spliced (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	TextHighlightClosure *closure = user_data;

	g_output_stream_splice_finish (
		G_OUTPUT_STREAM (source_object),
		result, &closure->output_error);

	g_main_loop_quit (closure->main_loop);
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
	GInputStream *input_stream = NULL;
	GOutputStream *temp_stream = NULL;
	GInputStream *stdout_stream = NULL;
	GOutputStream *stdin_stream = NULL;
	GMainContext *main_context;
	gchar *utf8_data;
	gconstpointer data;
	gsize size;
	gboolean success;

	/* We need to dump CamelDataWrapper to a buffer, force the content
	 * to valid UTF-8, feed the UTF-8 data to the 'highlight' process,
	 * read the converted data back and feed it to the CamelStream. */

	/* FIXME Use GSubprocess once we can require GLib 2.40. */

	temp_stream = g_memory_output_stream_new_resizable ();

	success = camel_data_wrapper_decode_to_output_stream_sync (
		data_wrapper, temp_stream, cancellable, error);

	if (!success)
		goto exit;

	main_context = g_main_context_new ();

	closure.main_loop = g_main_loop_new (main_context, FALSE);
	closure.input_error = NULL;
	closure.output_error = NULL;

	g_main_context_push_thread_default (main_context);

	data = g_memory_output_stream_get_data (
		G_MEMORY_OUTPUT_STREAM (temp_stream));
	size = g_memory_output_stream_get_data_size (
		G_MEMORY_OUTPUT_STREAM (temp_stream));

	/* FIXME Write a GConverter that does this so we can decode
	 *       straight to the stdin pipe and skip all this extra
	 *       buffering. */
	utf8_data = e_util_utf8_data_make_valid ((gchar *) data, size);

	g_clear_object (&temp_stream);

	/* Takes ownership of the UTF-8 string. */
	input_stream = g_memory_input_stream_new_from_data (
		utf8_data, -1, (GDestroyNotify) g_free);

	stdin_stream = g_unix_output_stream_new (pipe_stdin, TRUE);
	stdout_stream = g_unix_input_stream_new (pipe_stdout, TRUE);

	/* Splice the streams together. */

	/* GCancellable is only supposed to be used in one operation
	 * at a time.  Skip it here and use it for reading converted
	 * data, since that operation terminates the main loop. */
	g_output_stream_splice_async (
		stdin_stream, input_stream,
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
		G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, NULL,
		text_highlight_input_spliced,
		&closure);

	g_output_stream_splice_async (
		output_stream, stdout_stream,
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
		G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, cancellable,
		text_highlight_output_spliced,
		&closure);

	g_main_loop_run (closure.main_loop);

	g_main_context_pop_thread_default (main_context);

	g_main_context_unref (main_context);
	g_main_loop_unref (closure.main_loop);

	g_clear_object (&input_stream);
	g_clear_object (&stdin_stream);
	g_clear_object (&stdout_stream);

	if (closure.input_error != NULL) {
		g_propagate_error (error, closure.input_error);
		g_clear_error (&closure.output_error);
		success = FALSE;
		goto exit;
	}

	if (closure.output_error != NULL) {
		g_propagate_error (error, closure.output_error);
		success = FALSE;
		goto exit;
	}

exit:
	g_clear_object (&temp_stream);

	return success;
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

		dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
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
			" class=\"-e-mail-formatter-frame-color -e-web-view-background-color\" "
			" frameborder=\"0\" src=\"%s\" "
			" style=\"border: 1px solid;\">"
			"</iframe>"
			"</div>",
			e_mail_part_get_id (part),
			e_mail_part_get_id (part),
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


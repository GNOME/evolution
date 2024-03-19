/*
 * evolution-source.c
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

#include <e-util/e-util.h>

#include "e-mail-formatter-extension.h"
#include "e-mail-inline-filter.h"

typedef EMailFormatterExtension EMailFormatterSource;
typedef EMailFormatterExtensionClass EMailFormatterSourceClass;

GType e_mail_formatter_source_get_type (void);

G_DEFINE_TYPE (
	EMailFormatterSource,
	e_mail_formatter_source,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"application/vnd.evolution.source",
	NULL
};

static gboolean
emfe_source_format (EMailFormatterExtension *extension,
                    EMailFormatter *formatter,
                    EMailFormatterContext *context,
                    EMailPart *part,
                    GOutputStream *stream,
                    GCancellable *cancellable)
{
	GString *buffer;
	GOutputStream *filtered_stream;
	CamelMimeFilter *filter;
	CamelMimePart *mime_part;
	gboolean did_read_file = FALSE;

	mime_part = e_mail_part_ref_mime_part (part);

	buffer = g_string_new ("");

	if (CAMEL_IS_MIME_MESSAGE (mime_part)) {
		g_string_append (
			buffer,
			"<div class=\"part-container "
			"-e-mail-formatter-body-color "
			"-e-web-view-text-color\" "
			"style=\"border: 0;\" >");
	} else {
		g_string_append (
			buffer,
			"<div class=\"part-container "
			"-e-mail-formatter-body-color "
			"-e-web-view-text-color "
			"-e-mail-formatter-frame-color\">"
			"<div class=\"part-container-inner-margin pre\">\n");
	}

	g_string_append (buffer, "<code class=\"pre\">");

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	filter = camel_mime_filter_tohtml_new (
		CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
		CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
	filtered_stream = camel_filter_output_stream_new (stream, filter);
	g_filter_output_stream_set_close_base_stream (
		G_FILTER_OUTPUT_STREAM (filtered_stream), FALSE);
	g_object_unref (filter);

	if (g_strcmp0 (e_mail_part_get_id (part), ".message") == 0 &&
	    CAMEL_IS_MIME_MESSAGE (mime_part)) {
		EMailPartList *part_list;

		part_list = e_mail_part_ref_part_list (part);
		if (part_list) {
			CamelFolder *folder;
			const gchar *uid;

			folder = e_mail_part_list_get_folder (part_list);
			uid = e_mail_part_list_get_message_uid (part_list);

			if (folder && uid) {
				gchar *filename;

				filename = camel_folder_get_filename (folder, uid, NULL);
				if (filename) {
					GFile *file;

					file = g_file_new_for_path (filename);
					if (file) {
						GFileInputStream *file_input_stream;

						file_input_stream = g_file_read (file, cancellable, NULL);
						if (file_input_stream) {
							did_read_file = TRUE;
							g_output_stream_splice (filtered_stream, G_INPUT_STREAM (file_input_stream),
								G_OUTPUT_STREAM_SPLICE_NONE, cancellable, NULL);
						}

						g_clear_object (&file_input_stream);
						g_clear_object (&file);
					}

					g_free (filename);
				}
			}
		}

		g_clear_object (&part_list);
	}

	if (!did_read_file && !g_cancellable_is_cancelled (cancellable)) {
		camel_data_wrapper_write_to_output_stream_sync (
			CAMEL_DATA_WRAPPER (mime_part),
			filtered_stream, cancellable, NULL);
	}

	g_output_stream_flush (filtered_stream, cancellable, NULL);

	g_object_unref (filtered_stream);

	/* Resets the string buffer. */
	g_string_assign (buffer, "</code>");

	if (CAMEL_IS_MIME_MESSAGE (mime_part))
		g_string_append (buffer, "</div>");
	else
		g_string_append (buffer, "</div></div>");

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	g_string_free (buffer, TRUE);

	g_object_unref (mime_part);

	return TRUE;
}

static void
e_mail_formatter_source_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Source");
	class->description = _("Display source of a MIME part");
	class->mime_types = formatter_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->format = emfe_source_format;
}

static void
e_mail_formatter_source_init (EMailFormatterExtension *extension)
{
}

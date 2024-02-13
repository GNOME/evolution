/*
 * e-file-request.c
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

#include <stdio.h>
#include <string.h>

#include <libsoup/soup.h>

#include "e-file-request.h"

#define d(x)

struct _EFileRequestPrivate {
	gint dummy;
};

static void e_file_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EFileRequest, e_file_request, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EFileRequest)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_file_request_content_request_init))

static gboolean
e_file_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_FILE_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "evo-file:", 9) == 0;
}

static gboolean
e_file_request_process_sync (EContentRequest *request,
			     const gchar *uri,
			     GObject *requester,
			     GInputStream **out_stream,
			     gint64 *out_stream_length,
			     gchar **out_mime_type,
			     GCancellable *cancellable,
			     GError **error)
{
	GFile *file;
	GFileInputStream *file_input_stream;
	GFileInfo *info;
	goffset total_size;
	gchar *filename = NULL, *path;
	GUri *guri;

	g_return_val_if_fail (E_IS_FILE_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (guri != NULL, FALSE);

	path = g_uri_unescape_string (g_uri_get_path (guri) ? g_uri_get_path (guri) : "", "/");

	if (g_strcmp0 (g_uri_get_host (guri), "$EVOLUTION_WEBKITDATADIR") == 0) {
		filename = g_build_filename (EVOLUTION_WEBKITDATADIR, path, NULL);
	} else if (g_strcmp0 (g_uri_get_host (guri), "$EVOLUTION_IMAGESDIR") == 0) {
		filename = g_build_filename (EVOLUTION_IMAGESDIR, path, NULL);
	}

	file = g_file_new_for_path (filename ? filename : path);
	file_input_stream = g_file_read (file, cancellable, error);

	if (file_input_stream) {
		total_size = -1;
		info = g_file_input_stream_query_info (file_input_stream, G_FILE_ATTRIBUTE_STANDARD_SIZE, cancellable, NULL);
		if (info) {
			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
				total_size = g_file_info_get_size (info);
			g_object_unref (info);
		}

		if (total_size == -1) {
			info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, cancellable, NULL);
			if (info) {
				if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
					total_size = g_file_info_get_size (info);
				g_object_unref (info);
			}
		}
	} else {
		total_size = -1;
	}

	if (file_input_stream) {
		*out_stream = G_INPUT_STREAM (file_input_stream);
		*out_stream_length = (gint64) total_size;
		*out_mime_type = g_content_type_guess (filename ? filename : path, NULL, 0, NULL);
	} else {
		*out_stream = NULL;
		*out_stream_length = (gint64) total_size;
		*out_mime_type = NULL;
	}

	g_object_unref (file);
	g_uri_unref (guri);
	g_free (filename);
	g_free (path);

	return file_input_stream != NULL;
}

static void
e_file_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_file_request_can_process_uri;
	iface->process_sync = e_file_request_process_sync;
}

static void
e_file_request_class_init (EFileRequestClass *class)
{
}

static void
e_file_request_init (EFileRequest *request)
{
	request->priv = e_file_request_get_instance_private (request);
}

EContentRequest *
e_file_request_new (void)
{
	return g_object_new (E_TYPE_FILE_REQUEST, NULL);
}

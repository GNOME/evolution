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

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include "e-file-request.h"

#include <libsoup/soup.h>

#include <string.h>

#define d(x)

#define E_FILE_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_FILE_REQUEST, EFileRequestPrivate))

struct _EFileRequestPrivate {
	gchar *content_type;
	gint content_length;
};

G_DEFINE_TYPE (EFileRequest, e_file_request, SOUP_TYPE_REQUEST)

static void
handle_file_request (GSimpleAsyncResult *res,
                     GObject *object,
                     GCancellable *cancellable)
{
	EFileRequest *request = E_FILE_REQUEST (object);
	SoupURI *uri;
	GInputStream *stream;
	gchar *contents;
	gsize length;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	uri = soup_request_get_uri (SOUP_REQUEST (request));

	if (g_file_get_contents (uri->path, &contents, &length, NULL)) {

		request->priv->content_type =
			g_content_type_guess (uri->path, NULL, 0, NULL);
		request->priv->content_length = length;

		stream = g_memory_input_stream_new_from_data (
				contents, length, (GDestroyNotify) g_free);
		g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
	}
}

static void
file_request_finalize (GObject *object)
{
	EFileRequest *request = E_FILE_REQUEST (object);

	if (request->priv->content_type) {
		g_free (request->priv->content_type);
		request->priv->content_type = NULL;
	}

	G_OBJECT_CLASS (e_file_request_parent_class)->finalize (object);
}

static gboolean
file_request_check_uri (SoupRequest *request,
                       SoupURI *uri,
                       GError **error)
{
	return (strcmp (uri->scheme, "evo-file") == 0);
}

static void
file_request_send_async (SoupRequest *request,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;

	d (printf ("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	/* WebKit won't allow us to load data through local file:// protocol
	 * when using "remote" mail:// protocol, so we have evo-file://
	 * which WebKit thinks it's remote, but in fact it behaves like
	 * oridnary file:// */

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback, user_data,
		file_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, handle_file_request,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

static GInputStream *
file_request_send_finish (SoupRequest *request,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	GInputStream *stream;

	simple = G_SIMPLE_ASYNC_RESULT (result);
	stream = g_simple_async_result_get_op_res_gpointer (simple);

	/* Reset the stream before passing it back to webkit */
	if (stream && G_IS_SEEKABLE (stream))
		g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, NULL);

	if (!stream) /* We must always return something */
		stream = g_memory_input_stream_new ();
	else
		g_object_ref (stream);

	return stream;
}

static goffset
file_request_get_content_length (SoupRequest *request)
{
	EFileRequest *efr = E_FILE_REQUEST (request);

	d (printf ("Content-Length: %d bytes\n", efr->priv->content_length));

	return efr->priv->content_length;
}

static const gchar *
file_request_get_content_type (SoupRequest *request)
{
	EFileRequest *efr = E_FILE_REQUEST (request);

	d (printf ("Content-Type: %s\n", efr->priv->content_type));

	return efr->priv->content_type;
}

static const gchar *data_schemes[] = { "evo-file", NULL };

static void
e_file_request_class_init (EFileRequestClass *class)
{
	GObjectClass *object_class;
	SoupRequestClass *request_class;

	g_type_class_add_private (class, sizeof (EFileRequestPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = file_request_finalize;

	request_class = SOUP_REQUEST_CLASS (class);
	request_class->schemes = data_schemes;
	request_class->send_async = file_request_send_async;
	request_class->send_finish = file_request_send_finish;
	request_class->get_content_type = file_request_get_content_type;
	request_class->get_content_length = file_request_get_content_length;
	request_class->check_uri = file_request_check_uri;
}

static void
e_file_request_init (EFileRequest *request)
{
	request->priv = E_FILE_REQUEST_GET_PRIVATE (request);
}


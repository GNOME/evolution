/*
 * e-stock-request.c
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

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include "e-stock-request.h"

#include <stdlib.h>
#include <libsoup/soup.h>

#include <e-util/e-util.h>

#include <string.h>

#define d(x)

#define E_STOCK_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_STOCK_REQUEST, EStockRequestPrivate))

struct _EStockRequestPrivate {
	gchar *content_type;
	gint content_length;
};

G_DEFINE_TYPE (EStockRequest, e_stock_request, SOUP_TYPE_REQUEST)

static void
handle_stock_request (GSimpleAsyncResult *res,
                      GObject *object,
                      GCancellable *cancellable)
{
	SoupURI *uri;
	GHashTable *query;
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;
	const gchar *file;
	gchar *a_size;
	gssize size;
	gchar *buffer;
	gsize buff_len;
	GtkStyleContext *context;
	GtkWidgetPath *path;
	GtkIconSet *set;
	EStockRequest *request;

	request = E_STOCK_REQUEST (object);
	uri = soup_request_get_uri (SOUP_REQUEST (object));

	query = NULL;
	if (uri->query)
		query = soup_form_decode (uri->query);

	if (query) {
		a_size = g_hash_table_lookup (query, "size");
		if (a_size) {
			size = atoi (a_size);
		} else {
			size = GTK_ICON_SIZE_BUTTON;
		}
		g_hash_table_destroy (query);
	} else {
		size = GTK_ICON_SIZE_BUTTON;
	}

	/* Try style context first */
	context = gtk_style_context_new ();
	path = gtk_widget_path_new ();
	gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
	gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
	gtk_style_context_set_path (context, path);

	set = gtk_style_context_lookup_icon_set (context, uri->host);
	if (!set) {
		/* Fallback to icon theme */
		icon_theme = gtk_icon_theme_get_default ();
		icon_info = gtk_icon_theme_lookup_icon (
				icon_theme, uri->host, size,
				GTK_ICON_LOOKUP_USE_BUILTIN);
		if (!icon_info) {
			gtk_widget_path_free (path);
			g_object_unref (context);
			return;
		}

		file = gtk_icon_info_get_filename (icon_info);
		buffer = NULL;
		if (file) {
			if (g_file_get_contents (file, &buffer, &buff_len, NULL)) {

				request->priv->content_type =
					g_content_type_guess (file, NULL, 0, NULL);
				request->priv->content_length = buff_len;
			}

		} else {
			GdkPixbuf *pixbuf;

			pixbuf = gtk_icon_info_get_builtin_pixbuf (icon_info);
			if (pixbuf) {
				gdk_pixbuf_save_to_buffer (
					pixbuf, &buffer,
					&buff_len, "png", NULL, NULL);

				request->priv->content_type = g_strdup ("image/png");
				request->priv->content_length = buff_len;

				g_object_unref (pixbuf);
			}
		}

		gtk_icon_info_free (icon_info);

	} else {
		GdkPixbuf *pixbuf;

		pixbuf = gtk_icon_set_render_icon_pixbuf (set, context, size);
				gdk_pixbuf_save_to_buffer (
					pixbuf, &buffer,
					&buff_len, "png", NULL, NULL);

		request->priv->content_type = g_strdup ("image/png");
		request->priv->content_length = buff_len;

		g_object_unref (pixbuf);
	}

	if (buffer) {
		GInputStream *stream;
		stream = g_memory_input_stream_new_from_data (
				buffer, buff_len, (GDestroyNotify) g_free);
		g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
	}

	gtk_widget_path_free (path);
	g_object_unref (context);

}
static void
stock_request_finalize (GObject *object)
{
	EStockRequest *request = E_STOCK_REQUEST (object);

	if (request->priv->content_type) {
		g_free (request->priv->content_type);
		request->priv->content_type = NULL;
	}

	G_OBJECT_CLASS (e_stock_request_parent_class)->finalize (object);
}

static gboolean
stock_request_check_uri (SoupRequest *request,
                       SoupURI *uri,
                       GError **error)
{
	return (strcmp (uri->scheme, "gtk-stock") == 0);
}

static void
stock_request_send_async (SoupRequest *request,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;

	d (printf ("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback, user_data,
		stock_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, handle_stock_request,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

static GInputStream *
stock_request_send_finish (SoupRequest *request,
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

	return stream;
}

static goffset
stock_request_get_content_length (SoupRequest *request)
{
	EStockRequest *esr = E_STOCK_REQUEST (request);

	d (printf ("Content-Length: %d bytes\n", esr->priv->content_length));
	return esr->priv->content_length;
}

static const gchar *
stock_request_get_content_type (SoupRequest *request)
{
	EStockRequest *esr = E_STOCK_REQUEST (request);

	d (printf ("Content-Type: %s\n", esr->priv->content_type));

	return esr->priv->content_type;
}

static const gchar *data_schemes[] = { "gtk-stock", NULL };

static void
e_stock_request_class_init (EStockRequestClass *class)
{
	GObjectClass *object_class;
	SoupRequestClass *request_class;

	g_type_class_add_private (class, sizeof (EStockRequestPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = stock_request_finalize;

	request_class = SOUP_REQUEST_CLASS (class);
	request_class->schemes = data_schemes;
	request_class->send_async = stock_request_send_async;
	request_class->send_finish = stock_request_send_finish;
	request_class->get_content_type = stock_request_get_content_type;
	request_class->get_content_length = stock_request_get_content_length;
	request_class->check_uri = stock_request_check_uri;
}

static void
e_stock_request_init (EStockRequest *request)
{
	request->priv = E_STOCK_REQUEST_GET_PRIVATE (request);
}


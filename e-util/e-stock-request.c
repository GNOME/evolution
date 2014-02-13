/*
 * e-stock-request.c
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

#include "e-stock-request.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <string.h>

#define E_STOCK_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_STOCK_REQUEST, EStockRequestPrivate))

struct _EStockRequestPrivate {
	gchar *content_type;
	gint content_length;
};

static const gchar *data_schemes[] = { "gtk-stock", NULL };

G_DEFINE_TYPE (EStockRequest, e_stock_request, SOUP_TYPE_REQUEST)

static gboolean
handle_stock_request_idle_cb (gpointer user_data)
{
	EStockRequestPrivate *priv;
	GSimpleAsyncResult *simple;
	GObject *object;
	SoupURI *uri;
	GHashTable *query = NULL;
	GtkStyleContext *context;
	GtkWidgetPath *path;
	GtkIconSet *icon_set;
	gssize size = GTK_ICON_SIZE_BUTTON;
	gchar *a_size;
	gchar *buffer = NULL;
	gsize buff_len = 0;
	GError *local_error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	/* This returns a new reference. */
	object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));

	priv = E_STOCK_REQUEST_GET_PRIVATE (object);

	uri = soup_request_get_uri (SOUP_REQUEST (object));
	if (uri->query != NULL)
		query = soup_form_decode (uri->query);

	if (query != NULL) {
		a_size = g_hash_table_lookup (query, "size");
		if (a_size != NULL)
			size = atoi (a_size);
		g_hash_table_destroy (query);
	}

	/* Try style context first */
	context = gtk_style_context_new ();
	path = gtk_widget_path_new ();
	gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
	gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
	gtk_style_context_set_path (context, path);
	gtk_widget_path_free (path);

	icon_set = gtk_style_context_lookup_icon_set (context, uri->host);
	if (icon_set != NULL) {
		GdkPixbuf *pixbuf;

		pixbuf = gtk_icon_set_render_icon_pixbuf (
			icon_set, context, size);
		gdk_pixbuf_save_to_buffer (
			pixbuf, &buffer, &buff_len,
			"png", &local_error, NULL);
		g_object_unref (pixbuf);

	/* Fallback to icon theme */
	} else {
		GtkIconTheme *icon_theme;
		GtkIconInfo *icon_info;
		const gchar *filename;

		icon_theme = gtk_icon_theme_get_default ();

		icon_info = gtk_icon_theme_lookup_icon (
			icon_theme, uri->host, size,
			GTK_ICON_LOOKUP_USE_BUILTIN);

		/* Some icons can be missing in the theme */
		if (icon_info) {
			filename = gtk_icon_info_get_filename (icon_info);
			if (filename != NULL) {
				if (!g_file_get_contents (
					filename, &buffer, &buff_len, &local_error)) {
					buffer = NULL;
					buff_len = 0;
				}
				priv->content_type =
					g_content_type_guess (filename, NULL, 0, NULL);

			} else {
				GdkPixbuf *pixbuf;

				pixbuf = gtk_icon_info_get_builtin_pixbuf (icon_info);
				if (pixbuf != NULL) {
					gdk_pixbuf_save_to_buffer (
						pixbuf, &buffer, &buff_len,
						"png", &local_error, NULL);
					g_object_unref (pixbuf);
				}
			}

			gtk_icon_info_free (icon_info);
		}
	}

	/* Sanity check */
	g_warn_if_fail (
		((buffer != NULL) && (local_error == NULL)) ||
		((buffer == NULL) && (local_error != NULL)));

	if (priv->content_type == NULL)
		priv->content_type = g_strdup ("image/png");
	priv->content_length = buff_len;

	if (buffer != NULL) {
		GInputStream *stream;

		stream = g_memory_input_stream_new_from_data (
			buffer, buff_len, (GDestroyNotify) g_free);
		g_simple_async_result_set_op_res_gpointer (
			simple, g_object_ref (stream),
			(GDestroyNotify) g_object_unref);
		g_object_unref (stream);
	}

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);

	g_simple_async_result_complete_in_idle (simple);

	g_object_unref (context);
	g_object_unref (object);

	return FALSE;
}

static void
stock_request_finalize (GObject *object)
{
	EStockRequestPrivate *priv;

	priv = E_STOCK_REQUEST_GET_PRIVATE (object);

	g_free (priv->content_type);

	/* Chain up to parent's finalize() method. */
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

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback, user_data,
		stock_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	/* Need to run this operation in an idle callback rather
	 * than a worker thread, since we're making all kinds of
	 * GdkPixbuf/GTK+ calls. */
	g_idle_add_full (
		G_PRIORITY_HIGH_IDLE,
		handle_stock_request_idle_cb,
		g_object_ref (simple),
		(GDestroyNotify) g_object_unref);

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

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	/* Reset the stream before passing it back to WebKit. */
	if (G_IS_SEEKABLE (stream))
		g_seekable_seek (
			G_SEEKABLE (stream), 0,
			G_SEEK_SET, NULL, NULL);

	if (stream != NULL)
		return g_object_ref (stream);

	return g_memory_input_stream_new ();
}

static goffset
stock_request_get_content_length (SoupRequest *request)
{
	EStockRequestPrivate *priv;

	priv = E_STOCK_REQUEST_GET_PRIVATE (request);

	return priv->content_length;
}

static const gchar *
stock_request_get_content_type (SoupRequest *request)
{
	EStockRequestPrivate *priv;

	priv = E_STOCK_REQUEST_GET_PRIVATE (request);

	return priv->content_type;
}

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
	request_class->check_uri = stock_request_check_uri;
	request_class->send_async = stock_request_send_async;
	request_class->send_finish = stock_request_send_finish;
	request_class->get_content_length = stock_request_get_content_length;
	request_class->get_content_type = stock_request_get_content_type;
}

static void
e_stock_request_init (EStockRequest *request)
{
	request->priv = E_STOCK_REQUEST_GET_PRIVATE (request);
}


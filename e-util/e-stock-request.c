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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <libedataserver/libedataserver.h>

#include "e-misc-utils.h"
#include "e-stock-request.h"

struct _EStockRequestPrivate {
	gint dummy;
};

static void e_stock_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EStockRequest, e_stock_request, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_stock_request_content_request_init))

static gboolean
e_stock_request_can_process_uri (EContentRequest *request,
				 const gchar *uri)
{
	g_return_val_if_fail (E_IS_STOCK_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "gtk-stock:", 10) == 0;
}

typedef struct _StockIdleData
{
	EContentRequest *request;
	const gchar *uri;
	GObject *requester;
	GInputStream **out_stream;
	gint64 *out_stream_length;
	gchar **out_mime_type;
	GCancellable *cancellable;
	GError **error;

	gboolean success;
	EFlag *flag;
} StockIdleData;

static gboolean
process_stock_request_idle_cb (gpointer user_data)
{
	StockIdleData *sid = user_data;
	SoupURI *suri;
	GHashTable *query = NULL;
	GtkStyleContext *context;
	GtkWidgetPath *path;
	GtkIconSet *icon_set;
	gssize size = GTK_ICON_SIZE_BUTTON;
	gchar *a_size;
	gchar *buffer = NULL, *mime_type = NULL;
	gsize buff_len = 0;
	GError *local_error = NULL;

	g_return_val_if_fail (sid != NULL, FALSE);
	g_return_val_if_fail (E_IS_STOCK_REQUEST (sid->request), FALSE);
	g_return_val_if_fail (sid->uri != NULL, FALSE);
	g_return_val_if_fail (sid->flag != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (sid->cancellable, sid->error)) {
		sid->success = FALSE;
		e_flag_set (sid->flag);

		return FALSE;
	}

	suri = soup_uri_new (sid->uri);
	g_return_val_if_fail (suri != NULL, FALSE);

	if (suri->query != NULL)
		query = soup_form_decode (suri->query);

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

	icon_set = gtk_style_context_lookup_icon_set (context, suri->host);
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
		gint icon_width, icon_height;

		if (!gtk_icon_size_lookup (size, &icon_width, &icon_height)) {
			icon_width = size;
			icon_height = size;
		}

		size = MAX (icon_width, icon_height);

		icon_theme = gtk_icon_theme_get_default ();

		icon_info = gtk_icon_theme_lookup_icon (
			icon_theme, suri->host, size,
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
				mime_type = g_content_type_guess (filename, NULL, 0, NULL);
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
		} else if (g_strcmp0 (suri->host, "x-evolution-arrow-down") == 0) {
			GdkPixbuf *pixbuf;
			GdkRGBA rgba;
			guchar *data;
			gint stride;
			cairo_surface_t *surface;
			cairo_t *cr;

			stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, size);
			buff_len = stride * size;
			data = g_malloc0 (buff_len);
			surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_RGB24, size, size, stride);

			cr = cairo_create (surface);

			if (gtk_style_context_lookup_color (context, "color", &rgba))
				gdk_cairo_set_source_rgba (cr, &rgba);
			else
				cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

			gtk_render_background (context, cr, 0, 0, size, size);
			gtk_render_arrow (context, cr, G_PI, 0, 0, size);

			cairo_destroy (cr);

			cairo_surface_flush (surface);

			pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, TRUE, 8, size, size, stride, NULL, NULL);
			gdk_pixbuf_save_to_buffer (
				pixbuf, &buffer, &buff_len,
				"png", &local_error, NULL);
			g_object_unref (pixbuf);

			cairo_surface_destroy (surface);
			g_free (data);
		}
	}

	/* Sanity check */
	g_warn_if_fail (
		((buffer != NULL) && (local_error == NULL)) ||
		((buffer == NULL) && (local_error != NULL)));

	if (!mime_type)
		mime_type = g_strdup ("image/png");

	if (buffer != NULL) {
		*sid->out_stream = g_memory_input_stream_new_from_data (buffer, buff_len, g_free);;
		*sid->out_stream_length = buff_len;
		*sid->out_mime_type = mime_type;

		sid->success = TRUE;
	} else {
		g_free (mime_type);

		if (local_error)
			g_propagate_error (sid->error, local_error);

		sid->success = FALSE;
	}

	soup_uri_free (suri);
	g_object_unref (context);

	e_flag_set (sid->flag);

	return FALSE;
}

static gboolean
e_stock_request_process_sync (EContentRequest *request,
			      const gchar *uri,
			      GObject *requester,
			      GInputStream **out_stream,
			      gint64 *out_stream_length,
			      gchar **out_mime_type,
			      GCancellable *cancellable,
			      GError **error)
{
	StockIdleData sid;

	g_return_val_if_fail (E_IS_STOCK_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	sid.request = request;
	sid.uri = uri;
	sid.requester = requester;
	sid.out_stream = out_stream;
	sid.out_stream_length = out_stream_length;
	sid.out_mime_type = out_mime_type;
	sid.cancellable = cancellable;
	sid.error = error;
	sid.flag = e_flag_new ();
	sid.success = FALSE;

	if (e_util_is_main_thread (NULL)) {
		process_stock_request_idle_cb (&sid);
	} else {
		/* Need to run this operation in an idle callback rather
		 * than a worker thread, since we're making all kinds of
		 * GdkPixbuf/GTK+ calls. */
		g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			process_stock_request_idle_cb,
			&sid, NULL);

		e_flag_wait (sid.flag);
	}

	e_flag_free (sid.flag);

	return sid.success;
}

static void
e_stock_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_stock_request_can_process_uri;
	iface->process_sync = e_stock_request_process_sync;
}

static void
e_stock_request_class_init (EStockRequestClass *class)
{
	g_type_class_add_private (class, sizeof (EStockRequestPrivate));
}

static void
e_stock_request_init (EStockRequest *request)
{
	request->priv = G_TYPE_INSTANCE_GET_PRIVATE (request, E_TYPE_STOCK_REQUEST, EStockRequestPrivate);
}

EContentRequest *
e_stock_request_new (void)
{
	return g_object_new (E_TYPE_STOCK_REQUEST, NULL);
}

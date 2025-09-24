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

#include "evolution-config.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <libedataserver/libedataserver.h>

#include "e-icon-factory.h"
#include "e-misc-utils.h"
#include "e-stock-request.h"

struct _EStockRequestPrivate {
	gint scale_factor;
};

enum {
	PROP_0,
	PROP_SCALE_FACTOR
};

static void e_stock_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EStockRequest, e_stock_request, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EStockRequest)
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
	GdkRGBA theme_fg = { 0, };
	GUri *guri;
	GHashTable *query = NULL;
	GtkStyleContext *context;
	GtkWidgetPath *path;
	GtkIconSet *icon_set = NULL;
	gssize size = GTK_ICON_SIZE_BUTTON;
	gboolean dark_color_scheme = FALSE;
	const gchar *icon_name;
	gchar *icon_name_symbolic = NULL;
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

	guri = g_uri_parse (sid->uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (guri != NULL, FALSE);

	if (g_uri_get_query (guri))
		query = soup_form_decode (g_uri_get_query (guri));

	if (query != NULL) {
		const gchar *value;

		value = g_hash_table_lookup (query, "size");
		if (value)
			size = atoi (value);
		value = g_hash_table_lookup (query, "color-scheme");
		dark_color_scheme = value && g_ascii_strcasecmp (value, "dark") == 0;

		g_hash_table_destroy (query);
	}

	/* Try style context first */
	context = gtk_style_context_new ();
	path = gtk_widget_path_new ();
	gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
	gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
	gtk_style_context_set_path (context, path);
	gtk_widget_path_free (path);

	if (!gtk_style_context_lookup_color (context, "theme_fg_color", &theme_fg))
		gdk_rgba_parse (&theme_fg, E_UTILS_DEFAULT_THEME_FG_COLOR);

	icon_name = g_uri_get_host (guri);
	if (e_icon_factory_get_prefer_symbolic_icons () && !g_str_has_suffix (icon_name, "-symbolic"))
		icon_name_symbolic = g_strconcat (icon_name, "-symbolic", NULL);

	if (icon_name_symbolic)
		icon_set = gtk_style_context_lookup_icon_set (context, icon_name_symbolic);
	if (!icon_set)
		icon_set = gtk_style_context_lookup_icon_set (context, icon_name);
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
		GtkIconInfo *icon_info = NULL;
		gint icon_width, icon_height, scale_factor;

		scale_factor = e_stock_request_get_scale_factor (E_STOCK_REQUEST (sid->request));

		if (scale_factor < 1)
			scale_factor = 1;

		if (!gtk_icon_size_lookup (size, &icon_width, &icon_height)) {
			icon_width = size;
			icon_height = size;
		}

		size = MAX (icon_width, icon_height) * scale_factor;

		icon_theme = gtk_icon_theme_get_default ();

		if (icon_name_symbolic)
			icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name_symbolic, size, GTK_ICON_LOOKUP_USE_BUILTIN);
		if (!icon_info)
			icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, size, GTK_ICON_LOOKUP_USE_BUILTIN);

		/* Some icons can be missing in the theme */
		if (icon_info) {
			GdkPixbuf *pixbuf = NULL;

			if (icon_name_symbolic || g_str_has_suffix (icon_name, "-symbolic"))
				pixbuf = gtk_icon_info_load_symbolic (icon_info, &theme_fg, NULL, NULL, NULL, NULL, NULL);

			if (!pixbuf)
				pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

			if (pixbuf) {
				gdk_pixbuf_save_to_buffer (
					pixbuf, &buffer, &buff_len,
					"png", &local_error, NULL);
				g_object_unref (pixbuf);
			}

			g_object_unref (icon_info);
		} else if (g_strcmp0 (icon_name, "x-evolution-arrow-down") == 0) {
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
		} else if (g_strcmp0 (icon_name, "x-evolution-pan-down") == 0) {
			#define PAN_SCHEME_LIGHT "#2e3436"
			#define PAN_SCHEME_DARK "#d1cbc9"
			#define PAN_PATH_DOWN "M 3.4393771,1.4543954 H 0.7935438 l 1.3229166,1.3229167 z"
			#define PAN_PATH_END "M 1.4550021,3.4387704 V 0.7929371 l 1.3229167,1.3229166 z"
			#define PAN_PATH_END_RTL "M 2.7779188,3.4387704 V 0.7929371 L 1.4550021,2.1158537 Z"
			#define PAN_SVG(_path, _color) \
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
				"<svg viewBox=\"0 0 4.2333332 4.2333333\" width=\"16px\" height=\"16px\" xmlns=\"http://www.w3.org/2000/svg\">\n" \
				"  <path d=\"" _path "\" fill=\"" _color "\"/>\n" \
				"</svg>\n"

			const gchar *svg;

			if (dark_color_scheme)
				svg = PAN_SVG (PAN_PATH_DOWN, PAN_SCHEME_DARK);
			else
				svg = PAN_SVG (PAN_PATH_DOWN, PAN_SCHEME_LIGHT);

			mime_type = g_strdup ("image/svg+xml");
			buff_len = strlen (svg);
			buffer = g_strdup (svg);
		} else if (g_strcmp0 (icon_name, "x-evolution-pan-end") == 0) {
			const gchar *svg;

			if (dark_color_scheme) {
				if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
					svg = PAN_SVG (PAN_PATH_END_RTL, PAN_SCHEME_DARK);
				else
					svg = PAN_SVG (PAN_PATH_END, PAN_SCHEME_DARK);
			} else {
				if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
					svg = PAN_SVG (PAN_PATH_END_RTL, PAN_SCHEME_LIGHT);
				else
					svg = PAN_SVG (PAN_PATH_END, PAN_SCHEME_LIGHT);
			}

			mime_type = g_strdup ("image/svg+xml");
			buff_len = strlen (svg);
			buffer = g_strdup (svg);

			#undef PAN_COLOR_LIGHT
			#undef PAN_COLOR_DARK
			#undef PAN_PATH_DOWN
			#undef PAN_PATH_END
			#undef PAN_PATH_END_RTL
			#undef PAN_SVG
		}
	}

	/* Sanity check */
	g_warn_if_fail (
		((buffer != NULL) && (local_error == NULL)) ||
		((buffer == NULL) && (local_error != NULL)));

	if (!mime_type)
		mime_type = g_strdup ("image/png");

	if (buffer != NULL) {
		*sid->out_stream = g_memory_input_stream_new_from_data (buffer, buff_len, g_free);
		*sid->out_stream_length = buff_len;
		*sid->out_mime_type = mime_type;

		sid->success = TRUE;
	} else {
		g_free (mime_type);

		if (local_error)
			g_propagate_error (sid->error, local_error);

		sid->success = FALSE;
	}

	g_uri_unref (guri);
	g_object_unref (context);
	g_free (icon_name_symbolic);

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
e_stock_request_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCALE_FACTOR:
			e_stock_request_set_scale_factor (
				E_STOCK_REQUEST (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_stock_request_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCALE_FACTOR:
			g_value_set_int (
				value,
				e_stock_request_get_scale_factor (
				E_STOCK_REQUEST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_stock_request_set_property;
	object_class->get_property = e_stock_request_get_property;

	g_object_class_install_property (
		object_class,
		PROP_SCALE_FACTOR,
		g_param_spec_int (
			"scale-factor",
			"Scale Factor",
			NULL,
			G_MININT, G_MAXINT, 0,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_stock_request_init (EStockRequest *request)
{
	request->priv = e_stock_request_get_instance_private (request);
	request->priv->scale_factor = 0;
}

EContentRequest *
e_stock_request_new (void)
{
	return g_object_new (E_TYPE_STOCK_REQUEST, NULL);
}

gint
e_stock_request_get_scale_factor (EStockRequest *stock_request)
{
	g_return_val_if_fail (E_IS_STOCK_REQUEST (stock_request), 0);

	return stock_request->priv->scale_factor;
}

void
e_stock_request_set_scale_factor (EStockRequest *stock_request,
				  gint scale_factor)
{
	g_return_if_fail (E_IS_STOCK_REQUEST (stock_request));

	if (stock_request->priv->scale_factor == scale_factor)
		return;

	stock_request->priv->scale_factor = scale_factor;

	g_object_notify (G_OBJECT (stock_request), "scale-factor");
}

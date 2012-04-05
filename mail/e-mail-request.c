/*
 * e-mail-request.c
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

#include "e-mail-request.h"

#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>

#include <glib/gi18n.h>
#include <camel/camel.h>

#include "em-format-html.h"

#include <e-util/e-icon-factory.h>
#include <e-util/e-util.h>

#define d(x)
#define dd(x)

#define E_MAIL_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_REQUEST, EMailRequestPrivate))

struct _EMailRequestPrivate {
	EMFormatHTML *efh;

	CamelStream *output_stream;
	EMFormatPURI *puri;
	gchar *mime_type;

	gint content_length;

	GHashTable *uri_query;

        gchar *ret_mime_type;
};

G_DEFINE_TYPE (EMailRequest, e_mail_request, SOUP_TYPE_REQUEST)

static void
handle_mail_request (GSimpleAsyncResult *res,
                     GObject *object,
                     GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	EMFormatHTML *efh = request->priv->efh;
	EMFormat *emf = EM_FORMAT (efh);
	GInputStream *stream;
	GByteArray *ba;
	gchar *part_id;
	EMFormatWriterInfo info = {0};
	gchar *val;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	if (request->priv->output_stream != NULL) {
		g_object_unref (request->priv->output_stream);
	}

	request->priv->output_stream = camel_stream_mem_new ();

	val = g_hash_table_lookup (request->priv->uri_query, "headers_collapsed");
	if (val)
		info.headers_collapsed = atoi (val);

	val = g_hash_table_lookup (request->priv->uri_query, "headers_collapsable");
	if (val)
		info.headers_collapsable = atoi (val);

	val = g_hash_table_lookup (request->priv->uri_query, "mode");
	if (val)
		info.mode = atoi (val);

	part_id = g_hash_table_lookup (request->priv->uri_query, "part_id");
	if (part_id) {
		/* original part_id is owned by the GHashTable */
		part_id = soup_uri_decode (part_id);
		request->priv->puri = em_format_find_puri (emf, part_id);

		if (request->priv->puri) {
			em_format_puri_write (request->priv->puri,
				request->priv->output_stream, &info, NULL);
		} else {
			g_warning ("Failed to lookup requested part '%s' - this should not happen!", part_id);
		}

		g_free (part_id);
	} else {
		if (info.mode == 0)
			info.mode = EM_FORMAT_WRITE_MODE_NORMAL;

		em_format_write (emf, request->priv->output_stream, &info, NULL);
	}

	/* Convert the GString to GInputStream and send it back to WebKit */
	ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (request->priv->output_stream));
	if (!ba->data) {
		gchar *data = g_strdup_printf(_("Failed to load part '%s'"), part_id);
		dd(printf("%s", data));
		g_byte_array_append (ba, (guchar *) data, strlen (data));
		g_free (data);
	} else {
		dd ({
			gchar *d = g_strndup ((gchar *) ba->data, ba->len);
			printf("%s", d);
			g_free (d);
		});
	}

	stream = g_memory_input_stream_new_from_data (
			(gchar *) ba->data, ba->len, NULL);
	g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
}

static void
handle_file_request (GSimpleAsyncResult *res,
                     GObject *object,
                     GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	SoupURI *uri;
	GInputStream *stream;
	gchar *contents;
	gsize length;

	if (g_cancellable_is_cancelled (cancellable))
		return;

	uri = soup_request_get_uri (SOUP_REQUEST (request));

	if (g_file_get_contents (uri->path, &contents, &length, NULL)) {

		request->priv->mime_type = g_content_type_guess (uri->path, NULL, 0, NULL);
		request->priv->content_length = length;

		stream = g_memory_input_stream_new_from_data (
				contents, length, (GDestroyNotify) g_free);
		g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
	}
}

struct http_request_async_data {
	GMainLoop *loop;
	GCancellable *cancellable;
	CamelDataCache *cache;
	gchar *cache_key;

	GInputStream *stream;
	CamelStream *cache_stream;
	gchar *content_type;
	goffset content_length;

	gchar *buff;
};

static void
http_request_write_to_cache (GInputStream *stream,
                             GAsyncResult *res,
                             struct http_request_async_data *data)
{
	GError *error;
	gssize len;

	error = NULL;
	len = g_input_stream_read_finish (stream, res, &error);

	/* Error while reading data */
	if (len == -1) {
		g_message ("Error while reading input stream: %s",
			error ? error->message : "Unknown error");
		g_clear_error (&error);

		g_main_loop_quit (data->loop);

		if (data->buff)
			g_free (data->buff);

		/* Don't keep broken data in cache */
		camel_data_cache_remove (data->cache, "http", data->cache_key, NULL);
		return;
	}

	/* EOF */
	if (len == 0) {
		camel_stream_close (data->cache_stream, data->cancellable, NULL);

		if (data->buff)
			g_free (data->buff);

		g_main_loop_quit (data->loop);
		return;
	}

	if (!data->cache_stream) {

		if (data->buff)
			g_free (data->buff);

		g_main_loop_quit (data->loop);
		return;
	}

	/* Write chunk to cache and read another block of data. */
	camel_stream_write (data->cache_stream, data->buff, len,
		data->cancellable, NULL);

	g_input_stream_read_async (stream, data->buff, 4096,
		G_PRIORITY_DEFAULT, data->cancellable,
		(GAsyncReadyCallback) http_request_write_to_cache, data);
}

static void
http_request_finished (SoupRequest *request,
                       GAsyncResult *res,
                       struct http_request_async_data *data)
{
	GError *error;
	SoupMessage *message;

	error = NULL;
	data->stream = soup_request_send_finish (request, res, &error);

	if (!data->stream) {
		g_warning("HTTP request failed: %s", error ? error->message: "Unknown error");
		g_clear_error (&error);
		g_main_loop_quit (data->loop);
		return;
	}

	message = soup_request_http_get_message (SOUP_REQUEST_HTTP (request));
	if (!SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
		g_warning ("HTTP request failed: HTTP code %d", message->status_code);
		g_main_loop_quit (data->loop);
		g_object_unref (message);
		return;
	}

	g_object_unref (message);

	data->content_length = soup_request_get_content_length (request);
	data->content_type = g_strdup (soup_request_get_content_type (request));

	if (!data->cache_stream) {
		g_main_loop_quit (data->loop);
		return;
	}

	data->buff = g_malloc (4096);
	g_input_stream_read_async (data->stream, data->buff, 4096,
		G_PRIORITY_DEFAULT, data->cancellable,
		(GAsyncReadyCallback) http_request_write_to_cache, data);
}

static void
handle_http_request (GSimpleAsyncResult *res,
                     GObject *object,
                     GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	SoupURI *soup_uri;
	gchar *evo_uri, *uri;
	GInputStream *stream;
	gboolean force_load_images = FALSE;
	gchar *uri_md5;

	const gchar *user_cache_dir;
	CamelDataCache *cache;
	CamelStream *cache_stream;

	gssize len;
	gchar *buff;

	GHashTable *query;

	/* Remove the __evo-mail query */
	soup_uri = soup_request_get_uri (SOUP_REQUEST (request));
	query = soup_form_decode (soup_uri->query);
	g_hash_table_remove (query, "__evo-mail");

	/* Remove __evo-load-images if present (and in such case set
	 * force_load_images to TRUE) */
	force_load_images = g_hash_table_remove (query, "__evo-load-images");

	soup_uri_set_query_from_form (soup_uri, query);
	g_hash_table_unref (query);

	evo_uri = soup_uri_to_string (soup_uri, FALSE);

	/* Remove the "evo-" prefix from scheme */
	if (evo_uri && (strlen (evo_uri) > 5)) {
		uri = g_strdup (&evo_uri[4]);
		g_free (evo_uri);
	}

	g_return_if_fail (uri && *uri);

	/* Use MD5 hash of the URI as a filname of the resourec cache file.
	 * We were previously using the URI as a filename but the URI is
	 * sometimes too long for a filename. */
	uri_md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);

	/* Open Evolution's cache */
	user_cache_dir = e_get_user_cache_dir ();
	cache = camel_data_cache_new (user_cache_dir, NULL);
	if (cache) {
		camel_data_cache_set_expire_age (cache, 24 * 60 * 60);
		camel_data_cache_set_expire_access (cache, 2 * 60 * 60);
	}

	/* Found item in cache! */
	cache_stream = camel_data_cache_get (cache, "http", uri_md5, NULL);
	if (cache_stream) {

		stream = g_memory_input_stream_new ();

		request->priv->content_length = 0;

		buff = g_malloc (4096);
		while ((len = camel_stream_read (cache_stream, buff, 4096,
				cancellable, NULL)) > 0) {

			g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
				buff, len, g_free);
			request->priv->content_length += len;

			buff = g_malloc (4096);
		}

		g_object_unref (cache_stream);

		/* When succesfully read some data from cache then
		 * get mimetype and return the stream to WebKit.
		 * Otherwise try to fetch the resource again from the network. */
		if ((len != -1) && (request->priv->content_length > 0)) {
			GFile *file;
			GFileInfo *info;
			gchar *path;

			path = camel_data_cache_get_filename (cache, "http", uri_md5, NULL);
			file = g_file_new_for_path (path);
			info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
					0, cancellable, NULL);

			request->priv->mime_type = g_strdup (
				g_file_info_get_content_type (info));

			d(printf ("'%s' found in cache (%d bytes, %s)\n",
				uri, request->priv->content_length,
				request->priv->mime_type));

			g_object_unref (info);
			g_object_unref (file);
			g_free (path);

			/* Set result and quit the thread */
			g_simple_async_result_set_op_res_gpointer (res, stream, NULL);

			goto cleanup;
		} else {
			d(printf("Failed to load '%s' from cache.\n", uri));
		}
	}

	/* Item not found in cache, but image loading policy allows us to fetch
	 * it from the interwebs */
	if (force_load_images || em_format_html_can_load_images (request->priv->efh)) {

		SoupRequester *requester;
		SoupRequest *http_request;
		SoupSession *session;
		GMainContext *context;
		GError *error;

		struct http_request_async_data data = { 0 };

		context = g_main_context_get_thread_default ();
		session = soup_session_async_new_with_options (
					SOUP_SESSION_ASYNC_CONTEXT, context, NULL);

		requester = soup_requester_new ();
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (requester));

		http_request = soup_requester_request (requester, uri, NULL);

		error = NULL;
		data.loop = g_main_loop_new (context, TRUE);
		data.cancellable = cancellable;
		data.cache = cache;
		data.cache_key = uri_md5;
		data.cache_stream = camel_data_cache_add (cache, "http", uri_md5, &error);

		if (!data.cache_stream) {
			g_warning ("Failed to create cache file for '%s': %s",
				uri, error ? error->message : "Unknown error");
			g_clear_error (&error);
		}

		/* Send the request and waint in mainloop until it's finished
		 * and copied to cache */
		d(printf(" '%s' not in cache, sending HTTP request\n", uri));
		soup_request_send_async (http_request, cancellable,
			(GAsyncReadyCallback) http_request_finished, &data);

		g_main_loop_run (data.loop);
		d(printf (" '%s' fetched from internet and (hopefully) stored in"
			  " cache\n", uri));

		g_main_loop_unref (data.loop);

		g_object_unref (session);

		g_object_unref (http_request);
		g_object_unref (requester);

		stream = data.stream;
		if (!stream)
			goto cleanup;

		request->priv->content_length = data.content_length;
		request->priv->mime_type = data.content_type;

		g_simple_async_result_set_op_res_gpointer (res, stream, NULL);

		goto cleanup;

	}

cleanup:
	g_free (uri);
	g_free (uri_md5);
}

static void
handle_stock_request (GSimpleAsyncResult *res,
                      GObject *object,
                      GCancellable *cancellable)
{
	EMailRequest *request;
	SoupURI *uri;
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

	request = E_MAIL_REQUEST (object);
	uri = soup_request_get_uri (SOUP_REQUEST (object));

	if (request->priv->uri_query) {
		a_size = g_hash_table_lookup (request->priv->uri_query, "size");
	} else {
		a_size = NULL;
	}

	if (!a_size) {
		size = GTK_ICON_SIZE_BUTTON;
	} else {
		size = atoi (a_size);
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

				request->priv->mime_type =
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

				request->priv->mime_type = g_strdup("image/png");
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

		request->priv->mime_type = g_strdup("image/png");
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
mail_request_finalize (GObject *object)
{
	EMailRequest *request = E_MAIL_REQUEST (object);

	if (request->priv->output_stream) {
		g_object_unref (request->priv->output_stream);
		request->priv->output_stream = NULL;
	}

	if (request->priv->mime_type) {
		g_free (request->priv->mime_type);
		request->priv->mime_type = NULL;
	}

	if (request->priv->uri_query) {
		g_hash_table_destroy (request->priv->uri_query);
		request->priv->uri_query = NULL;
	}

	if (request->priv->ret_mime_type) {
		g_free (request->priv->ret_mime_type);
		request->priv->ret_mime_type = NULL;
	}

	G_OBJECT_CLASS (e_mail_request_parent_class)->finalize (object);
}

static gboolean
mail_request_check_uri (SoupRequest *request,
                       SoupURI *uri,
                       GError **error)
{
	return ((strcmp (uri->scheme, "mail") == 0) ||
		(strcmp (uri->scheme, "evo-file") == 0) ||
		(strcmp (uri->scheme, "evo-http") == 0) ||
		(strcmp (uri->scheme, "evo-https") == 0) ||
		(strcmp (uri->scheme, "gtk-stock") == 0));
}

static void
mail_request_send_async (SoupRequest *request,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	SoupSession *session;
	EMailRequest *emr = E_MAIL_REQUEST (request);
	GSimpleAsyncResult *result;
	SoupURI *uri;
	GHashTable *formatters;

	session = soup_request_get_session (request);
	uri = soup_request_get_uri (request);

	d(printf("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	/* WebKit won't allow us to load data through local file:// protocol
	 * when using "remote" mail:// protocol, so we have evo-file://
	 * which WebKit thinks it's remote, but in fact it behaves like
	 * oridnary file:// */
	if (g_strcmp0 (uri->scheme, "evo-file") == 0) {

		result = g_simple_async_result_new (G_OBJECT (request), callback,
				user_data, mail_request_send_async);
		g_simple_async_result_run_in_thread (result, handle_file_request,
				G_PRIORITY_DEFAULT, cancellable);

		return;
	}

	if (uri->query) {
		emr->priv->uri_query = soup_form_decode (uri->query);
	} else {
		emr->priv->uri_query = NULL;
	}

	formatters = g_object_get_data (G_OBJECT (session), "formatters");
					g_return_if_fail (formatters != NULL);

	/* Get HTML content of given PURI part */
	if (g_strcmp0 (uri->scheme, "mail") == 0) {
		gchar *uri_str;

		uri_str = g_strdup_printf (
			"%s://%s%s", uri->scheme, uri->host, uri->path);
		emr->priv->efh = g_hash_table_lookup (formatters, uri_str);
		g_free (uri_str);

		g_return_if_fail (emr->priv->efh);

		result = g_simple_async_result_new (G_OBJECT (request), callback,
				user_data, mail_request_send_async);
		g_simple_async_result_run_in_thread (result, handle_mail_request,
				G_PRIORITY_DEFAULT, cancellable);

		return;

	/* For http and https requests we have this evo-http(s) protocol.
	 * We first try to lookup the data in local cache and when not found,
	 * we send standard  http(s) request to fetch them. But only when image 
	 * loading policy allows us. */
	} else if ((g_strcmp0 (uri->scheme, "evo-http") == 0) ||
		   (g_strcmp0 (uri->scheme, "evo-https") == 0)) {

		gchar *mail_uri;
		const gchar *enc = g_hash_table_lookup (emr->priv->uri_query,
					"__evo-mail");

		g_return_if_fail (enc && *enc);

		mail_uri = soup_uri_decode (enc);

		emr->priv->efh = g_hash_table_lookup (formatters, mail_uri);
		g_free (mail_uri);

		g_return_if_fail (emr->priv->efh);

		result = g_simple_async_result_new (G_OBJECT (request), callback,
				user_data, mail_request_send_async);
		g_simple_async_result_run_in_thread (result, handle_http_request,
				G_PRIORITY_DEFAULT, cancellable);

		return;

	} else if ((g_strcmp0 (uri->scheme, "gtk-stock") == 0)) {

		result = g_simple_async_result_new (G_OBJECT (request), callback,
				user_data, mail_request_send_async);
		g_simple_async_result_run_in_thread (result, handle_stock_request,
				G_PRIORITY_DEFAULT, cancellable);

		return;
	}
}

static GInputStream *
mail_request_send_finish (SoupRequest *request,
                          GAsyncResult *result,
                          GError **error)
{
	GInputStream *stream;

	stream = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	g_object_unref (result);

	/* Reset the stream before passing it back to webkit */
	if (stream && G_IS_SEEKABLE (stream))
		g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, NULL);
	else /* We must always return something */
		stream = g_memory_input_stream_new ();

	return stream;
}

static goffset
mail_request_get_content_length (SoupRequest *request)
{
	EMailRequest *emr = E_MAIL_REQUEST (request);
	GByteArray *ba;
	gint content_length = 0;

	if (emr->priv->content_length > 0)
		content_length = emr->priv->content_length;
	else if (emr->priv->output_stream) {
		ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (emr->priv->output_stream));
		if (ba) {
			content_length = ba->len;
		}
	}

	d(printf("Content-Length: %d bytes\n", content_length));
	return content_length;
}

static const gchar *
mail_request_get_content_type (SoupRequest *request)
{
	EMailRequest *emr = E_MAIL_REQUEST (request);
	gchar *mime_type;

	if (emr->priv->mime_type) {
		mime_type = g_strdup (emr->priv->mime_type);
	} else if (!emr->priv->puri) {
		mime_type = g_strdup ("text/html");
	} else if (!emr->priv->puri->mime_type) {
		CamelContentType *ct = camel_mime_part_get_content_type (emr->priv->puri->part);
		mime_type = camel_content_type_simple (ct);
	} else {
		mime_type = g_strdup (emr->priv->puri->mime_type);
	}

	if (g_strcmp0 (mime_type, "text/html") == 0) {
		emr->priv->ret_mime_type = g_strconcat (mime_type, "; charset=\"UTF-8\"", NULL);
		g_free (mime_type);
	} else {
		emr->priv->ret_mime_type = mime_type;
	}

	d(printf("Content-Type: %s\n", emr->priv->ret_mime_type));

	return emr->priv->ret_mime_type;
}

static const char *data_schemes[] = { "mail", "evo-file", "evo-http", "evo-https", "gtk-stock", NULL };

static void
e_mail_request_class_init (EMailRequestClass *class)
{
	GObjectClass *object_class;
	SoupRequestClass *request_class;

	g_type_class_add_private (class, sizeof (EMailRequestPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_request_finalize;

	request_class = SOUP_REQUEST_CLASS (class);
	request_class->schemes = data_schemes;
	request_class->send_async = mail_request_send_async;
	request_class->send_finish = mail_request_send_finish;
	request_class->get_content_type = mail_request_get_content_type;
	request_class->get_content_length = mail_request_get_content_length;
	request_class->check_uri = mail_request_check_uri;
}

static void
e_mail_request_init (EMailRequest *request)
{
	request->priv = E_MAIL_REQUEST_GET_PRIVATE (request);
}


/*
 * e-http-request.c
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

#include <string.h>

#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>
#include <camel/camel.h>
#include <webkit2/webkit2.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/em-utils.h>

#include <shell/e-shell.h>

#include "e-mail-ui-session.h"
#include "e-http-request.h"

#define d(x)

struct _EHTTPRequestPrivate {
	gint dummy;
};

static void e_http_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EHTTPRequest, e_http_request, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_http_request_content_request_init))

static gboolean
e_http_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_HTTP_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "evo-http:", 9) == 0 ||
	       g_ascii_strncasecmp (uri, "evo-https:", 10) == 0;
}

static gssize
copy_stream_to_stream (GIOStream *file_io_stream,
                       GMemoryInputStream *output,
                       GCancellable *cancellable)
{
	GInputStream *input_stream;
	gchar *buff;
	gssize read_len = 0;
	gssize total_len = 0;
	const gsize buff_size = 4096;

	g_seekable_seek (
		G_SEEKABLE (file_io_stream), 0,
		G_SEEK_SET, cancellable, NULL);

	input_stream = g_io_stream_get_input_stream (file_io_stream);

	buff = g_malloc (buff_size);
	read_len = g_input_stream_read (
		input_stream, buff, buff_size, cancellable, NULL);
	while (read_len > 0) {
		g_memory_input_stream_add_data (
			output, buff, read_len, g_free);

		total_len += read_len;

		buff = g_malloc (buff_size);
		read_len = g_input_stream_read (
			input_stream, buff, buff_size, cancellable, NULL);
	}

	/* Free the last unused buffer */
	g_free (buff);

	return total_len;
}

static void
redirect_handler (SoupMessage *msg,
                  gpointer user_data)
{
	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		SoupSession *soup_session = user_data;
		SoupURI *new_uri;
		const gchar *new_loc;

		new_loc = soup_message_headers_get_list (
			msg->response_headers, "Location");
		if (new_loc == NULL)
			return;

		new_uri = soup_uri_new_with_base (
			soup_message_get_uri (msg), new_loc);
		if (new_uri == NULL) {
			soup_message_set_status_full (
				msg,
				SOUP_STATUS_MALFORMED,
				"Invalid Redirect URL");
			return;
		}

		soup_message_set_uri (msg, new_uri);
		soup_session_requeue_message (soup_session, msg);

		soup_uri_free (new_uri);
	}
}

static void
send_and_handle_redirection (SoupSession *session,
                             SoupMessage *message,
                             gchar **new_location)
{
	SoupURI *soup_uri;
	gchar *old_uri = NULL;

	g_return_if_fail (message != NULL);

	soup_uri = soup_message_get_uri (message);

	if (new_location != NULL)
		old_uri = soup_uri_to_string (soup_uri, FALSE);

	soup_message_set_flags (message, SOUP_MESSAGE_NO_REDIRECT);
	soup_message_add_header_handler (
		message, "got_body", "Location",
		G_CALLBACK (redirect_handler), session);
	soup_message_headers_append (
		message->request_headers, "Connection", "close");
	soup_session_send_message (session, message);

	if (new_location != NULL) {
		gchar *new_loc;

		new_loc = soup_uri_to_string (soup_uri, FALSE);

		if (new_loc && old_uri && !g_str_equal (new_loc, old_uri)) {
			*new_location = new_loc;
		} else {
			g_free (new_loc);
		}
	}

	g_free (old_uri);
}

static void
http_request_cancelled_cb (GCancellable *cancellable,
			   SoupSession *session)
{
	soup_session_abort (session);
}

static gboolean
e_http_request_process_sync (EContentRequest *request,
			     const gchar *uri,
			     GObject *requester,
			     GInputStream **out_stream,
			     gint64 *out_stream_length,
			     gchar **out_mime_type,
			     GCancellable *cancellable,
			     GError **error)
{
	SoupURI *soup_uri;
	gchar *evo_uri, *use_uri;
	gchar *mail_uri = NULL;
	GInputStream *stream;
	gboolean force_load_images = FALSE;
	EImageLoadingPolicy image_policy;
	gchar *uri_md5;
	EShell *shell;
	GSettings *settings;
	const gchar *user_cache_dir, *soup_query;
	CamelDataCache *cache;
	GIOStream *cache_stream;
	gint uri_len;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_HTTP_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	soup_uri = soup_uri_new (uri);
	g_return_val_if_fail (soup_uri != NULL, FALSE);

	/* Remove the __evo-mail query */
	soup_query = soup_uri_get_query (soup_uri);
	if (soup_query) {
		GHashTable *query;

		query = soup_form_decode (soup_uri_get_query (soup_uri));
		mail_uri = g_hash_table_lookup (query, "__evo-mail");
		if (mail_uri)
			mail_uri = g_strdup (mail_uri);

		g_hash_table_remove (query, "__evo-mail");

		/* Remove __evo-load-images if present (and in such case set
		 * force_load_images to TRUE) */
		force_load_images = g_hash_table_remove (query, "__evo-load-images");

		soup_uri_set_query_from_form (soup_uri, query);
		g_hash_table_unref (query);
	}

	evo_uri = soup_uri_to_string (soup_uri, FALSE);

	if (camel_debug_start ("emformat:requests")) {
		printf (
			"%s: looking for '%s'\n",
			G_STRFUNC, evo_uri ? evo_uri : "[null]");
		camel_debug_end ();
	}

	/* Remove the "evo-" prefix from scheme */
	uri_len = (evo_uri != NULL) ? strlen (evo_uri) : 0;
	use_uri = NULL;
	if (evo_uri != NULL && (uri_len > 5)) {

		/* Remove trailing "?" if there is no URI query */
		if (evo_uri[uri_len - 1] == '?') {
			use_uri = g_strndup (evo_uri + 4, uri_len - 5);
		} else {
			use_uri = g_strdup (evo_uri + 4);
		}
		g_free (evo_uri);
	}

	g_return_val_if_fail (use_uri && *use_uri, FALSE);

	*out_stream_length = -1;

	/* Use MD5 hash of the URI as a filname of the resourec cache file.
	 * We were previously using the URI as a filename but the URI is
	 * sometimes too long for a filename. */
	uri_md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, use_uri, -1);

	/* Open Evolution's cache */
	user_cache_dir = e_get_user_cache_dir ();
	cache = camel_data_cache_new (user_cache_dir, NULL);
	camel_data_cache_set_expire_age (cache, 24 * 60 * 60);
	camel_data_cache_set_expire_access (cache, 2 * 60 * 60);

	/* Found item in cache! */
	cache_stream = camel_data_cache_get (cache, "http", uri_md5, NULL);
	if (cache_stream != NULL) {
		gssize len;

		stream = g_memory_input_stream_new ();

		len = copy_stream_to_stream (cache_stream, G_MEMORY_INPUT_STREAM (stream), cancellable);
		*out_stream_length = len;

		g_object_unref (cache_stream);

		/* When succesfully read some data from cache then
		 * get mimetype and return the stream to WebKit.
		 * Otherwise try to fetch the resource again from the network. */
		if (len != -1 && *out_stream_length > 0) {
			GFile *file;
			GFileInfo *info;
			gchar *path;

			path = camel_data_cache_get_filename (
				cache, "http", uri_md5);
			file = g_file_new_for_path (path);
			info = g_file_query_info (
				file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				0, cancellable, NULL);

			if (info) {
				*out_mime_type = g_strdup (g_file_info_get_content_type (info));

				d (
					printf ("'%s' found in cache (%d bytes, %s)\n",
					use_uri, (gint) *out_stream_length,
					*out_mime_type));
			}

			g_clear_object (&info);
			g_clear_object (&file);
			g_free (path);

			/* Set result and quit the thread */
			*out_stream = stream;
			success = TRUE;

			goto cleanup;
		} else {
			d (printf ("Failed to load '%s' from cache.\n", use_uri));
			g_object_unref (stream);
		}
	}

	/* If the item is not cached and Evolution is offline
	 * then quit regardless of any image loading policy. */
	shell = e_shell_get_default ();
	if (!e_shell_get_online (shell))
		goto cleanup;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	image_policy = g_settings_get_enum (settings, "image-loading-policy");
	g_object_unref (settings);

	/* Item not found in cache, but image loading policy allows us to fetch
	 * it from the interwebs */
	if (!force_load_images && mail_uri != NULL &&
	    (image_policy == E_IMAGE_LOADING_POLICY_SOMETIMES)) {
		CamelObjectBag *registry;
		gchar *decoded_uri;
		EMailPartList *part_list;

		registry = e_mail_part_list_get_registry ();
		decoded_uri = soup_uri_decode (mail_uri);

		part_list = camel_object_bag_get (registry, decoded_uri);
		if (part_list != NULL) {
			EShellBackend *shell_backend;
			EMailBackend *backend;
			EMailSession *session;
			CamelInternetAddress *addr;
			CamelMimeMessage *message;
			gboolean known_address = FALSE;

			shell_backend =
				e_shell_get_backend_by_name (shell, "mail");
			backend = E_MAIL_BACKEND (shell_backend);
			session = e_mail_backend_get_session (backend);

			message = e_mail_part_list_get_message (part_list);
			addr = camel_mime_message_get_from (message);

			if (!e_mail_ui_session_check_known_address_sync (
				E_MAIL_UI_SESSION (session),
				addr, FALSE, cancellable,
				&known_address, error)) {
				g_object_unref (part_list);
				g_free (decoded_uri);
				goto cleanup;
			}

			if (known_address)
				force_load_images = TRUE;

			g_object_unref (part_list);
		}

		g_free (decoded_uri);
	}

	if ((image_policy == E_IMAGE_LOADING_POLICY_ALWAYS) ||
	    force_load_images) {
		ESource *proxy_source;
		SoupSession *temp_session;
		SoupMessage *message;
		GIOStream *cache_stream;
		GMainContext *context;
		gulong cancelled_id = 0;

		if (g_cancellable_set_error_if_cancelled (cancellable, error))
			goto cleanup;

		message = soup_message_new (SOUP_METHOD_GET, use_uri);
		if (!message) {
			g_debug ("%s: Skipping invalid URI '%s'", G_STRFUNC, use_uri);
			goto cleanup;
		}

		context = g_main_context_new ();
		g_main_context_push_thread_default (context);

		temp_session = soup_session_new_with_options (
			SOUP_SESSION_TIMEOUT, 90, NULL);

		proxy_source = e_source_registry_ref_builtin_proxy (e_shell_get_registry (shell));

		g_object_set (
			temp_session,
			SOUP_SESSION_PROXY_RESOLVER,
			G_PROXY_RESOLVER (proxy_source),
			NULL);

		g_object_unref (proxy_source);

		soup_message_headers_append (
			message->request_headers,
			"User-Agent", "Evolution/" VERSION);

		if (cancellable)
			cancelled_id = g_cancellable_connect (cancellable, G_CALLBACK (http_request_cancelled_cb), temp_session, NULL);

		send_and_handle_redirection (temp_session, message, NULL);

		if (cancellable && cancelled_id)
			g_cancellable_disconnect (cancellable, cancelled_id);

		if (!SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
			g_debug ("Failed to request %s (code %d)", use_uri, message->status_code);
			g_object_unref (message);
			g_object_unref (temp_session);
			g_main_context_unref (context);
			goto cleanup;
		}

		/* Write the response body to cache */
		cache_stream = camel_data_cache_add (
			cache, "http", uri_md5, error);
		if (cache_stream) {
			GOutputStream *output_stream;

			output_stream =
				g_io_stream_get_output_stream (cache_stream);

			success = g_output_stream_write_all (
				output_stream,
				message->response_body->data,
				message->response_body->length,
				NULL, cancellable, error);

			g_io_stream_close (cache_stream, NULL, NULL);
			g_object_unref (cache_stream);

			if (success) {
				/* Send the response body to WebKit */
				stream = g_memory_input_stream_new_from_data (
					g_memdup (
						message->response_body->data,
						message->response_body->length),
					message->response_body->length,
					(GDestroyNotify) g_free);

				*out_stream = stream;
				*out_stream_length = message->response_body->length;
				*out_mime_type = g_strdup (
					soup_message_headers_get_content_type (
						message->response_headers, NULL));
			}
		}

		g_object_unref (message);
		g_object_unref (temp_session);
		g_main_context_unref (context);

		d (printf ("Received image from %s\n"
			"Content-Type: %s\n"
			"Content-Length: %d bytes\n"
			"URI MD5: %s:\n",
			use_uri, *out_mime_type ? *out_mime_type : "[null]",
			(gint) *out_stream_length, uri_md5));
	}

 cleanup:
	g_clear_object (&cache);

	g_free (use_uri);
	g_free (uri_md5);
	g_free (mail_uri);
	soup_uri_free (soup_uri);

	return success;
}

static void
e_http_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_http_request_can_process_uri;
	iface->process_sync = e_http_request_process_sync;
}

static void
e_http_request_class_init (EHTTPRequestClass *class)
{
	g_type_class_add_private (class, sizeof (EHTTPRequestPrivate));
}

static void
e_http_request_init (EHTTPRequest *request)
{
	request->priv = G_TYPE_INSTANCE_GET_PRIVATE (request, E_TYPE_HTTP_REQUEST, EHTTPRequestPrivate);
}

EContentRequest *
e_http_request_new (void)
{
	return g_object_new (E_TYPE_HTTP_REQUEST, NULL);
}

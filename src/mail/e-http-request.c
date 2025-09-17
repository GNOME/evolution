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

#include "evolution-config.h"

#include <string.h>

#include <libsoup/soup.h>
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
	G_ADD_PRIVATE (EHTTPRequest)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_http_request_content_request_init))

static gboolean
e_http_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_HTTP_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "evo-http:", 9) == 0 ||
	       g_ascii_strncasecmp (uri, "evo-https:", 10) == 0 ||
	       g_ascii_strncasecmp (uri, "http:", 5) == 0 ||
	       g_ascii_strncasecmp (uri, "https:", 6) == 0;
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
	GUri *guri;
	gchar *evo_uri = NULL, *use_uri;
	gchar *mail_uri = NULL;
	GInputStream *stream;
	gboolean force_load_images = FALSE;
	gboolean disable_remote_content = FALSE;
	EImageLoadingPolicy image_policy;
	gchar *uri_md5;
	EShell *shell;
	GSettings *settings;
	const gchar *user_cache_dir, *soup_query;
	CamelDataCache *cache = NULL;
	GIOStream *cache_stream;
	gint uri_len;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_HTTP_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (guri != NULL, FALSE);

	/* Remove the __evo-mail query */
	soup_query = g_uri_get_query (guri);
	if (soup_query) {
		GHashTable *query;
		gchar *query_str;

		query = soup_form_decode (g_uri_get_query (guri));
		mail_uri = g_hash_table_lookup (query, "__evo-mail");
		if (mail_uri)
			mail_uri = g_strdup (mail_uri);

		g_hash_table_remove (query, "__evo-mail");

		/* Required, because soup_form_encode_hash() can change
		   order of arguments, then the URL checksum doesn't match. */
		evo_uri = g_hash_table_lookup (query, "__evo-original-uri");
		if (evo_uri)
			evo_uri = g_strdup (evo_uri);

		g_hash_table_remove (query, "__evo-original-uri");

		/* Remove __evo-load-images if present (and in such case set
		 * force_load_images to TRUE) */
		force_load_images = g_hash_table_remove (query, "__evo-load-images");

		query_str = soup_form_encode_hash (query);
		e_util_change_uri_component (&guri, SOUP_URI_QUERY, query_str);
		g_hash_table_unref (query);
		g_free (query_str);
	}

	if (!evo_uri)
		evo_uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

	if (camel_debug ("emformat:requests"))
		printf ("%s: Looking for '%s'\n", G_STRFUNC, evo_uri ? evo_uri : "[null]");

	/* Remove the "evo-" prefix from scheme */
	uri_len = (evo_uri != NULL) ? strlen (evo_uri) : 0;
	use_uri = NULL;
	if (evo_uri != NULL && (uri_len > 5)) {
		gint inc = 0;

		if (g_str_has_prefix (evo_uri, "evo-"))
			inc = 4;

		/* Remove trailing "?" if there is no URI query */
		if (evo_uri[uri_len - 1] == '?') {
			use_uri = g_strndup (evo_uri + inc, uri_len - 1 - inc);
		} else {
			use_uri = g_strdup (evo_uri + inc);
		}
	}

	g_free (evo_uri);

	g_return_val_if_fail (use_uri && *use_uri, FALSE);

	*out_stream_length = -1;

	/* Use MD5 hash of the URI as a filname of the resource cache file.
	 * We were previously using the URI as a filename but the URI is
	 * sometimes too long for a filename. */
	uri_md5 = e_http_request_util_compute_uri_checksum (use_uri);
	if (!uri_md5) {
		if (camel_debug ("emformat:requests"))
			printf ("%s: Failed to get hash of URI '%s'\n", G_STRFUNC, use_uri);
		goto cleanup;
	}

	/* Open Evolution's cache */
	user_cache_dir = e_get_user_cache_dir ();
	cache = camel_data_cache_new (user_cache_dir, NULL);
	if (cache) {
		camel_data_cache_set_expire_age (cache, 24 * 60 * 60);
		camel_data_cache_set_expire_access (cache, 2 * 60 * 60);

		cache_stream = camel_data_cache_get (cache, "http", uri_md5, NULL);
	} else
		cache_stream = NULL;

	if (cache_stream != NULL) {
		gssize len;

		stream = g_memory_input_stream_new ();

		len = copy_stream_to_stream (cache_stream, G_MEMORY_INPUT_STREAM (stream), cancellable);
		*out_stream_length = len;

		g_object_unref (cache_stream);

		/* When successfully read some data from cache then
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

				if (camel_debug ("emformat:requests")) {
					printf ("%s: URI '%s' found in cache (%d bytes, %s)\n",
						G_STRFUNC, use_uri, (gint) *out_stream_length, *out_mime_type);
				}
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
	if (!e_shell_get_online (shell)) {
		if (camel_debug ("emformat:requests"))
			printf ("%s: Shell not online, not downloading URI '%s'\n", G_STRFUNC, use_uri);
		goto cleanup;
	}

	if (WEBKIT_IS_WEB_VIEW (requester))
		disable_remote_content = g_strcmp0 (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (requester)), "evo://disable-remote-content") == 0;

	if (disable_remote_content) {
		force_load_images = FALSE;
		image_policy = E_IMAGE_LOADING_POLICY_NEVER;
	} else {
		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		image_policy = g_settings_get_enum (settings, "image-loading-policy");
		g_object_unref (settings);
	}

	/* Item not found in cache, but image loading policy allows us to fetch
	 * it from the interwebs */
	if (!force_load_images && mail_uri != NULL &&
	    (image_policy == E_IMAGE_LOADING_POLICY_SOMETIMES)) {
		CamelObjectBag *registry;
		gchar *decoded_uri;
		EMailPartList *part_list;

		registry = e_mail_part_list_get_registry ();
		decoded_uri = g_uri_unescape_string (mail_uri, NULL);

		if (!decoded_uri)
			part_list = NULL;
		else
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
				if (camel_debug ("emformat:requests"))
					printf ("%s: Failed to check whether sender is a known address, not downloading URI '%s'\n", G_STRFUNC, use_uri);
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
		GInputStream *input_stream;
		gulong cancelled_id = 0;

		if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
			if (camel_debug ("emformat:requests"))
				printf ("%s: Request cancelled, not downloading URI '%s'\n", G_STRFUNC, use_uri);
			goto cleanup;
		}

		message = soup_message_new (SOUP_METHOD_GET, use_uri);
		if (!message) {
			if (camel_debug ("emformat:requests"))
				printf ("%s: URI '%s' is invalid, not downloading it\n", G_STRFUNC, use_uri);
			goto cleanup;
		}

		proxy_source = e_source_registry_ref_builtin_proxy (e_shell_get_registry (shell));

		temp_session = soup_session_new_with_options (
			"timeout", 15,
			"proxy-resolver", proxy_source,
			NULL);

		g_object_unref (proxy_source);

		soup_message_headers_append (
			soup_message_get_request_headers (message),
			"User-Agent", "Evolution/" VERSION);

		if (cancellable)
			cancelled_id = g_cancellable_connect (cancellable, G_CALLBACK (http_request_cancelled_cb), temp_session, NULL);

		soup_message_headers_append (soup_message_get_request_headers (message), "Connection", "close");
		input_stream = soup_session_send (temp_session, message, cancellable, error);

		if (cancellable && cancelled_id)
			g_cancellable_disconnect (cancellable, cancelled_id);

		if (!input_stream || !SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message))) {
			if (camel_debug ("emformat:requests")) {
				const gchar *reason = soup_message_get_reason_phrase (message);

				if (reason && !*reason)
					reason = NULL;

				printf ("%s: Failed to download URI '%s', code: %u%s%s%s\n", G_STRFUNC, use_uri, soup_message_get_status (message),
					reason ? " (" : "", reason ? reason : "", reason ? ")" : "");
			}
			g_clear_object (&input_stream);
			g_object_unref (message);
			g_object_unref (temp_session);
			goto cleanup;
		}

		if (cache) {
			GError *local_error = NULL;

			cache_stream = camel_data_cache_add (
				cache, "http", uri_md5, &local_error);
			if (local_error) {
				g_warning (
					"Failed to create cache file for '%s': %s",
					uri, local_error->message);
				g_clear_error (&local_error);
			} else {
				GOutputStream *output_stream;
				gssize bytes_copied;

				output_stream = g_io_stream_get_output_stream (cache_stream);
				bytes_copied = g_output_stream_splice (output_stream, input_stream, G_OUTPUT_STREAM_SPLICE_NONE, cancellable, &local_error);

				g_io_stream_close (cache_stream, NULL, NULL);
				g_object_unref (cache_stream);

				if (local_error != NULL) {
					if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
						g_warning ("Failed to write data to cache stream: %s", local_error->message);
					g_clear_error (&local_error);
					g_clear_object (&input_stream);
					g_object_unref (message);
					g_object_unref (temp_session);
					goto cleanup;
				}

				if (bytes_copied >= 0) {
					gchar *filename;

					filename = camel_data_cache_get_filename (cache, "http", uri_md5);

					if (filename) {
						GFileInputStream *file_input_stream;
						GFile *file;

						file = g_file_new_for_path (filename);
						file_input_stream = g_file_read (file, cancellable, &local_error);

						g_object_unref (file);

						if (file_input_stream) {
							*out_stream = G_INPUT_STREAM (file_input_stream);
							*out_stream_length = bytes_copied;
							*out_mime_type = g_strdup (soup_message_headers_get_content_type (soup_message_get_response_headers (message), NULL));

							success = TRUE;
						} else {
							g_warning ("Failed to read cache file '%s': %s", filename, local_error ? local_error->message : "Unknown error");
							g_clear_error (&local_error);
						}

						g_free (filename);
					} else {
						g_warning ("Failed to get just written cache file name");
					}
				}
			}
		}

		g_clear_object (&input_stream);
		g_object_unref (message);
		g_object_unref (temp_session);

		if (camel_debug ("emformat:requests")) {
			printf ("%s: Received data from '%s' Content-Type: %s Content-Length: %d bytes URI MD5: %s:\n",
				G_STRFUNC, use_uri, *out_mime_type ? *out_mime_type : "[null]", (gint) *out_stream_length, uri_md5);
		}
	}

 cleanup:
	g_clear_object (&cache);

	if (!success && error && !*error)
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to get resource '%s'", use_uri);

	g_free (use_uri);
	g_free (uri_md5);
	g_free (mail_uri);
	g_uri_unref (guri);

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
}

static void
e_http_request_init (EHTTPRequest *request)
{
	request->priv = e_http_request_get_instance_private (request);
}

EContentRequest *
e_http_request_new (void)
{
	return g_object_new (E_TYPE_HTTP_REQUEST, NULL);
}

/* Computes MD5 checksum of the URI with normalized URI query */
gchar *
e_http_request_util_compute_uri_checksum (const gchar *in_uri)
{
	GString *string;
	GUri *guri;
	const gchar *soup_query;
	gchar *md5, *uri;

	g_return_val_if_fail (in_uri != NULL, NULL);

	guri = g_uri_parse (in_uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (guri != NULL, NULL);

	string = g_string_new ("");

	soup_query = g_uri_get_query (guri);
	if (soup_query) {
		GHashTable *query;
		GList *keys, *link;

		query = soup_form_decode (soup_query);
		keys = g_hash_table_get_keys (query);
		keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
		for (link = keys; link; link = g_list_next (link)) {
			const gchar *key, *value;

			key = link->data;
			if (key && *key) {
				value = g_hash_table_lookup (query, key);
				g_string_append_printf (string, "%s=%s;", key, value ? value : "");
			}
		}
		g_list_free (keys);
		g_hash_table_unref (query);

		e_util_change_uri_component (&guri, SOUP_URI_QUERY, NULL);
	}

	uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
	g_string_append (string, uri ? uri : "");
	g_free (uri);

	/* This is not constructing real URI, only its query parameters in sorted
	   order with the URI part. */
	if (string->len)
		md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, string->str, -1);
	else
		md5 = NULL;

	g_string_free (string, TRUE);
	g_uri_unref (guri);

	return md5;
}

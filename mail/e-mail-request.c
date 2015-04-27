/*
 * e-mail-request.c
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

#include "e-mail-request.h"
#include "em-utils.h"

#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>

#include <webkit/webkit.h>

#include <glib/gi18n.h>
#include <camel/camel.h>

#include "shell/e-shell.h"

#include "em-format/e-mail-formatter.h"
#include "em-format/e-mail-formatter-utils.h"
#include "em-format/e-mail-formatter-print.h"

#include "e-mail-ui-session.h"

#define d(x)
#define dd(x)

#define E_MAIL_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_REQUEST, EMailRequestPrivate))

struct _EMailRequestPrivate {
	GBytes *bytes;
	gchar *mime_type;

	GHashTable *uri_query;
	gchar *uri_base;
	gchar *full_uri;

	gchar *ret_mime_type;
};

static const gchar *data_schemes[] = { "mail", NULL };

G_DEFINE_TYPE (EMailRequest, e_mail_request, SOUP_TYPE_REQUEST)

static void
handle_mail_request (GSimpleAsyncResult *simple,
                     GObject *object,
                     GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	EMailFormatter *formatter;
	EMailPartList *part_list;
	CamelObjectBag *registry;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	const gchar *val;
	const gchar *default_charset, *charset;

	EMailFormatterContext context = { 0 };

	if (g_cancellable_is_cancelled (cancellable))
		return;

	registry = e_mail_part_list_get_registry ();
	part_list = camel_object_bag_get (registry, request->priv->uri_base);

	if (camel_debug_start ("emformat:requests")) {
		printf ("%s: found part-list %p for full_uri '%s'\n", G_STRFUNC, part_list, request->priv->full_uri);
		camel_debug_end ();
	}

	if (!part_list)
		return;

	val = g_hash_table_lookup (
		request->priv->uri_query, "headers_collapsed");
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED;

	val = g_hash_table_lookup (
		request->priv->uri_query, "headers_collapsable");
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE;

	val = g_hash_table_lookup (request->priv->uri_query, "mode");
	if (val != NULL)
		context.mode = atoi (val);

	default_charset = g_hash_table_lookup (
		request->priv->uri_query, "formatter_default_charset");
	charset = g_hash_table_lookup (
		request->priv->uri_query, "formatter_charset");

	context.part_list = g_object_ref (part_list);
	context.uri = request->priv->full_uri;

	if (context.mode == E_MAIL_FORMATTER_MODE_PRINTING)
		formatter = e_mail_formatter_print_new ();
	else
		formatter = e_mail_formatter_new ();

	if (default_charset != NULL && *default_charset != '\0')
		e_mail_formatter_set_default_charset (formatter, default_charset);
	if (charset != NULL && *charset != '\0')
		e_mail_formatter_set_charset (formatter, charset);

	output_stream = g_memory_output_stream_new_resizable ();

	val = g_hash_table_lookup (request->priv->uri_query, "part_id");
	if (val != NULL) {
		EMailPart *part;
		const gchar *mime_type;
		gchar *part_id;

		part_id = soup_uri_decode (val);
		part = e_mail_part_list_ref_part (part_list, part_id);
		if (!part) {
			if (camel_debug_start ("emformat:requests")) {
				printf ("%s: part with id '%s' not found\n", G_STRFUNC, part_id);
				camel_debug_end ();
			}

			g_free (part_id);
			goto no_part;
		}
		g_free (part_id);

		mime_type = g_hash_table_lookup (
			request->priv->uri_query, "mime_type");

		if (context.mode == E_MAIL_FORMATTER_MODE_SOURCE)
			mime_type = "application/vnd.evolution.source";

		if (context.mode == E_MAIL_FORMATTER_MODE_CID) {
			CamelDataWrapper *dw;
			CamelMimePart *mime_part;

			mime_part = e_mail_part_ref_mime_part (part);
			dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
			g_return_if_fail (dw);

			if (!mime_type) {
				g_free (request->priv->mime_type);
				request->priv->mime_type = camel_data_wrapper_get_mime_type (dw);
			}

			camel_data_wrapper_decode_to_output_stream_sync (
				dw, output_stream, cancellable, NULL);

			g_object_unref (mime_part);
		} else {
			if (mime_type == NULL)
				mime_type = e_mail_part_get_mime_type (part);

			e_mail_formatter_format_as (
				formatter, &context, part,
				output_stream, mime_type,
				cancellable);
		}

		g_object_unref (part);

	} else {
		e_mail_formatter_format_sync (
			formatter, part_list, output_stream,
			context.flags, context.mode, cancellable);
	}

 no_part:
	g_clear_object (&context.part_list);

	g_output_stream_close (output_stream, NULL, NULL);

	if (request->priv->bytes != NULL)
		g_bytes_unref (request->priv->bytes);

	request->priv->bytes = g_memory_output_stream_steal_as_bytes (
		G_MEMORY_OUTPUT_STREAM (output_stream));

	if (g_bytes_get_size (request->priv->bytes) == 0) {
		gchar *data;

		g_bytes_unref (request->priv->bytes);

		data = g_strdup_printf (
			"<p align='center'>%s</p>",
			_("The message has no text content."));

		/* Takes ownership of the string. */
		request->priv->bytes = g_bytes_new_take (
			data, strlen (data) + 1);
	}

	input_stream =
		g_memory_input_stream_new_from_bytes (request->priv->bytes);

	g_simple_async_result_set_op_res_gpointer (
		simple, g_object_ref (input_stream),
		(GDestroyNotify) g_object_unref);

	g_object_unref (input_stream);
	g_object_unref (output_stream);

	g_object_unref (part_list);
	g_object_unref (formatter);
}

static GInputStream *
get_empty_image_stream (void)
{
	GdkPixbuf *pixbuf;
	gchar *buffer;
	gsize length;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
	gdk_pixbuf_fill (pixbuf, 0x00000000); /* transparent black */
	gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &length, "png", NULL, NULL);
	g_object_unref (pixbuf);

	return g_memory_input_stream_new_from_data (buffer, length, g_free);
}

static void
handle_contact_photo_request (GSimpleAsyncResult *simple,
                              GObject *object,
                              GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *mail_backend;
	EMailSession *mail_session;
	EPhotoCache *photo_cache;
	CamelInternetAddress *cia;
	GInputStream *stream = NULL;
	const gchar *email_address;
	const gchar *escaped_string;
	gchar *unescaped_string;
	GError *error = NULL;

	/* XXX Is this really the only way to obtain
	 *     the mail session instance from here? */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	mail_backend = E_MAIL_BACKEND (shell_backend);
	mail_session = e_mail_backend_get_session (mail_backend);

	photo_cache = e_mail_ui_session_get_photo_cache (
		E_MAIL_UI_SESSION (mail_session));

	request->priv->mime_type = g_strdup ("image/*");

	escaped_string = g_hash_table_lookup (
		request->priv->uri_query, "mailaddr");
	if (escaped_string == NULL || *escaped_string == '\0')
		goto exit;

	cia = camel_internet_address_new ();

	unescaped_string = g_uri_unescape_string (escaped_string, NULL);
	camel_address_decode (CAMEL_ADDRESS (cia), unescaped_string);
	g_free (unescaped_string);

	if (camel_internet_address_get (cia, 0, NULL, &email_address))
		e_photo_cache_get_photo_sync (
			photo_cache, email_address,
			cancellable, &stream, &error);

	g_object_unref (cia);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
	} else if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}

exit:
	if (stream == NULL)
		stream = get_empty_image_stream ();

	g_simple_async_result_set_op_res_gpointer (
		simple, g_object_ref (stream),
		(GDestroyNotify) g_object_unref);

	g_object_unref (stream);
}

static void
mail_request_finalize (GObject *object)
{
	EMailRequestPrivate *priv;

	priv = E_MAIL_REQUEST_GET_PRIVATE (object);

	if (priv->bytes != NULL)
		g_bytes_unref (priv->bytes);

	if (priv->uri_query != NULL)
		g_hash_table_destroy (priv->uri_query);

	g_free (priv->mime_type);
	g_free (priv->uri_base);
	g_free (priv->full_uri);
	g_free (priv->ret_mime_type);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_request_parent_class)->finalize (object);
}

static gboolean
mail_request_check_uri (SoupRequest *request,
                       SoupURI *uri,
                       GError **error)
{
	return (strcmp (uri->scheme, "mail") == 0);
}

static void
mail_request_send_async (SoupRequest *request,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	EMailRequestPrivate *priv;
	GSimpleAsyncResult *simple;
	SoupURI *uri;

	priv = E_MAIL_REQUEST_GET_PRIVATE (request);

	uri = soup_request_get_uri (request);

	d (printf ("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	if (uri->query) {
		priv->uri_query = soup_form_decode (uri->query);
	} else {
		priv->uri_query = NULL;
	}

	priv->full_uri = soup_uri_to_string (uri, FALSE);
	priv->uri_base = g_strdup_printf (
		"%s://%s%s", uri->scheme, uri->host, uri->path);

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback,
		user_data, mail_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	if (g_strcmp0 (uri->host, "contact-photo") == 0) {
		e_util_run_simple_async_result_in_thread (
			simple, handle_contact_photo_request,
			cancellable);
	} else {
		/* Process e-mail mail requests in this thread, which should be
		 * the main/UI thread, because any EMailFormatter can create
		 * GtkWidget-s, or manipulate with them, which should be always
		 * done in the main/UI thread. */
		handle_mail_request (simple, G_OBJECT (request), cancellable);
		g_simple_async_result_complete_in_idle (simple);
	}

	g_object_unref (simple);
}

static GInputStream *
mail_request_send_finish (SoupRequest *request,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	GInputStream *stream;

	simple = G_SIMPLE_ASYNC_RESULT (result);
	stream = g_simple_async_result_get_op_res_gpointer (simple);

	/* Reset the stream before passing it back to webkit */
	if (G_IS_SEEKABLE (stream))
		g_seekable_seek (
			G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, NULL);

	if (stream == NULL) {
		/* We must always return something */
		stream = g_memory_input_stream_new ();
	} else {
		g_object_ref (stream);
	}

	return stream;
}

static goffset
mail_request_get_content_length (SoupRequest *request)
{
	EMailRequestPrivate *priv;
	goffset content_length = -1;  /* -1 means unknown */

	priv = E_MAIL_REQUEST_GET_PRIVATE (request);

	if (priv->bytes != NULL)
		content_length = g_bytes_get_size (priv->bytes);

	return content_length;
}

static const gchar *
mail_request_get_content_type (SoupRequest *request)
{
	EMailRequestPrivate *priv;
	gchar *mime_type;

	priv = E_MAIL_REQUEST_GET_PRIVATE (request);

	if (priv->mime_type != NULL) {
		mime_type = g_strdup (priv->mime_type);
	} else {
		mime_type = g_strdup ("text/html");
	}

	if (g_strcmp0 (mime_type, "text/html") == 0) {
		priv->ret_mime_type = g_strconcat (
			mime_type, "; charset=\"UTF-8\"", NULL);
		g_free (mime_type);
	} else {
		priv->ret_mime_type = mime_type;
	}

	d (printf ("Content-Type: %s\n", priv->ret_mime_type));

	return priv->ret_mime_type;
}

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


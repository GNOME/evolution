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
				request->priv->output_stream, &info, cancellable);
		} else {
			g_warning ("Failed to lookup requested part '%s' - this should not happen!", part_id);
		}

		g_free (part_id);
	} else {
		if (info.mode == 0)
			info.mode = EM_FORMAT_WRITE_MODE_NORMAL;

		em_format_write (emf, request->priv->output_stream, &info, cancellable);
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

	if (request->priv->efh) {
		g_object_unref (request->priv->efh);
		request->priv->efh = NULL;
	}

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
	SoupSession *session;
	EMailRequest *emr = E_MAIL_REQUEST (request);
	GSimpleAsyncResult *simple;
	SoupURI *uri;
	GHashTable *formatters;
	gchar *uri_str;

	session = soup_request_get_session (request);
	uri = soup_request_get_uri (request);

	d(printf("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	if (uri->query) {
		emr->priv->uri_query = soup_form_decode (uri->query);
	} else {
		emr->priv->uri_query = NULL;
	}

	formatters = g_object_get_data (G_OBJECT (session), "formatters");
					g_return_if_fail (formatters != NULL);

	uri_str = g_strdup_printf (
		"%s://%s%s", uri->scheme, uri->host, uri->path);
	emr->priv->efh = g_hash_table_lookup (formatters, uri_str);
	g_free (uri_str);

	g_return_if_fail (emr->priv->efh);

	/* Make sure the formatter lives until we are finished here */
	g_object_ref (emr->priv->efh);

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback,
		user_data, mail_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, handle_mail_request,
		G_PRIORITY_DEFAULT, cancellable);
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

	if (!stream) /* We must always return something */
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

static const char *data_schemes[] = { "mail", NULL };

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


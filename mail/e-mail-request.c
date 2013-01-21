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
#include "em-utils.h"

#include <libsoup/soup.h>
#include <libsoup/soup-requester.h>
#include <libsoup/soup-request-http.h>

#include <webkit/webkit.h>

#include <glib/gi18n.h>
#include <camel/camel.h>

#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-formatter-print.h>

#include <e-util/e-icon-factory.h>
#include <e-util/e-util.h>

#include <shell/e-shell.h>

#define d(x)
#define dd(x)

#define E_MAIL_REQUEST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_REQUEST, EMailRequestPrivate))

struct _EMailRequestPrivate {
	CamelStream *output_stream;
	gchar *mime_type;

	gint content_length;

	GHashTable *uri_query;
	gchar *uri_base;
	gchar *full_uri;

        gchar *ret_mime_type;
};

G_DEFINE_TYPE (EMailRequest, e_mail_request, SOUP_TYPE_REQUEST)

static void
handle_mail_request (GSimpleAsyncResult *res,
                     GObject *object,
                     GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	GInputStream *stream;
	EMailFormatter *formatter;
	EMailPartList *part_list;
	CamelObjectBag *registry;
	GByteArray *ba;
	gchar *part_id;
	gchar *val;
	const gchar *default_charset, *charset;

	EMailFormatterContext context = { 0 };

	if (g_cancellable_is_cancelled (cancellable))
		return;

	if (request->priv->output_stream != NULL) {
		g_object_unref (request->priv->output_stream);
	}

	registry = e_mail_part_list_get_registry ();
	part_list = camel_object_bag_get (registry, request->priv->uri_base);
	g_return_if_fail (part_list != NULL);

	request->priv->output_stream = camel_stream_mem_new ();

	val = g_hash_table_lookup (request->priv->uri_query, "headers_collapsed");
	if (val && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED;

	val = g_hash_table_lookup (request->priv->uri_query, "headers_collapsable");
	if (val && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE;

	val = g_hash_table_lookup (request->priv->uri_query, "mode");
	if (val)
		context.mode = atoi (val);

	context.message = part_list->message;
	context.message_uid = part_list->message_uid;
	context.folder = part_list->folder;
	context.parts = part_list->list;
	context.uri = request->priv->full_uri;

	default_charset = g_hash_table_lookup (request->priv->uri_query, "formatter_default_charset");
	charset = g_hash_table_lookup (request->priv->uri_query, "formatter_charset");

	if (context.mode == E_MAIL_FORMATTER_MODE_PRINTING)
		formatter = e_mail_formatter_print_new ();
	else
		formatter = e_mail_formatter_new ();

	if (default_charset && *default_charset)
		e_mail_formatter_set_default_charset (formatter, default_charset);
	if (charset && *charset)
		e_mail_formatter_set_charset (formatter, charset);

	part_id = g_hash_table_lookup (request->priv->uri_query, "part_id");
	if (part_id) {
		EMailPart *part;
		const gchar *mime_type;
		/* original part_id is owned by the GHashTable */
		part_id = soup_uri_decode (part_id);
		part = e_mail_part_list_find_part (part_list, part_id);

		val = g_hash_table_lookup (request->priv->uri_query, "mime_type");
		if (val) {
			mime_type = val;
		} else {
			mime_type = NULL;
		}

		if (context.mode == E_MAIL_FORMATTER_MODE_SOURCE) {
			mime_type = "application/vnd.evolution.source";
		}

		if (part) {
			CamelContentType *content_type;

			content_type = camel_mime_part_get_content_type (part->part);

			if (context.mode == E_MAIL_FORMATTER_MODE_RAW && content_type &&
			    camel_content_type_is (content_type, "text", "*") &&
			    !camel_content_type_is (content_type, "text", "plain") &&
			    !camel_content_type_is (content_type, "text", "html")) {
				CamelDataWrapper *dw;
				CamelStream *raw_content;
				GByteArray *ba;

				dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
				g_return_if_fail (dw);

				raw_content = camel_stream_mem_new ();
				camel_data_wrapper_decode_to_stream_sync (dw, raw_content, cancellable, NULL);
				ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (raw_content));

				camel_stream_write (request->priv->output_stream, (gchar *) ba->data, ba->len, cancellable, NULL);

				g_object_unref (raw_content);
			} else {
				e_mail_formatter_format_as (
					formatter, &context, part, request->priv->output_stream,
					mime_type ? mime_type : part->mime_type, cancellable);
			}
		} else {
			g_warning ("Failed to lookup requested part '%s' - this should not happen!", part_id);
		}

	} else {
		e_mail_formatter_format_sync (
			formatter, part_list, request->priv->output_stream,
			context.flags, context.mode, cancellable);
	}

	/* Convert the GString to GInputStream and send it back to WebKit */
	ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (request->priv->output_stream));
	if (!ba->data) {
		gchar *data = g_strdup_printf (_("Failed to load part '%s'"), part_id);
		dd (printf ("%s", data));
		g_byte_array_append (ba, (guchar *) data, strlen (data));
		g_free (data);
	} else {
		dd ({
			gchar *d = g_strndup ((gchar *) ba->data, ba->len);
			printf ("%s", d);
			g_free (d);
		});
	}

	g_free (part_id);
	g_object_unref (part_list);
	g_object_unref (formatter);

	stream = g_memory_input_stream_new_from_data (
			(gchar *) ba->data, ba->len, NULL);
	g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
}

static GInputStream *
get_empty_image_stream (gsize *len)
{
	GdkPixbuf *p;
	gchar *buff;
	GInputStream *stream;

	p = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
	gdk_pixbuf_fill (p, 0x00000000);	/* transparent black */
	gdk_pixbuf_save_to_buffer (p, &buff, len, "png", NULL, NULL);

	stream = g_memory_input_stream_new_from_data (buff, *len, g_free);

	g_object_unref (p);

	return stream;
}

static void
handle_contact_photo_request (GSimpleAsyncResult *res,
                              GObject *object,
                              GCancellable *cancellable)
{
	EMailRequest *request = E_MAIL_REQUEST (object);
	const gchar *email;
	gchar *photo_name;
	gboolean only_local_photo;
	CamelMimePart *photopart;
	EShell *shell;
	ESourceRegistry *registry;
	CamelInternetAddress *cia;
	CamelDataWrapper *dw;
	GByteArray *ba;
	GInputStream *stream = NULL;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	request->priv->mime_type = g_strdup ("image/*");

	email = g_hash_table_lookup (
			request->priv->uri_query, "mailaddr");
	if (!email || !*email) {
		gsize len;

		stream = get_empty_image_stream (&len);
		request->priv->content_length = len;

		g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
		return;
	}

	photo_name = g_uri_unescape_string (email, NULL);
	only_local_photo = g_hash_table_lookup_extended (
				request->priv->uri_query, "only-local-photo",
				NULL, NULL);

	cia = camel_internet_address_new ();
	camel_address_decode ((CamelAddress *) cia, (const gchar *) photo_name);
	photopart = em_utils_contact_photo (
			registry, cia, only_local_photo, cancellable);
	g_object_unref (cia);
	if (!photopart) {
		gsize len;

		stream = get_empty_image_stream (&len);
		request->priv->content_length = len;

		g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
		g_free (photo_name);
		return;
	}

	ba = NULL;
	dw = camel_medium_get_content (CAMEL_MEDIUM (photopart));
	if (dw) {
		ba = camel_data_wrapper_get_byte_array (dw);
	}

	if (!ba || ba->len == 0) {

		const gchar *filename = camel_mime_part_get_filename (photopart);

		if (filename && *filename &&
		    g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			gchar *data;
			gsize len;

			if (!g_file_get_contents (filename, &data, &len, NULL)) {
				stream = get_empty_image_stream (&len);
			} else {
				stream = g_memory_input_stream_new_from_data (
					(gchar *) data, len, g_free);
			}

			request->priv->content_length = len;
		}

	} else {

		stream = g_memory_input_stream_new_from_data (
				(gchar *) ba->data, ba->len, NULL);

		request->priv->content_length = ba->len;

	}

	g_free (photo_name);
	g_simple_async_result_set_op_res_gpointer (res, stream, NULL);
}

static void
mail_request_finalize (GObject *object)
{
	EMailRequest *request = E_MAIL_REQUEST (object);

	g_clear_object (&request->priv->output_stream);

	g_free (request->priv->mime_type);
	request->priv->mime_type = NULL;

	if (request->priv->uri_query) {
		g_hash_table_destroy (request->priv->uri_query);
		request->priv->uri_query = NULL;
	}

	g_free (request->priv->ret_mime_type);
	request->priv->ret_mime_type = NULL;

	g_free (request->priv->uri_base);
	request->priv->uri_base = NULL;

	g_free (request->priv->full_uri);
	request->priv->full_uri = NULL;

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
	EMailRequest *emr = E_MAIL_REQUEST (request);
	GSimpleAsyncResult *simple;
	SoupURI *uri;
	gchar *uri_str;

	uri = soup_request_get_uri (request);

	d (printf ("received request for %s\n", soup_uri_to_string (uri, FALSE)));

	if (uri->query) {
		emr->priv->uri_query = soup_form_decode (uri->query);
	} else {
		emr->priv->uri_query = NULL;
	}

	emr->priv->full_uri = soup_uri_to_string (uri, FALSE);
	uri_str = g_strdup_printf (
		"%s://%s%s", uri->scheme, uri->host, uri->path);
	emr->priv->uri_base = uri_str;

	simple = g_simple_async_result_new (
		G_OBJECT (request), callback,
		user_data, mail_request_send_async);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	if (g_strcmp0 (uri->host, "contact-photo") == 0) {
		g_simple_async_result_run_in_thread (
			simple, handle_contact_photo_request,
			G_PRIORITY_DEFAULT, cancellable);
	} else {
		g_simple_async_result_run_in_thread (
			simple, handle_mail_request,
			G_PRIORITY_DEFAULT, cancellable);
	}

	g_object_unref (simple);
}

static GInputStream *
mail_request_send_finish (SoupRequest *request,
                          GAsyncResult *result,
                          GError **error)
{
	GInputStream *stream;

	stream = g_simple_async_result_get_op_res_gpointer (
					G_SIMPLE_ASYNC_RESULT (result));

	/* Reset the stream before passing it back to webkit */
	if (G_IS_INPUT_STREAM (stream) && G_IS_SEEKABLE (stream))
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

	d (printf ("Content-Length: %d bytes\n", content_length));
	return content_length;
}

static const gchar *
mail_request_get_content_type (SoupRequest *request)
{
	EMailRequest *emr = E_MAIL_REQUEST (request);
	gchar *mime_type;

	if (emr->priv->mime_type) {
		mime_type = g_strdup (emr->priv->mime_type);
	} else {
		mime_type = g_strdup ("text/html");
	}

	if (g_strcmp0 (mime_type, "text/html") == 0) {
		emr->priv->ret_mime_type = g_strconcat (mime_type, "; charset=\"UTF-8\"", NULL);
		g_free (mime_type);
	} else {
		emr->priv->ret_mime_type = mime_type;
	}

	d (printf ("Content-Type: %s\n", emr->priv->ret_mime_type));

	return emr->priv->ret_mime_type;
}

static const gchar *data_schemes[] = { "mail", NULL };

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


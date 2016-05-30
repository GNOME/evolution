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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libsoup/soup.h>

#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "shell/e-shell.h"

#include "em-format/e-mail-formatter.h"
#include "em-format/e-mail-formatter-utils.h"
#include "em-format/e-mail-formatter-print.h"

#include "em-utils.h"
#include "e-mail-ui-session.h"
#include "e-mail-request.h"

#define d(x)

struct _EMailRequestPrivate {
	gint dummy;
};

static void e_mail_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailRequest, e_mail_request, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_mail_request_content_request_init))

static gboolean
e_mail_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_MAIL_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "mail:", 5) == 0;
}

static gboolean
mail_request_process_mail_sync (EContentRequest *request,
				SoupURI *suri,
				GHashTable *uri_query,
				GObject *requester,
				GInputStream **out_stream,
				gint64 *out_stream_length,
				gchar **out_mime_type,
				GCancellable *cancellable,
				GError **error)
{
	EMailFormatter *formatter;
	EMailPartList *part_list;
	CamelObjectBag *registry;
	GOutputStream *output_stream;
	GBytes *bytes;
	gchar *tmp, *use_mime_type = NULL;
	const gchar *val;
	const gchar *default_charset, *charset;
	gboolean part_converted_to_utf8 = FALSE;

	EMailFormatterContext context = { 0 };

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	tmp = g_strdup_printf ("%s://%s%s", suri->scheme, suri->host, suri->path);

	registry = e_mail_part_list_get_registry ();
	part_list = camel_object_bag_get (registry, tmp);

	g_free (tmp);

	context.uri = soup_uri_to_string (suri, FALSE);

	if (camel_debug_start ("emformat:requests")) {
		printf ("%s: found part-list %p for full_uri '%s'\n", G_STRFUNC, part_list, context.uri);
		camel_debug_end ();
	}

	if (!part_list) {
		g_free (context.uri);
		return FALSE;
	}

	val = g_hash_table_lookup (uri_query, "headers_collapsed");
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED;

	val = g_hash_table_lookup (uri_query, "headers_collapsable");
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE;

	val = g_hash_table_lookup (uri_query, "mode");
	if (val != NULL)
		context.mode = atoi (val);

	default_charset = g_hash_table_lookup (uri_query, "formatter_default_charset");
	charset = g_hash_table_lookup (uri_query, "formatter_charset");

	context.part_list = g_object_ref (part_list);

	if (context.mode == E_MAIL_FORMATTER_MODE_PRINTING)
		formatter = e_mail_formatter_print_new ();
	else
		formatter = e_mail_formatter_new ();

	if (default_charset != NULL && *default_charset != '\0')
		e_mail_formatter_set_default_charset (formatter, default_charset);
	if (charset != NULL && *charset != '\0')
		e_mail_formatter_set_charset (formatter, charset);

	output_stream = g_memory_output_stream_new_resizable ();

	val = g_hash_table_lookup (uri_query, "part_id");
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

		mime_type = g_hash_table_lookup (uri_query, "mime_type");

		if (context.mode == E_MAIL_FORMATTER_MODE_SOURCE)
			mime_type = "application/vnd.evolution.source";

		if (mime_type == NULL)
			mime_type = e_mail_part_get_mime_type (part);

		e_mail_formatter_format_as (
			formatter, &context, part,
			output_stream, mime_type,
			cancellable);

		part_converted_to_utf8 = e_mail_part_get_converted_to_utf8 (part);

		g_object_unref (part);

	} else {
		e_mail_formatter_format_sync (
			formatter, part_list, output_stream,
			context.flags, context.mode, cancellable);
	}

 no_part:
	g_clear_object (&context.part_list);

	g_output_stream_close (output_stream, NULL, NULL);

	bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output_stream));

	if (g_bytes_get_size (bytes) == 0) {
		gchar *data;

		g_bytes_unref (bytes);

		data = g_strdup_printf (
			"<p align='center'>%s</p>",
			_("The message has no text content."));

		/* Takes ownership of the string. */
		bytes = g_bytes_new_take (data, strlen (data) + 1);
	}

	if (!use_mime_type)
		use_mime_type = g_strdup ("text/html");

	if (part_converted_to_utf8 && g_strcmp0 (use_mime_type, "text/html") == 0) {
		tmp = g_strconcat (use_mime_type, "; charset=\"UTF-8\"", NULL);
		g_free (use_mime_type);
		use_mime_type = tmp;
	}

	*out_stream = g_memory_input_stream_new_from_bytes (bytes);
	*out_stream_length = g_bytes_get_size (bytes);
	*out_mime_type = use_mime_type;

	g_object_unref (output_stream);
	g_object_unref (part_list);
	g_object_unref (formatter);
	g_bytes_unref (bytes);
	g_free (context.uri);

	return TRUE;
}

static gboolean
mail_request_process_contact_photo_sync (EContentRequest *request,
					 SoupURI *suri,
					 GHashTable *uri_query,
					 GObject *requester,
					 GInputStream **out_stream,
					 gint64 *out_stream_length,
					 gchar **out_mime_type,
					 GCancellable *cancellable,
					 GError **error)
{
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
	gboolean success = FALSE;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	mail_backend = E_MAIL_BACKEND (shell_backend);
	mail_session = e_mail_backend_get_session (mail_backend);

	photo_cache = e_mail_ui_session_get_photo_cache (E_MAIL_UI_SESSION (mail_session));

	escaped_string = g_hash_table_lookup (uri_query, "mailaddr");
	if (escaped_string && *escaped_string) {
		cia = camel_internet_address_new ();

		unescaped_string = g_uri_unescape_string (escaped_string, NULL);
		camel_address_decode (CAMEL_ADDRESS (cia), unescaped_string);
		g_free (unescaped_string);

		if (camel_internet_address_get (cia, 0, NULL, &email_address))
			success = e_photo_cache_get_photo_sync (
				photo_cache, email_address,
				cancellable, &stream, error);

		g_object_unref (cia);

		if (success) {
			*out_stream = stream;
			*out_stream_length = -1;
			*out_mime_type = g_strdup ("image/*");
		}
	}

	if (!success) {
		GdkPixbuf *pixbuf;
		gchar *buffer;
		gsize length;

		g_clear_error (error);

		/* Construct empty image stream, to not show "broken image" icon when no contact photo is found */
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
		gdk_pixbuf_fill (pixbuf, 0x00000000); /* transparent black */
		gdk_pixbuf_save_to_buffer (pixbuf, &buffer, &length, "png", NULL, NULL);
		g_object_unref (pixbuf);

		*out_stream = g_memory_input_stream_new_from_data (buffer, length, g_free);
		*out_stream_length = length;
		*out_mime_type = g_strdup ("image/png");
	}

	return TRUE;
}

typedef struct _MailIdleData
{
	EContentRequest *request;
	SoupURI *suri;
	GHashTable *uri_query;
	GObject *requester;
	GInputStream **out_stream;
	gint64 *out_stream_length;
	gchar **out_mime_type;
	GCancellable *cancellable;
	GError **error;

	gboolean success;
	EFlag *flag;
} MailIdleData;

static gboolean
process_mail_request_idle_cb (gpointer user_data)
{
	MailIdleData *mid = user_data;

	g_return_val_if_fail (mid != NULL, FALSE);
	g_return_val_if_fail (E_IS_MAIL_REQUEST (mid->request), FALSE);
	g_return_val_if_fail (mid->suri != NULL, FALSE);
	g_return_val_if_fail (mid->flag != NULL, FALSE);

	mid->success = mail_request_process_mail_sync (mid->request,
		mid->suri, mid->uri_query, mid->requester, mid->out_stream,
		mid->out_stream_length, mid->out_mime_type,
		mid->cancellable, mid->error);

	e_flag_set (mid->flag);

	return FALSE;
}

static gboolean
e_mail_request_process_sync (EContentRequest *request,
			     const gchar *uri,
			     GObject *requester,
			     GInputStream **out_stream,
			     gint64 *out_stream_length,
			     gchar **out_mime_type,
			     GCancellable *cancellable,
			     GError **error)
{
	SoupURI *suri;
	GHashTable *uri_query;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_MAIL_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	suri = soup_uri_new (uri);
	g_return_val_if_fail (suri != NULL, FALSE);

	if (suri->query) {
		uri_query = soup_form_decode (suri->query);
	} else {
		uri_query = NULL;
	}

	if (g_strcmp0 (suri->host, "contact-photo") == 0) {
		success = mail_request_process_contact_photo_sync (request, suri, uri_query, requester,
			out_stream, out_stream_length, out_mime_type, cancellable, error);
	} else {
		MailIdleData mid;

		mid.request = request;
		mid.suri = suri;
		mid.uri_query = uri_query;
		mid.requester = requester;
		mid.out_stream = out_stream;
		mid.out_stream_length = out_stream_length;
		mid.out_mime_type = out_mime_type;
		mid.cancellable = cancellable;
		mid.error = error;
		mid.flag = e_flag_new ();
		mid.success = FALSE;

		if (e_util_is_main_thread (NULL)) {
			process_mail_request_idle_cb (&mid);
		} else {
			/* Process e-mail mail requests in this thread, which should be
			 * the main/UI thread, because any EMailFormatter can create
			 * GtkWidget-s, or manipulate with them, which should be always
			 * done in the main/UI thread. */
			g_idle_add_full (
				G_PRIORITY_HIGH_IDLE,
				process_mail_request_idle_cb,
				&mid, NULL);

			e_flag_wait (mid.flag);
		}

		e_flag_free (mid.flag);

		success = mid.success;
	}

	if (uri_query)
		g_hash_table_destroy (uri_query);
	soup_uri_free (suri);

	return success;
}

static void
e_mail_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_mail_request_can_process_uri;
	iface->process_sync = e_mail_request_process_sync;
}

static void
e_mail_request_class_init (EMailRequestClass *class)
{
	g_type_class_add_private (class, sizeof (EMailRequestPrivate));
}

static void
e_mail_request_init (EMailRequest *request)
{
	request->priv = G_TYPE_INSTANCE_GET_PRIVATE (request, E_TYPE_MAIL_REQUEST, EMailRequestPrivate);
}

EContentRequest *
e_mail_request_new (void)
{
	return g_object_new (E_TYPE_MAIL_REQUEST, NULL);
}

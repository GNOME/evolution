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

#include "evolution-config.h"

#include <libsoup/soup.h>

#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "shell/e-shell.h"

#include "em-format/e-mail-formatter.h"
#include "em-format/e-mail-formatter-utils.h"
#include "em-format/e-mail-formatter-print.h"

#include "em-utils.h"
#include "e-mail-display.h"
#include "e-mail-ui-session.h"
#include "e-mail-request.h"

#define d(x)

struct _EMailRequestPrivate {
	gint scale_factor;
};

enum {
	PROP_0,
	PROP_SCALE_FACTOR
};

static void e_mail_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailRequest, e_mail_request, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EMailRequest)
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_mail_request_content_request_init))

static gboolean
e_mail_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	g_return_val_if_fail (E_IS_MAIL_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	return g_ascii_strncasecmp (uri, "mail:", 5) == 0;
}

static void
save_gicon_to_stream (GIcon *icon,
		      gint size,
		      GOutputStream *output_stream,
		      gchar **out_mime_type)
{
	GtkIconInfo *icon_info;
	GdkPixbuf *pixbuf;

	if (size < 16)
		size = 16;

	icon_info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (), icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
	if (!icon_info)
		return;

	pixbuf = gtk_icon_info_load_icon (icon_info, NULL);
	if (pixbuf) {
		if (gdk_pixbuf_save_to_stream (
			pixbuf, output_stream,
			"png", NULL, NULL, NULL)) {
			*out_mime_type = g_strdup ("image/png");
		}
		g_object_unref (pixbuf);
	}

	g_object_unref (icon_info);
}

static gboolean
mail_request_process_mail_sync (EContentRequest *request,
				GUri *guri,
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

	tmp = g_strdup_printf ("%s://%s%s", g_uri_get_scheme (guri), g_uri_get_host (guri), g_uri_get_path (guri));

	registry = e_mail_part_list_get_registry ();
	part_list = camel_object_bag_get (registry, tmp);

	g_free (tmp);

	if (!part_list && E_IS_MAIL_DISPLAY (requester) &&
	    e_mail_display_get_mode (E_MAIL_DISPLAY (requester)) == E_MAIL_FORMATTER_MODE_SOURCE) {
		part_list = e_mail_display_get_part_list (E_MAIL_DISPLAY (requester));
		if (part_list)
			g_object_ref (part_list);
	}

	context.uri = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

	if (camel_debug_start ("emformat:requests")) {
		printf ("%s: found part-list %p for full_uri '%s'\n", G_STRFUNC, part_list, context.uri);
		camel_debug_end ();
	}

	if (!part_list) {
		g_free (context.uri);
		return FALSE;
	}

	val = uri_query ? g_hash_table_lookup (uri_query, "headers_collapsed") : NULL;
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED;

	val = uri_query ? g_hash_table_lookup (uri_query, "headers_collapsable") : NULL;
	if (val != NULL && atoi (val) == 1)
		context.flags |= E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE;

	val = uri_query ? g_hash_table_lookup (uri_query, "mode") : NULL;
	if (val != NULL)
		context.mode = atoi (val);

	default_charset = uri_query ? g_hash_table_lookup (uri_query, "formatter_default_charset") : NULL;
	charset = uri_query ? g_hash_table_lookup (uri_query, "formatter_charset") : NULL;

	context.part_list = g_object_ref (part_list);

	if (context.mode == E_MAIL_FORMATTER_MODE_PRINTING)
		formatter = e_mail_formatter_print_new ();
	else if (E_IS_MAIL_DISPLAY (requester))
		formatter = g_object_ref (e_mail_display_get_formatter (E_MAIL_DISPLAY (requester)));
	else
		formatter = e_mail_formatter_new ();

	if (default_charset != NULL && *default_charset != '\0')
		e_mail_formatter_set_default_charset (formatter, default_charset);
	if (charset != NULL && *charset != '\0')
		e_mail_formatter_set_charset (formatter, charset);

	output_stream = g_memory_output_stream_new_resizable ();

	val = uri_query ? g_hash_table_lookup (uri_query, "attachment_icon") : NULL;
	if (val) {
		gchar *attachment_id;

		attachment_id = g_uri_unescape_string (val, NULL);
		if (attachment_id && E_IS_MAIL_DISPLAY (requester)) {
			EMailDisplay *mail_display = E_MAIL_DISPLAY (requester);
			EAttachmentStore *attachment_store;
			GList *attachments, *link;

			attachment_store = e_mail_display_get_attachment_store (mail_display);
			attachments = e_attachment_store_get_attachments (attachment_store);
			for (link = attachments; link; link = g_list_next (link)) {
				EAttachment *attachment = link->data;
				gboolean can_use;

				tmp = g_strdup_printf ("%p", attachment);
				can_use = g_strcmp0 (tmp, attachment_id) == 0;
				g_free (tmp);

				if (can_use) {
					GtkTreeIter iter;

					if (e_attachment_store_find_attachment_iter (attachment_store, attachment, &iter)) {
						GIcon *icon = NULL;

						gtk_tree_model_get (GTK_TREE_MODEL (attachment_store), &iter,
							E_ATTACHMENT_STORE_COLUMN_ICON, &icon,
							-1);

						if (icon) {
							const gchar *size = g_hash_table_lookup (uri_query, "size");
							gint scale_factor;

							if (!size)
								size = "16";

							scale_factor = e_mail_request_get_scale_factor (E_MAIL_REQUEST (request));

							if (scale_factor < 1)
								scale_factor = 1;

							save_gicon_to_stream (icon, atoi (size) * scale_factor, output_stream, &use_mime_type);

							g_object_unref (icon);
						}
					}

					break;
				}
			}

			g_list_free_full (attachments, g_object_unref);
		}

		g_free (attachment_id);

		goto no_part;
	}

	val = uri_query ? g_hash_table_lookup (uri_query, "part_id") : NULL;
	if (val != NULL) {
		EMailPart *part;
		const gchar *mime_type;
		gchar *part_id;

		part_id = g_uri_unescape_string (val, NULL);
		part = part_id ? e_mail_part_list_ref_part (part_list, part_id) : NULL;
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
					 GUri *guri,
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

	escaped_string = uri_query ? g_hash_table_lookup (uri_query, "mailaddr") : NULL;
	if (escaped_string && *escaped_string) {
		cia = camel_internet_address_new ();

		unescaped_string = g_uri_unescape_string (escaped_string, NULL);
		camel_address_decode (CAMEL_ADDRESS (cia), unescaped_string);
		g_free (unescaped_string);

		if (camel_internet_address_get (cia, 0, NULL, &email_address)) {
			/* The e_photo_cache_get_photo_sync() can return TRUE even when
			   there is no picture of the found contact, thus check for it. */
			success = e_photo_cache_get_photo_sync (
				photo_cache, email_address,
				cancellable, &stream, error) && stream;
		}

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
	GUri *guri;
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
	g_return_val_if_fail (mid->guri != NULL, FALSE);
	g_return_val_if_fail (mid->flag != NULL, FALSE);

	mid->success = mail_request_process_mail_sync (mid->request,
		mid->guri, mid->uri_query, mid->requester, mid->out_stream,
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
	GUri *guri;
	GHashTable *uri_query;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_MAIL_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_val_if_fail (guri != NULL, FALSE);

	if (g_uri_get_query (guri)) {
		uri_query = soup_form_decode (g_uri_get_query (guri));
	} else {
		uri_query = NULL;
	}

	if (g_strcmp0 (g_uri_get_host (guri), "contact-photo") == 0) {
		success = mail_request_process_contact_photo_sync (request, guri, uri_query, requester,
			out_stream, out_stream_length, out_mime_type, cancellable, error);
	} else {
		MailIdleData mid;

		mid.request = request;
		mid.guri = guri;
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
			/* Process e-mail mail requests in the main/UI thread, because
			 * any EMailFormatter can create GtkWidget-s, or manipulate with
			 * them, which should be always done in the main/UI thread. */
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
	g_uri_unref (guri);

	return success;
}

static void
e_mail_request_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCALE_FACTOR:
			e_mail_request_set_scale_factor (
				E_MAIL_REQUEST (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_request_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SCALE_FACTOR:
			g_value_set_int (
				value,
				e_mail_request_get_scale_factor (
				E_MAIL_REQUEST (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_mail_request_set_property;
	object_class->get_property = e_mail_request_get_property;

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
e_mail_request_init (EMailRequest *request)
{
	request->priv = e_mail_request_get_instance_private (request);
	request->priv->scale_factor = 0;
}

EContentRequest *
e_mail_request_new (void)
{
	return g_object_new (E_TYPE_MAIL_REQUEST, NULL);
}

gint
e_mail_request_get_scale_factor (EMailRequest *mail_request)
{
	g_return_val_if_fail (E_IS_MAIL_REQUEST (mail_request), 0);

	return mail_request->priv->scale_factor;
}

void
e_mail_request_set_scale_factor (EMailRequest *mail_request,
				 gint scale_factor)
{
	g_return_if_fail (E_IS_MAIL_REQUEST (mail_request));

	if (mail_request->priv->scale_factor == scale_factor)
		return;

	mail_request->priv->scale_factor = scale_factor;

	g_object_notify (G_OBJECT (mail_request), "scale-factor");
}

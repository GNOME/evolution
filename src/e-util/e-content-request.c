/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <errno.h>

#include <glib.h>
#include <glib-object.h>

#include "e-simple-async-result.h"

#include "e-content-request.h"

G_DEFINE_INTERFACE (EContentRequest, e_content_request, G_TYPE_OBJECT)

static void
e_content_request_default_init (EContentRequestInterface *iface)
{
}

gboolean
e_content_request_can_process_uri (EContentRequest *request,
				   const gchar *uri)
{
	EContentRequestInterface *iface;

	g_return_val_if_fail (E_IS_CONTENT_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	iface = E_CONTENT_REQUEST_GET_INTERFACE (request);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->can_process_uri != NULL, FALSE);

	return iface->can_process_uri (request, uri);
}

gboolean
e_content_request_process_sync (EContentRequest *request,
				const gchar *uri,
				GObject *requester,
				GInputStream **out_stream,
				gint64 *out_stream_length,
				gchar **out_mime_type,
				GCancellable *cancellable,
				GError **error)
{
	EContentRequestInterface *iface;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_CONTENT_REQUEST (request), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (G_IS_OBJECT (requester), FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);
	g_return_val_if_fail (out_stream_length != NULL, FALSE);
	g_return_val_if_fail (out_mime_type != NULL, FALSE);

	iface = E_CONTENT_REQUEST_GET_INTERFACE (request);
	g_return_val_if_fail (iface != NULL, FALSE);
	g_return_val_if_fail (iface->process_sync != NULL, FALSE);

	if (!iface->process_sync (request, uri, requester, out_stream, out_stream_length, out_mime_type, cancellable, &local_error)) {
		if (!local_error)
			local_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, g_strerror (ENOENT));

		g_propagate_error (error, local_error);

		return FALSE;
	}

	return TRUE;
}

typedef struct _ThreadData
{
	gchar *uri;
	GObject *requester;
	GInputStream *out_stream;
	gint64 out_stream_length;
	gchar *out_mime_type;
	GError *error;
	gboolean success;
} ThreadData;

static void
thread_data_free (gpointer ptr)
{
	ThreadData *td = ptr;

	if (td) {
		g_clear_object (&td->out_stream);
		g_clear_object (&td->requester);
		g_free (td->uri);
		g_free (td->out_mime_type);
		g_clear_error (&td->error);
		g_slice_free (ThreadData, td);
	}
}

static void
content_request_process_thread (ESimpleAsyncResult *result,
				gpointer source_object,
				GCancellable *cancellable)
{
	ThreadData *td;

	g_return_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result));
	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));

	td = e_simple_async_result_get_user_data (result);
	g_return_if_fail (td != NULL);

	td->success = e_content_request_process_sync (E_CONTENT_REQUEST (source_object),
		td->uri, td->requester, &td->out_stream, &td->out_stream_length, &td->out_mime_type,
		cancellable, &td->error);
}

void
e_content_request_process (EContentRequest *request,
			   const gchar *uri,
			   GObject *requester,
			   GCancellable *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer user_data)
{
	ThreadData *td;
	ESimpleAsyncResult *result;
	gboolean is_http, is_contact;

	g_return_if_fail (E_IS_CONTENT_REQUEST (request));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (G_IS_OBJECT (requester));

	is_http = g_ascii_strncasecmp (uri, "http", 4) == 0 ||
		  g_ascii_strncasecmp (uri, "evo-http", 8) == 0;
	is_contact = g_ascii_strncasecmp (uri, "mail://contact-photo", 20) == 0;

	td = g_slice_new0 (ThreadData);
	td->uri = g_strdup (uri);
	td->requester = g_object_ref (requester);

	result = e_simple_async_result_new (G_OBJECT (request), callback, user_data, e_content_request_process);

	e_simple_async_result_set_user_data (result, td, thread_data_free);
	e_simple_async_result_set_check_cancellable (result, cancellable);
	e_simple_async_result_run_in_thread (result, (is_http || is_contact) ? G_PRIORITY_LOW : G_PRIORITY_DEFAULT, content_request_process_thread, cancellable);

	g_object_unref (result);
}

gboolean
e_content_request_process_finish (EContentRequest *request,
				  GAsyncResult *result,
				  GInputStream **out_stream,
				  gint64 *out_stream_length,
				  gchar **out_mime_type,
				  GError **error)
{
	ThreadData *td;

	g_return_val_if_fail (g_async_result_is_tagged (result, e_content_request_process), FALSE);
	g_return_val_if_fail (E_IS_CONTENT_REQUEST (request), FALSE);
	g_return_val_if_fail (E_IS_SIMPLE_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);
	g_return_val_if_fail (out_stream_length != NULL, FALSE);
	g_return_val_if_fail (out_mime_type != NULL, FALSE);

	td = e_simple_async_result_get_user_data (E_SIMPLE_ASYNC_RESULT (result));
	g_return_val_if_fail (td != NULL, FALSE);

	if (td->error || !td->success) {
		if (td->error) {
			g_propagate_error (error, td->error);
			td->error = NULL;
		}

		return FALSE;
	}

	*out_stream = td->out_stream;
	*out_stream_length = td->out_stream_length;
	*out_mime_type = td->out_mime_type;

	td->out_stream = NULL;
	td->out_mime_type = NULL;

	return TRUE;
}

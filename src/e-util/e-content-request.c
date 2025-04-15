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
} ThreadData;

typedef struct _RequestProcessResult
{
	GInputStream *stream;
	gint64 stream_length;
	gchar *mime_type;
} RequestProcessResult;

static void
thread_data_free (gpointer ptr)
{
	ThreadData *td = ptr;

	if (td) {
		g_clear_object (&td->requester);
		g_clear_pointer (&td->uri, g_free);
		g_free (td);
	}
}

static void
request_process_result_free (gpointer ptr)
{
	RequestProcessResult *rpr = ptr;

	if (rpr) {
		g_clear_object (&rpr->stream);
		g_clear_pointer (&rpr->mime_type, g_free);
		g_free (rpr);
	}
}

static void
content_request_process_thread (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	ThreadData *td = task_data;
	GInputStream *stream = NULL;
	gint64 stream_length = 0;
	gchar *mime_type = NULL;
	GError *error = NULL;

	g_return_if_fail (G_IS_TASK (task));
	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));
	g_return_if_fail (td != NULL);

	if (e_content_request_process_sync (E_CONTENT_REQUEST (source_object),
		td->uri, td->requester, &stream, &stream_length, &mime_type,
		cancellable, &error)) {
		RequestProcessResult *rpr = g_new0 (RequestProcessResult, 1);
		rpr->stream = g_steal_pointer (&stream);
		rpr->stream_length = stream_length;
		rpr->mime_type = g_steal_pointer (&mime_type);

		g_task_return_pointer (task, g_steal_pointer (&rpr), request_process_result_free);
	} else {
		g_task_return_error (task, g_steal_pointer (&error));
	}
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
	GTask *task;
	gboolean is_http, is_contact;

	g_return_if_fail (E_IS_CONTENT_REQUEST (request));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (G_IS_OBJECT (requester));

	is_http = g_ascii_strncasecmp (uri, "http", 4) == 0 ||
		  g_ascii_strncasecmp (uri, "evo-http", 8) == 0;
	is_contact = g_ascii_strncasecmp (uri, "mail://contact-photo", 20) == 0;

	td = g_new0 (ThreadData, 1);
	td->uri = g_strdup (uri);
	td->requester = g_object_ref (requester);

	task = g_task_new (request, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_content_request_process);
	g_task_set_task_data (task, g_steal_pointer (&td), thread_data_free);
	g_task_set_priority (task, (is_http || is_contact) ? G_PRIORITY_LOW : G_PRIORITY_DEFAULT);

	g_task_run_in_thread (task, content_request_process_thread);

	g_object_unref (task);
}

gboolean
e_content_request_process_finish (EContentRequest *request,
				  GAsyncResult *result,
				  GInputStream **out_stream,
				  gint64 *out_stream_length,
				  gchar **out_mime_type,
				  GError **error)
{
	RequestProcessResult *rpr;

	g_return_val_if_fail (g_async_result_is_tagged (result, e_content_request_process), FALSE);
	g_return_val_if_fail (E_IS_CONTENT_REQUEST (request), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, request), FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);
	g_return_val_if_fail (out_stream_length != NULL, FALSE);
	g_return_val_if_fail (out_mime_type != NULL, FALSE);

	rpr = g_task_propagate_pointer (G_TASK (result), error);
	if (!rpr)
		return FALSE;

	*out_stream = g_steal_pointer (&rpr->stream);
	*out_stream_length = rpr->stream_length;
	*out_mime_type = g_steal_pointer (&rpr->mime_type);

	g_clear_pointer (&rpr, request_process_result_free);
	return TRUE;
}

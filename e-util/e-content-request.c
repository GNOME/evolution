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
#include <gio/gio.h>

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
		g_free (td);
	}
}

static void
content_request_process_thread (GTask *task,
				gpointer source_object,
				gpointer task_data,
				GCancellable *cancellable)
{
	ThreadData *td = task_data;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_CONTENT_REQUEST (source_object));
	g_return_if_fail (td != NULL);

	if (!e_content_request_process_sync (E_CONTENT_REQUEST (source_object),
		td->uri, td->requester, &td->out_stream, &td->out_stream_length, &td->out_mime_type,
		cancellable, &local_error)) {
		g_task_return_error (task, local_error);
	} else {
		g_task_return_boolean (task, TRUE);
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
	GTask *task;
	ThreadData *td;

	g_return_if_fail (E_IS_CONTENT_REQUEST (request));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (G_IS_OBJECT (requester));

	td = g_new0 (ThreadData, 1);
	td->uri = g_strdup (uri);
	td->requester = g_object_ref (requester);

	task = g_task_new (request, cancellable, callback, user_data);
	g_task_set_task_data (task, td, thread_data_free);
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
	ThreadData *td;

	g_return_val_if_fail (g_task_is_valid (result, request), FALSE);
	g_return_val_if_fail (E_IS_CONTENT_REQUEST (request), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);
	g_return_val_if_fail (out_stream_length != NULL, FALSE);
	g_return_val_if_fail (out_mime_type != NULL, FALSE);

	td = g_task_get_task_data (G_TASK (result));
	g_return_val_if_fail (td != NULL, FALSE);

	if (!g_task_propagate_boolean (G_TASK (result), error))
		return FALSE;

	*out_stream = td->out_stream;
	*out_stream_length = td->out_stream_length;
	*out_mime_type = td->out_mime_type;

	td->out_stream = NULL;
	td->out_mime_type = NULL;

	return TRUE;
}

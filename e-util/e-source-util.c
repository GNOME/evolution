/*
 * e-source-util.c
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

#include "e-source-util.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
};

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->activity != NULL)
		g_object_unref (async_context->activity);

	g_slice_free (AsyncContext, async_context);
}

static void
source_util_remove_cb (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	const gchar *display_name;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	display_name = e_source_get_display_name (source);

	e_source_remove_finish (source, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"system:remove-source-fail",
			display_name, error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

/**
 * e_source_util_remove:
 * @source: the #ESource to be removed
 * @alert_sink: an #EAlertSink
 *
 * Requests the D-Bus service to delete the key files for @source and all of
 * its descendants and broadcast their removal to all clients.  If an error
 * occurs, an #EAlert will be posted to @alert_sink.
 *
 * This function does not block.  The returned #EActivity can either be
 * ignored or passed to something that can display activity status to the
 * user, such as e_shell_backend_add_activity().
 *
 * Returns: an #EActivity to track the operation
 **/
EActivity *
e_source_util_remove (ESource *source,
                      EAlertSink *alert_sink)
{
	AsyncContext *async_context;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	cancellable = g_cancellable_new ();

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = e_activity_new ();

	e_activity_set_alert_sink (async_context->activity, alert_sink);
	e_activity_set_cancellable (async_context->activity, cancellable);

	e_source_remove (
		source, cancellable,
		source_util_remove_cb,
		async_context);

	g_object_unref (cancellable);

	return async_context->activity;
}

static void
source_util_write_cb (GObject *source_object,
                      GAsyncResult *result,
                      gpointer user_data)
{
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	const gchar *display_name;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	display_name = e_source_get_display_name (source);

	e_source_write_finish (source, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"system:write-source-fail",
			display_name, error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

/**
 * e_source_util_write:
 * @source: an #ESource
 * @alert_sink: an #EAlertSink
 *
 * Submits the current contents of @source to the D-Bus service to be
 * written to disk and broadcast to other clients.  If an error occurs,
 * an #EAlert will be posted to @alert_sink.
 *
 * This function does not block.  The returned #EActivity can either be
 * ignored or passed to something that can display activity status to the
 * user, such as e_shell_backend_add_activity().
 *
 * Returns: an #EActivity to track the operation
 **/
EActivity *
e_source_util_write (ESource *source,
                     EAlertSink *alert_sink)
{
	AsyncContext *async_context;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	cancellable = g_cancellable_new ();

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = e_activity_new ();

	e_activity_set_alert_sink (async_context->activity, alert_sink);
	e_activity_set_cancellable (async_context->activity, cancellable);

	e_source_write (
		source, cancellable,
		source_util_write_cb,
		async_context);

	g_object_unref (cancellable);

	return async_context->activity;
}

static void
source_util_remote_delete_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ESource *source;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	const gchar *display_name;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	display_name = e_source_get_display_name (source);

	e_source_remote_delete_finish (source, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"system:delete-resource-fail",
			display_name, error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

/**
 * e_source_util_remote_delete:
 * @source: an #ESource
 * @alert_sink: an #EAlertSink
 *
 * Deletes the resource represented by @source from a remote server.
 * The @source must be #ESource:remote-deletable.  This will also delete
 * the key file for @source and broadcast its removal to all clients,
 * similar to e_source_util_remove().  If an error occurs, an #EAlert
 * will be posted to @alert_sink.
 *
 * This function does not block.  The returned #EActivity can either be
 * ignored or passed to something that can display activity status to the
 * user, such as e_shell_backend_add_activity().
 *
 * Returns: an #EActivity to track the operation
 **/
EActivity *
e_source_util_remote_delete (ESource *source,
                             EAlertSink *alert_sink)
{
	AsyncContext *async_context;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	cancellable = g_cancellable_new ();

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = e_activity_new ();

	e_activity_set_alert_sink (async_context->activity, alert_sink);
	e_activity_set_cancellable (async_context->activity, cancellable);

	e_source_remote_delete (
		source, cancellable,
		source_util_remote_delete_cb,
		async_context);

	g_object_unref (cancellable);

	return async_context->activity;
}


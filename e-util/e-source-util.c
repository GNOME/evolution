/*
 * e-source-util.c
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

#include "e-source-util.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	ESource *source;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	if (context->source != NULL)
		g_object_unref (context->source);

	g_slice_free (AsyncContext, context);
}

static void
source_util_remove_cb (ESource *source,
                       GAsyncResult *result,
                       AsyncContext *context)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GError *error = NULL;

	activity = context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_source_remove_finish (source, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"source:remove-source-fail",
			e_source_get_display_name (context->source),
			error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (context);
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
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	cancellable = g_cancellable_new ();

	context = g_slice_new0 (AsyncContext);
	context->activity = e_activity_new ();
	context->source = g_object_ref (source);

	e_activity_set_alert_sink (context->activity, alert_sink);
	e_activity_set_cancellable (context->activity, cancellable);

	e_source_remove (
		source, cancellable, (GAsyncReadyCallback)
		source_util_remove_cb, context);

	g_object_unref (cancellable);

	return context->activity;
}

static void
source_util_write_cb (ESource *source,
                      GAsyncResult *result,
                      AsyncContext *context)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GError *error = NULL;

	activity = context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_source_write_finish (source, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"source:submit-data-fail",
			error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (context);
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
	AsyncContext *context;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);

	cancellable = g_cancellable_new ();

	context = g_slice_new0 (AsyncContext);
	context->activity = e_activity_new ();

	e_activity_set_alert_sink (context->activity, alert_sink);
	e_activity_set_cancellable (context->activity, cancellable);

	e_source_write (
		source, cancellable, (GAsyncReadyCallback)
		source_util_write_cb, context);

	g_object_unref (cancellable);

	return context->activity;
}


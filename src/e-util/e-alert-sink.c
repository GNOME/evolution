/*
 * e-alert-sink.c
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

/**
 * SECTION: e-alert-sink
 * @short_description: an interface to handle alerts
 * @include: e-util/e-util.h
 *
 * A widget that implements #EAlertSink means it can handle #EAlerts,
 * usually by displaying them to the user.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include "e-alert-sink.h"
#include "e-activity.h"

#include "e-alert-dialog.h"

G_DEFINE_INTERFACE (
	EAlertSink,
	e_alert_sink,
	GTK_TYPE_WIDGET)

static void
alert_sink_fallback (GtkWidget *widget,
                     EAlert *alert)
{
	GtkWidget *dialog;
	gpointer parent;

	parent = gtk_widget_get_toplevel (widget);
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = e_alert_dialog_new (parent, alert);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
alert_sink_submit_alert (EAlertSink *alert_sink,
                         EAlert *alert)
{
	/* This is just a lame fallback handler.  Implementors
	 * are strongly encouraged to override this method. */
	alert_sink_fallback (GTK_WIDGET (alert_sink), alert);
}

static void
e_alert_sink_default_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = alert_sink_submit_alert;
}

/**
 * e_alert_sink_submit_alert:
 * @alert_sink: an #EAlertSink
 * @alert: an #EAlert
 *
 * This function is a place to pass #EAlert objects.  Beyond that it has no
 * well-defined behavior.  It's up to the widget implementing the #EAlertSink
 * interface to decide what to do with them.
 **/
void
e_alert_sink_submit_alert (EAlertSink *alert_sink,
                           EAlert *alert)
{
	EAlertSinkInterface *iface;

	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (E_IS_ALERT (alert));

	iface = E_ALERT_SINK_GET_INTERFACE (alert_sink);
	g_return_if_fail (iface->submit_alert != NULL);

	iface->submit_alert (alert_sink, alert);
}

struct _EAlertSinkThreadJobData {
	EActivity *activity;
	gchar *alert_ident;
	gchar *alert_arg_0;
	GError *error;

	EAlertSinkThreadJobFunc func;
	gpointer user_data;
	GDestroyNotify free_user_data;
};

static gboolean
e_alert_sink_thread_job_done_cb (gpointer user_data)
{
	EAlertSinkThreadJobData *job_data = user_data;
	EAlertSink *alert_sink;
	GCancellable *cancellable;

	g_return_val_if_fail (job_data != NULL, FALSE);
	g_return_val_if_fail (job_data->func != NULL, FALSE);

	alert_sink = e_activity_get_alert_sink (job_data->activity);
	cancellable = e_activity_get_cancellable (job_data->activity);

	camel_operation_pop_message (cancellable);

	if (e_activity_handle_cancellation (job_data->activity, job_data->error)) {
		/* do nothing */
	} else if (job_data->error != NULL) {
		if (job_data->alert_arg_0) {
			e_alert_submit (
				alert_sink,
				job_data->alert_ident,
				job_data->alert_arg_0, job_data->error->message, NULL);
		} else {
			e_alert_submit (
				alert_sink,
				job_data->alert_ident,
				job_data->error->message, NULL);
		}
	} else {
		e_activity_set_state (job_data->activity, E_ACTIVITY_COMPLETED);
	}

	/* clean-up */
	g_clear_object (&job_data->activity);
	g_clear_error (&job_data->error);
	g_free (job_data->alert_ident);
	g_free (job_data->alert_arg_0);

	if (job_data->free_user_data)
		job_data->free_user_data (job_data->user_data);

	g_slice_free (EAlertSinkThreadJobData, job_data);

	return FALSE;
}

static gpointer
e_alert_sink_thread_job (gpointer user_data)
{
	EAlertSinkThreadJobData *job_data = user_data;
	GCancellable *cancellable;

	g_return_val_if_fail (job_data != NULL, NULL);
	g_return_val_if_fail (job_data->func != NULL, NULL);
	g_return_val_if_fail (job_data->error == NULL, NULL);

	cancellable = e_activity_get_cancellable (job_data->activity);

	job_data->func (job_data, job_data->user_data, cancellable, &job_data->error);

	g_timeout_add (1, e_alert_sink_thread_job_done_cb, job_data);

	return NULL;
}

/**
 * e_alert_sink_submit_thread_job:
 * @alert_sink: an #EAlertSink instance
 * @description: user-friendly description of the job, to be shown in UI
 * @alert_ident: in case of an error, this alert identificator is used
 *    for EAlert construction
 * @alert_arg_0: (allow-none): in case of an error, use this string as
 *    the first argument to the EAlert construction; the second argument
 *    is the actual error message; can be #NULL, in which case only
 *    the error message is passed to the EAlert construction
 * @func: function to be run in a dedicated thread
 * @user_data: (allow-none): custom data passed into @func; can be #NULL
 * @free_user_data: (allow-none): function to be called on @user_data,
 *   when the job is over; can be #NULL
 *
 * Runs the @func in a dedicated thread. Any error is propagated to UI.
 * The cancellable passed into the @func is a #CamelOperation, thus
 * the caller can overwrite progress and description message on it.
 *
 * Returns: (transfer full): Newly created #EActivity on success.
 *   The caller is responsible to g_object_unref() it when done with it.
 *
 * Note: The @free_user_data, if set, is called in the main thread.
 *
 * Note: This function should be called only from the main thread.
 *
 * Since: 3.16
 **/
EActivity *
e_alert_sink_submit_thread_job (EAlertSink *alert_sink,
				const gchar *description,
				const gchar *alert_ident,
				const gchar *alert_arg_0,
				EAlertSinkThreadJobFunc func,
				gpointer user_data,
				GDestroyNotify free_user_data)
{
	EActivity *activity;
	GCancellable *cancellable;
	EAlertSinkThreadJobData *job_data;
	GThread *thread;

	g_return_val_if_fail (E_IS_ALERT_SINK (alert_sink), NULL);
	g_return_val_if_fail (description != NULL, NULL);
	g_return_val_if_fail (func != NULL, NULL);

	activity = e_activity_new ();
	cancellable = camel_operation_new ();

	e_activity_set_alert_sink (activity, alert_sink);
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_text (activity, description);

	camel_operation_push_message (cancellable, "%s", description);

	job_data = g_slice_new0 (EAlertSinkThreadJobData);
	job_data->activity = g_object_ref (activity);
	job_data->alert_ident = g_strdup (alert_ident);
	job_data->alert_arg_0 = g_strdup (alert_arg_0);
	job_data->error = NULL;
	job_data->func = func;
	job_data->user_data = user_data;
	job_data->free_user_data = free_user_data;

	thread = g_thread_try_new (G_STRFUNC, e_alert_sink_thread_job, job_data, &job_data->error);

	g_object_unref (cancellable);

	if (thread) {
		g_thread_unref (thread);
	} else {
		g_prefix_error (&job_data->error, _("Failed to create a thread: "));
		g_timeout_add (1, e_alert_sink_thread_job_done_cb, job_data);
	}

	return activity;
}

/**
 * e_alert_sink_thread_job_set_alert_ident:
 * @job_data: Thread job data, as passed to a thread
 *    function specified at e_alert_sink_submit_thread_job()
 * @alert_ident: A new alert identificator to use; cannot be #NULL
 *
 * Change an alert identificator to be used for error reporting.
 * This can be used within a thread function at e_alert_sink_submit_thread_job(),
 * to overwrite the default error message, in case of a need to more fine-tuned
 * infomation to a user being available.
 *
 * See: e_alert_sink_thread_job_set_alert_arg_0
 *
 * Since: 3.16
 **/
void
e_alert_sink_thread_job_set_alert_ident (EAlertSinkThreadJobData *job_data,
					 const gchar *alert_ident)
{
	g_return_if_fail (job_data != NULL);
	g_return_if_fail (alert_ident != NULL);

	if (job_data->alert_ident != alert_ident) {
		g_free (job_data->alert_ident);
		job_data->alert_ident = g_strdup (alert_ident);
	}
}

/**
 * e_alert_sink_thread_job_set_alert_arg_0:
 * @job_data: Thread job data, as passed to a thread
 *    function specified at e_alert_sink_submit_thread_job()
 * @alert_arg_0: (allow-none): A new argument 0 of the alert;
 *    can be #NULL, to unset the previously set value
 *
 * Change an argument 0 for an alert to be used for error reporting.
 * This can be used within a thread function at e_alert_sink_submit_thread_job(),
 * to overwrite the default argument 0 of the erorr message. It might be
 * usually used with combination of e_alert_sink_thread_job_set_alert_ident().
 *
 * Since: 3.16
 **/
void
e_alert_sink_thread_job_set_alert_arg_0 (EAlertSinkThreadJobData *job_data,
					 const gchar *alert_arg_0)
{
	g_return_if_fail (job_data != NULL);

	if (job_data->alert_arg_0 != alert_arg_0) {
		g_free (job_data->alert_arg_0);
		job_data->alert_arg_0 = g_strdup (alert_arg_0);
	}
}

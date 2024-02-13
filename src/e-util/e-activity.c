/*
 * e-activity.c
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
 * SECTION: e-activity
 * @include: e-util/e-util.h
 * @short_description: Describe activities in progress
 *
 * #EActivity is used to track and describe application activities in
 * progress.  An #EActivity usually manifests in a user interface as a
 * status bar message (see #EActivityProxy) or information bar message
 * (see #EActivityBar), with optional progress indication and a cancel
 * button which is linked to a #GCancellable.
 **/

#include "evolution-config.h"

#include "e-activity.h"

#include <stdarg.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-util-enumtypes.h"

struct _EActivityPrivate {
	GCancellable *cancellable;
	EAlertSink *alert_sink;
	EActivityState state;

	gchar *icon_name;
	gchar *text;
	gchar *last_known_text;
	gdouble percent;

	/* Whether to emit a runtime warning if we
	 * have to suppress a bogus percent value. */
	gboolean warn_bogus_percent;
};

enum {
	PROP_0,
	PROP_ALERT_SINK,
	PROP_CANCELLABLE,
	PROP_ICON_NAME,
	PROP_PERCENT,
	PROP_STATE,
	PROP_TEXT
};

G_DEFINE_TYPE_WITH_PRIVATE (EActivity, e_activity, G_TYPE_OBJECT)

static void
activity_camel_status_cb (EActivity *activity,
                          const gchar *description,
                          gint percent)
{
	/* CamelOperation::status signals are always emitted from idle
	 * callbacks, so we don't have to screw around with locking. */

	g_object_set (
		activity, "percent", (gdouble) percent,
		"text", description, NULL);
}

static void
activity_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALERT_SINK:
			e_activity_set_alert_sink (
				E_ACTIVITY (object),
				g_value_get_object (value));
			return;

		case PROP_CANCELLABLE:
			e_activity_set_cancellable (
				E_ACTIVITY (object),
				g_value_get_object (value));
			return;

		case PROP_ICON_NAME:
			e_activity_set_icon_name (
				E_ACTIVITY (object),
				g_value_get_string (value));
			return;

		case PROP_PERCENT:
			e_activity_set_percent (
				E_ACTIVITY (object),
				g_value_get_double (value));
			return;

		case PROP_STATE:
			e_activity_set_state (
				E_ACTIVITY (object),
				g_value_get_enum (value));
			return;

		case PROP_TEXT:
			e_activity_set_text (
				E_ACTIVITY (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_get_property (GObject *object,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALERT_SINK:
			g_value_set_object (
				value, e_activity_get_alert_sink (
				E_ACTIVITY (object)));
			return;

		case PROP_CANCELLABLE:
			g_value_set_object (
				value, e_activity_get_cancellable (
				E_ACTIVITY (object)));
			return;

		case PROP_ICON_NAME:
			g_value_set_string (
				value, e_activity_get_icon_name (
				E_ACTIVITY (object)));
			return;

		case PROP_PERCENT:
			g_value_set_double (
				value, e_activity_get_percent (
				E_ACTIVITY (object)));
			return;

		case PROP_STATE:
			g_value_set_enum (
				value, e_activity_get_state (
				E_ACTIVITY (object)));
			return;

		case PROP_TEXT:
			g_value_set_string (
				value, e_activity_get_text (
				E_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_dispose (GObject *object)
{
	EActivity *self = E_ACTIVITY (object);

	g_clear_object (&self->priv->alert_sink);

	if (self->priv->cancellable != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->cancellable,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->cancellable);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_activity_parent_class)->dispose (object);
}

static void
activity_finalize (GObject *object)
{
	EActivity *self = E_ACTIVITY (object);

	g_free (self->priv->icon_name);
	g_free (self->priv->text);
	g_free (self->priv->last_known_text);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_activity_parent_class)->finalize (object);
}

static gchar *
activity_describe (EActivity *activity)
{
	GString *string;
	GCancellable *cancellable;
	EActivityState state;
	const gchar *text;
	gdouble percent;

	text = e_activity_get_text (activity);

	if (text == NULL)
		return NULL;

	string = g_string_sized_new (256);
	cancellable = e_activity_get_cancellable (activity);
	percent = e_activity_get_percent (activity);
	state = e_activity_get_state (activity);

	/* Sanity check the percentage. */
	if (percent > 100.0) {
		if (activity->priv->warn_bogus_percent) {
			g_warning (
				"Nonsensical (%d%% complete) reported on "
				"activity \"%s\"", (gint) (percent), text);
			activity->priv->warn_bogus_percent = FALSE;
		}
		percent = -1.0;  /* suppress it */
	} else {
		activity->priv->warn_bogus_percent = TRUE;
	}

	if (state == E_ACTIVITY_CANCELLED) {
		/* Translators: This is a cancelled activity. */
		g_string_printf (string, _("%s (cancelled)"), text);
	} else if (state == E_ACTIVITY_COMPLETED) {
		/* Translators: This is a completed activity. */
		g_string_printf (string, _("%s (completed)"), text);
	} else if (state == E_ACTIVITY_WAITING) {
		/* Translators: This is an activity waiting to run. */
		g_string_printf (string, _("%s (waiting)"), text);
	} else if (g_cancellable_is_cancelled (cancellable)) {
		/* Translators: This is a running activity which
		 *              the user has requested to cancel. */
		g_string_printf (string, _("%s (cancelling)"), text);
	} else if (percent <= 0.0) {
		g_string_printf (string, _("%s"), text);
	} else {
		g_string_printf (
			/* Translators: This is a running activity whose
			 *              percent complete is known. */
			string, _("%s (%d%% complete)"),
			text, (gint) (percent));
	}

	return g_string_free (string, FALSE);
}

static void
e_activity_class_init (EActivityClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = activity_set_property;
	object_class->get_property = activity_get_property;
	object_class->dispose = activity_dispose;
	object_class->finalize = activity_finalize;

	class->describe = activity_describe;

	g_object_class_install_property (
		object_class,
		PROP_ALERT_SINK,
		g_param_spec_object (
			"alert-sink",
			NULL,
			NULL,
			E_TYPE_ALERT_SINK,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CANCELLABLE,
		g_param_spec_object (
			"cancellable",
			NULL,
			NULL,
			G_TYPE_CANCELLABLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_ICON_NAME,
		g_param_spec_string (
			"icon-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_PERCENT,
		g_param_spec_double (
			"percent",
			NULL,
			NULL,
			-G_MAXDOUBLE,
			G_MAXDOUBLE,
			-1.0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_STATE,
		g_param_spec_enum (
			"state",
			NULL,
			NULL,
			E_TYPE_ACTIVITY_STATE,
			E_ACTIVITY_RUNNING,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_TEXT,
		g_param_spec_string (
			"text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_activity_init (EActivity *activity)
{
	activity->priv = e_activity_get_instance_private (activity);
	activity->priv->warn_bogus_percent = TRUE;
}

/**
 * e_activity_new:
 *
 * Creates a new #EActivity.
 *
 * Returns: an #EActivity
 **/
EActivity *
e_activity_new (void)
{
	return g_object_new (E_TYPE_ACTIVITY, NULL);
}

/**
 * e_activity_cancel:
 * @activity: an #EActivity
 *
 * Convenience function cancels @activity's #EActivity:cancellable.
 *
 * <para>
 *   <note>
 *     This function will not set @activity's #EActivity:state to
 *     @E_ACTIVITY_CANCELLED.  It merely requests that the associated
 *     operation be cancelled.  Only after the operation finishes with
 *     a @G_IO_ERROR_CANCELLED should the @activity's #EActivity:state
 *     be changed (see e_activity_handle_cancellation()).  During this
 *     interim period e_activity_describe() will indicate the activity
 *     is "cancelling".
 *   </note>
 * </para>
 **/
void
e_activity_cancel (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	/* This function handles NULL gracefully. */
	g_cancellable_cancel (activity->priv->cancellable);
}

/**
 * e_activity_describe:
 * @activity: an #EActivity
 *
 * Returns a description of the current state of the @activity based on
 * the #EActivity:text, #EActivity:percent and #EActivity:state properties.
 * Suitable for displaying in a status bar or similar widget.
 *
 * Free the returned string with g_free() when finished with it.
 *
 * Returns: a description of @activity
 **/
gchar *
e_activity_describe (EActivity *activity)
{
	EActivityClass *class;

	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	class = E_ACTIVITY_GET_CLASS (activity);
	g_return_val_if_fail (class != NULL, NULL);
	g_return_val_if_fail (class->describe != NULL, NULL);

	return class->describe (activity);
}

/**
 * e_activity_get_alert_sink:
 * @activity: an #EActivity
 *
 * Returns the #EAlertSink for @activity, if one was provided.
 *
 * The #EActivity:alert-sink property is convenient for when the user
 * should be alerted about a failed asynchronous operation.  Generally
 * an #EActivity:alert-sink is set prior to dispatching the operation,
 * and retrieved by a callback function when the operation completes.
 *
 * Returns: an #EAlertSink, or %NULL
 **/
EAlertSink *
e_activity_get_alert_sink (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->alert_sink;
}

/**
 * e_activity_set_alert_sink:
 * @activity: an #EActivity
 * @alert_sink: an #EAlertSink, or %NULL
 *
 * Sets (or clears) the #EAlertSink for @activity.
 *
 * The #EActivity:alert-sink property is convenient for when the user
 * should be alerted about a failed asynchronous operation.  Generally
 * an #EActivity:alert-sink is set prior to dispatching the operation,
 * and retrieved by a callback function when the operation completes.
 **/
void
e_activity_set_alert_sink (EActivity *activity,
                           EAlertSink *alert_sink)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->alert_sink == alert_sink)
		return;

	if (alert_sink != NULL) {
		g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
		g_object_ref (alert_sink);
	}

	if (activity->priv->alert_sink != NULL)
		g_object_unref (activity->priv->alert_sink);

	activity->priv->alert_sink = alert_sink;

	g_object_notify (G_OBJECT (activity), "alert-sink");
}

/**
 * e_activity_get_cancellable:
 * @activity: an #EActivity
 *
 * Returns the #GCancellable for @activity, if one was provided.
 *
 * Generally the @activity's #EActivity:cancellable property holds the same
 * #GCancellable instance passed to a cancellable function, so widgets like
 * #EActivityBar can bind the #GCancellable to a cancel button.
 *
 * Returns: a #GCancellable, or %NULL
 **/
GCancellable *
e_activity_get_cancellable (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->cancellable;
}

/**
 * e_activity_set_cancellable:
 * @activity: an #EActivity
 * @cancellable: a #GCancellable, or %NULL
 *
 * Sets (or clears) the #GCancellable for @activity.
 *
 * Generally the @activity's #EActivity:cancellable property holds the same
 * #GCancellable instance passed to a cancellable function, so widgets like
 * #EActivityBar can bind the #GCancellable to a cancel button.
 **/
void
e_activity_set_cancellable (EActivity *activity,
                            GCancellable *cancellable)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->cancellable == cancellable)
		return;

	if (cancellable != NULL) {
		g_return_if_fail (G_IS_CANCELLABLE (cancellable));
		g_object_ref (cancellable);
	}

	if (activity->priv->cancellable != NULL) {
		g_signal_handlers_disconnect_matched (
			activity->priv->cancellable,
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, activity);
		g_object_unref (activity->priv->cancellable);
	}

	activity->priv->cancellable = cancellable;

	/* If this is a CamelOperation, listen for status updates
	 * from it and propagate them to our own status properties. */
	if (CAMEL_IS_OPERATION (cancellable))
		g_signal_connect_swapped (
			cancellable, "status",
			G_CALLBACK (activity_camel_status_cb), activity);

	g_object_notify (G_OBJECT (activity), "cancellable");
}

/**
 * e_activity_get_icon_name:
 * @activity: an #EActivity
 *
 * Returns the themed icon name for @activity, if one was provided.
 *
 * Generally widgets like #EActivityBar will honor the #EActivity:icon-name
 * property while the @activity's #EActivity:state is @E_ACTIVITY_RUNNING or
 * @E_ACTIVITY_WAITING, but will override the icon for @E_ACTIVITY_CANCELLED
 * and @E_ACTIVITY_COMPLETED.
 *
 * Returns: a themed icon name, or %NULL
 **/
const gchar *
e_activity_get_icon_name (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->icon_name;
}

/**
 * e_activity_set_icon_name:
 * @activity: an #EActivity
 * @icon_name: a themed icon name, or %NULL
 *
 * Sets (or clears) the themed icon name for @activity.
 *
 * Generally widgets like #EActivityBar will honor the #EActivity:icon-name
 * property while the @activity's #EActivity:state is @E_ACTIVITY_RUNNING or
 * @E_ACTIVITY_WAITING, but will override the icon for @E_ACTIVITY_CANCELLED
 * and @E_ACTIVITY_COMPLETED.
 **/
void
e_activity_set_icon_name (EActivity *activity,
                          const gchar *icon_name)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (g_strcmp0 (activity->priv->icon_name, icon_name) == 0)
		return;

	g_free (activity->priv->icon_name);
	activity->priv->icon_name = g_strdup (icon_name);

	g_object_notify (G_OBJECT (activity), "icon-name");
}

/**
 * e_activity_get_percent:
 * @activity: an #EActivity
 *
 * Returns the percent complete for @activity as a value between 0 and 100,
 * or a negative value if the percent complete is unknown.
 *
 * Generally widgets like #EActivityBar will display the percent complete by
 * way of e_activity_describe(), but only if the value is between 0 and 100.
 *
 * Returns: the percent complete, or a negative value if unknown
 **/
gdouble
e_activity_get_percent (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), -1.0);

	return activity->priv->percent;
}

/**
 * e_activity_set_percent:
 * @activity: an #EActivity
 * @percent: the percent complete, or a negative value if unknown
 *
 * Sets the percent complete for @activity.  The value should be between 0
 * and 100, or negative if the percent complete is unknown.
 *
 * Generally widgets like #EActivityBar will display the percent complete by
 * way of e_activity_describe(), but only if the value is between 0 and 100.
 **/
void
e_activity_set_percent (EActivity *activity,
                        gdouble percent)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->percent == percent)
		return;

	activity->priv->percent = percent;

	g_object_notify (G_OBJECT (activity), "percent");
}

/**
 * e_activity_get_state:
 * @activity: an #EActivity
 *
 * Returns the state of @activity.
 *
 * Generally widgets like #EActivityBar will display the activity state by
 * way of e_activity_describe() and possibly an icon.  The activity state is
 * @E_ACTIVITY_RUNNING by default, and is usually only changed once when the
 * associated operation is finished.
 *
 * Returns: an #EActivityState
 **/
EActivityState
e_activity_get_state (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), 0);

	return activity->priv->state;
}

/**
 * e_activity_set_state:
 * @activity: an #EActivity
 * @state: an #EActivityState
 *
 * Sets the state of @activity.
 *
 * Generally widgets like #EActivityBar will display the activity state by
 * way of e_activity_describe() and possibly an icon.  The activity state is
 * @E_ACTIVITY_RUNNING by default, and is usually only changed once when the
 * associated operation is finished.
 **/
void
e_activity_set_state (EActivity *activity,
                      EActivityState state)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->state == state)
		return;

	activity->priv->state = state;

	g_object_notify (G_OBJECT (activity), "state");
}

/**
 * e_activity_get_text:
 * @activity: an #EActivity
 *
 * Returns a message describing what @activity is doing.
 *
 * Generally widgets like #EActivityBar will display the message by way of
 * e_activity_describe().
 *
 * Returns: a descriptive message
 **/
const gchar *
e_activity_get_text (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->text;
}

/**
 * e_activity_set_text:
 * @activity: an #EActivity
 * @text: a descriptive message, or %NULL
 *
 * Sets (or clears) a message describing what @activity is doing.
 *
 * Generally widgets like #EActivityBar will display the message by way of
 * e_activity_describe().
 **/
void
e_activity_set_text (EActivity *activity,
                     const gchar *text)
{
	gchar *last_known_text = NULL;

	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (g_strcmp0 (activity->priv->text, text) == 0)
		return;

	g_free (activity->priv->text);
	activity->priv->text = g_strdup (text);

	/* See e_activity_get_last_known_text(). */
	last_known_text = e_util_strdup_strip (text);
	if (last_known_text != NULL) {
		g_free (activity->priv->last_known_text);
		activity->priv->last_known_text = last_known_text;
	}

	g_object_notify (G_OBJECT (activity), "text");
}

/**
 * e_activity_get_last_known_text:
 * @activity: an #EActivity
 *
 * Returns the last non-empty #EActivity:text value, so it's possible to
 * identify what the @activity <emphasis>was</emphasis> doing even if it
 * currently has no description.
 *
 * Mostly useful for debugging.
 *
 * Returns: a descriptive message, or %NULL
 **/
const gchar *
e_activity_get_last_known_text (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->last_known_text;
}

/**
 * e_activity_handle_cancellation:
 * @activity: an #EActivity
 * @error: a #GError, or %NULL
 *
 * Convenience function sets @activity's #EActivity:state to
 * @E_ACTIVITY_CANCELLED if @error is @G_IO_ERROR_CANCELLED.
 *
 * Returns: %TRUE if @activity was set to @E_ACTIVITY_CANCELLED
 **/
gboolean
e_activity_handle_cancellation (EActivity *activity,
                                const GError *error)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		e_activity_set_state (activity, E_ACTIVITY_CANCELLED);
		handled = TRUE;
	}

	return handled;
}

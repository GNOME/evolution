/*
 * e-activity.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-activity.h"

#include <stdarg.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

#include "e-util-enumtypes.h"

#define E_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACTIVITY, EActivityPrivate))

struct _EActivityPrivate {
	GCancellable *cancellable;
	EAlertSink *alert_sink;
	EActivityState state;

	gchar *icon_name;
	gchar *text;
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

G_DEFINE_TYPE (
	EActivity,
	e_activity,
	G_TYPE_OBJECT)

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
	EActivityPrivate *priv;

	priv = E_ACTIVITY_GET_PRIVATE (object);

	if (priv->alert_sink != NULL) {
		g_object_unref (priv->alert_sink);
		priv->alert_sink = NULL;
	}

	if (priv->cancellable != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->cancellable,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_activity_parent_class)->dispose (object);
}

static void
activity_finalize (GObject *object)
{
	EActivityPrivate *priv;

	priv = E_ACTIVITY_GET_PRIVATE (object);

	g_free (priv->icon_name);
	g_free (priv->text);

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
		/* Translators: This is a running activity whose
		 *              percent complete is known. */
		g_string_printf (
			string, _("%s (%d%% complete)"),
			text, (gint) (percent));
	}

	return g_string_free (string, FALSE);
}

static void
e_activity_class_init (EActivityClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EActivityPrivate));

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
	activity->priv = E_ACTIVITY_GET_PRIVATE (activity);
	activity->priv->warn_bogus_percent = TRUE;
}

EActivity *
e_activity_new (void)
{
	return g_object_new (E_TYPE_ACTIVITY, NULL);
}

gchar *
e_activity_describe (EActivity *activity)
{
	EActivityClass *class;

	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	class = E_ACTIVITY_GET_CLASS (activity);
	g_return_val_if_fail (class->describe != NULL, NULL);

	return class->describe (activity);
}

EAlertSink *
e_activity_get_alert_sink (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->alert_sink;
}

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

GCancellable *
e_activity_get_cancellable (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->cancellable;
}

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

const gchar *
e_activity_get_icon_name (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->icon_name;
}

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

gdouble
e_activity_get_percent (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), -1.0);

	return activity->priv->percent;
}

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

EActivityState
e_activity_get_state (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), 0);

	return activity->priv->state;
}

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

const gchar *
e_activity_get_text (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->text;
}

void
e_activity_set_text (EActivity *activity,
                     const gchar *text)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (g_strcmp0 (activity->priv->text, text) == 0)
		return;

	g_free (activity->priv->text);
	activity->priv->text = g_strdup (text);

	g_object_notify (G_OBJECT (activity), "text");
}

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

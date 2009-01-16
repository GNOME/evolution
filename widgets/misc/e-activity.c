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

#include "e-activity.h"

#include <glib/gi18n.h>

#define E_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACTIVITY, EActivityPrivate))

struct _EActivityPrivate {
	gchar *icon_name;
	gchar *primary_text;
	gchar *secondary_text;
	gdouble percent;
	guint timeout_id;

	guint blocking		: 1;
	guint cancellable	: 1;
	guint cancelled		: 1;
	guint clickable		: 1;
	guint completed		: 1;
};

enum {
	PROP_0,
	PROP_BLOCKING,
	PROP_CANCELLABLE,
	PROP_CLICKABLE,
	PROP_ICON_NAME,
	PROP_PERCENT,
	PROP_PRIMARY_TEXT,
	PROP_SECONDARY_TEXT
};

enum {
	CANCELLED,
	CLICKED,
	COMPLETED,
	TIMEOUT,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static gboolean
activity_timeout_cb (EActivity *activity)
{
	g_signal_emit (activity, signals[TIMEOUT], 0);

	return FALSE;
}

static void
activity_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BLOCKING:
			e_activity_set_blocking (
				E_ACTIVITY (object),
				g_value_get_boolean (value));
			return;

		case PROP_CANCELLABLE:
			e_activity_set_cancellable (
				E_ACTIVITY (object),
				g_value_get_boolean (value));
			return;

		case PROP_CLICKABLE:
			e_activity_set_clickable (
				E_ACTIVITY (object),
				g_value_get_boolean (value));
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

		case PROP_PRIMARY_TEXT:
			e_activity_set_primary_text (
				E_ACTIVITY (object),
				g_value_get_string (value));
			return;

		case PROP_SECONDARY_TEXT:
			e_activity_set_secondary_text (
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
		case PROP_BLOCKING:
			g_value_set_boolean (
				value, e_activity_get_blocking (
				E_ACTIVITY (object)));
			return;

		case PROP_CANCELLABLE:
			g_value_set_boolean (
				value, e_activity_get_cancellable (
				E_ACTIVITY (object)));
			return;

		case PROP_CLICKABLE:
			g_value_set_boolean (
				value, e_activity_get_clickable (
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

		case PROP_PRIMARY_TEXT:
			g_value_set_string (
				value, e_activity_get_primary_text (
				E_ACTIVITY (object)));
			return;

		case PROP_SECONDARY_TEXT:
			g_value_set_string (
				value, e_activity_get_secondary_text (
				E_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_finalize (GObject *object)
{
	EActivityPrivate *priv;

	priv = E_ACTIVITY_GET_PRIVATE (object);

	g_free (priv->icon_name);
	g_free (priv->primary_text);
	g_free (priv->secondary_text);

	if (priv->timeout_id > 0)
		g_source_remove (priv->timeout_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
activity_class_init (EActivityClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = activity_set_property;
	object_class->get_property = activity_get_property;
	object_class->finalize = activity_finalize;

	g_object_class_install_property (
		object_class,
		PROP_BLOCKING,
		g_param_spec_boolean (
			"blocking",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CANCELLABLE,
		g_param_spec_boolean (
			"cancellable",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CLICKABLE,
		g_param_spec_boolean (
			"clickable",
			NULL,
			NULL,
			FALSE,
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
		PROP_PRIMARY_TEXT,
		g_param_spec_string (
			"primary-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SECONDARY_TEXT,
		g_param_spec_string (
			"secondary-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[CANCELLED] = g_signal_new (
		"cancelled",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CLICKED] = g_signal_new (
		"clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMPLETED] = g_signal_new (
		"completed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[TIMEOUT] = g_signal_new (
		"timeout",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
activity_init (EActivity *activity)
{
	activity->priv = E_ACTIVITY_GET_PRIVATE (activity);
}

GType
e_activity_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EActivityClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) activity_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EActivity),
			0,     /* n_preallocs */
			(GInstanceInitFunc) activity_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EActivity", &type_info, 0);
	}

	return type;
}

EActivity *
e_activity_new (const gchar *primary_text)
{
	return g_object_new (
		E_TYPE_ACTIVITY,
		"primary-text", primary_text, NULL);
}

void
e_activity_cancel (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));
	g_return_if_fail (activity->priv->cancellable);

	if (activity->priv->cancelled)
		return;

	if (activity->priv->completed)
		return;

	activity->priv->cancelled = TRUE;
	g_signal_emit (activity, signals[CANCELLED], 0);
}

void
e_activity_complete (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->cancelled)
		return;

	if (activity->priv->completed)
		return;

	activity->priv->completed = TRUE;
	g_signal_emit (activity, signals[COMPLETED], 0);
}

void
e_activity_clicked (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_signal_emit (activity, signals[CLICKED], 0);
}

gchar *
e_activity_describe (EActivity *activity)
{
	GString *string;
	const gchar *text;
	gboolean cancelled;
	gboolean completed;
	gdouble percent;

	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	string = g_string_sized_new (256);
	text = e_activity_get_primary_text (activity);
	cancelled = e_activity_is_cancelled (activity);
	completed = e_activity_is_completed (activity);
	percent = e_activity_get_percent (activity);

	if (cancelled) {
		/* Translators: This is a cancelled activity. */
		g_string_printf (string, _("%s (cancelled)"), text);
	} else if (completed) {
		/* Translators: This is a completed activity. */
		g_string_printf (string, _("%s (completed)"), text);
	} else if (percent < 0.0) {
		/* Translators: This is an activity whose percent
		 * complete is unknown. */
		g_string_printf (string, _("%s..."), text);
	} else {
		/* Translators: This is an activity whose percent
		 * complete is known. */
		g_string_printf (
			string, _("%s (%d%% complete)"), text,
			(gint) (percent * 100.0 + 0.5));
	}

	return g_string_free (string, FALSE);
}

gboolean
e_activity_is_cancelled (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->cancelled;
}

gboolean
e_activity_is_completed (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->completed;
}

void
e_activity_add_timeout (EActivity *activity,
                        guint seconds)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	e_activity_cancel_timeout (activity);

	activity->priv->timeout_id = g_timeout_add_seconds (
		seconds, (GSourceFunc) activity_timeout_cb, activity);
}

void
e_activity_cancel_timeout (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (activity->priv->timeout_id > 0) {
		g_source_remove (activity->priv->timeout_id);
		activity->priv->timeout_id = 0;
	}
}

gboolean
e_activity_get_blocking (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->blocking;
}

void
e_activity_set_blocking (EActivity *activity,
                         gboolean blocking)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	activity->priv->blocking = blocking;

	g_object_notify (G_OBJECT (activity), "blocking");
}

gboolean
e_activity_get_cancellable (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->cancellable;
}

void
e_activity_set_cancellable (EActivity *activity,
                            gboolean cancellable)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	activity->priv->cancellable = cancellable;

	g_object_notify (G_OBJECT (activity), "cancellable");
}

gboolean
e_activity_get_clickable (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->clickable;
}

void
e_activity_set_clickable (EActivity *activity,
                          gboolean clickable)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	activity->priv->clickable = clickable;

	g_object_notify (G_OBJECT (activity), "clickable");
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

	activity->priv->percent = percent;

	g_object_notify (G_OBJECT (activity), "percent");
}

const gchar *
e_activity_get_primary_text (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->primary_text;
}

void
e_activity_set_primary_text (EActivity *activity,
                             const gchar *primary_text)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_free (activity->priv->primary_text);
	activity->priv->primary_text = g_strdup (primary_text);

	g_object_notify (G_OBJECT (activity), "primary-text");
}

const gchar *
e_activity_get_secondary_text (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return activity->priv->secondary_text;
}

void
e_activity_set_secondary_text (EActivity *activity,
                               const gchar *secondary_text)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	g_free (activity->priv->secondary_text);
	activity->priv->secondary_text = g_strdup (secondary_text);

	g_object_notify (G_OBJECT (activity), "secondary-text");
}

/************************* Error Dialog Integration **************************/

void
e_activity_error (EActivity *activity,
                  GtkWidget *error_dialog)
{
	GObject *object;
	const gchar *primary_text;
	const gchar *secondary_text;

	/* XXX Convert an activity to a clickable error message.
	 *     Clicking on the activity completes it and displays
	 *     the error dialog.  Eventually I'd like to eliminate
	 *     error dialogs altogether and show errors directly
	 *     in the shell window. */

	g_return_if_fail (E_IS_ACTIVITY (activity));
	g_return_if_fail (GTK_IS_DIALOG (error_dialog));

	object = G_OBJECT (error_dialog);
	primary_text = g_object_get_data (object, "primary");
	secondary_text = g_object_get_data (object, "secondary");

	e_activity_set_primary_text (activity, primary_text);
	e_activity_set_secondary_text (activity, secondary_text);
	e_activity_set_icon_name (activity, "dialog-warning");
	e_activity_set_clickable (activity, TRUE);

	g_signal_connect (
		activity, "cancelled",
		G_CALLBACK (e_activity_cancel_timeout), NULL);

	g_signal_connect (
		activity, "completed",
		G_CALLBACK (e_activity_cancel_timeout), NULL);

	g_signal_connect (
		activity, "clicked",
		G_CALLBACK (e_activity_complete), NULL);

	g_signal_connect_swapped (
		activity, "clicked",
		G_CALLBACK (gtk_dialog_run), error_dialog);

	g_signal_connect (
		activity, "timeout",
		G_CALLBACK (e_activity_complete), NULL);

	/* XXX Allow for a configurable timeout. */
	e_activity_add_timeout (activity, 60);
}

void
e_activity_info (EActivity *activity,
                 GtkWidget *info_dialog)
{
	GObject *object;
	const gchar *primary_text;
	const gchar *secondary_text;

	/* XXX Convert an activity to a clickable info message.
	 *     Clicking on the activity completes it and displays
	 *     the info dialog.  Eventually I'd like to eliminate
	 *     info dialogs altogether and show errors directly
	 *     in the shell window. */

	g_return_if_fail (E_IS_ACTIVITY (activity));
	g_return_if_fail (GTK_IS_DIALOG (info_dialog));

	object = G_OBJECT (info_dialog);
	primary_text = g_object_get_data (object, "primary");
	secondary_text = g_object_get_data (object, "secondary");

	e_activity_set_primary_text (activity, primary_text);
	e_activity_set_secondary_text (activity, secondary_text);
	e_activity_set_icon_name (activity, "dialog-warning");
	e_activity_set_clickable (activity, TRUE);

	g_signal_connect (
		activity, "cancelled",
		G_CALLBACK (e_activity_cancel_timeout), NULL);

	g_signal_connect (
		activity, "completed",
		G_CALLBACK (e_activity_cancel_timeout), NULL);

	g_signal_connect (
		activity, "clicked",
		G_CALLBACK (e_activity_complete), NULL);

	g_signal_connect_swapped (
		activity, "clicked",
		G_CALLBACK (gtk_dialog_run), info_dialog);

	g_signal_connect (
		activity, "timeout",
		G_CALLBACK (e_activity_complete), NULL);

	/* XXX Allow for a configurable timeout. */
	e_activity_add_timeout (activity, 60);
}

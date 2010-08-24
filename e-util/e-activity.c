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

#include <stdarg.h>
#include <glib/gi18n.h>

#include "e-util/e-util.h"

#define E_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACTIVITY, EActivityPrivate))

struct _EActivityPrivate {
	gchar *icon_name;
	gchar *primary_text;
	gchar *secondary_text;
	gdouble percent;

	guint allow_cancel	: 1;
	guint cancelled		: 1;
	guint clickable		: 1;
	guint completed		: 1;
};

enum {
	PROP_0,
	PROP_ALLOW_CANCEL,
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
	DESCRIBE,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EActivity,
	e_activity,
	G_TYPE_OBJECT)

static gboolean
activity_describe_accumulator (GSignalInvocationHint *ihint,
                               GValue *return_accu,
                               const GValue *handler_return,
                               gpointer accu_data)
{
	const gchar *string;

	string = g_value_get_string (handler_return);
	g_value_set_string (return_accu, string);

	return (string == NULL);
}

static void
activity_set_property (GObject *object,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_CANCEL:
			e_activity_set_allow_cancel (
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
		case PROP_ALLOW_CANCEL:
			g_value_set_boolean (
				value, e_activity_get_allow_cancel (
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

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_activity_parent_class)->finalize (object);
}

static void
activity_cancelled (EActivity *activity)
{
	activity->priv->cancelled = TRUE;
}

static void
activity_completed (EActivity *activity)
{
	activity->priv->completed = TRUE;
}

static void
activity_clicked (EActivity *activity)
{
	/* Allow subclasses to safely chain up. */
}

static gchar *
activity_describe (EActivity *activity)
{
	GString *string;
	const gchar *text;
	gboolean cancelled;
	gboolean completed;
	gdouble percent;

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
			(gint) (percent));
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
	object_class->finalize = activity_finalize;

	class->cancelled = activity_cancelled;
	class->completed = activity_completed;
	class->clicked = activity_clicked;
	class->describe = activity_describe;

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_CANCEL,
		g_param_spec_boolean (
			"allow-cancel",
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
		G_STRUCT_OFFSET (EActivityClass, cancelled),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CLICKED] = g_signal_new (
		"clicked",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EActivityClass, clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMPLETED] = g_signal_new (
		"completed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EActivityClass, completed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[DESCRIBE] = g_signal_new (
		"describe",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EActivityClass, describe),
		activity_describe_accumulator, NULL,
		e_marshal_STRING__VOID,
		G_TYPE_STRING, 0);
}

static void
e_activity_init (EActivity *activity)
{
	activity->priv = E_ACTIVITY_GET_PRIVATE (activity);
}

EActivity *
e_activity_new (const gchar *primary_text)
{
	return g_object_new (
		E_TYPE_ACTIVITY,
		"primary-text", primary_text, NULL);
}

EActivity *
e_activity_newv (const gchar *format, ...)
{
	EActivity *activity;
	gchar *primary_text;
	va_list args;

	va_start (args, format);
	primary_text = g_strdup_vprintf (format, args);
	activity = e_activity_new (primary_text);
	g_free (primary_text);
	va_end (args);

	return activity;
}

void
e_activity_cancel (EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (!activity->priv->allow_cancel)
		return;

	if (activity->priv->cancelled)
		return;

	if (activity->priv->completed)
		return;

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
	EActivityClass *class;

	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	class = E_ACTIVITY_GET_CLASS (activity);
	g_return_val_if_fail (class->describe != NULL, NULL);

	return class->describe (activity);
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

gboolean
e_activity_get_allow_cancel (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), FALSE);

	return activity->priv->allow_cancel;
}

void
e_activity_set_allow_cancel (EActivity *activity,
                            gboolean allow_cancel)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));

	activity->priv->allow_cancel = allow_cancel;

	g_object_notify (G_OBJECT (activity), "allow-cancel");
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

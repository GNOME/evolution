/*
 * e-timeout-activity.c
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

#include "e-timeout-activity.h"

#include <stdarg.h>

#define E_TIMEOUT_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TIMEOUT_ACTIVITY, ETimeoutActivityPrivate))

struct _ETimeoutActivityPrivate {
	guint timeout_id;
};

enum {
	TIMEOUT,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ETimeoutActivity,
	e_timeout_activity,
	E_TYPE_ACTIVITY)

static gboolean
timeout_activity_cb (ETimeoutActivity *timeout_activity)
{
	g_signal_emit (timeout_activity, signals[TIMEOUT], 0);

	return FALSE;
}

static void
timeout_activity_finalize (GObject *object)
{
	ETimeoutActivityPrivate *priv;

	priv = E_TIMEOUT_ACTIVITY_GET_PRIVATE (object);

	if (priv->timeout_id > 0)
		g_source_remove (priv->timeout_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_timeout_activity_parent_class)->finalize (object);
}

static void
timeout_activity_cancelled (EActivity *activity)
{
	ETimeoutActivityPrivate *priv;

	priv = E_TIMEOUT_ACTIVITY_GET_PRIVATE (activity);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	/* Chain up to parent's cancelled() method. */
	E_ACTIVITY_CLASS (e_timeout_activity_parent_class)->cancelled (activity);
}

static void
timeout_activity_completed (EActivity *activity)
{
	ETimeoutActivityPrivate *priv;

	priv = E_TIMEOUT_ACTIVITY_GET_PRIVATE (activity);

	if (priv->timeout_id > 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	/* Chain up to parent's completed() method. */
	E_ACTIVITY_CLASS (e_timeout_activity_parent_class)->completed (activity);
}

static void
timeout_activity_timeout (ETimeoutActivity *timeout_activity)
{
	/* Allow subclasses to safely chain up. */
}

static void
e_timeout_activity_class_init (ETimeoutActivityClass *class)
{
	GObjectClass *object_class;
	EActivityClass *activity_class;

	g_type_class_add_private (class, sizeof (ETimeoutActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = timeout_activity_finalize;

	activity_class = E_ACTIVITY_CLASS (class);
	activity_class->cancelled = timeout_activity_cancelled;
	activity_class->completed = timeout_activity_completed;

	class->timeout = timeout_activity_timeout;

	signals[TIMEOUT] = g_signal_new (
		"timeout",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ETimeoutActivityClass, timeout),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_timeout_activity_init (ETimeoutActivity *timeout_activity)
{
	timeout_activity->priv =
		E_TIMEOUT_ACTIVITY_GET_PRIVATE (timeout_activity);
}

EActivity *
e_timeout_activity_new (const gchar *primary_text)
{
	return g_object_new (
		E_TYPE_TIMEOUT_ACTIVITY,
		"primary-text", primary_text, NULL);
}

EActivity *
e_timeout_activity_newv (const gchar *format, ...)
{
	EActivity *activity;
	gchar *primary_text;
	va_list args;

	va_start (args, format);
	primary_text = g_strdup_vprintf (format, args);
	activity = e_timeout_activity_new (primary_text);
	g_free (primary_text);
	va_end (args);

	return activity;
}

void
e_timeout_activity_set_timeout (ETimeoutActivity *timeout_activity,
                                guint seconds)
{
	g_return_if_fail (E_IS_TIMEOUT_ACTIVITY (timeout_activity));

	if (timeout_activity->priv->timeout_id > 0)
		e_activity_cancel (E_ACTIVITY (timeout_activity));

	timeout_activity->priv->timeout_id = g_timeout_add_seconds (
		seconds, (GSourceFunc) timeout_activity_cb, timeout_activity);
}

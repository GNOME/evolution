/*
 * e-io-activity.c
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

#include "e-io-activity.h"

#define E_IO_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_IO_ACTIVITY, EIOActivityPrivate))

struct _EIOActivityPrivate {
	GAsyncResult *async_result;
	GCancellable *cancellable;
};

enum {
	PROP_0,
	PROP_ASYNC_RESULT,
	PROP_CANCELLABLE
};

G_DEFINE_TYPE (
	EIOActivity,
	e_io_activity,
	E_TYPE_ACTIVITY)

static void
io_activity_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ASYNC_RESULT:
			e_io_activity_set_async_result (
				E_IO_ACTIVITY (object),
				g_value_get_object (value));
			return;

		case PROP_CANCELLABLE:
			e_io_activity_set_cancellable (
				E_IO_ACTIVITY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
io_activity_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ASYNC_RESULT:
			g_value_set_object (
				value, e_io_activity_get_async_result (
				E_IO_ACTIVITY (object)));
			return;

		case PROP_CANCELLABLE:
			g_value_set_object (
				value, e_io_activity_get_cancellable (
				E_IO_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
io_activity_dispose (GObject *object)
{
	EIOActivityPrivate *priv;

	priv = E_IO_ACTIVITY_GET_PRIVATE (object);

	if (priv->async_result != NULL) {
		g_object_unref (priv->async_result);
		priv->async_result = NULL;
	}

	if (priv->cancellable != NULL) {
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_io_activity_parent_class)->dispose (object);
}

static void
io_activity_cancelled (EActivity *activity)
{
	EIOActivity *io_activity;
	GCancellable *cancellable;

	/* Chain up to parent's cancelled() method. */
	E_ACTIVITY_CLASS (e_io_activity_parent_class)->cancelled (activity);

	io_activity = E_IO_ACTIVITY (activity);
	cancellable = e_io_activity_get_cancellable (io_activity);

	if (cancellable != NULL)
		g_cancellable_cancel (cancellable);
}

static void
io_activity_completed (EActivity *activity)
{
	EIOActivity *io_activity;
	GAsyncResult *async_result;

	/* Chain up to parent's completed() method. */
	E_ACTIVITY_CLASS (e_io_activity_parent_class)->completed (activity);

	io_activity = E_IO_ACTIVITY (activity);
	async_result = e_io_activity_get_async_result (io_activity);

	/* We know how to invoke a GSimpleAsyncResult.  For any other
	 * type of GAsyncResult the caller will have to take measures
	 * to invoke it himself. */
	if (G_IS_SIMPLE_ASYNC_RESULT (async_result))
		g_simple_async_result_complete (
			G_SIMPLE_ASYNC_RESULT (async_result));
}

static void
e_io_activity_class_init (EIOActivityClass *class)
{
	GObjectClass *object_class;
	EActivityClass *activity_class;

	g_type_class_add_private (class, sizeof (EIOActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = io_activity_set_property;
	object_class->get_property = io_activity_get_property;
	object_class->dispose = io_activity_dispose;

	activity_class = E_ACTIVITY_CLASS (class);
	activity_class->cancelled = io_activity_cancelled;
	activity_class->completed = io_activity_completed;

	g_object_class_install_property (
		object_class,
		PROP_ASYNC_RESULT,
		g_param_spec_object (
			"async-result",
			"Asynchronous Result",
			NULL,
			G_TYPE_ASYNC_RESULT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CANCELLABLE,
		g_param_spec_object (
			"cancellable",
			"Cancellable",
			NULL,
			G_TYPE_CANCELLABLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_io_activity_init (EIOActivity *io_activity)
{
	io_activity->priv = E_IO_ACTIVITY_GET_PRIVATE (io_activity);
}

EActivity *
e_io_activity_new (const gchar *primary_text,
                   GAsyncResult *async_result,
                   GCancellable *cancellable)
{
	g_return_val_if_fail (primary_text != NULL, NULL);

	if (async_result != NULL)
		g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), NULL);

	if (cancellable != NULL)
		g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);

	return g_object_new (
		E_TYPE_IO_ACTIVITY,
		"async-result", async_result, "cancellable",
		cancellable, "primary-text", primary_text, NULL);
}

GAsyncResult *
e_io_activity_get_async_result (EIOActivity *io_activity)
{
	g_return_val_if_fail (E_IS_IO_ACTIVITY (io_activity), NULL);

	return io_activity->priv->async_result;
}

void
e_io_activity_set_async_result (EIOActivity *io_activity,
                                GAsyncResult *async_result)
{
	g_return_if_fail (E_IS_IO_ACTIVITY (io_activity));

	if (async_result != NULL) {
		g_return_if_fail (G_IS_ASYNC_RESULT (async_result));
		g_object_ref (async_result);
	}

	if (io_activity->priv->async_result != NULL)
		g_object_unref (io_activity->priv->async_result);

	io_activity->priv->async_result = async_result;

	g_object_notify (G_OBJECT (io_activity), "async-result");
}

GCancellable *
e_io_activity_get_cancellable (EIOActivity *io_activity)
{
	g_return_val_if_fail (E_IS_IO_ACTIVITY (io_activity), NULL);

	return io_activity->priv->cancellable;
}

void
e_io_activity_set_cancellable (EIOActivity *io_activity,
                               GCancellable *cancellable)
{
	g_return_if_fail (E_IS_IO_ACTIVITY (io_activity));

	if (cancellable != NULL) {
		g_return_if_fail (G_IS_CANCELLABLE (cancellable));
		g_object_ref (cancellable);
	}

	if (io_activity->priv->cancellable != NULL)
		g_object_unref (io_activity->priv->cancellable);

	io_activity->priv->cancellable = cancellable;

	g_object_freeze_notify (G_OBJECT (io_activity));

	e_activity_set_allow_cancel (
		E_ACTIVITY (io_activity), (cancellable != NULL));

	g_object_notify (G_OBJECT (io_activity), "cancellable");

	g_object_thaw_notify (G_OBJECT (io_activity));
}

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
	GObject *source_object;
	GCancellable *cancellable;
	GAsyncReadyCallback callback;
	gpointer user_data;
};

enum {
	PROP_0,
	PROP_CANCELLABLE
};

static gpointer parent_class;

static void
io_activity_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
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

	if (priv->source_object != NULL) {
		g_object_unref (priv->source_object);
		priv->source_object = NULL;
	}

	if (priv->cancellable != NULL) {
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
io_activity_cancelled (EActivity *activity)
{
	EIOActivityPrivate *priv;

	priv = E_IO_ACTIVITY_GET_PRIVATE (activity);

	/* Chain up to parent's cancelled() method. */
	E_ACTIVITY_CLASS (parent_class)->cancelled (activity);

	g_cancellable_cancel (priv->cancellable);
}

static void
io_activity_completed (EActivity *activity)
{
	EIOActivityPrivate *priv;

	priv = E_IO_ACTIVITY_GET_PRIVATE (activity);

	/* Chain up to parent's completed() method. */
	E_ACTIVITY_CLASS (parent_class)->completed (activity);

	/* Clear the function pointer after invoking it
	 * to guarantee it will not be invoked again. */
	if (priv->callback != NULL) {
		priv->callback (
			priv->source_object,
			G_ASYNC_RESULT (activity),
			priv->user_data);
		priv->callback = NULL;
	}
}

static gpointer
io_activity_get_user_data (GAsyncResult *async_result)
{
	EIOActivityPrivate *priv;

	priv = E_IO_ACTIVITY_GET_PRIVATE (async_result);

	return priv->user_data;
}

static GObject *
io_activity_get_source_object (GAsyncResult *async_result)
{
	EIOActivityPrivate *priv;

	priv = E_IO_ACTIVITY_GET_PRIVATE (async_result);

	return priv->source_object;
}

static void
io_activity_class_init (EIOActivityClass *class)
{
	GObjectClass *object_class;
	EActivityClass *activity_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EIOActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = io_activity_get_property;
	object_class->dispose = io_activity_dispose;

	activity_class = E_ACTIVITY_CLASS (class);
	activity_class->cancelled = io_activity_cancelled;
	activity_class->completed = io_activity_completed;

	g_object_class_install_property (
		object_class,
		PROP_CANCELLABLE,
		g_param_spec_object (
			"cancellable",
			"Cancellable",
			NULL,
			G_TYPE_CANCELLABLE,
			G_PARAM_READABLE));
}

static void
io_activity_iface_init (GAsyncResultIface *iface)
{
	iface->get_user_data = io_activity_get_user_data;
	iface->get_source_object = io_activity_get_source_object;
}

static void
io_activity_init (EIOActivity *io_activity)
{
	io_activity->priv = E_IO_ACTIVITY_GET_PRIVATE (io_activity);

	io_activity->priv->cancellable = g_cancellable_new ();
}

GType
e_io_activity_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EIOActivityClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) io_activity_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EIOActivity),
			0,     /* n_preallocs */
			(GInstanceInitFunc) io_activity_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo iface_info = {
			(GInterfaceInitFunc) io_activity_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			E_TYPE_ACTIVITY, "EIOActivity", &type_info, 0);

		g_type_add_interface_static (
			type, G_TYPE_ASYNC_RESULT, &iface_info);
	}

	return type;
}

EActivity *
e_io_activity_new (GObject *source_object,
                   const gchar *primary_text,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
	EActivity *activity;
	EIOActivityPrivate *priv;

	g_return_val_if_fail (G_IS_OBJECT (source_object), NULL);
	g_return_val_if_fail (primary_text != NULL, NULL);
	g_return_val_if_fail (callback != NULL, NULL);

	activity = g_object_new (
		E_TYPE_IO_ACTIVITY, "primary-text", primary_text, NULL);

	/* XXX Should these be construct properties? */
	priv = E_IO_ACTIVITY_GET_PRIVATE (activity);
	priv->source_object = g_object_ref (source_object);
	priv->callback = callback;
	priv->user_data = user_data;

	return activity;
}

GCancellable *
e_io_activity_get_cancellable (EIOActivity *io_activity)
{
	g_return_val_if_fail (E_IS_IO_ACTIVITY (io_activity), NULL);

	return io_activity->priv->cancellable;
}

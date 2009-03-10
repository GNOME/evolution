/*
 * e-file-activity.c
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

#include "e-file-activity.h"

#define E_FILE_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_FILE_ACTIVITY, EFileActivityPrivate))

struct _EFileActivityPrivate {
	GCancellable *cancellable;
	gulong handler_id;
};

enum {
	PROP_0,
	PROP_CANCELLABLE
};

static gpointer parent_class;

static void
file_activity_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CANCELLABLE:
			e_file_activity_set_cancellable (
				E_FILE_ACTIVITY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
file_activity_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CANCELLABLE:
			g_value_set_object (
				value, e_file_activity_get_cancellable (
				E_FILE_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
file_activity_dispose (GObject *object)
{
	EFileActivityPrivate *priv;

	priv = E_FILE_ACTIVITY_GET_PRIVATE (object);

	if (priv->cancellable != NULL) {
		g_signal_handler_disconnect (
			priv->cancellable, priv->handler_id);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
file_activity_cancelled (EActivity *activity)
{
	EFileActivity *file_activity;
	GCancellable *cancellable;

	file_activity = E_FILE_ACTIVITY (activity);
	cancellable = e_file_activity_get_cancellable (file_activity);
	g_cancellable_cancel (cancellable);

	/* Chain up to parent's cancelled() method. */
	E_ACTIVITY_CLASS (parent_class)->cancelled (activity);
}

static void
file_activity_class_init (EFileActivityClass *class)
{
	GObjectClass *object_class;
	EActivityClass *activity_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EFileActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = file_activity_set_property;
	object_class->get_property = file_activity_get_property;
	object_class->dispose = file_activity_dispose;

	activity_class = E_ACTIVITY_CLASS (class);
	activity_class->cancelled = file_activity_cancelled;

	g_object_class_install_property (
		object_class,
		PROP_CANCELLABLE,
		g_param_spec_object (
			"cancellable",
			"Cancellable",
			NULL,
			G_TYPE_CANCELLABLE,
			G_PARAM_READWRITE));
}

static void
file_activity_init (EFileActivity *file_activity)
{
	GCancellable *cancellable;

	file_activity->priv = E_FILE_ACTIVITY_GET_PRIVATE (file_activity);

	e_activity_set_allow_cancel (E_ACTIVITY (file_activity), TRUE);

	cancellable = g_cancellable_new ();
	e_file_activity_set_cancellable (file_activity, cancellable);
	g_object_unref (cancellable);
}

GType
e_file_activity_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EFileActivityClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) file_activity_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EFileActivity),
			0,     /* n_preallocs */
			(GInstanceInitFunc) file_activity_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_ACTIVITY, "EFileActivity", &type_info, 0);
	}

	return type;
}

EActivity *
e_file_activity_new (const gchar *primary_text)
{
	return g_object_new (
		E_TYPE_FILE_ACTIVITY,
		"primary-text", primary_text, NULL);
}

GCancellable *
e_file_activity_get_cancellable (EFileActivity *file_activity)
{
	g_return_val_if_fail (E_IS_FILE_ACTIVITY (file_activity), NULL);

	return file_activity->priv->cancellable;
}

void
e_file_activity_set_cancellable (EFileActivity *file_activity,
                                 GCancellable *cancellable)
{
	g_return_if_fail (E_IS_FILE_ACTIVITY (file_activity));

	if (cancellable != NULL) {
		g_return_if_fail (G_IS_CANCELLABLE (cancellable));
		g_object_ref (cancellable);
	}

	if (file_activity->priv->cancellable != NULL) {
		g_signal_handler_disconnect (
			file_activity->priv->cancellable,
			file_activity->priv->handler_id);
		g_object_unref (file_activity->priv->cancellable);
		file_activity->priv->handler_id = 0;
	}

	file_activity->priv->cancellable = cancellable;

	if (cancellable != NULL)
		file_activity->priv->handler_id =
			g_signal_connect_swapped (
				cancellable, "cancelled",
				G_CALLBACK (e_activity_cancel),
				file_activity);

	g_object_notify (G_OBJECT (file_activity), "cancellable");
}

void
e_file_activity_progress (goffset current_num_bytes,
                          goffset total_num_bytes,
                          gpointer activity)
{
	gdouble percent = -1.0;

	g_return_if_fail (E_IS_ACTIVITY (activity));

	if (current_num_bytes > 0 && total_num_bytes > 0)
		percent = (gdouble) current_num_bytes / total_num_bytes;

	e_activity_set_percent (activity, percent);
}

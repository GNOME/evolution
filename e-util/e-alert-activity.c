/*
 * e-alert-activity.c
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
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#include "e-alert-activity.h"
#include "e-alert-dialog.h"

#define E_ALERT_ACTIVITY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ALERT_ACTIVITY, EAlertActivityPrivate))

struct _EAlertActivityPrivate {
	GtkWidget *message_dialog;
};

enum {
	PROP_0,
	PROP_MESSAGE_DIALOG
};

G_DEFINE_TYPE (
	EAlertActivity,
	e_alert_activity,
	E_TYPE_TIMEOUT_ACTIVITY)

static void
alert_activity_set_message_dialog (EAlertActivity *alert_activity,
                                   GtkWidget *message_dialog)
{
	g_return_if_fail (alert_activity->priv->message_dialog == NULL);

	alert_activity->priv->message_dialog = g_object_ref (message_dialog);
}

static void
alert_activity_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MESSAGE_DIALOG:
			alert_activity_set_message_dialog (
				E_ALERT_ACTIVITY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_activity_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MESSAGE_DIALOG:
			g_value_set_object (
				value, e_alert_activity_get_message_dialog (
				E_ALERT_ACTIVITY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_activity_dispose (GObject *object)
{
	EAlertActivityPrivate *priv;

	priv = E_ALERT_ACTIVITY_GET_PRIVATE (object);

	if (priv->message_dialog != NULL) {
		g_object_unref (priv->message_dialog);
		priv->message_dialog = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_activity_parent_class)->dispose (object);
}

static void
alert_activity_constructed (GObject *object)
{
	EActivity *activity;
	EAlertActivity *alert_activity;
	EAlert *alert;
	GtkWidget *message_dialog;
	gchar *primary_text;
	gchar *secondary_text;

	alert_activity = E_ALERT_ACTIVITY (object);
	message_dialog = e_alert_activity_get_message_dialog (alert_activity);

	alert = e_alert_dialog_get_alert (E_ALERT_DIALOG (message_dialog));
	primary_text = e_alert_get_primary_text (alert, FALSE);
	secondary_text = e_alert_get_secondary_text (alert, FALSE);
	g_object_unref (alert);

	activity = E_ACTIVITY (alert_activity);
	e_activity_set_primary_text (activity, primary_text);
	e_activity_set_secondary_text (activity, secondary_text);

	g_free (primary_text);
	g_free (secondary_text);

	/* This is a constructor property, so can't do it in init().
	 * XXX What we really want to do is override the property's
	 *     default value, but GObject does not support that. */
	e_activity_set_clickable (E_ACTIVITY (alert_activity), TRUE);
}

static void
alert_activity_clicked (EActivity *activity)
{
	EAlertActivity *alert_activity;
	GtkWidget *message_dialog;

	e_activity_complete (activity);

	alert_activity = E_ALERT_ACTIVITY (activity);
	message_dialog = e_alert_activity_get_message_dialog (alert_activity);
	gtk_dialog_run (GTK_DIALOG (message_dialog));
	gtk_widget_hide (message_dialog);

	/* Chain up to parent's clicked() method. */
	E_ACTIVITY_CLASS (e_alert_activity_parent_class)->clicked (activity);
}

static void
alert_activity_timeout (ETimeoutActivity *activity)
{
	e_activity_complete (E_ACTIVITY (activity));

	/* Chain up to parent's timeout() method. */
	E_TIMEOUT_ACTIVITY_CLASS (e_alert_activity_parent_class)->timeout (activity);
}

static void
e_alert_activity_class_init (EAlertActivityClass *class)
{
	GObjectClass *object_class;
	EActivityClass *activity_class;
	ETimeoutActivityClass *timeout_activity_class;

	g_type_class_add_private (class, sizeof (EAlertActivityPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = alert_activity_set_property;
	object_class->get_property = alert_activity_get_property;
	object_class->dispose = alert_activity_dispose;
	object_class->constructed = alert_activity_constructed;

	activity_class = E_ACTIVITY_CLASS (class);
	activity_class->clicked = alert_activity_clicked;

	timeout_activity_class = E_TIMEOUT_ACTIVITY_CLASS (class);
	timeout_activity_class->timeout = alert_activity_timeout;

	g_object_class_install_property (
		object_class,
		PROP_MESSAGE_DIALOG,
		g_param_spec_object (
			"message-dialog",
			NULL,
			NULL,
			GTK_TYPE_DIALOG,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_alert_activity_init (EAlertActivity *alert_activity)
{
	alert_activity->priv = E_ALERT_ACTIVITY_GET_PRIVATE (alert_activity);

	e_timeout_activity_set_timeout (E_TIMEOUT_ACTIVITY (alert_activity), 60);
}

EActivity *
e_alert_activity_new_info (GtkWidget *message_dialog)
{
	g_return_val_if_fail (GTK_IS_DIALOG (message_dialog), NULL);

	return g_object_new (
		E_TYPE_ALERT_ACTIVITY,
		"icon-name", "dialog-information",
		"message-dialog", message_dialog, NULL);
}

EActivity *
e_alert_activity_new_error (GtkWidget *message_dialog)
{
	g_return_val_if_fail (GTK_IS_DIALOG (message_dialog), NULL);

	return g_object_new (
		E_TYPE_ALERT_ACTIVITY,
		"icon-name", "dialog-error",
		"message-dialog", message_dialog, NULL);
}

EActivity *
e_alert_activity_new_warning (GtkWidget *message_dialog)
{
	g_return_val_if_fail (GTK_IS_DIALOG (message_dialog), NULL);

	return g_object_new (
		E_TYPE_ALERT_ACTIVITY,
		"icon-name", "dialog-warning",
		"message-dialog", message_dialog, NULL);
}

GtkWidget *
e_alert_activity_get_message_dialog (EAlertActivity *alert_activity)
{
	g_return_val_if_fail (E_IS_ALERT_ACTIVITY (alert_activity), NULL);

	return alert_activity->priv->message_dialog;
}

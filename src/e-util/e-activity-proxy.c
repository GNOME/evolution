/*
 * e-activity-proxy.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-misc-utils.h"
#include "e-spinner.h"

#include "e-activity-proxy.h"

#define FEEDBACK_PERIOD		1 /* seconds */
#define COMPLETED_ICON_NAME	"emblem-default"

struct _EActivityProxyPrivate {
	EActivity *activity;	/* weak reference */
	GtkWidget *image;	/* not referenced */
	GtkWidget *label;	/* not referenced */
	GtkWidget *cancel;	/* not referenced */
	GtkWidget *spinner;	/* not referenced */

	/* If the user clicks the Cancel button, keep the cancelled
	 * EActivity object alive for a short duration so the user
	 * gets some visual feedback that cancellation worked. */
	guint timeout_id;
};

enum {
	PROP_0,
	PROP_ACTIVITY
};

G_DEFINE_TYPE_WITH_PRIVATE (EActivityProxy, e_activity_proxy, GTK_TYPE_FRAME)

typedef struct {
	EActivityProxy *proxy; /* Not referenced */
	EActivity *activity;   /* Referenced */
} UnsetTimeoutData;

static void
unset_timeout_data_free (gpointer ptr)
{
	UnsetTimeoutData *utd = ptr;

	if (utd) {
		g_object_unref (utd->activity);
		g_slice_free (UnsetTimeoutData, utd);
	}
}

static gboolean
activity_proxy_unset_timeout_id (gpointer user_data)
{
	UnsetTimeoutData *utd = user_data;

	g_return_val_if_fail (utd != NULL, FALSE);

	if (g_source_is_destroyed (g_main_current_source ()))
		return FALSE;

	g_return_val_if_fail (E_IS_ACTIVITY_PROXY (utd->proxy), FALSE);

	if (g_source_get_id (g_main_current_source ()) == utd->proxy->priv->timeout_id)
		utd->proxy->priv->timeout_id = 0;

	return FALSE;
}

static void
activity_proxy_feedback (EActivityProxy *proxy)
{
	EActivity *activity;
	EActivityState state;
	UnsetTimeoutData *utd;

	activity = e_activity_proxy_get_activity (proxy);
	g_return_if_fail (E_IS_ACTIVITY (activity));

	state = e_activity_get_state (activity);
	if (state != E_ACTIVITY_CANCELLED)
		return;

	if (proxy->priv->timeout_id > 0)
		g_source_remove (proxy->priv->timeout_id);

	utd = g_slice_new0 (UnsetTimeoutData);
	utd->proxy = proxy;
	/* Hold a reference on the EActivity for a short
	 * period so the activity proxy stays visible. */
	utd->activity = g_object_ref (activity);

	proxy->priv->timeout_id = e_named_timeout_add_seconds_full (
		G_PRIORITY_LOW, FEEDBACK_PERIOD, activity_proxy_unset_timeout_id,
		utd, unset_timeout_data_free);
}

static void
activity_proxy_update (EActivityProxy *proxy)
{
	EActivity *activity;
	EActivityState state;
	GCancellable *cancellable;
	const gchar *icon_name;
	gboolean sensitive;
	gboolean visible;
	gchar *description;

	activity = e_activity_proxy_get_activity (proxy);

	if (activity == NULL) {
		gtk_widget_hide (GTK_WIDGET (proxy));
		return;
	}

	cancellable = e_activity_get_cancellable (activity);
	icon_name = e_activity_get_icon_name (activity);
	state = e_activity_get_state (activity);

	description = e_activity_describe (activity);
	gtk_widget_set_tooltip_text (GTK_WIDGET (proxy), description);
	gtk_label_set_text (GTK_LABEL (proxy->priv->label), description);

	if (state == E_ACTIVITY_CANCELLED) {
		PangoAttribute *attr;
		PangoAttrList *attr_list;

		attr_list = pango_attr_list_new ();

		attr = pango_attr_strikethrough_new (TRUE);
		pango_attr_list_insert (attr_list, attr);

		gtk_label_set_attributes (
			GTK_LABEL (proxy->priv->label), attr_list);

		pango_attr_list_unref (attr_list);
	} else
		gtk_label_set_attributes (
			GTK_LABEL (proxy->priv->label), NULL);

	if (state == E_ACTIVITY_COMPLETED)
		icon_name = COMPLETED_ICON_NAME;

	if (state == E_ACTIVITY_CANCELLED) {
		gtk_image_set_from_icon_name (
			GTK_IMAGE (proxy->priv->image),
			"process-stop", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (proxy->priv->image);
	} else if (icon_name != NULL) {
		gtk_image_set_from_icon_name (
			GTK_IMAGE (proxy->priv->image),
			icon_name, GTK_ICON_SIZE_MENU);
		gtk_widget_show (proxy->priv->image);
	} else {
		gtk_widget_hide (proxy->priv->image);
	}

	visible = (cancellable != NULL);
	gtk_widget_set_visible (proxy->priv->cancel, visible);

	sensitive = (state == E_ACTIVITY_RUNNING);
	gtk_widget_set_sensitive (proxy->priv->cancel, sensitive);

	visible = (description != NULL && *description != '\0');
	gtk_widget_set_visible (GTK_WIDGET (proxy), visible);

	g_free (description);
}

static void
activity_proxy_cancel (EActivityProxy *proxy)
{
	EActivity *activity;

	activity = e_activity_proxy_get_activity (proxy);
	g_return_if_fail (E_IS_ACTIVITY (activity));

	e_activity_cancel (activity);

	activity_proxy_update (proxy);
}

static void
activity_proxy_weak_notify_cb (EActivityProxy *proxy,
                               GObject *where_the_object_was)
{
	g_return_if_fail (E_IS_ACTIVITY_PROXY (proxy));

	proxy->priv->activity = NULL;
	e_activity_proxy_set_activity (proxy, NULL);
}

static void
activity_proxy_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			e_activity_proxy_set_activity (
				E_ACTIVITY_PROXY (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_proxy_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			g_value_set_object (
				value, e_activity_proxy_get_activity (
				E_ACTIVITY_PROXY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
activity_proxy_dispose (GObject *object)
{
	EActivityProxy *self = E_ACTIVITY_PROXY (object);

	if (self->priv->timeout_id > 0) {
		g_source_remove (self->priv->timeout_id);
		self->priv->timeout_id = 0;
	}

	if (self->priv->activity != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->activity, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_weak_unref (
			G_OBJECT (self->priv->activity), (GWeakNotify)
			activity_proxy_weak_notify_cb, object);
		self->priv->activity = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_activity_proxy_parent_class)->dispose (object);
}

static void
e_activity_proxy_class_init (EActivityProxyClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = activity_proxy_set_property;
	object_class->get_property = activity_proxy_get_property;
	object_class->dispose = activity_proxy_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVITY,
		g_param_spec_object (
			"activity",
			NULL,
			NULL,
			E_TYPE_ACTIVITY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
e_activity_proxy_init (EActivityProxy *proxy)
{
	GtkWidget *container;
	GtkWidget *widget;

	proxy->priv = e_activity_proxy_get_instance_private (proxy);

	gtk_frame_set_shadow_type (GTK_FRAME (proxy), GTK_SHADOW_IN);

	container = GTK_WIDGET (proxy);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	proxy->priv->image = widget;

	widget = e_spinner_new ();
	e_spinner_start (E_SPINNER (widget));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 3);
	proxy->priv->spinner = widget;

	/* The spinner is only visible when the image is not. */
	e_binding_bind_property (
		proxy->priv->image, "visible",
		proxy->priv->spinner, "visible",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE |
		G_BINDING_INVERT_BOOLEAN);

	widget = gtk_label_new (NULL);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	proxy->priv->label = widget;
	gtk_widget_show (widget);

	/* This is only shown if the EActivity has a GCancellable. */
	widget = e_dialog_button_new_with_icon ("process-stop", NULL);
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text (widget, _("Cancel"));
	proxy->priv->cancel = widget;
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (activity_proxy_cancel), proxy);
}

GtkWidget *
e_activity_proxy_new (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return g_object_new (
		E_TYPE_ACTIVITY_PROXY, "activity", activity, NULL);
}

EActivity *
e_activity_proxy_get_activity (EActivityProxy *proxy)
{
	g_return_val_if_fail (E_IS_ACTIVITY_PROXY (proxy), NULL);

	return proxy->priv->activity;
}

void
e_activity_proxy_set_activity (EActivityProxy *proxy,
                               EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY_PROXY (proxy));

	if (activity != NULL)
		g_return_if_fail (E_IS_ACTIVITY (activity));

	if (proxy->priv->timeout_id > 0) {
		g_source_remove (proxy->priv->timeout_id);
		proxy->priv->timeout_id = 0;
	}

	if (proxy->priv->activity != NULL) {
		g_signal_handlers_disconnect_matched (
			proxy->priv->activity, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, proxy);
		g_object_weak_unref (
			G_OBJECT (proxy->priv->activity),
			(GWeakNotify) activity_proxy_weak_notify_cb, proxy);
	}

	proxy->priv->activity = activity;

	if (activity != NULL) {
		g_object_weak_ref (
			G_OBJECT (activity), (GWeakNotify)
			activity_proxy_weak_notify_cb, proxy);

		g_signal_connect_swapped (
			activity, "notify::state",
			G_CALLBACK (activity_proxy_feedback), proxy);

		g_signal_connect_swapped (
			activity, "notify",
			G_CALLBACK (activity_proxy_update), proxy);
	}

	activity_proxy_update (proxy);

	g_object_notify (G_OBJECT (proxy), "activity");
}

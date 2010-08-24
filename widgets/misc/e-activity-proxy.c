/*
 * e-activity-proxy.c
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

#include "e-activity-proxy.h"

#include <glib/gi18n.h>

#define E_ACTIVITY_PROXY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACTIVITY_PROXY, EActivityProxyPrivate))

struct _EActivityProxyPrivate {
	EActivity *activity;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *cancel;
	GtkWidget *spinner;
};

enum {
	PROP_0,
	PROP_ACTIVITY
};

G_DEFINE_TYPE (
	EActivityProxy,
	e_activity_proxy,
	GTK_TYPE_EVENT_BOX)

static void
activity_proxy_update (EActivityProxy *proxy)
{
	EActivity *activity = proxy->priv->activity;
	const gchar *icon_name;
	gboolean allow_cancel;
	gboolean cancelled;
	gboolean clickable;
	gboolean completed;
	gboolean sensitive;
	gchar *description;

	allow_cancel = e_activity_get_allow_cancel (activity);
	cancelled = e_activity_is_cancelled (activity);
	clickable = e_activity_get_clickable (activity);
	completed = e_activity_is_completed (activity);
	icon_name = e_activity_get_icon_name (activity);

	description = e_activity_describe (activity);
	gtk_widget_set_tooltip_text (GTK_WIDGET (proxy), description);
	gtk_label_set_text (GTK_LABEL (proxy->priv->label), description);
	g_free (description);

	/* Note, an activity requires an icon name in order to
	 * be clickable.  We don't support spinner buttons. */
	if (icon_name != NULL) {
		gtk_image_set_from_icon_name (
			GTK_IMAGE (proxy->priv->image),
			icon_name, GTK_ICON_SIZE_MENU);
		gtk_button_set_image (
			GTK_BUTTON (proxy->priv->button),
			gtk_image_new_from_icon_name (
			icon_name, GTK_ICON_SIZE_MENU));
		gtk_widget_hide (proxy->priv->spinner);
		if (clickable) {
			gtk_widget_show (proxy->priv->button);
			gtk_widget_hide (proxy->priv->image);
		} else {
			gtk_widget_hide (proxy->priv->button);
			gtk_widget_show (proxy->priv->image);
		}
	} else {
		gtk_widget_show (proxy->priv->spinner);
		gtk_widget_hide (proxy->priv->button);
		gtk_widget_hide (proxy->priv->image);
	}

	if (allow_cancel)
		gtk_widget_show (proxy->priv->cancel);
	else
		gtk_widget_hide (proxy->priv->cancel);

	sensitive = !(cancelled || completed);
	gtk_widget_set_sensitive (proxy->priv->cancel, sensitive);
}

static void
activity_proxy_set_activity (EActivityProxy *proxy,
                             EActivity *activity)
{
	g_return_if_fail (proxy->priv->activity == NULL);

	proxy->priv->activity = g_object_ref (activity);
}

static void
activity_proxy_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			activity_proxy_set_activity (
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
	EActivityProxyPrivate *priv;

	priv = E_ACTIVITY_PROXY_GET_PRIVATE (object);

	if (priv->activity != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->activity, G_SIGNAL_MATCH_FUNC, 0, 0,
			NULL, activity_proxy_update, NULL);
		g_object_unref (priv->activity);
		priv->activity = NULL;
	}

	if (priv->button != NULL) {
		g_object_unref (priv->button);
		priv->button = NULL;
	}

	if (priv->image != NULL) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->label != NULL) {
		g_object_unref (priv->label);
		priv->label = NULL;
	}

	if (priv->cancel != NULL) {
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->spinner != NULL) {
		g_object_unref (priv->spinner);
		priv->spinner = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_activity_proxy_parent_class)->dispose (object);
}

static void
activity_proxy_constructed (GObject *object)
{
	EActivityProxy *proxy;

	proxy = E_ACTIVITY_PROXY (object);

	g_signal_connect_swapped (
		proxy->priv->button, "clicked",
		G_CALLBACK (e_activity_clicked), proxy->priv->activity);

	g_signal_connect_swapped (
		proxy->priv->cancel, "clicked",
		G_CALLBACK (e_activity_cancel), proxy->priv->activity);

	g_signal_connect_swapped (
		proxy->priv->activity, "cancelled",
		G_CALLBACK (activity_proxy_update), proxy);

	g_signal_connect_swapped (
		proxy->priv->activity, "completed",
		G_CALLBACK (activity_proxy_update), proxy);

	g_signal_connect_swapped (
		proxy->priv->activity, "notify",
		G_CALLBACK (activity_proxy_update), proxy);

	activity_proxy_update (proxy);
}

static void
e_activity_proxy_class_init (EActivityProxyClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EActivityProxyPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = activity_proxy_set_property;
	object_class->get_property = activity_proxy_get_property;
	object_class->dispose = activity_proxy_dispose;
	object_class->constructed = activity_proxy_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVITY,
		g_param_spec_object (
			"activity",
			NULL,
			NULL,
			E_TYPE_ACTIVITY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_activity_proxy_init (EActivityProxy *proxy)
{
	GtkWidget *container;
	GtkWidget *widget;

	proxy->priv = E_ACTIVITY_PROXY_GET_PRIVATE (proxy);

	container = GTK_WIDGET (proxy);

	widget = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_hbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	proxy->priv->image = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	proxy->priv->button = g_object_ref (widget);
	gtk_widget_hide (widget);

	widget = gtk_spinner_new ();
	gtk_spinner_start (GTK_SPINNER (widget));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 3);
	proxy->priv->spinner = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	proxy->priv->label = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_STOP, GTK_ICON_SIZE_MENU));
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text (widget, _("Cancel"));
	proxy->priv->cancel = g_object_ref (widget);
	gtk_widget_show (widget);
}

GtkWidget *
e_activity_proxy_new (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return g_object_new (
		E_TYPE_ACTIVITY_PROXY,
		"activity", activity, NULL);
}

EActivity *
e_activity_proxy_get_activity (EActivityProxy *proxy)
{
	g_return_val_if_fail (E_IS_ACTIVITY_PROXY (proxy), NULL);

	return proxy->priv->activity;
}

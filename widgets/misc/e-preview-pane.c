/*
 * e-preview-pane.c
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

#include "e-preview-pane.h"

#include <gdk/gdkkeysyms.h>

#define E_PREVIEW_PANE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PREVIEW_PANE, EPreviewPanePrivate))

struct _EPreviewPanePrivate {
	GtkWidget *web_view;
	GtkWidget *search_bar;
};

enum {
	PROP_0,
	PROP_SEARCH_BAR,
	PROP_WEB_VIEW
};

enum {
	SHOW_SEARCH_BAR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EPreviewPane,
	e_preview_pane,
	GTK_TYPE_VBOX)

static void
preview_pane_set_web_view (EPreviewPane *preview_pane,
                           EWebView *web_view)
{
	g_return_if_fail (E_IS_WEB_VIEW (web_view));
	g_return_if_fail (preview_pane->priv->web_view == NULL);

	preview_pane->priv->web_view = g_object_ref_sink (web_view);
}

static void
preview_pane_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WEB_VIEW:
			preview_pane_set_web_view (
				E_PREVIEW_PANE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
preview_pane_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SEARCH_BAR:
			g_value_set_object (
				value, e_preview_pane_get_search_bar (
				E_PREVIEW_PANE (object)));
			return;

		case PROP_WEB_VIEW:
			g_value_set_object (
				value, e_preview_pane_get_web_view (
				E_PREVIEW_PANE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
preview_pane_dispose (GObject *object)
{
	EPreviewPanePrivate *priv;

	priv = E_PREVIEW_PANE_GET_PRIVATE (object);

	if (priv->search_bar != NULL) {
		g_object_unref (priv->search_bar);
		priv->search_bar = NULL;
	}

	if (priv->web_view != NULL) {
		g_object_unref (priv->web_view);
		priv->web_view = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_preview_pane_parent_class)->dispose (object);
}

static void
preview_pane_constructed (GObject *object)
{
	EPreviewPanePrivate *priv;
	GtkWidget *widget;

	priv = E_PREVIEW_PANE_GET_PRIVATE (object);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (object), widget, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (widget), priv->web_view);
	gtk_widget_show (widget);

	widget = e_search_bar_new (E_WEB_VIEW (priv->web_view));
	gtk_box_pack_start (GTK_BOX (object), widget, FALSE, FALSE, 0);
	priv->search_bar = g_object_ref (widget);
	gtk_widget_hide (widget);
}

static void
preview_pane_show_search_bar (EPreviewPane *preview_pane)
{
	GtkWidget *search_bar;

	search_bar = preview_pane->priv->search_bar;

	if (!gtk_widget_get_visible (search_bar))
		gtk_widget_show (search_bar);
}

static void
e_preview_pane_class_init (EPreviewPaneClass *class)
{
	GObjectClass *object_class;
	GtkBindingSet *binding_set;

	g_type_class_add_private (class, sizeof (EPreviewPanePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = preview_pane_set_property;
	object_class->get_property = preview_pane_get_property;
	object_class->dispose = preview_pane_dispose;
	object_class->constructed = preview_pane_constructed;

	class->show_search_bar = preview_pane_show_search_bar;

	g_object_class_install_property (
		object_class,
		PROP_SEARCH_BAR,
		g_param_spec_object (
			"search-bar",
			"Search Bar",
			NULL,
			E_TYPE_SEARCH_BAR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_WEB_VIEW,
		g_param_spec_object (
			"web-view",
			"Web View",
			NULL,
			E_TYPE_WEB_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[SHOW_SEARCH_BAR] = g_signal_new (
		"show-search-bar",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EPreviewPaneClass, show_search_bar),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);

	gtk_binding_entry_add_signal (
		binding_set, GDK_f, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
		"show-search-bar", 0);
}

static void
e_preview_pane_init (EPreviewPane *preview_pane)
{
	preview_pane->priv = E_PREVIEW_PANE_GET_PRIVATE (preview_pane);

	gtk_box_set_spacing (GTK_BOX (preview_pane), 1);
}

GtkWidget *
e_preview_pane_new (EWebView *web_view)
{
	g_return_val_if_fail (E_IS_WEB_VIEW (web_view), NULL);

	return g_object_new (
		E_TYPE_PREVIEW_PANE,
		"web-view", web_view, NULL);
}

EWebView *
e_preview_pane_get_web_view (EPreviewPane *preview_pane)
{
	g_return_val_if_fail (E_IS_PREVIEW_PANE (preview_pane), NULL);

	return E_WEB_VIEW (preview_pane->priv->web_view);
}

ESearchBar *
e_preview_pane_get_search_bar (EPreviewPane *preview_pane)
{
	g_return_val_if_fail (E_IS_PREVIEW_PANE (preview_pane), NULL);

	return E_SEARCH_BAR (preview_pane->priv->search_bar);
}

void
e_preview_pane_show_search_bar (EPreviewPane *preview_pane)
{
	g_return_if_fail (E_IS_PREVIEW_PANE (preview_pane));

	g_signal_emit (preview_pane, signals[SHOW_SEARCH_BAR], 0);
}

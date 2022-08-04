/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-menu-bar.h"

struct _EMenuBarPrivate {
	GtkWidget *inner_menu_bar;

	guint visible : 1;
	gulong delayed_show_id;
	gulong delayed_hide_id;
};

G_DEFINE_TYPE_WITH_CODE (EMenuBar, e_menu_bar, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EMenuBar))


enum {
	PROP_0,
	PROP_VISIBLE
};

static void
menu_bar_visible_settings_changed_cb (GSettings *settings,
				      const gchar *key,
				      gpointer data)
{
	g_return_if_fail (E_IS_MENU_BAR (data));

	e_menu_bar_set_visible (
		E_MENU_BAR (data),
		g_settings_get_boolean (settings, key));
}

static void
menu_bar_dispose (GObject *menu_bar)
{
	EMenuBar *self = E_MENU_BAR (menu_bar);

	if (self->priv->delayed_show_id) {
		g_source_remove (self->priv->delayed_show_id);
		self->priv->delayed_show_id = 0;
	}

	if (self->priv->delayed_hide_id) {
		g_source_remove (self->priv->delayed_hide_id);
		self->priv->delayed_hide_id = 0;
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_menu_bar_parent_class)->dispose (menu_bar);
}

static void
menu_bar_set_property (GObject *object,
		       guint property_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	EMenuBar *menubar = E_MENU_BAR (object);

	switch (property_id) {
		case PROP_VISIBLE:
			e_menu_bar_set_visible (
				menubar,
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_bar_get_property (GObject *object,
		       guint property_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	EMenuBar *menubar = E_MENU_BAR (object);

	switch (property_id) {
		case PROP_VISIBLE:
			g_value_set_boolean (
				value, e_menu_bar_get_visible (menubar));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_menu_bar_class_init (EMenuBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = menu_bar_dispose;
	object_class->set_property = menu_bar_set_property;
	object_class->get_property = menu_bar_get_property;

	g_object_class_install_property (
		object_class,
		PROP_VISIBLE,
		g_param_spec_boolean (
			"visible",
			"Visible",
			"Inner menubar visible",
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_menu_bar_init (EMenuBar *self)
{
	self->priv = e_menu_bar_get_instance_private (self);
}

static gboolean
delayed_show_cb (gpointer user_data)
{
	EMenuBar *self = user_data;

	self->priv->delayed_show_id = 0;

	if (!self->priv->visible) {
		gtk_widget_set_visible (GTK_WIDGET (self->priv->inner_menu_bar), TRUE);
	}

	return FALSE;
}

static gboolean
delayed_hide_cb (gpointer user_data)
{
	EMenuBar *self = user_data;
	GtkWidget *widget = GTK_WIDGET (self->priv->inner_menu_bar);

	self->priv->delayed_hide_id = 0;

	if (!self->priv->visible &&
	    !self->priv->delayed_show_id) {
		if (gtk_widget_get_visible (widget) &&
		    !gtk_menu_shell_get_selected_item (GTK_MENU_SHELL (self->priv->inner_menu_bar)))
			gtk_widget_set_visible (widget, FALSE);
	}

	return FALSE;
}

static void
e_menu_bar_window_event_after_cb (GtkWindow *window,
			          GdkEvent *event,
			          EMenuBar *self)
{
	g_return_if_fail (event != NULL);

	if (event->type != GDK_KEY_PRESS &&
	    event->type != GDK_KEY_RELEASE &&
	    event->type != GDK_BUTTON_RELEASE &&
	    event->type != GDK_FOCUS_CHANGE)
		return;

	if (event->type == GDK_KEY_PRESS) {
		GdkEventKey *key_event;

		key_event = (GdkEventKey *) event;

		if ((key_event->keyval == GDK_KEY_Alt_L || key_event->keyval == GDK_KEY_Alt_R) &&
		    !(key_event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
				          GDK_SUPER_MASK | GDK_HYPER_MASK |
				          GDK_META_MASK))) {
			if (self->priv->delayed_hide_id) {
				g_source_remove (self->priv->delayed_hide_id);
				self->priv->delayed_hide_id = 0;
			}

			if (self->priv->delayed_show_id) {
				g_source_remove (self->priv->delayed_show_id);
				self->priv->delayed_show_id = 0;

				delayed_show_cb (self);
			} else {
				/* To not flash when using Alt+Tab or
				   similar system-wide shortcuts */
				self->priv->delayed_show_id =
					g_timeout_add (250, delayed_show_cb, self);
			}
		}
	} else if (event->type != GDK_BUTTON_RELEASE || !(event->button.state & GDK_MOD1_MASK)) {
		if (self->priv->delayed_show_id) {
			g_source_remove (self->priv->delayed_show_id);
			self->priv->delayed_show_id = 0;
		}

		if (gtk_widget_get_visible (GTK_WIDGET (self->priv->inner_menu_bar)) &&
		    !self->priv->delayed_hide_id) {
			self->priv->delayed_hide_id =
				g_timeout_add (500, delayed_hide_cb, self);
		}
	}
}

/**
 * e_menu_bar_new:
 * @inner_menu_bar: #GtkMenuBar to handle
 * @window: monitor #GtkWindow for &lt;Alt&gt; key event
 *
 * Creates a new #EMenuBar showing @inner_menu_bar on demand
 *
 * Returns: (transfer full): a new #EMenuBar
 *
 * Since: 3.46
 **/
GtkWidget *
e_menu_bar_new (GtkMenuBar *inner_menu_bar,
		GtkWindow *window)
{
	GtkWidget *menu_bar;
	GSettings *settings;

	g_return_val_if_fail (GTK_IS_MENU_BAR (inner_menu_bar), NULL);
	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	menu_bar = g_object_new (E_TYPE_MENU_BAR, NULL);
	E_MENU_BAR (menu_bar)->priv->inner_menu_bar = GTK_WIDGET (inner_menu_bar);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_signal_connect_object (
		settings, "changed::menubar-visible",
		G_CALLBACK (menu_bar_visible_settings_changed_cb),
		menu_bar, 0);
	e_menu_bar_set_visible (
		E_MENU_BAR (menu_bar),
		g_settings_get_boolean (settings, "menubar-visible"));
	g_object_unref (settings);

	g_signal_connect_object (window, "event-after",
		G_CALLBACK (e_menu_bar_window_event_after_cb), menu_bar, G_CONNECT_AFTER);

	return menu_bar;
}

/**
 * e_menu_bar_get_visible:
 * @self: a #EMenuBar
 *
 * Determines whether the inner menu bar is visible.
 *
 * Returns: %TRUE if the inner menu bar is visible
 *
 * Since: 3.46
 **/
gboolean
e_menu_bar_get_visible (EMenuBar *self)
{
	g_return_val_if_fail (E_IS_MENU_BAR (self), FALSE);

	return self->priv->visible;
}

/**
 * e_menu_bar_set_visible:
 * @self: a #EMenuBar
 * @visible: whether the inner menu bar should be shown or not
 *
 * Sets the visibility state of the inner menu bar.
 *
 * Since: 3.46
 **/
void
e_menu_bar_set_visible (EMenuBar *menu_bar,
			gboolean visible)
{
	g_return_if_fail (E_IS_MENU_BAR (menu_bar));

	menu_bar->priv->visible = visible;
	gtk_widget_set_visible (GTK_WIDGET (menu_bar->priv->inner_menu_bar), visible);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-util/e-util.h"
#include "e-shell-headerbar.h"
#include "e-shell-window-private.h"

#include <glib/gi18n.h>

struct _EShellHeaderBarPrivate {
	GWeakRef shell_window;
	GtkWidget *menu_button;
	GtkWidget *new_button;

	gulong prefered_item_notify_id;
};

enum {
	PROP_0,
	PROP_MENU_BUTTON,
	PROP_SHELL_WINDOW
};

G_DEFINE_TYPE_WITH_CODE (EShellHeaderBar, e_shell_header_bar, E_TYPE_HEADER_BAR,
	G_ADD_PRIVATE (EShellHeaderBar))

static void
shell_header_bar_clear_box (GList *children,
			    const gchar *name)
{
	GList *iter;
	const gchar *widget_name;

	for (iter = children; iter != NULL; iter = g_list_next (iter)) {
		GtkWidget *widget = iter->data;

		widget_name = gtk_widget_get_name (widget);
		if (widget_name != NULL && g_str_has_prefix (widget_name, name))
			gtk_widget_destroy (widget);
	}
}

static EShellWindow *
shell_header_bar_dup_shell_window (EShellHeaderBar *headerbar)
{
	g_return_val_if_fail (E_IS_SHELL_HEADER_BAR (headerbar), NULL);

	return g_weak_ref_get (&headerbar->priv->shell_window);
}

static void
shell_header_bar_set_menu_button (EShellHeaderBar *headerbar,
				  GtkWidget *menu_button)
{
	g_return_if_fail (GTK_IS_WIDGET (menu_button));
	g_return_if_fail (headerbar->priv->menu_button == NULL);

	headerbar->priv->menu_button = g_object_ref_sink (menu_button);
}

static void
shell_header_bar_set_shell_window (EShellHeaderBar *headerbar,
				   EShellWindow *shell_window)
{
	EShellWindow *priv_shell_window = shell_header_bar_dup_shell_window (headerbar);

	/* This should be always NULL, but for a sake of completeness
	   and no memory leak do unref it here. Also do *not* use
	   g_clear_object(), because the non-NULL is tested below. */
	if (priv_shell_window)
		g_object_unref (priv_shell_window);

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (priv_shell_window == NULL);

	g_weak_ref_set (
		&headerbar->priv->shell_window,
		G_OBJECT (shell_window));
}

static void
shell_header_bar_update_new_menu (EShellWindow *shell_window,
				  gpointer user_data)
{
	EShellHeaderBar *headerbar = user_data;
	GtkWidget *menu;

	/* Update the "New" menu button submenu. */
	menu = e_shell_window_create_new_menu (shell_window);
	e_header_bar_button_take_menu (E_HEADER_BAR_BUTTON (headerbar->priv->new_button), menu);
}

static void
shell_header_bar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MENU_BUTTON:
			shell_header_bar_set_menu_button (
				E_SHELL_HEADER_BAR (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_WINDOW:
			shell_header_bar_set_shell_window (
				E_SHELL_HEADER_BAR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_header_bar_get_property (GObject *object,
			       guint property_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_WINDOW:
			g_value_take_object (
				value, shell_header_bar_dup_shell_window (
				E_SHELL_HEADER_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_header_bar_constructed (GObject *object)
{
	EShellHeaderBar *self = E_SHELL_HEADER_BAR (object);
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	GtkWidget *new_button;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_shell_header_bar_parent_class)->constructed (object);

	shell_window = shell_header_bar_dup_shell_window (self);

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	new_button = e_header_bar_button_new (C_("toolbar-button", "New"), NULL);
	/* show label also on the New button, but only if all other buttons have space for their label */
	e_header_bar_pack_start (E_HEADER_BAR (self), new_button, G_MAXUINT);
	gtk_widget_show (new_button);
	self->priv->new_button = g_object_ref (new_button);

	if (self->priv->menu_button)
		e_header_bar_pack_end (E_HEADER_BAR (self), self->priv->menu_button, G_MAXUINT);

	e_header_bar_button_add_accelerator (
		E_HEADER_BAR_BUTTON (self->priv->new_button),
		gtk_ui_manager_get_accel_group (ui_manager),
		GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

	self->priv->prefered_item_notify_id = g_signal_connect (
               shell_window, "update-new-menu",
               G_CALLBACK (shell_header_bar_update_new_menu), self);

	g_object_unref (shell_window);
}

static void
shell_header_bar_dispose (GObject *object)
{
	EShellHeaderBar *headerbar = E_SHELL_HEADER_BAR (object);

	if (headerbar->priv->new_button != NULL) {
		EShellWindow *shell_window = shell_header_bar_dup_shell_window (headerbar);

		if (shell_window) {
			g_signal_handler_disconnect (
				shell_window,
				headerbar->priv->prefered_item_notify_id);
			g_object_unref (headerbar->priv->new_button);

			g_object_unref (shell_window);
		}

		headerbar->priv->new_button = NULL;
		headerbar->priv->prefered_item_notify_id = 0;
	}

	g_clear_object (&headerbar->priv->menu_button);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_shell_header_bar_parent_class)->dispose (object);
}

static void
shell_header_bar_finalize (GObject *object)
{
	EShellHeaderBar *self = E_SHELL_HEADER_BAR (object);

	g_weak_ref_clear (&self->priv->shell_window);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_shell_header_bar_parent_class)->finalize (object);
}

static void
e_shell_header_bar_class_init (EShellHeaderBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = shell_header_bar_set_property;
	object_class->get_property = shell_header_bar_get_property;
	object_class->constructed = shell_header_bar_constructed;
	object_class->dispose = shell_header_bar_dispose;
	object_class->finalize = shell_header_bar_finalize;

	g_object_class_install_property (
		object_class,
		PROP_MENU_BUTTON,
		g_param_spec_object (
			"menu-button",
			"Menu Button",
			"Menu button to add to the header bar",
			GTK_TYPE_WIDGET,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellHeaderbar:shell-window
	 *
	 * The #EShellWindow to which the headerbar belongs.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_WINDOW,
		g_param_spec_object (
			"shell-window",
			"Shell Window",
			"The window to which the headerbar belongs",
			E_TYPE_SHELL_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_shell_header_bar_init (EShellHeaderBar *self)
{
	self->priv = e_shell_header_bar_get_instance_private (self);
	g_weak_ref_init (&self->priv->shell_window, NULL);
}

/**
 * e_shell_header_bar_new:
 * @shel_window: The #EShellWindow to which the headerbar belongs
 * @menu_button: a menu button to add to the header bar
 *
 * Creates a new #EShellHeaderBar
 *
 * Returns: (transfer full): a new #EShellHeaderBar
 *
 * Since: 3.46
 **/
GtkWidget *
e_shell_header_bar_new (EShellWindow *shell_window,
			GtkWidget *menu_button)
{
	return g_object_new (E_TYPE_SHELL_HEADER_BAR,
		"shell-window", shell_window,
		"menu-button", menu_button,
		NULL);
}

/**
 * e_shell_header_bar_get_new_button:
 * @headerbar: an #EShellHeaderBar
 *
 * Returns: (transfer none): the 'New' button widget, which is #EHeaderBarButton
 *
 * Since: 3.46
 **/
GtkWidget *
e_shell_header_bar_get_new_button (EShellHeaderBar *headerbar)
{
	g_return_val_if_fail (E_IS_SHELL_HEADER_BAR (headerbar), NULL);

	return headerbar->priv->new_button;
}

/**
 * e_shell_header_bar_clear:
 * @headerbar: an #EShellHeaderBar
 * @name: widget name starts with
 *
 * Removes all widgets from the header bar where widget name starts with @name
 *
 * Since: 3.46
 **/
void
e_shell_header_bar_clear (EShellHeaderBar *headerbar,
			  const gchar *name)
{
	GList *children;

	g_return_if_fail (E_IS_SHELL_HEADER_BAR (headerbar));

	children = e_header_bar_get_start_widgets (E_HEADER_BAR (headerbar));
	shell_header_bar_clear_box (children, name);
	g_list_free (children);

	children = e_header_bar_get_end_widgets (E_HEADER_BAR (headerbar));
	shell_header_bar_clear_box (children, name);
	g_list_free (children);
}

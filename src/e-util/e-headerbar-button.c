/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* EHeaderBarButton is a collection of buttons (one main button and additionnal actions)
 * with an optional dropdown menu.
 *
 * You can use e_header_bar_button_new() to get a new #EHeaderBarButton. If you do not provide
 * an action, prefered action will be used. See e_header_bar_button_take_menu().
 */

#include "evolution-config.h"

#include "e-headerbar-button.h"
#include "e-misc-utils.h"

#include <glib/gi18n.h>

struct _EHeaderBarButtonPrivate {
	GtkWidget *button;
	GtkWidget *dropdown_button;

	GtkAction *action;
	gchar *label;
	gchar *prefer_item;
};

enum {
	PROP_0,
	PROP_PREFER_ITEM,
	PROP_LABEL,
	PROP_ACTION
};

G_DEFINE_TYPE_WITH_CODE (EHeaderBarButton, e_header_bar_button, GTK_TYPE_BOX,
	G_ADD_PRIVATE (EHeaderBarButton))

static GtkAction *
header_bar_button_get_prefer_action (EHeaderBarButton *header_bar_button)
{
	GtkMenu *menu;
	GList *children;
	GtkAction *action = NULL;
	GList *link;
	const gchar *prefer_item;

	if (header_bar_button->priv->dropdown_button == NULL)
		return NULL;

	menu = gtk_menu_button_get_popup ( GTK_MENU_BUTTON (header_bar_button->priv->dropdown_button));
	g_return_val_if_fail (menu != NULL, NULL);

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	g_return_val_if_fail (children != NULL, NULL);

	prefer_item = header_bar_button->priv->prefer_item;

	for (link = children; link != NULL; link = g_list_next (link)) {
		GtkWidget *child;
		const gchar *name;

		child = GTK_WIDGET (link->data);

		if (!GTK_IS_MENU_ITEM (child))
			continue;

		action = gtk_activatable_get_related_action (
			GTK_ACTIVATABLE (child));

		if (action == NULL)
			continue;

		name = gtk_action_get_name (action);

		if (prefer_item == NULL ||
				*prefer_item == '\0' ||
				g_strcmp0 (name, prefer_item) == 0) {
			break;
		}
	}

	g_list_free (children);

	return action;
}

static void
header_bar_button_update_button_for_action (GtkButton *button,
					    GtkAction *action)
{
	GtkWidget *image;
	GtkStyleContext *style_context;
	const gchar *tooltip;
	const gchar *icon_name;

	g_return_if_fail (button != NULL);
	g_return_if_fail (action != NULL);

	tooltip = gtk_action_get_tooltip (action);
	gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip);

	icon_name = gtk_action_get_icon_name (action);
	if (icon_name == NULL) {
		GIcon *icon = gtk_action_get_gicon (action);
		image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
	} else {
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	}
	gtk_widget_set_margin_end (image, 2);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_widget_show (image);

	/* Force text button class to fix some themes */
	style_context = gtk_widget_get_style_context (GTK_WIDGET (button));
	gtk_style_context_add_class (style_context, "text-button");
}

static void
header_bar_button_update_button (EHeaderBarButton *header_bar_button)
{
	GtkAction *action;

	if (header_bar_button->priv->action == NULL)
		action = header_bar_button_get_prefer_action (header_bar_button);
	else
		action = header_bar_button->priv->action;

	if (action != NULL) {
		header_bar_button_update_button_for_action (
			GTK_BUTTON (header_bar_button->priv->button),
			action);
	}
}

static void
header_bar_button_clicked (EHeaderBarButton *header_bar_button)
{
	GtkAction *action;

	if (header_bar_button->priv->action == NULL)
		action = header_bar_button_get_prefer_action (header_bar_button);
	else
		action = header_bar_button->priv->action;

	if (action != NULL)
		gtk_action_activate (action);
}

static void
header_bar_button_set_prefer_item (EHeaderBarButton *self,
				   const gchar *prefer_item)
{
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (self));

	if (g_strcmp0 (self->priv->prefer_item, prefer_item) == 0)
		return;

	g_free (self->priv->prefer_item);

	self->priv->prefer_item = g_strdup (prefer_item);
	header_bar_button_update_button (self);
}

static gboolean
e_headerbar_button_transform_sensitive_cb (GBinding *binding,
					   const GValue *from_value,
					   GValue *to_value,
					   gpointer user_data)
{
	/* The GtkAction::sensitive property does not take into the consideration
	   also the group's sensitivity, thus use the gtk_action_is_sensitive() function. */

	g_value_set_boolean (to_value, gtk_action_is_sensitive (GTK_ACTION (g_binding_get_source (binding))));

	return TRUE;
}

static void
header_bar_button_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	EHeaderBarButton *header_bar_button = E_HEADER_BAR_BUTTON (object);

	switch (property_id) {
		case PROP_PREFER_ITEM:
			header_bar_button_set_prefer_item (
				header_bar_button,
				g_value_get_string (value));
			return;
		case PROP_LABEL:
			if (header_bar_button->priv->label == NULL)
				header_bar_button->priv->label = g_value_dup_string (value);
			return;
		case PROP_ACTION:
			header_bar_button->priv->action = g_value_get_object (value);
			if (header_bar_button->priv->action != NULL)
				g_object_ref (header_bar_button->priv->action);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
header_bar_button_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	EHeaderBarButton *header_bar_button = E_HEADER_BAR_BUTTON (object);

	switch (property_id) {
		case PROP_PREFER_ITEM:
			g_value_set_string (
				value, header_bar_button->priv->prefer_item);
			return;
		case PROP_LABEL:
			g_value_set_string (value, header_bar_button->priv->label);
			return;
		case PROP_ACTION:
			g_value_set_object (value, header_bar_button->priv->action);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
header_bar_button_constructed (GObject *object)
{
	EHeaderBarButton *header_bar_button = E_HEADER_BAR_BUTTON (object);
	GtkStyleContext *style_context;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_header_bar_button_parent_class)->constructed (object);

	if (GTK_IS_TOGGLE_ACTION (header_bar_button->priv->action)) {
		header_bar_button->priv->button = gtk_toggle_button_new_with_label (
			header_bar_button->priv->label);
	} else {
		header_bar_button->priv->button = gtk_button_new_with_label (
			header_bar_button->priv->label);
	}
	gtk_widget_show (header_bar_button->priv->button);

	gtk_box_pack_start (
		GTK_BOX (header_bar_button), header_bar_button->priv->button,
		FALSE, FALSE, 0);

	if (header_bar_button->priv->action != NULL) {
		header_bar_button_update_button_for_action (
			GTK_BUTTON (header_bar_button->priv->button),
			header_bar_button->priv->action);

		e_binding_bind_property_full (
			header_bar_button->priv->action, "sensitive",
			header_bar_button, "sensitive",
			G_BINDING_SYNC_CREATE,
			e_headerbar_button_transform_sensitive_cb,
			NULL, NULL, NULL);

		if (GTK_IS_TOGGLE_ACTION (header_bar_button->priv->action))
			e_binding_bind_property (
				header_bar_button->priv->action, "active",
				header_bar_button->priv->button, "active",
				G_BINDING_SYNC_CREATE);
	}

	/* TODO: GTK4 port: do not use linked buttons
	 * https://developer.gnome.org/hig/patterns/containers/header-bars.html#button-grouping */
	style_context = gtk_widget_get_style_context (GTK_WIDGET (header_bar_button));
	gtk_style_context_add_class (style_context, "linked");

	g_signal_connect_swapped (
		header_bar_button->priv->button, "clicked",
		G_CALLBACK (header_bar_button_clicked), header_bar_button);
}

static void
header_bar_button_finalize (GObject *object)
{
	EHeaderBarButton *header_bar_button = E_HEADER_BAR_BUTTON (object);

	g_free (header_bar_button->priv->prefer_item);
	g_free (header_bar_button->priv->label);

	if (header_bar_button->priv->action != NULL)
		g_object_unref (header_bar_button->priv->action);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_header_bar_button_parent_class)->finalize (object);
}

static void
e_header_bar_button_class_init (EHeaderBarButtonClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = header_bar_button_set_property;
	object_class->get_property = header_bar_button_get_property;
	object_class->constructed = header_bar_button_constructed;
	object_class->finalize = header_bar_button_finalize;

	g_object_class_install_property (
		object_class,
		PROP_PREFER_ITEM,
		g_param_spec_string (
			"prefer-item",
			"Prefer Item",
			"Name of an item to show instead of the first",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_LABEL,
		g_param_spec_string (
			"label",
			"Label",
			"Button label",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_ACTION,
		g_param_spec_object (
			"action",
			"Action",
			"Button action",
			GTK_TYPE_ACTION,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
e_header_bar_button_init (EHeaderBarButton *self)
{
	self->priv = e_header_bar_button_get_instance_private (self);

	self->priv->dropdown_button = NULL;
	self->priv->prefer_item = NULL;
	self->priv->label = NULL;
}

/**
 * e_header_bar_button_new:
 * @action: An action overriding menu default action
 *
 * Creates a new #EHeaderBarButton labeled with @label.
 * If action is %NULL, button will use default preferred menu action if available,
 * see e_header_bar_button_take_menu().
 *
 * Returns: (transfer full): a new #EHeaderBarButton
 *
 * Since: 3.46
 **/
GtkWidget*
e_header_bar_button_new (const gchar *label,
			 GtkAction *action)
{
	return g_object_new (E_TYPE_HEADER_BAR_BUTTON,
		"label", label,
		"action", action, NULL);
}

/**
 * e_header_bar_button_add_action:
 * @header_bar_button: #EHeaderBarButton
 * @label: The text you want the #GtkButton to hold.
 * @action: #GtkButton related action
 *
 * Adds a new button with a related action.
 *
 * Since: 3.46
 **/
void
e_header_bar_button_add_action (EHeaderBarButton *header_bar_button,
				const gchar *label,
				GtkAction *action)
{
	GtkWidget *button;

	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));
	g_return_if_fail (GTK_IS_ACTION (action));

	if (GTK_IS_TOGGLE_ACTION (action))
		button = gtk_toggle_button_new_with_label (label);
	else
		button = gtk_button_new_with_label (label);

	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (header_bar_button), button, FALSE, FALSE, 0);

	e_binding_bind_property (
		action, "sensitive",
		button, "sensitive",
		G_BINDING_SYNC_CREATE);

	if (GTK_IS_TOGGLE_ACTION (action))
		e_binding_bind_property (
			action, "active",
			button, "active",
			G_BINDING_SYNC_CREATE);

	g_signal_connect_swapped (
		button, "clicked",
		G_CALLBACK (gtk_action_activate), action);

	header_bar_button_update_button_for_action (GTK_BUTTON (button), action);
}

/**
 * e_header_bar_button_take_menu:
 * @header_bar_button: #EHeaderBarButton
 * @menu: (transfer full) (nullable): A #GtkMenu, or %NULL to unset and disable the dropdown button.
 *
 * Sets the #GtkMenu that will be popped up when the @menu_button is clicked, or
 * %NULL to dissociate any existing menu and disable the dropdown button.
 *
 * If current #EHeaderBarButton action is %NULL, clicking the button will fire
 * the preferred item, if set, or the first menu item otherwise.
 *
 * Since: 3.46
 **/
void
e_header_bar_button_take_menu (EHeaderBarButton *header_bar_button,
			       GtkWidget *menu)
{
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));

	if (!GTK_IS_MENU (menu)) {
		if (header_bar_button->priv->dropdown_button != NULL)
			gtk_widget_hide (header_bar_button->priv->dropdown_button);
		return;
	}

	if (header_bar_button->priv->dropdown_button == NULL) {
		header_bar_button->priv->dropdown_button = gtk_menu_button_new ();
		gtk_box_pack_end (
			GTK_BOX (header_bar_button), header_bar_button->priv->dropdown_button,
			FALSE, FALSE, 0);
	}

	gtk_menu_button_set_popup (GTK_MENU_BUTTON (header_bar_button->priv->dropdown_button), menu);

	header_bar_button_update_button (header_bar_button);

	gtk_widget_show (header_bar_button->priv->dropdown_button);
}

/**
 * e_header_bar_button_css_add_class:
 * @header_bar_button: #EHeaderBarButton
 * @class_name: a CSS class name
 *
 * Adds a CSS class to #EHeaderBarButton main button
 *
 * Since: 3.46
 **/
void e_header_bar_button_css_add_class (EHeaderBarButton *header_bar_button,
					const gchar *class_name)
{
	GtkStyleContext *style_context;

	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));

	style_context = gtk_widget_get_style_context (header_bar_button->priv->button);
	gtk_style_context_add_class (style_context, class_name);
}

/**
 * e_header_bar_button_add_accelerator:
 * @header_bar_button: #EHeaderBarButton to activate
 * @accel_group: Accel group for this widget, added to its toplevel.
 * @accel_key: GDK keyval of the accelerator
 * @accel_mods: Modifier key combination of the accelerator.
 * @accel_flags: Flag accelerators, e.g. GTK_ACCEL_VISIBLE
 *
 * Installs an accelerator for main action
 *
 * Since: 3.46
 **/
void e_header_bar_button_add_accelerator (EHeaderBarButton *header_bar_button,
					  GtkAccelGroup* accel_group,
					  guint accel_key,
					  GdkModifierType accel_mods,
					  GtkAccelFlags accel_flags)
{
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));

	gtk_widget_add_accelerator (
		GTK_WIDGET (header_bar_button->priv->button), "clicked",
		accel_group, accel_key, accel_mods, accel_flags);
}

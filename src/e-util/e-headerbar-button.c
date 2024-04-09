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
	GtkWidget *labeled_button;
	GtkWidget *icon_only_button;
	GtkWidget *dropdown_button;

	GtkAction *action;
	gchar *label;
	gchar *prefer_item;
	gint last_labeled_button_width;
	gint last_icon_only_button_width;
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
			GTK_BUTTON (header_bar_button->priv->labeled_button),
			action);
		if (header_bar_button->priv->icon_only_button) {
			header_bar_button_update_button_for_action (
				GTK_BUTTON (header_bar_button->priv->icon_only_button),
				action);
		}
	}
}

static void
header_bar_button_clicked (GtkWidget *button,
			   gpointer user_data)
{
	EHeaderBarButton *header_bar_button = user_data;
	GtkAction *action;

	if (header_bar_button->priv->action == NULL)
		action = header_bar_button_get_prefer_action (header_bar_button);
	else
		action = header_bar_button->priv->action;

	if (action != NULL)
		gtk_action_activate (action);
}

static void
header_bar_button_action_activate_cb (GObject *button,
				      gpointer user_data)
{
	GtkAction *action = user_data;

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
header_bar_button_transform_sensitive_cb (GBinding *binding,
					  const GValue *from_value,
					  GValue *to_value,
					  gpointer user_data)
{
	/* The GtkAction::sensitive property does not take into the consideration
	   also the group's sensitivity, thus use the gtk_action_is_sensitive() function. */

	g_value_set_boolean (to_value, gtk_action_is_sensitive (GTK_ACTION (g_binding_get_source (binding))));

	return TRUE;
}

typedef struct _ToggleActionData {
	GWeakRef *button_weakref;
	gulong handler_id;
} ToggleActionData;

static void
toggle_action_data_free (gpointer ptr,
			 GClosure *closure)
{
	ToggleActionData *tad = ptr;

	if (tad) {
		e_weak_ref_free (tad->button_weakref);
		g_free (tad);
	}
}

static void
header_button_action_notify_active_cb (GObject *action,
				       GParamSpec *param,
				       gpointer user_data)
{
	ToggleActionData *tad = user_data;
	GtkToggleButton *button;
	gboolean active = FALSE;

	button = g_weak_ref_get (tad->button_weakref);
	if (!button)
		return;

	g_object_get (action, "active", &active, NULL);

	/* The "clicked" callback calls gtk_action_activate(), which, in case
	   of the toggle action, means to flip the option, thus it calls a notification
	   about action's 'active' property change, which leads back here, causing
	   a busy loop though the signal handlers. Blocking the handler breaks the loop. */
	if (tad->handler_id)
		g_signal_handler_block (button, tad->handler_id);

	if ((gtk_toggle_button_get_active (button) ? 1 : 0) != (active ? 1 : 0))
		gtk_toggle_button_set_active (button, active);

	if (tad->handler_id)
		g_signal_handler_unblock (button, tad->handler_id);

	g_clear_object (&button);
}

static GtkWidget *
header_bar_button_add_action_button (EHeaderBarButton *header_bar_button,
				     const gchar *label,
				     GtkAction *action,
				     GCallback clicked_cb,
				     gpointer clicked_cb_user_data)
{
	GtkWidget *button;
	gulong clicked_handler = 0;

	if (GTK_IS_TOGGLE_ACTION (action))
		button = gtk_toggle_button_new_with_label (label);
	else
		button = gtk_button_new_with_label (label);

	gtk_box_pack_start (GTK_BOX (header_bar_button), button, FALSE, FALSE, 0);

	if (clicked_cb) {
		clicked_handler = g_signal_connect_object (
			button, "clicked", clicked_cb, clicked_cb_user_data, 0);
	}

	if (action) {
		e_binding_bind_property_full (
			action, "sensitive",
			button, "sensitive",
			G_BINDING_SYNC_CREATE,
			header_bar_button_transform_sensitive_cb,
			NULL, NULL, NULL);

		if (GTK_IS_TOGGLE_ACTION (action)) {
			ToggleActionData *tad;

			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));

			tad = g_new0 (ToggleActionData, 1);
			tad->button_weakref = e_weak_ref_new (button);
			tad->handler_id = clicked_handler;

			g_signal_connect_data (action, "notify::active",
				G_CALLBACK (header_button_action_notify_active_cb), tad, toggle_action_data_free, 0);
		}

		header_bar_button_update_button_for_action (GTK_BUTTON (button), action);
	}

	return button;
}

static void
header_bar_button_add_action (EHeaderBarButton *header_bar_button,
			      const gchar *label,
			      GtkAction *action,
			      GCallback clicked_cb,
			      gpointer clicked_cb_user_data,
			      GtkWidget **out_labeled_button,
			      GtkWidget **out_icon_only_button)
{
	GtkWidget *labeled_button;
	GtkWidget *icon_only_button;

	labeled_button = header_bar_button_add_action_button (header_bar_button, label, action, clicked_cb, clicked_cb_user_data);

	if (label) {
		icon_only_button = header_bar_button_add_action_button (header_bar_button, NULL, action, clicked_cb, clicked_cb_user_data);
		gtk_widget_show (icon_only_button);

		e_binding_bind_property (
			labeled_button, "sensitive",
			icon_only_button, "sensitive",
			G_BINDING_SYNC_CREATE);
	} else {
		icon_only_button = NULL;
		gtk_widget_show (labeled_button);
	}

	if (out_labeled_button)
		*out_labeled_button = labeled_button;

	if (out_icon_only_button)
		*out_icon_only_button = icon_only_button;
}

static void
header_bar_button_style_updated (GtkWidget *widget)
{
	EHeaderBarButton *self = E_HEADER_BAR_BUTTON (widget);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_header_bar_button_parent_class)->style_updated (widget);

	self->priv->last_labeled_button_width = -1;
	self->priv->last_icon_only_button_width = -1;
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

	header_bar_button_add_action (header_bar_button,
		header_bar_button->priv->label,
		header_bar_button->priv->action,
		G_CALLBACK (header_bar_button_clicked),
		header_bar_button,
		&header_bar_button->priv->labeled_button,
		&header_bar_button->priv->icon_only_button);

	/* TODO: GTK4 port: do not use linked buttons
	 * https://developer.gnome.org/hig/patterns/containers/header-bars.html#button-grouping */
	style_context = gtk_widget_get_style_context (GTK_WIDGET (header_bar_button));
	gtk_style_context_add_class (style_context, "linked");
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
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = header_bar_button_set_property;
	object_class->get_property = header_bar_button_get_property;
	object_class->constructed = header_bar_button_constructed;
	object_class->finalize = header_bar_button_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->style_updated = header_bar_button_style_updated;

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
	self->priv->last_labeled_button_width = -1;
	self->priv->last_icon_only_button_width = -1;
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
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));
	g_return_if_fail (GTK_IS_ACTION (action));

	header_bar_button_add_action (header_bar_button, label, action,
		G_CALLBACK (header_bar_button_action_activate_cb), action, NULL, NULL);
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

		e_binding_bind_property (
			header_bar_button->priv->labeled_button, "sensitive",
			header_bar_button->priv->dropdown_button, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	gtk_menu_button_set_popup (GTK_MENU_BUTTON (header_bar_button->priv->dropdown_button), menu);

	/* Setting/unsetting menu can sensitize/un-sensitize the menu button,
	   thus re-sync the state with the labeled button. */
	if (header_bar_button->priv->dropdown_button && menu) {
		gtk_widget_set_sensitive (header_bar_button->priv->dropdown_button,
			gtk_widget_get_sensitive (header_bar_button->priv->labeled_button));
	}

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
void
e_header_bar_button_css_add_class (EHeaderBarButton *header_bar_button,
				   const gchar *class_name)
{
	GtkStyleContext *style_context;

	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));

	style_context = gtk_widget_get_style_context (header_bar_button->priv->labeled_button);
	gtk_style_context_add_class (style_context, class_name);

	if (header_bar_button->priv->icon_only_button) {
		style_context = gtk_widget_get_style_context (header_bar_button->priv->icon_only_button);
		gtk_style_context_add_class (style_context, class_name);
	}
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
void
e_header_bar_button_add_accelerator (EHeaderBarButton *header_bar_button,
				     GtkAccelGroup* accel_group,
				     guint accel_key,
				     GdkModifierType accel_mods,
				     GtkAccelFlags accel_flags)
{
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (header_bar_button));

	gtk_widget_add_accelerator (
		header_bar_button->priv->labeled_button, "clicked",
		accel_group, accel_key, accel_mods, accel_flags);

	if (header_bar_button->priv->icon_only_button) {
		gtk_widget_add_accelerator (
			header_bar_button->priv->icon_only_button, "clicked",
			accel_group, accel_key, accel_mods, accel_flags);
	}
}

/**
 * e_header_bar_button_get_widths:
 * @self: an #EHeaderBarButton
 * @out_labeled_width: (out): return location for width of the button with the label
 * @out_icon_only_width: (out): return location for width of the button with the icon only
 *
 * Returns expected width of the button when it has set a label
 * and when only icon is shown. When either of the two is -1,
 * the width could not be calculated, like when the button
 * cannot have (un)set the label, then the out_icon_only_width
 * is set to -1.
 *
 * Since: 3.48
 **/
void
e_header_bar_button_get_widths (EHeaderBarButton *self,
				gint *out_labeled_width,
				gint *out_icon_only_width)
{
	gint labeled_width = -1;
	gint icon_only_width = -1;
	gint current_width = -1;

	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (self));
	g_return_if_fail (out_labeled_width != NULL);
	g_return_if_fail (out_icon_only_width != NULL);

	gtk_widget_get_preferred_width (GTK_WIDGET (self), &current_width, NULL);

	if (!self->priv->icon_only_button) {
		*out_labeled_width = current_width;
		*out_icon_only_width = -1;
		return;
	}

	if (self->priv->last_labeled_button_width > 0) {
		*out_labeled_width = self->priv->last_labeled_button_width;
		*out_icon_only_width = self->priv->last_icon_only_button_width;
		return;
	}

	if (gtk_widget_get_visible (self->priv->labeled_button)) {
		gtk_widget_get_preferred_width (self->priv->labeled_button, &labeled_width, NULL);
	} else {
		gtk_widget_show (self->priv->labeled_button);
		gtk_widget_get_preferred_width (self->priv->labeled_button, &labeled_width, NULL);
		gtk_widget_hide (self->priv->labeled_button);
	}

	if (gtk_widget_get_visible (self->priv->icon_only_button)) {
		gtk_widget_get_preferred_width (self->priv->icon_only_button, &icon_only_width, NULL);
	} else {
		gtk_widget_show (self->priv->icon_only_button);
		gtk_widget_get_preferred_width (self->priv->icon_only_button, &icon_only_width, NULL);
		gtk_widget_hide (self->priv->icon_only_button);
	}

	if (gtk_widget_get_visible (self->priv->labeled_button)) {
		*out_labeled_width = current_width;
		*out_icon_only_width = current_width - labeled_width + icon_only_width;
	} else {
		*out_labeled_width = current_width - icon_only_width + labeled_width;
		*out_icon_only_width = current_width;
	}

	self->priv->last_labeled_button_width = *out_labeled_width;
	self->priv->last_icon_only_button_width = *out_icon_only_width;
}

/**
 * e_header_bar_button_get_show_icon_only:
 * @self: an #EHeaderBarButton
 *
 * Returns whether the button shows only icon, without label.
 * Returns %FALSE, when the label cannot be (un)set.
 *
 * Returns: whether the button shows only icon, without label
 *
 * Since: 3.48
 **/
gboolean
e_header_bar_button_get_show_icon_only (EHeaderBarButton *self)
{
	g_return_val_if_fail (E_IS_HEADER_BAR_BUTTON (self), FALSE);

	if (!self->priv->icon_only_button)
		return FALSE;

	return gtk_widget_get_visible (self->priv->icon_only_button);
}

/**
 * e_header_bar_button_set_show_icon_only:
 * @self: an #EHeaderBarButton
 * @show_icon_only: value to set
 *
 * Changes button's appearance between showing only icon and showing
 * label with an icon. The function does nothing, when the label
 * cannot be (un)set.
 *
 * Since: 3.48
 **/
void
e_header_bar_button_set_show_icon_only (EHeaderBarButton *self,
					gboolean show_icon_only)
{
	g_return_if_fail (E_IS_HEADER_BAR_BUTTON (self));

	if (!self->priv->icon_only_button)
		return;

	if ((gtk_widget_get_visible (self->priv->icon_only_button) ? 1 : 0) == (show_icon_only ? 1 : 0))
		return;

	/* Hide first, to not change window width */
	if (show_icon_only) {
		gtk_widget_hide (self->priv->labeled_button);
		gtk_widget_show (self->priv->icon_only_button);
	} else {
		gtk_widget_hide (self->priv->icon_only_button);
		gtk_widget_show (self->priv->labeled_button);
	}
}

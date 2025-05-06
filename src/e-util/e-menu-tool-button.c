/*
 * e-menu-tool-button.c
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

#include "e-misc-utils.h"
#include "e-ui-manager.h"

#include "e-menu-tool-button.h"

struct _EMenuToolButtonPrivate {
	gchar *prefer_item;
	EUIAction *fallback_action;
	EUIManager *ui_manager;
};

enum {
	PROP_0,
	PROP_PREFER_ITEM,
	PROP_UI_MANAGER,
	PROP_FALLBACK_ACTION
};

G_DEFINE_TYPE_WITH_PRIVATE (EMenuToolButton, e_menu_tool_button, GTK_TYPE_MENU_TOOL_BUTTON)

static EUIAction *
menu_tool_button_get_prefer_item_action (EMenuToolButton *self)
{
	if (!self->priv->ui_manager ||
	    !self->priv->prefer_item)
		return NULL;

	return e_ui_manager_get_action (self->priv->ui_manager, self->priv->prefer_item);
}

static void
menu_tool_button_update_button (EMenuToolButton *self)
{
	EUIAction *action;
	gchar *label;

	action = menu_tool_button_get_prefer_item_action (self);
	if (!action)
		action = self->priv->fallback_action;
	if (!action)
		return;

	/* preserve the label */
	label = g_strdup (gtk_tool_button_get_label (GTK_TOOL_BUTTON (self)));

	e_ui_manager_update_item_from_action (self->priv->ui_manager, self, action);

	gtk_tool_button_set_label (GTK_TOOL_BUTTON (self), label);

	g_free (label);
}

static void
menu_tool_button_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_ITEM:
			e_menu_tool_button_set_prefer_item (
				E_MENU_TOOL_BUTTON (object),
				g_value_get_string (value));
			return;

		case PROP_UI_MANAGER:
			g_clear_object (&E_MENU_TOOL_BUTTON (object)->priv->ui_manager);
			E_MENU_TOOL_BUTTON (object)->priv->ui_manager = g_value_dup_object (value);
			return;

		case PROP_FALLBACK_ACTION:
			e_menu_tool_button_set_fallback_action (
				E_MENU_TOOL_BUTTON (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_tool_button_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_PREFER_ITEM:
			g_value_set_string (
				value, e_menu_tool_button_get_prefer_item (
				E_MENU_TOOL_BUTTON (object)));
			return;

		case PROP_UI_MANAGER:
			g_value_set_object (value, E_MENU_TOOL_BUTTON (object)->priv->ui_manager);
			return;

		case PROP_FALLBACK_ACTION:
			g_value_set_object (value, e_menu_tool_button_get_fallback_action (E_MENU_TOOL_BUTTON (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_tool_button_finalize (GObject *object)
{
	EMenuToolButton *self = E_MENU_TOOL_BUTTON (object);

	g_clear_object (&self->priv->fallback_action);
	g_clear_object (&self->priv->ui_manager);
	g_free (self->priv->prefer_item);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_menu_tool_button_parent_class)->finalize (object);
}

static void
e_menu_tool_button_class_init (EMenuToolButtonClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = menu_tool_button_set_property;
	object_class->get_property = menu_tool_button_get_property;
	object_class->finalize = menu_tool_button_finalize;

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
		PROP_UI_MANAGER,
		g_param_spec_object (
			"ui-manager",
			NULL,
			NULL,
			E_TYPE_UI_MANAGER,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_FALLBACK_ACTION,
		g_param_spec_object (
			"fallback-action",
			NULL,
			NULL,
			E_TYPE_UI_ACTION,
			G_PARAM_READWRITE));
}

static void
e_menu_tool_button_init (EMenuToolButton *button)
{
	button->priv = e_menu_tool_button_get_instance_private (button);

	button->priv->prefer_item = NULL;

	e_signal_connect_notify (
		button, "notify::menu",
		G_CALLBACK (menu_tool_button_update_button), NULL);
}

GtkToolItem *
e_menu_tool_button_new (const gchar *label,
			EUIManager *ui_manager)
{
	return g_object_new (E_TYPE_MENU_TOOL_BUTTON,
		"label", label,
		"ui-manager", ui_manager,
		NULL);
}

const gchar *
e_menu_tool_button_get_prefer_item (EMenuToolButton *button)
{
	g_return_val_if_fail (E_IS_MENU_TOOL_BUTTON (button), NULL);

	return button->priv->prefer_item;
}

void
e_menu_tool_button_set_prefer_item (EMenuToolButton *button,
                                    const gchar *prefer_item)
{
	g_return_if_fail (E_IS_MENU_TOOL_BUTTON (button));

	if (g_strcmp0 (button->priv->prefer_item, prefer_item) == 0)
		return;

	g_free (button->priv->prefer_item);
	button->priv->prefer_item = g_strdup (prefer_item);

	menu_tool_button_update_button (button);

	g_object_notify (G_OBJECT (button), "prefer-item");
}

EUIAction *
e_menu_tool_button_get_fallback_action (EMenuToolButton *button)
{
	g_return_val_if_fail (E_IS_MENU_TOOL_BUTTON (button), NULL);

	return button->priv->fallback_action;
}

void
e_menu_tool_button_set_fallback_action (EMenuToolButton *button,
					EUIAction *action)
{
	g_return_if_fail (E_IS_MENU_TOOL_BUTTON (button));

	if (button->priv->fallback_action == action)
		return;

	g_clear_object (&button->priv->fallback_action);
	button->priv->fallback_action = action ? g_object_ref (action) : NULL;

	menu_tool_button_update_button (button);

	g_object_notify (G_OBJECT (button), "fallback-action");
}

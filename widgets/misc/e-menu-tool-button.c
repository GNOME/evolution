/*
 * e-menu-tool-button.c
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

#include "e-menu-tool-button.h"

G_DEFINE_TYPE (
	EMenuToolButton,
	e_menu_tool_button,
	GTK_TYPE_MENU_TOOL_BUTTON)

static GtkWidget *
menu_tool_button_clone_image (GtkWidget *source)
{
	GtkIconSize size;
	GtkImageType image_type;
	const gchar *icon_name;

	/* XXX This isn't general purpose because it requires that the
	 *     source image be using a named icon.  Somewhat surprised
	 *     GTK+ doesn't offer something like this. */
	image_type = gtk_image_get_storage_type (GTK_IMAGE (source));
	g_return_val_if_fail (image_type == GTK_IMAGE_ICON_NAME, NULL);
	gtk_image_get_icon_name (GTK_IMAGE (source), &icon_name, &size);

	return gtk_image_new_from_icon_name (icon_name, size);
}

static GtkMenuItem *
menu_tool_button_get_first_menu_item (GtkMenuToolButton *menu_tool_button)
{
	GtkWidget *menu;
	GtkMenuItem *item;
	GList *children;

	menu = gtk_menu_tool_button_get_menu (menu_tool_button);
	if (!GTK_IS_MENU (menu))
		return NULL;

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	if (children == NULL)
		return NULL;

	item = GTK_MENU_ITEM (children->data);

	g_list_free (children);

	return item;
}

static void
menu_tool_button_update_button (GtkToolButton *tool_button)
{
	GtkMenuItem *menu_item;
	GtkMenuToolButton *menu_tool_button;
	GtkImageMenuItem *image_menu_item;
	GtkAction *action;
	GtkWidget *image;
	gchar *tooltip = NULL;

	menu_tool_button = GTK_MENU_TOOL_BUTTON (tool_button);
	menu_item = menu_tool_button_get_first_menu_item (menu_tool_button);
	if (!GTK_IS_IMAGE_MENU_ITEM (menu_item))
		return;

	image_menu_item = GTK_IMAGE_MENU_ITEM (menu_item);
	image = gtk_image_menu_item_get_image (image_menu_item);
	if (!GTK_IS_IMAGE (image))
		return;

	image = menu_tool_button_clone_image (image);
	gtk_tool_button_set_icon_widget (tool_button, image);
	gtk_widget_show (image);

	/* If the menu item is a proxy for a GtkAction, extract
	 * the action's tooltip and use it as our own tooltip. */
	action = gtk_activatable_get_related_action (
		GTK_ACTIVATABLE (menu_item));
	if (action != NULL)
		g_object_get (action, "tooltip", &tooltip, NULL);
	gtk_widget_set_tooltip_text (GTK_WIDGET (tool_button), tooltip);
	g_free (tooltip);
}

static void
menu_tool_button_size_request (GtkWidget *widget,
                               GtkRequisition *requisition)
{
	gint minimum_width;

	/* Chain up to parent's size_request() method. */
	GTK_WIDGET_CLASS (e_menu_tool_button_parent_class)->
		size_request (widget, requisition);

	/* XXX This is a hack.  This widget is only used for the New
	 *     button in the main window toolbar.  The New button is
	 *     pretty important, but the word "New" is pretty short
	 *     (in English, anyway) and this results in a small screen
	 *     target when using a "text below item" toolbar style.
	 *
	 *     We can't go hard-coding a width, but we -can- use a
	 *     heuristic based on the toolbar button height. */
	minimum_width = requisition->height * 2;
	requisition->width = MAX (minimum_width, requisition->width);
}

static void
menu_tool_button_clicked (GtkToolButton *tool_button)
{
	GtkMenuItem *menu_item;
	GtkMenuToolButton *menu_tool_button;

	menu_tool_button = GTK_MENU_TOOL_BUTTON (tool_button);
	menu_item = menu_tool_button_get_first_menu_item (menu_tool_button);

	if (GTK_IS_MENU_ITEM (menu_item))
		gtk_menu_item_activate (menu_item);
}

static void
e_menu_tool_button_class_init (EMenuToolButtonClass *class)
{
	GtkWidgetClass *widget_class;
	GtkToolButtonClass *tool_button_class;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = menu_tool_button_size_request;

	tool_button_class = GTK_TOOL_BUTTON_CLASS (class);
	tool_button_class->clicked = menu_tool_button_clicked;
}

static void
e_menu_tool_button_init (EMenuToolButton *button)
{
	g_signal_connect (
		button, "notify::menu",
		G_CALLBACK (menu_tool_button_update_button), NULL);
}

GtkToolItem *
e_menu_tool_button_new (const gchar *label)
{
	return g_object_new (E_TYPE_MENU_TOOL_BUTTON, "label", label, NULL);
}

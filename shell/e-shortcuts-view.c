/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shortcuts-view.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include <gal/util/e-util.h>

#include "e-shortcuts-view-model.h"

#include "e-shortcuts-view.h"


#define PARENT_TYPE E_TYPE_SHORTCUT_BAR
static EShortcutBarClass *parent_class = NULL;

struct _EShortcutsViewPrivate {
	EShortcuts *shortcuts;
};

enum {
	ACTIVATE_SHORTCUT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* FIXME this should all be in the model.  */

static const char *
get_storage_set_path_from_uri (const char *uri)
{
	const char *colon;

	if (g_path_is_absolute (uri))
		return NULL;

	colon = strchr (uri, ':');
	if (colon == NULL || colon == uri || colon[1] == '\0')
		return NULL;

	if (! g_path_is_absolute (colon + 1))
		return NULL;

	if (g_strncasecmp (uri, "evolution", colon - uri) != 0)
		return NULL;

	return colon + 1;
}

/* Icon callback for the shortcut bar.  */
static GdkPixbuf *
icon_callback (EShortcutBar *shortcut_bar,
	       const char *uri,
	       gpointer data)
{
	EFolderTypeRegistry *folder_type_registry;
	EShortcuts *shortcuts;
	EStorageSet *storage_set;
	EFolder *folder;
	GdkPixbuf *pixbuf;
	const char *type;

	shortcuts = E_SHORTCUTS (data);

	storage_set = e_shortcuts_get_storage_set (shortcuts);
	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	folder = e_storage_set_get_folder (storage_set,
					   get_storage_set_path_from_uri (uri));

	if (folder == NULL)
		return NULL;

	type = e_folder_get_type_string (folder);
	if (type == NULL)
		return NULL;

	/* FIXME mini icons?  */
	pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry, type, FALSE);
	if (pixbuf != NULL)
		gdk_pixbuf_ref (pixbuf);

	return pixbuf;
}


/* Shortcut bar right-click menu.  */

struct _RightClickMenuData {
	EShortcutsView *shortcuts_view;
	int group_num;
};
typedef struct _RightClickMenuData RightClickMenuData;

static void
toggle_large_icons_cb (GtkWidget *widget,
		       void *data)
{
	RightClickMenuData *menu_data;

	g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (widget));

	if (data == NULL)
		return;

	menu_data = (RightClickMenuData *) data;

	if (! GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (menu_data->shortcuts_view),
				      menu_data->group_num,
				      E_ICON_BAR_LARGE_ICONS);
}

static void
toggle_small_icons_cb (GtkWidget *widget,
		       void *data)
{
	RightClickMenuData *menu_data;

	g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (widget));

	if (data == NULL)
		return;

	menu_data = (RightClickMenuData *) data;

	if (! GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	e_shortcut_bar_set_view_type (E_SHORTCUT_BAR (menu_data->shortcuts_view),
				      menu_data->group_num,
				      E_ICON_BAR_SMALL_ICONS);
}

static GnomeUIInfo icon_size_radio_group_uiinfo[] = {
	{ GNOME_APP_UI_ITEM, N_("_Small icons"),
	  N_("Show the shortcuts as small icons"), toggle_small_icons_cb, NULL,
	  NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("_Large icons"),
	  N_("Show the shortcuts as large icons"), toggle_large_icons_cb, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_END
};

static GnomeUIInfo right_click_menu_uiinfo[] = {
	GNOMEUIINFO_RADIOLIST (icon_size_radio_group_uiinfo),
	GNOMEUIINFO_END
};

static void
pop_up_right_click_menu_for_group (EShortcutsView *shortcuts_view,
				   GdkEventButton *event,
				   int group_num)
{
	RightClickMenuData *menu_data;
	GtkWidget *popup_menu;

	menu_data = g_new (RightClickMenuData, 1);
	menu_data->shortcuts_view = shortcuts_view;
	menu_data->group_num      = group_num;

	popup_menu = gnome_popup_menu_new (right_click_menu_uiinfo);

	if (e_shortcut_bar_get_view_type (E_SHORTCUT_BAR (shortcuts_view), group_num)
	    == E_ICON_BAR_SMALL_ICONS)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (icon_size_radio_group_uiinfo[0].widget),
						TRUE);
	else
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (icon_size_radio_group_uiinfo[1].widget),
						TRUE);

	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event, menu_data);

	g_free (menu_data);
	gtk_widget_destroy (popup_menu);
}


/* Shortcut right-click menu.  */

struct _ShortcutRightClickMenuData {
	EShortcutsView *shortcuts_view;
	int group_num;
	int item_num;
};
typedef struct _ShortcutRightClickMenuData ShortcutRightClickMenuData;

static void
activate_shortcut_cb (GtkWidget *widget,
		      void *data)
{
	ShortcutRightClickMenuData *menu_data;
	EShortcutsView *shortcuts_view;
	EShortcuts *shortcuts;
	const char *uri;

	menu_data = (ShortcutRightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	uri = e_shortcuts_get_uri (shortcuts, menu_data->group_num, menu_data->item_num);
	if (uri == NULL)
		return;

	gtk_signal_emit (GTK_OBJECT (shortcuts_view), signals[ACTIVATE_SHORTCUT],
			 shortcuts, uri);
}

static void
remove_shortcut_cb (GtkWidget *widget,
		    void *data)
{
	ShortcutRightClickMenuData *menu_data;
	EShortcutsView *shortcuts_view;
	EShortcuts *shortcuts;

	menu_data = (ShortcutRightClickMenuData *) data;
	shortcuts_view = menu_data->shortcuts_view;
	shortcuts = shortcuts_view->priv->shortcuts;

	e_shortcuts_remove_shortcut (shortcuts, menu_data->group_num, menu_data->item_num);

	/* FIXME not real model-view.  */
	e_shortcut_model_remove_item (E_SHORTCUT_BAR (shortcuts_view)->model,
				      menu_data->group_num,
				      menu_data->item_num);
}

static GnomeUIInfo shortcut_right_click_menu_uiinfo[] = {
	GNOMEUIINFO_ITEM       (N_("Activate"), N_("Activate this shortcut"),
				activate_shortcut_cb, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK (N_("Remove"), N_("Remove this shortcut from the shortcut bar"),
				remove_shortcut_cb, GNOME_STOCK_MENU_CLOSE),
	GNOMEUIINFO_END
};

static void
pop_up_right_click_menu_for_shortcut (EShortcutsView *shortcuts_view,
				      GdkEventButton *event,
				      int group_num,
				      int item_num)
{
	ShortcutRightClickMenuData *menu_data;
	GtkWidget *popup_menu;

	menu_data = g_new (ShortcutRightClickMenuData, 1);
	menu_data->shortcuts_view = shortcuts_view;
	menu_data->group_num 	  = group_num;
	menu_data->item_num  	  = item_num;

	popup_menu = gnome_popup_menu_new (shortcut_right_click_menu_uiinfo);

	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event, menu_data);

	g_free (menu_data);
	gtk_widget_destroy (popup_menu);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShortcutsViewPrivate *priv;
	EShortcutsView *shortcuts_view;

	shortcuts_view = E_SHORTCUTS_VIEW (object);

	priv = shortcuts_view->priv;

	gtk_object_unref (GTK_OBJECT (priv->shortcuts));

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EShortcutBar methods.  */

static void
item_selected (EShortcutBar *shortcut_bar,
	       GdkEvent *event,
	       int group_num,
	       int item_num)
{
	EShortcuts *shortcuts;
	EShortcutsView *shortcuts_view;
	const char *uri;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	shortcuts = shortcuts_view->priv->shortcuts;

	if (event->button.button == 3) {
		if (item_num < 0)
			pop_up_right_click_menu_for_group (shortcuts_view, &event->button,
							   group_num);
		else
			pop_up_right_click_menu_for_shortcut (shortcuts_view, &event->button,
							      group_num, item_num);
		return;
	} else if (event->button.button != 1) {
		return;
	}

	if (item_num < 0)
		return;

	uri = e_shortcuts_get_uri (shortcuts, group_num, item_num);
	if (uri == NULL)
		return;

	gtk_signal_emit (GTK_OBJECT (shortcuts_view), signals[ACTIVATE_SHORTCUT],
			 shortcuts, uri);
}

static void
impl_shortcut_dropped (EShortcutBar *shortcut_bar,
		       int group_num,
		       int position,
		       const char *item_url,
		       const char *item_name)
{
	EShortcutsView *shortcuts_view;
	EShortcutsViewPrivate *priv;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = shortcuts_view->priv;

	e_shortcuts_add_shortcut (priv->shortcuts, group_num, position, item_url);
}

static void
impl_shortcut_dragged (EShortcutBar *shortcut_bar,
		       gint group_num,
		       gint item_num)
{
	EShortcutsView *shortcuts_view;
	EShortcutsViewPrivate *priv;

	shortcuts_view = E_SHORTCUTS_VIEW (shortcut_bar);
	priv = shortcuts_view->priv;

	e_shortcuts_remove_shortcut (priv->shortcuts, group_num, item_num);
}


static void
class_init (EShortcutsViewClass *klass)
{
	GtkObjectClass *object_class;
	EShortcutBarClass *shortcut_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	shortcut_bar_class = E_SHORTCUT_BAR_CLASS (klass);
	shortcut_bar_class->item_selected    = item_selected;
	shortcut_bar_class->shortcut_dropped = impl_shortcut_dropped;
	shortcut_bar_class->shortcut_dragged = impl_shortcut_dragged;

	parent_class = gtk_type_class (e_shortcut_bar_get_type ());

	signals[ACTIVATE_SHORTCUT] =
		gtk_signal_new ("activate_shortcut",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutsViewClass, activate_shortcut),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EShortcutsView *shortcuts_view)
{
	EShortcutsViewPrivate *priv;

	priv = g_new (EShortcutsViewPrivate, 1);
	priv->shortcuts = NULL;

	shortcuts_view->priv = priv;
}


void
e_shortcuts_view_construct (EShortcutsView *shortcuts_view,
			    EShortcuts *shortcuts)
{
	EShortcutsViewPrivate *priv;

	g_return_if_fail (shortcuts != NULL);
	g_return_if_fail (E_IS_SHORTCUTS (shortcuts));

	priv = shortcuts_view->priv;

	e_shortcut_bar_set_icon_callback (E_SHORTCUT_BAR (shortcuts_view), icon_callback,
					  shortcuts);

	e_shortcut_bar_set_model (E_SHORTCUT_BAR (shortcuts_view),
				  E_SHORTCUT_MODEL (e_shortcuts_view_model_new (shortcuts)));
}

GtkWidget *
e_shortcuts_view_new (EShortcuts *shortcuts)
{
	GtkWidget *new;

	g_return_val_if_fail (shortcuts != NULL, NULL);
	g_return_val_if_fail (E_IS_SHORTCUTS (shortcuts), NULL);

	new = gtk_type_new (e_shortcuts_view_get_type ());
	e_shortcuts_view_construct (E_SHORTCUTS_VIEW (new), shortcuts);

	return new;
}


E_MAKE_TYPE (e_shortcuts_view, "EShortcutsView", EShortcutsView, class_init, init, PARENT_TYPE)

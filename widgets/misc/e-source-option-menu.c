/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-option-menu.c
 *
 * Copyright (C) 2003  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include "e-source-option-menu.h"

#include <gal/util/e-util.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>


#define PARENT_TYPE gtk_option_menu_get_type ()
static GtkOptionMenuClass *parent_class = NULL;


/* We set data on each menu item specifying the corresponding ESource using this key.  */
#define MENU_ITEM_SOURCE_DATA_ID	"ESourceOptionMenu:Source"


struct _ESourceOptionMenuPrivate {
	ESourceList *source_list;

	ESource *selected_source;
};


/* Selecting a source.  */

typedef struct {
	ESourceOptionMenu *option_menu;
	int i;
} ForeachMenuItemData;

static void
select_source_foreach_menu_item (GtkWidget *menu_item,
				 ForeachMenuItemData *data)
{
	ESource *source = gtk_object_get_data (GTK_OBJECT (menu_item), MENU_ITEM_SOURCE_DATA_ID);

	if (source == data->option_menu->priv->selected_source)
		gtk_option_menu_set_history (GTK_OPTION_MENU (data->option_menu), data->i);

	data->i ++;
}

static void
select_source (ESourceOptionMenu *menu,
	       ESource *source)
{
	if (menu->priv->selected_source != NULL)
		g_object_unref (menu->priv->selected_source);
	menu->priv->selected_source = source;

	if (source != NULL) {
		ForeachMenuItemData *foreach_data = g_new0 (ForeachMenuItemData, 1);

		foreach_data->option_menu = menu;

		gtk_container_foreach (GTK_CONTAINER (GTK_OPTION_MENU (menu)->menu),
				       (GtkCallback) select_source_foreach_menu_item, foreach_data);

		g_free (foreach_data);
		g_object_ref (source);
	}
}


/* Menu callback.  */

static void
menu_item_activate_callback (GtkMenuItem *menu_item,
			     ESourceOptionMenu *option_menu)
{
	ESource *source = gtk_object_get_data (GTK_OBJECT (menu_item), MENU_ITEM_SOURCE_DATA_ID);

	if (source != NULL)
		select_source (option_menu, source);
}


/* Functions to keep the menu in sync with the ESourceList.  */

static void
populate (ESourceOptionMenu *option_menu)
{
	GtkWidget *menu = gtk_menu_new ();
	GSList *groups = e_source_list_peek_groups (option_menu->priv->source_list);
	GSList *p;
	ESource *first_source = NULL;
	int first_source_item = -1;
	int selected_item = -1;
	int i;

	i = 0;
	for (p = groups; p != NULL; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		GtkWidget *item = gtk_menu_item_new_with_label (e_source_group_peek_name (group));
		GSList *q;

		gtk_widget_set_sensitive (item, FALSE);
		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);

		i ++;

		for (q = e_source_group_peek_sources (group); q != NULL; q = q->next) {
			ESource *source = E_SOURCE (q->data);
			char *label = g_strconcat ("    ", e_source_peek_name (source), NULL);
			GtkWidget *item = gtk_menu_item_new_with_label (label);

			gtk_object_set_data_full (GTK_OBJECT (item), MENU_ITEM_SOURCE_DATA_ID, source,
						  (GtkDestroyNotify) g_object_unref);
			g_object_ref (source);

			g_signal_connect (item, "activate", G_CALLBACK (menu_item_activate_callback), option_menu);

			gtk_widget_show (item);
			gtk_menu_append (GTK_MENU (menu), item);

			if (first_source_item == -1) {
				first_source_item = i;
				first_source = source;
			}

			if (source == option_menu->priv->selected_source)
				selected_item = i;

			i ++;
		}
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	if (selected_item != -1) {
		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), selected_item);
	} else {
		if (option_menu->priv->selected_source != NULL)
			g_object_unref (option_menu->priv->selected_source);
		option_menu->priv->selected_source = first_source;
		if (option_menu->priv->selected_source != NULL)
			g_object_ref (option_menu->priv->selected_source);

		gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), first_source_item);
	}
}


static void
source_list_changed_callback (ESourceList *list,
			      ESourceOptionMenu *menu)
{
	populate (menu);
}

static void
connect_signals (ESourceOptionMenu *menu)
{
	g_signal_connect_object (menu->priv->source_list, "changed",
				 G_CALLBACK (source_list_changed_callback), G_OBJECT (menu), 0);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ESourceOptionMenuPrivate *priv = E_SOURCE_OPTION_MENU (object)->priv;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->selected_source != NULL) {
		g_object_unref (priv->selected_source);
		priv->selected_source = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ESourceOptionMenuPrivate *priv = E_SOURCE_OPTION_MENU (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (ESourceOptionMenuClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
init (ESourceOptionMenu *source_option_menu)
{
	ESourceOptionMenuPrivate *priv;

	priv = g_new0 (ESourceOptionMenuPrivate, 1);

	source_option_menu->priv = priv;
}


/* Public methods.  */

GtkWidget *
e_source_option_menu_new (ESourceList *source_list)
{
	ESourceOptionMenu *menu;

	g_return_val_if_fail (E_IS_SOURCE_LIST (source_list), NULL);

	menu = g_object_new (e_source_option_menu_get_type (), NULL);

	menu->priv->source_list = source_list;
	g_object_ref (source_list);

	connect_signals (menu);
	populate (menu);

	return GTK_WIDGET (menu);
}


ESource *
e_source_option_menu_peek_selected  (ESourceOptionMenu *menu)
{
	g_return_val_if_fail (E_IS_SOURCE_OPTION_MENU (menu), NULL);

	return menu->priv->selected_source;
}


void
e_source_option_menu_select (ESourceOptionMenu *menu,
			     ESource *source)
{
	g_return_if_fail (E_IS_SOURCE_OPTION_MENU (menu));
	g_return_if_fail (E_IS_SOURCE (source));

	select_source (menu, source);
}


E_MAKE_TYPE (e_source_option_menu, "ESourceOptionMenu", ESourceOptionMenu, class_init, init, PARENT_TYPE)

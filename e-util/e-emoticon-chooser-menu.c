/*
 * e-emoticon-chooser-menu.c
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include "e-emoticon-chooser-menu.h"
#include "e-emoticon-chooser.h"

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_CURRENT_FACE
};

/* Forward Declarations */
static void	e_emoticon_chooser_menu_interface_init
					(EEmoticonChooserInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EEmoticonChooserMenu,
	e_emoticon_chooser_menu,
	GTK_TYPE_MENU,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EMOTICON_CHOOSER,
		e_emoticon_chooser_menu_interface_init))

static void
emoticon_chooser_menu_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FACE:
			e_emoticon_chooser_set_current_emoticon (
				E_EMOTICON_CHOOSER (object),
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
emoticon_chooser_menu_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CURRENT_FACE:
			g_value_set_boxed (
				value,
				e_emoticon_chooser_get_current_emoticon (
				E_EMOTICON_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static EEmoticon *
emoticon_chooser_menu_get_current_emoticon (EEmoticonChooser *chooser)
{
	GtkWidget *item;

	item = gtk_menu_get_active (GTK_MENU (chooser));
	if (item == NULL)
		return NULL;

	return g_object_get_data (G_OBJECT (item), "emoticon");
}

static void
emoticon_chooser_menu_set_current_emoticon (EEmoticonChooser *chooser,
                                            EEmoticon *emoticon)
{
	GList *list, *iter;

	list = gtk_container_get_children (GTK_CONTAINER (chooser));

	for (iter = list; iter != NULL; iter = iter->next) {
		GtkWidget *item = iter->data;
		EEmoticon *candidate;

		candidate = g_object_get_data (G_OBJECT (item), "emoticon");
		if (candidate == NULL)
			continue;

		if (e_emoticon_equal (emoticon, candidate)) {
			gtk_menu_shell_activate_item (
				GTK_MENU_SHELL (chooser), item, TRUE);
			break;
		}
	}

	g_list_free (list);
}

static void
e_emoticon_chooser_menu_class_init (EEmoticonChooserMenuClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = emoticon_chooser_menu_set_property;
	object_class->get_property = emoticon_chooser_menu_get_property;

	g_object_class_override_property (
		object_class, PROP_CURRENT_FACE, "current-emoticon");
}

static void
e_emoticon_chooser_menu_interface_init (EEmoticonChooserInterface *interface)
{
	interface->get_current_emoticon =
		emoticon_chooser_menu_get_current_emoticon;
	interface->set_current_emoticon =
		emoticon_chooser_menu_set_current_emoticon;
}

static void
e_emoticon_chooser_menu_init (EEmoticonChooserMenu *chooser_menu)
{
	EEmoticonChooser *chooser;
	GList *list, *iter;

	chooser = E_EMOTICON_CHOOSER (chooser_menu);
	list = e_emoticon_chooser_get_items ();

	for (iter = list; iter != NULL; iter = iter->next) {
		EEmoticon *emoticon = iter->data;
		GtkWidget *item;

		/* To keep translated strings in subclasses */
		item = gtk_image_menu_item_new_with_mnemonic (_(emoticon->label));
		gtk_image_menu_item_set_image (
			GTK_IMAGE_MENU_ITEM (item),
			gtk_image_new_from_icon_name (
			emoticon->icon_name, GTK_ICON_SIZE_MENU));
		gtk_widget_show (item);

		g_object_set_data_full (
			G_OBJECT (item), "emoticon",
			e_emoticon_copy (emoticon),
			(GDestroyNotify) e_emoticon_free);

		g_signal_connect_swapped (
			item, "activate",
			G_CALLBACK (e_emoticon_chooser_item_activated),
			chooser);

		gtk_menu_shell_append (GTK_MENU_SHELL (chooser_menu), item);
	}

	g_list_free (list);
}

GtkWidget *
e_emoticon_chooser_menu_new (void)
{
	return g_object_new (E_TYPE_EMOTICON_CHOOSER_MENU, NULL);
}

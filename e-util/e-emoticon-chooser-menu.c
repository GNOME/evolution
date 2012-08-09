/* e-emoticon-chooser-menu.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-emoticon-chooser-menu.h"
#include "e-emoticon-chooser.h"

#include <glib/gi18n-lib.h>

enum {
	PROP_0,
	PROP_CURRENT_FACE
};

static gpointer parent_class;

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
emoticon_chooser_menu_class_init (EEmoticonChooserMenuClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = emoticon_chooser_menu_set_property;
	object_class->get_property = emoticon_chooser_menu_get_property;

	g_object_class_override_property (
		object_class, PROP_CURRENT_FACE, "current-emoticon");
}

static void
emoticon_chooser_menu_iface_init (EEmoticonChooserIface *iface)
{
	iface->get_current_emoticon = emoticon_chooser_menu_get_current_emoticon;
	iface->set_current_emoticon = emoticon_chooser_menu_set_current_emoticon;
}

static void
emoticon_chooser_menu_init (EEmoticonChooserMenu *chooser_menu)
{
	EEmoticonChooser *chooser;
	GList *list, *iter;

	chooser = E_EMOTICON_CHOOSER (chooser_menu);
	list = e_emoticon_chooser_get_items ();

	for (iter = list; iter != NULL; iter = iter->next) {
		EEmoticon *face = iter->data;
		GtkWidget *item;

		/* To keep translated strings in subclasses */
		item = gtk_image_menu_item_new_with_mnemonic (_(face->label));
		gtk_image_menu_item_set_image (
			GTK_IMAGE_MENU_ITEM (item),
			gtk_image_new_from_icon_name (
			face->icon_name, GTK_ICON_SIZE_MENU));
		gtk_widget_show (item);

		g_object_set_data_full (
			G_OBJECT (item), "face",
			e_emoticon_copy (face),
			(GDestroyNotify) e_emoticon_free);

		g_signal_connect_swapped (
			item, "activate",
			G_CALLBACK (e_emoticon_chooser_item_activated),
			chooser);

		gtk_menu_shell_append (GTK_MENU_SHELL (chooser_menu), item);
	}

	g_list_free (list);
}

GType
e_emoticon_chooser_menu_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EEmoticonChooserMenuClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) emoticon_chooser_menu_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EEmoticonChooserMenu),
			0,     /* n_preallocs */
			(GInstanceInitFunc) emoticon_chooser_menu_init,
			NULL   /* value_table */
		};

		static const GInterfaceInfo iface_info = {
			(GInterfaceInitFunc) emoticon_chooser_menu_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL  /* interface_data */
		};

		type = g_type_register_static (
			GTK_TYPE_MENU, "EEmoticonChooserMenu",
			&type_info, 0);

		g_type_add_interface_static (
			type, E_TYPE_EMOTICON_CHOOSER, &iface_info);
	}

	return type;
}

GtkWidget *
e_emoticon_chooser_menu_new (void)
{
	return g_object_new (E_TYPE_EMOTICON_CHOOSER_MENU, NULL);
}

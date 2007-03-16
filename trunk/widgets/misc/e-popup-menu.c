/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-popup-menu.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Jody Goldberg (jgoldberg@home.com)
 *   Jeffrey Stedfast <fejj@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <libintl.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "e-gui-utils.h"
#include "e-popup-menu.h"

/*
 * Creates an item with an optional icon
 */
static void
make_item (GtkMenu *menu, GtkMenuItem *item, const char *name, GtkWidget *pixmap)
{
	GtkWidget *label;

	if (*name == '\0')
		return;

	/*
	 * Ugh.  This needs to go into Gtk+
	 */
	label = gtk_label_new_with_mnemonic (name);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	
	gtk_container_add (GTK_CONTAINER (item), label);
	
	if (pixmap && GTK_IS_IMAGE_MENU_ITEM (item)){
		gtk_widget_show (pixmap);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), pixmap);
	}
}

GtkMenu *
e_popup_menu_create (EPopupMenu *menu_list,
		     guint32 disable_mask,
		     guint32 hide_mask,
		     void *default_closure)
{
	return e_popup_menu_create_with_domain (menu_list,
						disable_mask,
						hide_mask,
						default_closure,
						NULL);
}


GtkMenu *
e_popup_menu_create_with_domain (EPopupMenu *menu_list,
				 guint32 disable_mask,
				 guint32 hide_mask,
				 void *default_closure,
				 const char *domain)
{
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
	GSList *group = NULL;
	gboolean last_item_separator = TRUE;
	int last_non_separator = -1;
	int i;

	for (i = 0; menu_list[i].name; i++) {
		if (strcmp ("", menu_list[i].name) && !(menu_list [i].disable_mask & hide_mask)) {
			last_non_separator = i;
		}
	}

	for (i = 0; i <= last_non_separator; i++) {
		gboolean separator;

		separator = !strcmp ("", menu_list[i].name);

		if ((!(separator && last_item_separator)) && !(menu_list [i].disable_mask & hide_mask)) {
			GtkWidget *item = NULL;

			if (!separator) {
				if (menu_list[i].is_toggle)
					item = gtk_check_menu_item_new ();
				else if (menu_list[i].is_radio)
					item = gtk_radio_menu_item_new (group);
				else
					item = menu_list[i].pixmap_widget ? gtk_image_menu_item_new () : gtk_menu_item_new ();
				if (menu_list[i].is_toggle || menu_list[i].is_radio)
					gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), menu_list[i].is_active);
				if (menu_list[i].is_radio)
					group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

				make_item (menu, GTK_MENU_ITEM (item), dgettext(domain, menu_list[i].name), menu_list[i].pixmap_widget);
			} else {
				item = gtk_menu_item_new ();
			}

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			if (!menu_list[i].submenu) {
				if (menu_list[i].fn)
					g_signal_connect (item, "activate",
							  G_CALLBACK (menu_list[i].fn),
							  menu_list[i].use_custom_closure ? menu_list[i].closure : default_closure);
			} else {
				/* submenu */
				GtkMenu *submenu;

				submenu = e_popup_menu_create (menu_list[i].submenu, disable_mask, hide_mask,
							       default_closure);

				gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
			}

			if (menu_list[i].disable_mask & disable_mask)
				gtk_widget_set_sensitive (item, FALSE);

			gtk_widget_show (item);

			last_item_separator = separator;
		}
	}

	return menu;
}

void
e_popup_menu_run (EPopupMenu *menu_list, GdkEvent *event, guint32 disable_mask, guint32 hide_mask, void *default_closure)
{
	GtkMenu *menu;
	
	g_return_if_fail (menu_list != NULL);
	g_return_if_fail (event != NULL);
	
	menu = e_popup_menu_create (menu_list, disable_mask, hide_mask, default_closure);
	
	e_popup_menu (menu, event);
}

void
e_popup_menu_copy_1 (EPopupMenu *destination,
		     const EPopupMenu *source)
{
	destination->name = g_strdup (source->name);
	destination->pixname = g_strdup (source->pixname);
	destination->fn = source->fn;
	destination->submenu = e_popup_menu_copy (source->submenu);
	destination->disable_mask = source->disable_mask;

	destination->pixmap_widget = source->pixmap_widget;
	if (destination->pixmap_widget)
		g_object_ref (destination->pixmap_widget);
	destination->closure = source->closure;

	destination->is_toggle = source->is_toggle;
	destination->is_radio = source->is_radio;
	destination->is_active = source->is_active;

	destination->use_custom_closure = source->use_custom_closure;
}

void
e_popup_menu_free_1 (EPopupMenu *menu_item)
{
	g_free (menu_item->name);
	g_free (menu_item->pixname);
	e_popup_menu_free (menu_item->submenu);

	if (menu_item->pixmap_widget)
		g_object_unref (menu_item->pixmap_widget);
}

EPopupMenu *
e_popup_menu_copy (const EPopupMenu *menu_list)
{
	int i;
	EPopupMenu *ret_val;

	if (menu_list == NULL)
		return NULL;

	for (i = 0; menu_list[i].name; i++) {
		/* Intentionally empty */
	}

	ret_val = g_new (EPopupMenu, i + 1);

	for (i = 0; menu_list[i].name; i++) {
		e_popup_menu_copy_1 (ret_val + i, menu_list + i);
	}

	/* Copy the terminator */
	e_popup_menu_copy_1 (ret_val + i, menu_list + i);

	return ret_val;
}

void
e_popup_menu_free (EPopupMenu *menu_list)
{
	int i;

	if (menu_list == NULL)
		return;

	for (i = 0; menu_list[i].name; i++) {
		e_popup_menu_free_1 (menu_list + i);
	}
	g_free (menu_list);
}


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
#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkimagemenuitem.h>

#include "e-popup-menu.h"
#include "e-gui-utils.h"

#include <gal/util/e-i18n.h>

#ifndef GNOME_APP_HELPER_H
/* Copied this i18n function to use for the same purpose */

#ifdef ENABLE_NLS
#define L_(x) gnome_app_helper_gettext(x)

static gchar *
gnome_app_helper_gettext (const gchar *str)
{
	char *s;

        s = gettext (str);
	if ( s == str )
	        s = dgettext (PACKAGE, str);

	return s;
}

#else
#define L_(x) x
#endif

#endif

/*
 * Creates an item with an optional icon
 */
static GtkWidget *
make_item (GtkMenu *menu, const char *name, const char *pixname)
{
	GtkWidget *item;

	if (*name == '\0')
		return gtk_menu_item_new ();

	item = gtk_image_menu_item_new_with_mnemonic (name);
	gtk_image_menu_item_set_image (
		GTK_IMAGE_MENU_ITEM (item),
		gtk_image_new_from_stock (pixname, GTK_ICON_SIZE_MENU));

	gtk_widget_show_all (GTK_WIDGET (item));
	return item;
}

GtkMenu *
e_popup_menu_create (EPopupMenu *menu_list, guint32 disable_mask, guint32 hide_mask, void *closure)
{
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
	gboolean last_item_seperator = TRUE;
	gint last_non_seperator = -1;
	gint i;
	
	for (i = 0; menu_list[i].name; i++) {
		if (strcmp ("", menu_list[i].name) && !(menu_list [i].disable_mask & hide_mask)) {
			last_non_seperator = i;
		}
	}
	
	for (i = 0; i <= last_non_seperator; i++) {
		gboolean seperator;
		
		seperator = !strcmp ("", menu_list[i].name);
		
		if ((!(seperator && last_item_seperator)) && !(menu_list [i].disable_mask & hide_mask)) {
			GtkWidget *item;
			
			item = make_item (menu, seperator ? "" : L_(menu_list[i].name), menu_list[i].pixname);
			gtk_menu_append (menu, item);

			if (!menu_list[i].submenu) {
				if (menu_list[i].fn)
					gtk_signal_connect (GTK_OBJECT (item), "activate",
							    GTK_SIGNAL_FUNC (menu_list[i].fn),
							    closure);
			} else {
				/* submenu */
				GtkMenu *submenu;

				submenu = e_popup_menu_create (menu_list[i].submenu, disable_mask, hide_mask, closure);

				gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
			}

			if (menu_list[i].disable_mask & disable_mask)
				gtk_widget_set_sensitive (item, FALSE);

			gtk_widget_show (item);

			last_item_seperator = seperator;
		}
	}

	return menu;
}

void
e_popup_menu_run (EPopupMenu *menu_list, GdkEvent *event, guint32 disable_mask, guint32 hide_mask, void *closure)
{
	GtkMenu *menu;
	
	g_return_if_fail (menu_list != NULL);
	g_return_if_fail (event != NULL);
	
	menu = e_popup_menu_create (menu_list, disable_mask, hide_mask, closure);
	
	e_popup_menu (menu, event);
}


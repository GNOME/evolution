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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkaccellabel.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gtkpixmapmenuitem.h>
#include <libgnomeui/gnome-stock.h>

#include "e-popup-menu.h"
#include "e-gui-utils.h"

#include <libgnome/gnome-i18n.h>

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
make_item (GtkMenu *menu, const char *name, GtkWidget *pixmap)
{
	GtkWidget *label, *item;
	guint label_accel;
	
	if (*name == '\0')
		return gtk_menu_item_new ();
	
	/*
	 * Ugh.  This needs to go into Gtk+
	 */
	label = gtk_accel_label_new ("");
	label_accel = gtk_label_parse_uline (GTK_LABEL (label), name);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	
	item = pixmap ? gtk_pixmap_menu_item_new () : gtk_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (item), label);
	
	if (label_accel != GDK_VoidSymbol){
		gtk_widget_add_accelerator (
			item,
			"activate_item",
			gtk_menu_ensure_uline_accel_group (GTK_MENU (menu)),
			label_accel, 0,
			GTK_ACCEL_LOCKED);
	}
	
	if (pixmap){
		gtk_widget_show (pixmap);
		gtk_pixmap_menu_item_set_pixmap (GTK_PIXMAP_MENU_ITEM (item), pixmap);
	}
	
	return item;
}

GtkMenu *
e_popup_menu_create (EPopupMenu *menu_list, guint32 disable_mask, guint32 hide_mask, void *default_closure)
{
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
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
			GtkWidget *item;
			
			if (!separator)
				item = make_item (menu, L_(menu_list[i].name), menu_list[i].pixmap);
			else
				item = make_item (menu, "", NULL);
			
			gtk_menu_append (menu, item);
			
			if (!menu_list[i].submenu) {
				if (menu_list[i].fn)
					gtk_signal_connect (GTK_OBJECT (item), "activate",
							    GTK_SIGNAL_FUNC (menu_list[i].fn),
							    menu_list[i].closure ? menu_list[i].closure : default_closure);
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


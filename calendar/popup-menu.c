/* Popup menu utilities for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#include <config.h>
#include <gnome.h>
#include "popup-menu.h"


void
popup_menu (struct menu_item *items, int nitems, GdkEventButton *event)
{
	GtkWidget *menu;
	GtkWidget *item;
	int i;

	menu = gtk_menu_new (); /* FIXME: this baby is never freed */

	for (i = 0; i < nitems; i++) {
		if (items[i].text) {
			item = gtk_menu_item_new_with_label (_(items[i].text));
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    items[i].callback,
					    items[i].data);
			gtk_widget_set_sensitive (item, items[i].sensitive);
		} else
			item = gtk_menu_item_new ();

		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
}

/* Popup menu utilities for gncal
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <quartic@gimp.org>
 */

#ifndef POPUP_MENU_H
#define POPUP_MENU_H

#include <gdk/gdktypes.h>
#include <gtk/gtksignal.h>


struct menu_item {
	char *text;
	GtkSignalFunc callback;
	gpointer data;
	int sensitive;
};

void popup_menu (struct menu_item *items, int nitems, GdkEventButton *event);


#endif

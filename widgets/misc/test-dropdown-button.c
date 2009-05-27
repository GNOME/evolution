/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-ui-init.h>

#include "e-dropdown-button.h"


/* (The following is shameless stolen from `testgnome.c'.  */

static void
item_activated (GtkWidget *widget,
		gpointer data)
{
	printf ("%s activated.\n", (gchar *) data);
}

static GnomeUIInfo ui_info[] = {
	{ GNOME_APP_UI_ITEM, "_New", "Create a new file", item_activated, (gpointer) "file/new", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_NEW, 'n', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, "_Open...", "Open an existing file", item_activated, (gpointer) "file/open", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_OPEN, 'o', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, "_Save", "Save the current file", item_activated, (gpointer) "file/save", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_SAVE, 's', GDK_CONTROL_MASK, NULL },
	{ GNOME_APP_UI_ITEM, "Save _as...", "Save the current file with a new name", item_activated, (gpointer) "file/save as", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_SAVE_AS, 0, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, "_Print...", "Print the current file", item_activated, (gpointer) "file/print", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PRINT, 'p', GDK_CONTROL_MASK, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, "_Close", "Close the current file", item_activated, (gpointer) "file/close", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_CLOSE, 0, 0, NULL },
	{ GNOME_APP_UI_ITEM, "E_xit", "Exit the program", item_activated, (gpointer) "file/exit", NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_QUIT, 'q', GDK_CONTROL_MASK, NULL },
	GNOMEUIINFO_END
};


gint
main (gint argc, gchar **argv)
{
	GtkWidget *window;
	GtkWidget *menu;
	GtkWidget *dropdown_button;

	gnome_program_init (
		"test-dropdown-button", "0.0", LIBGNOMEUI_MODULE,
		argc, argv, GNOME_PARAM_NONE);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 1, 1);

	menu = gtk_menu_new ();

	gnome_app_fill_menu (GTK_MENU_SHELL (menu), ui_info, NULL, TRUE, 0);

	dropdown_button = e_dropdown_button_new ("Me_nu", GTK_MENU (menu));
	gtk_container_add (GTK_CONTAINER (window), dropdown_button);

	gtk_widget_show (window);
	gtk_widget_show (dropdown_button);

	gtk_main ();

	return 0;
}

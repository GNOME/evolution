/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-source-option-menu.c - Test for ESourceOptionMenu.
 *
 * Copyright (C) 2003 Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "e-source-option-menu.h"

#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>

#include <libgnomeui/gnome-ui-init.h>


static void
source_selected_callback (ESourceOptionMenu *menu,
			  ESource *source,
			  void *unused_data)
{
	g_print ("source selected: \"%s\"\n", e_source_peek_name (source));
}


static int
on_idle_create_widget (const char *gconf_path)
{
	GtkWidget *window;
	GtkWidget *option_menu;
	ESourceList *source_list;
	GConfClient *gconf_client;

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, gconf_path);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	option_menu = e_source_option_menu_new (source_list);
	g_signal_connect (option_menu, "source_selected", G_CALLBACK (source_selected_callback), NULL);

	gtk_container_add (GTK_CONTAINER (window), option_menu);
	gtk_widget_show_all (window);

	g_object_unref (gconf_client);
	g_object_unref (source_list);

	return FALSE;
}


int
main (int argc, char **argv)
{
	GnomeProgram *program;
	const char *gconf_path;

	program = gnome_program_init ("test-source-list", "0.0", LIBGNOMEUI_MODULE, argc, argv, NULL);

	if (argc < 2)
		gconf_path = "/apps/evolution/calendar/sources";
	else
		gconf_path = argv [1];

	g_idle_add ((GSourceFunc) on_idle_create_widget, (void *) gconf_path);

	gtk_main ();

	return 0;
}

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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include "e-contact-print-style-editor.h"

int
main (int argc, char *argv[])
{
	GtkWidget *editor;
	GtkWidget *window;
	const gchar *title;

	title = "Contact Print Style Editor Test";

	gtk_init (&argc, &argv);

	glade_init ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), title);

	editor = e_contact_print_style_editor_new ("");
	gtk_container_add (GTK_CONTAINER (window), editor);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	gtk_main ();

	/* Not reached. */
	return 0;
}

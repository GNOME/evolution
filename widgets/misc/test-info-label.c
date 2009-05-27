/*
 *
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "e-info-label.h"

static void
delete_event_cb (GtkWidget *widget,
		 GdkEventAny *event,
		 gpointer data)
{
	gtk_main_quit ();
}

gint
main (gint argc, gchar **argv)
{
	GtkWidget *window;
	GtkWidget *info_label;
	GtkWidget *label;
	GtkWidget *vbox;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "EInfoLabel Test");
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	info_label = e_info_label_new ("stock_default-folder");
	e_info_label_set_info ((EInfoLabel *) info_label, "Component Name", "An annoyingly long component message");
	gtk_widget_show (info_label);

	label = gtk_label_new ("boo");
	gtk_widget_show (label);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), info_label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (window);

	gtk_main ();

	return 0;
}

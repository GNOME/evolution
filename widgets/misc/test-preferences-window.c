/*
 * test-preferences-window.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-preferences-window.c"

#include <gtk/gtk.h>

static GtkWidget *
create_page_number (gint i)
{
	gchar *caption;
	GtkWidget *widget;

	caption = g_strdup_printf ("Title of page %d", i);

	widget = gtk_label_new (caption);
	gtk_widget_show (widget);

	g_free (caption);

	return widget;
}

static GtkWidget *
create_page_zero (EPreferencesWindow *preferences_window)
{
	return create_page_number (0);
}
static GtkWidget *
create_page_one (EPreferencesWindow *preferences_window)
{
	return create_page_number (1);
}
static GtkWidget *
create_page_two (EPreferencesWindow *preferences_window)
{
	return create_page_number (2);
}

static void
add_pages (EPreferencesWindow *preferences_window)
{
	e_preferences_window_add_page (preferences_window, "page-0",
				       "gtk-properties", "title 0",
				       create_page_zero, 0);
	e_preferences_window_add_page (preferences_window, "page-1",
				       "gtk-properties", "title 1",
				       create_page_one, 1);
	e_preferences_window_add_page (preferences_window, "page-2",
				       "gtk-properties", "title 2",
				       create_page_two, 2);
}

static gint
delete_event_callback (GtkWidget *widget,
		       GdkEventAny *event,
		       gpointer data)
{
	gtk_main_quit ();

	return TRUE;
}

gint
main (gint argc, gchar **argv)
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	window = e_preferences_window_new (NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (delete_event_callback), NULL);

	add_pages (E_PREFERENCES_WINDOW (window));
	e_preferences_window_setup (E_PREFERENCES_WINDOW (window));

	gtk_widget_show (window);

	gtk_main ();

	return 0;
}

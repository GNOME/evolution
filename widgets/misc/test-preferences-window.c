/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-preferences-window.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-preferences-window.c"

#include <gtk/gtk.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-ui-init.h>

#define NUM_PAGES 10

static void
add_pages (EPreferencesWindow *preferences_window)
{
	int i;

	for (i = 0; i < NUM_PAGES; i ++) {
		GtkWidget *widget;
		char *caption;
		char *page_name;

		caption = g_strdup_printf ("Title of page %d", i);
		page_name = g_strdup_printf ("page-%d", i);

		widget = gtk_label_new (caption);
		gtk_widget_show (widget);

		e_preferences_window_add_page (
			preferences_window, page_name,
			"gtk-properties", caption, i, widget);

		g_free (caption);
		g_free (page_name);
	}
}

static int
delete_event_callback (GtkWidget *widget,
		       GdkEventAny *event,
		       void *data)
{
	gtk_main_quit ();

	return TRUE;
}

int
main (int argc, char **argv)
{
	GtkWidget *dialog;

	gnome_program_init (
		"test-preferences-window", "0.0", LIBGNOMEUI_MODULE,
		argc, argv, GNOME_PARAM_NONE);

	dialog = e_preferences_window_new ();

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);
	g_signal_connect((dialog), "delete_event",
			    G_CALLBACK (delete_event_callback), NULL);

	add_pages (E_PREFERENCES_WINDOW (dialog));

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}

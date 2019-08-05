/*
 * test-preferences-window.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
	e_preferences_window_add_page (
		preferences_window, "page-0",
		"document-properties", "title 0", NULL,
		create_page_zero, 0);
	e_preferences_window_add_page (
		preferences_window, "page-1",
		"document-properties", "title 1", NULL,
		create_page_one, 1);
	e_preferences_window_add_page (
		preferences_window, "page-2",
		"document-properties", "title 2", NULL,
		create_page_two, 2);
}

static void
window_notify_visible_cb (GObject *object,
			  GParamSpec *param,
			  gpointer user_data)
{
	if (!gtk_widget_get_visible (GTK_WIDGET (object)))
		gtk_main_quit ();
}

gint
main (gint argc,
      gchar **argv)
{
	GtkWidget *window;

	gtk_init (&argc, &argv);

	window = e_preferences_window_new (NULL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	g_signal_connect (
		window, "notify::visible",
		G_CALLBACK (window_notify_visible_cb), NULL);

	add_pages (E_PREFERENCES_WINDOW (window));
	e_preferences_window_setup (E_PREFERENCES_WINDOW (window));

	gtk_widget_show (window);

	gtk_main ();

	gtk_widget_destroy (window);
	e_misc_util_free_global_memory ();

	return 0;
}

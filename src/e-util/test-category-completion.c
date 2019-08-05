/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#include <e-util/e-util.h>

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static gboolean
on_idle_create_widget (void)
{
	GtkWidget *window;
	GtkWidget *vgrid;
	GtkWidget *entry;
	GtkEntryCompletion *completion;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 200);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	vgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", FALSE,
		"row-spacing", 3,
		NULL);
	gtk_container_add (GTK_CONTAINER (window), vgrid);

	entry = gtk_entry_new ();
	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);
	gtk_widget_set_vexpand (entry, TRUE);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_widget_set_halign (entry, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (vgrid), entry);

	gtk_widget_show_all (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	gtk_init (&argc, &argv);

	g_idle_add ((GSourceFunc) on_idle_create_widget, NULL);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}

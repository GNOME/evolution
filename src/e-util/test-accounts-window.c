/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-util.h"

static gboolean
delete_event_cb (GtkWidget *widget,
		 GdkEvent *event,
		 gpointer user_data)
{
	GMainLoop *loop = user_data;

	g_main_loop_quit (loop);

	return FALSE;
}

gint
main (gint argc,
      gchar *argv[])
{
	ESourceRegistry *registry;
	GtkWidget *window;
	GMainLoop *loop;
	GList *modules;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	e_util_init_main_thread (NULL);

	registry = e_source_registry_new_sync (NULL, &error);
	if (error) {
		g_warning ("Failed to create source registry: %s", error->message);
		g_clear_error (&error);
		return 1;
	}

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);

	loop = g_main_loop_new (NULL, FALSE);

	window = e_accounts_window_new (registry);

	g_signal_connect (window, "delete-event", G_CALLBACK (delete_event_cb), loop);

	e_accounts_window_show_with_parent (E_ACCOUNTS_WINDOW (window), NULL);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	g_object_unref (registry);

	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);
	e_misc_util_free_global_memory ();

	return 0;
}

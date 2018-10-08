/*
 * test-proxy-preferences.c
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
 */

#include <e-util/e-util.h>

static EProxyPreferences *preferences;

static gboolean
quit_delay_cb (gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static gint
delete_event_cb (GtkWidget *widget,
                 GdkEventAny *event,
                 gpointer data)
{
	e_proxy_preferences_submit (preferences);

	/* Cycle the main loop for a bit longer to
	 * give the commit operation time to finish. */
	g_timeout_add_seconds (1, quit_delay_cb, NULL);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	GtkWidget *window;
	GtkWidget *widget;
	GError *local_error = NULL;

	gtk_init (&argc, &argv);

	registry = e_source_registry_new_sync (NULL, &local_error);

	if (local_error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			local_error->message);
		g_return_val_if_reached (-1);
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	gtk_window_set_title (GTK_WINDOW (window), "Proxy Preferences");
	gtk_widget_show (window);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (delete_event_cb), NULL);

	widget = e_proxy_preferences_new (registry);
	gtk_container_add (GTK_CONTAINER (window), widget);
	preferences = E_PROXY_PREFERENCES (widget);
	gtk_widget_show (widget);

	g_object_unref (registry);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}


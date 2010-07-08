/*
 * killev.c
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

#include <config.h>

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include <libedataserver/e-data-server-util.h>

/* Seconds to wait after asking Evolution to terminate gracefully.
 * If the process has not terminated before the timeout expires,
 * then we get violent. */
#define EVOLUTION_SHUTDOWN_TIMEOUT 5

static GPid evolution_pid;
static GMainLoop *main_loop;

static void
file_monitor_changed_cb (GFileMonitor *monitor,
                         GFile *file,
                         GFile *not_used,
                         GFileMonitorEvent event_type)
{
	if (event_type != G_FILE_MONITOR_EVENT_DELETED)
		return;

	g_print ("Evolution process exited normally\n");

	g_main_loop_quit (main_loop);
}

static gboolean
evolution_not_responding_cb (void)
{
	g_print ("No response from Evolution -- killing the process\n");

	/* Kill the process. */
	kill ((pid_t) evolution_pid, SIGTERM);

	g_main_loop_quit (main_loop);

	return FALSE;
}

static gboolean
get_evolution_pid (GFile *file)
{
	gint64 v_int64;
	gchar *contents = NULL;
	gboolean success = FALSE;

	/* Try to read Evolution's PID from its .running file. */

	if (!g_file_load_contents (file, NULL, &contents, NULL, NULL, NULL))
		goto exit;

	/* Try to extract an integer value from the string. */
	v_int64 = g_ascii_strtoll (contents, NULL, 10);
	if (!(v_int64 > 0 && v_int64 < G_MAXINT64))
		goto exit;

	/* XXX Probably not portable. */
	evolution_pid = (GPid) v_int64;

	success = TRUE;

exit:
	g_free (contents);

	return success;
}

gint
main (gint argc, gchar **argv)
{
	GFile *pid_file;
	GFileMonitor *monitor;
	const gchar *user_config_dir;
	gchar *filename;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	user_config_dir = e_get_user_config_dir ();
	filename = g_build_filename (user_config_dir, ".running", NULL);
	pid_file = g_file_new_for_path (filename);
	g_free (filename);

	if (!get_evolution_pid (pid_file)) {
		g_printerr ("Could not find Evolution's process ID\n");
		exit (EXIT_FAILURE);
	}

	/* Play it safe here and bail if something goes wrong.  We don't
	 * want to just skip to the killing if we can't ask Evolution to
	 * terminate gracefully.  Despite our name we actually want to
	 * -avoid- killing Evolution if at all possible. */
	if (!g_spawn_command_line_async ("evolution --quit", &error)) {
		g_printerr ("%s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	/* Now we set up a monitor on Evolution's .running file.
	 * If Evolution is still responsive it will delete this
	 * file just before terminating and we'll be notified. */
	monitor = g_file_monitor_file (pid_file, 0, NULL, &error);
	if (error != NULL) {
		g_printerr ("%s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	g_signal_connect (
		monitor, "changed",
		G_CALLBACK (file_monitor_changed_cb), NULL);

	g_timeout_add_seconds (
		EVOLUTION_SHUTDOWN_TIMEOUT, (GSourceFunc)
		evolution_not_responding_cb, NULL);

	/* Start the clock. */

	main_loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_object_unref (monitor);

	return EXIT_SUCCESS;
}

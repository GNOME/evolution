/*
 * Evolution calendar - Alarm notification service main file
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
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include "alarm-notify.h"
#include "config-data.h"

#ifdef G_OS_WIN32
#include <libedataserver/libedataserver.h>
#endif

#include "e-util/e-util-private.h"

#ifdef G_OS_UNIX
#include <glib-unix.h>

static gboolean
handle_term_signal (gpointer data)
{
	g_application_quit (data);

	return FALSE;
}
#endif

gint
main (gint argc,
      gchar **argv)
{
	AlarmNotify *alarm_notify_service;
	gint exit_status;
	GError *error = NULL;

#ifdef G_OS_WIN32
	e_util_win32_initialize ();
#endif

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	alarm_notify_service = alarm_notify_new (NULL, &error);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	g_application_register (G_APPLICATION (alarm_notify_service), NULL, &error);

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_object_unref (alarm_notify_service);
		exit (EXIT_FAILURE);
	}

	if (g_application_get_is_remote (G_APPLICATION (alarm_notify_service))) {
		g_object_unref (alarm_notify_service);
		return 0;
	}

#ifdef G_OS_UNIX
	g_unix_signal_add_full (
		G_PRIORITY_DEFAULT, SIGTERM,
		handle_term_signal, alarm_notify_service, NULL);
#endif

	exit_status = g_application_run (
		G_APPLICATION (alarm_notify_service), argc, argv);

	g_object_unref (alarm_notify_service);
	config_data_cleanup ();
	e_util_cleanup_settings ();

	return exit_status;
}

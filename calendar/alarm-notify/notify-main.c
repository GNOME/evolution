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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <glib/gi18n.h>

#include "alarm-notify.h"
#include "config-data.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <conio.h>
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002
#endif
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
	gchar *path;

	/* Reduce risks */
	{
		typedef BOOL (WINAPI *t_SetDllDirectoryA) (LPCSTR lpPathName);
		t_SetDllDirectoryA p_SetDllDirectoryA;

		p_SetDllDirectoryA = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetDllDirectoryA");
		if (p_SetDllDirectoryA)
			(*p_SetDllDirectoryA) ("");
	}
#ifndef _WIN64
	{
		typedef BOOL (WINAPI *t_SetProcessDEPPolicy) (DWORD dwFlags);
		t_SetProcessDEPPolicy p_SetProcessDEPPolicy;

		p_SetProcessDEPPolicy = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetProcessDEPPolicy");
		if (p_SetProcessDEPPolicy)
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif
#endif

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	e_gdbus_templates_init_main_thread ();

#ifdef G_OS_WIN32
	path = g_build_path (";", _e_get_bindir (), g_getenv ("PATH"), NULL);

	if (!g_setenv ("PATH", path, TRUE))
		g_warning ("Could not set PATH for Evolution Alarm Notifier");
#endif

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

	return exit_status;
}

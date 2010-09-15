/*
 * Evolution calendar - Alarm notification service main file
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <unique/unique.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>

#include "alarm.h"
#include "alarm-queue.h"
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

gint
main (gint argc, gchar **argv)
{
	GtkIconTheme *icon_theme;
	AlarmNotify *alarm_notify_service;
	UniqueApp *app;
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
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE|PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif
#endif

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);

#ifdef G_OS_WIN32
	path = g_build_path (";", _e_get_bindir (), g_getenv ("PATH"), NULL);

	if (!g_setenv ("PATH", path, TRUE))
		g_warning ("Could not set PATH for Evolution Alarm Notifier");
#endif

	gtk_init (&argc, &argv);

	app = unique_app_new ("org.gnome.EvolutionAlarmNotify", NULL);

	if (unique_app_is_running (app))
		goto exit;

	alarm_notify_service = alarm_notify_new ();

	/* FIXME Ideally we should not use camel libraries in calendar,
	 *       though it is the case currently for attachments. Remove
	 *       this once that is fixed. */

	/* Initialize Camel's type system. */
	camel_object_get_type();

	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme, EVOLUTION_ICONDIR);

	gtk_main ();

	if (alarm_notify_service != NULL)
		g_object_unref (alarm_notify_service);

	alarm_done ();

	e_passwords_shutdown ();

exit:
	g_object_unref (app);

	return 0;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Intel Corporation (www.intel.com)
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserverui/e-passwords.h>
#include <mail/mail-mt.h>
#include "settings/mail-capplet-shell.h"
#include <gconf/gconf-client.h>
#include <libedataserver/e-categories.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef DATADIR
#undef DATADIR
#endif
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <conio.h>
#include <io.h>
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002
#endif
#endif

gboolean windowed = FALSE;
gboolean anjal_icon_decoration = FALSE;
gboolean default_app =  FALSE;
guint32 socket_id = 0;
GtkWidget *main_window;
static gchar **remaining_args;
extern gchar *shell_moduledir;

#define GCONF_KEY_MAILTO_ENABLED "/desktop/gnome/url-handlers/mailto/enabled"
#define GCONF_KEY_MAILTO_COMMAND "/desktop/gnome/url-handlers/mailto/command"
#define ANJAL_MAILTO_COMMAND "anjal %s"

static void
categories_icon_theme_hack (void)
{
	GtkIconTheme *icon_theme;
	const gchar *category_name;
	const gchar *filename;
	gchar *dirname;

	/* XXX Allow the category icons to be referenced as named
	 *     icons, since GtkAction does not support GdkPixbufs. */

	/* Get the icon file for some default category.  Doesn't matter
	 * which, so long as it has an icon.  We're just interested in
	 * the directory components. */
	category_name = _("Birthday");
	filename = e_categories_get_icon_file_for (category_name);
	g_return_if_fail (filename != NULL && *filename != '\0');

	/* Extract the directory components. */
	dirname = g_path_get_dirname (filename);

	/* Add it to the icon theme's search path.  This relies on
	 * GtkIconTheme's legacy feature of using image files found
	 * directly in the search path. */
	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme, dirname);

	g_free (dirname);
}

static void
check_and_set_default_mail (void)
{
	GConfClient *client = gconf_client_get_default ();
	gchar *mailer;

	mailer  = gconf_client_get_string (client, GCONF_KEY_MAILTO_COMMAND, NULL);
	if (mailer && *mailer && (strcmp (mailer, ANJAL_MAILTO_COMMAND) == 0)) {
		g_object_unref (client);
		return; /* Anjal is the default mailer */
	}

	gconf_client_set_bool (client, GCONF_KEY_MAILTO_ENABLED, TRUE, NULL);
	gconf_client_set_string (client, GCONF_KEY_MAILTO_COMMAND, ANJAL_MAILTO_COMMAND, NULL);
	g_object_unref (client);
}

static gboolean
idle_cb (MailCappletShell *mshell G_GNUC_UNUSED)
{

	if (default_app) {
		check_and_set_default_mail ();
	}

	return FALSE;
}

static void
create_default_shell (void)
{
	main_window = mail_capplet_shell_new (socket_id, FALSE, TRUE);
	if (!socket_id)
		gtk_widget_show (main_window);
	g_idle_add ((GSourceFunc) idle_cb, remaining_args);
}

gint
main (gint argc, gchar *argv[])
{
	GError *error = NULL;
	GConfClient *client;


#ifdef G_OS_WIN32
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

	if (fileno (stdout) != -1 && _get_osfhandle (fileno (stdout)) != -1) {
		/* stdout is fine, presumably redirected to a file or pipe */
	} else {
		typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

		AttachConsole_t p_AttachConsole =
			(AttachConsole_t) GetProcAddress (GetModuleHandle ("kernel32.dll"), "AttachConsole");

		if (p_AttachConsole && p_AttachConsole (ATTACH_PARENT_PROCESS)) {
			freopen ("CONOUT$", "w", stdout);
			dup2 (fileno (stdout), 1);
			freopen ("CONOUT$", "w", stderr);
			dup2 (fileno (stderr), 2);
		}
	}
#endif

	static GOptionEntry entries[] = {
		{ "windowed", 'w', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &windowed,N_("Run Anjal in a window"), NULL },
		{ "default-mailer", 'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &default_app,N_("Make Anjal the default email client"), NULL },
		{ "socket",
		  's',
		  G_OPTION_FLAG_IN_MAIN,
		  G_OPTION_ARG_INT,
		  &socket_id,
		  /* TRANSLATORS: don't translate the terms in brackets */
		  N_("ID of the socket to embed in"),
		  N_("socket") },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining_args, NULL, NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	setlocale (LC_ALL, NULL);

	if (!gtk_init_with_args (&argc, &argv, _("Anjal email client"), entries, NULL, &error)) {
		g_error ("Unable to start Anjal: %s\n", error->message);
		g_error_free (error);
	}

	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	client = gconf_client_get_default ();

	e_passwords_init ();
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default(), PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "anjal" G_DIR_SEPARATOR_S "icons");
	categories_icon_theme_hack ();

	gconf_client_set_bool (client, "/apps/evolution/mail/display/enable_vfolders", FALSE, NULL);
	g_object_unref (client);

	create_default_shell ();

	if (windowed)
		anjal_icon_decoration = TRUE;

	gtk_main ();

	return 0;
}

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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#if HAVE_CLUTTER
#include <clutter-gtk/clutter-gtk.h>
#include <mx/mx.h>
#endif

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

#include <gconf/gconf-client.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libedataserver/e-categories.h>
#include <libedataserverui/e-passwords.h>

#include "e-shell.h"
#include "e-shell-migrate.h"
#include "e-shell-meego.h"
#include "es-event.h"

#include "e-util/e-dialog-utils.h"
#include "e-util/e-import.h"
#include "e-util/e-plugin.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/e-profile-event.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#ifdef G_OS_WIN32
#include "e-util/e-win32-defaults.h"
#endif

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ICAL_UNKNOWN_TOKEN_HANDLING
#include <libical/ical.h>
#endif

#define SKIP_WARNING_DIALOG_KEY \
	"/apps/evolution/shell/skip_warning_dialog"

/* STABLE_VERSION is only defined for development versions. */
#ifdef STABLE_VERSION
#define DEVELOPMENT 1
#endif

/* Command-line options.  */
#ifdef G_OS_WIN32
static gboolean register_handlers = FALSE;
static gboolean reinstall = FALSE;
static gboolean show_icons = FALSE;
static gboolean hide_icons = FALSE;
static gboolean unregister_handlers = FALSE;
#endif /* G_OS_WIN32 */
static gboolean express_mode = FALSE;
static gboolean start_online = FALSE;
static gboolean start_offline = FALSE;
static gboolean setup_only = FALSE;
static gboolean force_shutdown = FALSE;
static gboolean disable_eplugin = FALSE;
static gboolean disable_preview = FALSE;
static gboolean import_uris = FALSE;
static gboolean quit = FALSE;

static gchar *geometry = NULL;
static gchar *requested_view = NULL;
static gchar *evolution_debug_log = NULL;
static gchar **remaining_args;

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

#ifdef DEVELOPMENT

/* Warning dialog to scare people off a little bit.  */

static gboolean
show_development_warning (void)
{
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *warning_dialog;
	GtkWidget *checkbox;
	GtkWidget *alignment;
	gboolean skip;
	gchar *text;

	warning_dialog = gtk_dialog_new ();
	gtk_window_set_title (
		GTK_WINDOW (warning_dialog), "Evolution " VERSION);
	gtk_window_set_modal (
		GTK_WINDOW (warning_dialog), TRUE);
	gtk_dialog_add_button (
		GTK_DIALOG (warning_dialog),
		GTK_STOCK_OK, GTK_RESPONSE_OK);
#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (warning_dialog, "has-separator", FALSE, NULL);
#endif

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (warning_dialog));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (warning_dialog));

	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

	text = g_strdup_printf (
		/* xgettext:no-c-format */
		/* Preview/Alpha/Beta version warning message */
		_("Hi.  Thanks for taking the time to download this preview release\n"
		  "of the Evolution groupware suite.\n"
		  "\n"
		  "This version of Evolution is not yet complete. It is getting close,\n"
		  "but some features are either unfinished or do not work properly.\n"
		  "\n"
		  "If you want a stable version of Evolution, we urge you to uninstall\n"
		  "this version, and install version %s instead.\n"
		  "\n"
		  "If you find bugs, please report them to us at bugzilla.gnome.org.\n"
                  "This product comes with no warranty and is not intended for\n"
		  "individuals prone to violent fits of anger.\n"
                  "\n"
		  "We hope that you enjoy the results of our hard work, and we\n"
		  "eagerly await your contributions!\n"),
		STABLE_VERSION);
	label = gtk_label_new (text);
	g_free (text);

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	label = gtk_label_new (_("Thanks\n"
				 "The Evolution Team\n"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 1, .5);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	checkbox = gtk_check_button_new_with_label (_("Do not tell me again"));

	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);

	gtk_container_add (GTK_CONTAINER (alignment), checkbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);

	gtk_widget_show_all (warning_dialog);

	gtk_dialog_run (GTK_DIALOG (warning_dialog));

	skip = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

	gtk_widget_destroy (warning_dialog);

	return skip;
}

#endif /* DEVELOPMENT */

/* This is for doing stuff that requires the GTK+ loop to be running already.  */

static gboolean
idle_cb (gchar **uris)
{
	EShell *shell;

	shell = e_shell_get_default ();

	/* These calls do the right thing when another Evolution
	 * process is running. */
	if (uris != NULL && *uris != NULL) {
		if (e_shell_handle_uris (shell, uris, import_uris) == 0)
			gtk_main_quit ();
	} else {
		if (express_mode && requested_view == NULL)
			requested_view = (gchar *) "mail";
		e_shell_create_shell_window (shell, requested_view);
	}

	/* If another Evolution process is running, we're done. */
	if (unique_app_is_running (UNIQUE_APP (shell)))
		gtk_main_quit ();

	return FALSE;
}

#ifndef G_OS_WIN32

/* SIGSEGV handling.

   The GNOME SEGV handler will lose if it's not run from the main Gtk
   thread. So if we have to redirect the signal if the crash happens in another
   thread.  */

static void (*gnome_segv_handler) (gint);
static GStaticMutex segv_mutex = G_STATIC_MUTEX_INIT;
static GThread *main_thread = NULL;

static void
segv_redirect (gint sig)
{
	gnome_segv_handler (sig);

	if (g_thread_self () != main_thread) {
		/* We can't return from the signal handler or the thread may
		   SEGV again. But we can't g_thread_exit, because then the
		   thread may get cleaned up before bug-buddy can get a stack
		   trace. So we block by trying to lock a mutex we know is
		   already locked.  */
		g_static_mutex_lock (&segv_mutex);
	}
}

static void
setup_segv_redirect (void)
{
	struct sigaction sa, osa;

	sigaction (SIGSEGV, NULL, &osa);
	if (osa.sa_handler == SIG_DFL)
		return;

	main_thread = g_thread_self ();

	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);
	sa.sa_handler = segv_redirect;
	sigaction (SIGSEGV, &sa, NULL);
	sigaction (SIGBUS, &sa, NULL);
	sigaction (SIGFPE, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction (SIGXFSZ, &sa, NULL);
	gnome_segv_handler = osa.sa_handler;
	g_static_mutex_lock (&segv_mutex);
}

#else
#define setup_segv_redirect() (void)0
#endif

static GOptionEntry entries[] = {
#ifdef G_OS_WIN32
	{ "register-handlers", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &register_handlers,
	  NULL, NULL },
	{ "reinstall", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reinstall,
	  NULL, NULL },
	{ "show-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_icons,
	  NULL, NULL },
	{ "hide-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &hide_icons,
	  NULL, NULL },
	{ "unregister-handlers", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &unregister_handlers,
	  NULL, NULL },
#endif /* G_OS_WIN32 */
	{ "component", 'c', 0, G_OPTION_ARG_STRING, &requested_view,
	  N_("Start Evolution activating the specified component"), NULL },
	{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
	  N_("Apply the given geometry to the main window"), "GEOMETRY" },
	{ "offline", '\0', 0, G_OPTION_ARG_NONE, &start_offline,
	  N_("Start in offline mode"), NULL },
	{ "online", '\0', 0, G_OPTION_ARG_NONE, &start_online,
	  N_("Start in online mode"), NULL },
	{ "express", '\0', 0, G_OPTION_ARG_NONE, &express_mode,
	  N_("Start in \"express\" mode"), NULL },
#ifdef KILL_PROCESS_CMD
	{ "force-shutdown", '\0', 0, G_OPTION_ARG_NONE, &force_shutdown,
	  N_("Forcibly shut down Evolution"), NULL },
#endif
	{ "debug", '\0', 0, G_OPTION_ARG_STRING, &evolution_debug_log,
	  N_("Send the debugging output of all components to a file."), "FILE" },
	{ "disable-eplugin", '\0', 0, G_OPTION_ARG_NONE, &disable_eplugin,
	  N_("Disable loading of any plugins."), NULL },
	{ "disable-preview", '\0', 0, G_OPTION_ARG_NONE, &disable_preview,
	  N_("Disable preview pane of Mail, Contacts and Tasks."), NULL },
	{ "setup-only", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
	  &setup_only, NULL, NULL },
	{ "import", 'i', 0, G_OPTION_ARG_NONE, &import_uris,
	  N_("Import URIs or file names given as rest of arguments."), NULL },
	{ "quit", 'q', 0, G_OPTION_ARG_NONE, &quit,
	  N_("Request a running Evolution process to quit"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining_args, NULL, NULL },
	{ NULL }
};

static void G_GNUC_NORETURN
shell_force_shutdown (void)
{
	gchar *filename;

	filename = g_build_filename (EVOLUTION_TOOLSDIR, "killev", NULL);
	execl (filename, "killev", NULL);

	g_assert_not_reached ();
}

static EShell *
create_default_shell (void)
{
	EShell *shell;
	GConfClient *client;
	gboolean online = TRUE;
	const gchar *key;
	GError *error = NULL;
	gboolean is_meego = FALSE, small_screen = FALSE;

	client = gconf_client_get_default ();

	/* Requesting online or offline mode from the command-line
	 * should be persistent, just like selecting it in the UI. */

	key = "/apps/evolution/shell/start_offline";

	if (start_online) {
		online = TRUE;
		gconf_client_set_bool (client, key, FALSE, &error);
	} else if (start_offline) {
		online = FALSE;
		gconf_client_set_bool (client, key, TRUE, &error);
	} else {
		gboolean value;

		value = gconf_client_get_bool (client, key, &error);
		if (error == NULL)
			online = !value;
	}

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	/* Determine whether to run Evolution in "express" mode. */

	key = "/apps/evolution/shell/express_mode";

	if (!express_mode)
		express_mode = gconf_client_get_bool (client, key, &error);

	if (express_mode)
		e_shell_detect_meego (&is_meego, &small_screen);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	shell = g_object_new (
		E_TYPE_SHELL,
		"name", "org.gnome.Evolution",
		"geometry", geometry,
		"module-directory", EVOLUTION_MODULEDIR,
		"meego-mode", is_meego,
		"express-mode", express_mode,
		"small-screen-mode", small_screen,
		"online", online,
		NULL);

	g_object_unref (client);

	return shell;
}

gint
main (gint argc, gchar **argv)
{
	EShell *shell;
	GConfClient *client;
#ifdef DEVELOPMENT
	gboolean skip_warning_dialog;
#endif
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
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE|PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif

	if (fileno (stdout) != -1 && _get_osfhandle (fileno (stdout)) != -1) {
		/* stdout is fine, presumably redirected to a file or pipe */
	} else {
		typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

		AttachConsole_t p_AttachConsole =
			(AttachConsole_t) GetProcAddress (
			GetModuleHandle ("kernel32.dll"), "AttachConsole");

		if (p_AttachConsole && p_AttachConsole (ATTACH_PARENT_PROCESS)) {
			freopen ("CONOUT$", "w", stdout);
			dup2 (fileno (stdout), 1);
			freopen ("CONOUT$", "w", stderr);
			dup2 (fileno (stderr), 2);
		}
	}
#endif

	/* Make ElectricFence work.  */
	free (malloc (10));

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#if HAVE_CLUTTER
	gtk_clutter_init_with_args (
		&argc, &argv,
		_("- The Evolution PIM and Email Client"),
		entries, (gchar *) GETTEXT_PACKAGE, &error);
	mx_style_load_from_file (
		mx_style_get_default (),
		EVOLUTION_MX_THEMEDIR "/default.css", NULL);
#else
	gtk_init_with_args (
		&argc, &argv,
		_("- The Evolution PIM and Email Client"),
		entries, (gchar *) GETTEXT_PACKAGE, &error);
#endif
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (1);
	}

	g_type_init ();
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	#ifdef HAVE_ICAL_UNKNOWN_TOKEN_HANDLING
	ical_set_unknown_token_handling_setting (ICAL_DISCARD_TOKEN);
	#endif

#ifdef G_OS_WIN32
	path = g_build_path (";", _e_get_bindir (), g_getenv ("PATH"), NULL);

	if (!g_setenv ("PATH", path, TRUE))
		g_warning ("Could not set PATH for Evolution and its child processes");

	g_free (path);

	if (register_handlers || reinstall || show_icons) {
		_e_win32_register_mailer ();
		_e_win32_register_addressbook ();
	}

	if (register_handlers)
		exit (0);

	if (reinstall) {
		_e_win32_set_default_mailer ();
		exit (0);
	}

	if (show_icons) {
		_e_win32_set_default_mailer ();
		exit (0);
	}

	if (hide_icons) {
		_e_win32_unset_default_mailer ();
		exit (0);
	}

	if (unregister_handlers) {
		_e_win32_unregister_mailer ();
		_e_win32_unregister_addressbook ();
		exit (0);
	}

	if (strcmp (gettext (""), "") == 0) {
		/* No message catalog installed for the current locale
		 * language, so don't bother with the localisations
		 * provided by other things then either. Reset thread
		 * locale to "en-US" and C library locale to "C". */
		SetThreadLocale (MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US),
					   SORT_DEFAULT));
		setlocale (LC_ALL, "C");
	}
#endif
	if (start_online && start_offline) {
		g_printerr (
			_("%s: --online and --offline cannot be used "
			  "together.\n  Use %s --help for more information.\n"),
			 argv[0], argv[0]);
		exit (1);
	}

	if (force_shutdown)
		shell_force_shutdown ();

	client = gconf_client_get_default ();

	if (disable_preview) {
		const gchar *key;

		key = "/apps/evolution/mail/display/safe_list";
		gconf_client_set_bool (client, key, TRUE, NULL);

		key = "/apps/evolution/addressbook/display/show_preview";
		gconf_client_set_bool (client, key, FALSE, NULL);

		key = "/apps/evolution/calendar/display/show_memo_preview";
		gconf_client_set_bool (client, key, FALSE, NULL);

		key = "/apps/evolution/calendar/display/show_task_preview";
		gconf_client_set_bool (client, key, FALSE, NULL);
	}

	setup_segv_redirect ();

	if (evolution_debug_log) {
		gint fd;

		fd = g_open (evolution_debug_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd != -1) {
			dup2 (fd, STDOUT_FILENO);
			dup2 (fd, STDERR_FILENO);
			close (fd);
		} else
			g_warning ("Could not set up debugging output file.");
	}

	e_passwords_init ();

	gtk_window_set_default_icon_name ("evolution");

	if (setup_only)
		exit (0);

	categories_icon_theme_hack ();
	gtk_accel_map_load (e_get_accels_filename ());

#ifdef DEVELOPMENT
	skip_warning_dialog = gconf_client_get_bool (
		client, SKIP_WARNING_DIALOG_KEY, NULL);

	if (!skip_warning_dialog && !getenv ("EVOLVE_ME_HARDER"))
		gconf_client_set_bool (
			client, SKIP_WARNING_DIALOG_KEY,
			show_development_warning (), NULL);
#endif

	g_object_unref (client);

	shell = create_default_shell ();

	if (quit) {
		e_shell_quit (shell, E_SHELL_QUIT_OPTION);
		goto exit;
	}

	e_shell_load_modules (shell);

	if (!disable_eplugin) {
		/* Register built-in plugin hook types. */
		es_event_hook_get_type ();
		e_import_hook_get_type ();
#ifdef ENABLE_PROFILING
		e_profile_event_hook_get_type ();
#endif
		e_plugin_ui_hook_get_type ();

		/* All EPlugin and EPluginHook subclasses should be
		 * registered in GType now, so load plugins now. */
		e_plugin_load_plugins ();
	}

	if (requested_view)
		e_shell_set_startup_view (shell, requested_view);
	else if (express_mode)
		e_shell_set_startup_view (shell, "mail");

	/* Attempt migration -after- loading all modules and plugins,
	 * as both shell backends and certain plugins hook into this. */
	e_shell_migrate_attempt (shell);

	/* This is an alternative to the "upgrade.done" EEvent. */
	e_shell_event (shell, "ready-to-start", NULL);

	g_idle_add ((GSourceFunc) idle_cb, remaining_args);

	gtk_main ();

exit:
	/* Drop what should be the last reference to the shell.
	 * That will cause e_shell_get_default() to henceforth
	 * return NULL.  Use that to check for reference leaks. */
	g_object_unref (shell);

	if (e_shell_get_default () != NULL)
		g_warning ("Shell not finalized on exit");

	gtk_accel_map_save (e_get_accels_filename ());

	return 0;
}

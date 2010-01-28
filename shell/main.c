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
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef DATADIR
#undef DATADIR
#endif
#include <io.h>
#include <conio.h>
#define _WIN32_WINNT 0x0501
#include <windows.h>
#endif

#include "e-util/e-dialog-utils.h"
#include "e-util/e-bconf-map.h"

#include <e-util/e-icon-factory.h>
#include "e-shell-constants.h"
#include "e-util/e-profile-event.h"
#include "e-util/e-util.h"

#include "e-shell.h"
#include "es-menu.h"
#include "es-event.h"

#include "e-util/e-util-private.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <gconf/gconf-client.h>

#include <glib/gi18n.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <bonobo-activation/bonobo-activation.h>

#include <libedataserverui/e-passwords.h>

#include <glade/glade.h>

#include "e-config-upgrade.h"
#include "Evolution-DataServer.h"

#include <misc/e-cursors.h>
#include "e-util/e-error.h"
#include "e-util/e-import.h"

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pthread.h>

#include "e-util/e-plugin.h"
#include "e-util/e-plugin-ui.h"

#define SKIP_WARNING_DIALOG_KEY \
	"/apps/evolution/shell/skip_warning_dialog"

/* STABLE_VERSION is only defined for development versions. */
#ifdef STABLE_VERSION
#define DEVELOPMENT 1
#endif

static EShell *shell = NULL;

/* Command-line options.  */
static gboolean start_online = FALSE;
static gboolean start_offline = FALSE;
static gboolean setup_only = FALSE;
static gboolean killev = FALSE;
#ifdef DEVELOPMENT
static gboolean force_migrate = FALSE;
#endif
static gboolean disable_eplugin = FALSE;
static gboolean disable_preview = FALSE;
static gboolean idle_cb (gchar **uris);

static gchar *default_component_id = NULL;
static gchar *evolution_debug_log = NULL;
static gchar **remaining_args;

static void
no_windows_left_cb (EShell *shell, gpointer data)
{
	bonobo_object_unref (BONOBO_OBJECT (shell));
	bonobo_main_quit ();
}

static void
shell_weak_notify (gpointer data,
                   GObject *where_the_object_was)
{
	bonobo_main_quit ();
}

#ifdef KILL_PROCESS_CMD

static void
kill_dataserver (void)
{
	g_message ("Killing old version of evolution-data-server...");

	system (KILL_PROCESS_CMD " -9 lt-evolution-data-server 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.0 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.2 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.4 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.6 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.8 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.10 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server-1.12 2> /dev/null");

	system (KILL_PROCESS_CMD " -9 lt-evolution-alarm-notify 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-alarm-notify 2> /dev/null");
}

static void
kill_old_dataserver (void)
{
	GNOME_Evolution_DataServer_InterfaceCheck iface;
	CORBA_Environment ev;
	CORBA_char *version;

	CORBA_exception_init (&ev);

	/* FIXME Should we really kill it off?  We also shouldn't hard code the version */
	iface = bonobo_activation_activate_from_id (
		(Bonobo_ActivationID) "OAFIID:GNOME_Evolution_DataServer_InterfaceCheck", 0, NULL, &ev);
	if (BONOBO_EX (&ev) || iface == CORBA_OBJECT_NIL) {
		kill_dataserver ();
		CORBA_exception_free (&ev);
		return;
	}

	version = GNOME_Evolution_DataServer_InterfaceCheck__get_interfaceVersion (iface, &ev);
	if (BONOBO_EX (&ev)) {
		kill_dataserver ();
		CORBA_Object_release (iface, &ev);
		CORBA_exception_free (&ev);
		return;
	}

	if (strcmp (version, DATASERVER_VERSION) != 0) {
		CORBA_free (version);
		kill_dataserver ();
		CORBA_Object_release (iface, &ev);
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_free (version);
	CORBA_Object_release (iface, &ev);
	CORBA_exception_free (&ev);
}
#endif

#ifdef DEVELOPMENT

/* Warning dialog to scare people off a little bit.  */

static gboolean
show_development_warning(void)
{
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *warning_dialog;
	GtkWidget *checkbox;
	GtkWidget *alignment;
	gboolean skip;
	gchar *text;

	warning_dialog = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (warning_dialog), "Evolution " VERSION);
	gtk_window_set_modal (GTK_WINDOW (warning_dialog), TRUE);
	gtk_dialog_add_button (GTK_DIALOG (warning_dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);

	gtk_dialog_set_has_separator (GTK_DIALOG (warning_dialog), FALSE);

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (warning_dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (warning_dialog)->action_area), 12);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox), vbox,
			    TRUE, TRUE, 0);

	text = g_strdup_printf(
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
	g_free(text);

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	label = gtk_label_new (_("Thanks\n"
				 "The Evolution Team\n"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, .5);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	checkbox = gtk_check_button_new_with_label (_("Do not tell me again"));

	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);

	gtk_container_add (GTK_CONTAINER (alignment), checkbox);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);

	gtk_widget_show_all (warning_dialog);

	gtk_dialog_run (GTK_DIALOG (warning_dialog));

	skip = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

	gtk_widget_destroy (warning_dialog);

	idle_cb (NULL);

	return skip;
}

static void
destroy_config (GConfClient *client)
{
	/* Unset the source stuff */
	gconf_client_unset (client, "/apps/evolution/calendar/sources", NULL);
	gconf_client_unset (client, "/apps/evolution/tasks/sources", NULL);
	gconf_client_unset (client, "/apps/evolution/addressbook/sources", NULL);

	/* Reset the version */
	gconf_client_set_string (client, "/apps/evolution/version", "1.4.0", NULL);

	/* Clear the dir */
	system ("rm -Rf ~/.evolution");
}

#endif /* DEVELOPMENT */

static void
open_uris (GNOME_Evolution_Shell corba_shell, gchar **uris)
{
	CORBA_Environment ev;
	guint n_uris, ii;

	g_return_if_fail (uris != NULL);
	n_uris = g_strv_length (uris);

	CORBA_exception_init (&ev);

	for (ii = 0; ii < n_uris; ii++) {
		GNOME_Evolution_Shell_handleURI (corba_shell, uris[ii], &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Invalid URI: %s", uris[ii]);
			CORBA_exception_free (&ev);
		}
	}

	CORBA_exception_free (&ev);
}

/* This is for doing stuff that requires the GTK+ loop to be running already.  */

static gboolean
idle_cb (gchar **uris)
{
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;
	EShellConstructResult result;
	EShellStartupLineMode startup_line_mode;

	g_return_val_if_fail (uris == NULL || g_strv_length (uris) > 0, FALSE);

#ifdef KILL_PROCESS_CMD
	kill_old_dataserver ();
#endif

	CORBA_exception_init (&ev);

	if (! start_online && ! start_offline)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_CONFIG;
	else if (start_online)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_ONLINE;
	else
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_OFFLINE;

	shell = e_shell_new (startup_line_mode, &result);

	switch (result) {
	case E_SHELL_CONSTRUCT_RESULT_OK:
		e_shell_set_crash_recovery (shell, e_file_lock_exists ());
		g_signal_connect (shell, "no_windows_left", G_CALLBACK (no_windows_left_cb), NULL);
		g_object_weak_ref (G_OBJECT (shell), shell_weak_notify, NULL);
		corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell));
		corba_shell = CORBA_Object_duplicate (corba_shell, &ev);
		break;

	case E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER:
		corba_shell = bonobo_activation_activate_from_id (
			(Bonobo_ActivationID) E_SHELL_OAFIID, 0, NULL, &ev);
		if (ev._major != CORBA_NO_EXCEPTION || corba_shell == CORBA_OBJECT_NIL) {
			e_error_run(NULL, "shell:noshell", NULL);
			CORBA_exception_free (&ev);
			bonobo_main_quit ();
			return FALSE;
		}
		break;

	default:
		e_error_run(NULL, "shell:noshell-reason",
			    e_shell_construct_result_to_string(result), NULL);
		CORBA_exception_free (&ev);
		bonobo_main_quit ();
		return FALSE;

	}

	if (shell != NULL) {
		if (uris != NULL)
			open_uris (corba_shell, uris);
		else {
			e_file_lock_create ();
			e_shell_create_window (shell, default_component_id, NULL);
		}
	} else {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		if (uris != NULL)
			open_uris (corba_shell, uris);
		else
			if (default_component_id == NULL)
				GNOME_Evolution_Shell_createNewWindow (corba_shell, "", &ev);
			else
				GNOME_Evolution_Shell_createNewWindow (corba_shell, default_component_id, &ev);

		CORBA_exception_free (&ev);
	}

	CORBA_Object_release (corba_shell, &ev);

	CORBA_exception_free (&ev);

	if (shell == NULL) {
		/*there is another instance but because we don't open any windows
		we must notify the startup was complete manually*/
		gdk_notify_startup_complete ();
		bonobo_main_quit ();
	}

	/* This must be done after Bonobo has created all the components. For
	 * example the mail component makes the global variable `session` which
	 * is being used by several EPlugins */

	if (!disable_eplugin) {
		e_plugin_load_plugins_with_missing_symbols ();
	}

	return FALSE;
}

#ifndef G_OS_WIN32

/* SIGSEGV handling.

   The GNOME SEGV handler will lose if it's not run from the main Gtk
   thread. So if we have to redirect the signal if the crash happens in another
   thread.  */

static void (*gnome_segv_handler) (gint);
static GStaticMutex segv_mutex = G_STATIC_MUTEX_INIT;
static pthread_t main_thread;

static void
segv_redirect (gint sig)
{
	if (pthread_self () == main_thread)
		gnome_segv_handler (sig);
	else {
		pthread_kill (main_thread, sig);

		/* We can't return from the signal handler or the thread may
		   SEGV again. But we can't pthread_exit, because then the
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

	main_thread = pthread_self ();

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

static gint
gnome_master_client_save_yourself_cb (GnomeClient *client, GnomeSaveStyle save_style, gint shutdown, GnomeInteractStyle interact_style, gint fast, gpointer user_data)
{
	return !shell || e_shell_can_quit (shell);
}

static void
gnome_master_client_die_cb (GnomeClient *client)
{
	e_shell_do_quit (shell);
}

static const GOptionEntry options[] = {
	{ "component", 'c', 0, G_OPTION_ARG_STRING, &default_component_id,
	  N_("Start Evolution activating the specified component"), NULL },
	{ "offline", '\0', 0, G_OPTION_ARG_NONE, &start_offline,
	  N_("Start in offline mode"), NULL },
	{ "online", '\0', 0, G_OPTION_ARG_NONE, &start_online,
	  N_("Start in online mode"), NULL },
#ifdef KILL_PROCESS_CMD
	{ "force-shutdown", '\0', 0, G_OPTION_ARG_NONE, &killev,
	  N_("Forcibly shut down all Evolution components"), NULL },
#endif
#ifdef DEVELOPMENT
	{ "force-migrate", '\0', 0, G_OPTION_ARG_NONE, &force_migrate,
	  N_("Forcibly re-migrate from Evolution 1.4"), NULL },
#endif
	{ "debug", '\0', 0, G_OPTION_ARG_STRING, &evolution_debug_log,
	  N_("Send the debugging output of all components to a file."), NULL },
	{ "disable-eplugin", '\0', 0, G_OPTION_ARG_NONE, &disable_eplugin,
	  N_("Disable loading of any plugins."), NULL },
	{ "disable-preview", '\0', 0, G_OPTION_ARG_NONE, &disable_preview,
	  N_("Disable preview pane of Mail, Contacts and Tasks."), NULL },
	{ "setup-only", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
	  &setup_only, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining_args, NULL, NULL },
	{ NULL }
};

#ifdef G_OS_WIN32
static void
set_paths (void)
{
	/* Set PATH to include the Evolution executable's folder
	 * and the lib/evolution/$(BASE_VERSION)/components folder.
	 */
	wchar_t exe_filename[MAX_PATH];
	wchar_t *p;
	gchar *exe_folder_utf8;
	gchar *components_folder_utf8;
	gchar *top_folder_utf8;
	gchar *path;

	GetModuleFileNameW (NULL, exe_filename, G_N_ELEMENTS (exe_filename));

	p = wcsrchr (exe_filename, L'\\');
	g_assert (p != NULL);

	*p = L'\0';
	exe_folder_utf8 = g_utf16_to_utf8 (exe_filename, -1, NULL, NULL, NULL);

	p = wcsrchr (exe_filename, L'\\');
	g_assert (p != NULL);

	*p = L'\0';
	top_folder_utf8 = g_utf16_to_utf8 (exe_filename, -1, NULL, NULL, NULL);
	components_folder_utf8 =
		g_strconcat (top_folder_utf8,
			     "/lib/evolution/" BASE_VERSION "/components",
			     NULL);

	path = g_build_path (";",
			     exe_folder_utf8,
			     components_folder_utf8,
			     g_getenv ("PATH"),
			     NULL);
	if (!g_setenv ("PATH", path, TRUE))
		g_warning ("Could not set PATH for Evolution and its child processes");

	g_free (path);
	g_free (exe_folder_utf8);
	g_free (components_folder_utf8);

	/* Set BONOBO_ACTIVATION_PATH */
	if (g_getenv ("BONOBO_ACTIVATION_PATH" ) == NULL) {
		path = g_build_filename (top_folder_utf8,
					 "lib/bonobo/servers",
					 NULL);
		if (!g_setenv ("BONOBO_ACTIVATION_PATH", path, TRUE))
			g_warning ("Could not set BONOBO_ACTIVATION_PATH");
		g_free (path);
	}
	g_free (top_folder_utf8);
}
#endif

gint
main (gint argc, gchar **argv)
{
#ifdef G_OS_WIN32
    if (fileno (stdout) != -1 &&
	  _get_osfhandle (fileno (stdout)) != -1)
	{
	  /* stdout is fine, presumably redirected to a file or pipe */
	}
    else
    {
	  typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

	  AttachConsole_t p_AttachConsole =
	    (AttachConsole_t) GetProcAddress (GetModuleHandle ("kernel32.dll"), "AttachConsole");

	  if (p_AttachConsole != NULL && p_AttachConsole (ATTACH_PARENT_PROCESS))
      {
	      freopen ("CONOUT$", "w", stdout);
	      dup2 (fileno (stdout), 1);
	      freopen ("CONOUT$", "w", stderr);
	      dup2 (fileno (stderr), 2);

      }
	}

	extern void link_shutdown (void);
#endif

	GConfClient *client;
#ifdef DEVELOPMENT
	gboolean skip_warning_dialog;
#endif
	GnomeProgram *program;
	GnomeClient *master_client;
	GOptionContext *context;
	gchar *filename;

	/* Make ElectricFence work.  */
	free (malloc (10));

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- The Evolution PIM and Email Client"));

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

#ifdef G_OS_WIN32
	set_paths ();
#endif

	program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Evolution"),
				      NULL);

#ifdef G_OS_WIN32
	if (strcmp (gettext (""), "") == 0) {
		/* No message catalog installed for the current locale language,
		 * so don't bother with the localisations provided by other things then
		 * either. Reset thread locale to "en-US" and C library locale to "C".
		 */
		SetThreadLocale (MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US),
					   SORT_DEFAULT));
		setlocale (LC_ALL, "C");
	}
#endif
	if (start_online && start_offline) {
		fprintf (stderr, _("%s: --online and --offline cannot be used together.\n  Use %s --help for more information.\n"),
			 argv[0], argv[0]);
		exit (1);
	}

	if (killev) {
		filename = g_build_filename (EVOLUTION_TOOLSDIR,
					     "killev",
					     NULL);
		execl (filename, "killev", NULL);
		/* Not reached */
		exit (0);
	}

	client = gconf_client_get_default ();

#ifdef DEVELOPMENT

	if (force_migrate) {
		destroy_config (client);
	}
#endif
	if (disable_preview) {
		gconf_client_set_bool (client, "/apps/evolution/mail/display/show_preview", FALSE, NULL);
		gconf_client_set_bool (client, "/apps/evolution/mail/display/safe_list", TRUE, NULL);
		gconf_client_set_bool (client, "/apps/evolution/addressbook/display/show_preview", FALSE, NULL);
		gconf_client_set_bool (client, "/apps/evolution/calendar/display/show_task_preview", FALSE, NULL);
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

	master_client = gnome_master_client ();

	g_signal_connect (G_OBJECT (master_client), "save_yourself", G_CALLBACK (gnome_master_client_save_yourself_cb), NULL);
	g_signal_connect (G_OBJECT (master_client), "die", G_CALLBACK (gnome_master_client_die_cb), NULL);

	glade_init ();
	e_cursors_init ();
	e_icon_factory_init ();
	e_passwords_init();

	gtk_window_set_default_icon_name ("evolution");

	if (setup_only)
		exit (0);

	gnome_sound_init ("localhost");
	gtk_accel_map_load (e_get_accels_filename ());

	if (!disable_eplugin) {
		e_plugin_register_type(e_plugin_lib_get_type());
		e_plugin_hook_register_type(es_menu_hook_get_type());
		e_plugin_hook_register_type(es_event_hook_get_type());
#ifdef ENABLE_PROFILING
		e_plugin_hook_register_type(e_profile_event_hook_get_type());
#endif
		e_plugin_hook_register_type(e_plugin_type_hook_get_type());
		e_plugin_hook_register_type(e_import_hook_get_type());
		e_plugin_hook_register_type(E_TYPE_PLUGIN_UI_HOOK);
		e_plugin_load_plugins ();
	}

#ifdef DEVELOPMENT
	skip_warning_dialog = gconf_client_get_bool (
		client, SKIP_WARNING_DIALOG_KEY, NULL);

	if (!skip_warning_dialog && !getenv ("EVOLVE_ME_HARDER"))
		gconf_client_set_bool (
			client, SKIP_WARNING_DIALOG_KEY,
			show_development_warning (), NULL);
	else
		g_idle_add ((GSourceFunc) idle_cb, remaining_args);

#else
	g_idle_add ((GSourceFunc) idle_cb, remaining_args);
#endif
	g_object_unref (client);

	bonobo_main ();

	gtk_accel_map_save (e_get_accels_filename ());

	e_icon_factory_shutdown ();
	g_object_unref (program);
	gnome_sound_shutdown ();
	e_cursors_shutdown ();
#ifdef G_OS_WIN32
	link_shutdown ();
#endif
	return 0;
}

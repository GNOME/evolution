/*
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#ifdef WITH_CONTACT_MAPS
#include <clutter-gtk/clutter-gtk.h>
#endif

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef DATADIR
#undef DATADIR
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include "e-util/e-util-private.h"
#include <libedataserver/libedataserver.h>

#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <webkit2/webkit2.h>

#include "e-shell.h"
#include "e-shell-migrate.h"

#ifdef G_OS_WIN32
#include "e-util/e-win32-defaults.h"
#endif

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "e-util/e-util.h"

#define APPLICATION_ID "org.gnome.Evolution"

/* STABLE_VERSION is only defined for development versions. */
#ifdef STABLE_VERSION
#define DEVELOPMENT 1
#endif

/* Set this to TRUE and rebuild to enable MeeGo's Express Mode. */
#define EXPRESS_MODE FALSE

/* Command-line options.  */
#ifdef G_OS_WIN32
static gboolean register_handlers = FALSE;
static gboolean reinstall = FALSE;
static gboolean show_icons = FALSE;
static gboolean hide_icons = FALSE;
static gboolean unregister_handlers = FALSE;
#endif /* G_OS_WIN32 */
static gboolean force_online = FALSE;
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
static gchar **remaining_args;

/* Forward declarations */
void e_convert_local_mail (EShell *shell);
void e_migrate_base_dirs (EShell *shell);

static void
categories_icon_theme_hack (void)
{
	GList *categories, *link;
	GtkIconTheme *icon_theme;
	GHashTable *dirnames;
	const gchar *category_name;
	gchar *filename;
	gchar *dirname;

	/* XXX Allow the category icons to be referenced as named
	 *     icons, since GtkAction does not support GdkPixbufs. */

	icon_theme = gtk_icon_theme_get_default ();
	dirnames = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* Get the icon file for some default category.  Doesn't matter
	 * which, so long as it has an icon.  We're just interested in
	 * the directory components. */
	categories = e_categories_dup_list ();

	for (link = categories; link; link = g_list_next (link)) {
		category_name = link->data;

		filename = e_categories_dup_icon_file_for (category_name);
		if (filename && *filename) {
			/* Extract the directory components. */
			dirname = g_path_get_dirname (filename);

			if (dirname && !g_hash_table_contains (dirnames, dirname)) {
				/* Add it to the icon theme's search path.  This relies on
				 * GtkIconTheme's legacy feature of using image files found
				 * directly in the search path. */
				gtk_icon_theme_append_search_path (icon_theme, dirname);
				g_hash_table_insert (dirnames, dirname, NULL);
			} else {
				g_free (dirname);
			}
		}

		g_free (filename);
	}

	g_list_free_full (categories, g_free);
	g_hash_table_destroy (dirnames);
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
		_("_OK"), GTK_RESPONSE_OK);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (warning_dialog));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (warning_dialog));

	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
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

	label = gtk_label_new (_("Thanks\nThe Evolution Team\n"));
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
idle_cb (const gchar * const *uris)
{
	EShell *shell;

	shell = e_shell_get_default ();

	/* These calls do the right thing when another Evolution
	 * process is running. */
	if (uris != NULL && *uris != NULL) {
		if (e_shell_handle_uris (shell, uris, import_uris) == 0)
			gtk_main_quit ();
	} else {
		e_shell_create_shell_window (shell, requested_view);
	}

	/* If another Evolution process is running, we're done. */
	if (g_application_get_is_remote (G_APPLICATION (shell)))
		gtk_main_quit ();

	return FALSE;
}

#ifdef G_OS_UNIX
static gboolean
handle_term_signal (gpointer data)
{
	EShell *shell;

	g_print ("Received terminate signal...\n");

	shell = e_shell_get_default ();

	if (shell != NULL)
		e_shell_quit (shell, E_SHELL_QUIT_OPTION);

	return FALSE;
}
#endif

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *option_value,
                   gpointer data,
                   GError **error)
{
	g_print ("%s\n", PACKAGE_STRING);

	exit (0);
}

static GOptionEntry entries[] = {
#ifdef G_OS_WIN32
	{ "register-handlers", '\0', G_OPTION_FLAG_HIDDEN,
	  G_OPTION_ARG_NONE, &register_handlers, NULL, NULL },
	{ "reinstall", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reinstall,
	  NULL, NULL },
	{ "show-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_icons,
	  NULL, NULL },
	{ "hide-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &hide_icons,
	  NULL, NULL },
	{ "unregister-handlers", '\0', G_OPTION_FLAG_HIDDEN,
	  G_OPTION_ARG_NONE, &unregister_handlers, NULL, NULL },
#endif /* G_OS_WIN32 */
	{ "component", 'c', 0, G_OPTION_ARG_STRING, &requested_view,
	/* Translators: Do NOT translate the five component
	 * names, they MUST remain in English! */
	  N_("Start Evolution showing the specified component. "
	     "Available options are 'mail', 'calendar', 'contacts', "
	     "'tasks', and 'memos'"), NULL },
	{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
	  N_("Apply the given geometry to the main window"), "GEOMETRY" },
	{ "offline", '\0', 0, G_OPTION_ARG_NONE, &start_offline,
	  N_("Start in offline mode"), NULL },
	{ "online", '\0', 0, G_OPTION_ARG_NONE, &start_online,
	  N_("Start in online mode"), NULL },
	{ "force-online", '\0', 0, G_OPTION_ARG_NONE, &force_online,
	  N_("Ignore network availability"), NULL },
#ifdef KILL_PROCESS_CMD
	{ "force-shutdown", '\0', 0, G_OPTION_ARG_NONE, &force_shutdown,
	  N_("Forcibly shut down Evolution"), NULL },
#endif
	{ "disable-eplugin", '\0', 0, G_OPTION_ARG_NONE, &disable_eplugin,
	  N_("Disable loading of any plugins."), NULL },
	{ "disable-preview", '\0', 0, G_OPTION_ARG_NONE, &disable_preview,
	  N_("Disable preview pane of Mail, Contacts and Tasks."), NULL },
	{ "setup-only", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
	  &setup_only, NULL, NULL },
	{ "import", 'i', 0, G_OPTION_ARG_NONE, &import_uris,
	  N_("Import URIs or filenames given as rest of arguments."), NULL },
	{ "quit", 'q', 0, G_OPTION_ARG_NONE, &quit,
	  N_("Request a running Evolution process to quit"), NULL },
	{ "version", 'v', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
	  G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
	  &remaining_args, NULL, NULL },
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
	GSettings *settings;
	GApplicationFlags flags;
	gboolean online = TRUE;
	GError *error = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	/* Requesting online or offline mode from the command-line
	 * should be persistent, just like selecting it in the UI. */

	if (start_online || force_online) {
		online = TRUE;
		g_settings_set_boolean (settings, "start-offline", FALSE);
	} else if (start_offline) {
		online = FALSE;
		g_settings_set_boolean (settings, "start-offline", TRUE);
	} else {
		gboolean value;

		value = g_settings_get_boolean (settings, "start-offline");
		if (error == NULL)
			online = !value;
	}

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	/* Determine whether to run Evolution in "express" mode. */

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	flags = G_APPLICATION_HANDLES_OPEN |
		G_APPLICATION_HANDLES_COMMAND_LINE;

	shell = g_initable_new (
		E_TYPE_SHELL, NULL, &error,
		"application-id", APPLICATION_ID,
		"flags", flags,
		"geometry", geometry,
		"module-directory", EVOLUTION_MODULEDIR,
		"express-mode", EXPRESS_MODE,
		"online", online,
		"register-session", TRUE,
		NULL);

	/* Failure to register is fatal. */
	if (error != NULL) {
		e_notice (
			NULL, GTK_MESSAGE_ERROR,
			_("Cannot start Evolution.  Another Evolution "
			"instance may be unresponsive. System error: %s"),
			error->message);
		g_clear_error (&error);
	}

	if (force_online && shell)
		e_shell_lock_network_available (shell);

	g_object_unref (settings);

	return shell;
}

gint
main (gint argc,
      gchar **argv)
{
	EShell *shell;
	GSettings *settings;
#ifdef DEVELOPMENT
	gboolean skip_warning_dialog;
#endif
	GError *error = NULL;

#ifdef G_OS_WIN32
	e_util_win32_initialize ();
#endif

	/* Make ElectricFence work.  */
	free (malloc (10));

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Do not require Gtk+ for --force-shutdown */
	if (argc == 2 && argv[1] && g_str_equal (argv[1], "--force-shutdown")) {
		shell_force_shutdown ();

		return 0;
	}

	/* Initialize timezone specific global variables */
	tzset ();

	/* The contact maps feature uses clutter-gtk. */
#ifdef WITH_CONTACT_MAPS
	/* XXX This function is declared in gtk-clutter-util.h with an
	 *     unnecessary G_GNUC_WARN_UNUSED_RESULT attribute.  But we
	 *     don't need the returned error code because we're checking
	 *     the GError directly.  Just ignore this warning. */
	gtk_clutter_init_with_args (
		&argc, &argv,
		_("- The Evolution PIM and Email Client"),
		entries, (gchar *) GETTEXT_PACKAGE, &error);
#else
	gtk_init_with_args (
		&argc, &argv,
		_("- The Evolution PIM and Email Client"),
		entries, (gchar *) GETTEXT_PACKAGE, &error);
#endif /* WITH_CONTACT_MAPS */

	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (1);
	}

#ifdef HAVE_ICAL_UNKNOWN_TOKEN_HANDLING
	ical_set_unknown_token_handling_setting (ICAL_DISCARD_TOKEN);
#endif

#ifdef HAVE_ICALTZUTIL_SET_EXACT_VTIMEZONES_SUPPORT
	icaltzutil_set_exact_vtimezones_support (0);
#endif

#ifdef G_OS_WIN32
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
		SetThreadLocale (
			MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US),
			SORT_DEFAULT));
		setlocale (LC_ALL, "C");
	}
#endif

	if (start_online && start_offline) {
		g_printerr (
			_("%s: --online and --offline cannot be used "
			"together.\n  Run '%s --help' for more "
			"information.\n"), argv[0], argv[0]);
		exit (1);
	} else if (force_online && start_offline) {
		g_printerr (
			_("%s: --force-online and --offline cannot be used "
			"together.\n  Run '%s --help' for more "
			"information.\n"), argv[0], argv[0]);
		exit (1);
	}

	if (force_shutdown)
		shell_force_shutdown ();

	if (disable_preview) {
		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		g_settings_set_boolean (settings, "safe-list", TRUE);
		g_object_unref (settings);

		settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
		g_settings_set_boolean (settings, "show-preview", FALSE);
		g_object_unref (settings);

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		g_settings_set_boolean (settings, "show-memo-preview", FALSE);
		g_settings_set_boolean (settings, "show-task-preview", FALSE);
		g_object_unref (settings);
	}

#ifdef G_OS_UNIX
	g_unix_signal_add_full (
		G_PRIORITY_DEFAULT, SIGTERM,
		handle_term_signal, NULL, NULL);
#endif

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	gtk_window_set_default_icon_name ("evolution");

	if (setup_only)
		exit (0);

	categories_icon_theme_hack ();
	gtk_accel_map_load (e_get_accels_filename ());

#ifdef DEVELOPMENT
	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	skip_warning_dialog = g_settings_get_boolean (
		settings, "skip-warning-dialog");

	if (!skip_warning_dialog && !getenv ("EVOLVE_ME_HARDER"))
		g_settings_set_boolean (
			settings, "skip-warning-dialog",
			show_development_warning ());

	g_object_unref (settings);
#endif

	/* FIXME WK2 - Look if we still need this it looks like it's not. */
	/* Workaround https://bugzilla.gnome.org/show_bug.cgi?id=683548 */
	if (!quit)
		g_type_ensure (WEBKIT_TYPE_WEB_VIEW);

	shell = create_default_shell ();
	if (!shell)
		return 1;

	if (quit) {
		e_shell_quit (shell, E_SHELL_QUIT_OPTION);
		goto exit;
	}

	if (g_application_get_is_remote (G_APPLICATION (shell))) {
		if (remaining_args && *remaining_args)
			e_shell_handle_uris (shell, (const gchar * const *) remaining_args, import_uris);

		/* This will be redirected to the previously run instance,
		   because this instance is remote. */
		if (requested_view && *requested_view)
			e_shell_create_shell_window (shell, requested_view);

		goto exit;
	}

	/* This routine converts the local mail store from mbox format to
	 * Maildir format as needed.  The reason the code is here and not
	 * in the mail module is because we inform the user at startup of
	 * the impending mail conversion by displaying a popup dialog and
	 * waiting for confirmation before proceeding.
	 *
	 * This has to be done before we load modules because some of the
	 * EShellBackends immediately add GMainContext sources that would
	 * otherwise get dispatched during gtk_dialog_run(), and we don't
	 * want them dispatched until after the conversion is complete.
	 *
	 * Addendum: We need to perform the XDG Base Directory migration
	 *           before converting the local mail store, because the
	 *           conversion is triggered by checking for certain key
	 *           files and directories under XDG_DATA_HOME.  Without
	 *           this the mail conversion will not trigger for users
	 *           upgrading from Evolution 2.30 or older. */
	e_migrate_base_dirs (shell);
	e_convert_local_mail (shell);

	e_shell_load_modules (shell);

	if (!disable_eplugin) {
		/* Register built-in plugin hook types. */
		g_type_ensure (E_TYPE_IMPORT_HOOK);
		g_type_ensure (E_TYPE_PLUGIN_UI_HOOK);

		/* All EPlugin and EPluginHook subclasses should be
		 * registered in GType now, so load plugins now. */
		e_plugin_load_plugins ();
	}

	/* Attempt migration -after- loading all modules and plugins,
	 * as both shell backends and certain plugins hook into this. */
	e_shell_migrate_attempt (shell);

	e_shell_event (shell, "ready-to-start", NULL);

	g_idle_add ((GSourceFunc) idle_cb, remaining_args);

	gtk_main ();

exit:
	if (e_shell_requires_shutdown (shell)) {
		/* Workaround https://bugzilla.gnome.org/show_bug.cgi?id=737949 */
		g_signal_emit_by_name (shell, "shutdown");
	}

	/* Drop what should be the last reference to the shell.
	 * That will cause e_shell_get_default() to henceforth
	 * return NULL.  Use that to check for reference leaks. */
	g_object_unref (shell);

	if (e_shell_get_default () != NULL) {
		g_warning ("Shell not finalized on exit");

		/* To not run in the safe mode the next start */
		if (e_file_lock_get_pid () == getpid ())
			e_file_lock_destroy ();
	}

	gtk_accel_map_save (e_get_accels_filename ());

	e_util_cleanup_settings ();
	e_spell_checker_free_global_memory ();

	return 0;
}

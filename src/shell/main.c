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

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
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

#endif

#include <libedataserver/libedataserver.h>

#include <webkit2/webkit2.h>

#ifdef ENABLE_CONTACT_MAPS
#include <clutter-gtk/clutter-gtk.h>
#endif

#include "e-shell-migrate.h"
#include "e-shell.h"

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

#ifdef OVERRIDE_APPLICATION_ID
#define APPLICATION_ID OVERRIDE_APPLICATION_ID
#else
#define APPLICATION_ID "org.gnome.Evolution"
#endif

/* STABLE_VERSION is only defined for development versions. */
#ifdef STABLE_VERSION
#define DEVELOPMENT 1
#endif

/* Set this to TRUE and rebuild to enable MeeGo's Express Mode. */
#define EXPRESS_MODE FALSE

/* Forward declarations */
void e_convert_local_mail (EShell *shell);
void e_migrate_base_dirs (EShell *shell);

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
		"If you find bugs, please report them at\n"
		"https://gitlab.gnome.org/GNOME/evolution/issues/.\n"
		"This product comes with no warranty and is not intended for\n"
		"individuals prone to violent fits of anger.\n"
		"\n"
		"We hope that you enjoy the results of our hard work, and we\n"
		"eagerly await your contributions!\n"),
		STABLE_VERSION);
	label = gtk_label_new (text);
	g_free (text);

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_yalign (GTK_LABEL (label), 0);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	label = gtk_label_new (_("Thanks\nThe Evolution Team\n"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (label), 1);

	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	checkbox = gtk_check_button_new_with_label (_("Do not tell me again"));
	gtk_widget_set_halign (checkbox, GTK_ALIGN_START);

	gtk_box_pack_start (GTK_BOX (vbox), checkbox, TRUE, TRUE, 0);

	gtk_widget_show_all (warning_dialog);

	gtk_dialog_run (GTK_DIALOG (warning_dialog));

	skip = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

	gtk_widget_destroy (warning_dialog);

	return skip;
}

#endif /* DEVELOPMENT */

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

#ifdef G_OS_WIN32
static gboolean
is_any_gettext_catalog_installed (void)
{
	gchar txt[2] = { 0, 0 };
	gchar *text;

	text = gettext (txt);

	return text && strcmp (text, "") != 0;
}
#endif

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
	GApplicationFlags flags;
	GList *module_types;
	GError *error = NULL;

	/* Load all shared library modules. */
	module_types = e_module_load_all_in_directory_and_prefixes (EVOLUTION_MODULEDIR, EVOLUTION_PREFIX);
	g_list_free_full (module_types, (GDestroyNotify) g_type_module_unuse);

	flags = 0;

	shell = g_initable_new (
		E_TYPE_SHELL, NULL, &error,
		"application-id", APPLICATION_ID,
		"flags", flags,
		"module-directory", EVOLUTION_MODULEDIR,
		"express-mode", EXPRESS_MODE,
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

	return shell;
}

gint
main (gint argc,
      gchar **argv)
{
	EShell *shell;
	gboolean is_remote;
	gint ret;

#ifdef G_OS_WIN32
	e_util_win32_initialize ();
#endif

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Do not require Gtk+ for --force-shutdown */
	if (argc == 2 && argv[1] && g_str_equal (argv[1], "--force-shutdown")) {
		shell_force_shutdown ();

		return 0;
	}

	/* The bug is fixed in 2.38.0, thus disable sandboxing only for previous versions */
	if (webkit_get_major_version () < 2 || (webkit_get_major_version () == 2 && webkit_get_minor_version () < 38)) {
		/* Disable sandboxing to enable printing, until WebKitGTK is fixed:
		   https://bugs.webkit.org/show_bug.cgi?id=202363 */
		g_setenv ("WEBKIT_FORCE_SANDBOX", "0", FALSE);
	}

	/* To pair the app with the desktop file */
	g_set_prgname ("org.gnome.Evolution");

	/* Pre-cache list of supported locales */
	e_util_enum_supported_locales ();

	/* Initialize timezone specific global variables */
	tzset ();

	/* Workaround https://bugzilla.gnome.org/show_bug.cgi?id=674885 */
	g_type_ensure (G_TYPE_DBUS_CONNECTION);
	g_type_ensure (G_TYPE_DBUS_PROXY);
	g_type_ensure (G_BUS_TYPE_SESSION);

	i_cal_set_unknown_token_handling_setting (I_CAL_DISCARD_TOKEN);

#ifdef G_OS_UNIX
	g_unix_signal_add_full (
		G_PRIORITY_DEFAULT, SIGTERM,
		handle_term_signal, NULL, NULL);
#endif

	e_util_init_main_thread (NULL);
	e_xml_initialize_in_main ();

	shell = create_default_shell ();
	if (!shell)
		return 1;

	is_remote = g_application_get_is_remote (G_APPLICATION (shell));

	if (!is_remote) {
		#ifdef ENABLE_MAINTAINER_MODE
		GtkIconTheme *icon_theme;
		#endif
		#ifdef DEVELOPMENT
		GSettings *settings;
		gboolean skip_warning_dialog;
		#endif

		e_passwords_init ();
		gtk_window_set_default_icon_name ("evolution");

		#ifdef ENABLE_MAINTAINER_MODE
		icon_theme = gtk_icon_theme_get_default ();
		gtk_icon_theme_prepend_search_path (icon_theme, EVOLUTION_ICONDIR_IN_PREFIX);
		#endif

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

		/* The CamelStoreDB migrates on open automatically, which means when
		   the EMailBackend is constructed, which does not provide UI feedback,
		   but the migration can take time, thus abuse it and open each folders.db
		   file before the EMailBackend creates the CamelStore-s */
		e_shell_maybe_migrate_mail_folders_db (shell);

		/* Clutter is not developed anymore. Unfortunately, when its options are parsed,
		   it can cause a crash when the instance is a remote app, not the main app.
		   Side-effect of this change is that the --help will show different options
		   when invoked on the remote instance and when on a primary instance. */
		#ifdef ENABLE_CONTACT_MAPS
		g_application_add_option_group (G_APPLICATION (shell), cogl_get_option_group ());
		g_application_add_option_group (G_APPLICATION (shell), clutter_get_option_group ());
		g_application_add_option_group (G_APPLICATION (shell), gtk_clutter_get_option_group ());
		#endif
	}

	ret = g_application_run (G_APPLICATION (shell), argc, argv);

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

	e_misc_util_free_global_memory ();

	return ret;
}

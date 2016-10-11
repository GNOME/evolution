/*
 * e-shell-migrate.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <libedataserver/libedataserver.h>

#include "e-shell-migrate.h"
#include "evo-version.h"

static gboolean
shell_migrate_attempt (EShell *shell,
                       gint major,
                       gint minor,
                       gint micro)
{
	GtkWindow *parent;
	GList *backends;
	gboolean success = TRUE;

	parent = e_shell_get_active_window (shell);
	backends = e_shell_get_shell_backends (shell);

	/* New user accounts have nothing to migrate. */
	if (major == 0 && minor == 0 && micro == 0)
		return TRUE;

	/* We only support migrating from version 2 now. */
	if (major < 2) {
		gchar *version;
		gint response;

		version = g_strdup_printf ("%d.%d", major, minor);
		response = e_alert_run_dialog_for_args (
			parent, "shell:upgrade-version-too-old",
			version, NULL);
		g_free (version);

		return (response == GTK_RESPONSE_OK);
	}

	/* Ask each of the shell backends to migrate their own data.
	 * XXX If something fails the user may end up with only partially
	 *     migrated data.  Need transaction semantics here, but how? */
	while (success && backends != NULL) {
		EShellBackend *shell_backend = backends->data;
		GError *error = NULL;

		success = e_shell_backend_migrate (
			shell_backend, major, minor, micro, &error);

		if (error != NULL) {
			gint response;

			response = e_alert_run_dialog_for_args (
				parent, "shell:upgrade-failed",
				error->message, NULL);

			success = (response == GTK_RESPONSE_OK);

			g_error_free (error);
		}

		backends = g_list_next (backends);
	}

	return success;
}

static void
shell_migrate_get_version (EShell *shell,
                           gint *major,
                           gint *minor,
                           gint *micro)
{
	GSettings *settings;
	gchar *string;

	*major = 0;
	*minor = 0;
	*micro = 0;

	settings = e_util_ref_settings ("org.gnome.evolution");
	string = g_settings_get_string (settings, "version");

	if (string != NULL) {
		/* Since 1.4.0 we've kept the version key in GSettings. */
		sscanf (string, "%d.%d.%d", major, minor, micro);
		g_free (string);
	}

	g_object_unref (settings);
}

static gboolean
shell_migrate_downgraded (gint previous_major,
                          gint previous_minor,
                          gint previous_micro)
{
	gboolean downgraded;

	/* This could just be a single boolean expression,
	 * but I find this form easier to understand. */

	if (previous_major == EVO_MAJOR_VERSION) {
		if (previous_minor == EVO_MINOR_VERSION) {
			downgraded = (previous_micro > EVO_MICRO_VERSION);
		} else {
			downgraded = (previous_minor > EVO_MINOR_VERSION);
		}
	} else {
		downgraded = (previous_major > EVO_MAJOR_VERSION);
	}

	return downgraded;
}

static void
change_dir_modes (const gchar *path)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *file = NULL;

	dir = g_dir_open (path, 0, &err);
	if (err) {
		g_warning ("Error opening directory %s: %s \n", path, err->message);
		g_clear_error (&err);
		return;
	}

	while ((file = g_dir_read_name (dir))) {
		gchar *full_path = g_build_filename (path, file, NULL);

		if (g_file_test (full_path, G_FILE_TEST_IS_DIR))
			change_dir_modes (full_path);

		g_free (full_path);
	}

	if (g_chmod (path, 0700) == -1)
		g_warning ("%s: Failed to chmod of '%s': %s", G_STRFUNC, path, g_strerror (errno));

	g_dir_close (dir);
}

static void
fix_folder_permissions (const gchar *data_dir)
{
	struct stat sb;

	if (g_stat (data_dir, &sb) == -1) {
		g_warning ("error stat: %s \n", data_dir);
		return;
	}

	if (((guint32) sb.st_mode & 0777) != 0700)
		change_dir_modes (data_dir);
}

static void
shell_migrate_save_current_version (void)
{
	GSettings *settings;
	gchar *version;

	/* Save the version after the startup wizard has had a chance to
	 * run.  If the user chooses to restore data and settings from a
	 * backup, Evolution will restart and the restored data may need
	 * to be migrated.
	 *
	 * If we save the version before the restart, then Evolution will
	 * think it has already migrated data and settings to the current
	 * version and the restored data may not be handled properly.
	 *
	 * This implies an awareness of module behavior from within the
	 * application core, but practical considerations overrule here. */

	settings = e_util_ref_settings ("org.gnome.evolution");

	version = g_strdup_printf (
		"%d.%d.%d",
		EVO_MAJOR_VERSION,
		EVO_MINOR_VERSION,
		EVO_MICRO_VERSION);
	g_settings_set_string (settings, "version", version);
	g_free (version);

	g_object_unref (settings);
}

static void
shell_migrate_ready_to_start_event_cb (EShell *shell)
{
	shell_migrate_save_current_version ();
}

gboolean
e_shell_migrate_attempt (EShell *shell)
{
	gint major, minor, micro;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	shell_migrate_get_version (shell, &major, &minor, &micro);

	/* Abort all migration if the user downgraded. */
	if (shell_migrate_downgraded (major, minor, micro))
		return TRUE;

	/* This sets the folder permissions to S_IRWXU if needed */
	if (major <= 2 && minor <= 30)
		fix_folder_permissions (e_get_user_data_dir ());

	/* Attempt to run migration all the time and let the backend
	 * make the choice */
	if (!shell_migrate_attempt (shell, major, minor, micro))
		_exit (EXIT_SUCCESS);

	/* We want our handler to run last, hence g_signal_connect_after(). */
	g_signal_connect_after (
		shell, "event::ready-to-start",
		G_CALLBACK (shell_migrate_ready_to_start_event_cb), NULL);

	return TRUE;
}

GQuark
e_shell_migrate_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
		quark = g_quark_from_static_string (
			"e-shell-migrate-error-quark");

	return quark;
}

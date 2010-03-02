/*
 * e-shell-migrate.c
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

#include "e-shell-migrate.h"

#include <string.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libedataserver/e-xml-utils.h>

#include "e-util/e-alert-dialog.h"
#include "e-util/e-file-utils.h"
#include "e-util/e-util.h"

#include "es-event.h"

#define GCONF_VERSION_KEY	"/apps/evolution/version"
#define GCONF_LAST_VERSION_KEY	"/apps/evolution/last_version"

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
	GConfClient *client;
	const gchar *key;
	gchar *string;

	key = GCONF_VERSION_KEY;
	client = e_shell_get_gconf_client (shell);
	string = gconf_client_get_string (client, key, NULL);

	if (string != NULL) {
		/* Since 1.4.0 we've kept the version key in GConf. */
		sscanf (string, "%d.%d.%d", major, minor, micro);
		g_free (string);

	} else {
		/* Otherwise, assume it's a new installation. */
		*major = 0;
		*minor = 0;
		*micro = 0;
	}
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

	g_chmod (path, 0700);
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

gboolean
e_shell_migrate_attempt (EShell *shell)
{
	ESEvent *ese;
	GConfClient *client;
	const gchar *key;
	gint major, minor, micro;
	gint last_major, last_minor, last_micro;
	gint curr_major, curr_minor, curr_micro;
	gboolean migrated = FALSE;
	gchar *string;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	client = e_shell_get_gconf_client (shell);

	if (sscanf (BASE_VERSION, "%d.%d", &curr_major, &curr_minor) != 2) {
		g_warning ("Could not parse BASE_VERSION (%s)", BASE_VERSION);
		return TRUE;
	}

	curr_micro = atoi (UPGRADE_REVISION);

	shell_migrate_get_version (shell, &major, &minor, &micro);

	/* This sets the folder permissions to S_IRWXU if needed */
	if (curr_major <= 2 && curr_minor <= 30)
		fix_folder_permissions (e_get_user_data_dir ());

	if (!(curr_major > major ||
		(curr_major == major && curr_minor > minor) ||
		(curr_major == major && curr_minor == minor && curr_micro > micro)))
		goto check_old;

	if (!shell_migrate_attempt (shell, major, minor, micro))
		_exit (EXIT_SUCCESS);

	/* Record a successful migration. */
	string = g_strdup_printf (
		"%d.%d.%d", curr_major, curr_minor, curr_micro);
	gconf_client_set_string (client, GCONF_VERSION_KEY, string, NULL);
	g_free (string);

	migrated = TRUE;

check_old:

	key = GCONF_LAST_VERSION_KEY;

	/* Try to retrieve the last migrated version from GConf. */
	string = gconf_client_get_string (client, key, NULL);
	if (migrated || string == NULL || sscanf (string, "%d.%d.%d",
		&last_major, &last_minor, &last_micro) != 3) {
		last_major = major;
		last_minor = minor;
		last_micro = micro;
	}
	g_free (string);

	string = g_strdup_printf (
		"%d.%d.%d", last_major, last_minor, last_micro);
	gconf_client_set_string (client, key, string, NULL);
	g_free (string);

	/** @Event: Shell attempted upgrade
	 * @Id: upgrade.done
	 * @Target: ESMenuTargetState
	 *
	 * This event is emitted whenever the shell successfully attempts
	 * an upgrade.
	 **/
	ese = es_event_peek ();
	e_event_emit (
		(EEvent *) ese, "upgrade.done",
		(EEventTarget *) es_event_target_new_upgrade (
		ese, curr_major, curr_minor, curr_micro));

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

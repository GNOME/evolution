/*
 * e-migrate-base-dirs.c
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
 */

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>

#include <shell/e-shell.h>

/* Forward Declarations */
void e_migrate_base_dirs (EShell *shell);

/* These are the known EShellBackend names as of Evolution 3.0 */
static const gchar *shell_backend_names[] =
	{ "addressbook", "calendar", "mail", "memos", "tasks", NULL };

static gboolean
shell_xdg_migrate_rename (const gchar *old_filename,
                          const gchar *new_filename)
{
	gboolean old_filename_is_dir;
	gboolean old_filename_exists;
	gboolean new_filename_exists;
	gboolean success = TRUE;

	old_filename_is_dir = g_file_test (old_filename, G_FILE_TEST_IS_DIR);
	old_filename_exists = g_file_test (old_filename, G_FILE_TEST_EXISTS);
	new_filename_exists = g_file_test (new_filename, G_FILE_TEST_EXISTS);

	if (!old_filename_exists)
		return TRUE;

	g_print ("  mv %s %s\n", old_filename, new_filename);

	/* It's safe to go ahead and move directories because rename ()
	 * will fail if the new directory already exists with content.
	 * With regular files we have to be careful not to overwrite
	 * new files with old files. */
	if (old_filename_is_dir || !new_filename_exists) {
		if (g_rename (old_filename, new_filename) < 0) {
			g_printerr ("  FAILED: %s\n", g_strerror (errno));
			success = FALSE;
		}
	} else {
		g_printerr ("  FAILED: Destination file already exists\n");
		success = FALSE;
	}

	return success;
}

static gboolean
shell_xdg_migrate_rmdir (const gchar *dirname)
{
	GDir *dir = NULL;
	gboolean success = TRUE;

	if (g_file_test (dirname, G_FILE_TEST_IS_DIR)) {
		g_print ("  rmdir %s\n", dirname);
		if (g_rmdir (dirname) < 0) {
			g_printerr ("  FAILED: %s", g_strerror (errno));
			if (errno == ENOTEMPTY) {
				dir = g_dir_open (dirname, 0, NULL);
				g_printerr (" (contents follows)");
			}
			g_printerr ("\n");
			success = FALSE;
		}
	}

	/* List the directory's contents to aid debugging. */
	if (dir != NULL) {
		const gchar *basename;

		/* Align the filenames beneath the error message. */
		while ((basename = g_dir_read_name (dir)) != NULL)
			g_print ("          %s\n", basename);

		g_dir_close (dir);
	}

	return success;
}

static void
shell_xdg_migrate_process_corrections (GHashTable *corrections)
{
	GHashTableIter iter;
	gpointer old_filename;
	gpointer new_filename;

	g_hash_table_iter_init (&iter, corrections);

	while (g_hash_table_iter_next (&iter, &old_filename, &new_filename)) {
		gboolean is_directory;

		is_directory = g_file_test (old_filename, G_FILE_TEST_IS_DIR);

		/* If the old filename is a directory and the new filename
		 * is NULL, treat it as a request to remove the directory. */
		if (is_directory && new_filename == NULL)
			shell_xdg_migrate_rmdir (old_filename);
		else
			shell_xdg_migrate_rename (old_filename, new_filename);

		g_hash_table_iter_remove (&iter);
	}
}

static gboolean
shell_xdg_migrate_rename_files (const gchar *src_directory,
                                const gchar *dst_directory)
{
	GDir *dir;
	GHashTable *corrections;
	const gchar *basename;
	const gchar *home_dir;
	gchar *old_base_dir;
	gchar *new_base_dir;

	dir = g_dir_open (src_directory, 0, NULL);
	if (dir == NULL)
		return FALSE;

	/* This is to avoid renaming files which we're iterating over the
	 * directory.  POSIX says the outcome of that is unspecified. */
	corrections = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	g_mkdir_with_parents (dst_directory, 0700);

	home_dir = g_get_home_dir ();
	old_base_dir = g_build_filename (home_dir, ".evolution", NULL);
	e_util_make_safe_filename (old_base_dir);
	new_base_dir = g_strdup (e_get_user_data_dir ());
	e_util_make_safe_filename (new_base_dir);

	while ((basename = g_dir_read_name (dir)) != NULL) {
		GString *buffer;
		gchar *old_filename;
		gchar *new_filename;
		gchar *cp;

		buffer = g_string_new (basename);

		if ((cp = strstr (basename, old_base_dir)) != NULL) {
			g_string_erase (
				buffer, cp - basename,
				strlen (old_base_dir));
			g_string_insert (
				buffer, cp - basename, new_base_dir);
		}

		old_filename = g_build_filename (
			src_directory, basename, NULL);
		new_filename = g_build_filename (
			dst_directory, buffer->str, NULL);

		g_string_free (buffer, TRUE);

		g_hash_table_insert (corrections, old_filename, new_filename);
	}

	g_free (old_base_dir);
	g_free (new_base_dir);

	g_dir_close (dir);

	shell_xdg_migrate_process_corrections (corrections);
	g_hash_table_destroy (corrections);

	/* It's tempting to want to remove the source directory here.
	 * Don't.  We might be iterating over the source directory's
	 * parent directory, and removing the source directory would
	 * screw up the iteration. */

	return TRUE;
}

static gboolean
shell_xdg_migrate_move_contents (const gchar *src_directory,
                                 const gchar *dst_directory)
{
	GDir *dir;
	GHashTable *corrections;
	const gchar *basename;

	dir = g_dir_open (src_directory, 0, NULL);
	if (dir == NULL)
		return FALSE;

	/* This is to avoid renaming files which we're iterating over the
	 * directory.  POSIX says the outcome of that is unspecified. */
	corrections = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	g_mkdir_with_parents (dst_directory, 0700);

	while ((basename = g_dir_read_name (dir)) != NULL) {
		gchar *old_filename;
		gchar *new_filename;

		old_filename = g_build_filename (src_directory, basename, NULL);
		new_filename = g_build_filename (dst_directory, basename, NULL);

		g_hash_table_insert (corrections, old_filename, new_filename);
	}

	g_dir_close (dir);

	shell_xdg_migrate_process_corrections (corrections);
	g_hash_table_destroy (corrections);

	/* It's tempting to want to remove the source directory here.
	 * Don't.  We might be iterating over the source directory's
	 * parent directory, and removing the source directory would
	 * screw up the iteration. */

	return TRUE;
}

static void
shell_xdg_migrate_cache_dir (EShell *shell,
                             const gchar *old_base_dir)
{
	const gchar *new_cache_dir;
	gchar *old_cache_dir;
	gchar *old_filename;
	gchar *new_filename;

	old_cache_dir = g_build_filename (old_base_dir, "cache", NULL);
	new_cache_dir = e_get_user_cache_dir ();

	g_print ("Migrating cached data\n");

	g_mkdir_with_parents (new_cache_dir, 0700);

	old_filename = g_build_filename (old_cache_dir, "http", NULL);
	new_filename = g_build_filename (new_cache_dir, "http", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	old_filename = g_build_filename (old_cache_dir, "tmp", NULL);
	new_filename = g_build_filename (new_cache_dir, "tmp", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* Try to remove the old cache directory.  Good chance this will
	 * fail on the first try, since E-D-S puts stuff here too. */
	shell_xdg_migrate_rmdir (old_cache_dir);

	g_free (old_cache_dir);
}

static void
shell_xdg_migrate_config_dir_common (EShell *shell,
                                     const gchar *old_base_dir,
                                     const gchar *backend_name)
{
	GDir *dir;
	const gchar *user_config_dir;
	gchar *old_config_dir;
	gchar *new_config_dir;
	gchar *old_filename;
	gchar *new_filename;
	gchar *dirname;

	user_config_dir = e_get_user_config_dir ();

	old_config_dir = g_build_filename (old_base_dir, backend_name, NULL);
	new_config_dir = g_build_filename (user_config_dir, backend_name, NULL);

	g_mkdir_with_parents (new_config_dir, 0700);

	old_filename = g_build_filename (old_config_dir, "views", NULL);
	new_filename = g_build_filename (new_config_dir, "views", NULL);
	shell_xdg_migrate_rename_files (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	old_filename = g_build_filename (old_config_dir, "searches.xml", NULL);
	new_filename = g_build_filename (new_config_dir, "searches.xml", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* This one only occurs in calendar and memos.
	 * For other backends this will just be a no-op. */
	old_filename = g_build_filename (
		old_config_dir, "config", "MemoPad", NULL);
	new_filename = g_build_filename (new_config_dir, "MemoPad", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* This one only occurs in calendar and tasks.
	 * For other backends this will just be a no-op. */
	old_filename = g_build_filename (
		old_config_dir, "config", "TaskPad", NULL);
	new_filename = g_build_filename (new_config_dir, "TaskPad", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* Subtle name change: config/state --> state.ini */
	old_filename = g_build_filename (old_config_dir, "config", "state", NULL);
	new_filename = g_build_filename (new_config_dir, "state.ini", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* GIO had a bug for awhile where it would leave behind an empty
	 * temp file with the pattern .goutputstream-XXXXXX if an output
	 * stream operation was cancelled.  We've had several reports of
	 * these files in config directories, so remove any we find. */
	dirname = g_build_filename (old_config_dir, "config", NULL);
	dir = g_dir_open (dirname, 0, NULL);
	if (dir != NULL) {
		const gchar *basename;

		while ((basename = g_dir_read_name (dir)) != NULL) {
			gchar *filename;
			struct stat st;

			if (!g_str_has_prefix (basename, ".goutputstream"))
				continue;

			filename = g_build_filename (dirname, basename, NULL);

			/* Verify the file is indeed empty. */
			if (g_stat (filename, &st) == 0 && st.st_size == 0)
				g_unlink (filename);

			g_free (filename);
		}

		g_dir_close (dir);
	}
	g_free (dirname);

	g_free (old_config_dir);
	g_free (new_config_dir);
}

static void
shell_xdg_migrate_config_dir_mail (EShell *shell,
                                   const gchar *old_base_dir)
{
	const gchar *user_config_dir;
	gchar *old_config_dir;
	gchar *new_config_dir;
	gchar *old_filename;
	gchar *new_filename;

	user_config_dir = e_get_user_config_dir ();

	old_config_dir = g_build_filename (old_base_dir, "mail", NULL);
	new_config_dir = g_build_filename (user_config_dir, "mail", NULL);

	old_filename = g_build_filename (old_config_dir, "filters.xml", NULL);
	new_filename = g_build_filename (new_config_dir, "filters.xml", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	old_filename = g_build_filename (old_config_dir, "vfolders.xml", NULL);
	new_filename = g_build_filename (new_config_dir, "vfolders.xml", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* I hate this file.  GtkHtml uses style properties for fonts. */
	old_filename = g_build_filename (
		old_config_dir, "config", "gtkrc-mail-fonts", NULL);
	new_filename = g_build_filename (
		new_config_dir, "gtkrc-mail-fonts", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* This file is no longer used.  Try removing it. */
	old_filename = g_build_filename (
		old_config_dir, "config",
		"folder-tree-expand-state.xml", NULL);
	g_unlink (old_filename);
	g_free (old_filename);

	/* Everything else in the "config" directory just should be
	 * per-folder ETree files recording the expanded state of mail
	 * threads.  Rename this directory to "folders". */
	old_filename = g_build_filename (old_config_dir, "config", NULL);
	new_filename = g_build_filename (new_config_dir, "folders", NULL);
	shell_xdg_migrate_rename_files (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	g_free (old_config_dir);
	g_free (new_config_dir);
}

static void
shell_xdg_migrate_dir_cleanup (EShell *shell,
                                      const gchar *old_base_dir,
                                      const gchar *backend_name,
                                      const gchar *dir_name)
{
	gchar *dirname;

	dirname = g_build_filename (
		old_base_dir, backend_name, dir_name, NULL);

	shell_xdg_migrate_rmdir (dirname);

	g_free (dirname);
}

static void
shell_xdg_migrate_config_dir (EShell *shell,
                              const gchar *old_base_dir)
{
	const gchar *old_config_dir;
	const gchar *new_config_dir;
	gchar *old_filename;
	gchar *new_filename;
	gint ii;

	g_print ("Migrating config data\n");

	/* Some files are common to all shell backends. */
	for (ii = 0; shell_backend_names[ii] != NULL; ii++)
		shell_xdg_migrate_config_dir_common (
			shell, old_base_dir, shell_backend_names[ii]);

	/* Handle backend-specific files. */
	shell_xdg_migrate_config_dir_mail (shell, old_base_dir);

	/* Remove leftover config directories. */
	for (ii = 0; shell_backend_names[ii] != NULL; ii++) {
		shell_xdg_migrate_dir_cleanup (
			shell, old_base_dir, shell_backend_names[ii], "config");
		shell_xdg_migrate_dir_cleanup (
			shell, old_base_dir, shell_backend_names[ii], "views");
	}

	/*** Miscellaneous configuration files. ***/

	old_config_dir = old_base_dir;
	new_config_dir = e_get_user_config_dir ();

	/* Subtle name change: datetime-formats --> datetime-formats.ini */
	old_filename = g_build_filename (old_config_dir, "datetime-formats", NULL);
	new_filename = g_build_filename (new_config_dir, "datetime-formats.ini", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);

	/* Subtle name change: printing --> printing.ini */
	old_filename = g_build_filename (old_config_dir, "printing", NULL);
	new_filename = g_build_filename (new_config_dir, "printing.ini", NULL);
	shell_xdg_migrate_rename (old_filename, new_filename);
	g_free (old_filename);
	g_free (new_filename);
}

static void
shell_xdg_migrate_data_dir (EShell *shell,
                            const gchar *old_base_dir)
{
	GDir *dir;
	GHashTable *corrections;
	const gchar *basename;
	const gchar *old_data_dir;
	const gchar *new_data_dir;
	gchar *src_directory;
	gchar *dst_directory;

	g_print ("Migrating local user data\n");

	old_data_dir = old_base_dir;
	new_data_dir = e_get_user_data_dir ();

	/* The mail hierarchy is complex and Camel doesn't distinguish
	 * between user data files and disposable cache files, so just
	 * move everything to the data directory for now.  We'll sort
	 * it out sometime down the road. */

	src_directory = g_build_filename (old_data_dir, "mail", NULL);
	dst_directory = g_build_filename (new_data_dir, "mail", NULL);

	dir = g_dir_open (src_directory, 0, NULL);
	if (dir == NULL)
		goto skip_mail;

	/* This is to avoid removing directories while we're iterating
	 * over the parent directory.  POSIX says the outcome of that
	 * is unspecified. */
	corrections = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	/* Iterate over the base CamelProvider directories. */
	while ((basename = g_dir_read_name (dir)) != NULL) {
		gchar *provider_src_directory;
		gchar *provider_dst_directory;

		provider_src_directory =
			g_build_filename (src_directory, basename, NULL);
		provider_dst_directory =
			g_build_filename (dst_directory, basename, NULL);

		if (!g_file_test (provider_src_directory, G_FILE_TEST_IS_DIR)) {
			g_free (provider_src_directory);
			g_free (provider_dst_directory);
			continue;
		}

		shell_xdg_migrate_move_contents (
			provider_src_directory, provider_dst_directory);

		g_hash_table_insert (corrections, provider_src_directory, NULL);
		g_free (provider_dst_directory);
	}

	g_dir_close (dir);

	/* Remove the old base CamelProvider directories. */
	shell_xdg_migrate_process_corrections (corrections);
	g_hash_table_destroy (corrections);

skip_mail:

	g_free (src_directory);
	g_free (dst_directory);

	/* We don't want to move the source directory directly because the
	 * destination directory may already exist with content.  Instead
	 * we want to merge the content of the source directory into the
	 * destination directory.
	 *
	 * For example, given:
	 *
	 *    $(src_directory)/A   and   $(dst_directory)/B
	 *    $(src_directory)/C
	 *
	 * we want to end up with:
	 *
	 *    $(dst_directory)/A
	 *    $(dst_directory)/B
	 *    $(dst_directory)/C
	 *
	 * Any name collisions will be left in the source directory.
	 */

	src_directory = g_build_filename (old_data_dir, "signatures", NULL);
	dst_directory = g_build_filename (new_data_dir, "signatures", NULL);

	shell_xdg_migrate_move_contents (src_directory, dst_directory);
	shell_xdg_migrate_rmdir (src_directory);

	g_free (src_directory);
	g_free (dst_directory);

	/* Move all remaining regular files to the new data directory. */

	dir = g_dir_open (old_data_dir, 0, NULL);
	if (dir == NULL)
		return;

	/* This is to avoid renaming files while we're iterating over the
	 * directory.  POSIX says the outcome of that is unspecified. */
	corrections = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	while ((basename = g_dir_read_name (dir)) != NULL) {
		gchar *old_filename;
		gchar *new_filename;

		old_filename = g_build_filename (old_data_dir, basename, NULL);
		new_filename = g_build_filename (new_data_dir, basename, NULL);

		/* If we encounter a directory, try removing it.  This
		 * will only work if the directory is empty, so there's
		 * no risk of data loss. */
		if (g_file_test (old_filename, G_FILE_TEST_IS_DIR)) {
			shell_xdg_migrate_rmdir (old_filename);
			g_free (old_filename);
			g_free (new_filename);
			continue;
		}

		g_hash_table_insert (corrections, old_filename, new_filename);
	}

	g_dir_close (dir);

	shell_xdg_migrate_process_corrections (corrections);
	g_hash_table_destroy (corrections);
}

void
e_migrate_base_dirs (EShell *shell)
{
	const gchar *home_dir;
	gchar *old_base_dir;

	g_return_if_fail (E_IS_SHELL (shell));

	/* XXX This blocks, but it's all just local file
	 *     renames so it should be nearly instantaneous. */

	home_dir = g_get_home_dir ();
	old_base_dir = g_build_filename (home_dir, ".evolution", NULL);

	/* Is there even anything to migrate? */
	if (!g_file_test (old_base_dir, G_FILE_TEST_IS_DIR))
		goto exit;

	shell_xdg_migrate_cache_dir (shell, old_base_dir);
	shell_xdg_migrate_config_dir (shell, old_base_dir);
	shell_xdg_migrate_data_dir (shell, old_base_dir);

	/* Try to remove the old base directory.  Good chance this will
	 * fail on the first try, since Evolution puts stuff here too. */
	g_rmdir (old_base_dir);

exit:
	g_free (old_base_dir);
}


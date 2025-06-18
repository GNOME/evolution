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
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <camel/camel.h>
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

static void
shell_migrate_mail_folders_gather_dirs_in (GPtrArray *dirs, /* gchar * */
					   const gchar *root_dir)
{
	GDir *dir;
	gchar *path;

	path = g_build_filename (root_dir, "mail", NULL);
	dir = g_dir_open (path, 0, NULL);

	if (dir) {
		const gchar *name;

		while ((name = g_dir_read_name (dir)) != NULL) {
			gchar *filename;

			if (g_strcmp0 (name, "trash") == 0)
				continue;

			filename = g_build_filename (path, name, CAMEL_STORE_DB_FILE, NULL);

			if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
				g_ptr_array_add (dirs, g_build_filename (path, name, NULL));

			g_free (filename);
		}

		g_dir_close (dir);
	}

	g_free (path);
}

typedef struct _MigrateFoldersData {
	EShell *shell;
	GMainLoop *main_loop;
	GCancellable *cancellable;
	GtkWidget *window;
	GtkProgressBar *total_progress;
	GtkProgressBar *file_progress;
	GPtrArray *dirs; /* gchar *; ending with the source UID */
	guint dir_index;
	guint show_dialog_id; /* timeout ID, set to 0 when triggered */
	guint update_dialog_id;
	GMutex lock;
	gchar *store_display_name;
	gchar *file_progress_text;
	gdouble file_progress_fraction;
	guint last_dir_index;
	GString *all_errors;
} MigrateFoldersData;

static gboolean
shell_migrate_mail_folders_update_dialog_cb (gpointer user_data)
{
	MigrateFoldersData *mfd = user_data;

	g_mutex_lock (&mfd->lock);

	if (mfd->last_dir_index != mfd->dir_index &&
	    mfd->dir_index < mfd->dirs->len) {
		mfd->last_dir_index = mfd->dir_index;
		gtk_progress_bar_set_fraction (mfd->total_progress, ((gdouble) mfd->dir_index) / mfd->dirs->len);
		gtk_progress_bar_set_text (mfd->total_progress, mfd->store_display_name);
	}

	gtk_progress_bar_set_fraction (mfd->file_progress, mfd->file_progress_fraction);
	gtk_progress_bar_set_text (mfd->file_progress, mfd->file_progress_text);

	g_mutex_unlock (&mfd->lock);

	return G_SOURCE_CONTINUE;
}

static gpointer
shell_migrate_mail_folders_thread (gpointer user_data)
{
	MigrateFoldersData *mfd = user_data;
	ESourceRegistry *registry = e_shell_get_registry (mfd->shell);

	g_mutex_lock (&mfd->lock);
	shell_migrate_mail_folders_gather_dirs_in (mfd->dirs, e_get_user_data_dir ());
	shell_migrate_mail_folders_gather_dirs_in (mfd->dirs, e_get_user_cache_dir ());

	for (mfd->dir_index = 0; mfd->dir_index < mfd->dirs->len; mfd->dir_index++) {
		const gchar *dir = g_ptr_array_index (mfd->dirs, mfd->dir_index);
		ESource *source;
		gchar *uid;

		if (!dir)
			continue;

		uid = g_path_get_basename (dir);
		if (!uid)
			continue;

		source = e_source_registry_ref_source (registry, uid);
		if (source) {
			CamelStoreDB *store_db;
			GError *local_error = NULL;
			gchar *filename;

			g_free (mfd->store_display_name);
			mfd->store_display_name = e_source_dup_display_name (source);
			g_clear_object (&source);

			filename = g_build_filename (dir, CAMEL_STORE_DB_FILE, NULL);
			g_mutex_unlock (&mfd->lock);

			store_db = camel_store_db_new (filename, mfd->cancellable, &local_error);

			g_mutex_lock (&mfd->lock);

			if (local_error) {
				if (!mfd->all_errors)
					mfd->all_errors = g_string_new ("");
				else
					g_string_append_c (mfd->all_errors, '\n');
				/* Translators: the first '%s' is replaced with a file name, the second '%s' with the actual error message */
				g_string_append_printf (mfd->all_errors, _("File '%s' failed: %s"), filename, local_error->message);
			}

			g_clear_error (&local_error);
			g_clear_object (&store_db);
			g_free (filename);
		}

		g_free (uid);
	}

	g_mutex_unlock (&mfd->lock);

	g_main_loop_quit (mfd->main_loop);

	return NULL;
}

static gboolean
prevent_window_delete_cb (GtkWidget *widget,
			  GdkEvent *event,
			  gpointer user_data)
{
	return TRUE;
}

static gboolean
shell_migrate_mail_folders_show_dialog_cb (gpointer user_data)
{
	MigrateFoldersData *mfd = user_data;
	PangoAttrList *attrs;
	GtkWidget *box;
	GtkWidget *widget;

	mfd->show_dialog_id = 0;

	mfd->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_object_set (mfd->window,
		"window-position", GTK_WIN_POS_CENTER,
		"deletable", FALSE,
		"resizable", FALSE,
		"modal", TRUE, /* to not have "minimize" button */
		"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG, /* to not have "minimize" button */
		"title", _("Evolution"),
		NULL);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	g_object_set (box,
		"border-width", 12,
		NULL);

	gtk_container_add (GTK_CONTAINER (mfd->window), box);

	widget = gtk_label_new (_("Migrating mail folders, please wait…"));

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (1.5));
	gtk_label_set_attributes (GTK_LABEL (widget), attrs);
	pango_attr_list_unref (attrs);

	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

	widget = gtk_progress_bar_new ();
	g_object_set (widget,
		"hexpand", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"show-text", TRUE,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"width-request", 300,
		NULL);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

	mfd->total_progress = GTK_PROGRESS_BAR (widget);

	widget = gtk_progress_bar_new ();
	g_object_set (widget,
		"hexpand", FALSE,
		"halign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"show-text", TRUE,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"width-request", 300,
		NULL);
	gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

	mfd->file_progress = GTK_PROGRESS_BAR (widget);

	shell_migrate_mail_folders_update_dialog_cb (mfd);

	/* do not update the GUI more than 5 times per second */
	mfd->update_dialog_id = g_timeout_add (1000 / 5, shell_migrate_mail_folders_update_dialog_cb, mfd);

	g_signal_connect (mfd->window, "delete-event", G_CALLBACK (prevent_window_delete_cb), NULL);

	gtk_widget_show_all (box);
	gtk_window_present (GTK_WINDOW (mfd->window));

	return G_SOURCE_REMOVE;
}

static void
shell_migrate_mail_folders_status_cb (GCancellable *cancellable,
				      const gchar *msg,
				      gint percent,
				      gpointer user_data)
{
	MigrateFoldersData *mfd = user_data;

	g_mutex_lock (&mfd->lock);
	g_free (mfd->file_progress_text);
	mfd->file_progress_text = g_strdup (msg);
	mfd->file_progress_fraction = percent / 100.0;
	g_mutex_unlock (&mfd->lock);
}

void
e_shell_maybe_migrate_mail_folders_db (EShell *shell)
{
	MigrateFoldersData mfd = { 0, };
	gint major = 0, minor = 0, micro = 0;

	g_return_if_fail (E_IS_SHELL (shell));

	shell_migrate_get_version (shell, &major, &minor, &micro);

	if (major == 0 && minor == 0 && micro == 0)
		return;

	/* Skip migration if the user downgraded. */
	if (shell_migrate_downgraded (major, minor, micro))
		return;

	if (!(major <= 2 || (major == 3 && minor < 57) || (major == 3 && minor == 57 && micro < 1)))
		return;

	memset (&mfd, 0, sizeof (MigrateFoldersData));
	g_mutex_init (&mfd.lock);
	mfd.shell = shell;
	mfd.main_loop = g_main_loop_new (NULL, FALSE);
	mfd.cancellable = camel_operation_new ();
	mfd.dirs = g_ptr_array_new_with_free_func (g_free);
	/* show a dialog when it takes longer than 500 ms */
	mfd.show_dialog_id = g_timeout_add (500, shell_migrate_mail_folders_show_dialog_cb, &mfd);
	mfd.last_dir_index = G_MAXUINT;

	g_signal_connect (mfd.cancellable, "status", G_CALLBACK (shell_migrate_mail_folders_status_cb), &mfd);

	g_thread_unref (g_thread_new ("MigrateFoldersDB", shell_migrate_mail_folders_thread, &mfd));

	g_main_loop_run (mfd.main_loop);

	if (mfd.show_dialog_id)
		g_source_remove (mfd.show_dialog_id);
	if (mfd.update_dialog_id)
		g_source_remove (mfd.update_dialog_id);
	if (mfd.all_errors) {
		e_alert_run_dialog_for_args ((gpointer) mfd.window, "system:generic-warning",
			_("Some of the mail accounts failed to migrate."),
			mfd.all_errors->str,
			NULL);
		g_string_free (mfd.all_errors, TRUE);
	}
	if (mfd.window)
		gtk_widget_destroy (mfd.window);
	g_object_unref (mfd.cancellable);
	g_ptr_array_unref (mfd.dirs);
	g_main_loop_unref (mfd.main_loop);
	g_mutex_clear (&mfd.lock);
	g_free (mfd.store_display_name);
	g_free (mfd.file_progress_text);
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

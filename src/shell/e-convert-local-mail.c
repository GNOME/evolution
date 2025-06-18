/*
 * e-convert-local-mail.c
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

#include "evolution-config.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <camel/camel.h>

#include <shell/e-shell.h>

/* Forward Declarations */
void e_convert_local_mail (EShell *shell);

static gboolean
mail_to_maildir_migration_needed (const gchar *mail_data_dir)
{
	gchar *local_store;
	gchar *local_outbox;
	gboolean migration_needed = FALSE;

	local_store = g_build_filename (mail_data_dir, "local", NULL);
	local_outbox = g_build_filename (local_store, ".Outbox", NULL);

	/* If this is a fresh install (no local store exists yet)
	 * then obviously there's nothing to migrate to Maildir. */
	if (!g_file_test (local_store, G_FILE_TEST_IS_DIR))
		migration_needed = FALSE;

	/* Look for a Maildir Outbox folder. */
	else if (!g_file_test (local_outbox, G_FILE_TEST_IS_DIR))
		migration_needed = TRUE;

	g_free (local_store);
	g_free (local_outbox);

	return migration_needed;
}

/* Folder names with '.' are converted to '_' */
static gchar *
sanitize_maildir_folder_name (gchar *folder_name)
{
	gchar *maildir_folder_name;

	maildir_folder_name = g_strdup (folder_name);
	g_strdelimit (maildir_folder_name, ".", '_');

	 return maildir_folder_name;
}

static void
copy_folder (CamelStore *mail_store,
             CamelStore *maildir_store,
             const gchar *mail_fname,
             const gchar *maildir_fname)
{
	CamelFolder *fromfolder, *tofolder;
	GPtrArray *uids;

	fromfolder = camel_store_get_folder_sync (
		mail_store, mail_fname, 0, NULL, NULL);
	if (fromfolder == NULL) {
		g_warning ("Cannot find mail folder %s \n", mail_fname);
		return;
	}

	tofolder = camel_store_get_folder_sync (
		maildir_store, maildir_fname,
		CAMEL_STORE_FOLDER_CREATE, NULL, NULL);
	if (tofolder == NULL) {
		g_warning ("Cannot create maildir folder %s \n", maildir_fname);
		g_object_unref (fromfolder);
		return;
	}

	uids = camel_folder_dup_uids (fromfolder);
	camel_folder_transfer_messages_to_sync (
		fromfolder, uids, tofolder, FALSE, NULL, NULL, NULL);
	g_ptr_array_unref (uids);

	g_object_unref (fromfolder);
	g_object_unref (tofolder);
}

static void
copy_folders (CamelStore *mail_store,
              CamelStore *maildir_store,
              CamelFolderInfo *fi,
              CamelSession *session)
{
	if (fi) {
		if (!g_str_has_prefix (fi->full_name, ".#evolution")) {
			gchar *maildir_folder_name;

			/* sanitize folder names and copy folders */
			maildir_folder_name =
				sanitize_maildir_folder_name (fi->full_name);
			copy_folder (
				mail_store, maildir_store,
				fi->full_name, maildir_folder_name);
			g_free (maildir_folder_name);
		}

		if (fi->child)
			copy_folders (mail_store, maildir_store, fi->child, session);

		copy_folders (mail_store, maildir_store, fi->next, session);
	}
}

struct MigrateStore {
	CamelSession *session;
	CamelStore *mail_store;
	CamelStore *maildir_store;
	gboolean complete;
};

static void
migrate_stores (struct MigrateStore *ms)
{
	CamelFolderInfo *mail_fi;
	CamelStore *mail_store = ms->mail_store;
	CamelStore *maildir_store = ms->maildir_store;

	mail_fi = camel_store_get_folder_info_sync (
		mail_store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
		NULL, NULL);

	/* FIXME progres dialog */
	copy_folders (mail_store, maildir_store, mail_fi, ms->session);
	ms->complete = TRUE;
}

static void
rename_mbox_dir (ESource *mbox_source,
                 const gchar *mail_data_dir)
{
	gchar *old_mail_dir;
	gchar *new_mail_dir;
	gboolean need_rename;
	const gchar *mbox_uid;

	mbox_uid = e_source_get_uid (mbox_source);

	old_mail_dir = g_build_filename (mail_data_dir, "local", NULL);
	new_mail_dir = g_build_filename (mail_data_dir, mbox_uid, NULL);

	/* Rename if old directory exists and new directory does not. */
	need_rename =
		g_file_test (old_mail_dir, G_FILE_TEST_EXISTS) &&
		!g_file_test (new_mail_dir, G_FILE_TEST_EXISTS);

	if (need_rename) {
		if (g_rename (old_mail_dir, new_mail_dir) == -1)
			g_warning (
				"%s: Failed to rename '%s' to '%s': %s",
				G_STRFUNC, old_mail_dir, new_mail_dir, g_strerror (errno));
	}

	g_free (old_mail_dir);
	g_free (new_mail_dir);
}

static gboolean
migrate_mbox_to_maildir (EShell *shell,
                         CamelSession *session,
                         ESource *mbox_source)
{
	ESourceRegistry *registry;
	ESourceExtension *extension;
	const gchar *extension_name;
	CamelService *mbox_service = NULL;
	CamelService *maildir_service = NULL;
	CamelSettings *settings;
	const gchar *data_dir;
	const gchar *mbox_uid;
	gchar *path;
	struct MigrateStore ms;
	GThread *thread;
	GError *error = NULL;

	registry = e_shell_get_registry (shell);

	data_dir = camel_session_get_user_data_dir (session);

	mbox_uid = e_source_get_uid (mbox_source);
	e_source_set_display_name (mbox_source, "local_mbox");

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (mbox_source, extension_name);

	e_source_backend_set_backend_name (
		E_SOURCE_BACKEND (extension), "mbox");

	extension_name = e_source_camel_get_extension_name ("mbox");
	extension = e_source_get_extension (mbox_source, extension_name);
	settings = e_source_camel_get_settings (E_SOURCE_CAMEL (extension));

	path = g_build_filename (data_dir, mbox_uid, NULL);
	g_object_set (settings, "path", path, NULL);
	g_free (path);

	e_source_registry_commit_source_sync (
		registry, mbox_source, NULL, &error);

	if (error == NULL)
		mbox_service = camel_session_add_service (
			session, mbox_uid, "mbox",
			CAMEL_PROVIDER_STORE, &error);

	if (error == NULL)
		maildir_service = camel_session_add_service (
			session, "local", "maildir",
			CAMEL_PROVIDER_STORE, &error);

	if (error != NULL) {
		if (mbox_service != NULL)
			g_object_unref (mbox_service);
		if (maildir_service != NULL)
			g_object_unref (maildir_service);
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return FALSE;
	}

	g_return_val_if_fail (CAMEL_IS_STORE (mbox_service), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (maildir_service), FALSE);

	camel_service_set_settings (mbox_service, settings);

	settings = camel_service_ref_settings (maildir_service);

	path = g_build_filename (data_dir, "local", NULL);
	g_object_set (settings, "path", path, NULL);
	if (g_mkdir (path, 0700) == -1)
		g_warning (
			"%s: Failed to make directory '%s': %s",
			G_STRFUNC, path, g_strerror (errno));
	g_free (path);

	g_object_unref (settings);

	ms.mail_store = CAMEL_STORE (mbox_service);
	ms.maildir_store = CAMEL_STORE (maildir_service);
	ms.session = session;
	ms.complete = FALSE;

	thread = g_thread_new (NULL, (GThreadFunc) migrate_stores, &ms);
	/* coverity[loop_condition] */
	while (!ms.complete)
		g_main_context_iteration (NULL, TRUE);

	g_object_unref (mbox_service);
	g_object_unref (maildir_service);
	g_thread_unref (thread);

	/* Folders can leave notifications in the main loop which would be delivered
	   on idle, but these can be left in the main loop longer than the temporary
	   CamelSession object is alive, which leads to a crash, because of
	   the CamelStore's descendant being freed too early. */
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);

	return TRUE;
}

void
e_convert_local_mail (EShell *shell)
{
	CamelSession *session;
	ESource *mbox_source;
	const gchar *user_data_dir;
	const gchar *user_cache_dir;
	gchar *mail_data_dir;
	gchar *mail_cache_dir;
	gchar *local_store;
	gint response;

	user_data_dir = e_get_user_data_dir ();
	user_cache_dir = e_get_user_cache_dir ();

	mail_data_dir = g_build_filename (user_data_dir, "mail", NULL);
	mail_cache_dir = g_build_filename (user_cache_dir, "mail", NULL);

	if (!mail_to_maildir_migration_needed (mail_data_dir))
		goto exit;

	response = e_alert_run_dialog_for_args (
		e_shell_get_active_window (NULL),
		"mail:ask-migrate-store", NULL);

	if (response == GTK_RESPONSE_CANCEL)
		exit (EXIT_SUCCESS);

	mbox_source = e_source_new (NULL, NULL, NULL);

	rename_mbox_dir (mbox_source, mail_data_dir);

	local_store = g_build_filename (mail_data_dir, "local", NULL);

	if (!g_file_test (local_store, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (local_store, 0700);

	g_free (local_store);

	session = g_object_new (
		CAMEL_TYPE_SESSION,
		"online", FALSE,
		"user-data-dir", mail_data_dir,
		"user-cache-dir", mail_cache_dir,
		NULL);

	migrate_mbox_to_maildir (shell, session, mbox_source);

	g_object_unref (session);

	g_object_unref (mbox_source);

exit:
	g_free (mail_data_dir);
	g_free (mail_cache_dir);
}

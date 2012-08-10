/*
 * e-convert-local-mail.c
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
 */

#include <config.h>

#include <glib/gstdio.h>
#include <camel/camel.h>

#include <libedataserver/e-data-server-util.h>

#include <shell/e-shell.h>
#include <libemail-utils/e-account-utils.h>
#include <libevolution-utils/e-alert-dialog.h>

#define MBOX_UID "local_mbox"

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

	uids = camel_folder_get_uids (fromfolder);
	camel_folder_transfer_messages_to_sync (
		fromfolder, uids, tofolder, FALSE, NULL, NULL, NULL);
	camel_folder_free_uids (fromfolder, uids);

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
			maildir_folder_name = sanitize_maildir_folder_name (fi->full_name);
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
rename_mbox_dir (const gchar *mail_data_dir)
{
	gchar *old_mail_dir;
	gchar *new_mail_dir;
	gboolean need_rename;

	old_mail_dir = g_build_filename (mail_data_dir, "local", NULL);
	new_mail_dir = g_build_filename (mail_data_dir, MBOX_UID, NULL);

	/* Rename if old directory exists and new directory does not. */
	need_rename =
		g_file_test (old_mail_dir, G_FILE_TEST_EXISTS) &&
		!g_file_test (new_mail_dir, G_FILE_TEST_EXISTS);

	if (need_rename)
		g_rename (old_mail_dir, new_mail_dir);

	g_free (old_mail_dir);
	g_free (new_mail_dir);
}

static gboolean
migrate_mbox_to_maildir (CamelSession *session)
{
	CamelService *mbox_service;
	CamelService *maildir_service;
	CamelSettings *settings;
	const gchar *data_dir;
	gchar *path;
	struct MigrateStore ms;
	GError *error = NULL;

	data_dir = camel_session_get_user_data_dir (session);

	mbox_service = camel_session_add_service (
		session, MBOX_UID, "mbox",
		CAMEL_PROVIDER_STORE, &error);

	if (error != NULL) {
		g_warn_if_fail (mbox_service == NULL);
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return FALSE;
	}

	settings = camel_service_get_settings (mbox_service);
	path = g_build_filename (data_dir, MBOX_UID, NULL);
	g_object_set (settings, "path", path, NULL);
	g_free (path);

	maildir_service = camel_session_add_service (
		session, "local", "maildir",
		CAMEL_PROVIDER_STORE, &error);

	if (error != NULL) {
		g_warn_if_fail (maildir_service == NULL);
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_object_unref (mbox_service);
		g_error_free (error);
		return FALSE;
	}

	settings = camel_service_get_settings (maildir_service);
	path = g_build_filename (data_dir, "local", NULL);
	g_object_set (settings, "path", path, NULL);
	g_mkdir (path, 0700);
	g_free (path);

	ms.mail_store = CAMEL_STORE (mbox_service);
	ms.maildir_store = CAMEL_STORE (maildir_service);
	ms.session = session;
	ms.complete = FALSE;

	g_thread_create ((GThreadFunc) migrate_stores, &ms, TRUE, NULL);
	while (!ms.complete)
		g_main_context_iteration (NULL, TRUE);

	return TRUE;
}

static gboolean
create_mbox_account (CamelSession *session)
{
	CamelURL *url;
	EAccountList *accounts;
	EAccount *account;
	const gchar *data_dir;
	gchar *name, *id, *temp, *uri;

	data_dir = camel_session_get_user_data_dir (session);

	account = e_account_new ();
	account->enabled = TRUE;

	g_free (account->uid);
	account->uid = g_strdup ("local_mbox");

	url = camel_url_new ("mbox:", NULL);
	temp = g_build_filename (data_dir, "local_mbox", NULL);
	camel_url_set_path (url, temp);
	g_free (temp);

	uri = camel_url_to_string (url, 0);
	e_account_set_string (account, E_ACCOUNT_SOURCE_URL, uri);

#ifndef G_OS_WIN32
	name = g_locale_to_utf8 (g_get_user_name (), -1, NULL, NULL, NULL);
#else
	name = g_strdup (g_get_user_name ());
#endif

	id = g_strconcat (name, "@", "localhost", NULL);
	e_account_set_string (account, E_ACCOUNT_ID_NAME, name);
	e_account_set_string (account, E_ACCOUNT_ID_ADDRESS, id);
	e_account_set_string (account, E_ACCOUNT_NAME, "local_mbox");

	accounts = e_get_account_list ();
	if (e_account_list_find (accounts, E_ACCOUNT_ID_ADDRESS, id)) {
		g_object_unref (account);
		goto exit;
	}

	e_account_list_add (accounts, account);

exit:
	camel_url_free (url);
	g_free (uri);
	g_free (name);
	g_free (id);

	return TRUE;
}

void
e_convert_local_mail (EShell *shell)
{
	CamelSession *session;
	const gchar *user_data_dir;
	const gchar *user_cache_dir;
	gchar *mail_data_dir;
	gchar *mail_cache_dir;
	gchar *local_store;
	gint response;

	camel_provider_init ();

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

	rename_mbox_dir (mail_data_dir);

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

	migrate_mbox_to_maildir (session);
	create_mbox_account (session);

	g_object_unref (session);

exit:
	g_free (mail_data_dir);
	g_free (mail_cache_dir);
}

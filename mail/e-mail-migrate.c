/*
 * e-mail-migrate.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-migrate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <errno.h>
#include <ctype.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-data-server-util.h>

#include <shell/e-shell.h>
#include <shell/e-shell-migrate.h>

#include <e-util/e-util.h>
#include <libevolution-utils/e-xml-utils.h>

#include <libevolution-utils/e-alert-dialog.h>
#include <e-util/e-util-private.h>
#include <e-util/e-plugin.h>

#include <libemail-utils/e-account-utils.h>
#include <libemail-utils/e-signature-utils.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include "e-mail-backend.h"
#include "em-utils.h"

#define d(x) x

/* 1.4 upgrade functions */

#define EM_TYPE_MIGRATE_SESSION \
	(em_migrate_session_get_type ())

typedef struct _EMMigrateSession {
	CamelSession parent_object;

	CamelStore *store;   /* new folder tree store */
	gchar *srcdir;        /* old folder tree path */
} EMMigrateSession;

typedef struct _EMMigrateSessionClass {
	CamelSessionClass parent_class;

} EMMigrateSessionClass;

GType em_migrate_session_get_type (void);

G_DEFINE_TYPE (EMMigrateSession, em_migrate_session, CAMEL_TYPE_SESSION)

static void
em_migrate_session_class_init (EMMigrateSessionClass *class)
{
}

static void
em_migrate_session_init (EMMigrateSession *session)
{
}

static EMMigrateSession *
em_migrate_session_new (const gchar *user_data_dir)
{
	const gchar *user_cache_dir;

	/* FIXME Really need to kill this function and
	 *       get the cache dir from EShellBackend. */
	user_cache_dir = mail_session_get_cache_dir ();

	return g_object_new (
		EM_TYPE_MIGRATE_SESSION,
		"user-data-dir", user_data_dir,
		"user-cache-dir", user_cache_dir, NULL);
}

static GtkProgressBar *progress;

static void
em_migrate_set_progress (double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

enum {
	CP_UNIQUE = 0,
	CP_OVERWRITE,
	CP_APPEND
};

static gint open_flags[3] = {
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_TRUNC,
	O_WRONLY | O_CREAT | O_APPEND,
};

static gboolean
cp (const gchar *src,
    const gchar *dest,
    gboolean show_progress,
    gint mode)
{
	guchar readbuf[65536];
	gssize nread, nwritten;
	gint errnosav, readfd, writefd;
	gsize total = 0;
	struct stat st;
	struct utimbuf ut;

	/* if the dest file exists and has content, abort - we don't
	 * want to corrupt their existing data */
	if (g_stat (dest, &st) == 0 && st.st_size > 0 && mode == CP_UNIQUE) {
		errno = EEXIST;
		return FALSE;
	}

	if (g_stat (src, &st) == -1
	    || (readfd = g_open (src, O_RDONLY | O_BINARY, 0)) == -1)
		return FALSE;

	if ((writefd = g_open (dest, open_flags[mode] | O_BINARY, 0666)) == -1) {
		errnosav = errno;
		close (readfd);
		errno = errnosav;
		return FALSE;
	}

	do {
		do {
			nread = read (readfd, readbuf, sizeof (readbuf));
		} while (nread == -1 && errno == EINTR);

		if (nread == 0)
			break;
		else if (nread < 0)
			goto exception;

		do {
			nwritten = write (writefd, readbuf, nread);
		} while (nwritten == -1 && errno == EINTR);

		if (nwritten < nread)
			goto exception;

		total += nwritten;
		if (show_progress)
			em_migrate_set_progress (((gdouble) total) / ((gdouble) st.st_size));
	} while (total < st.st_size);

	if (fsync (writefd) == -1)
		goto exception;

	close (readfd);
	if (close (writefd) == -1)
		goto failclose;

	ut.actime = st.st_atime;
	ut.modtime = st.st_mtime;
	utime (dest, &ut);
	chmod (dest, st.st_mode);

	return TRUE;

 exception:

	errnosav = errno;
	close (readfd);
	close (writefd);
	errno = errnosav;

 failclose:

	errnosav = errno;
	unlink (dest);
	errno = errnosav;

	return FALSE;
}

#ifndef G_OS_WIN32

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

static void
em_update_accounts_2_11 (void)
{
	EAccountList *accounts;
	EIterator *iter;
	gboolean changed = FALSE;

	if (!(accounts = e_get_account_list ()))
		return;

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);

		if (g_str_has_prefix (account->source->url, "spool://")) {
			if (g_file_test (account->source->url + 8, G_FILE_TEST_IS_DIR)) {
				gchar *str;

				str = g_strdup_printf (
					"spooldir://%s",
					account->source->url + 8);

				g_free (account->source->url);
				account->source->url = str;
				changed = TRUE;
			}
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	if (changed)
		e_account_list_save (accounts);
}

#endif	/* !G_OS_WIN32 */

static gboolean
emm_setup_initial (const gchar *data_dir)
{
	GDir *dir;
	const gchar *d;
	gchar *local = NULL, *base;
	const gchar * const *language_names;

	/* special-case - this means brand new install of evolution */
	/* FIXME: create default folders and stuff... */

	d(printf("Setting up initial mail tree\n"));

	base = g_build_filename (data_dir, "local", NULL);
	if (g_mkdir_with_parents (base, 0700) == -1 && errno != EEXIST) {
		g_free (base);
		return FALSE;
	}

	/* e.g. try en-AU then en, etc */
	language_names = g_get_language_names ();
	while (*language_names != NULL) {
		local = g_build_filename (
			EVOLUTION_PRIVDATADIR, "default",
			*language_names, "mail", "local", NULL);
		if (g_file_test (local, G_FILE_TEST_EXISTS))
			break;
		g_free (local);
		language_names++;
	}

	/* Make sure we found one. */
	g_return_val_if_fail (*language_names != NULL, FALSE);

	dir = g_dir_open (local, 0, NULL);
	if (dir) {
		while ((d = g_dir_read_name (dir))) {
			gchar *src, *dest;

			src = g_build_filename (local, d, NULL);
			dest = g_build_filename (base, d, NULL);

			cp (src, dest, FALSE, CP_UNIQUE);
			g_free (dest);
			g_free (src);
		}
		g_dir_close (dir);
	}

	g_free (local);
	g_free (base);

	return TRUE;
}

static gboolean
is_in_plugs_list (GSList *list,
                  const gchar *value)
{
	GSList *l;

	for (l = list; l; l = l->next) {
		if (l->data && !strcmp (l->data, value))
			return TRUE;
	}

	return FALSE;
}

/*
 * em_update_message_notify_settings_2_21
 * DBus plugin and sound email notification was merged to
 * mail-notification plugin, so move the options to new locations.
 */
static void
em_update_message_notify_settings_2_21 (void)
{
	GConfClient *client;
	GConfValue  *is_key;
	gboolean dbus, status;
	GSList *list;
	gchar *str;
	gint val;

	client = gconf_client_get_default ();

	is_key = gconf_client_get (
		client,
		"/apps/evolution/eplugin/mail-notification/dbus-enabled", NULL);
	if (is_key) {
		/* already migrated, so do not migrate again */
		gconf_value_free (is_key);
		g_object_unref (client);

		return;
	}

	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-blink-icon",
		gconf_client_get_bool (
			client,
			"/apps/evolution/mail/notification/blink-status-icon",
			NULL), NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-notification",
		gconf_client_get_bool (
			client,
			"/apps/evolution/mail/notification/notification",
			NULL), NULL);

	list = gconf_client_get_list (
		client, "/apps/evolution/eplugin/disabled",
		GCONF_VALUE_STRING, NULL);
	dbus = !is_in_plugs_list (list, "org.gnome.evolution.new_mail_notify");
	status = !is_in_plugs_list (
		list, "org.gnome.evolution.mail_notification");

	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/dbus-enabled",
		dbus, NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/status-enabled",
		status, NULL);

	if (!status) {
		GSList *plugins, *l;

		plugins = e_plugin_list_plugins ();

		for (l = plugins; l; l = l->next) {
			EPlugin *p = l->data;

			if (p && p->id && !strcmp (p->id,
				"org.gnome.evolution.mail_notification")) {
				e_plugin_enable (p, 1);
				break;
			}
		}

		g_slist_foreach (plugins, (GFunc) g_object_unref, NULL);
		g_slist_free (plugins);
	}

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	val = gconf_client_get_int (
		client, "/apps/evolution/mail/notify/type", NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-enabled",
		val == 1 || val == 2, NULL);
	gconf_client_set_bool (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-beep",
		val == 0 || val == 1, NULL);

	str = gconf_client_get_string (
		client, "/apps/evolution/mail/notify/sound", NULL);
	gconf_client_set_string (
		client,
		"/apps/evolution/eplugin/mail-notification/sound-file",
		str ? str : "", NULL);
	g_free (str);

	g_object_unref (client);
}

/* fixing typo in SpamAssassin name */
static void
em_update_sa_junk_setting_2_23 (void)
{
	GConfClient *client;
	GConfValue  *key;

	client = gconf_client_get_default ();

	key = gconf_client_get (
		client, "/apps/evolution/mail/junk/default_plugin", NULL);
	if (key) {
		const gchar *str = gconf_value_get_string (key);

		if (str && strcmp (str, "Spamassasin") == 0)
			gconf_client_set_string (
				client,
				"/apps/evolution/mail/junk/default_plugin",
				"SpamAssassin", NULL);

		gconf_value_free (key);
		g_object_unref (client);

		return;
	}

	g_object_unref (client);
}

static gboolean
mbox_to_maildir_migration_needed (EShellBackend *shell_backend)
{
	gchar *local_store;
	gchar *local_outbox;
	const gchar *data_dir;
	gboolean migration_needed = FALSE;

	data_dir = e_shell_backend_get_data_dir (shell_backend);

	local_store = g_build_filename (data_dir, "local", NULL);
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
copy_folder (CamelStore *mbox_store,
             CamelStore *maildir_store,
             const gchar *mbox_fname,
             const gchar *maildir_fname)
{
	CamelFolder *fromfolder, *tofolder;
	GPtrArray *uids;

	fromfolder = camel_store_get_folder_sync (
		mbox_store, mbox_fname, 0, NULL, NULL);
	if (fromfolder == NULL) {
		g_warning ("Cannot find mbox folder %s \n", mbox_fname);
		return;
	}

	tofolder = camel_store_get_folder_sync (
		maildir_store, maildir_fname,
		CAMEL_STORE_FOLDER_CREATE,
		NULL, NULL);
	if (tofolder == NULL) {
		g_warning ("Cannot create maildir folder %s \n", maildir_fname);
		g_object_unref (fromfolder);
		return;
	}

	uids = camel_folder_get_uids (fromfolder);
	camel_folder_transfer_messages_to_sync (
			fromfolder, uids, tofolder,
			FALSE, NULL,
			NULL, NULL);
	camel_folder_free_uids (fromfolder, uids);

	g_object_unref (fromfolder);
	g_object_unref (tofolder);
}

static void
copy_folders (CamelStore *mbox_store,
              CamelStore *maildir_store,
              CamelFolderInfo *fi,
              EMMigrateSession *session)
{
	if (fi) {
		if (!g_str_has_prefix (fi->full_name, ".#evolution")) {
			gchar *maildir_folder_name;

			/* sanitize folder names and copy folders */
			maildir_folder_name = sanitize_maildir_folder_name (fi->full_name);
			copy_folder (
				mbox_store, maildir_store,
				fi->full_name, maildir_folder_name);
			g_free (maildir_folder_name);
		}

		if (fi->child)
			copy_folders (mbox_store, maildir_store, fi->child, session);

		copy_folders (mbox_store, maildir_store, fi->next, session);
	}
}

struct MigrateStore {
	EMMigrateSession *session;
	CamelStore *mbox_store;
	CamelStore *maildir_store;
	gboolean complete;
};

static void
migrate_stores (struct MigrateStore *ms)
{
	CamelFolderInfo *mbox_fi;
	CamelStore *mbox_store = ms->mbox_store;
	CamelStore *maildir_store = ms->maildir_store;

	mbox_fi = camel_store_get_folder_info_sync (
		mbox_store, NULL,
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
		NULL, NULL);

	/* FIXME progres dialog */
	copy_folders (mbox_store, maildir_store, mbox_fi, ms->session);
	ms->complete = TRUE;

	return;
}

static gboolean
migrate_mbox_to_maildir (EShellBackend *shell_backend,
                         EMMigrateSession *session)
{
	CamelService *mbox_service, *maildir_service;
	CamelStore *mbox_store, *maildir_store;
	CamelSettings *settings;
	const gchar *data_dir;
	gchar *path;
	struct MigrateStore ms;

	data_dir = e_shell_backend_get_data_dir (shell_backend);

	mbox_service = camel_session_add_service (
		CAMEL_SESSION (session), "local_mbox", "mbox",
		CAMEL_PROVIDER_STORE, NULL);

	settings = camel_service_get_settings (mbox_service);
	path = g_build_filename (data_dir, "local_mbox", NULL);
	g_object_set (settings, "path", path, NULL);
	g_free (path);

	maildir_service = camel_session_add_service (
		CAMEL_SESSION (session), "local", "maildir",
		CAMEL_PROVIDER_STORE, NULL);

	settings = camel_service_get_settings (maildir_service);
	path = g_build_filename (data_dir, "local", NULL);
	g_object_set (settings, "path", path, NULL);
	g_mkdir (path, 0700);
	g_free (path);

	mbox_store = CAMEL_STORE (mbox_service);
	maildir_store = CAMEL_STORE (maildir_service);

	ms.mbox_store = mbox_store;
	ms.maildir_store = maildir_store;
	ms.session = session;
	ms.complete = FALSE;

	g_thread_create ((GThreadFunc) migrate_stores, &ms, TRUE, NULL);
	while (!ms.complete)
		g_main_context_iteration (NULL, TRUE);

	return TRUE;
}

static void
rename_mbox_dir (EShellBackend *shell_backend)
{
	gchar *local_mbox_path, *new_mbox_path;
	const gchar *data_dir;

	data_dir = e_shell_backend_get_data_dir (shell_backend);
	local_mbox_path = g_build_filename (data_dir, "local", NULL);
	new_mbox_path = g_build_filename (data_dir, "local_mbox", NULL);

	if (!g_file_test (local_mbox_path, G_FILE_TEST_EXISTS))
		goto exit;

	if (g_file_test (new_mbox_path, G_FILE_TEST_EXISTS))
		goto exit;

	g_rename (local_mbox_path, new_mbox_path);

exit:
	g_free (local_mbox_path);
	g_free (new_mbox_path);
}

static gboolean
create_mbox_account (EShellBackend *shell_backend,
                     EMMigrateSession *session)
{
	EMailBackend *mail_backend;
	EMailSession *mail_session;
	CamelService *service;
	CamelURL *url;
	EAccountList *accounts;
	EAccount *account;
	const gchar *data_dir;
	gchar *name, *id, *temp, *uri, *folder_uri;

	mail_backend = E_MAIL_BACKEND (shell_backend);
	mail_session = e_mail_backend_get_session (mail_backend);
	data_dir = e_shell_backend_get_data_dir (shell_backend);

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
	e_account_set_string (account, E_ACCOUNT_NAME, id);

	accounts = e_get_account_list ();
	if (e_account_list_find (accounts, E_ACCOUNT_ID_ADDRESS, id)) {
		g_object_unref (account);
		goto exit;
	}

	/* This will also add it to the EMailSession. */
	e_account_list_add (accounts, account);

	service = camel_session_get_service (
		CAMEL_SESSION (mail_session), account->uid);

	folder_uri = e_mail_folder_uri_build (
		CAMEL_STORE (service), "Sent");
	e_account_set_string (
		account, E_ACCOUNT_SENT_FOLDER_URI, folder_uri);
	g_free (folder_uri);

	folder_uri = e_mail_folder_uri_build (
		CAMEL_STORE (service), "Drafts");
	e_account_set_string (
		account, E_ACCOUNT_DRAFTS_FOLDER_URI, folder_uri);
	g_free (folder_uri);

	e_account_list_save (accounts);

exit:
	camel_url_free (url);
	g_free (uri);
	g_free (name);
	g_free (id);

	return TRUE;
}

static void
change_sent_and_drafts_local_folders (EShellBackend *shell_backend)
{
	EMailBackend *backend;
	EMailSession *session;
	EAccountList *accounts;
	EIterator *iter;
	const gchar *data_dir;
	gboolean changed = FALSE;
	CamelURL *url;
	gchar *tmp_uri, *drafts_uri, *sent_uri, *old_drafts_uri, *old_sent_uri;

	accounts = e_get_account_list ();
	if (!accounts)
		return;

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	data_dir = e_shell_backend_get_data_dir (shell_backend);

	tmp_uri = g_strconcat ("mbox:", data_dir, "/", "local", NULL);
	url = camel_url_new (tmp_uri, NULL);
	g_free (tmp_uri);

	g_return_if_fail (url != NULL);

	camel_url_set_fragment (url, "Drafts");
	drafts_uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	camel_url_set_fragment (url, "Sent");
	sent_uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	camel_url_free (url);

	tmp_uri = g_strconcat (
		"mbox:", g_get_home_dir (),
		"/.evolution/mail/local", NULL);
	url = camel_url_new (tmp_uri, NULL);
	g_free (tmp_uri);

	g_return_if_fail (url != NULL);

	camel_url_set_fragment (url, "Drafts");
	old_drafts_uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	camel_url_set_fragment (url, "Sent");
	old_sent_uri = camel_url_to_string (url, CAMEL_URL_HIDE_ALL);

	camel_url_free (url);

	for (iter = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (iter); e_iterator_next (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);
		const gchar *uri;

		if (!account)
			continue;

		uri = e_account_get_string (account, E_ACCOUNT_DRAFTS_FOLDER_URI);
		if (g_strcmp0 (uri, drafts_uri) == 0 ||
		    g_strcmp0 (uri, old_drafts_uri) == 0) {
			changed = TRUE;
			e_account_set_string (
				account, E_ACCOUNT_DRAFTS_FOLDER_URI,
				e_mail_session_get_local_folder_uri (
				session, E_MAIL_LOCAL_FOLDER_DRAFTS));
		}

		uri = e_account_get_string (account, E_ACCOUNT_SENT_FOLDER_URI);
		if (g_strcmp0 (uri, sent_uri) == 0 || g_strcmp0 (uri, old_sent_uri) == 0) {
			changed = TRUE;
			e_account_set_string (
				account, E_ACCOUNT_SENT_FOLDER_URI,
				e_mail_session_get_local_folder_uri (
				session, E_MAIL_LOCAL_FOLDER_SENT));
		}
	}

	g_object_unref (iter);
	g_free (old_drafts_uri);
	g_free (drafts_uri);
	g_free (old_sent_uri);
	g_free (sent_uri);

	if (changed)
		e_account_list_save (accounts);
}

static void
em_rename_camel_url_params (CamelURL *url)
{
	/* This list includes known URL parameters from built-in providers
	 * in Camel, as well as from evolution-exchange, evolution-groupwise,
	 * and evolution-mapi.  Add more as needed. */
	static struct {
		const gchar *url_parameter;
		const gchar *property_name;
	} camel_url_conversion[] = {
		{ "account_uid",		"account-uid" },
		{ "ad_auth",			"gc-auth-method" },
		{ "ad_browse",			"gc-allow-browse" },
		{ "ad_expand_groups",		"gc-expand-groups" },
		{ "ad_limit",			"gc-results-limit" },
		{ "ad_server",			"gc-server-name" },
		{ "all_headers",		"fetch-headers" },
		{ "basic_headers",		"fetch-headers" },
		{ "cachedconn"			"concurrent-connections" },
		{ "check_all",			"check-all" },
		{ "check_lsub",			"check-subscribed" },
		{ "command",			"shell-command" },
		{ "delete_after",		"delete-after-days" },
		{ "delete_expunged",		"delete-expunged" },
		{ "disable_extensions",		"disable-extensions" },
		{ "dotfolders",			"use-dot-folders" },
		{ "filter",			"filter-inbox" },
		{ "filter_junk",		"filter-junk" },
		{ "filter_junk_inbox",		"filter-junk-inbox" },
		{ "folder_hierarchy_relative",	"folder-hierarchy-relative" },
		{ "imap_custom_headers",	"fetch-headers-extra" },
		{ "keep_on_server",		"keep-on-server" },
		{ "oab_offline",		"oab-offline" },
		{ "oal_selected",		"oal-selected" },
		{ "offline_sync",		"stay-synchronized" },
		{ "override_namespace",		"use-namespace" },
		{ "owa_path",			"owa-path" },
		{ "owa_url",			"owa-url" },
		{ "password_exp_warn_period",	"password-exp-warn-period" },
		{ "real_junk_path",		"real-junk-path" },
		{ "real_trash_path",		"real-trash-path" },
		{ "show_short_notation",	"short-folder-names" },
		{ "soap_port",			"soap-port" },
		{ "ssl",			"security-method" },
		{ "sync_offline",		"stay-synchronized" },
		{ "use_command",		"use-shell-command" },
		{ "use_idle",			"use-idle" },
		{ "use_lsub",			"use-subscriptions" },
		{ "use_qresync",		"use-qresync" },
		{ "use_ssl",			"security-method" },
		{ "xstatus",			"use-xstatus-headers" }
	};

	const gchar *param;
	const gchar *use_param;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (camel_url_conversion); ii++) {
		const gchar *key;
		gpointer value;

		key = camel_url_conversion[ii].url_parameter;
		value = g_datalist_get_data (&url->params, key);

		if (value == NULL)
			continue;

		g_datalist_remove_no_notify (&url->params, key);

		key = camel_url_conversion[ii].property_name;

		/* Deal with a few special enum cases where
		 * the parameter value also needs renamed. */

		if (strcmp (key, "all_headers") == 0) {
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_FETCH_HEADERS_TYPE);
			enum_value = g_enum_get_value (
				enum_class, CAMEL_FETCH_HEADERS_ALL);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		if (strcmp (key, "basic_headers") == 0) {
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_FETCH_HEADERS_TYPE);
			enum_value = g_enum_get_value (
				enum_class, CAMEL_FETCH_HEADERS_BASIC);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		if (strcmp (key, "imap_custom_headers") == 0)
			g_strdelimit (value, " ", ',');

		if (strcmp (key, "security-method") == 0) {
			CamelNetworkSecurityMethod method;
			GEnumClass *enum_class;
			GEnumValue *enum_value;

			if (strcmp (value, "always") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			else if (strcmp (value, "1") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_SSL_ON_ALTERNATE_PORT;
			else if (strcmp (value, "when-possible") == 0)
				method = CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT;
			else
				method = CAMEL_NETWORK_SECURITY_METHOD_NONE;

			enum_class = g_type_class_ref (
				CAMEL_TYPE_NETWORK_SECURITY_METHOD);
			enum_value = g_enum_get_value (enum_class, method);
			if (enum_value != NULL) {
				g_free (value);
				value = g_strdup (enum_value->value_nick);
			} else
				g_warn_if_reached ();
			g_type_class_unref (enum_class);
		}

		g_datalist_set_data_full (&url->params, key, value, g_free);
	}

	/* A few more adjustments...
	 *
	 * These are all CAMEL_PROVIDER_CONF_CHECKSPIN settings.  The spin
	 * button value is bound to "param" and the checkbox state is bound
	 * to "use-param".  The "use-param" settings are new.  If "param"
	 * exists but no "use-param", then set "use-param" to "true". */

	param = g_datalist_get_data (&url->params, "gc-results-limit");
	use_param = g_datalist_get_data (&url->params, "use-gc-results-limit");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-gc-results-limit",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "kerberos");
	if (g_strcmp0 (param, "required") == 0) {
		g_datalist_set_data_full (
			&url->params, "kerberos",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (
		&url->params, "password-exp-warn-period");
	use_param = g_datalist_get_data (
		&url->params, "use-password-exp-warn-period");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-password-exp-warn-period",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "real-junk-path");
	use_param = g_datalist_get_data (&url->params, "use-real-junk-path");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-real-junk-path",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}

	param = g_datalist_get_data (&url->params, "real-trash-path");
	use_param = g_datalist_get_data (&url->params, "use-real-trash-path");
	if (param != NULL && *param != '\0' && use_param == NULL) {
		g_datalist_set_data_full (
			&url->params, "use-real-trash-path",
			g_strdup ("true"), (GDestroyNotify) g_free);
	}
}

static void
em_rename_account_params (void)
{
	EAccountList *account_list;
	EIterator *iterator;

	/* XXX As of 3.2, CamelServices store settings in GObject properties,
	 *     not CamelURL parameters.  CamelURL parameters are still used
	 *     for storage in GConf until we can move account information to
	 *     key files, but this is only within Evolution.  Some of the new
	 *     GObject property names differ from the old CamelURL parameter
	 *     names.  This routine renames the CamelURL parameter names to
	 *     the GObject property names for all accounts, both the source
	 *     and tranport URLs. */

	account_list = e_get_account_list ();
	iterator = e_list_get_iterator (E_LIST (account_list));

	while (e_iterator_is_valid (iterator)) {
		EAccount *account;
		CamelURL *url = NULL;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (iterator);

		if (account->source->url != NULL)
			url = camel_url_new (account->source->url, NULL);

		if (url != NULL) {
			em_rename_camel_url_params (url);
			g_free (account->source->url);
			account->source->url = camel_url_to_string (url, 0);
			camel_url_free (url);
		}

		url = NULL;

		if (account->transport->url != NULL)
			url = camel_url_new (account->transport->url, NULL);

		if (url != NULL) {
			em_rename_camel_url_params (url);
			g_free (account->transport->url);
			account->transport->url = camel_url_to_string (url, 0);
			camel_url_free (url);
		}

		e_iterator_next (iterator);
	}

	g_object_unref (iterator);
	e_account_list_save (account_list);
}

static gboolean
migrate_local_store (EShellBackend *shell_backend)
{
	EMMigrateSession *session;
	const gchar *data_dir;
	gchar *local_store;
	gint response;

	if (!mbox_to_maildir_migration_needed (shell_backend))
		return TRUE;

	response = e_alert_run_dialog_for_args (
		e_shell_get_active_window (NULL),
		"mail:ask-migrate-store", NULL);

	if (response == GTK_RESPONSE_CANCEL)
		exit (EXIT_SUCCESS);

	rename_mbox_dir (shell_backend);
	data_dir = e_shell_backend_get_data_dir (shell_backend);
	local_store = g_build_filename (data_dir, "local", NULL);

	if (!g_file_test (local_store, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (local_store, 0700);

	session = em_migrate_session_new (data_dir);
	camel_session_set_online (CAMEL_SESSION (session), FALSE);

	migrate_mbox_to_maildir (shell_backend, session);
	create_mbox_account (shell_backend, session);
	change_sent_and_drafts_local_folders (shell_backend);

	g_object_unref (session);

	g_free (local_store);

	return TRUE;
}

static void
em_ensure_proxy_ignore_hosts_being_list (void)
{
	const gchar *key = "/apps/evolution/shell/network_config/ignore_hosts";
	GConfClient *client;
	GConfValue  *key_value;

	/* Makes sure the 'key' is a list of strings, not a string,
	 * as set by previous versions. */

	client = gconf_client_get_default ();
	key_value = gconf_client_get (client, key, NULL);
	if (key_value && key_value->type == GCONF_VALUE_STRING) {
		gchar *value = gconf_client_get_string (client, key, NULL);
		GSList *lst = NULL;
		GError *error = NULL;

		if (value && *value) {
			gchar **split = g_strsplit (value, ",", -1);

			if (split) {
				gint ii;

				for (ii = 0; split[ii]; ii++) {
					const gchar *tmp = split[ii];

					if (tmp && *tmp) {
						gchar *val = g_strstrip (g_strdup (tmp));

						if (val && *val)
							lst = g_slist_append (lst, val);
						else
							g_free (val);
					}
				}
			}

			g_strfreev (split);
		}

		gconf_client_unset (client, key, NULL);
		gconf_client_set_list (client, key, GCONF_VALUE_STRING, lst, &error);

		g_slist_foreach (lst, (GFunc) g_free, NULL);
		g_slist_free (lst);
		g_free (value);

		if (error) {
			fprintf (
				stderr, "%s: Failed to set a list values "
				"with error: %s\n", G_STRFUNC, error->message);
			g_error_free (error);
		}
	}

	if (key_value)
		gconf_value_free (key_value);
	g_object_unref (client);
}

static void
em_rename_view_in_folder (gpointer data,
                          gpointer user_data)
{
	const gchar *filename = data;
	const gchar *views_dir = user_data;
	gchar *folderpos, *dotpos;

	g_return_if_fail (filename != NULL);
	g_return_if_fail (views_dir != NULL);

	folderpos = strstr (filename, "-folder:__");
	if (!folderpos)
		folderpos = strstr (filename, "-folder___");
	if (!folderpos)
		return;

	/* points on 'f' from the "folder" word */
	folderpos++;
	dotpos = strrchr (filename, '.');
	if (folderpos < dotpos && g_str_equal (dotpos, ".xml")) {
		GChecksum *checksum;
		gchar *oldname, *newname, *newfile;
		const gchar *md5_string;

		*dotpos = 0;

		/* use MD5 checksum of the folder URI, to not depend on its length */
		checksum = g_checksum_new (G_CHECKSUM_MD5);
		g_checksum_update (checksum, (const guchar *) folderpos, -1);

		*folderpos = 0;
		md5_string = g_checksum_get_string (checksum);
		newfile = g_strconcat (filename, md5_string, ".xml", NULL);
		*folderpos = 'f';
		*dotpos = '.';

		oldname = g_build_filename (views_dir, filename, NULL);
		newname = g_build_filename (views_dir, newfile, NULL);

		g_rename (oldname, newname);

		g_checksum_free (checksum);
		g_free (oldname);
		g_free (newname);
		g_free (newfile);
	}
}

static void
em_rename_folder_views (EShellBackend *shell_backend)
{
	const gchar *config_dir;
	gchar *views_dir;
	GDir *dir;

	g_return_if_fail (shell_backend != NULL);

	config_dir = e_shell_backend_get_config_dir (shell_backend);
	views_dir = g_build_filename (config_dir, "views", NULL);

	dir = g_dir_open (views_dir, 0, NULL);
	if (dir) {
		GSList *to_rename = NULL;
		const gchar *filename;

		while (filename = g_dir_read_name (dir), filename) {
			if (strstr (filename, "-folder:__") ||
			    strstr (filename, "-folder___"))
				to_rename = g_slist_prepend (to_rename, g_strdup (filename));
		}

		g_dir_close (dir);

		g_slist_foreach (to_rename, em_rename_view_in_folder, views_dir);
		g_slist_free_full (to_rename, g_free);
	}

	g_free (views_dir);
}

gboolean
e_mail_migrate (EShellBackend *shell_backend,
                gint major,
                gint minor,
                gint micro,
                GError **error)
{
	struct stat st;
	const gchar *data_dir;

	/* make sure ~/.evolution/mail exists */
	data_dir = e_shell_backend_get_data_dir (shell_backend);
	if (g_stat (data_dir, &st) == -1) {
		if (errno != ENOENT || g_mkdir_with_parents (data_dir, 0700) == -1) {
			g_set_error (
				error, E_SHELL_MIGRATE_ERROR,
				E_SHELL_MIGRATE_ERROR_FAILED,
				_("Unable to create local mail folders at "
				"'%s': %s"), data_dir, g_strerror (errno));
			return FALSE;
		}
	}

	if (major == 0)
		return emm_setup_initial (data_dir);

#ifndef G_OS_WIN32
	if (major < 2 || (major == 2 && minor < 12)) {
		em_update_accounts_2_11 ();
	}

	if (major < 2 || (major == 2 && minor < 22))
		em_update_message_notify_settings_2_21 ();

	if (major < 2 || (major == 2 && minor < 24))
		em_update_sa_junk_setting_2_23 ();
#else
	if (major < 2 || (major == 2 && minor < 24))
		g_warning (
			"Upgrading from ancient versions %d.%d "
			"not supported on Windows", major, minor);
#endif

	if (major < 2 || (major == 2 && minor < 32)) {
		em_ensure_proxy_ignore_hosts_being_list ();
	}

	/* Rename account URL parameters to
	 * match CamelSettings property names. */
	em_rename_account_params ();

	if (!migrate_local_store (shell_backend))
		return FALSE;

	if (major <= 2 || (major == 3 && minor < 4))
		em_rename_folder_views (shell_backend);

	return TRUE;
}

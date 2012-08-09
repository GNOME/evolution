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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <shell/e-shell.h>
#include <shell/e-shell-migrate.h>

#include <e-util/e-util.h>
#include <libevolution-utils/e-xml-utils.h>

#include <libevolution-utils/e-alert-dialog.h>
#include <e-util/e-util-private.h>
#include <e-util/e-plugin.h>

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
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	ESourceExtension *extension;
	const gchar *extension_name;
	CamelService *mbox_service;
	CamelService *maildir_service;
	CamelStore *mbox_store;
	CamelStore *maildir_store;
	CamelSettings *settings;
	const gchar *data_dir;
	const gchar *uid;
	gchar *path;
	struct MigrateStore ms;
	GError *error = NULL;

	data_dir = e_shell_backend_get_data_dir (shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	source = e_source_new (NULL, NULL, NULL);
	e_source_set_display_name (source, "local_mbox");

	uid = e_source_get_uid (source);

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	extension = e_source_get_extension (source, extension_name);

	e_source_backend_set_backend_name (
		E_SOURCE_BACKEND (extension), "mbox");

	extension_name = e_source_camel_get_extension_name ("mbox");
	extension = e_source_get_extension (source, extension_name);
	settings = e_source_camel_get_settings (E_SOURCE_CAMEL (extension));

	path = g_build_filename (data_dir, "local_mbox", NULL);
	g_object_set (settings, "path", path, NULL);
	g_free (path);

	e_source_registry_commit_source_sync (
		registry, source, NULL, &error);

	if (error == NULL)
		mbox_service = camel_session_add_service (
			CAMEL_SESSION (session), uid, "mbox",
			CAMEL_PROVIDER_STORE, &error);

	if (error == NULL)
		maildir_service = camel_session_add_service (
			CAMEL_SESSION (session), "local", "maildir",
			CAMEL_PROVIDER_STORE, &error);

	g_object_unref (source);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		return FALSE;
	}

	camel_service_set_settings (mbox_service, settings);

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

	g_object_unref (session);

	g_free (local_store);

	return TRUE;
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

	if (!migrate_local_store (shell_backend))
		return FALSE;

	if (major <= 2 || (major == 3 && minor < 4))
		em_rename_folder_views (shell_backend);

	return TRUE;
}

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
 *		Iain Holmes <iain@ximian.com>
 *      Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "e-util/e-util-private.h"
#include "shell/e-shell-backend.h"

#include "mail-importer.h"
#include "kmail-libs.h"

struct _import_mbox_msg {
	MailMsg base;

	EMailSession *session;
	gchar *path;
	gchar *uri;

	void (*done)(gpointer data, GError **error);
	gpointer done_data;
};

static gchar *
import_mbox_desc (struct _import_mbox_msg *m)
{
	return g_strdup (_("Importing mailbox"));
}

static gchar *
import_kmail_desc (struct _import_mbox_msg *m)
{
       return g_strdup (_("Importing mail and contacts from KMail"));
}

static struct {
	gchar tag;
	guint32 mozflag;
	guint32 flag;
} status_flags[] = {
	{ 'F', MSG_FLAG_MARKED, CAMEL_MESSAGE_FLAGGED },
	{ 'A', MSG_FLAG_REPLIED, CAMEL_MESSAGE_ANSWERED },
	{ 'D', MSG_FLAG_EXPUNGED, CAMEL_MESSAGE_DELETED },
	{ 'R', MSG_FLAG_READ, CAMEL_MESSAGE_SEEN },
};

static guint32
decode_status (const gchar *status)
{
	const gchar *p;
	guint32 flags = 0;
	gint i;

	p = status;
	while ((*p++)) {
		for (i = 0; i < G_N_ELEMENTS (status_flags); i++)
			if (status_flags[i].tag == *p)
				flags |= status_flags[i].flag;
	}

	return flags;
}

static guint32
decode_mozilla_status (const gchar *tmp)
{
	gulong status = strtoul (tmp, NULL, 16);
	guint32 flags = 0;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (status_flags); i++)
		if (status_flags[i].mozflag & status)
			flags |= status_flags[i].flag;
	return flags;
}

static void
import_mbox_add_message (CamelFolder *folder,
			 CamelMimeMessage *msg,
			 GCancellable *cancellable,
			 GError **error)
{
	CamelMessageInfo *info;
	CamelMedium *medium;
	guint32 flags = 0;
	const gchar *tmp;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	medium = CAMEL_MEDIUM (msg);

	tmp = camel_medium_get_header (medium, "X-Mozilla-Status");
	if (tmp)
		flags |= decode_mozilla_status (tmp);
	tmp = camel_medium_get_header (medium, "Status");
	if (tmp)
		flags |= decode_status (tmp);
	tmp = camel_medium_get_header (medium, "X-Status");
	if (tmp)
		flags |= decode_status (tmp);

	info = camel_message_info_new (NULL);

	camel_message_info_set_flags (info, flags, ~0);
	camel_folder_append_message_sync (
		folder, msg, info, NULL,
		cancellable, error);
	g_clear_object (&info);
}

static void
import_mbox_exec (struct _import_mbox_msg *m,
                  GCancellable *cancellable,
                  GError **error)
{
	CamelFolder *folder;
	struct stat st;
	gint fd;

	if (g_stat (m->path, &st) == -1) {
		g_warning (
			"cannot find source file to import '%s': %s",
			m->path, g_strerror (errno));
		return;
	}

	if (m->uri == NULL || m->uri[0] == 0) {
		folder = e_mail_session_get_local_folder (
			m->session, E_MAIL_LOCAL_FOLDER_INBOX);
		if (folder)
			g_object_ref (folder);
	} else {
		folder = e_mail_session_uri_to_folder_sync (
			m->session, m->uri, CAMEL_STORE_FOLDER_CREATE,
			cancellable, error);
	}

	if (folder == NULL)
		return;

	if (S_ISREG (st.st_mode)) {
		CamelMimeParser *mp = NULL;
		gboolean any_read = FALSE;

		fd = g_open (m->path, O_RDONLY | O_BINARY, 0);
		if (fd == -1) {
			g_warning (
				"cannot find source file to import '%s': %s",
				m->path, g_strerror (errno));
			goto fail1;
		}

		camel_operation_push_message (
			cancellable, _("Importing “%s”"),
			camel_folder_get_display_name (folder));

		camel_folder_freeze (folder);

		if (mail_importer_file_is_mbox (m->path)) {
			mp = camel_mime_parser_new ();
			camel_mime_parser_scan_from (mp, TRUE);
			if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
				camel_folder_thaw (folder);
				/* will never happen - 0 is unconditionally returned */
				goto fail2;
			}

			while (camel_mime_parser_step (mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM &&
			       !g_cancellable_is_cancelled (cancellable)) {

				CamelMimeMessage *msg;
				gint pc = 0;

				any_read = TRUE;

				if (st.st_size > 0)
					pc = (gint) (100.0 * ((gdouble)
						camel_mime_parser_tell (mp) /
						(gdouble) st.st_size));
				camel_operation_progress (cancellable, pc);

				msg = camel_mime_message_new ();
				if (!camel_mime_part_construct_from_parser_sync (
					(CamelMimePart *) msg, mp, NULL, NULL)) {
					/* set exception? */
					g_object_unref (msg);
					break;
				}

				import_mbox_add_message (folder, msg, cancellable, error);

				g_object_unref (msg);

				if (error && *error != NULL)
					break;

				camel_mime_parser_step (mp, NULL, NULL);
			}
		} else {
			close (fd);
		}

		if (!any_read && !g_cancellable_is_cancelled (cancellable)) {
			CamelStream *stream;

			stream = camel_stream_fs_new_with_name (m->path, O_RDONLY, 0, NULL);
			if (stream) {
				CamelMimeMessage *msg;

				msg = camel_mime_message_new ();

				if (camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) msg, stream, NULL, NULL))
					import_mbox_add_message (folder, msg, cancellable, error);

				g_object_unref (msg);
				g_object_unref (stream);
			}
		}
		/* Not passing a GCancellable or GError here. */
		camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
		camel_folder_thaw (folder);
		camel_operation_pop_message (cancellable);
	fail2:
		g_clear_object (&mp);
	}
fail1:
	/* Not passing a GCancellable or GError here. */
	camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
	g_object_unref (folder);
	/* 'fd' is freed together with 'mp' */
	/* coverity[leaked_handle] */
}

static void
import_mbox_done (struct _import_mbox_msg *m)
{
	if (m->done)
		m->done (m->done_data, &m->base.error);
}

static void
import_mbox_free (struct _import_mbox_msg *m)
{
	g_object_unref (m->session);
	g_free (m->uri);
	g_free (m->path);
}

static void
import_kmail_folder (struct _import_mbox_msg *m,
                     gchar *k_path_in,
                     GCancellable *cancellable,
                     GError **error)
{
	const gchar *special_folders []= {"cur", "tmp", "new", NULL};
	gchar *special_path;
	const CamelStore *store;
	CamelFolder *folder;
	CamelMimeParser *mp = NULL;
	CamelMessageInfo *info;
	CamelMimeMessage *msg;
	guint32 flags = 0;

	gchar *e_uri, *e_path;
	gchar *k_path;
	const gchar *d;
	gchar *mail_url;
	GDir *dir;
	struct stat st;
	gint fd, i;

	e_uri = kuri_to_euri (k_path_in);
	/* we need to drop some folders, like: Trash */
	if (!e_uri)
		return;

	/* In case we using async way in the future */
	k_path = g_strdup (k_path_in);
	store = evolution_get_local_store ();
	e_path = e_uri + strlen (EVOLUTION_LOCAL_BASE) + 1;
	e_mail_store_create_folder_sync ((CamelStore *)store, e_path, NULL, NULL);
	folder = e_mail_session_uri_to_folder_sync (
			m->session, e_uri, CAMEL_STORE_FOLDER_CREATE,
			cancellable, NULL);

	if (folder == NULL) {
		g_free (k_path);
		g_warning ("evolution error: cannot get the folder\n");
		return;
	}

	camel_operation_push_message (
			cancellable, _("Importing “%s”"),
			camel_folder_get_display_name (folder));
	camel_folder_freeze (folder);

	for (i = 0; special_folders [i]; i++) {
		camel_operation_progress (cancellable, 100*i/3);
		special_path = g_build_filename (k_path, special_folders[i], NULL);
		dir = g_dir_open (special_path, 0, NULL);
		while ((d = g_dir_read_name (dir))) {
			if ((strcmp (d, ".") == 0) || (strcmp (d, "..") == 0)) {
				continue;
			}
			mail_url = g_build_filename (special_path, d, NULL);
			if (g_stat (mail_url, &st) == -1) {
				g_free (mail_url);
				continue;
			}
			if (S_ISREG (st.st_mode)) {
				fd = g_open (mail_url, O_RDONLY | O_BINARY, 0);
				g_free (mail_url);
				if (fd == -1) {
					continue;
				}
				mp = camel_mime_parser_new ();
				camel_mime_parser_scan_from (mp, FALSE);
				if (camel_mime_parser_init_with_fd (mp, fd) == -1) {
					/* will never happen - 0 is unconditionally returned */
					g_object_unref (mp);
					continue;
				}
				msg = camel_mime_message_new ();
				if (!camel_mime_part_construct_from_parser_sync (
						(CamelMimePart *) msg, mp, NULL, NULL)) {
					/* set exception? */
					g_object_unref (mp);
					g_object_unref (msg);
					continue;
				}
				info = camel_message_info_new (NULL);
				if (strcmp (special_folders[i], "cur") == 0) {
					flags |= CAMEL_MESSAGE_SEEN;
				} else if (strcmp (special_folders[i], "tmp") == 0) {
					flags |= CAMEL_MESSAGE_DELETED; /* Mark the 'tmp' mails as 'deleted' */
				}
				camel_message_info_set_flags (info, flags, ~0);
				camel_folder_append_message_sync (
					folder, msg, info, NULL,
					cancellable, error);
				g_clear_object (&info);
				g_object_unref (msg);
				g_object_unref (mp);
			} else {
				g_free (mail_url);
			}
		}
	}
	camel_operation_progress (cancellable, 100);
	camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
	camel_folder_thaw (folder);
	camel_operation_pop_message (cancellable);

	g_free (k_path);
}

static void
import_kmail_exec (struct _import_mbox_msg *m,
                   GCancellable *cancellable,
                   GError **error)
{
	GSList *list, *l;
	gchar *folder;
	struct stat st;

	if (g_stat (m->path, &st) == -1) {
		g_warning (
			"cannot find source file to import '%s': %s",
			m->path, g_strerror (errno));
		return;
	}

	if (!S_ISDIR (st.st_mode)) {
		g_warning (
			"the source path '%s' is not a directory.",
			m->path);
		return;
	}

	list = kmail_get_folders (m->path);
	for (l = list; l; l = l->next) {
		folder = (gchar *) l->data;
		import_kmail_folder (m, folder, cancellable, NULL);
	}
	if (list)
		g_slist_free_full (list, g_free);
}

static void
import_kmail_done (struct _import_mbox_msg *m)
{
	if (m->done)
		m->done (m->done_data, &m->base.error);
}

static void
import_kmail_free (struct _import_mbox_msg *m)
{
	g_object_unref (m->session);
	g_free (m->uri);
	g_free (m->path);
}

static MailMsgInfo import_mbox_info = {
	sizeof (struct _import_mbox_msg),
	(MailMsgDescFunc) import_mbox_desc,
	(MailMsgExecFunc) import_mbox_exec,
	(MailMsgDoneFunc) import_mbox_done,
	(MailMsgFreeFunc) import_mbox_free
};

/* Only difference with mbox_info is: _exec
   but I put it into to different info. */
static MailMsgInfo import_kmail_info = {
	sizeof (struct _import_mbox_msg),
	(MailMsgDescFunc) import_kmail_desc,
	(MailMsgExecFunc) import_kmail_exec,
	(MailMsgDoneFunc) import_kmail_done,
	(MailMsgFreeFunc) import_kmail_free
};

gboolean
mail_importer_file_is_mbox (const gchar *path)
{
	GFile *file;
	GFileInfo *info;
	const gchar *content_type;
	gboolean is_mbox;

	if (!path)
		return FALSE;

	file = g_file_new_for_path (path);
	info = 	g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (!info) {
		g_clear_object (&file);
		return TRUE;
	}

	content_type = g_file_info_get_content_type (info);
	is_mbox = content_type && g_content_type_is_mime_type (content_type, "application/mbox");

	g_clear_object (&info);
	g_clear_object (&file);

	return is_mbox;
}

gint
mail_importer_import_mbox (EMailSession *session,
                           const gchar *path,
                           const gchar *folderuri,
                           GCancellable *cancellable,
                           void (*done) (gpointer data,
                                         GError **error),
                           gpointer data)
{
	struct _import_mbox_msg *m;
	gint id;

	m = mail_msg_new_with_cancellable (&import_mbox_info, cancellable);
	m->session = g_object_ref (session);
	m->path = g_strdup (path);
	m->uri = g_strdup (folderuri);
	m->done = done;
	m->done_data = data;

	id = m->base.seq;
	mail_msg_fast_ordered_push (m);

	return id;
}

void
mail_importer_import_mbox_sync (EMailSession *session,
                                const gchar *path,
                                const gchar *folderuri,
                                GCancellable *cancellable)
{
	struct _import_mbox_msg *m;

	m = mail_msg_new_with_cancellable (&import_mbox_info, cancellable);
	m->session = g_object_ref (session);
	m->path = g_strdup (path);
	m->uri = g_strdup (folderuri);

	import_mbox_exec (m, cancellable, &m->base.error);
	import_mbox_done (m);
	mail_msg_unref (m);
}

gint
mail_importer_import_kmail (EMailSession *session,
                            const gchar *path,          /* path is basedir */
                            const gchar *folderuri,     /* not used now, use it when the user want to port to a certain folder */
                            GCancellable *cancellable,
                            void (*done) (gpointer data,
			                 GError **error),
                            gpointer data)
{
	struct _import_mbox_msg *m;
	gint id;

	m = mail_msg_new_with_cancellable (&import_kmail_info, cancellable);
	m->session = g_object_ref (session);
	m->path = g_strdup (path);
	m->uri = g_strdup (folderuri);
	m->done = done;
	m->done_data = data;
	id = m->base.seq;
	mail_msg_fast_ordered_push (m);

	return id;
}

void
mail_importer_import_kmail_sync (EMailSession *session,
                                 const gchar *path,
                                 const gchar *folderuri,
                                 GCancellable *cancellable)
{
	struct _import_mbox_msg *m;

	m = mail_msg_new_with_cancellable (&import_kmail_info, cancellable);
	m->session = g_object_ref (session);
	m->path = g_strdup (path);
	if (folderuri)
		m->uri = g_strdup (folderuri);
	else
		m->uri = NULL;

	import_kmail_exec (m, cancellable, &m->base.error);
	import_kmail_done (m);
	mail_msg_unref (m);
}

struct _import_folders_data {
	MailImporterSpecial *special_folders;
	EMailSession *session;
	GCancellable *cancellable;

	guint elmfmt : 1;
};

static void
import_folders_rec (struct _import_folders_data *m,
                    const gchar *filepath,
                    const gchar *folderparent)
{
	GDir *dir;
	const gchar *d;
	struct stat st;
	const gchar *data_dir;
	gchar *filefull, *foldersub, *uri, *utf8_filename;
	const gchar *folder;

	dir = g_dir_open (filepath, 0, NULL);
	if (dir == NULL)
		return;

	data_dir = mail_session_get_data_dir ();

	utf8_filename = g_filename_to_utf8 (filepath, -1, NULL, NULL, NULL);
	camel_operation_push_message (m->cancellable, _("Scanning %s"), utf8_filename);
	g_free (utf8_filename);

	while ((d = g_dir_read_name (dir))) {
		if (d[0] == '.')
			continue;

		filefull = g_build_filename (filepath, d, NULL);

		/* skip non files and directories, and skip directories in mozilla mode */
		if (g_stat (filefull, &st) == -1
		    || !(S_ISREG (st.st_mode)
			 || (m->elmfmt && S_ISDIR (st.st_mode)))) {
			g_free (filefull);
			continue;
		}

		folder = d;
		if (folderparent == NULL) {
			gint i;

			for (i = 0; m->special_folders[i].orig; i++)
				if (strcmp (m->special_folders[i].orig, folder) == 0) {
					folder = m->special_folders[i].new;
					break;
				}
			/* FIXME: need a better way to get default store location */
			uri = g_strdup_printf (
				"mbox:%s/local#%s", data_dir, folder);
		} else {
			uri = g_strdup_printf (
				"mbox:%s/local#%s/%s",
				data_dir, folderparent, folder);
		}

		printf ("importing to uri %s\n", uri);
		mail_importer_import_mbox_sync (
			m->session, filefull, uri, m->cancellable);
		g_free (uri);

		/* This little gem re-uses the stat buffer and filefull
		 * to automagically scan mozilla-format folders. */
		if (!m->elmfmt) {
			gchar *tmp = g_strdup_printf ("%s.sbd", filefull);

			g_free (filefull);
			filefull = tmp;
			if (g_stat (filefull, &st) == -1) {
				g_free (filefull);
				continue;
			}
		}

		if (S_ISDIR (st.st_mode)) {
			foldersub = folderparent ?
				g_strdup_printf (
					"%s/%s", folderparent, folder) :
				g_strdup (folder);
			import_folders_rec (m, filefull, foldersub);
			g_free (foldersub);
		}

		g_free (filefull);
	}
	g_dir_close (dir);

	camel_operation_pop_message (m->cancellable);
}

/**
 * mail_importer_import_folders_sync:
 * @filepath:
 * @:
 * @flags:
 * @cancel:
 *
 * import from a base path @filepath into the root local folder tree,
 * scanning all sub-folders.
 *
 * if @flags is MAIL_IMPORTER_MOZFMT, then subfolders are assumed to
 * be in mozilla/evolutoin 1.5 format, appending '.sbd' to the
 * directory name. Otherwise they are in elm/mutt/pine format, using
 * standard unix directories.
 **/
void
mail_importer_import_folders_sync (EMailSession *session,
                                   const gchar *filepath,
                                   MailImporterSpecial special_folders[],
                                   gint flags,
                                   GCancellable *cancellable)
{
	struct _import_folders_data m;

	m.special_folders = special_folders;
	m.elmfmt = (flags & MAIL_IMPORTER_MOZFMT) == 0;
	m.session = g_object_ref (session);
	m.cancellable = cancellable;

	import_folders_rec (&m, filepath, NULL);
}
